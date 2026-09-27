#!/usr/bin/env python3
"""scratch/figs_86.py — Lesson 8.6's diagrams.

Same rules as 5.1-8.5's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - filenames numbered by PAGE ORDER
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - `rule()` takes a CSS CLASS; `cline()` takes a COLOUR
  - LEGENDS AND ANNOTATION BOXES GO OUTSIDE THE PLOT
  - a SHAPE can leave the viewBox where a label cannot
  - panel heights are COMPUTED, never guessed
  - the figure palette must contain the DEMO's own colours (rle_rects SNAPS)

Every number below is transcribed from scratch/verify_86.log. Nothing here is
estimated, and the harness section each block came from is named above it.

*** WRITTEN IN LITERAL UNICODE THROUGHOUT, NEVER \\uXXXX. ***

THE COLOUR RULE, inherited from 8.4 and 8.5 so a reader moving between the three
relearns nothing:
  GREEN  = apart / a proof of separation / the answer after the push
  RED    = overlapping
  AMBER  = the polytope, and the answer the algorithm returned
  BLUE   = shape A and geometry belonging to it
  PURPLE = the vector to the closest face — 8.5's `v`, pointing the other way
  GREY   = the difference set itself, or a candidate that found nothing

SEVERAL FIGURES RUN A REAL 2D EPA. `epa2` below is the same algorithm as
engine/src/phys/epa.cpp reduced to the plane, so the polytopes and bounds in
figures 3 and 6 are EXPANDED rather than drawn. If the C++ changes and this stops
agreeing, the figures are wrong and should be regenerated rather than patched.
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
from figs_71 import cmarker, carrow, poly, frame, D, View           # noqa: E402
from figs_76 import table, f, fmt_e                                 # noqa: E402
from figs_81 import legend, logspan                                 # noqa: E402

OUT = "scratch"

# demos/epa/main.cpp's constants, transcribed. rle_rects SNAPS every sampled cell
# to the nearest palette entry, so a colour the demo draws and this list omits
# comes out as whichever entry happens to be closest.
C_BG      = "#101218"
C_GRID    = "#262a34"
C_AXIS    = "#545c6c"
C_SHAPE_A = "#96a0ff"
C_SHAPE_B = "#eb6060"
C_GHOST   = "#5a966e"
C_MTV     = "#ebc860"
C_CLOUD   = "#464e60"
C_POLY    = "#bea046"
C_VLINE   = "#eb78c8"
C_ORIGIN  = "#e6ecf8"
EPA_PALETTE = [hexrgb(c) for c in (C_BG, C_GRID, C_AXIS, C_SHAPE_A, C_SHAPE_B,
                                   C_GHOST, C_MTV, C_CLOUD, C_POLY, C_VLINE,
                                   C_ORIGIN)]


def render_panel(ppm, crop, x, y, px, cell=4, levels=3, peak=True):
    """A real render, downsampled and run-length encoded. See figs_76."""
    _w, _h, data = read_ppm(os.path.join(OUT, ppm))
    grid_w, grid_h, grid = peak_sample(data, _w, crop, cell)
    body = rle_rects(grid, grid_w, grid_h, x, y, px, EPA_PALETTE, levels=levels,
                     bg_class="fill-shot")
    return (['<g shape-rendering="crispEdges">'] + body + ['</g>'],
            grid_w * px, grid_h * px)


# ===========================================================================
# A tiny 2D convex-geometry kit, so figures are COMPUTED
# ===========================================================================

def sub(a, b):
    return (a[0] - b[0], a[1] - b[1])


def add(a, b):
    return (a[0] + b[0], a[1] + b[1])


def mul(a, s):
    return (a[0] * s, a[1] * s)


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1]


def norm(a):
    m = math.hypot(a[0], a[1])
    return (a[0] / m, a[1] / m) if m > 0 else (0.0, 0.0)


def support_poly(pts, d):
    """The farthest point of a point set in direction `d`."""
    best = pts[0]
    bv = dot(best, d)
    for p in pts[1:]:
        v = dot(p, d)
        if v > bv:
            bv, best = v, p
    return best


def cso_pts(a, b):
    """Every pairwise difference. Only for drawing the set, never for search."""
    return [sub(p, q) for p in a for q in b]


def hull2(pts):
    """Andrew's monotone chain. Returns the hull in counter-clockwise order."""
    pts = sorted(set((round(p[0], 9), round(p[1], 9)) for p in pts))
    if len(pts) <= 2:
        return pts

    def half(seq):
        out = []
        for p in seq:
            while len(out) >= 2:
                o, q = out[-2], out[-1]
                if (q[0] - o[0]) * (p[1] - o[1]) - (q[1] - o[1]) * (p[0] - o[0]) <= 0:
                    out.pop()
                else:
                    break
            out.append(p)
        return out

    lower = half(pts)
    upper = half(reversed(pts))
    return lower[:-1] + upper[:-1]


def boundary_distance(hull_pts):
    """Distance from the origin to a convex polygon's boundary, and the point.

    The 2D shadow of §3's definition: the minimum over the polygon's edges of
    the distance to each edge segment.
    """
    best_d = 1e30
    best_p = (0.0, 0.0)
    n = len(hull_pts)
    for i in range(n):
        a = hull_pts[i]
        b = hull_pts[(i + 1) % n]
        ab = sub(b, a)
        denom = dot(ab, ab)
        t = 0.0 if denom == 0 else max(0.0, min(1.0, dot(mul(a, -1.0), ab) / denom))
        p = add(a, mul(ab, t))
        d = math.hypot(p[0], p[1])
        if d < best_d:
            best_d, best_p = d, p
    return best_d, best_p


def epa2(a, b, steps):
    """EPA in the plane: an expanding POLYGON, one vertex per step.

    Reduced from engine/src/phys/epa.cpp. The polytope is a polygon, its "faces"
    are edges, and the closest edge plays the part of the closest triangle. The
    sandwich is identical: the closest edge's plane distance is a lower bound and
    the support along its outward normal is an upper bound.

    Returns a list of (vertices, closest_index, lower, upper) per step.
    """
    def cso(d):
        return sub(support_poly(a, d), support_poly(b, mul(d, -1.0)))

    # Seed: the hull of the support points in four spread directions — the plane's
    # analogue of §6's argument that one probe out of the plane is not enough.
    # Three would give a triangle that need not contain the origin, and a lower
    # bound computed on a polytope the origin is outside of is not a lower bound.
    verts = hull2([cso((1.0, 0.0)), cso((0.0, 1.0)), cso((-1.0, 0.0)), cso((0.0, -1.0))])

    frames = []
    for _ in range(steps + 1):
        best_i, best_d, best_n = -1, 1e30, (0.0, 0.0)
        for i in range(len(verts)):
            p, q = verts[i], verts[(i + 1) % len(verts)]
            e = sub(q, p)
            n = norm((e[1], -e[0]))       # outward for CCW winding
            d = dot(n, p)
            if d < best_d:
                best_i, best_d, best_n = i, d, n
        w = cso(best_n)
        upper = dot(w, best_n)
        if best_d > 0.0:
            frames.append((list(verts), best_i, best_d, upper))
        if upper - best_d <= 1e-6 or any(math.hypot(*sub(w, v)) < 1e-9 for v in verts):
            break
        verts.insert(best_i + 1, w)
    return frames


def ngon(n, r, cx=0.0, cy=0.0, phase=0.0):
    """A regular n-gon. Used for the "rounded" operand in figures 2 and 3, which
    is also a foretaste of §10: the more a shape approximates a curve, the longer
    the expansion takes to reach its boundary."""
    return [(cx + r * math.cos(2 * math.pi * i / n + phase),
             cy + r * math.sin(2 * math.pi * i / n + phase)) for i in range(n)]


