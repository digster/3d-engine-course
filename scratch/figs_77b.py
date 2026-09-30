#!/usr/bin/env python3
"""scratch/figs_77b.py — Lesson 7.7b's diagrams.

Same rules as every lesson since 5.10:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - ~5.2 units per character for `xs`, ~6.0 for `sm`
  - filenames numbered by PAGE ORDER
  - `rule()` takes a CSS CLASS; `cline()` takes a COLOUR
  - viewBoxes at most 928 wide, labels no smaller than `xs`

Every number is verify_77b's (scratch/_v77b_rel.txt, Release library; the exact
sections print the same numbers at -O0), and the plots read the data files it
writes (scratch/l77b_*.csv) and the half-resolution render crops
scratch/shots_77b.sh makes (scratch/l77b_render_*.ppm), all tracked, so the
figures regenerate from the repository. Two curves are computed here instead —
the nlerp lag and the pieces-per-arc — and cross-checked against the harness's
table before a single one is drawn (see `check_against_harness`).

Colours: FILE (what the glTF says) = BLUE, OURS = AMBER, right = GREEN,
wrong = RED, ancestor/unweighted = GREY.
"""
import csv
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)
from figs_610 import cline                                          # noqa: E402
from figs_45 import read_ppm, box_sample, rle_rects, hexrgb         # noqa: E402
from figs_71 import cmarker, carrow, poly, frame                    # noqa: E402

OUT = "scratch"

# ---- measured (verify_77b) -------------------------------------------------
CHECKS = 36

# the file (§A)
NODES = [(0, 'head', 1), (1, 'neck', 10), (2, 'hand.L', 3), (3, 'forearm.L', 4),
         (4, 'upper_arm.L', 5), (5, 'shoulder.L', 10), (6, 'hand.R', 7), (7, 'forearm.R', 8),
         (8, 'upper_arm.R', 9), (9, 'shoulder.R', 10), (10, 'chest', 11), (11, 'spine', 18),
         (12, 'foot.L', 13), (13, 'shin.L', 14), (14, 'thigh.L', 18), (15, 'foot.R', 16),
         (16, 'shin.R', 17), (17, 'thigh.R', 18), (18, 'hips', 20), (19, 'body', 20),
         (20, 'Armature', -1)]
SKIN = [18, 11, 10, 1, 0, 5, 4, 3, 2, 9, 8, 7, 6, 14, 13, 12, 17, 16, 15]

# §D
INFL = (1301, 1095, 199, 86)
WEIGHT_SUM_ERR = "8.57e-08"
VERTEX = {"index": 28, "joints": (0, 13, 16, 0), "weights": ("0.4284", "0.2883", "0.2833", "0")}

# §E
ARCS = [(15.5, 0.0046, 1), (30.0, 0.0331, 1), (60.0, 0.2675, 2), (90.0, 0.9188, 3),
        (120.0, 2.2340, 4), (150.0, 4.5140, 5), (179.0, 8.0010, 6)]
TOL_DEG = 0.05
CUBIC = {"channels": 39, "keys_in": 332, "keys_out": 1227, "split": 895, "no_td": 40.25}
TWO = {"anywhere": 0.916, "frames": 0.218}

# §F
XYZW = {"n": 748, "min": 110.9, "median": 168.5, "max": 180.0}

# §H
NEUTRAL = {"vertices": 25, "lag_mm": 16.9, "t": 0.600}

# §J
NOSE_Z = 0.121


def fmt_int(n):
    return f"{n:,}"


def read_csv(name):
    with open(os.path.join(OUT, name), newline="") as fh:
        return list(csv.DictReader(fh))


def write(name, text):
    with open(os.path.join(OUT, name), "w") as fh:
        fh.write(text)


def dots(points, colour, size=1.8, opacity=0.85):
    d = "".join(f"M{x:.1f},{y:.1f}h0" for x, y in points)
    return (f'<path d="{d}" fill="none" stroke="{colour}" stroke-width="{size}" '
            f'stroke-linecap="round" stroke-opacity="{opacity}"/>')


def dot(x, y, r, colour):
    return f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r}" fill="{colour}" stroke="none"/>'


# ---- the two curves computed here, and their cross-check ----------------------

def nlerp_lag_deg(theta_deg, u):
    """|angle between nlerp and slerp| at u, for a ROTATION of theta — in rotation
    degrees. On the sphere the arc is theta/2 and both paths lie on it; nlerp's
    sphere angle is atan2(u sin W, (1-u) + u cos W), slerp's is u W."""
    w = math.radians(theta_deg) / 2.0
    phi = math.atan2(u * math.sin(w), (1.0 - u) + u * math.cos(w))
    return math.degrees(2.0 * abs(phi - u * w))


def max_lag(theta_deg):
    return max(nlerp_lag_deg(theta_deg, i / 1000.0) for i in range(1001))


def pieces(theta_deg, tol_deg, probes=7):
    """The importer's rule: the fewest equal pieces for which every probe inside
    every piece is within tolerance. For a slerp the pieces are congruent, so one
    piece of arc theta/m decides it."""
    for m in range(1, 65):
        arc = theta_deg / m
        worst = max(nlerp_lag_deg(arc, p / (probes + 1)) for p in range(1, probes + 1))
        if worst <= tol_deg:
            return m
    return 64


def check_against_harness():
    for arc, lag, m in ARCS:
        got_lag = max_lag(arc)
        got_m = pieces(arc, TOL_DEG)
        assert abs(got_lag - lag) < 2e-3, (arc, got_lag, lag)
        assert got_m == m, (arc, got_m, m)


# ---- render panels ------------------------------------------------------------

