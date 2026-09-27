#!/usr/bin/env python3
"""scratch/figs_85.py — Lesson 8.5's diagrams.

Same rules as 5.1-8.4's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - filenames numbered by PAGE ORDER
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - `rule()` takes a CSS CLASS; `cline()` takes a COLOUR
  - LEGENDS AND ANNOTATION BOXES GO OUTSIDE THE PLOT
  - a SHAPE can leave the viewBox where a label cannot
  - panel heights are COMPUTED, never guessed
  - the figure palette must contain the DEMO's own colours (rle_rects SNAPS)

Every number below is transcribed from scratch/verify_85.log. Nothing here is
estimated, and the section of the harness each block came from is named above it.

*** WRITTEN IN LITERAL UNICODE THROUGHOUT, NEVER \\uXXXX. *** 8.1's figs file
mixed the two and a str.replace written against one form fails SILENTLY against
the other.

THE COLOUR RULE, inherited from 8.4 so a reader moving between the two lessons
relearns nothing:
  GREEN  = apart / this is a proof of separation
  RED    = overlapping
  AMBER  = the simplex, and the answer the algorithm returned
  BLUE   = shape A and geometry belonging to it
  PURPLE = v, the current closest point — the one thing 8.4 had no name for
  GREY   = the set itself, or a candidate that found nothing

SEVERAL FIGURES RUN A REAL 2D GJK. `gjk2` below is the same algorithm as
engine/src/phys/gjk.cpp with the tetrahedron case removed, so the simplices and
the `v` sequence in figures 4 and 5 are SEARCHED rather than drawn. If the C++
changes and this stops agreeing, the figures are wrong and should be regenerated
rather than patched.
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
from figs_76 import table, f, fmt_e                                 # noqa: E402
from figs_81 import legend, logspan                                 # noqa: E402

OUT = "scratch"

# demos/gjk/main.cpp's constants, transcribed. rle_rects SNAPS every sampled
# cell to the nearest palette entry, so a colour the demo draws and this list
# omits comes out as whichever entry happens to be closest.
C_BG      = "#101218"
C_GRID    = "#262a34"
C_AXIS    = "#545c6c"
C_SHAPE_A = "#96a0ff"
C_SHAPE_B = "#78dca0"
C_HIT     = "#eb6060"
C_WITNESS = "#ebc860"
C_CLOUD   = "#464e60"
C_VLINE   = "#eb78c8"
C_ORIGIN  = "#e6ecf8"
GJK_PALETTE = [hexrgb(c) for c in (C_BG, C_GRID, C_AXIS, C_SHAPE_A, C_SHAPE_B,
                                   C_HIT, C_WITNESS, C_CLOUD, C_VLINE, C_ORIGIN)]


def render_panel(ppm, crop, x, y, px, cell=4, levels=3, peak=True):
    """A real render, downsampled and run-length encoded. See figs_76."""
    _w, _h, data = read_ppm(os.path.join(OUT, ppm))
    sampler = peak_sample if peak else box_sample
    grid_w, grid_h, grid = sampler(data, _w, crop, cell)
    body = rle_rects(grid, grid_w, grid_h, x, y, px, GJK_PALETTE, levels=levels,
                     bg_class="fill-shot")
    return (['<g shape-rendering="crispEdges">'] + body + ['</g>'],
            grid_w * px, grid_h * px)


# ===========================================================================
# A 2D convex-geometry kit, so the figures are SEARCHED rather than drawn
# ===========================================================================
def sub(a, b):
    return (a[0] - b[0], a[1] - b[1])


def add(a, b):
    return (a[0] + b[0], a[1] + b[1])


def mul(a, s):
    return (a[0] * s, a[1] * s)


def dot2(a, b):
    return a[0] * b[0] + a[1] * b[1]


def norm2(a):
    return math.hypot(a[0], a[1])


def hull2(points):
    """Andrew's monotone chain. Returns the hull in counter-clockwise order."""
    pts = sorted(set(points))
    if len(pts) <= 2:
        return pts

    def half(seq):
        out = []
        for p in seq:
            while len(out) >= 2:
                o, a = out[-2], out[-1]
                if (a[0] - o[0]) * (p[1] - o[1]) - (a[1] - o[1]) * (p[0] - o[0]) > 0:
                    break
                out.pop()
            out.append(p)
        return out

    lower = half(pts)
    upper = half(reversed(pts))
    return lower[:-1] + upper[:-1]


def support2(points, d):
    """The farthest point of a point set in direction d. Ties break on the first."""
    best = points[0]
    best_v = dot2(best, d)
    for p in points[1:]:
        v = dot2(p, d)
        if v > best_v:
            best_v, best = v, p
    return best


def minkowski_difference(a_pts, b_pts):
    """Every a minus every b, hulled. Small sets only — this is O(n*m)."""
    return hull2([sub(a, b) for a in a_pts for b in b_pts])


def closest_on_segment(a, b):
    """Closest point of segment ab to the ORIGIN, with barycentric weights."""
    ab = sub(b, a)
    denom = dot2(ab, ab)
    if denom <= 0.0:
        return a, (1.0, 0.0), 1
    t = dot2(mul(a, -1.0), ab) / denom
    if t <= 0.0:
        return a, (1.0, 0.0), 1
    if t >= 1.0:
        return b, (0.0, 1.0), 2
    return add(a, mul(ab, t)), (1.0 - t, t), 0


def gjk2(a_pts, b_pts, max_steps=8):
    """The loop from engine/src/phys/gjk.cpp, in two dimensions.

    Returns a list of steps, each recording what the reader is shown: the simplex
    BEFORE the step, the search direction, the new support point, the reduced
    simplex, and the two bounds. The 3-point case reduces to whichever of the
    three clamped edges is nearest the origin, which is figure 6's argument with
    the interior candidate unreachable (in 2D a triangle containing the origin IS
    the terminal case).
    """
    cso = minkowski_difference(a_pts, b_pts)
    simplex = [support2(cso, (1.0, 0.0))]
    v = simplex[0]
    steps = []
    for _ in range(max_steps):
        d = mul(v, -1.0)
        w = support2(cso, d)
        vw = dot2(v, w)
        v2 = dot2(v, v)
        upper = math.sqrt(v2)
        lower = vw / upper if upper > 0 else 0.0
        before = list(simplex)
        if any(norm2(sub(p, w)) < 1e-12 for p in simplex):
            steps.append(dict(before=before, d=d, w=w, after=before, v=v,
                              upper=upper, lower=lower, done="duplicate"))
            break
        simplex = simplex + [w]
        if len(simplex) == 2:
            v, _, keep = closest_on_segment(simplex[0], simplex[1])
            if keep == 1:
                simplex = [simplex[0]]
            elif keep == 2:
                simplex = [simplex[1]]
        else:
            best = None
            for i in range(3):
                for j in range(i + 1, 3):
                    q, _, _ = closest_on_segment(simplex[i], simplex[j])
                    if best is None or dot2(q, q) < dot2(best[0], best[0]):
                        best = (q, [simplex[i], simplex[j]])
            v, simplex = best
        steps.append(dict(before=before, d=d, w=w, after=list(simplex), v=v,
                          upper=upper, lower=lower, done=""))
        if math.sqrt(dot2(v, v)) <= 1e-9:
            break
    return cso, steps


class View:
    """Metres to pixels, y up. Every figure that draws geometry uses one."""

    def __init__(self, cx, cy, scale):
        self.cx, self.cy, self.scale = cx, cy, scale

    def __call__(self, p):
        return (self.cx + p[0] * self.scale, self.cy - p[1] * self.scale)

    def x(self, p):
        return self.cx + p[0] * self.scale

    def y(self, p):
        return self.cy - p[1] * self.scale


def vpoly(view, pts, colour, width=1.4, dash=None, close=True):
    return poly([view(p) for p in pts], colour, width=width, dash=dash, close=close)


def vdot(view, p, colour, r=3.2):
    x, y = view(p)
    return f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r}" fill="{colour}"/>'


def cross_mark(view, p, colour, r=6):
    x, y = view(p)
    return (cline(x - r, y, x + r, y, colour, width=1.6) + "\n" +
            cline(x, y - r, x, y + r, colour, width=1.6))


# ===========================================================================
# Numbers transcribed from scratch/verify_85.log
# ===========================================================================