# THE ONE FIXTURE FIGURES 2 AND 3 SHARE. Figure 3's caption says "on figure 2's
# difference set", so it is defined once rather than typed twice.
FIG_A = [(-1.10, -0.70), (0.30, -1.00), (1.05, 0.05), (0.35, 0.95), (-0.95, 0.60)]
FIG_B = ngon(10, 0.85, 0.9, 0.05)


# ===========================================================================
# The measured numbers, transcribed from scratch/verify_86.log
# ===========================================================================

# --- A ---------------------------------------------------------------------
A_PAIRS = 200000
A_ZERO = 200000
A_GUESS_MEAN = 40.15
A_GUESS_P50 = 37.75
A_GUESS_P95 = 78.19
A_GUESS_MAX = 90.00
A_PUSH_MEAN = 1.71
A_PUSH_P95 = 2.4
A_PUSH_MAX = 2538
A_EPA_MEAN = 0.0089
A_EPA_P95 = 0.0222
A_EPA_P999 = 0.0283
A_EPA_ERR_MEAN = 2.574e-07
A_EPA_ERR_MAX = 3.107e-04
A_DEPTH = 0.400000

# --- B / C -----------------------------------------------------------------
B_PAIRS = 50000
C_PAIRS = 2000
C_SAMPLES = 4096
C_CALLS = C_PAIRS * C_SAMPLES
C_BEATEN = 0
C_SAMPLED_MEAN = 1.01864
C_SAMPLED_MIN = 1.00032
C_EXACT_MEAN = 1.000000
B_OTHER_MIN = 1.0354
B_OTHER_MEAN = 2.93

# --- D: the terminal simplex, by arrangement --------------------------------
D_ROWS = [
    ("crates on a floor",     50000,     0,   49, 49951,     0,     0),
    ("…one tilted 1°",        50000,     0,    1,   197, 49802, 49802),
    ("free orientations",     50000,     0,   29,   755, 49216, 49216),
    ("ball in a crate",       50000,     0,   63,  1242, 48695, 48695),
    ("capsule vs crate",      50000,     0,   65,  1032, 48903, 48903),
    ("hull vs hull",          50000,     0,    0,   204, 49796, 49796),
]

# --- E ---------------------------------------------------------------------
E_FLAT = 30000
E_COVERS = 30000
E_ONE_APEX_ZERO = 30000
E_TWO_MEAN = 9.044e-02
E_TWO_P95 = 2.234e-01
E_TRUE_MEAN = 4.908e-01
E_NON_CONVEX = 15855
E_NON_CONVEX_PCT = 52.85

# --- F ---------------------------------------------------------------------
F_QUERIES = 100000
F_MANIFOLD = 100000
F_MAX_V = 32
F_MAX_F = 60
F_SWALLOWED = 0
F_TEAR = [
    ("crates on a floor", 50000, 2086, 4.17, 2074, 4.15, 0.065310, 0.170662, 61.7),
    ("free orientations", 50000,    5, 0.01,    5, 0.01, 0.121198, 0.429087, 71.8),
    ("hull vs hull",      50000,    0, 0.00,    0, 0.00, 0.0,      0.0,       0.0),
]

# --- G ---------------------------------------------------------------------
G_TOLS = [1e-2, 1e-3, 1e-4, 1e-5, 1e-6]
G_ROWS = [
    ("box vs box",     [9.79, 10.08, 10.10, 10.11, 10.11]),
    ("hull vs hull",   [7.92,  8.34,  8.39,  8.39,  8.39]),
    ("capsule vs box", [8.76, 10.34, 11.72, 12.48, 13.16]),
    ("sphere vs box",  [10.43, 12.64, 13.91, 14.31, 15.17]),
    ("sphere/sphere",  [28.64, 40.56, 51.39, 60.00, 60.00]),
]
G_GJK_SPHERE = 1.00
G_CAPS = [(12, 51.55, 8.866e-03), (20, 11.12, 6.347e-04),
          (32, 0.86, 1.609e-05), (48, 0.01, 9.391e-06)]

# --- H ---------------------------------------------------------------------
H_ACC = [(1e-2, 1.119e-03, 2.347e-02, 1.355),
         (1e-3, 1.307e-05, 1.880e-03, 1.086),
         (1e-4, 1.835e-07, 2.167e-04, 1.251),
         (1e-5, 5.022e-08, 6.972e-06, 0.403),
         (1e-6, 4.958e-08, 5.855e-07, 0.338)]
H_ORIGIN = [(2, 1.49e-08, 0.0),
            (28, 1.49e-08, 1.67e-06),
            (1774, 1.49e-08, 9.78e-06),
            (28378, 1.49e-08, 1.72e-03),
            (227023, 1.49e-08, 1.00e-02),
            (1816187, 7.45e-09, 1.00e-02)]

# --- I ---------------------------------------------------------------------
I_SAT = 84.17
I_GJK = 77.92
I_BOTH = 1146.07
I_EPA = 1068.14
I_ALLOCS = 0
I_DEPTH = [(0.95, 6.26, 9.26, 626.7), (0.75, 7.28, 10.29, 740.1),
           (0.50, 8.48, 11.48, 860.9), (0.25, 11.71, 14.72, 1282.0),
           (0.05, 16.53, 19.57, 1886.4)]

# THE BEFORE/AFTER PAIR IS MEASURED BACK TO BACK, and it has to be: run-to-run
# spread on this machine is a few per cent, so a "before" taken an hour earlier
# would be quoting drift as a finding. These two runs are consecutive, with only
# a rebuild between them — the library was rebuilt with the default member
# initialisers restored, measured, rebuilt without them, measured. I_EPA above is
# the second of the pair, so the headline bar and the comparison are one run.
I_BEFORE = 1344.04
I_GAIN_PCT = 100.0 * (I_BEFORE - I_EPA) / I_BEFORE

# The third array, also back to back: a trivially-constructible mirror type for
# the CSO vertices against the shipped `gjk_vertex`. 0.1% apart.
I_MIRROR = 1054.35
I_KEPT = 1053.11


# ===========================================================================
# 1. The placeholder
# ===========================================================================