# The demo's own colours, so the quantiser snaps to them (7.7's learning: the
# figure palette must contain the demo's colours).
RENDER_PALETTE = [hexrgb("#f4cbaa"), hexrgb("#4a78b8"), hexrgb("#f2ba5c"),
                  hexrgb("#787e8c"), hexrgb("#2c3038")]


def render_panel(name, x, y, px, levels=6):
    w, h, data = read_ppm(os.path.join(OUT, name))
    gw, gh, grid = box_sample(data, w, (0, 0, w, h), 1)
    return (['<g shape-rendering="crispEdges">']
            + rle_rects(grid, gw, gh, x, y, px, RENDER_PALETTE, levels=levels, bg_class="fill-shot")
            + ['</g>'], gw * px, gh * px)


# ===========================================================================
# Figure 1 — three orders of one skeleton  (§2)
# ===========================================================================
def fig_orders():
    uid = "l77bf1"
    row = 15.0
    top = 64
    cols = [30, 330, 630]
    b = []
    names = {i: n for i, n, _ in NODES}
    parent = {i: p for i, _, p in NODES}

    def cells(x, y, values, cls):
        # SVG collapses runs of spaces, so every column is its own <text> at its
        # own x: numbers right-aligned, names left-aligned.
        out = []
        for (dx, anchor), v in zip(((22, "end"), (30, "start"), (138, "end")), values):
            out.append(label(x + dx, y, str(v), cls, anchor))
        return out

    b.append(label(cols[0], 22, "nodes[] — the file's order", "sm", "start"))
    b.append(label(cols[0], 38, "index, name, parent index", "xs muted", "start"))
    b.append(label(cols[1], 22, "skins[0].joints — the skin's order", "sm", "start"))
    b.append(label(cols[1], 38, "slot, node / name, parent's slot", "xs muted", "start"))
    b.append(label(cols[2], 22, "our skeleton — sorted, ancestor carried", "sm", "start"))
    b.append(label(cols[2], 38, "joint, name, parent joint", "xs muted", "start"))

    # Column A: the node array. A joint whose parent has a LARGER index is marked.
    for i, n, p in NODES:
        y = top + i * row
        joint = i in SKIN
        cls = "xs mono" if joint else "xs mono muted"
        b += cells(cols[0] + 14, y, (i, n, p if p >= 0 else "–"), cls)
        if joint and p > i:
            b.append(dot(cols[0] + 4, y - 3.5, 3.0, RED))
    b.append(label(cols[0], top + 21 * row + 10, "19 of 19 joints come before their parent",
                   "xs", "start"))

    # Column B: the skin's order. Parent slot is earlier or not a joint.
    slot = {n: r for r, n in enumerate(SKIN)}
    for r, n in enumerate(SKIN):
        y = top + r * row
        ps = slot.get(parent[n])
        b += cells(cols[1] + 14, y, (r, f"{n}  {names[n]}", ps if ps is not None else "–"), "xs mono")
        b.append(dot(cols[1] + 4, y - 3.5, 3.0, GREEN))
    b.append(label(cols[1], top + 21 * row + 10, "already parent-first: the sort moves nothing",
                   "xs", "start"))

    # Column C: ours. Armature first, then the skin's order.
    order = [20] + SKIN
    ours = {n: j for j, n in enumerate(order)}
    for j, n in enumerate(order):
        y = top + j * row
        pj = ours.get(parent[n])
        cls = "xs mono muted" if n == 20 else "xs mono"
        b += cells(cols[2] + 14, y, (j, names[n], pj if pj is not None else "–"), cls)
    b.append(label(cols[2], top + 21 * row + 10, "20 joints: joint j's parent is always < j",
                   "xs", "start"))
    b.append(label(cols[2], top + 21 * row - 6, "joint 0: the ancestor, no vertices", "xs muted", "start"))
    H = top + 21 * row + 26
    desc = ("Three tables side by side. The first is Blender's node array in index order, 21 "
            "nodes from head (0) to Armature (20); every one of the 19 joints is marked because "
            "its parent has a larger index — the file writes children first. The second is the "
            "skin's joint list, 19 slots from hips to foot.R, every parent in an earlier slot, "
            "so the list is already parent-first. The third is the imported skeleton: the "
            "Armature node first, as joint 0 with no vertices, then the skin's joints in the "
            "skin's own order, 20 joints each with a parent earlier than itself.")
    write("l77b_fig1.svg", svg(uid, 910, H, "Three orders of one skeleton", desc, b))