# A — the number the SAT could not give
A_SAT = 1.0000000
A_GJK = 1.7320508
A_RATIO = 0.577350
A_LOW_PCT = 42.3
A_WA = (0.5, 0.5, 0.5)
A_WB = (1.5, 1.5, 1.5)
A_PAIRS = 200000
A_APART = 124216
A_MEAN = 0.9817
A_WORST = 0.6901
A_LOW1 = (35671, 28.7)
A_LOW10 = (7031, 5.7)

# B — the support function is the shape
B_MINK_TRIALS = 2000
B_MINK_WORST = 0.0
B_CAPSULE_TRIALS = 2000
B_HULL_TRIALS = 5000
B_HULL_CHANGED = 0
B_ZERO_CAPSULE = 20000

# C — the simplex solver
C_FAT_TRIALS = 200000
C_FAT_WORSE = 3
C_THIN_TRIALS = 200000
C_THIN_WORSE = 1048
C_THIN_REL = 1.4540e-01
C_TRIANGLE_NS = 21.681
C_FLAT_TETRA_FACES = 4

# C.4 — the shared up axis
C4_PAIRS = 100000
C4_FLAT = [958, 21599, 77443, 0]        # terminal simplex size 1..4, shared axis
C4_TILT = [17, 1185, 21472, 77326]      # the same with one box tilted 1 degree
C4_DEEP_FLAT = (65633, 0)
C4_DEEP_TILT = (66361, 65644)
C4_IN_PLANE = 100000

# D — agreement with the SAT
D_PAIRS = 200000
D_SAT_OVERLAP = 113385
D_DISAGREE = 22
D_WORST_GAP = 0.0
D_CAPSULE_APART = 40577
D_HULL_APART = 13627

# E — the certificate
E_APART = 117722
E_BRACKETED = 117722
E_BELOW = 1.5643e-06
E_ABOVE = 4.1552e-06
E_SLACK_ABS = 8.5452e-04
E_SLACK_REL = 2.2590e-03
E_TILT_SLACK = 3.7726e-01
E_TILT_GROWTH = 441
E_WITNESS_A = 4.6654e-07
E_WITNESS_B = 4.4277e-07

# F — polytopes terminate, curves converge
F_HIST = [(1, 9071, 7.7), (2, 18819, 16.0), (3, 32347, 27.5), (4, 25927, 22.1),
          (5, 23681, 20.2), (6, 6128, 5.2), (7, 1265, 1.1), (8, 158, 0.1),
          (9, 18, 0.0), (10, 3, 0.0)]
F_MEAN = 3.52
F_MAX = 10
F_TOL = [1e-2, 1e-3, 1e-4, 1e-5, 1e-6, 1e-7]
F_BOX = [2.68, 2.77, 2.78, 2.78, 2.78, 2.80]
F_SPH = [1.00, 1.00, 1.00, 1.00, 1.00, 1.02]
F_CAP = [2.06, 3.14, 4.17, 5.24, 6.33, 7.43]
F_HULL = [1.57, 1.90, 1.97, 1.97, 1.97, 1.98]
F_ERR_BOX = [1.8278e-04, 6.5005e-07, 2.2071e-07, 2.2071e-07, 2.2071e-07, 2.2071e-07]
F_ERR_SPH = [1.6569e-07] * 6

# F.5 — the resolution floor (turned 2 m cubes)
F5 = [(1e0,  1.000000e+00, 9.73e-08, "no",  "separated"),
      (1e-1, 1.000003e-01, 8.64e-07, "no",  "separated"),
      (1e-2, 1.000050e-02, 2.95e-07, "no",  "separated"),
      (3e-3, 3.000084e-03, 8.31e-06, "no",  "separated"),
      (1e-3, 1.000388e-03, 6.21e-05, "no",  "separated"),
      (3e-4, 0.0,          0.0,      "no",  "intersecting"),
      (1e-4, 0.0,          0.0,      "no",  "intersecting"),
      (1e-5, 0.0,          0.0,      "no",  "intersecting"),
      (1e-6, 0.0,          0.0,      "no",  "intersecting")]
F5_PROOF_FLOOR = 4.77e-03

# F.6 — the margin
F6 = [(0.1, 2.1115e-05, 2.111), (1.0, 2.1116e-04, 2.112), (10.0, 2.1120e-03, 2.112),
      (100.0, 2.1102e-02, 2.110), (1000.0, 2.1102e-01, 2.110)]

# G — warm starting
G_QUERIES = 234197
G_COLD = 2.219
G_WARM = 2.152
G_RANDOM = 3.414

# H — where the origin is
H_ROWS = [(0.0,       1.552417e+00, 1.552417e+00, 1.552417e+00),
          (1.0,       1.552418e+00, 1.552417e+00, 1.552417e+00),
          (100.0,     1.552417e+00, 1.552418e+00, 1.552417e+00),
          (1000.0,    1.552417e+00, 1.552428e+00, 1.552417e+00),
          (10000.0,   1.552512e+00, 1.552424e+00, 1.552512e+00),
          (100000.0,  1.552418e+00, 1.551765e+00, 1.552418e+00),
          (1000000.0, 1.552669e+00, 1.542370e+00, 1.552669e+00)]
H_WORST_REL = 1.0698e-07
H_WORST_NAIVE = 1.0299e-02
H_FACTOR = 96270
H_MECH = [(100.0,     3.815e-06, 6.646e-07, 5.063e-08),
          (1000.0,    6.104e-05, 1.032e-05, 5.087e-08),
          (10000.0,   4.883e-04, 8.798e-05, 8.240e-09),
          (100000.0,  3.906e-03, 6.536e-04, 3.937e-08),
          (1000000.0, 6.250e-02, 1.030e-02, 2.872e-09)]

# I — the budget
I_PAIRS = 20000
I_OVERLAP = 10297
I_OVERLAPS_NS = 8.433
I_SAT_NS = 49.477
I_GJK_BOOL_NS = 73.223
I_GJK_DIST_NS = 114.744
I_HULL = [(8, 3.938, 77.896), (16, 7.542, 106.292), (32, 14.417, 144.438),
          (64, 25.896, 238.750), (128, 51.500, 411.854)]
I_DIRECT_NS = 1.908
I_INDIRECT_NS = 1.906