def fig1():
    uid = "l86f1"
    W, H = 900, 440
    b = [cmarker(uid, "amber", AMBER), cmarker(uid, "red", RED),
         cmarker(uid, "grey", GREY), cmarker(uid, "purple", PURPLE)]

    b.append(label(20, 22, "Two crates overlapping. GJK answers the question it "
                           "was asked, and stops.", "sm", "start"))

    # ---- left: the world, with the two unknowns --------------------------
    view = View(150, 190, 64.0)

    def cube(c, h):
        return [(c[0] + (h[0] if i & 1 else -h[0]),
                 c[1] + (h[1] if i & 2 else -h[1]),
                 c[2] + (h[2] if i & 4 else -h[2])) for i in range(8)]

    edges = [(0, 1), (2, 3), (4, 5), (6, 7), (0, 2), (1, 3), (4, 6), (5, 7),
             (0, 4), (1, 5), (2, 6), (3, 7)]

    for c, h, colour in (((0.0, 0.0, 0.0), (0.5, 0.5, 0.5), BLUE),
                         ((0.60, 0.15, 0.0), (0.5, 0.5, 0.5), RED)):
        pts = [view(p) for p in cube(c, h)]
        for i, j in edges:
            b.append(cline(pts[i][0], pts[i][1], pts[j][0], pts[j][1], colour, width=1.3))

    pa = view((0.5, 0.15, 0.0))
    pb = view((0.9, 0.15, 0.0))
    b.append(carrow(pa[0], pa[1], pb[0], pb[1], uid, "amber", AMBER, width=2.4))
    # The label goes on a leader OUT of the shape: a caption sitting on the arrow
    # it describes is the one thing check-page.js flags every single time.
    mid = ((pa[0] + pb[0]) / 2, (pa[1] + pb[1]) / 2)
    b.append(rule(mid[0], mid[1] - 6, mid[0] + 26, mid[1] - 34, "grid", dash="3 3"))
    b.append(label(mid[0] + 30, mid[1] - 34, f"{A_DEPTH:.3f} m", "xs t-hi", "start"))

    b.append(label(150, 306, "what a solver needs: HOW FAR, and WHICH WAY",
                   "xs muted", "middle"))

    rows = [("GJK status", "intersecting"),
            ("GJK distance", "0.000000 m"),
            ("GJK direction", "(0, 0, 0)"),
            ("8.5's separation.depth", "+0  (placeholder)")]
    tbl, th = table(20, 326, ["", ""], rows, [170, 150])
    b += tbl

    # ---- right: the guess, measured --------------------------------------
    px = 400
    b.append(label(px, 50, "8.5's doc comment called the axis “the last search "
                           "direction, a", "xs", "start"))
    b.append(label(px, 66, "reasonable guess at a contact normal”. It was not a "
                           "poor normal.", "xs", "start"))
    b.append(label(px, 82, "`gjk_result::direction` is only written on the "
                           "SEPARATED path:", "xs", "start"))

    rows0 = [("overlapping pairs measured", f"{A_PAIRS:,}"),
             ("of which the axis was exactly zero", f"{A_ZERO:,}  (100.0%)")]
    tbl0, th0 = table(px, 96, ["", ""], rows0, [260, 170])
    b += tbl0

    y2 = 96 + th0 + 34
    b.append(label(px, y2 - 10, "So the guess a naive implementation reaches for "
                                "instead — centre", "xs", "start"))
    b.append(label(px, y2 + 6, "to centre — against the true minimum-translation "
                               "normal:", "xs", "start"))
    rows1 = [("angle, mean", f"{A_GUESS_MEAN:.2f}°"),
             ("angle, p95 / max", f"{A_GUESS_P95:.1f}° / {A_GUESS_MAX:.0f}°"),
             ("push it implies, mean", f"{A_PUSH_MEAN:.2f}× the MTV"),
             ("push it implies, max", f"{A_PUSH_MAX:,}× the MTV")]
    tbl1, th1 = table(px, y2 + 16, ["", ""], rows1, [260, 170])
    b += tbl1

    y3 = y2 + 16 + th1 + 28
    b.append(label(px, y3, "CONTROL — this lesson's answer, against the same "
                           "exact reference:", "xs muted", "start"))
    rows2 = [("angle, mean / p99.9", f"{A_EPA_MEAN:.4f}° / {A_EPA_P999:.4f}°"),
             ("depth error, mean / max", f"{fmt_e(A_EPA_ERR_MEAN)} / "
                                         f"{fmt_e(A_EPA_ERR_MAX)} m")]
    tbl2, _ = table(px, y3 + 10, ["", ""], rows2, [260, 190])
    b += tbl2

    return svg(uid, W, H,
               "What GJK returns when two shapes overlap, and what a solver needs "
               "instead.",
               "Left, two wireframe crates overlapping, with an amber arrow marking "
               "the 0.400 m penetration depth GJK does not compute, and a table "
               "showing GJK reporting status intersecting, distance zero and a zero "
               "direction. Right, tables: the axis was exactly zero on all 200,000 "
               "overlapping pairs measured; centre-to-centre as a guessed normal is "
               "40.15 degrees off on average and implies a push 1.71 times the true "
               "minimum translation; EPA's normal is 0.0089 degrees off.",
               b)


# ===========================================================================
# 2. The minimum translation IS the difference set's boundary
# ===========================================================================

def fig2():
    uid = "l86f2"
    W, H = 900, 476
    b = [cmarker(uid, "amber", AMBER), cmarker(uid, "purple", PURPLE),
         cmarker(uid, "green", GREEN), cmarker(uid, "grey", GREY)]

    b.append(label(20, 22, "The translations that keep two shapes overlapping ARE "
                           "their Minkowski difference.", "sm", "start"))

    # Two overlapping convex polygons, in metres. See FIG_A / FIG_B.
    A = FIG_A
    B = FIG_B
    Cso = hull2(cso_pts(A, B))
    depth, near = boundary_distance(Cso)
    nrm = norm(near) if depth > 0 else (1.0, 0.0)
    Bpush = [add(p, near) for p in B]

    scale = 52.0

    def P1(p):
        return (120 + p[0] * scale, 200 - p[1] * scale)

    def P2(p):
        return (450 + p[0] * scale, 200 - p[1] * scale)

    def P3(p):
        return (760 + p[0] * scale, 200 - p[1] * scale)

    # ---- panel 1: the pair -----------------------------------------------
    b.append(label(120, 56, "1.  A and B overlap", "xs", "middle"))
    b.append(poly([P1(p) for p in A], BLUE, width=1.6))
    b.append(poly([P1(p) for p in B], RED, width=1.6))
    b.append(label(120, 322, "how far, and which way, to pull", "xs muted", "middle"))
    b.append(label(120, 336, "them apart?", "xs muted", "middle"))

    # ---- panel 2: the difference -----------------------------------------
    b.append(label(450, 56, "2.  A ⊖ B, with the origin inside", "xs", "middle"))
    b.append(poly([P2(p) for p in Cso], GREY, width=1.5))
    np_ = P2(near)
    o = P2((0.0, 0.0))
    b.append(carrow(o[0], o[1], np_[0], np_[1], uid, "purple", PURPLE, width=2.2))
    b.append(rule(o[0] - 9, o[1], o[0] + 9, o[1], "ink", width=2.0))
    b.append(rule(o[0], o[1] - 9, o[0], o[1] + 9, "ink", width=2.0))
    b.append(label(o[0] - 12, o[1] + 20, "the origin", "xs muted", "end"))
    b.append(label(np_[0] + 8, np_[1] - 6, f"{depth:.4f} m", "xs t-hi", "start"))
    b.append(label(450, 322, "every point of the set is a translation", "xs muted",
                  "middle"))
    b.append(label(450, 336, "that still overlaps, so the shortest one", "xs muted",
                  "middle"))
    b.append(label(450, 350, "that does NOT is the nearest point of", "xs muted",
                  "middle"))
    b.append(label(450, 364, "its BOUNDARY.", "xs t-hi", "middle"))

    # ---- panel 3: the push ------------------------------------------------
    b.append(label(760, 56, "3.  B moved by that vector", "xs", "middle"))
    b.append(poly([P3(p) for p in A], BLUE, width=1.6))
    b.append(poly([P3(p) for p in B], RED, width=1.0, dash="3 3"))
    b.append(poly([P3(p) for p in Bpush], GREEN, width=1.8))
    c0 = P3((sum(p[0] for p in B) / len(B), sum(p[1] for p in B) / len(B)))
    c1 = P3((sum(p[0] for p in Bpush) / len(B), sum(p[1] for p in Bpush) / len(B)))
    b.append(carrow(c0[0], c0[1], c1[0], c1[1], uid, "green", GREEN, width=2.0))
    b.append(label(760, 322, "exactly touching, and no shorter", "xs muted", "middle"))
    b.append(label(760, 336, "translation in any direction does it", "xs muted",
                  "middle"))

    # ---- the derivation, spelled out --------------------------------------
    b.append(rule(20, 390, W - 20, 390, "grid"))
    b.append(label(20, 414, "A and B + t overlap   ⟺   a = b + t for some a ∈ A, "
                            "b ∈ B   ⟺   t = a − b   ⟺   t ∈ A ⊖ B", "sm mono",
                   "start"))
    b.append(label(20, 440, "Two lines, and 8.5 §3 already proved the equivalence "
                            "they turn on. Everything else in this lesson is "
                            "arithmetic.", "xs muted", "start"))

    return svg(uid, W, H,
               "The minimum translation vector is the nearest boundary point of the "
               "Minkowski difference.",
               "Three panels. Left: a blue pentagon and a red ten-sided polygon "
               "overlapping. Middle: their Minkowski difference as one grey polygon "
               "with the origin marked inside it and a purple arrow to the nearest "
               "point of its boundary, measuring the penetration depth. Right: the "
               "ten-sided polygon translated by that arrow, now exactly touching the "
               "pentagon, drawn in green with its old position dashed. Below, the "
               "two-line derivation that the set of overlapping translations is the "
               "Minkowski difference itself.",
               b)