# ===========================================================================
# Figure 2 — the spec's own example, with its numbers  (§3)
# ===========================================================================
def fig_spec():
    uid = "l77bf2"
    b = [cmarker(uid, "r", RED), cmarker(uid, "g", GREEN), cmarker(uid, "k", "var(--dia-ink)")]
    # Left: the node tree.
    nodes = [("node_0", "T (0, 1, 0)", "not a joint — APPLIED", 30, 40, GREEN),
             ("node_1", "S (0.5, 0.5, 0.5)", "joint A — slot 0", 30, 108, AMBER),
             ("node_2", "T (1, 0, 0)", "joint B — slot 1", 30, 176, AMBER),
             ("node_3", "T (1, 0, 0)", "IGNORED", 250, 40, RED),
             ("node_4", "R 180° about y", "mesh + skin: IGNORED", 250, 108, RED)]
    for name, xf, role, x, y, col in nodes:
        b.append(hollow(x, y, 180, 50, col, width=1.5))
        b.append(label(x + 10, y + 17, name, "sm mono", "start"))
        b.append(label(x + 10, y + 31, xf, "xs mono", "start"))
        b.append(label(x + 10, y + 44, role, "xs muted", "start"))
    b.append(carrow(120, 90, 120, 106, uid, "k", "var(--dia-ink)", width=1.2))
    b.append(carrow(120, 158, 120, 174, uid, "k", "var(--dia-ink)", width=1.2))
    b.append(carrow(340, 90, 340, 106, uid, "k", "var(--dia-ink)", width=1.2))
    b.append(label(30, 256, "inverse binds (the file's):", "xs", "start"))
    b.append(label(30, 270, "A: S(2) T(0, −1, 0)", "xs mono", "start"))
    b.append(label(30, 284, "B: S(2) T(−0.5, −1, 0)", "xs mono", "start"))
    b.append(label(250, 190, "The spec: “the transform of the", "xs", "start"))
    b.append(label(250, 204, "skinned mesh node MUST be ignored.”", "xs", "start"))
    b.append(label(250, 228, "jointMatrix = global(joint) · IBM", "xs mono", "start"))
    b.append(label(250, 242, "v′ = Σ w · jointMatrix · v", "xs mono", "start"))

    # Right: the plane z = 0, x right (red), y up (green).
    ox, oy, s = 520, 335, 150.0

    def P(x, y):
        return ox + x * s, oy - y * s

    b.append(frame(ox - 44, oy - 1.72 * s, 1.52 * s + 70, 1.72 * s + 30))
    x0, y0 = P(0, 0)
    b.append(f'<line x1="{x0}" y1="{y0}" x2="{P(1.3, 0)[0]}" y2="{y0}" class="ax-x" stroke-width="1.3"/>')
    b.append(f'<line x1="{x0}" y1="{y0}" x2="{x0}" y2="{P(0, 1.62)[1]}" class="ax-y" stroke-width="1.3"/>')
    b.append(label(P(1.3, 0)[0] + 8, y0 + 4, "x", "xs lbl-x", "start"))
    b.append(label(x0 - 8, P(0, 1.62)[1] + 4, "y", "xs lbl-y", "end"))
    for v in (0.5, 1.0):
        b.append(rule(P(v, 0)[0], y0, P(v, 0)[0], y0 + 5, "ink-soft"))
        b.append(label(P(v, 0)[0], y0 + 16, f"{v:g}", "xs mono", "middle"))
        b.append(rule(x0 - 5, P(0, v)[1], x0, P(0, v)[1], "ink-soft"))
        b.append(label(x0 - 8, P(0, v)[1] + 12, f"{v:g}", "xs mono", "end"))
    # rest triangle
    tri = [P(0, 1), P(0.5, 1), P(0.25, 1.5)]
    b.append(poly(tri, AMBER, width=1.6))
    b.append(label(tri[0][0] + 6, tri[0][1] + 16, "v0 (0, 1)", "xs mono", "start"))
    b.append(label(tri[1][0] + 8, tri[1][1] + 16, "v1 (0.5, 1)", "xs mono", "start"))
    b.append(label(tri[2][0] + 9, tri[2][1] + 6, "v2", "xs mono", "start"))
    # v1's path under the 90-degree turn of node_1: radius 0.5 about (0, 1).
    arc = [P(0.5 * math.cos(math.radians(a)), 1 + 0.5 * math.sin(math.radians(a))) for a in range(0, 91, 5)]
    b.append(poly(arc, BLUE, width=1.3, dash="4 3", close=False))
    px, py = P(0.5 * math.cos(math.radians(45)), 1 + 0.5 * math.sin(math.radians(45)))
    b.append(dot(px, py, 3.2, BLUE))
    b.append(label(px + 10, py - 4, "t = 0.5  (0.35355, 1.35355)", "xs mono", "start"))
    px, py = P(0, 1.5)
    b.append(dot(px, py, 3.2, BLUE))
    b.append(label(px + 8, py - 14, "t = 1  (0, 1.5)", "xs mono", "start"))
    # the two wrong answers
    cx, cy = P(1, 1)
    b.append(dot(cx, cy, 3.2, RED))
    b.append(label(cx + 8, cy - 8, "(1, 1): mesh node", "xs", "start"))
    b.append(label(cx + 8, cy + 5, "applied", "xs", "start"))
    cx, cy = P(0, 0)
    b.append(dot(cx, cy, 3.2, RED))
    b.append(label(cx + 10, cy - 8, "(0, 0): node_0 dropped", "xs", "start"))
    b.append(label(ox - 38, oy - 1.72 * s - 8, "v0, v1 and v2 in the plane z = 0", "xs muted", "start"))

    desc = ("Left: the specification's skin example as a node tree. node_0 translates by (0, 1, 0) "
            "and is not a joint but is applied; node_1 scales by one half and is joint A; node_2 "
            "translates by one unit and is joint B; node_3 and node_4, which carry the mesh, are "
            "ignored. The inverse binds are S(2)T(0,-1,0) and S(2)T(-0.5,-1,0). Right: the triangle "
            "in the plane, with v0 at (0, 1) and v1 at (0.5, 1); v1's path when node_1 turns 90 "
            "degrees, through (0.35355, 1.35355) at t = 0.5 to (0, 1.5) at t = 1; and in red the "
            "two wrong answers, (1, 1) if the mesh node were applied and (0, 0) if node_0 were dropped.")
    write("l77b_fig2.svg", svg(uid, 900, 380, "The spec's example", desc, b))


# ===========================================================================
# Figure 3 — sorted, and in the node array's order  (§4)
# ===========================================================================
def fig_order_render():
    uid = "l77bf3"
    px = 2
    b = []
    left, lw, lh = render_panel("l77b_render_walk.ppm", 20, 34, px)
    right, rw, rh = render_panel("l77b_render_order.ppm", 40 + lw, 34, px)
    b.append(label(20, 22, "sorted parent-first (as imported)", "sm", "start"))
    b.append(label(40 + lw, 22, "joints in node-array order: 19 of 20 out of order", "sm", "start"))
    b += left + right
    b.append(label(20, 34 + lh + 18, "mannequin --time 0.25   and   --order-file", "xs muted mono", "start"))
    desc = ("Two renders of the mannequin at the same instant of its walk. Left: imported "
            "correctly, a standing figure mid-stride. Right: the same joints built in the order "
            "of the file's node array, where every parent comes after its child; each joint is "
            "composed as a root at its own offset from the origin, and the body is a pile of "
            "parts at the feet.")
    write("l77b_fig3.svg", svg(uid, 40 + lw + rw + 20, 34 + lh + 28, "Sorted and unsorted", desc, b))