# ===========================================================================
# Figure 1 — the number the SAT could not give  (§2)
# ===========================================================================
def fig1():
    uid = "l85f1"
    W, H = 900, 400
    b = [cmarker(uid, "amber", AMBER), cmarker(uid, "green", GREEN),
         cmarker(uid, "grey", GREY)]

    b.append(label(20, 22, "Two unit cubes, centres (2, 2, 2) apart.", "sm", "start"))

    # ---- left: the construction, in oblique projection -------------------
    # x right, y up, z back-left at 30 degrees. Computed, not placed.
    oz = (-0.46 * math.cos(math.radians(30.0)), -0.46 * math.sin(math.radians(30.0)))

    def proj(p):
        return (p[0] + p[2] * oz[0], p[1] + p[2] * oz[1])

    view = View(128, 236, 62.0)

    def cube(c, h=0.5):
        pts = []
        for i in range(8):
            pts.append((c[0] + (h if i & 1 else -h),
                        c[1] + (h if i & 2 else -h),
                        c[2] + (h if i & 4 else -h)))
        return pts

    edges = [(0, 1), (2, 3), (4, 5), (6, 7), (0, 2), (1, 3), (4, 6), (5, 7),
             (0, 4), (1, 5), (2, 6), (3, 7)]

    for c, colour in (((0.0, 0.0, 0.0), BLUE), ((2.0, 2.0, 2.0), GREEN)):
        pts = [view(proj(p)) for p in cube(c)]
        for i, j in edges:
            b.append(cline(pts[i][0], pts[i][1], pts[j][0], pts[j][1], colour, width=1.3))

    # the corner-to-corner segment, which IS the distance
    pa = view(proj(A_WA))
    pb = view(proj(A_WB))
    b.append(cline(pa[0], pa[1], pb[0], pb[1], AMBER, width=2.4))
    b.append(f'<circle cx="{pa[0]:.1f}" cy="{pa[1]:.1f}" r="3.4" fill="{AMBER}"/>')
    b.append(f'<circle cx="{pb[0]:.1f}" cy="{pb[1]:.1f}" r="3.4" fill="{AMBER}"/>')
    mid = ((pa[0] + pb[0]) / 2, (pa[1] + pb[1]) / 2)
    b.append(rule(mid[0] + 6, mid[1] - 4, 286, 188, "grid", dash="3 3"))
    b.append(label(292, 186, f"√3 = {A_GJK:.4f} m", "sm t-hi", "start"))
    b.append(label(292, 202, "corner to corner: the two", "xs muted", "start"))
    b.append(label(292, 216, "witness points GJK returns", "xs muted", "start"))

    # ---- the x shadows, underneath ---------------------------------------
    sy = 348
    b.append(label(128, sy - 54, "their shadows on the x axis", "xs muted", "middle"))
    for lo, hi, colour in ((-0.5, 0.5, BLUE), (1.5, 2.5, GREEN)):
        x0 = view.x((lo, 0))
        x1 = view.x((hi, 0))
        b.append(cline(x0, sy, x1, sy, colour, width=5.0))
    gx0 = view.x((0.5, 0))
    gx1 = view.x((1.5, 0))
    b.append(rule(gx0, sy - 10, gx0, sy + 10, "grid", dash="3 3"))
    b.append(rule(gx1, sy - 10, gx1, sy + 10, "grid", dash="3 3"))
    b.append(arrow(gx0, sy - 16, gx1, sy - 16, uid, "s"))
    b.append(label((gx0 + gx1) / 2, sy - 24, f"{A_SAT:.1f} m", "xs", "middle"))
    b.append(label(128, sy + 26, "every one of the SAT's fifteen axes reports this",
                   "xs muted", "middle"))

    # ---- right: the ratio, and how often it matters ----------------------
    px = 470
    b.append(label(px, 66, "The SAT is not wrong. It is measuring the "
                           "COMPONENT of the offset", "xs", "start"))
    b.append(label(px, 82, "along one direction; the distance is its LENGTH, and the "
                           "closest", "xs", "start"))
    b.append(label(px, 98, "features here are two CORNERS, whose direction is in no "
                           "axis list.", "xs", "start"))

    rows = [("the shipped first-axis answer", f"{A_SAT:.4f} m"),
            ("best of all fifteen axes", f"{A_SAT:.4f} m"),
            ("GJK", f"{A_GJK:.4f} m"),
            ("ratio", f"{A_RATIO:.6f} = 1/√3"),
            ("low by", f"{A_LOW_PCT:.1f}%")]
    tbl, th = table(px, 118, ["", "value"], rows, [250, 120])
    b += tbl

    b.append(label(px, 118 + th + 34, f"And on {A_PAIRS:,} random box pairs "
                                      f"({A_APART:,} apart):", "xs", "start"))
    rows2 = [("mean best-of-15 / true", f"{A_MEAN:.4f}"),
             ("worst", f"{A_WORST:.4f}"),
             ("more than 1% low", f"{A_LOW1[0]:,}  ({A_LOW1[1]:.1f}%)"),
             ("more than 10% low", f"{A_LOW10[0]:,}  ({A_LOW10[1]:.1f}%)")]
    tbl2, _ = table(px, 118 + th + 44, ["", ""], rows2, [250, 160])
    b += tbl2

    return svg(uid, W, H,
               "Two unit cubes whose centres are (2,2,2) apart: the SAT reports a gap "
               "of 1 m, GJK reports the true distance of sqrt(3).",
               "Left, an oblique view of two unit cubes with their nearest corners joined "
               "by a segment labelled sqrt(3) = 1.7320508 m, and below them their shadows "
               "on the x axis, which are 1 m apart. Right, a table giving the shipped SAT "
               "answer, the best of all fifteen axes, GJK's answer and the ratio 1/sqrt(3), "
               "followed by the distribution over 200,000 random pairs.",
               b)


# The two shapes figures 2, 4 and 5 all work on. A pentagon and a quadrilateral,
# in metres, chosen so that nothing about the answer is symmetric.
SHAPE_A = [(-2.6, 0.5), (-2.0, 1.5), (-0.9, 1.3), (-0.6, 0.1), (-1.8, -0.9)]
SHAPE_B = [(1.1, -0.6), (2.3, -0.2), (2.1, 1.2), (1.0, 1.0)]
SHAPE_B_NEAR = [(p[0] - 2.5, p[1] - 0.35) for p in SHAPE_B]


# ===========================================================================
# Figure 2 — two sets become one set  (§4)
# ===========================================================================
def fig2():
    uid = "l85f2"
    W, H = 900, 420
    b = [cmarker(uid, "purple", PURPLE), cmarker(uid, "amber", AMBER)]

    pw, ph = 288, 300
    panels = [(8, 62), (306, 62), (604, 62)]
    heads = ["1 · two shapes, in the world",
             "2 · one set: A ⊖ B",
             "3 · and when they overlap"]

    b.append(label(20, 22, "The substitution the whole algorithm rests on.",
                   "sm", "start"))
    b.append(label(20, 42, "A question about a PAIR becomes a question about a POINT.",
                   "xs muted", "start"))

    for (px, py), head in zip(panels, heads):
        b += frame(px, py, pw, ph)
        b.append(label(px + 10, py + 18, head, "xs muted", "start"))

    # ---- panel 1: the world ---------------------------------------------
    v1 = View(panels[0][0] + pw / 2, panels[0][1] + ph / 2 + 10, 44.0)
    b.append(vpoly(v1, SHAPE_A, BLUE))
    b.append(vpoly(v1, SHAPE_B, GREEN))
    b.append(label(v1.x((-1.7, 0)), v1.y((0, 0.45)), "A", "sm t-hi", "middle"))
    b.append(label(v1.x((1.6, 0)), v1.y((0, 0.35)), "B", "sm t-hi", "middle"))

    # the true closest pair, found by brute force over the two hulls' edges
    def closest_pair(pa, pb):
        best = None
        for i in range(len(pa)):
            for j in range(len(pb)):
                for _ in range(1):
                    p0, p1 = pa[i], pa[(i + 1) % len(pa)]
                    q0, q1 = pb[j], pb[(j + 1) % len(pb)]
                    for s in range(41):
                        t = s / 40.0
                        p = add(p0, mul(sub(p1, p0), t))
                        for u in range(41):
                            r = u / 40.0
                            q = add(q0, mul(sub(q1, q0), r))
                            d = norm2(sub(q, p))
                            if best is None or d < best[0]:
                                best = (d, p, q)
        return best

    dist, cp, cq = closest_pair(SHAPE_A, SHAPE_B)
    b.append(cline(v1.x(cp), v1.y(cp), v1.x(cq), v1.y(cq), AMBER, width=2.2))
    b.append(vdot(v1, cp, AMBER))
    b.append(vdot(v1, cq, AMBER))
    b.append(label(panels[0][0] + pw / 2, panels[0][1] + ph - 16,
                   f"distance = {dist:.4f} m", "xs t-hi", "middle"))

    # ---- panels 2 and 3: the difference set ------------------------------
    for idx, (shape_b, overlapping) in enumerate(((SHAPE_B, False), (SHAPE_B_NEAR, True))):
        px, py = panels[idx + 1]
        v = View(px + pw / 2, py + ph / 2 + 10, 30.0)
        cso = minkowski_difference(SHAPE_A, shape_b)
        b.append(vpoly(v, cso, GREY, width=1.5))
        b.append(cross_mark(v, (0.0, 0.0), RED if overlapping else GREEN, r=7))

        if not overlapping:
            # the closest point of the set to the origin, by the same brute force
            best = None
            for i in range(len(cso)):
                p0, p1 = cso[i], cso[(i + 1) % len(cso)]
                q, _, _ = closest_on_segment(p0, p1)
                if best is None or norm2(q) < norm2(best):
                    best = q
            b.append(carrow(v.x((0, 0)), v.y((0, 0)), v.x(best), v.y(best),
                            uid, "purple", PURPLE, width=2.0))
            b.append(label(px + pw / 2, py + ph - 34,
                           f"|v| = {norm2(best):.4f} m", "xs t-hi", "middle"))
            b.append(label(px + pw / 2, py + ph - 16,
                           "the same number as panel 1", "xs muted", "middle"))
        else:
            b.append(label(px + pw / 2, py + ph - 34,
                           "the origin is INSIDE", "xs t-hi", "middle"))
            b.append(label(px + pw / 2, py + ph - 16,
                           "so the shapes overlap", "xs muted", "middle"))

        # The label goes where the SET is not. In panel 2 the origin sits
        # outside the outline and a nudge is enough; in panel 3 it is inside, so
        # the label is parked clear of the polygon with a leader to it.
        if overlapping:
            far = max(p[0] for p in cso) + 0.9
            lx, ly = v.x((far, 0)), v.y((0, 0.0))
            b.append(rule(v.x((0, 0)) + 9, v.y((0, 0)), lx - 4, ly, "grid", dash="3 3"))
            b.append(label(lx, ly + 4, "origin", "xs", "start"))
        else:
            b.append(label(v.x((0, 0)) + 12, v.y((0, 0)) - 12, "origin", "xs", "start"))

    return svg(uid, W, H,
               "Two shapes and their Minkowski difference: overlap becomes "
               "containment of the origin, and distance becomes distance to the origin.",
               "Three panels. The first shows a blue pentagon A and a green "
               "quadrilateral B with the segment between their closest points. The "
               "second shows the single set A minus B, drawn in grey, with the origin "
               "marked outside it and an arrow v from the origin to its nearest point, "
               "whose length is the same number as in the first panel. The third shows "
               "the difference set when the shapes overlap, with the origin inside it.",
               b)


