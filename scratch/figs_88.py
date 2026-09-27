#!/usr/bin/env python3
"""scratch/figs_88.py — Lesson 8.8's diagrams.

Same rules as 5.1-8.7's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - no hardcoded colour on an `.ink` stroke either
  - filenames numbered by PAGE ORDER
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - `rule()` takes a CSS CLASS; `cline()` takes a COLOUR
  - LEGENDS AND ANNOTATION BOXES GO OUTSIDE THE PLOT
  - a SHAPE can leave the viewBox where a label cannot
  - panel heights are COMPUTED, never guessed

Every number below is transcribed from scratch/verify_88.log. Nothing here is
estimated, and the harness section each block came from is named above it.

*** WRITTEN IN LITERAL UNICODE THROUGHOUT, NEVER \\uXXXX. ***

THE COLOUR RULE, inherited from 8.4 through 8.7:
  GREEN  = the answer that works / the grid
  RED    = the failure, and the quadratic
  AMBER  = what the algorithm produced — here, candidate pairs
  BLUE   = a proxy, and the cheap test
  PURPLE = the cell structure
  GREY   = discarded work

FIGURE 5 RUNS THE REAL OWNER RULE. `owner_cell` there is the header's function
transcribed, and the four meetings drawn in its left panel were ENUMERATED from
the two cell ranges rather than drawn by hand.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)
from figs_610 import cline                                          # noqa: E402
from figs_71 import cmarker, carrow, poly, frame                    # noqa: E402
from figs_76 import table                                           # noqa: E402
from figs_81 import legend, logspan                                 # noqa: E402

OUT = "scratch"
COLS = 118


def para(x, y, text, cols=COLS, cls="xs muted", leading=17, anchor="start"):
    """Wrap `text` to `cols` characters and emit one label per line.

    Returns (body, height): the height it took, so the next block goes at
    `y + height` and nothing is guessed.
    """
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


def table_body(*args, **kwargs):
    """`figs_76.table` returns (body, height); every table here wants the body."""
    body, _h = table(*args, **kwargs)
    return body


def logy(v, lo, hi, y0, height):
    """Map v onto [y0 + height, y0] with a log10 axis (y grows downward)."""
    t = (math.log10(max(v, lo)) - math.log10(lo)) / (math.log10(hi) - math.log10(lo))
    return y0 + height - min(max(t, 0.0), 1.0) * height


def write(name, text):
    with open(os.path.join(OUT, name), "w") as fh:
        fh.write(text)
    print(f"{name}: {len(text)} bytes")


# ===========================================================================
# MEASUREMENTS — every one from scratch/verify_88.log, section named
# ===========================================================================

NARROW_NS = 432.4          # §A, resting crates, 8.7's own fixture
TUMBLE_NS = 795.9          # §A, freely rotated and deep
AABB_NS = 1.190            # §A, one overlaps(aabb, aabb)
WALL_NARROW = 278          # §A, where n(n-1)/2 * NARROW_NS passes 16.67 ms
WALL_AABB = 5293           # §A, the same for AABB_NS

# §B: n, true pairs, brute ms, grid ms
GRID_VS_BRUTE = [
    (64, 12, 0.001, 0.002),
    (128, 42, 0.007, 0.005),
    (256, 54, 0.050, 0.009),
    (512, 125, 0.312, 0.018),
    (1024, 315, 1.284, 0.036),
    (2048, 648, 4.586, 0.075),
    (4096, 1283, 18.276, 0.181),
    (8192, 2463, 71.696, 0.521),
]

# §I: the crossover sweep
CROSSOVER = [
    (8, 0.00, 0.21), (16, 0.04, 0.50), (24, 0.12, 0.62), (32, 0.29, 0.96),
    (48, 0.71, 1.42), (64, 1.17, 2.08), (96, 3.29, 2.71), (128, 5.00, 4.00),
    (192, 17.83, 5.58), (256, 36.54, 9.08),
]

# §E sparse: cell m, entries, bucket tests, best ms
CELL_SWEEP_SPARSE = [
    (0.10, 15329076, 4735790, 264.430),
    (0.25, 1288767, 332410, 15.981),
    (0.50, 241907, 91639, 2.436),
    (1.00, 60434, 31728, 0.680),
    (1.50, 30656, 22129, 0.393),
    (2.00, 20752, 19105, 0.242),
    (2.50, 16109, 22747, 0.186),
    (3.00, 13314, 24223, 0.192),
    (4.00, 10324, 31306, 0.212),
    (5.00, 8917, 45228, 0.266),
    (8.00, 6509, 93129, 0.556),
    (15.00, 5484, 429340, 2.350),
    (40.00, 4655, 4328910, 19.860),
]
MODEL_A = 8.09e-6          # ms per entry      (§E sparse fit)
MODEL_B = 5.45e-6          # ms per pair test  (§E sparse fit)
MODEL_MIN_SPARSE = 3.30    # m, = 2.13 x the mean longest side
MODEL_MIN_DENSE = 2.05     # m, = 1.32 x the dense scene's mean longest side
MODEL_MIN_DENSE_RATIO = 1.32  # as the harness prints it
MEAN_SIDE = 1.551          # m, §E sparse
AUTO_CELL = 2.33           # m, auto_cell_size on that scene

# §F
HASH_ROWS = [("lattice", 1445, 4096, 254, 209, 202),
             ("scattered", 7301, 16384, 1175, 1240, 1215)]
LOAD_ROWS = [(1, 2048, 346, 323), (2, 4096, 254, 202),
             (4, 8192, 176, 113), (8, 16384, 156, 60)]
XOR_TESTS, MIX_TESTS = 99852, 90514
XOR_NS, MIX_NS, CALL_NS = 0.115, 0.231, 0.692

# §G: margin m, pairs, ratio, predicted
MARGIN_ROWS = [(0.00, 833, 1.000, 1.000), (0.01, 879, 1.055, 1.042),
               (0.05, 1005, 1.206, 1.221), (0.10, 1186, 1.424, 1.471),
               (0.25, 1918, 2.303, 2.425), (0.50, 3770, 4.526, 4.800)]

# §H
TEAPOT = [("crates only", 30225, 1090), ("+ plate, guard off", 111027, 1127),
          ("+ plate, guard on", 30225, 1127)]
PLATE_CELLS = [(4.00, 5202), (2.00, 20402), (1.00, 80802),
               (0.50, 321602), (0.25, 1283202)]

# §I: the frame
BROAD_MS, NARROW_MS, IMPLIED_MS = 0.091, 0.426, 843.6
CANDIDATES, SCENE_N = 983, 2000

# §C, §D
MISSED, FRAMES_EQUAL, TRUE_PAIRS = 0, 12, 5230
TRUNC_LOST, TRUNC_EXCESS = 0, 4.0
CENTRE_LOST, CENTRE_TOTAL = 1892, 2836
DUP_PRED, DUP_COUNT, DUP_PAIRS = 3271, 3271, 963
SET_MS, SORT_MS, GRID_MS = 0.036, 0.004, 0.080


# ===========================================================================
# Figure 1 — the quadratic is already here
# ===========================================================================

def fig1():
    uid = "f88a"
    W = 900
    body = [cmarker(uid, "red", RED), cmarker(uid, "grn", GREEN), cmarker(uid, "blu", BLUE)]

    px, py, pw, ph = 70, 40, 520, 300
    body.append(frame(px, py, pw, ph))
    body.append(label(px + pw / 2, py - 14, "the cost of testing every pair, against a 16.67 ms frame",
                      "sm"))

    n_lo, n_hi = 10.0, 30000.0
    t_lo, t_hi = 1e-3, 3e5

    # axes
    for n in (10, 100, 1000, 10000):
        x = logspan(n, n_lo, n_hi, px, pw)
        body.append(rule(x, py, x, py + ph, cls="grid", width=0.6))
        body.append(label(x, py + ph + 16, f"{n:,}", "xs muted"))
    for t in (1e-3, 1e-2, 1e-1, 1, 10, 100, 1000, 1e4, 1e5):
        y = logy(t, t_lo, t_hi, py, ph)
        body.append(rule(px, y, px + pw, y, cls="grid", width=0.6))
        txt = f"{t:g}" if t >= 1 else f"{t:g}"
        body.append(label(px - 8, y + 4, txt, "xs muted", "end"))
    body.append(label(px + pw / 2, py + ph + 34, "n, bodies in the scene", "xs muted"))
    body.append(label(px - 46, py + ph / 2, "ms", "xs muted"))

    def curve(ns_per_pair, colour, width=2.0):
        # CLIPPED, not clamped. `logy` clamps, so a curve that starts below the
        # axis draws a flat run along the bottom — which on a log-log cost plot
        # reads as "the cost is constant down here", the exact opposite of the
        # figure's point. Drop the points instead.
        pts = []
        n = n_lo
        while n <= n_hi:
            ms = 0.5 * n * (n - 1) * ns_per_pair * 1e-6
            if ms >= t_lo:
                pts.append((logspan(n, n_lo, n_hi, px, pw), logy(ms, t_lo, t_hi, py, ph)))
            n *= 1.15
        return poly(pts, colour, width=width, close=False)

    body.append(curve(NARROW_NS, RED, 2.2))
    body.append(curve(AABB_NS, BLUE, 2.2))

    # the measured grid, §B
    gpts = [(logspan(n, n_lo, n_hi, px, pw), logy(g, t_lo, t_hi, py, ph))
            for n, _p, _b, g in GRID_VS_BRUTE]
    body.append(poly(gpts, GREEN, width=2.4, close=False))
    for x, y in gpts:
        body.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="2.6" fill="{GREEN}"/>')

    # the budget
    yb = logy(16.666, t_lo, t_hi, py, ph)
    # THE LABEL FOR THIS LINE LIVES IN THE LEGEND, NOT ON IT. An annotation
    # inside a plot that contains three curves will land on one of them; the
    # rule figs_87 states is that legends and annotations go OUTSIDE the frame,
    # and check-page.js enforces it (`svgTextOnShape` samples the actual stroke).
    body.append(cline(px, yb, px + pw, yb, AMBER, width=1.6, dash="6 4"))

    # where each crosses it
    for n, colour, txt in ((WALL_NARROW, RED, f"n = {WALL_NARROW}"),
                           (WALL_AABB, BLUE, f"n = {WALL_AABB:,}")):
        x = logspan(n, n_lo, n_hi, px, pw)
        body.append(f'<circle cx="{x:.1f}" cy="{yb:.1f}" r="4" fill="none" '
                    f'stroke="{colour}" stroke-width="1.8"/>')
        body.append(label(x, yb + 20, txt, "xs", "middle"))

    lx = px + pw + 40
    body += legend(lx, py + 14, [
        (RED, f"every pair to the narrow phase — {NARROW_NS:.0f} ns each"),
        (BLUE, f"every pair, AABB only — {AABB_NS:.3f} ns each"),
        (GREEN, "the uniform grid, measured (§2)"),
        (AMBER, "one 60 Hz frame — 16.67 ms"),
    ])

    rows = [(f"{n:,}", f"{b:.3f}", f"{g:.3f}", f"{b / g:.1f}×")
            for n, _p, b, g in GRID_VS_BRUTE]
    body += table_body(lx, py + 108, ["n", "brute", "grid", "×"], rows,
                       [58, 66, 62, 52], title="§2 — same scene, same pairs (ms)")

    b, h = para(px, py + ph + 62,
                f"The narrow phase is {NARROW_NS / AABB_NS:.0f}× the price of the AABB test that "
                f"stands in for it, so substituting one for the other moves the wall from "
                f"{WALL_NARROW} bodies to {WALL_AABB:,} — and does not remove it, because both "
                f"curves are the same quadratic with different constants. Only the green one "
                f"changes shape, and it is the only one that is not testing every pair.")
    body += b
    H = int(py + ph + 62 + h + 16)
    return svg(uid, W, H, "The quadratic wall",
               "Log-log plot of frame cost against scene size for three strategies.", body)


# ===========================================================================
# Figure 2 — the two stages
# ===========================================================================

def fig2():
    uid = "f88b"
    W = 900
    body = [cmarker(uid, "grn", GREEN), cmarker(uid, "gry", GREY), cmarker(uid, "amb", AMBER)]

    y = 56
    bw, bh = 150, 64
    xs = [40, 250, 460, 670]

    stages = [(f"{SCENE_N:,} proxies", "a box and an index", BLUE),
              ("broadphase", f"{BROAD_MS:.3f} ms", GREEN),
              (f"{CANDIDATES} candidate pairs", "a superset, never a subset", AMBER),
              ("narrow phase", f"{NARROW_MS:.3f} ms", GREEN)]
    for i, (title, sub, colour) in enumerate(stages):
        body.append(hollow(xs[i], y, bw, bh, colour, width=1.6))
        body.append(label(xs[i] + bw / 2, y + 26, title, "sm"))
        body.append(label(xs[i] + bw / 2, y + 45, sub, "xs muted"))
        if i:
            body.append(carrow(xs[i - 1] + bw + 8, y + bh / 2, xs[i] - 8, y + bh / 2,
                               uid, "grn", GREEN, width=2.0))

    # what is avoided
    y2 = y + bh + 66
    body.append(hollow(xs[1], y2, bw * 2 + 60, bh, GREY, width=1.3, dash="5 4"))
    pairs = SCENE_N * (SCENE_N - 1) // 2
    body.append(label(xs[1] + (bw * 2 + 60) / 2, y2 + 26,
                      f"{pairs:,} pairs straight to the narrow phase", "sm"))
    body.append(label(xs[1] + (bw * 2 + 60) / 2, y2 + 45,
                      f"{IMPLIED_MS:.0f} ms — {IMPLIED_MS / (BROAD_MS + NARROW_MS):.0f}× the "
                      f"whole stage above", "xs muted"))
    body.append(carrow(xs[0] + bw / 2, y + bh + 6, xs[1] + 30, y2 - 8, uid, "gry", GREY,
                       width=1.4, dash="5 4"))
    body.append(carrow(xs[1] + bw * 2 + 60 - 30, y2 - 8, xs[3] + bw / 2, y + bh + 6,
                       uid, "gry", GREY, width=1.4, dash="5 4"))

    b, h = para(40, y2 + bh + 34,
                f"The broadphase does not make the narrow phase faster. It makes there be "
                f"{pairs / CANDIDATES:.0f}× less of it — {CANDIDATES} pairs instead of "
                f"{pairs:,} — and that is the whole of the arrangement. It costs "
                f"{100 * BROAD_MS / (BROAD_MS + NARROW_MS):.0f}% of the two stages together "
                f"(§10), which is the price of admission and the reason the stage exists as a "
                f"separate stage at all: it has to be so much cheaper per pair that running it "
                f"on everything is still cheaper than running the real test on a fraction.")
    body += b
    H = int(y2 + bh + 34 + h + 16)
    return svg(uid, W, H, "The two stages",
               "A flow diagram of proxies through the broadphase into the narrow phase.", body)


# ===========================================================================
# Figure 3 — the contract
# ===========================================================================

def fig3():
    uid = "f88c"
    W = 900
    body = [cmarker(uid, "red", RED), cmarker(uid, "grn", GREEN), cmarker(uid, "amb", AMBER)]

    cx, cy = 250, 150
    body.append(f'<ellipse cx="{cx}" cy="{cy}" rx="200" ry="112" fill="none" '
                f'stroke="{GREY}" stroke-width="1.4" stroke-dasharray="5 4"/>')
    body.append(f'<ellipse cx="{cx}" cy="{cy + 8}" rx="132" ry="78" fill="none" '
                f'stroke="{AMBER}" stroke-width="1.8"/>')
    body.append(f'<ellipse cx="{cx}" cy="{cy + 22}" rx="66" ry="44" fill="none" '
                f'stroke="{GREEN}" stroke-width="2.0"/>')

    body.append(label(cx, cy - 92, "every pair — n(n−1)/2", "xs muted"))
    body.append(label(cx, cy - 50, "what the broadphase reports", "xs"))
    body.append(label(cx, cy + 26, "pairs that really touch", "xs"))

    # the two kinds of wrong
    ax = 500
    body.append(carrow(cx + 96, cy - 26, ax - 10, cy - 46, uid, "amb", AMBER, width=1.6))
    body.append(label(ax, cy - 50, "A FALSE POSITIVE lives here.", "sm", "start"))
    b1, h1 = para(ax, cy - 30,
                  f"It costs exactly one narrow-phase call — {NARROW_NS:.0f} ns — and the answer "
                  f"that comes back is correct. The broadphase is allowed as many of these as it "
                  f"likes.", cols=52)
    body += b1

    body.append(carrow(cx + 40, cy + 58, ax - 10, cy + 54, uid, "red", RED, width=1.8))
    body.append(label(ax, cy + 48, "A FALSE NEGATIVE cannot live here.", "sm", "start"))
    b2, h2 = para(ax, cy + 68,
                  "There is no stage downstream that could notice. The pair is simply never "
                  "asked about, the objects pass through each other, and the bug presents as "
                  "“sometimes things fall through the floor” with no line of code to "
                  "blame.", cols=52)
    body += b2

    ytab = cy + 68 + max(h2, 0) + 26
    rows = [("pairs the grid missed", f"{MISSED}"),
            ("frames the two sets matched", f"{FRAMES_EQUAL} / 12"),
            ("true pairs over those frames", f"{TRUE_PAIRS:,}")]
    body += table_body(60, ytab, ["§3 — checked against the oracle", "measured"], rows,
                       [300, 110])
    b3, h3 = para(60, ytab + 20 + 3 * 17 + 26,
                  "Every approximation in this file is therefore made in one direction on "
                  "purpose: round the cell range outward, keep a proxy the grid cannot place "
                  "rather than dropping it, and reject a candidate only when a cheap exact test "
                  "says so. The grid in fact achieves equality rather than containment, because "
                  "it runs the AABB test itself — but it is the containment that is the contract, "
                  "and it is the containment 8.9's speculative margin will start leaning on.")
    body += b3
    H = int(ytab + 20 + 3 * 17 + 26 + h3 + 16)
    return svg(uid, W, H, "The broadphase contract",
               "Nested sets showing that the broadphase's output must contain the true pairs.",
               body)


# ===========================================================================
# Figure 4 — the cell map, and two ways to get it wrong
# ===========================================================================

def fig4():
    uid = "f88d"
    W = 900
    body = [cmarker(uid, "red", RED), cmarker(uid, "grn", GREEN), cmarker(uid, "blu", BLUE)]

    # ---- panel A: the range walk ------------------------------------------
    ax, ay, cell = 50, 56, 44
    body.append(label(ax, ay - 16, "A  a proxy occupies a RANGE of cells", "sm", "start"))
    for i in range(6):
        body.append(rule(ax + i * cell, ay, ax + i * cell, ay + 4 * cell, cls="grid"))
        body.append(rule(ax, ay + i % 5 * cell, ax + 5 * cell, ay + i % 5 * cell, cls="grid"))
    body.append(rule(ax + 5 * cell, ay, ax + 5 * cell, ay + 4 * cell, cls="grid"))

    # a box spanning cells 1..3 in x and 1..2 in y
    bx0, by0 = ax + 1.35 * cell, ay + 1.2 * cell
    bx1, by1 = ax + 3.55 * cell, ay + 2.7 * cell
    body.append(hollow(bx0, by0, bx1 - bx0, by1 - by0, BLUE, width=2.0))
    # SQUARE corners and a stroke-free fill: with rx=3 and a full-opacity stroke
    # (which `box` always emits) these read as six little boxes sitting inside a
    # big one rather than as six cells of the lattice being claimed.
    for gx in range(1, 4):
        for gy in range(1, 3):
            body.append(f'<rect x="{ax + gx * cell + 1}" y="{ay + gy * cell + 1}" '
                        f'width="{cell - 2}" height="{cell - 2}" fill="{BLUE}" '
                        f'fill-opacity="0.16" stroke="none"/>')
    body.append(label((bx0 + bx1) / 2, by0 - 8, "one proxy", "xs", "middle"))
    body.append(label(ax + 2.5 * cell, ay + 4 * cell + 18,
                      "cells x ∈ [1, 3], y ∈ [1, 2] — six insertions", "xs muted"))

    # ---- panel B: floor vs truncate ---------------------------------------
    bxs, bys = 380, 76
    body.append(label(bxs, bys - 36, "B  floor, and the cast everybody warns about", "sm", "start"))
    span, unit = 480, 48
    origin = bxs + span / 2
    body.append(cline(bxs, bys, bxs + span, bys, GREY, width=1.2))
    body.append(cline(bxs, bys + 54, bxs + span, bys + 54, GREY, width=1.2))
    for k in range(-5, 6):
        x = origin + k * unit
        if x < bxs or x > bxs + span:
            continue
        body.append(rule(x, bys - 8, x, bys + 8, cls="grid"))
        body.append(rule(x, bys + 46, x, bys + 62, cls="grid"))
    for k in range(-5, 5):
        xm = origin + (k + 0.5) * unit
        if xm < bxs or xm > bxs + span:
            continue
        body.append(label(xm, bys - 14, str(k), "xs muted"))
    # truncating: cells -1 and 0 merge
    for k in range(-5, 5):
        xm = origin + (k + 0.5) * unit
        if xm < bxs or xm > bxs + span:
            continue
        tk = k + 1 if k < 0 else k
        body.append(label(xm, bys + 76, str(tk), "xs muted"))
    body.append(box(origin - unit, bys + 46, 2 * unit, 16, colour=RED, opacity=0.22, rx=2,
                    width=1.0))
    body.append(label(bxs - 10, bys + 4, "floor", "xs", "end"))
    body.append(label(bxs - 10, bys + 58, "cast", "xs", "end"))
    body.append(label(origin, bys + 96, "one cell, twice as wide on every axis", "xs", "middle"))

    ytab = max(ay + 4 * cell + 40, bys + 112)
    rows = [("pairs lost by the cast", f"{TRUNC_LOST}"),
            ("pairs lost by floor", "0"),
            ("comparisons near the origin, cast ÷ floor", f"{TRUNC_EXCESS:.1f}×"),
            ("pairs lost by inserting the CENTRE cell only",
             f"{CENTRE_LOST:,} of {CENTRE_TOTAL:,}  ({100 * CENTRE_LOST / CENTRE_TOTAL:.1f}%)")]
    body += table_body(50, ytab + 18, ["§4 — measured", "result"], rows, [420, 260])

    b, h = para(50, ytab + 18 + 20 + 4 * 17 + 26,
                "The cast is not a correctness bug HERE, and the reason is one line of order "
                "theory: a range-walk grid needs its cell map to be MONOTONE and needs nothing "
                "else, because if two intervals overlap then so do their images under a "
                "non-decreasing map. Truncation is non-decreasing. What it costs is occupancy — "
                "eight cells become one, and a cell's pair loop is quadratic in what is inside "
                "it. The mistake that does lose pairs is the other one, and it is the natural "
                "first draft: objects are small and cells are big, so put each proxy in the one "
                "cell containing its centre. That loses two thirds of the pairs there are.")
    body += b
    H = int(ytab + 18 + 20 + 4 * 17 + 26 + h + 16)
    return svg(uid, W, H, "The cell map",
               "A proxy's cell range, and the difference between floor and truncation.", body)


# ===========================================================================
# Figure 5 — duplicates and the cell that owns a pair
# ===========================================================================

def fig5():
    uid = "f88e"
    W = 900
    body = [cmarker(uid, "grn", GREEN), cmarker(uid, "amb", AMBER)]

    ax, ay, cell = 56, 60, 52
    body.append(label(ax, ay - 18, "two proxies whose cell ranges overlap in four cells",
                      "sm", "start"))
    for i in range(7):
        body.append(rule(ax + i * cell, ay, ax + i * cell, ay + 5 * cell, cls="grid"))
    for j in range(6):
        body.append(rule(ax, ay + j * cell, ax + 6 * cell, ay + j * cell, cls="grid"))

    # ranges: A x[0,3] y[0,2], B x[2,5] y[1,4] -> overlap x[2,3] y[1,2] = 2x2
    ra = (0, 3, 0, 2)
    rb = (2, 5, 1, 4)
    body.append(hollow(ax + ra[0] * cell + 5, ay + ra[2] * cell + 5,
                       (ra[1] - ra[0] + 1) * cell - 10, (ra[3] - ra[2] + 1) * cell - 10,
                       BLUE, width=2.0))
    body.append(hollow(ax + rb[0] * cell + 11, ay + rb[2] * cell + 11,
                       (rb[1] - rb[0] + 1) * cell - 22, (rb[3] - rb[2] + 1) * cell - 22,
                       RED, width=2.0))

    ox0, ox1 = max(ra[0], rb[0]), min(ra[1], rb[1])
    oy0, oy1 = max(ra[2], rb[2]), min(ra[3], rb[3])
    meetings = [(gx, gy) for gy in range(oy0, oy1 + 1) for gx in range(ox0, ox1 + 1)]
    for gx, gy in meetings:
        owner = (gx == ox0 and gy == oy0)
        colour = GREEN if owner else GREY
        body.append(box(ax + gx * cell + 2, ay + gy * cell + 2, cell - 4, cell - 4,
                        colour=colour, opacity=0.26 if owner else 0.14, rx=2, width=0.8))
        body.append(label(ax + gx * cell + cell / 2, ay + gy * cell + cell / 2 + 4,
                          "owner" if owner else "dup", "xs"))

    body.append(label(ax + 3 * cell, ay + 5 * cell + 20,
                      f"the pair meets in all {len(meetings)}; "
                      f"owner_cell picks the minimum corner", "xs muted"))

    tx = ax + 6 * cell + 46
    body.append(label(tx, ay - 18, "owner_cell(lo_a, lo_b) = max(lo_a, lo_b), per axis", "sm",
                      "start"))
    b1, h1 = para(tx, ay + 6,
                  "The overlap of two axis-aligned cell RANGES is itself an axis-aligned range, "
                  "and a range has exactly one minimum corner. Both proxies are in it by "
                  "construction, because it lies inside both their ranges. So reporting only "
                  "from there reports the pair exactly once — not usually, exactly — for three "
                  "max calls and no memory.", cols=52)
    body += b1

    rows = [("duplicate meetings, closed form", f"{DUP_PRED:,}"),
            ("duplicate meetings, counted", f"{DUP_COUNT:,}"),
            ("pairs reported", f"{DUP_PAIRS}"),
            ("duplicates in the reported list", "0")]
    body += table_body(tx, ay + 6 + h1 + 24, ["§5 — 2000 proxies", ""], rows, [240, 90])

    ytab = ay + 5 * cell + 52
    rows2 = [("std::set of pair keys", f"{SET_MS:.3f} ms",
              f"{100 * SET_MS / GRID_MS:.0f}% of the broadphase"),
             ("sort + unique", f"{SORT_MS:.3f} ms",
              f"{100 * SORT_MS / GRID_MS:.0f}%"),
             ("owner_cell", "0.000 ms", "three max calls")]
    body += table_body(56, ytab + 18, ["the three ways to deduplicate", "cost", "against "
                                       f"{GRID_MS:.3f} ms"], rows2, [250, 110, 220])

    H = int(ytab + 18 + 20 + 3 * 17 + 30)
    return svg(uid, W, H, "Duplicates and the owner cell",
               "Two proxies meeting in four shared cells, and the rule that reports them once.",
               body)


# ===========================================================================
# Figure 6 — the cell size
# ===========================================================================

def fig6():
    uid = "f88f"
    W = 900
    body = [cmarker(uid, "grn", GREEN)]

    px, py, pw, ph = 74, 46, 500, 300
    body.append(frame(px, py, pw, ph))
    body.append(label(px + pw / 2, py - 14,
                      "both wings are cubic, which is why the floor is flat", "sm"))

    c_lo, c_hi = 0.08, 50.0
    v_lo, v_hi = 1e3, 2e7

    for c in (0.1, 1.0, 10.0):
        x = logspan(c, c_lo, c_hi, px, pw)
        body.append(rule(x, py, x, py + ph, cls="grid", width=0.6))
        body.append(label(x, py + ph + 16, f"{c:g} m", "xs muted"))
    for v in (1e3, 1e4, 1e5, 1e6, 1e7):
        y = logy(v, v_lo, v_hi, py, ph)
        body.append(rule(px, y, px + pw, y, cls="grid", width=0.6))
        body.append(label(px - 8, y + 4, f"1e{int(math.log10(v))}", "xs muted", "end"))
    body.append(label(px + pw / 2, py + ph + 34, "cell size", "xs muted"))
    body.append(label(px - 52, py + ph / 2, "count", "xs muted"))

    ent = [(logspan(c, c_lo, c_hi, px, pw), logy(e, v_lo, v_hi, py, ph))
           for c, e, _t, _m in CELL_SWEEP_SPARSE]
    tst = [(logspan(c, c_lo, c_hi, px, pw), logy(t, v_lo, v_hi, py, ph))
           for c, _e, t, _m in CELL_SWEEP_SPARSE]
    body.append(poly(ent, BLUE, width=2.2, close=False))
    body.append(poly(tst, RED, width=2.2, close=False))

    # the model, in the same units: (a*E + b*T) / a, so it plots beside the counts
    model = []
    for c, e, t, _m in CELL_SWEEP_SPARSE:
        v = (MODEL_A * e + MODEL_B * t) / MODEL_A
        model.append((logspan(c, c_lo, c_hi, px, pw), logy(v, v_lo, v_hi, py, ph)))
    body.append(poly(model, GREEN, width=2.6, close=False))

    xmin = logspan(MODEL_MIN_SPARSE, c_lo, c_hi, px, pw)
    body.append(cline(xmin, py, xmin, py + ph, AMBER, width=1.6, dash="6 4"))
    body.append(label(xmin, py + 16, f"  model minimum {MODEL_MIN_SPARSE:.2f} m", "xs", "start"))
    xauto = logspan(AUTO_CELL, c_lo, c_hi, px, pw)
    body.append(cline(xauto, py + ph - 40, xauto, py + ph, GREEN, width=1.6))
    body.append(label(xauto, py + ph - 48, f"auto {AUTO_CELL:.2f} m", "xs", "middle"))

    lx = px + pw + 34
    body += legend(lx, py + 14, [
        (BLUE, "entries — one per (proxy, cell)"),
        (RED, "pair tests inside buckets"),
        (GREEN, "the cost model, a·entries + b·tests"),
        (AMBER, "where the model is least"),
    ])

    rows = [(f"{c:g}", f"{e:,}", f"{t:,}", f"{m:.3f}")
            for c, e, t, m in CELL_SWEEP_SPARSE[3:11]]
    body += table_body(lx, py + 110, ["cell m", "entries", "tests", "ms"], rows,
                       [58, 86, 78, 56], title="§6 — 4000 proxies, sparse")

    b, h = para(px - 24, py + ph + 62,
                f"Halve the cell and each object claims eight times as many of them; double it "
                f"and each cell holds eight times as many objects, whose pair loop is quadratic. "
                f"Both wings go as the cube, so the minimum sits where a·E(s) = b·T(s) and moves "
                f"as the SIXTH ROOT of the scene's density — which is why one formula that never "
                f"looks at density can serve. Measured: the model's optimum is "
                f"{MODEL_MIN_SPARSE:.2f} m ({MODEL_MIN_SPARSE / MEAN_SIDE:.2f}× the mean longest "
                f"side) on this scene and {MODEL_MIN_DENSE:.2f} m "
                f"({MODEL_MIN_DENSE_RATIO:.2f}×) on one four times denser. The two constants "
                f"came out at {MODEL_A * 1e6:.1f} and {MODEL_B * 1e6:.1f} nanoseconds on BOTH "
                f"scenes, because they are properties of the machine rather than of the crowd.")
    body += b
    H = int(py + ph + 62 + h + 16)
    return svg(uid, W, H, "Choosing the cell size",
               "Entries and pair tests against cell size, with the fitted cost model.", body)


# ===========================================================================
# Figure 7 — hashing rather than allocating
# ===========================================================================

def fig7():
    uid = "f88g"
    W = 900
    body = [cmarker(uid, "red", RED), cmarker(uid, "grn", GREEN)]

    y = 56
    # dense array
    body.append(label(50, y - 16, "a dense array of cells", "sm", "start"))
    gw, gh, g = 300, 150, 15
    for i in range(0, gw + 1, g):
        body.append(rule(50 + i, y, 50 + i, y + gh, cls="grid", width=0.5))
    for j in range(0, gh + 1, g):
        body.append(rule(50, y + j, 50 + gw, y + j, cls="grid", width=0.5))
    for (gx, gy) in ((4, 3), (5, 3), (9, 6), (14, 2), (13, 7), (8, 4)):
        body.append(box(50 + gx * g + 1, y + gy * g + 1, g - 2, g - 2, colour=BLUE,
                        opacity=0.6, rx=1, width=0.6))
    body.append(label(50, y + gh + 20, "memory ∝ the WORLD's volume; a 1 km world at a 1 m",
                      "xs muted", "start"))
    body.append(label(50, y + gh + 37, "cell is 10⁹ cells — and it has an EDGE to fall off",
                      "xs muted", "start"))

    # hash table
    hx = 430
    body.append(label(hx, y - 16, "a hash of the occupied cells", "sm", "start"))
    slots = 16
    sw = 22
    for i in range(slots):
        body.append(hollow(hx + i * sw, y + 40, sw - 3, 26, GREY, width=0.9, rx=2))
    filled = {2: 1, 3: 2, 6: 1, 7: 1, 11: 1, 12: 1}
    for i, n in filled.items():
        body.append(box(hx + i * sw + 1, y + 41, sw - 5, 24, colour=GREEN if n == 1 else AMBER,
                        opacity=0.55, rx=2, width=0.6))
        if n > 1:
            body.append(label(hx + i * sw + (sw - 3) / 2, y + 84, "2 cells", "xs"))
    body.append(label(hx, y + gh + 20, "memory ∝ the SCENE; no bounds to declare, no edge,",
                      "xs muted", "start"))
    body.append(label(hx, y + gh + 37, "and a collision costs a comparison, never an answer",
                      "xs muted", "start"))

    ytab = y + gh + 66
    rows = [(name, f"{cells:,}", f"{buckets:,}", f"{xo}", f"{mx}", f"{ideal}")
            for name, cells, buckets, xo, mx, ideal in HASH_ROWS]
    body += table_body(50, ytab + 18,
                       ["§7 — buckets holding 2+ cells", "cells", "buckets", "xor", "mix",
                        "chance"],
                       rows, [186, 62, 68, 50, 50, 58],
                       title="the textbook hash, measured against balls in bins")

    rows2 = [(f"1 : {r}", f"{b:,}", f"{s}", f"{i}") for r, b, s, i in LOAD_ROWS]
    # AT 600, NOT 590. The first table now ends at 524; two tables whose columns
    # abut read as one table with eleven columns, and the reader parses
    # "202  1:1" as a single row.
    body += table_body(600, ytab + 18, ["load", "buckets", "shared", "chance"], rows2,
                       [60, 76, 68, 68], title="and the load factor")

    y2 = ytab + 18 + 20 + max(len(rows), len(rows2)) * 17 + 28
    b, h = para(50, y2,
                f"On a lattice of crates — which is what a physics scene looks like the moment it "
                f"settles — the textbook hash puts {HASH_ROWS[0][3]} pairs of cells in a shared "
                f"bucket where chance would put {HASH_ROWS[0][5]}, and a stronger mix puts "
                f"{HASH_ROWS[0][4]}. It stays, and the measurement is also why: those extra "
                f"collisions are worth {100 * (XOR_TESTS - MIX_TESTS) / XOR_TESTS:.1f}% of the "
                f"inner loop ({XOR_TESTS:,} pair tests against {MIX_TESTS:,}), and the hash that "
                f"removes them costs {MIX_NS / XOR_NS:.1f}× as much per call "
                f"({XOR_NS:.3f} ns against {MIX_NS:.3f}). Note the load-factor table's last "
                f"column: the gap to chance WIDENS as the table grows, which is this hash's "
                f"signature and the reason buying a bigger table is not the fix.")
    body += b
    H = int(y2 + h + 16)
    return svg(uid, W, H, "Hashing rather than allocating",
               "A dense cell array against a hash of the occupied cells, with collision counts.",
               body)


# ===========================================================================
# Figure 8 — the margin
# ===========================================================================

def fig8():
    uid = "f88h"
    W = 900
    body = [cmarker(uid, "grn", GREEN)]

    # THE LAYOUT IS THE FIGURE'S HARD PART, and the first draft got it wrong in
    # all three ways check-page.js can see: a title long enough to run under the
    # table beside it, a table whose top row sat level with that title, and a
    # paragraph placed at a y computed from the TABLE's height when the DIAGRAM
    # was taller. Every block below reports where it ended, and the next one
    # starts from the maximum.
    ax, ay = 60, 92
    s = 34
    body.append(label(ax, ay - 40, "the Minkowski sum decides which pairs overlap", "sm",
                      "start"))

    body.append(hollow(ax, ay, 2.2 * s, 1.6 * s, BLUE, width=1.8))
    body.append(label(ax + 1.1 * s, ay - 8, "box A, extent e_a", "xs"))
    body.append(hollow(ax + 3.2 * s, ay + 0.2 * s, 1.6 * s, 1.2 * s, RED, width=1.8))
    body.append(label(ax + 4.0 * s, ay - 8, "box B, e_b", "xs"))

    sx, sy = ax + 0.6 * s, ay + 2.8 * s
    body.append(hollow(sx, sy, 3.8 * s, 2.4 * s, AMBER, width=2.0))
    body.append(label(sx + 1.9 * s, sy + 1.4 * s, "e_a + e_b", "xs"))
    body.append(hollow(sx - 0.5 * s, sy - 0.5 * s, 4.8 * s, 3.4 * s, GREEN, width=1.8,
                       dash="5 4"))
    body.append(label(sx + 1.9 * s, sy - 0.5 * s - 10, "e_a + e_b + 4m", "xs"))

    diagram_bottom = sy + 2.9 * s + 8
    body.append(label(ax, diagram_bottom + 16,
                      "fattening each box by m adds 2m to its own extent,", "xs muted", "start"))
    body.append(label(ax, diagram_bottom + 33,
                      "and therefore 4m to the sum", "xs muted", "start"))
    diagram_bottom += 33

    tx = 470
    rows = [(f"{m:.2f}", f"{p:,}", f"{r:.3f}", f"{pr:.3f}", "yes")
            for m, p, r, pr in MARGIN_ROWS]
    body += table_body(tx, ay - 10,
                       ["margin m", "pairs", "measured", "((E+4m)/E)³", "contains m=0"], rows,
                       [74, 62, 74, 88, 90],
                       title="§8 — 4000 proxies, mean summed extent 2.912 m")
    table_bottom = ay - 10 + 20 + len(rows) * 17

    y2 = max(diagram_bottom, table_bottom) + 34
    b, h = para(60, y2,
                "A margin is the one knob here that 8.9 will actually turn, and it has two "
                "separate uses worth keeping apart: SPECULATION, because a pair a micron short of "
                "touching produces no manifold at all, and a solver that wants to stop a collision "
                "before it happens has to hear about it; and HYSTERESIS, because a proxy fattened "
                "by more than one frame's motion need not be reinserted at all. This grid rebuilds "
                "every frame and uses only the first. What it costs is the cube of the fattened "
                "sum rather than the ratio of the boxes' own volumes — measured within 12% of the "
                "prediction at every margin from a centimetre to half a metre — and it is "
                "conservative at every one of them: no margin ever lost a pair that a smaller one "
                "found.")
    body += b
    H = int(y2 + h + 16)
    return svg(uid, W, H, "The margin",
               "The Minkowski sum of two boxes and what fattening them costs in pairs.", body)


# ===========================================================================
# Figure 9 — the teapot in the stadium
# ===========================================================================

def fig9():
    uid = "f88i"
    W = 900
    body = [cmarker(uid, "red", RED), cmarker(uid, "grn", GREEN)]

    ax, ay, g = 50, 60, 17
    body.append(label(ax, ay - 18, "one proxy, alone in tens of thousands of cells", "sm",
                      "start"))
    nx, ny = 22, 10
    for i in range(nx + 1):
        body.append(rule(ax + i * g, ay, ax + i * g, ay + ny * g, cls="grid", width=0.5))
    for j in range(ny + 1):
        body.append(rule(ax, ay + j * g, ax + nx * g, ay + j * g, cls="grid", width=0.5))
    body.append(box(ax, ay + 6 * g, nx * g, 2 * g, colour=RED, opacity=0.2, rx=1, width=1.2))
    body.append(label(ax + nx * g / 2, ay + 7 * g + 5, "the ground plate", "xs"))
    for (gx, gy) in ((3, 2), (4, 2), (9, 4), (15, 1), (18, 3), (7, 5), (12, 2)):
        body.append(hollow(ax + gx * g + 3, ay + gy * g + 3, g - 6, g - 6, BLUE, width=1.1, rx=1))

    # the bar chart
    bx, by = 470, 60
    bw, bh = 380, 120
    body.append(label(bx, by - 18, "entries, 2000 crates at a 1 m cell", "sm", "start"))
    top = max(e for _n, e, _p in TEAPOT)
    for i, (name, ent, prs) in enumerate(TEAPOT):
        w = bw * ent / top
        yy = by + i * 44
        colour = RED if ent == top else GREEN
        # THE LABEL GOES ABOVE THE BAR, not inside it. Two of these three bars
        # are 27% of the width, so any label long enough to be useful crosses
        # the bar's right edge and reads as a bar that is longer than it is.
        body.append(label(bx, yy, f"{name} — {ent:,} entries, {prs:,} pairs", "xs", "start"))
        body.append(box(bx, yy + 6, w, 18, colour=colour, opacity=0.35, rx=2, width=1.0))

    ytab = max(ay + ny * g + 30, by + 3 * 44 + 20)
    rows = [(f"{c:g} m", f"{n:,}") for c, n in PLATE_CELLS]
    body += table_body(50, ytab + 18, ["cells the 200 × 0.2 × 200 m plate wants", "count"],
                       rows, [270, 110])
    rows2 = [("crates only", "30,225"), ("the plate alone, 1 m cell", "80,802")]
    body += table_body(520, ytab + 18, ["for comparison", "entries"], rows2, [250, 110])

    y2 = ytab + 18 + 20 + len(rows) * 17 + 28
    b, h = para(50, y2,
                "The plate is not exotic. It is the floor. At a 1 m cell it wants 80,802 cells "
                "for itself, nearly three times what the entire scene of two thousand crates "
                "wants, and it is alone in almost every one of them — so the grid pays to insert "
                "it and gains nothing. The cliff is cubic in the reciprocal of the cell size, "
                "which is why the table above quadruples and then quadruples again. A uniform "
                "grid has no structural answer to this: the answer is hierarchical, and this "
                "engine will not build one. What it does instead is NOTICE — past "
                "max_cells_per_proxy a proxy stops being gridded and is tested against "
                "everything, which keeps the contract exactly (the pair sets with the guard on "
                "and off are equal) and is linear in the number of such proxies times n. With "
                "one floor that is free. With sixty-four large objects it is most of the "
                "broadphase, and that is the grid telling you to use a tree.")
    body += b
    H = int(y2 + h + 16)
    return svg(uid, W, H, "The teapot in the stadium",
               "A ground plate claiming tens of thousands of cells it is alone in.", body)


# ===========================================================================
# Figure 10 — the budget, and the crossover
# ===========================================================================

def fig10():
    uid = "f88j"
    W = 900
    body = [cmarker(uid, "grn", GREEN), cmarker(uid, "red", RED)]

    # ---- the crossover plot ------------------------------------------------
    px, py, pw, ph = 66, 50, 380, 230
    body.append(frame(px, py, pw, ph))
    body.append(label(px + pw / 2, py - 14, "below about a hundred bodies the grid LOSES", "sm"))

    n_lo, n_hi = 8.0, 300.0
    t_lo, t_hi = 0.02, 60.0
    for n in (10, 30, 100, 300):
        x = logspan(n, n_lo, n_hi, px, pw)
        body.append(rule(x, py, x, py + ph, cls="grid", width=0.6))
        body.append(label(x, py + ph + 16, str(n), "xs muted"))
    for t in (0.1, 1, 10):
        y = logy(t, t_lo, t_hi, py, ph)
        body.append(rule(px, y, px + pw, y, cls="grid", width=0.6))
        body.append(label(px - 8, y + 4, f"{t:g}", "xs muted", "end"))
    body.append(label(px + pw / 2, py + ph + 34, "n, bodies", "xs muted"))
    body.append(label(px - 44, py + ph / 2, "µs", "xs muted"))

    bpts = [(logspan(n, n_lo, n_hi, px, pw), logy(max(b, t_lo), t_lo, t_hi, py, ph))
            for n, b, _g in CROSSOVER]
    gpts = [(logspan(n, n_lo, n_hi, px, pw), logy(g, t_lo, t_hi, py, ph))
            for n, _b, g in CROSSOVER]
    body.append(poly(bpts, RED, width=2.2, close=False))
    body.append(poly(gpts, GREEN, width=2.2, close=False))
    xc = logspan(100, n_lo, n_hi, px, pw)
    body.append(cline(xc, py, xc, py + ph, AMBER, width=1.5, dash="6 4"))
    body.append(label(xc + 6, py + 18, "~100", "xs", "start"))

    # OUTSIDE the frame: both curves cross the lower left of this plot.
    body += legend(px, py + ph + 52, [
        (RED, "brute force, n(n−1)/2 AABB tests"),
        (GREEN, "the grid"),
    ])

    # ---- the frame split ---------------------------------------------------
    sx, sy = 500, 74
    sw = 360
    total = BROAD_MS + NARROW_MS
    wb = sw * BROAD_MS / total
    body.append(label(sx, sy - 18, f"one frame, {SCENE_N:,} bodies, {CANDIDATES} candidates",
                      "sm", "start"))
    body.append(box(sx, sy, wb, 34, colour=GREEN, opacity=0.4, rx=2, width=1.0))
    body.append(box(sx + wb, sy, sw - wb, 34, colour=BLUE, opacity=0.3, rx=2, width=1.0))
    body.append(label(sx + wb / 2, sy + 22, f"{100 * BROAD_MS / total:.0f}%", "xs"))
    body.append(label(sx + wb + (sw - wb) / 2, sy + 22, f"{100 * NARROW_MS / total:.0f}%", "xs"))
    body.append(label(sx, sy + 52, f"broadphase {BROAD_MS:.3f} ms", "xs muted", "start"))
    body.append(label(sx + sw, sy + 52, f"narrow phase {NARROW_MS:.3f} ms", "xs muted", "end"))

    rows = [("the same narrow phase on ALL pairs", f"{IMPLIED_MS:.0f} ms"),
            ("frames that would fit in 16.67 ms", "0"),
            ("allocations over 2000 rebuilds", "0")]
    body += table_body(sx, sy + 86, ["§10", ""], rows, [270, 90])

    y2 = max(py + ph + 52 + 2 * 17 + 26, sy + 86 + 20 + 3 * 17 + 24)
    b, h = para(66, y2,
                "The crossover is the honest half of the lesson. An AABB overlap test is six "
                "comparisons on data already in registers; a grid build touches every proxy "
                "twice, chases a hash table and writes an entry array. Below roughly a hundred "
                "bodies the quadratic wins, and shipping a broadphase there is a pure loss — "
                "which is worth knowing because almost nobody measures it. Above it the two "
                "curves separate for ever: 17.4× at 512 bodies, 137.7× at 8,192. And having paid "
                "for it, the broadphase is 18% of the pair-finding work rather than the 99.95% "
                "the alternative spends on pairs that were never going to touch.")
    body += b
    H = int(y2 + h + 16)
    return svg(uid, W, H, "The budget and the crossover",
               "Grid against brute force at small scene sizes, and the frame split at 2000.",
               body)


if __name__ == "__main__":
    for i, fn in enumerate([fig1, fig2, fig3, fig4, fig5, fig6, fig7, fig8, fig9, fig10], 1):
        write(f"l88_fig{i}.svg", fn())