# ===========================================================================
# Figure 4 — three index spaces, and what the vertices kept  (§6)
# ===========================================================================
def fig_influences():
    uid = "l77bf4"
    b = [cmarker(uid, "k", "var(--dia-ink)")]
    b.append(label(20, 22, f"vertex {VERTEX['index']} of the body, at the top of the legs:",
                   "sm", "start"))
    cols = [(20, "JOINTS_0 (skin slots)"), (250, "skins[0].joints (nodes)"),
            (480, "our skeleton (joints)"), (710, "WEIGHTS_0")]
    for x, t in cols:
        b.append(label(x, 44, t, "xs muted", "start"))
    rows = [(0, 18, "hips", 1), (13, 14, "thigh.L", 14), (16, 17, "thigh.R", 17)]
    for k, (slot, node, name, joint) in enumerate(rows):
        y = 70 + k * 30
        b.append(hollow(20, y - 14, 170, 22, BLUE))
        b.append(label(30, y + 1, f"slot {slot}", "xs mono", "start"))
        b.append(carrow(192, y - 3, 246, y - 3, uid, "k", "var(--dia-ink)", width=1.1))
        b.append(hollow(250, y - 14, 170, 22, GREY))
        b.append(label(260, y + 1, f"node {node}  {name}", "xs mono", "start"))
        b.append(carrow(422, y - 3, 476, y - 3, uid, "k", "var(--dia-ink)", width=1.1))
        b.append(hollow(480, y - 14, 170, 22, AMBER))
        b.append(label(490, y + 1, f"joint {joint}  {name}", "xs mono", "start"))
        b.append(label(710, y + 1, VERTEX["weights"][k], "xs mono", "start"))
    b.append(label(20, 170, "the fourth slot: joint index 0 with weight 0 — the spec's “SHOULD be set to zero”",
                   "xs", "start"))

    # Histogram.
    top, base = 200, 330
    b.append(label(20, top, "what 2,681 vertices kept, by non-zero influences", "sm", "start"))
    total = sum(INFL)
    bw, gap, x0 = 90, 30, 60
    peak = max(INFL)
    for i, n in enumerate(INFL):
        x = x0 + i * (bw + gap)
        h = (base - top - 30) * n / peak
        b.append(box(x, base - h, bw, h, AMBER, rx=1, opacity=0.85))
        b.append(label(x + bw / 2, base - h - 6, f"{fmt_int(n)}  ({100.0 * n / total:.1f}%)", "xs mono"))
        b.append(label(x + bw / 2, base + 14, f"{i + 1}", "xs mono"))
    b.append(rule(x0 - 10, base, x0 + 4 * (bw + gap), base, "ink-soft"))
    b.append(label(x0 + 2 * (bw + gap) - gap / 2, base + 30, "influences per vertex", "xs muted"))
    b.append(label(560, top + 40, "none had more than four: truncated 0", "xs", "start"))
    b.append(label(560, top + 56, f"worst |sum − 1| in the file {WEIGHT_SUM_ERR}", "xs", "start"))
    b.append(label(560, top + 72, "(the spec's validator allows 2e-7 per weight)", "xs muted", "start"))
    desc = ("Top: one vertex's three influences followed through three index spaces — skin slot 0 "
            "is node 18, hips, which is joint 1 of the imported skeleton; slot 13 is node 14, "
            "thigh.L, joint 14; slot 16 is node 17, thigh.R, joint 17; weights 0.4284, 0.2883 and "
            "0.2833. Bottom: a histogram of how many non-zero influences each of the 2,681 "
            "vertices kept: 1,301 had one, 1,095 two, 199 three and 86 four.")
    write("l77b_fig4.svg", svg(uid, 900, base + 44, "Three index spaces", desc, b))


# ===========================================================================
# Figure 5 — the blind copy  (§7)
# ===========================================================================
def fig_xyzw():
    uid = "l77bf5"
    px = 2
    b = []
    panel, pw, ph = render_panel("l77b_render_xyzw.ppm", 20, 34, px)
    b.append(label(20, 22, "every rotation read w-first", "sm", "start"))
    b += panel
    b.append(label(20, 34 + ph + 16, "the root's identity became 180° about z:", "xs", "start"))
    b.append(label(20, 34 + ph + 30, "the body hangs a metre under the floor", "xs", "start"))

    # Distribution of the 748 key errors.
    errs = [float(r["deg"]) for r in read_csv("l77b_f_xyzw.csv")]
    assert len(errs) == XYZW["n"]
    x0, x1 = 440, 880
    y0 = 300
    lo, hi = 100.0, 180.0

    def X(v):
        return x0 + (v - lo) / (hi - lo) * (x1 - x0)

    bins = [0] * 16
    for e in errs:
        k = min(15, int((e - lo) / 5.0))
        bins[k] += 1
    peak = max(bins)
    for k, n in enumerate(bins):
        h = 200.0 * n / peak
        b.append(box(X(lo + 5 * k) + 1, y0 - h, (x1 - x0) / 16 - 2, h, RED, rx=1, opacity=0.8))
    b.append(rule(x0, y0, x1, y0, "ink-soft"))
    for v in range(100, 181, 20):
        b.append(rule(X(v), y0, X(v), y0 + 5, "ink-soft"))
        b.append(label(X(v), y0 + 17, f"{v}°", "xs mono"))
    b.append(label((x0 + x1) / 2, y0 + 34, f"error of each of Blender's {XYZW['n']} rotation keys", "xs muted"))
    b.append(label(x0, 40, f"min {XYZW['min']}°   median {XYZW['median']}°   max {XYZW['max']:.1f}°", "sm mono", "start"))
    b.append(label(x0, 58, "never small — and not one fixed rotation either:", "xs", "start"))
    b.append(label(x0, 72, "identity → 180° about z;  (½, ½, ½, ½) → 0°", "xs mono", "start"))
    desc = ("Left: the mannequin with every rotation read as w, x, y, z instead of x, y, z, w; the "
            "root's identity rotation became a half turn about z and the folded body hangs about a "
            "metre below the floor grid. Right: a histogram of the angular error of all 748 rotation "
            "keys in Blender's file under the same misreading, from 110.9 to 180 degrees with a "
            "median of 168.5.")
    write("l77b_fig5.svg", svg(uid, 900, max(34 + ph + 40, y0 + 44), "The blind copy", desc, b))