# ===========================================================================
# Figure 3 — the support function IS the shape  (§5)
# ===========================================================================
def fig3():
    uid = "l85f3"
    W, H = 900, 400
    b = [cmarker(uid, "purple", PURPLE), cmarker(uid, "amber", AMBER),
         cmarker(uid, "blue", BLUE)]

    b.append(label(20, 22, "One function, and it determines the set completely.",
                   "sm", "start"))

    pw, ph = 420, 316
    for px in (8, 472):
        b += frame(px, 46, pw, ph)
    b.append(label(18, 64, "1 · a direction in, a point out", "xs muted", "start"))
    b.append(label(482, 64, "2 · and a Minkowski sum adds its operands' supports",
                  "xs muted", "start"))

    # ---- panel 1 ---------------------------------------------------------
    v = View(8 + pw / 2 - 20, 46 + ph / 2 - 6, 62.0)
    blob = [(-1.2, -0.9), (0.3, -1.2), (1.3, -0.2), (1.0, 1.0), (-0.4, 1.3), (-1.4, 0.4)]
    b.append(vpoly(v, blob, GREY, width=1.6))

    for angle, name in ((28.0, "d₁"), (118.0, "d₂"), (232.0, "d₃")):
        d = (math.cos(math.radians(angle)), math.sin(math.radians(angle)))
        sp = support2(blob, d)
        # the supporting line: through sp, perpendicular to d
        perp = (-d[1], d[0])
        p0 = add(sp, mul(perp, 0.95))
        p1 = add(sp, mul(perp, -0.95))
        b.append(cline(v.x(p0), v.y(p0), v.x(p1), v.y(p1), GREY, width=1.0, dash="4 4"))
        tail = add(sp, mul(d, -0.72))
        b.append(carrow(v.x(tail), v.y(tail), v.x(add(sp, mul(d, 0.30))),
                        v.y(add(sp, mul(d, 0.30))), uid, "amber", AMBER, width=1.8))
        b.append(vdot(v, sp, AMBER, r=3.6))
        lp = add(sp, mul(d, 0.56))
        b.append(label(v.x(lp), v.y(lp) + 4, name, "xs t-hi", "middle"))

    b.append(label(8 + pw / 2, 46 + ph - 40,
                   "a convex set is the intersection of every half-plane containing it",
                   "xs muted", "middle"))
    b.append(label(8 + pw / 2, 46 + ph - 22,
                   "and `support` names the boundary of each one",
                   "xs muted", "middle"))

    # ---- panel 2: segment ⊕ ball ----------------------------------------
    v2 = View(472 + pw / 2 - 10, 46 + ph / 2 + 4, 62.0)
    half_h, radius = 1.0, 0.55
    spine = ((0.0, -half_h), (0.0, half_h))
    b.append(cline(v2.x(spine[0]), v2.y(spine[0]), v2.x(spine[1]), v2.y(spine[1]),
                   BLUE, width=2.2))
    # the capsule outline: the segment inflated, drawn as two arcs and two lines
    r_px = radius * v2.scale
    x0, y0 = v2(spine[0])
    x1, y1 = v2(spine[1])
    b.append(f'<path d="M {x0 - r_px:.1f} {y0:.1f} A {r_px:.1f} {r_px:.1f} 0 0 0 '
             f'{x0 + r_px:.1f} {y0:.1f} L {x1 + r_px:.1f} {y1:.1f} '
             f'A {r_px:.1f} {r_px:.1f} 0 0 0 {x1 - r_px:.1f} {y1:.1f} Z" '
             f'fill="none" stroke="{GREY}" stroke-width="1.6"/>')

    ang = 34.0
    d = (math.cos(math.radians(ang)), math.sin(math.radians(ang)))
    seg_term = spine[1] if d[1] >= 0 else spine[0]
    ball_term = mul(d, radius)
    total = add(seg_term, ball_term)

    b.append(carrow(v2.x((0, 0)), v2.y((0, 0)), v2.x(seg_term), v2.y(seg_term),
                    uid, "blue", BLUE, width=2.0))
    b.append(carrow(v2.x(seg_term), v2.y(seg_term), v2.x(total), v2.y(total),
                    uid, "amber", AMBER, width=2.0))
    b.append(vdot(v2, total, AMBER, r=3.6))
    b.append(label(v2.x((0, 0)) - 46, v2.y((0, 0.55)), "±h·axis", "xs t-hi", "end"))
    b.append(label(v2.x(total) + 14, v2.y(total) + 4, "r·normalise(d)", "xs t-hi", "start"))

    dtail = add(total, mul(d, 0.30))
    dtip = add(total, mul(d, 0.95))
    b.append(carrow(v2.x(dtail), v2.y(dtail), v2.x(dtip), v2.y(dtip),
                    uid, "purple", PURPLE, width=1.6))
    b.append(label(v2.x(dtip) + 6, v2.y(dtip) - 6, "d", "xs t-hi", "start"))

    b.append(label(472 + pw / 2, 46 + ph - 40,
                   "support_{X⊕Y}(d) = support_X(d) + support_Y(d)",
                   "sm t-hi", "middle"))
    b.append(label(472 + pw / 2, 46 + ph - 22,
                   "because maximising dot(x+y, d) maximises each term separately",
                   "xs muted", "middle"))

    return svg(uid, W, H,
               "The support function: a direction in, the farthest point out — and "
               "the sum rule that makes a capsule two terms.",
               "Left, a convex polygon with three directions drawn; each picks out one "
               "vertex and a dashed supporting line through it, with the note that the "
               "set is the intersection of every half-plane containing it. Right, a "
               "capsule drawn as a segment inflated by a radius, with its support point "
               "built as the sum of the segment's support and the ball's.",
               b)


# ===========================================================================
# Figure 4 — the search, three iterations, with real numbers  (§6)
# ===========================================================================
def fig4():
    uid = "l85f4"
    W, H = 900, 400
    b = [cmarker(uid, "purple", PURPLE), cmarker(uid, "amber", AMBER),
         cmarker(uid, "green", GREEN)]

    cso, steps = gjk2(SHAPE_A, SHAPE_B)
    shown = steps[:3]

    b.append(label(20, 22, "The loop, run on figure 2's difference set.", "sm", "start"))
    b.append(label(20, 42, "Nothing here is drawn: the same algorithm as "
                           "engine/src/phys/gjk.cpp produced every point.",
                   "xs muted", "start"))

    pw, ph = 288, 268
    panels = [(8, 62), (306, 62), (604, 62)]

    for idx, (px, py) in enumerate(panels):
        if idx >= len(shown):
            break
        st = shown[idx]
        b += frame(px, py, pw, ph)
        b.append(label(px + 10, py + 18, f"iteration {idx + 1}", "xs muted", "start"))

        v = View(px + pw / 2, py + ph / 2 - 16, 33.0)
        b.append(vpoly(v, cso, GREY, width=1.2))
        b.append(cross_mark(v, (0.0, 0.0), GREEN, r=6))

        # the simplex BEFORE this step, and the search direction
        for p in st["before"]:
            b.append(vdot(v, p, AMBER, r=3.4))
        if len(st["before"]) == 2:
            p0, p1 = st["before"]
            b.append(cline(v.x(p0), v.y(p0), v.x(p1), v.y(p1), AMBER, width=1.8))

        vv = st["v"]
        b.append(carrow(v.x((0, 0)), v.y((0, 0)), v.x(vv), v.y(vv),
                        uid, "purple", PURPLE, width=1.8))

        # −v, the direction the support is taken in, drawn from v outward
        tip = add(vv, mul(st["d"], 0.30))
        b.append(rule(v.x(vv), v.y(vv), v.x(tip), v.y(tip), "grid", dash="3 3"))

        w = st["w"]
        b.append(vdot(v, w, GREEN, r=4.2))
        b.append(label(v.x(w) + 9, v.y(w) - 7, "w", "xs t-hi", "start"))

        rows = [("upper |v|", f"{st['upper']:.4f}"),
                ("lower", f"{st['lower']:.4f}"),
                ("simplex", f"{len(st['before'])} → {len(st['after'])}")]
        tbl, _ = table(px + 14, py + ph - 74, ["", ""], rows, [120, 92], row_h=15.0,
                       head_h=6.0)
        b += tbl

    return svg(uid, W, H,
               "Three iterations of GJK on the difference set from figure 2, with the "
               "simplex, the search direction and both bounds at each step.",
               "Three panels, one per iteration. Each shows the grey difference set, the "
               "origin as a green cross, the current simplex in amber, the vector v drawn "
               "from the origin in purple, a dashed continuation in the search direction "
               "minus v, and the new support point w as a green dot. A small table under "
               "each panel gives the upper bound, the lower bound and how the simplex "
               "size changed.",
               b)