# ===========================================================================
# 3. The sandwich, reversed
# ===========================================================================

def fig3():
    uid = "l86f3"
    W, H = 900, 440
    b = [cmarker(uid, "amber", AMBER), cmarker(uid, "purple", PURPLE),
         cmarker(uid, "green", GREEN), cmarker(uid, "grey", GREY)]

    b.append(label(20, 22, "Three snapshots of one expansion, on figure 2's difference "
                           "set, with nothing drawn by hand.", "sm", "start"))

    A = FIG_A
    B = FIG_B
    Cso = hull2(cso_pts(A, B))
    truth, _ = boundary_distance(Cso)
    frames = epa2(A, B, 8)
    # Three of the frames, spread across the run rather than the first three:
    # the interesting part is the bracket closing, and it closes at the end.
    picks = [0, len(frames) // 2, len(frames) - 1]

    scale = 44.0
    tops = [(140, 180), (450, 180), (760, 180)]
    for k, (cx, cy) in enumerate(tops):
        step = picks[k]
        verts, ci, lower, upper = frames[step]

        def P(p, cx=cx, cy=cy):
            return (cx + p[0] * scale, cy - p[1] * scale)

        b.append(label(cx, 54, f"after {step} expansion{'' if step == 1 else 's'}",
                       "xs", "middle"))
        b.append(poly([P(p) for p in Cso], GREY, width=1.3))
        b.append(poly([P(p) for p in verts], AMBER, width=1.7))

        o = P((0.0, 0.0))

        # the closest edge, and the vector to it
        p = verts[ci]
        q = verts[(ci + 1) % len(verts)]
        e = sub(q, p)
        n = norm((e[1], -e[0]))
        foot = mul(n, lower)
        fp = P(foot)
        b.append(cline(P(p)[0], P(p)[1], P(q)[0], P(q)[1], GREEN, width=3.0))
        b.append(carrow(o[0], o[1], fp[0], fp[1], uid, "purple", PURPLE, width=2.0))
        b.append(rule(o[0] - 8, o[1], o[0] + 8, o[1], "ink", width=1.9))
        b.append(rule(o[0], o[1] - 8, o[0], o[1] + 8, "ink", width=1.9))

        # the supporting line through w, which is the upper bound
        w = mul(n, upper)
        t = (-n[1], n[0])
        s0 = P(add(w, mul(t, 2.4)))
        s1 = P(sub(w, mul(t, 2.4)))
        b.append(cline(s0[0], s0[1], s1[0], s1[1], GREY, width=1.2, dash="4 3"))

        rows = [("lower", f"{lower:.4f}"), ("upper", f"{upper:.4f}")]
        tbl, _ = table(cx - 78, 288, ["", "m"], rows, [82, 74])
        b += tbl

    b.append(rule(20, 356, W - 20, 356, "grid"))
    b += legend(20, 380, [
        (AMBER, "the inner polytope: every vertex is a real point of A ⊖ B"),
        (GREEN, "its closest edge — a LOWER bound, because the polytope fits inside"),
        (GREY, "the supporting line through the support point — an UPPER bound"),
    ])
    b.append(label(560, 380, f"They meet at {truth:.6f} m, which is the number "
                             "figure 2", "xs", "start"))
    b.append(label(560, 397, "measured by brute force. Note the direction of "
                             "travel:", "xs", "start"))
    b.append(label(560, 414, "GJK's bound only ever FELL. This one only ever "
                             "RISES.", "xs t-hi", "start"))

    return svg(uid, W, H,
               "EPA's two bounds squeezing the penetration depth from both sides, "
               "the mirror image of GJK's.",
               "Three snapshots of the same Minkowski difference in grey with an "
               "amber polygon growing inside it over successive expansions. In each, "
               "the closest edge is green, a purple arrow runs from the origin to it, "
               "and a dashed grey supporting line marks the upper bound. The lower "
               "bound rises and the upper bound falls until they meet.",
               b)


# ===========================================================================
# 4. What GJK actually hands over
# ===========================================================================

def fig4():
    uid = "l86f4"
    W, H = 900, 450
    b = [cmarker(uid, "amber", AMBER), cmarker(uid, "red", RED)]

    b.append(label(20, 22, "Every description of EPA opens “start from the "
                           "tetrahedron GJK left”. Here is what GJK leaves.",
                   "sm", "start"))

    # ---- the stacked bars --------------------------------------------------
    x0, w_bar = 236, 420
    top = 76
    row_h = 40
    colours = {1: GREY, 2: PURPLE, 3: RED, 4: GREEN}
    # A KEY, NOT AN AXIS. An earlier version put these at fixed x offsets as if
    # they labelled the stack positions, which they cannot: the segment widths
    # are data and move from row to row.
    b.append(label(x0, top - 14, "terminal simplex:", "xs muted", "start"))
    for i, (count, colour) in enumerate(((1, colours[1]), (2, colours[2]),
                                         (3, colours[3]), (4, colours[4]))):
        kx = x0 + 88 + i * 132
        b.append(cline(kx, top - 18, kx + 16, top - 18, colour, width=4.0))
        text = {1: "1 point", 2: "2 points", 3: "3 — a flat triangle",
                4: "4, with volume"}[count]
        b.append(label(kx + 22, top - 14, text, "xs muted", "start"))

    for i, (name, total, c1, c2, c3, c4, tetra) in enumerate(D_ROWS):
        y = top + i * row_h
        b.append(label(x0 - 10, y + 13, name, "xs", "end"))
        cursor = x0
        for count, colour in ((c1, colours[1]), (c2, colours[2]),
                              (c3, colours[3]), (c4, colours[4])):
            wpx = w_bar * count / total
            if wpx > 0.4:
                b.append(box(cursor, y, wpx, 18, colour, opacity=0.85, width=0.8, rx=1))
            cursor += wpx
        pct = 100.0 * (total - tetra) / total
        b.append(label(x0 + w_bar + 12, y + 13,
                       f"{pct:.2f}% refused by a textbook EPA", "xs mono", "start"))

    # ---- the finding -------------------------------------------------------
    y = top + len(D_ROWS) * row_h + 18
    b.append(rule(20, y, W - 20, y, "grid"))
    b.append(label(20, y + 22, "Two crates standing on the same floor produce a "
                               "4-point simplex ZERO times in 50,000 — and a flat "
                               "TRIANGLE 49,951 times.", "sm t-hi", "start"))
    b.append(label(20, y + 42, "Not a flat tetrahedron, which is what the "
                               "literature warns about: `reduce_simplex` discards "
                               "any vertex the closest point does not use, and", "xs",
                   "start"))
    b.append(label(20, y + 58, "three points in a plane already span it. Tilt one "
                               "crate by a single degree and 49,802 tetrahedra "
                               "appear.", "xs", "start"))
    b.append(label(20, y + 82, "This is 8.5 §9's finding from the other side: a "
                               "shared up axis puts y = 0 on every difference "
                               "vertex the search visits, so the simplex is",
                   "xs muted", "start"))
    b.append(label(20, y + 98, "trapped in a plane. The SET is three-dimensional; "
                               "the SEARCH never leaves its equator. Building the "
                               "starting polytope is therefore on the critical",
                   "xs muted", "start"))
    b.append(label(20, y + 114, "path, not the error path.", "xs muted", "start"))

    return svg(uid, W, H,
               "The terminal simplex GJK hands to EPA, by arrangement, over 50,000 "
               "overlapping pairs each.",
               "Six stacked bars. Crates standing on the same floor are almost "
               "entirely flat triangles with no four-point simplex at all, so a "
               "textbook EPA refuses 100 percent of them. Tilting one crate by one "
               "degree drops the refusal rate to 0.40 percent. Free orientations, a "
               "ball in a crate, a capsule against a crate and two hulls are all "
               "between 0.41 and 2.61 percent.",
               b)


# ===========================================================================
# 5. One apex is not enough, and six faces are not a polytope
# ===========================================================================

def fig5():
    uid = "l86f5"
    W, H = 900, 420
    b = [cmarker(uid, "amber", AMBER), cmarker(uid, "purple", PURPLE),
         cmarker(uid, "red", RED), cmarker(uid, "green", GREEN)]

    b.append(label(20, 22, "The simplex arrives flat, so the seed has to leave the "
                           "plane — and one way out is not enough.", "sm", "start"))

    view = View(190, 210, 78.0, az=26.0, el=20.0)

    tri = [(-0.9, 0.0, -0.5), (0.95, 0.0, -0.35), (0.05, 0.0, 0.95)]
    up = (0.15, 1.05, 0.05)
    down = (-0.1, -0.95, 0.1)
    origin = (0.02, 0.0, 0.04)

    def draw_tri(v, colour, width=1.5):
        pts = [view(p) for p in v]
        for i in range(len(v)):
            j = (i + 1) % len(v)
            b.append(cline(pts[i][0], pts[i][1], pts[j][0], pts[j][1], colour,
                           width=width))

    # ---- left: one apex ---------------------------------------------------
    b.append(label(190, 56, "one apex: a tetrahedron", "xs", "middle"))
    draw_tri(tri, AMBER, 1.8)
    for t in tri:
        p0, p1 = view(t), view(up)
        b.append(cline(p0[0], p0[1], p1[0], p1[1], AMBER, width=1.4))
    o = view(origin)
    b.append(rule(o[0] - 7, o[1], o[0] + 7, o[1], "ink", width=2.0))
    b.append(rule(o[0], o[1] - 7, o[0], o[1] + 7, "ink", width=2.0))
    b.append(rule(o[0] + 6, o[1] + 4, o[0] + 30, o[1] + 34, "grid", dash="3 3"))
    b.append(label(o[0] + 34, o[1] + 38, "the origin is ON the base", "xs t-hi",
                   "start"))
    b.append(label(190, 330, "so the closest face is at distance ZERO,", "xs muted",
                  "middle"))
    b.append(label(190, 344, "and the lower bound starts — and stays — at 0.",
                   "xs muted", "middle"))

    # ---- middle: two apexes ------------------------------------------------
    view2 = View(490, 210, 78.0, az=26.0, el=20.0)
    b.append(label(490, 56, "two apexes: the origin is enclosed", "xs", "middle"))
    pts_t = [view2(p) for p in tri]
    for i in range(3):
        j = (i + 1) % 3
        b.append(cline(pts_t[i][0], pts_t[i][1], pts_t[j][0], pts_t[j][1], AMBER,
                       width=1.8))
    for apex, colour in ((up, GREEN), (down, GREEN)):
        pa = view2(apex)
        for t in tri:
            p0 = view2(t)
            b.append(cline(p0[0], p0[1], pa[0], pa[1], colour, width=1.3))
    o2 = view2(origin)
    b.append(rule(o2[0] - 7, o2[1], o2[0] + 7, o2[1], "ink", width=2.0))
    b.append(rule(o2[0], o2[1] - 7, o2[0], o2[1] + 7, "ink", width=2.0))
    b.append(label(490, 330, "one support call along each side of the plane,",
                   "xs muted", "middle"))
    b.append(label(490, 344, "and now every face has the origin behind it.",
                   "xs muted", "middle"))

    # ---- right: the measurements ------------------------------------------
    px = 640
    b.append(label(px, 66, f"Over {E_FLAT:,} flat simplices, crates on a floor:",
                   "xs", "start"))
    rows = [("triangle covers the origin", f"{E_COVERS:,} / {E_FLAT:,}"),
            ("one apex: bound below 1e−6", f"{E_ONE_APEX_ZERO:,} / {E_FLAT:,}"),
            ("two apexes: mean bound", f"{E_TWO_MEAN:.4f} m"),
            ("the true depth, mean", f"{E_TRUE_MEAN:.4f} m")]
    tbl, th = table(px, 80, ["", ""], rows, [148, 112])
    b += tbl

    y = 80 + th + 26
    b.append(label(px, y, "AND THE SIX FACES MUST NOT BE", "xs t-hi", "start"))
    b.append(label(px, y + 15, "STITCHED BY HAND.", "xs t-hi", "start"))
    b.append(label(px, y + 34, "A bipyramid is convex only when", "xs", "start"))
    b.append(label(px, y + 49, "each apex projects INSIDE the", "xs", "start"))
    b.append(label(px, y + 64, "triangle, and a support point", "xs", "start"))
    b.append(label(px, y + 79, "has no reason to:", "xs", "start"))
    rows2 = [("not convex", f"{E_NON_CONVEX:,}  ({E_NON_CONVEX_PCT:.2f}%)")]
    tbl2, _ = table(px, y + 90, ["", ""], rows2, [110, 140])
    b += tbl2

    return svg(uid, W, H,
               "Why the flat case needs two support points rather than one, and why "
               "the resulting solid must be built by the same expansion the loop uses.",
               "Left, a triangle with a single apex above it forming a tetrahedron, "
               "with the origin marked lying exactly on the triangle: the closest "
               "face is at distance zero. Middle, the same triangle with apexes above "
               "and below, enclosing the origin. Right, the measurements: on 30,000 "
               "flat simplices the one-apex seed gives a lower bound below 1e-6 every "
               "single time, and 52.85 percent of hand-stitched bipyramids are not "
               "convex at all.",
               b)


# ===========================================================================
# 6. The horizon
# ===========================================================================

def fig6():
    uid = "l86f6"
    W, H = 900, 400
    b = [cmarker(uid, "amber", AMBER), cmarker(uid, "red", RED),
         cmarker(uid, "green", GREEN), cmarker(uid, "grey", GREY)]

    b.append(label(20, 22, "Adding a vertex: delete what can see it, then stitch "
                           "the rim.", "sm", "start"))

    # A 2D analogue, because a horizon in the plane is two points and is legible.
    cx, cy, R = 200, 210, 96.0
    n = 9
    ring = [(cx + R * math.cos(2 * math.pi * i / n - math.pi / 2),
             cy + R * math.sin(2 * math.pi * i / n - math.pi / 2)) for i in range(n)]
    wpt = (cx + 1.85 * R * math.cos(-0.5), cy + 1.85 * R * math.sin(-0.5))

    # Which edges can "see" w: an edge is visible when w is beyond its outward line.
    visible = []
    for i in range(n):
        p, q = ring[i], ring[(i + 1) % n]
        e = sub(q, p)
        nn = norm((e[1], -e[0]))
        nn = nn if dot(nn, sub(p, (cx, cy))) > 0 else mul(nn, -1.0)
        if dot(nn, sub(wpt, p)) > 0:
            visible.append(i)

    b.append(label(cx, 56, "visible faces and the rim", "xs", "middle"))
    for i in range(n):
        p, q = ring[i], ring[(i + 1) % n]
        colour = RED if i in visible else AMBER
        b.append(cline(p[0], p[1], q[0], q[1], colour, width=2.4 if i in visible else 1.6))
    for i in visible:
        for pt in (ring[i], ring[(i + 1) % n]):
            b.append(cline(pt[0], pt[1], wpt[0], wpt[1], GREY, width=0.9, dash="3 3"))
    b.append(f'<circle cx="{wpt[0]:.1f}" cy="{wpt[1]:.1f}" r="4.2" fill="{GREEN}"/>')
    b.append(label(wpt[0] + 10, wpt[1] + 4, "w", "sm mono t-hi", "start"))

    rim = [visible[0], visible[-1] + 1]
    for k in rim:
        pt = ring[k % n]
        b.append(f'<circle cx="{pt[0]:.1f}" cy="{pt[1]:.1f}" r="4.6" fill="none" '
                 f'stroke="{GREEN}" stroke-width="2"/>')
    b.append(label(cx, 340, "the rim is where the visible run ENDS", "xs muted",
                  "middle"))

    # ---- the identity ------------------------------------------------------
    px = 380
    b.append(label(px, 62, "THE RIM IS FOUND BY CANCELLATION, with no adjacency "
                           "stored.", "xs t-hi", "start"))
    b.append(label(px, 82, "Every edge INTERIOR to the deleted cap belongs to two "
                           "deleted faces", "xs", "start"))
    b.append(label(px, 98, "and appears twice, once in each direction. Every edge "
                           "on the rim", "xs", "start"))
    b.append(label(px, 114, "appears once. So push each deleted face's three "
                            "edges onto a list", "xs", "start"))
    b.append(label(px, 130, "and cancel any pair that are reverses. What survives "
                            "is the horizon,", "xs", "start"))
    b.append(label(px, 146, "already in winding order.", "xs", "start"))

    b.append(label(px, 180, "AND “THE VISIBLE SET IS CONNECTED” IS A THEOREM — IN "
                            "EXACT", "xs t-hi", "start"))
    b.append(label(px, 196, "ARITHMETIC.", "xs t-hi", "start"))
    b.append(label(px, 216, "A support point that lands EXACTLY on several face "
                            "planes leaves", "xs", "start"))
    b.append(label(px, 232, "`dot(n, w)` and `d` as the same number computed two "
                            "different ways,", "xs", "start"))
    b.append(label(px, 248, "and the last few bits decide which side of the "
                            "comparison each", "xs", "start"))
    b.append(label(px, 264, "face falls on. Measured on two cubes face to face "
                            "with 0.1 mm of", "xs", "start"))
    b.append(label(px, 280, "overlap: three faces called visible, scattered, "
                            "sharing no edge —", "xs", "start"))
    b.append(label(px, 296, "nine horizon edges with nothing to cancel, and a "
                            "surface that is", "xs", "start"))
    b.append(label(px, 312, "no longer a polytope.", "xs", "start"))

    b.append(label(px, 344, "So the candidates are FLOOD FILLED from the closest "
                            "face, which is", "xs t-hi", "start"))
    b.append(label(px, 360, "the one face that is certainly visible. In exact "
                            "arithmetic that", "xs t-hi", "start"))
    b.append(label(px, 376, "changes nothing; in float it undoes the scatter.",
                   "xs t-hi", "start"))

    return svg(uid, W, H,
               "How the horizon of the deleted cap is found, and why the visible set "
               "is flood filled first.",
               "Left, a nine-sided polygon standing in for the polytope, with a new "
               "point w outside it. The four edges that can see w are drawn red and "
               "the two rim vertices are circled green. Right, the edge-cancellation "
               "identity written out, and the measured failure it does not survive "
               "on its own: a support point exactly coplanar with four faces "
               "scatters the visible set, and the horizon comes back in three pieces.",
               b)


# ===========================================================================
# 7. What the tear costs
# ===========================================================================

def fig7():
    uid = "l86f7"
    W, H = 900, 380
    b = [cmarker(uid, "red", RED), cmarker(uid, "green", GREEN)]

    b.append(label(20, 22, "A random test suite would never find this.", "sm",
                   "start"))

    x0, w_bar = 250, 330
    top = 66
    row_h = 74
    peak = 5.0
    for i, (name, n, torn, torn_pct, wrong, wrong_pct, bad, good, worst) in \
            enumerate(F_TEAR):
        y = top + i * row_h
        b.append(label(x0 - 12, y + 12, name, "xs", "end"))
        b.append(label(x0 - 12, y + 28, f"{n:,} overlapping pairs", "xs muted", "end"))
        wpx = w_bar * min(torn_pct, peak) / peak
        b.append(box(x0, y, max(wpx, 1.0), 20, RED, opacity=0.85, width=0.8, rx=2))
        b.append(label(x0 + max(wpx, 1.0) + 10, y + 15,
                       f"{torn:,} torn   ({torn_pct:.2f}%)", "xs mono", "start"))
        if worst > 0:
            b.append(label(x0, y + 40, f"worst: {bad:.6f} m reported against "
                                       f"{good:.6f} — {worst:.1f}% low",
                           "xs muted", "start"))
        else:
            b.append(label(x0, y + 40, "never once, on 50,000 pairs", "xs muted",
                           "start"))
    b.append(label(x0 + w_bar / 2, top - 14, "share of queries whose horizon tore, "
                                             "without the flood fill", "xs muted",
                   "middle"))

    y = top + len(F_TEAR) * row_h + 4
    b.append(rule(20, y, W - 20, y, "grid"))
    b.append(label(20, y + 24, "Crates standing on a floor: 4.17%. The same crates "
                               "at free orientations: 0.01% — four hundred times "
                               "rarer. Two point-cloud", "sm", "start"))
    b.append(label(20, y + 44, "hulls, which have no shared axes and no coplanar "
                               "structure at all: never.", "sm", "start"))
    b.append(label(20, y + 70, "This is the third lesson running in which the "
                               "failure needs the structure a real scene is made "
                               "of — axis alignment, shared up axes, faces resting",
                   "xs muted", "start"))
    b.append(label(20, y + 86, "on faces — and is invisible to uniform random "
                               "input. 8.4 §F said it about box orientations, 8.5 "
                               "§C said it about sliver triangles.", "xs muted",
                   "start"))

    return svg(uid, W, H,
               "How often the textbook visibility test tears the horizon, by "
               "arrangement.",
               "Three horizontal bars. Crates standing on the same floor tear 2,086 "
               "times in 50,000, or 4.17 percent, with a worst case reporting 0.065 m "
               "against a true 0.171 m, 61.7 percent low. The same crates at free "
               "orientations tear 5 times, 0.01 percent. Two convex hulls built from "
               "random points never tear at all.",
               b)


# ===========================================================================
# 8. The demo
# ===========================================================================

def fig8():
    uid = "l86f8"
    W = 900
    b = []

    b.append(label(20, 22, "The polytope growing until it presses against a wall "
                           "you cannot see.", "sm", "start"))

    # Two real frames, cropped to the content rather than the window: a 960×540
    # frame downsampled to fit half this figure is 320 px wide and the pink
    # vector is two of them. The crop drops the empty margins and buys back the
    # resolution where it matters.
    crop = (96, 96, 800, 344)
    left, lw, lh = render_panel("l86_shot_step.ppm", crop, 18, 44, 1,
                                cell=2, levels=3)
    b += left
    right, rw, rh = render_panel("l86_shot_done.ppm", crop, 18 + lw + 20, 44, 1,
                                 cell=2, levels=3)
    b += right

    b.append(label(18 + lw / 2, 44 + lh + 18, "two expansions in", "xs", "middle"))
    b.append(label(18 + lw + 20 + rw / 2, 44 + lh + 18, "converged", "xs", "middle"))

    y = 44 + lh + 44
    b += legend(20, y, [
        (C_SHAPE_A, "shape A"),
        (C_SHAPE_B, "shape B"),
        (C_GHOST, "B moved by the current answer — still overlapping on the left"),
    ])
    b += legend(470, y, [
        (C_CLOUD, "the difference set's silhouette"),
        (C_POLY, "the expanding polytope, gold"),
        (C_VLINE, "the vector to its closest face: the lower bound"),
    ])

    b.append(label(20, y + 74, "Preset 3, two crates standing on the same floor. "
                               "The left panel draws a GHOST of the second crate "
                               "where the current answer would put it, which",
                   "xs muted", "start"))
    b.append(label(20, y + 90, "is what an unconverged lower bound looks like: a "
                               "shape that has not quite come free. Press [S] and "
                               "the pink vector LENGTHENS — the opposite of",
                   "xs muted", "start"))
    b.append(label(20, y + 106, "GJK's `v`, which only ever shrank. Press [F] to "
                                "drop the flood fill and watch the depth go "
                                "wrong.", "xs muted", "start"))

    # COMPUTED, not guessed: the last baseline plus a descender and a margin.
    H = int(y + 106 + 16)

    return svg(uid, W, H,
               "Two frames from demos/epa: the polytope after two expansions and "
               "after it has converged.",
               "Left frame: two wireframe crates overlapping, with a green ghost "
               "still inside the blue crate, and on the right panel a small gold "
               "bipyramid inside the difference set's grey outline with a short pink "
               "vector from the origin. Right frame, the same pair converged: the "
               "ghost is clear of the blue crate and the gold polytope now fills most "
               "of the difference set.",
               b)


# ===========================================================================
# 9. Polytopes terminate, curves do not
# ===========================================================================

def fig9():
    uid = "l86f9"
    W, H = 900, 450
    b = [cmarker(uid, "amber", AMBER), cmarker(uid, "green", GREEN)]

    b.append(label(20, 22, "Mean expansions as the tolerance tightens — and the "
                           "odd one out has swapped places.", "sm", "start"))

    px0, pw = 150, 420
    top, ph = 76, 210
    b.append(frame(px0, top, pw, ph))
    lo, hi = 6.0, 62.0

    def ypos(v):
        return top + ph - (v - lo) / (hi - lo) * ph

    for v in (10, 20, 30, 40, 50, 60):
        yy = ypos(v)
        b.append(rule(px0, yy, px0 + pw, yy, "grid"))
        b.append(label(px0 - 8, yy + 4, str(v), "xs muted", "end"))
    b.append(label(px0 - 8, top - 8, "expansions", "xs muted", "end"))

    for i, tol in enumerate(G_TOLS):
        xx = px0 + pw * i / (len(G_TOLS) - 1)
        b.append(rule(xx, top, xx, top + ph, "grid", dash="2 4"))
        b.append(label(xx, top + ph + 16, f"1e−{i + 2}", "xs muted", "middle"))

    colours = [BLUE, GREY, AMBER, PURPLE, RED]
    ends = []
    for r, (name, vals) in enumerate(G_ROWS):
        pts = [(px0 + pw * i / (len(G_TOLS) - 1), ypos(v)) for i, v in enumerate(vals)]
        for i in range(len(pts) - 1):
            b.append(cline(pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1],
                           colours[r], width=2.0))
        for p in pts:
            b.append(f'<circle cx="{p[0]:.1f}" cy="{p[1]:.1f}" r="2.6" '
                     f'fill="{colours[r]}"/>')
        ends.append((pts[-1][1], name, colours[r]))

    # FOUR OF THE FIVE SERIES END WITHIN TWENTY PIXELS OF EACH OTHER, so their
    # labels are pushed apart to a minimum spacing and joined to their lines by
    # leaders. Computed, not placed: sort by the end position, then walk down
    # enforcing the gap, so the labels keep the series' own vertical order.
    ends.sort()
    min_gap = 16.0
    slots = []
    for y_end, name, colour in ends:
        y_lab = y_end if not slots else max(y_end, slots[-1] + min_gap)
        slots.append(y_lab)
    for (y_end, name, colour), y_lab in zip(ends, slots):
        if abs(y_lab - y_end) > 1.0:
            b.append(cline(px0 + pw + 2, y_end, px0 + pw + 12, y_lab - 4, colour,
                           width=0.9, dash="2 2"))
        b.append(label(px0 + pw + 16, y_lab, name, "xs", "start"))

    y = top + ph + 44
    b.append(rule(20, y, W - 20, y, "grid"))
    b.append(label(20, y + 24, f"8.5 ran this table for GJK and found the SPHERE "
                               f"flat at ONE iteration. Here it is the worst row — "
                               f"{G_ROWS[4][1][0]:.1f} rising to the cap.",
                   "sm t-hi", "start"))
    b.append(label(20, y + 46, "Same shape, same support function, opposite "
                               "behaviour. GJK walks TO a ball's surface, and the "
                               "nearest point of a ball to an outside point is on",
                   "xs", "start"))
    b.append(label(20, y + 62, "the line to its centre, so the first support call "
                               "lands on the answer. EPA has to COVER that surface "
                               "with flat triangles, and no finite number of",
                   "xs", "start"))
    b.append(label(20, y + 78, "them is ever exact. A box and a hull are "
                               "polytopes: their vertex sets are finite, so the "
                               "expansion finishes and the tolerance is never what "
                               "stopped it.", "xs", "start"))
    b.append(label(20, y + 100, "Which is why 8.4 ships `collide(sphere, sphere)` "
                                "as one subtraction, and why an engine keeps it.",
                   "xs muted", "start"))

    return svg(uid, W, H,
               "Mean EPA expansions against tolerance for five shape pairs.",
               "A line chart with tolerance from 1e-2 to 1e-6 on the horizontal axis. "
               "Box against box is flat at about 10 expansions and hull against hull "
               "flat at about 8: both are polytopes and terminate exactly. Capsule "
               "and sphere against a box climb gently. Sphere against sphere climbs "
               "steeply from 28.6 to the 60-expansion cap, because a ball has no "
               "faces to reach.",
               b)