# ===========================================================================
# Figure 6 — nlerp against slerp, and the pieces it takes  (§8.1)
# ===========================================================================
def fig_lag():
    uid = "l77bf6"
    b = []
    # Left: lag against u.
    x0, x1, y0, y1 = 60, 440, 290, 50
    b.append(frame(x0, y1, x1 - x0, y0 - y1))
    top = 9.0

    def P(u, e):
        return x0 + u * (x1 - x0), y0 - e / top * (y0 - y1)

    for p in range(1, 8):
        xx = P(p / 8, 0)[0]
        b.append(rule(xx, y1, xx, y0, "grid", dash="2 3"))
    cols = {60: GREEN, 90: BLUE, 120: PURPLE, 150: AMBER, 179: RED}
    for arc, col in cols.items():
        pts = [P(i / 200, nlerp_lag_deg(arc, i / 200)) for i in range(201)]
        b.append(poly(pts, col, width=1.6, close=False))
        m = max(range(201), key=lambda i: nlerp_lag_deg(arc, i / 200))
        px, py = P(m / 200, nlerp_lag_deg(arc, m / 200))
        b.append(label(px + 4 if m < 100 else px - 4, py - 5, f"{arc}°", "xs mono",
                       "start" if m < 100 else "end"))
    for e in (0, 2, 4, 6, 8):
        yy = P(0, e)[1]
        b.append(label(x0 - 6, yy + 3, f"{e}", "xs mono", "end"))
    for u in (0, 0.25, 0.5, 0.75, 1):
        b.append(label(P(u, 0)[0], y0 + 15, f"{u:g}", "xs mono"))
    b.append(label((x0 + x1) / 2, y0 + 32, "u, the parameter across one interval", "xs muted"))
    b.append(label(x0, y1 - 26, "nlerp's error against slerp, rotation degrees", "sm", "start"))
    b.append(label(x0, y1 - 12, "dashed: the importer's seven probes; the dot: u = ½, zero at every arc", "xs muted", "start"))
    mx, my = P(0.5, 0)
    b.append(dot(mx, my, 3.4, "var(--dia-ink)"))

    # Right: pieces needed against arc.
    X0, X1, Y0, Y1 = 540, 880, 290, 50
    b.append(frame(X0, Y1, X1 - X0, Y0 - Y1))

    def Q(arc, m):
        return X0 + arc / 180.0 * (X1 - X0), Y0 - m / 7.0 * (Y0 - Y1)

    steps = []
    prev = None
    for a10 in range(1, 1801):
        arc = a10 / 10.0
        m = pieces(arc, TOL_DEG)
        if prev is None:
            steps.append(Q(0, m))
        elif m != prev:
            steps.append(Q(arc, prev))
            steps.append(Q(arc, m))
        prev = m
    steps.append(Q(180, prev))
    b.append(poly(steps, AMBER, width=1.8, close=False))
    for arc, _, m in ARCS:
        qx, qy = Q(arc, m)
        b.append(dot(qx, qy, 3.2, BLUE))
    for m in range(1, 8):
        b.append(label(X0 - 6, Q(0, m)[1] + 3, f"{m}", "xs mono", "end"))
    for arc in (0, 45, 90, 135, 180):
        b.append(label(Q(arc, 0)[0], Y0 + 15, f"{arc}°", "xs mono"))
    b.append(label((X0 + X1) / 2, Y0 + 32, "the arc between two file keys", "xs muted"))
    b.append(label(X0, Y1 - 26, "pieces for 0.05°: computed (line), imported (dots)", "sm", "start"))
    b.append(label(X0, Y1 - 12, "Blender's widest step is 17.6°: one piece", "xs muted", "start"))
    desc = ("Left: the angle between nlerp and slerp across one interval, for rotations of 60, 90, "
            "120, 150 and 179 degrees; each curve is zero at both ends and at the midpoint and peaks "
            "near a fifth and four fifths of the way, at 0.27, 0.92, 2.23, 4.51 and 8.0 degrees. "
            "Dashed lines mark the seven probes. Right: a step curve of how many equal pieces an "
            "interval needs for nlerp to stay within 0.05 degrees, from one piece below about 38 "
            "degrees to six at 179, with the importer's measured counts on it as dots.")
    write("l77b_fig6.svg", svg(uid, 900, 336, "nlerp against slerp", desc, b))