# ===========================================================================
# Figure 5 — the two bounds closing  (§6)
# ===========================================================================
def fig5():
    uid = "l85f5"
    W, H = 900, 360
    b = [cmarker(uid, "purple", PURPLE), cmarker(uid, "green", GREEN)]

    _cso, steps = gjk2(SHAPE_A, SHAPE_B)

    b.append(label(20, 22, "The answer is squeezed from both sides, and both sides "
                           "are provable.", "sm", "start"))

    px, py, pw, ph = 60, 56, 520, 236
    b += frame(px, py, pw, ph)

    hi = max(s["upper"] for s in steps) * 1.06
    lo = min(min(s["lower"] for s in steps), 0.0)
    span = hi - lo

    def ty(value):
        return py + ph - 26 - (value - lo) / span * (ph - 52)

    for k in range(5):
        value = lo + span * k / 4.0
        y = ty(value)
        b.append(rule(px + 44, y, px + pw - 14, y, "grid"))
        b.append(label(px + 38, y + 4, f"{value:.2f}", "xs mono muted", "end"))

    n = len(steps)
    def tx(i):
        return px + 58 + (pw - 84) * i / max(1, n - 1)

    up = [(tx(i), ty(s["upper"])) for i, s in enumerate(steps)]
    lw = [(tx(i), ty(s["lower"])) for i, s in enumerate(steps)]
    b.append(poly(up, PURPLE, width=2.2, close=False))
    b.append(poly(lw, GREEN, width=2.2, close=False))
    for (x, y) in up:
        b.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3.0" fill="{PURPLE}"/>')
    for (x, y) in lw:
        b.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3.0" fill="{GREEN}"/>')

    for i in range(n):
        b.append(label(tx(i), py + ph - 8, str(i + 1), "xs mono muted", "middle"))
    b.append(label(px + pw / 2, py + ph + 16, "iteration", "xs muted", "middle"))
    b.append(label(px + 6, py + 16, "metres", "xs muted", "start"))

    b += legend(px + pw + 30, py + 30, [
        (PURPLE, "upper: |v|, the length of a real"),
        (None, "vector between two real points"),
        (GREEN, "lower: the supporting plane's"),
        (None, "distance from the origin"),
    ])
    final = steps[-1]
    rows = [("final upper", f"{final['upper']:.6f} m"),
            ("final lower", f"{final['lower']:.6f} m"),
            ("slack", f"{final['upper'] - final['lower']:.2e} m")]
    tbl, _ = table(px + pw + 30, py + 124, ["", ""], rows, [140, 110], row_h=16.0)
    b += tbl
    b.append(rule(px + pw + 30, py + 190, px + pw + 268, py + 190, "grid"))
    b.append(label(px + pw + 30, py + 214,
                   "When they meet the answer is not", "xs", "start"))
    b.append(label(px + pw + 30, py + 228,
                   "believed — it is PROVEN.", "xs", "start"))

    return svg(uid, W, H,
               "GJK's upper and lower bounds converging on the distance, iteration by "
               "iteration.",
               "A chart with iteration number along the bottom and metres up the side. A "
               "purple line falls from above, the upper bound given by the length of v; a "
               "green line rises from below, the lower bound given by the supporting "
               "plane. They meet within a few iterations, and a table gives the final "
               "values and the slack between them.",
               b)


# ===========================================================================
# Figure 6 — four candidates, and why not seven regions  (§7)
# ===========================================================================
def fig6():
    uid = "l85f6"
    W, H = 900, 400
    b = [cmarker(uid, "purple", PURPLE), cmarker(uid, "amber", AMBER),
         cmarker(uid, "green", GREEN), cmarker(uid, "red", RED)]

    b.append(label(20, 22, "The simplex solver: compute every candidate, or decide "
                           "which one to compute.", "sm", "start"))

    pw, ph = 420, 318
    for px in (8, 472):
        b += frame(px, 46, pw, ph)
    b.append(label(18, 64, "1 · four candidates cover all seven regions", "xs muted",
                   "start"))
    b.append(label(482, 64, "2 · and deciding goes wrong exactly where GJK lives",
                   "xs muted", "start"))

    # ---- panel 1 ---------------------------------------------------------
    v = View(8 + pw / 2 - 30, 46 + ph / 2 + 10, 56.0)
    tri = [(-1.5, -0.9), (1.4, -1.3), (0.4, 1.5)]
    b.append(vpoly(v, tri, GREY, width=1.6))
    for i, p in enumerate(tri):
        b.append(vdot(v, p, GREY, r=3.0))
        b.append(label(v.x(p) + (14 if p[0] > 0 else -14), v.y(p) + 4,
                       "abc"[i], "xs muted", "start" if p[0] > 0 else "end"))
    b.append(cross_mark(v, (0.0, 0.0), GREEN, r=6))

    # The origin is INSIDE this triangle, so the interior candidate wins. To
    # show the losing candidates too, the three clamped edges are marked.
    cands = []
    for i in range(3):
        q, _, _ = closest_on_segment(tri[i], tri[(i + 1) % 3])
        cands.append(q)
    for q in cands:
        b.append(vdot(v, q, AMBER, r=3.2))
        b.append(rule(v.x((0, 0)), v.y((0, 0)), v.x(q), v.y(q), "grid", dash="3 3"))

    b.append(label(8 + pw / 2 - 30, 46 + ph - 58,
                   "the origin is inside, so the interior candidate wins at 0",
                   "xs muted", "middle"))
    b.append(label(8 + pw / 2 - 30, 46 + ph - 40,
                   "and the three clamped edges are the amber points",
                   "xs muted", "middle"))
    b.append(label(8 + pw / 2 - 30, 46 + ph - 18,
                   "a clamped edge already contains its two endpoints, so three of "
                   "them", "xs muted", "middle"))
    b.append(label(8 + pw / 2 - 30, 46 + ph - 4,
                   "cover all six boundary regions", "xs muted", "middle"))

    # ---- panel 2: the same failure, one dimension down ------------------
    v2 = View(472 + 104, 46 + 150, 58.0)
    # Chosen, not found: the line through 0.35·(cos60°, sin60°) perpendicular to
    # that radius, so the foot of the perpendicular is 0.35 m from the origin
    # while the nearest point OF THE SEGMENT is 1.25 m away.
    seg = [(0.986, 0.771), (2.253, 1.503)]
    b.append(cline(v2.x(seg[0]), v2.y(seg[0]), v2.x(seg[1]), v2.y(seg[1]), GREY, width=2.4))
    # the infinite line it lies on
    dirv = sub(seg[1], seg[0])
    nl = norm2(dirv)
    unit = mul(dirv, 1.0 / nl)
    far0 = add(seg[0], mul(unit, -1.3))
    far1 = add(seg[1], mul(unit, 0.5))
    b.append(cline(v2.x(far0), v2.y(far0), v2.x(far1), v2.y(far1), GREY,
                   width=1.0, dash="4 4"))
    b.append(cross_mark(v2, (0.0, 0.0), GREEN, r=6))

    # the true closest point on the SEGMENT, and the foot on the LINE
    true_q, _, _ = closest_on_segment(seg[0], seg[1])
    t_foot = dot2(mul(seg[0], -1.0), dirv) / dot2(dirv, dirv)
    foot = add(seg[0], mul(dirv, t_foot))

    b.append(carrow(v2.x((0, 0)), v2.y((0, 0)), v2.x(true_q), v2.y(true_q),
                    uid, "amber", AMBER, width=2.0))
    b.append(carrow(v2.x((0, 0)), v2.y((0, 0)), v2.x(foot), v2.y(foot),
                    uid, "red", RED, width=2.0))
    b.append(vdot(v2, true_q, AMBER, r=3.6))
    b.append(vdot(v2, foot, RED, r=3.6))
    b.append(label(v2.x(true_q) - 6, v2.y(true_q) - 20,
                   f"nearest point OF the simplex: {norm2(true_q):.3f} m",
                   "xs t-hi", "end"))
    b.append(label(v2.x(foot) + 10, v2.y(foot) + 20,
                   f"foot on its LINE: {norm2(foot):.3f} m", "xs", "start"))

    b.append(label(482, 46 + 208,
                   "A mis-selected region returns the foot of the perpendicular on",
                   "xs muted", "start"))
    b.append(label(482, 46 + 224,
                   "the simplex's own affine hull, which is NEARER than the simplex",
                   "xs muted", "start"))
    b.append(label(482, 46 + 240,
                   "is. Measured on 200,000 triangles each:", "xs muted", "start"))

    rows = [("fat, origin anywhere", f"{C_FAT_WORSE}"),
            ("slivers, origin 10 µm–10 mm away", f"{C_THIN_WORSE:,}"),
            ("worst relative on a sliver", f"{C_THIN_REL * 100:.1f}%")]
    tbl, _ = table(482, 46 + 252, ["textbook answer beats ours", "count"], rows,
                   [250, 110], row_h=16.0)
    b += tbl

    return svg(uid, W, H,
               "Four candidates cover a triangle's seven Voronoi regions, and the "
               "textbook shortcut that decides between them fails on slivers.",
               "Left, a triangle with the origin inside it, the three clamped-edge "
               "candidates marked in amber and dashed lines drawn to them. Right, the "
               "same failure one dimension down: a segment with the origin off to one "
               "side, an amber arrow to the true closest point on the segment and a red "
               "arrow to the foot of the perpendicular on the infinite line, which is "
               "nearer and wrong. A table gives the measured failure counts on fat and "
               "sliver triangles.",
               b)