# ===========================================================================
# 10. Accuracy, and where the world origin bites
# ===========================================================================

def fig10():
    uid = "l86f10"
    W = 900
    b = [cmarker(uid, "green", GREEN), cmarker(uid, "red", RED)]

    b.append(label(20, 22, "How close, and what the arithmetic costs a kilometre "
                           "from the origin.", "sm", "start"))

    # ---- left: error against tolerance ------------------------------------
    b.append(label(240, 54, "|EPA − exact| for 1 m crates", "xs", "middle"))
    rows = [(f"1e−{i + 2}", fmt_e(mean), fmt_e(mx), f"{ratio:.2f}")
            for i, (tol, mean, mx, ratio) in enumerate(H_ACC)]
    tbl, th = table(60, 68, ["tolerance", "mean err", "max err", "÷ tol·size"],
                    rows, [100, 110, 110, 100])
    b += tbl
    b.append(label(60, 68 + th + 26, "The max error tracks `tolerance × size` to "
                                     "within a factor", "xs muted", "start"))
    b.append(label(60, 68 + th + 42, "of 1.4, which is what makes `tolerance` a "
                                     "relative quantity", "xs muted", "start"))
    b.append(label(60, 68 + th + 58, "rather than a distance — 8.5 §11's finding, "
                                     "inherited.", "xs muted", "start"))

    # ---- right: the world origin -------------------------------------------
    px = 500
    b.append(label(px + 180, 54, "the same overlap, walked away from the origin",
                   "xs", "middle"))
    rows2 = [(f"{d:,} m", fmt_e(rel), fmt_e(nai) if nai > 0 else "0")
             for d, rel, nai in H_ORIGIN]
    tbl2, th2 = table(px, 68, ["distance", "relative", "naive"], rows2,
                      [130, 120, 120])
    b += tbl2
    b.append(label(px, 68 + th2 + 26, "Two arms, and they are NOT two "
                                      "implementations: the same", "xs muted",
                   "start"))
    b.append(label(px, 68 + th2 + 42, "`epa.cpp` in both columns, differing only in "
                                      "the `convex` view", "xs muted", "start"))
    b.append(label(px, 68 + th2 + 58, "they are handed. The relative form's error "
                                      "does not move;", "xs muted", "start"))
    b.append(label(px, 68 + th2 + 74, "the naive form reaches a CENTIMETRE.",
                   "xs t-hi", "start"))

    # COMPUTED, not guessed: the lower of the two columns' last baselines.
    H = int(max(68 + th + 58, 68 + th2 + 74) + 18)

    return svg(uid, W, H,
               "EPA's accuracy against its tolerance, and the effect of distance from "
               "the world origin on the two support formulations.",
               "Left, a table of mean and maximum depth error for tolerances from "
               "1e-2 to 1e-6, with the maximum error divided by tolerance times size "
               "staying between 0.34 and 1.36. Right, a table of the same overlap "
               "measured from 2 m to 1.8 million metres from the world origin: the "
               "relative support formulation holds at about 1.5e-8 m throughout while "
               "the naive world-space one degrades to 1e-2 m.",
               b)