# ===========================================================================
# Figure 7 — holding a STEP  (§8.2)
# ===========================================================================
def fig_step():
    uid = "l77bf7"
    b = []
    x0, x1, y0, y1 = 70, 560, 250, 50

    def P(t, v):
        return x0 + t * (x1 - x0), y0 - v / 2.0 * (y0 - y1)

    b.append(frame(x0, y1, x1 - x0, y0 - y1))
    ramp = [P(0, 0), P(0.5, 1), P(1, 2)]
    b.append(poly(ramp, RED, width=1.5, dash="5 4", close=False))
    step = [P(0, 0), P(0.5, 0), P(0.5, 1), P(1, 1), P(1, 2)]
    b.append(poly(step, GREEN, width=2.2, close=False))
    for t, v in ((0, 0), (0.5, 1), (1, 2)):
        px, py = P(t, v)
        b.append(dot(px, py, 4, BLUE))
    for t, v in ((0.5, 0), (1, 1)):
        px, py = P(t, v)
        b.append(dot(px, py, 3.4, AMBER))
    for v in (0, 1, 2):
        b.append(label(x0 - 8, P(0, v)[1] + 3, f"{v}", "xs mono", "end"))
    for t in (0, 0.5, 1):
        b.append(label(P(t, 0)[0], y0 + 15, f"{t:g}", "xs mono"))
    b.append(label((x0 + x1) / 2, y0 + 32, "time (s)", "xs muted"))
    b.append(label(x0, y1 - 26, "a STEP translation: keys 0, 1, 2 at 0, 0.5, 1 s", "sm", "start"))
    b.append(label(x0, y1 - 12, "blue: the file's keys   amber: the importer's held keys", "xs muted", "start"))
    lx = 590
    b.append(cline(lx, 80, lx + 28, 80, GREEN, width=2.2))
    b.append(label(lx + 36, 84, "imported: holds, then jumps", "xs", "start"))
    b.append(cline(lx, 102, lx + 28, 102, RED, width=1.5, dash="5 4"))
    b.append(label(lx + 36, 106, "keys copied: a ramp", "xs", "start"))
    b.append(label(lx, 146, "the held key is at", "xs", "start"))
    b.append(label(lx, 162, "nextafter(0.5, 0) = 0.49999997", "xs mono", "start"))
    b.append(label(lx, 186, "x(0.25): imported 0.000, copied 0.500", "xs mono", "start"))
    b.append(label(lx, 202, "x(0.49999997) 0.000   x(0.5) 1.000", "xs mono", "start"))
    b.append(label(lx, 226, "5 keys, 2 of them held", "xs", "start"))
    desc = ("A step-interpolated translation with keys 0, 1 and 2 at 0, 0.5 and 1 seconds. The "
            "imported channel holds each value flat and jumps at the next key, because the importer "
            "writes a copy of each value one representable float before the next key time, at "
            "0.49999997 and just below 1. Copying the keys as they are gives a ramp instead, reading "
            "0.5 at a quarter second where the file says 0.")
    write("l77b_fig7.svg", svg(uid, 900, 290, "Holding a STEP", desc, b))


# ===========================================================================
# Figure 8 — a cubic channel, with and without t_d  (§8.3)
# ===========================================================================
def fig_cubic():
    uid = "l77bf8"
    rows = read_csv("l77b_e_cubic.csv")
    keys = read_csv("l77b_e_cubic_keys.csv")
    b = []
    x0, x1, y0, y1 = 70, 640, 270, 50
    top = max(max(float(r["spec"]), float(r["no_td"])) for r in rows)
    top = math.ceil(top / 10.0) * 10.0

    def P(t, v):
        return x0 + t * (x1 - x0), y0 - v / top * (y0 - y1)

    b.append(frame(x0, y1, x1 - x0, y0 - y1))
    b.append(poly([P(float(r["t"]), float(r["no_td"])) for r in rows], RED, width=1.4, dash="5 4", close=False))
    b.append(poly([P(float(r["t"]), float(r["spec"])) for r in rows], BLUE, width=2.4, close=False))
    b.append(poly([P(float(r["t"]), float(r["ours"])) for r in rows], AMBER, width=1.1, close=False))
    file_t = [float(k["t"]) for k in keys if k["kind"] == "file"]
    ours_t = [float(k["t"]) for k in keys if k["kind"] == "ours"]
    for t in ours_t:
        xx = P(t, 0)[0]
        b.append(cline(xx, y0, xx, y0 - 6, AMBER, width=1.0))
    spec_at = {round(float(r["t"]), 5): float(r["spec"]) for r in rows}
    for t in file_t:
        near = min(spec_at, key=lambda s: abs(s - t))
        px, py = P(t, spec_at[near])
        b.append(dot(px, py, 3.6, BLUE))
    for v in range(0, int(top) + 1, 10):
        b.append(label(x0 - 6, P(0, v)[1] + 3, f"{v}", "xs mono", "end"))
    for t in (0, 0.25, 0.5, 0.75, 1):
        b.append(label(P(t, 0)[0], y0 + 15, f"{t:g}", "xs mono"))
    b.append(label((x0 + x1) / 2, y0 + 32, "time (s)", "xs muted"))
    b.append(label(x0, y1 - 26, "thigh.L, walk, Blender with sampling off: degrees from the first key", "sm", "start"))
    b.append(label(x0, y1 - 12, f"ticks: the {len(ours_t)} keys the importer wrote, from the file's {len(file_t)}", "xs muted", "start"))
    lx = 660
    b.append(cline(lx, 80, lx + 28, 80, BLUE, width=2.4))
    b.append(label(lx + 36, 84, "the file's Hermite", "xs", "start"))
    b.append(cline(lx, 100, lx + 28, 100, AMBER, width=1.1))
    b.append(label(lx + 36, 104, "ours, imported", "xs", "start"))
    b.append(cline(lx, 120, lx + 28, 120, RED, width=1.4, dash="5 4"))
    b.append(label(lx + 36, 124, "tangents without t_d", "xs", "start"))
    b.append(label(lx, 160, f"all 39 channels: {CUBIC['keys_in']} keys", "xs", "start"))
    b.append(label(lx, 176, f"→ {fmt_int(CUBIC['keys_out'])} ({CUBIC['split']} split),", "xs", "start"))
    b.append(label(lx, 192, "within 0.05° everywhere", "xs", "start"))
    b.append(label(lx, 216, f"without t_d: off by {CUBIC['no_td']}°", "xs", "start"))
    desc = ("One Blender channel exported as a cubic spline: the left thigh's rotation over the one-"
            "second walk, as degrees from its first key. The file's Hermite curve passes through its "
            "nine keys; the imported linear keys, 61 of them marked as ticks on the time axis, follow "
            "it so closely the two lines coincide. The same curve evaluated with the tangents not "
            "scaled by the interval length is visibly flatter and misses by up to tens of degrees.")
    write("l77b_fig8.svg", svg(uid, 900, 318, "A cubic channel", desc, b))