# ===========================================================================
# Figure 7 — the demo  (§8)
# ===========================================================================
def fig7():
    uid = "l85f7"
    b = []
    px = 12
    # cell=3 rather than 2: the demo's wireframes are one pixel wide and
    # `peak_sample` keeps the brightest pixel of each block, so the lines survive
    # a coarser grid while the file halves. px=1.4 puts the display size back.
    left, lw, lh = render_panel("_b85_p1.ppm", (0, 0, 960, 540), px, 76, 1.4, cell=3,
                                levels=3)
    right, rw, rh = render_panel("_b85_hit.ppm", (0, 0, 960, 540), px + lw + 16, 76, 1.4,
                                 cell=3, levels=3)
    W = px + lw + 16 + rw + px
    H = 76 + max(lh, rh) + 48

    b.append(label(20, 24, "demos/gjk, two frames.", "sm", "start"))
    b.append(label(20, 44, "Left: a capsule and a hull, apart. Right: two crates, "
                           "overlapping — and the simplex is a tetrahedron around the "
                           "origin.", "xs muted", "start"))
    b += left
    b += right
    b.append(label(px, 76 + lh + 22, "preset 2 · the witness points and the segment "
                                     "between them, and v drawn from the origin",
                   "xs muted", "start"))
    b.append(label(px + lw + 16, 76 + rh + 22,
                   "preset 1 · red means overlapping, and the origin is enclosed",
                   "xs muted", "start"))

    return svg(uid, W, H,
               "Two frames from the gjk demo: a separated pair with its witness points, "
               "and an overlapping pair whose simplex encloses the origin.",
               "Each frame has two panels. The left panel is the world: two shapes in "
               "wireframe with, when they are apart, a gold segment joining the two "
               "closest surface points. The right panel is the Minkowski difference "
               "outlined in grey, with the origin as a white cross, the simplex in gold "
               "and the vector v in pink. In the second frame the shapes overlap, the "
               "moving one is drawn red and the simplex is a tetrahedron around the "
               "origin.",
               b)


# ===========================================================================
# Figure 8 — a shared up axis collapses the search  (§9)
# ===========================================================================
def fig8():
    uid = "l85f8"
    W, H = 900, 400
    b = [cmarker(uid, "amber", AMBER), cmarker(uid, "grey", GREY)]

    b.append(label(20, 22, "Two boxes standing on the same floor, at any yaw.",
                   "sm", "start"))
    b.append(label(20, 42, f"{C4_PAIRS:,} pairs, and what GJK hands 8.6 when it stops.",
                   "xs muted", "start"))

    pw, ph = 400, 250
    for px, title in ((20, "shared up axis"), (470, "CONTROL · one box tilted 1°")):
        b += frame(px, 62, pw, ph)
        b.append(label(px + 12, 80, title, "xs muted", "start"))

    peak = max(max(C4_FLAT), max(C4_TILT))
    for px, data in ((20, C4_FLAT), (470, C4_TILT)):
        bw = 58
        gap = 26
        x0 = px + 54
        base = 62 + ph - 52
        top = 100
        for i, count in enumerate(data):
            bx = x0 + i * (bw + gap)
            h = (count / peak) * (base - top)
            colour = AMBER if i == 3 else GREY
            if h >= 1.0:
                b.append(f'<rect x="{bx}" y="{base - h:.1f}" width="{bw}" '
                         f'height="{h:.1f}" fill="{colour}" fill-opacity="0.85"/>')
            else:
                b.append(rule(bx, base, bx + bw, base, "grid", width=2.0))
            b.append(label(bx + bw / 2, base + 16, str(i + 1), "xs mono muted", "middle"))
            b.append(label(bx + bw / 2, base - h - 6, f"{count:,}", "xs mono", "middle"))
        b.append(rule(px + 40, base, px + pw - 20, base, "grid"))
        b.append(label(px + pw / 2, base + 34, "terminal simplex size", "xs muted",
                       "middle"))

    b.append(label(20, 336, "EVERY vertex of the difference set has y exactly 0 in the "
                            "left case, because both boxes share an up axis and "
                            "`support_local`", "xs", "start"))
    b.append(label(20, 352, f"picks the same sign on it for both. The search is confined "
                            f"to a plane, so it can never build a tetrahedron: "
                            f"{C4_FLAT[3]} of {C4_PAIRS:,},", "xs", "start"))
    b.append(label(20, 368, f"against {C4_TILT[3]:,} once one box is tilted by a single "
                            f"degree. Of the {C4_DEEP_FLAT[0]:,} pairs overlapping by "
                            f"more than 10 cm, {C4_DEEP_FLAT[1]} produce one.",
                   "xs", "start"))
    b.append(label(20, 388, "8.6's EPA is always described as starting from the "
                            "tetrahedron GJK left behind. On the commonest arrangement "
                            "in a game there is none.", "xs t-hi", "start"))

    return svg(uid, W, H,
               "With a shared up axis GJK never produces a tetrahedron; tilting one box "
               "one degree produces 77,326 of them.",
               "Two bar charts of the terminal simplex size, one to four points. On the "
               "left, two boxes sharing an up axis: the four-point bar is empty. On the "
               "right, the same pairs with one box tilted by one degree: the four-point "
               "bar holds 77,326 of 100,000.",
               b)