# ===========================================================================
# 11. The budget
# ===========================================================================

def fig11():
    uid = "l86f11"
    W, H = 900, 400
    b = [cmarker(uid, "amber", AMBER), cmarker(uid, "green", GREEN)]

    b.append(label(20, 22, "EPA is fourteen times GJK, and that is the shape of the "
                           "answer rather than a defect.", "sm", "start"))

    # ---- left: the three timings ------------------------------------------
    x0, w_bar = 190, 190
    top = 70
    peak = I_BOTH
    rows = [("SAT  collide(obb, obb)", I_SAT, GREY),
            ("GJK  gjk_distance", I_GJK, BLUE),
            ("GJK + EPA", I_BOTH, RED)]
    for i, (name, v, colour) in enumerate(rows):
        y = top + i * 44
        b.append(label(x0 - 10, y + 13, name, "xs", "end"))
        wpx = w_bar * v / peak
        b.append(box(x0, y, max(wpx, 1.5), 19, colour, opacity=0.85, width=0.8, rx=2))
        b.append(label(x0 + max(wpx, 1.5) + 10, y + 14, f"{v:.1f} ns", "xs mono",
                       "start"))
    b.append(label(x0 + w_bar / 2, top - 14, f"{20000:,} box pairs, release build",
                   "xs muted", "middle"))

    y = top + 3 * 44 + 10
    b.append(label(20, y + 14, f"EPA's share: {I_EPA:.1f} ns, and over a thousand "
                               f"queries it allocated {I_ALLOCS} times —", "xs", "start"))
    b.append(label(20, y + 30, "measured by replacing the global `operator new` and "
                               "counting, as 6.17 §9 did.", "xs", "start"))

    b.append(label(20, y + 60, f"AND TWO STRUCT DEFINITIONS WERE WORTH "
                               f"{I_GAIN_PCT:.1f}%.", "xs t-hi", "start"))
    b.append(label(20, y + 78, f"{I_BEFORE:.1f} ns → {I_EPA:.1f} ns, measured back to "
                               "back, from deleting the", "xs", "start"))
    b.append(label(20, y + 94, "default member initialisers on `face` and `edge`. One "
                               "on any", "xs", "start"))
    b.append(label(20, y + 110, "member makes the whole type non-trivially-default-",
                   "xs", "start"))
    b.append(label(20, y + 126, "constructible, which made `polytope p;` write four "
                                "kilobytes of", "xs", "start"))
    b.append(label(20, y + 142, "zeroes the next line overwrote, and `expand`'s "
                                "horizon array three", "xs", "start"))
    b.append(label(20, y + 158, "more — once per pass. The third array was left "
                                "alone: removing", "xs", "start"))
    b.append(label(20, y + 174, f"its initialisers measured {I_MIRROR:.1f} against "
                                f"{I_KEPT:.1f}, inside the noise.", "xs", "start"))

    # ---- right: cost against depth ----------------------------------------
    px = 500
    b.append(label(px + 190, 54, "and it costs more the deeper they are", "xs",
                   "middle"))
    rows2 = [(f"{sep:.2f} m", f"{it:.2f}", f"{v:.2f}", f"{ns:.0f}")
             for sep, it, v, ns in I_DEPTH]
    tbl, th = table(px, 68, ["centre offset", "expansions", "vertices", "ns"],
                    rows2, [130, 105, 90, 70])
    b += tbl
    b.append(label(px, 68 + th + 26, "A deeper overlap is a bigger difference set "
                                     "with the origin", "xs muted", "start"))
    b.append(label(px, 68 + th + 42, "further from its boundary, so there is more "
                                     "polytope to", "xs muted", "start"))
    b.append(label(px, 68 + th + 58, "build. Which is one more argument for a "
                                     "solver that keeps", "xs muted", "start"))
    b.append(label(px, 68 + th + 74, "objects from getting deep in the first "
                                     "place — 8.10's job.", "xs muted", "start"))

    return svg(uid, W, H,
               "What EPA costs beside the SAT and GJK, and how the cost grows with "
               "penetration depth.",
               "Left, three bars: the SAT's full box test at 84.2 ns, GJK's distance "
               "query at 77.9 ns, and GJK followed by EPA at 1146.1 ns, of which EPA "
               "is 1068.1. Deleting default member initialisers on two internal "
               "structs took that from 1344.0 ns, measured back to back. Right, a "
               "table showing expansions rising from 6.3 to 16.5 and cost from 627 to "
               "1886 ns as two crates are driven from a shallow overlap to a deep one.",
               b)


def main():
    for n in range(1, 12):
        fn = globals()[f"fig{n}"]
        path = os.path.join(OUT, f"l86_fig{n}.svg")
        with open(path, "w") as fh:
            fh.write(fn())
        print(f"wrote {path}")


if __name__ == "__main__":
    main()