# ===========================================================================
# Figure 9 — two exports of one motion  (§8.4)
# ===========================================================================
def fig_two():
    uid = "l77bf9"
    rows = read_csv("l77b_e_two.csv")
    b = []
    x0, x1, y0, y1 = 70, 860, 250, 60
    top = 1.0

    def P(t, v):
        return x0 + t * (x1 - x0), y0 - v / top * (y0 - y1)

    b.append(frame(x0, y1, x1 - x0, y0 - y1))
    for f in range(31):
        xx = P(f / 30.0, 0)[0]
        b.append(rule(xx, y0, xx, y0 + 4, "ink-soft"))
    b.append(poly([P(float(r["t"]), float(r["worst_deg"])) for r in rows], PURPLE, width=1.4, close=False))
    fy = P(0, TWO["frames"])[1]
    b.append(cline(x0, fy, x1, fy, GREEN, width=1.1, dash="4 3"))
    for v in (0, 0.25, 0.5, 0.75, 1.0):
        b.append(label(x0 - 6, P(0, v)[1] + 3, f"{v:g}", "xs mono", "end"))
    for t in (0, 0.25, 0.5, 0.75, 1):
        b.append(label(P(t, 0)[0], y0 + 17, f"{t:g}", "xs mono"))
    b.append(label((x0 + x1) / 2, y0 + 34, "time (s); ticks are Blender's 30 frames", "xs muted"))
    b.append(label(x0, y1 - 30, "the walk, imported from both exports: the worst joint's disagreement, degrees", "sm", "start"))
    b.append(label(x0, y1 - 14, f"up to {TWO['anywhere']}° between frames; dashed green: {TWO['frames']}°, the most on any frame — and the files alone, with no importer, differ by exactly that", "xs muted", "start"))
    desc = ("A line over the one-second walk showing the largest angle by which any joint differs "
            "between the clip imported from Blender's sampled export and the clip imported from its "
            "cubic-spline export. It is near zero at the keys, rises between them to peaks of up to "
            "0.916 degrees, and on the frames themselves never exceeds 0.218 degrees — the same "
            "amount the two files differ by when evaluated with the specification's formulas alone.")
    write("l77b_fig9.svg", svg(uid, 900, 300, "Two exports of one motion", desc, b))


# ===========================================================================
# Figure 10 — the joint nobody animates  (§10)
# ===========================================================================
def fig_neutral():
    uid = "l77bf10"
    rows = read_csv("l77b_h_nose.csv")
    b = [cmarker(uid, "r", RED)]
    xs = [float(r["x_fixed"]) for r in rows] + [float(r["x_unfixed"]) for r in rows]
    zs = [float(r["z_fixed"]) for r in rows] + [float(r["z_unfixed"]) for r in rows]
    cx, cz = (min(xs) + max(xs)) / 2, (min(zs) + max(zs)) / 2
    span = max(max(xs) - min(xs), max(zs) - min(zs)) * 1.08
    X0, Y0, S = 60, 40, 330.0

    def P(x, z):
        # top view: x right, +z DOWN the page toward the reader (the face)
        return X0 + (x - cx + span / 2) / span * S, Y0 + (z - cz + span / 2) / span * S

    fixed = [P(float(r["x_fixed"]), float(r["z_fixed"])) for r in rows if r["frozen"] == "0"]
    b.append(dots(fixed, "#d8b090", size=3.0, opacity=0.8))
    frozen = [r for r in rows if r["frozen"] == "1"]
    for r in frozen:
        a = P(float(r["x_fixed"]), float(r["z_fixed"]))
        u = P(float(r["x_unfixed"]), float(r["z_unfixed"]))
        b.append(dot(a[0], a[1], 2.6, GREEN))
        b.append(dot(u[0], u[1], 2.6, RED))
    b.append(hollow(X0 - 10, Y0 - 10, S + 20, S + 20, GREY, width=1.0))
    # scale bar: 2 cm
    sb = 0.02 / span * S
    b.append(cline(X0, Y0 + S + 30, X0 + sb, Y0 + S + 30, "var(--dia-ink)", width=2.0))
    b.append(label(X0 + sb + 8, Y0 + S + 34, "2 cm", "xs", "start"))
    b.append(label(X0, 22, "the head from above at t = 0.6 s of the wave; the face is at the bottom", "sm", "start"))
    lx = 440
    b.append(dot(lx + 6, 80, 3, GREEN))
    b.append(label(lx + 18, 84, "nose-tip vertices, fixed file: turned with the head", "xs", "start"))
    b.append(dot(lx + 6, 102, 3, RED))
    b.append(label(lx + 18, 106, "the same vertices, first export: on neutral_bone", "xs", "start"))
    b.append(dots([(lx + 6, 124)], "#d8b090", size=2.6))
    b.append(label(lx + 18, 128, "every other head vertex (both files agree)", "xs", "start"))
    b.append(label(lx, 170, f"{NEUTRAL['vertices']} vertices bone heat left unweighted.", "xs", "start"))
    b.append(label(lx, 186, "Blender's exporter gave them full weight on a", "xs", "start"))
    b.append(label(lx, 202, "joint it invented and nothing animates.", "xs", "start"))
    b.append(label(lx, 226, f"worst lag in the wave: {NEUTRAL['lag_mm']} mm", "xs mono", "start"))
    b.append(label(lx, 242, "frozen_vertices: 25 → 0 after the fix", "xs mono", "start"))
    desc = ("A top view of the mannequin's head at the moment of the wave where its turned head is "
            "furthest from the rest pose. Grey-brown dots are the head's vertices, identical in both "
            "files. Twenty-five nose-tip vertices are drawn twice: green where the fixed file puts "
            "them, turned with the head, and red where the first export leaves them, because they "
            "were weighted to a joint nothing animates; the two sets are up to 16.9 mm apart.")
    write("l77b_fig10.svg", svg(uid, 900, Y0 + S + 50, "The joint nobody animates", desc, b))