# ===========================================================================
# Figure 9 — polytopes terminate, curves converge  (§10)
# ===========================================================================
def fig9():
    uid = "l85f9"
    W, H = 900, 400
    b = [cmarker(uid, "blue", BLUE)]

    b.append(label(20, 22, "How much work a query costs, and what decides it.",
                   "sm", "start"))

    # ---- left: the histogram --------------------------------------------
    px, py, pw, ph = 20, 56, 380, 276
    b += frame(px, py, pw, ph)
    b.append(label(px + 12, py + 18, "iterations, 117,417 separated box pairs",
                   "xs muted", "start"))
    peak = max(c for _, c, _ in F_HIST)
    rows = [r for r in F_HIST if r[1] > 0]
    row_h = (ph - 66) / len(rows)
    for i, (k, count, pct) in enumerate(rows):
        y = py + 42 + i * row_h
        w = (count / peak) * (pw - 150)
        b.append(label(px + 26, y + 10, str(k), "xs mono muted", "end"))
        b.append(f'<rect x="{px + 34}" y="{y}" width="{max(w, 1.0):.1f}" '
                 f'height="{row_h - 5:.1f}" fill="{BLUE}" fill-opacity="0.80"/>')
        b.append(label(px + 40 + w, y + 10, f"{count:,}  ({pct:.1f}%)", "xs mono",
                       "start"))
    b.append(label(px + pw / 2, py + ph - 12,
                   f"mean {F_MEAN:.2f}   max {F_MAX}   none above 15",
                   "xs muted", "middle"))

    # ---- right: iterations against tolerance ----------------------------
    qx, qy, qw, qh = 436, 56, 330, 276
    b += frame(qx, qy, qw, qh)
    b.append(label(qx + 12, qy + 18, "mean iterations as the tolerance tightens",
                   "xs muted", "start"))

    lo, hi = 1e-7, 1e-2
    x0 = qx + 46
    span = qw - 76
    ymax = 8.0
    base = qy + qh - 40
    top = qy + 44

    def yy(v):
        return base - (v / ymax) * (base - top)

    for g in (0, 2, 4, 6, 8):
        y = yy(g)
        b.append(rule(x0, y, x0 + span, y, "grid"))
        b.append(label(x0 - 8, y + 4, str(g), "xs mono muted", "end"))
    # Reversed, so the tolerance TIGHTENS to the right, which is the direction
    # the prose reads in.
    def txx(t):
        return x0 + span - (logspan(t, lo, hi, x0, span) - x0)

    for t in F_TOL:
        b.append(label(txx(t), base + 16, f"1e{int(round(math.log10(t)))}",
                       "xs mono muted", "middle"))

    series = ((F_BOX, GREY, "box"), (F_HULL, BLUE, "hull"),
              (F_SPH, GREEN, "sphere"), (F_CAP, RED, "capsule"))
    for values, colour, _name in series:
        pts = [(txx(t), yy(v)) for t, v in zip(F_TOL, values)]
        b.append(poly(pts, colour, width=2.2, close=False))
        for (x, y) in pts:
            b.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="2.6" fill="{colour}"/>')

    b.append(label(qx + qw / 2, base + 34, "tolerance, tightening →", "xs muted",
                   "middle"))
    b += legend(qx + qw + 22, qy + 60,
                [(c, n) for _v, c, n in series])
    b.append(label(qx + qw + 22, qy + 142, "three flat, one not —", "xs muted", "start"))
    b.append(label(qx + qw + 22, qy + 156, "and the odd one out is", "xs muted", "start"))
    b.append(label(qx + qw + 22, qy + 170, "not the one you would", "xs muted", "start"))
    b.append(label(qx + qw + 22, qy + 184, "guess.", "xs muted", "start"))

    b.append(label(20, 352, "A box and a hull are POLYTOPES: finitely many vertices, so "
                            "the simplex can only improve finitely often and the "
                            "tolerance is never", "xs", "start"))
    b.append(label(20, 368, "what stopped the search. The SPHERE is flat at ONE because "
                            "a ball's closest point to an outside point lies on the line "
                            "to its centre, so", "xs", "start"))
    b.append(label(20, 384, "the very first support call lands on the answer. Only the "
                            "CAPSULE converges, at about one extra pass per decade.",
                   "xs", "start"))

    return svg(uid, W, H,
               "GJK's iteration count: a histogram over 117,417 box pairs, and how the "
               "mean moves as the tolerance tightens for four shape pairs.",
               "Left, a horizontal bar chart of iterations from one to ten, peaking at "
               "three, with a mean of 3.52 and a maximum of 10. Right, four lines "
               "plotted against tolerance from 1e-2 to 1e-7: box, hull and sphere are "
               "flat, while capsule climbs from 2.06 to 7.43.",
               b)


# ===========================================================================
# Figure 10 — the resolution floor, and the margin  (§11)
# ===========================================================================
def fig10():
    uid = "l85f10"
    W, H = 900, 424
    b = [cmarker(uid, "green", GREEN), cmarker(uid, "red", RED)]

    b.append(label(20, 22, "Two different things give out, at two different gaps.",
                   "sm", "start"))

    # ---- left: the walk down --------------------------------------------
    px, py, pw, ph = 20, 52, 430, 282
    b += frame(px, py, pw, ph)
    b.append(label(px + 12, py + 18, "two turned 2 m cubes, walked together", "xs muted",
                   "start"))

    rows = []
    for gap, rep, slack, _stall, status in F5:
        rows.append((f"{gap:.0e}",
                     "—" if rep == 0.0 else f"{rep:.6e}",
                     "—" if rep == 0.0 else f"{slack:.1e}",
                     (status, "xs mono t-hi" if status == "intersecting" else "xs mono")))
    tbl, _ = table(px + 18, py + 34, ["gap (m)", "reported", "slack", "status"],
                   rows, [76, 122, 74, 104], row_h=17.0)
    b += tbl

    # ---- right: the margin scales ---------------------------------------
    qx, qy, qw, qh = 470, 52, 300, 282
    b += frame(qx, qy, qw, qh)
    b.append(label(qx + 12, qy + 18, "and the margin is RELATIVE", "xs muted", "start"))

    lo, hi = 0.1, 1000.0
    x0 = qx + 52
    span = qw - 84
    mlo, mhi = 1e-5, 1.0
    base = qy + qh - 44
    top = qy + 46

    def my(v):
        t = (math.log10(v) - math.log10(mlo)) / (math.log10(mhi) - math.log10(mlo))
        return base - t * (base - top)

    for e in range(-5, 1):
        y = my(10.0 ** e)
        b.append(rule(x0, y, x0 + span, y, "grid"))
        b.append(label(x0 - 8, y + 4, f"1e{e}", "xs mono muted", "end"))
    for s, _m, _r in F6:
        x = logspan(s, lo, hi, x0, span)
        b.append(label(x, base + 16, f"{s:g}", "xs mono muted", "middle"))

    pts = [(logspan(s, lo, hi, x0, span), my(m)) for s, m, _r in F6]
    b.append(poly(pts, RED, width=2.4, close=False))
    for (x, y) in pts:
        b.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3.0" fill="{RED}"/>')

    b.append(label(qx + qw / 2, base + 32, "cube side (m)", "xs muted", "middle"))
    b.append(label(qx + 14, qy + 38, "margin (m)", "xs muted", "start"))
    ratios = {r for _s, _m, r in F6}
    b.append(label(qx + qw - 14, qy + 38,
                   f"margin / (tol × side) = {min(ratios):.3f}…{max(ratios):.3f}",
                   "xs t-hi", "end"))

    b.append(label(20, 366, f"THE PROOF goes first, at about {F5_PROOF_FLOOR:.2e} m for "
                            f"these shapes: the termination threshold falls as the gap "
                            f"SQUARED while its", "xs", "start"))
    b.append(label(20, 384, "rounding error falls only as the gap, so below ε·|w|/tol "
                            "the test is noise against noise. The ANSWER is still right; "
                            "the certificate is gone.", "xs", "start"))
    b.append(label(20, 402, "THE ANSWER goes second, at tol × |w| — a contact margin, "
                            "and a deliberate one. 8.4 §11 found the same wall measured "
                            "from the WORLD ORIGIN.", "xs", "start"))

    return svg(uid, W, H,
               "GJK's resolution floor: the certificate fails before the answer does, "
               "and the contact margin scales with the shapes.",
               "Left, a table of two turned two-metre cubes walked from one metre apart "
               "to one micrometre, giving the reported distance, the certificate slack "
               "and the status; below three ten-thousandths of a metre the status "
               "becomes intersecting. Right, a log-log plot of the measured contact "
               "margin against cube side over four decades, a straight line whose ratio "
               "to tolerance times side is 2.11 throughout.",
               b)