# ===========================================================================
# Figure 11 — which way it faces  (§11)
# ===========================================================================
def fig_facing():
    uid = "l77bf11"
    b = [cmarker(uid, "x", "var(--axis-x)"), cmarker(uid, "z", "var(--axis-z)"),
         cmarker(uid, "k", "var(--dia-ink)")]
    # top view: x right, z down the page.
    ox, oy = 250, 130
    b.append(label(30, 24, "seen from above (+y toward you)", "sm", "start"))
    b.append(carrow(ox, oy, ox + 90, oy, uid, "x", "var(--axis-x)", width=1.6))
    b.append(label(ox + 96, oy + 4, "+x", "xs lbl-x", "start"))
    b.append(carrow(ox, oy, ox, oy + 90, uid, "z", "var(--axis-z)", width=1.6))
    b.append(label(ox + 6, oy + 104, "+z", "xs lbl-z", "start"))
    # the character: head circle and nose
    b.append(f'<circle cx="{ox}" cy="{oy}" r="26" fill="none" stroke="{AMBER}" stroke-width="1.8"/>')
    b.append(f'<circle cx="{ox}" cy="{oy + 30}" r="7" fill="{BLUE}" stroke="none"/>')
    b.append(label(ox - 36, oy - 30, "head", "xs", "end"))
    b.append(label(ox - 16, oy + 50, "nose: z = +0.121", "xs mono", "end"))
    # camera at +z looking -z
    cy = oy + 200
    b.append(f'<path d="M{ox - 18},{cy + 16} L{ox + 18},{cy + 16} L{ox + 10},{cy - 2} L{ox - 10},{cy - 2} Z" fill="none" stroke="var(--dia-ink)" stroke-width="1.4"/>')
    b.append(carrow(ox, cy - 4, ox, oy + 60, uid, "k", "var(--dia-ink)", width=1.2, dash="5 4"))
    b.append(label(ox + 26, cy + 10, "camera at z = 3, looking down −z", "xs", "start"))
    # right: statements
    lx = 470
    b.append(label(lx, 60, "glTF §3.4: “+Y as up, +Z as forward, and -X as right;", "xs", "start"))
    b.append(label(lx, 76, "the front of a glTF asset faces +Z.”", "xs", "start"))
    b.append(label(lx, 108, "this engine: world −z is forward, and the camera", "xs", "start"))
    b.append(label(lx, 124, "looks down −z — at the +z face of whatever it sees", "xs", "start"))
    b.append(label(lx, 156, "so an unrotated glTF character LOOKS AT YOU:", "xs", "start"))
    b.append(label(lx, 172, "nose 2.879 m from the camera, head centre 3.000", "xs mono", "start"))
    b.append(label(lx, 204, "what differs is FORWARD: moved along the engine's", "xs", "start"))
    b.append(label(lx, 220, "forward, it walks backwards — a yaw fixes that,", "xs", "start"))
    b.append(label(lx, 236, "on the entity, not in the loader", "xs", "start"))
    desc = ("A top view with x to the right in red and z down the page in blue. The mannequin's head "
            "is a circle at the origin with its nose at z plus 0.121; a camera at z equals 3 looks "
            "down minus z toward it, so it sees the face. Text on the right quotes the glTF "
            "specification, that the front of an asset faces plus z, and states the consequence: "
            "an unrotated character faces the camera, and only its forward direction disagrees "
            "with the engine's.")
    write("l77b_fig11.svg", svg(uid, 900, 360, "Which way it faces", desc, b))


# ===========================================================================
# Figure 12 — the demo  (§16)
# ===========================================================================
def fig_demo():
    uid = "l77bf12"
    px = 2
    b = []
    panel, pw, ph = render_panel("l77b_render_wave.ppm", 250, 34, px)
    b.append(label(250, 22, "mannequin --clip wave --time 1.0 --shot: the second clip, one second in", "sm", "start"))
    b += panel
    b.append(label(250, 34 + ph + 16, "amber: the skin's joints; grey: the Armature node the importer carried in", "xs muted", "start"))
    desc = ("The demo's frame for the wave: a clay-coloured mannequin standing above a floor grid, "
            "facing the camera with the dark blue face plate on the front of its head, its right arm "
            "raised with the forearm upright, its skeleton drawn over it in amber lines with one grey "
            "line rising from the floor to the hips.")
    write("l77b_fig12.svg", svg(uid, 900, 34 + ph + 28, "The demo", desc, b))


FIGS = [fig_orders, fig_spec, fig_order_render, fig_influences, fig_xyzw, fig_lag, fig_step,
        fig_cubic, fig_two, fig_neutral, fig_facing, fig_demo]


def main():
    check_against_harness()
    for f in FIGS:
        f()
    print(f"wrote {len(FIGS)} figures")


if __name__ == "__main__":
    main()