# ===========================================================================
# Figure 11 — where the origin is  (§12)
# ===========================================================================
def fig11():
    uid = "l85f11"
    W, H = 900, 400
    b = [cmarker(uid, "green", GREEN), cmarker(uid, "red", RED)]

    b.append(label(20, 22, "One pair of boxes, one fixed gap, walked away from the "
                           "world origin.", "sm", "start"))

    px, py, pw, ph = 60, 54, 500, 250
    b += frame(px, py, pw, ph)

    lo, hi = 1e2, 1e6
    elo, ehi = 1e-9, 1e-1
    x0 = px + 56
    span = pw - 84
    base = py + ph - 42
    top = py + 40

    def ey(v):
        t = (math.log10(max(v, elo)) - math.log10(elo)) / (math.log10(ehi) - math.log10(elo))
        return base - min(max(t, 0.0), 1.0) * (base - top)

    for e in range(-9, 0):
        y = ey(10.0 ** e)
        b.append(rule(x0, y, x0 + span, y, "grid"))
        if e % 2:
            b.append(label(x0 - 8, y + 4, f"1e{e}", "xs mono muted", "end"))
    for dist, _u, _n, _r in H_MECH:
        x = logspan(dist, lo, hi, x0, span)
        b.append(label(x, base + 16, f"1e{int(round(math.log10(dist)))}",
                       "xs mono muted", "middle"))

    for col, idx, width, dash in ((GREY, 1, 1.6, "5 4"), (RED, 2, 2.4, None),
                                  (GREEN, 3, 2.4, None)):
        pts = [(logspan(r[0], lo, hi, x0, span), ey(r[idx])) for r in H_MECH]
        b.append(poly(pts, col, width=width, dash=dash, close=False))
        for (x, y) in pts:
            b.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="2.8" fill="{col}"/>')

    b.append(label(px + pw / 2, base + 34, "distance from the world origin (m)",
                   "xs muted", "middle"))
    b.append(label(px + 10, py + 22, "error in the reported distance (m)", "xs muted",
                   "start"))

    b += legend(px + pw + 26, py + 40, [
        (GREY, "ulp(position)"),
        (RED, "naive: WORLD support points"),
        (GREEN, "relative: support from each"),
        (None, "shape's own centre, one delta"),
    ])
    rows = [("worst, relative", f"{H_WORST_REL:.2e} m"),
            ("worst, naive", f"{H_WORST_NAIVE:.2e} m"),
            ("factor", f"{H_FACTOR:,}×")]
    tbl, _ = table(px + pw + 26, py + 132, ["", ""], rows, [130, 110], row_h=16.0)
    b += tbl

    b.append(label(20, 330, "A box's support point is `centre + axes·s`, three additions. "
                            "In WORLD space each rounds to half an ulp of the POSITION, "
                            "and there are two", "xs", "start"))
    b.append(label(20, 346, "support points per iteration. Taken RELATIVE they round to "
                            "half an ulp of the SHAPE, and the one world-sized "
                            "subtraction", "xs", "start"))
    b.append(label(20, 362, "`b.origin − a.origin` is EXACT whenever the two are within "
                            "a factor of two of each other — Sterbenz's lemma, and two "
                            "things about to", "xs", "start"))
    b.append(label(20, 378, "collide always are. The red line tracks ulp; the green one "
                            "tracks nothing at all.", "xs", "start"))

    return svg(uid, W, H,
               "The same two boxes walked from 100 m to 1000 km from the origin: the "
               "naive formulation's error tracks ulp(position), the relative one's does "
               "not move.",
               "A log-log chart of error in the reported distance against distance from "
               "the world origin. A dashed grey line shows one ulp of the position; a "
               "red line for the naive world-space formulation runs parallel to it, "
               "reaching 1.03e-2 m at 1000 km; a green line for the relative formulation "
               "stays flat near 1e-8 m throughout.",
               b)


# ===========================================================================
# Figure 12 — the budget  (§13)
# ===========================================================================
def fig12():
    uid = "l85f12"
    W, H = 900, 380
    b = []

    b.append(label(20, 22, "What it costs, and what the cost is made of.", "sm", "start"))

    # ---- left: per-call cost --------------------------------------------
    px, py, pw, ph = 20, 54, 420, 250
    b += frame(px, py, pw, ph)
    b.append(label(px + 12, py + 18, f"ns per pair, {I_PAIRS:,} box pairs", "xs muted",
                   "start"))

    bars_data = [("overlaps (SAT, bool)", I_OVERLAPS_NS, GREY),
                 ("collide (SAT, MTV)", I_SAT_NS, GREY),
                 ("gjk_intersects (bool)", I_GJK_BOOL_NS, BLUE),
                 ("gjk_distance (metres)", I_GJK_DIST_NS, AMBER)]
    peak = max(v for _n, v, _c in bars_data)
    row_h = 40
    x0 = px + 160
    width = pw - 210
    for i, (name, value, colour) in enumerate(bars_data):
        y = py + 46 + i * row_h
        w = (value / peak) * width
        b.append(label(px + 150, y + 13, name, "xs", "end"))
        b.append(f'<rect x="{x0}" y="{y}" width="{w:.1f}" height="18" fill="{colour}" '
                 f'fill-opacity="0.85"/>')
        b.append(label(x0 + w + 8, y + 13, f"{value:.1f}", "xs mono", "start"))
    b.append(label(px + pw / 2, py + ph - 16,
                   "GJK is slower on boxes, and it should be", "xs muted", "middle"))

    # ---- right: hull support scales linearly ----------------------------
    qx, qy, qw, qh = 464, 54, 300, 250
    b += frame(qx, qy, qw, qh)
    b.append(label(qx + 12, qy + 18, "a hull's support, by vertex count", "xs muted",
                   "start"))

    x0 = qx + 52
    span = qw - 80
    base = qy + qh - 44
    top = qy + 44
    vmax = max(v for _n, v, _g in I_HULL) * 1.1
    nmax = 128.0

    def hx(n):
        # LINEAR, deliberately. A log axis would space the doubling counts evenly
        # and bend a straight line into a curve, which is the opposite of what
        # this plot is claiming.
        return x0 + (n / nmax) * span

    def hy(v):
        return base - (v / vmax) * (base - top)

    for g in (0, 10, 20, 30, 40, 50):
        y = hy(g)
        b.append(rule(x0, y, x0 + span, y, "grid"))
        b.append(label(x0 - 8, y + 4, str(g), "xs mono muted", "end"))
    for n, _s, _g in I_HULL:
        if n == 16:
            continue        # crowds 8; the line's shape is the point, not the ticks
        b.append(label(hx(n), base + 16, str(n), "xs mono muted", "middle"))

    pts = [(hx(n), hy(s)) for n, s, _g in I_HULL]
    b.append(poly(pts, BLUE, width=2.4, close=False))
    for (x, y) in pts:
        b.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3.0" fill="{BLUE}"/>')

    b.append(label(qx + qw / 2, base + 32, "vertices", "xs muted", "middle"))
    b.append(label(qx + 14, qy + 38, "ns", "xs muted", "start"))
    b.append(label(qx + qw - 14, qy + 38, "linear in n", "xs t-hi", "end"))

    b.append(label(20, 326, f"And the function pointer costs nothing: "
                            f"{I_DIRECT_NS:.3f} ns direct against "
                            f"{I_INDIRECT_NS:.3f} ns through `convex::support`, with the "
                            f"two arms", "xs", "start"))
    b.append(label(20, 342, "agreeing bit for bit. One target per query is a branch the "
                            "predictor learns immediately — 6.17's finding, at a hotter "
                            "call site.", "xs", "start"))
    b.append(label(20, 362, "What GJK buys is the metres and the generality, not the "
                            "speed, which is why an engine keeps both tests.",
                   "xs t-hi", "start"))

    return svg(uid, W, H,
               "GJK's cost against the SAT's on boxes, and how a hull's support scales "
               "with its vertex count.",
               "Left, four horizontal bars: the SAT's boolean test at 8.4 ns, its full "
               "test at 49.5 ns, GJK's boolean at 73.2 ns and GJK's distance at 114.7 "
               "ns. Right, a line of support-function cost against hull vertex count "
               "from 8 to 128 vertices, straight, from 3.9 ns to 51.5 ns.",
               b)


def main():
    for n in range(1, 13):
        fn = globals()[f"fig{n}"]
        path = os.path.join(OUT, f"l85_fig{n}.svg")
        with open(path, "w") as fh:
            fh.write(fn())
        print(f"wrote {path}")


if __name__ == "__main__":
    main()
