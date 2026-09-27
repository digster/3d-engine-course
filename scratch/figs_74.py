#!/usr/bin/env python3
"""scratch/figs_74.py — Lesson 7.4's diagrams.

Same rules as 5.1-7.3's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - filenames numbered by PAGE ORDER (7.2 got two of nine wrong)
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - `rule()` takes a CSS CLASS; `cline()` takes a COLOUR
  - LEGENDS AND ANNOTATION BOXES GO OUTSIDE THE PLOT
  - a SHAPE can leave the viewBox where a label cannot
  - panel heights are COMPUTED, never guessed
  - the figure palette must contain the DEMO's own colours (rle_rects SNAPS)

THIS LESSON IS BACK IN SPACE after 7.3's holiday in the plane, so `View` returns
and with it the one honesty problem 7.3 did not have: an angle on the page is
not the angle in the maths. Every figure that shows an angle therefore also
PRINTS it, and the two mirror-plane figures are drawn with the rotation axis
pointing at the reader so that the 2 phi is at least measurable in the one plane
where it matters.

Every number below comes from verify_74's output (scratch/verify_74.log) or from
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
from figs_71 import (View, poly, frame, cmarker, carrow, D,         # noqa: E402
                     axis_arrow, circle_pts, apply, mul, rx, ry, rz)

OUT = "scratch"

# THE GIMBAL DEMO'S PALETTE, transcribed from demos/gimbal/main.cpp so that page
# and render agree. `rle_rects` SNAPS each sampled pixel to the nearest entry
# (by hue, then brightness — figs_45), so a colour the palette lacks comes out
# as whichever is nearest: 7.3's first draft rendered a light blue mirror grey
# and amber ticks khaki, and the figure then disagreed with its own caption.
TEAL = "#60ded0"          # k_axis   — the rotation axis, and the object's turn
GOLD = "#f8d678"          # k_rotor  — the half-angle object
GHOST_A = "#eca856"       # k_ghost_euler — the first order
GHOST_B = "#7ebcf8"       # k_ghost_slerp — the second order
MAGENTA = "#d676e2"       # k_dead   — the gap between them
DEMO_GREY = "#7884a8"     # k_trail  — the dial rim, and history
CRAFT_BODY = "#c8ccd8"    # the fuselage, lit
CRAFT_WING = "#7f9edb"    # the wings, lit
CRAFT_FIN = "#d98570"     # the fin, lit

DEMO_PALETTE = [hexrgb(c) for c in (TEAL, GOLD, GHOST_A, GHOST_B, MAGENTA,
                                    DEMO_GREY, CRAFT_BODY, CRAFT_WING,
                                    CRAFT_FIN, GREEN, RED, "#464e60")]


# ===========================================================================
# Measured — every figure's data, with the line of verify_74.log it came from
# ===========================================================================

# §F.1-F.3 — nanoseconds per operation. Best of three within a run, after a
# warm-up pass. Run-to-run spread under 4% on every row.
COMPOSE = [("quat x quat", 1.666, "16 mul, 12 add"),
           ("mat3 x mat3", 2.612, "27 mul, 18 add")]
APPLY = [("mat3 x vec3", 1.070, "9 mul, 6 add"),
         ("rotate(q, v)", 1.679, "18 mul, 12 add")]
CONVERT_NS = 4.619
CROSSOVER = 7.58

# §F.4 — max |MtM - I| after 1,000,000 compositions, three step sizes.
DRIFT = [  # (step deg, quat raw, mat3 raw, quat tidied every step)
    (0.37, 8.774e-02, 1.780e-02, 5.960e-08),
    (1.00, 2.940e-02, 2.674e-02, 1.192e-07),
    (2.50, 1.824e-01, 5.681e-02, 1.192e-07),
]
REPAIR = [("renormalised_fast", 1.059), ("Gram-Schmidt", 3.437)]

# §F.5 — renormalised_fast outside its domain. EXACT at 1 and at 2.
RENORM = [  # (|q| in, norm error after, pose error deg)
    (1.000, 5.960e-08, 0.0000),
    (1.050, 3.812e-03, 0.3349),
    (1.250, 1.016e-01, 9.0135),
    (1.500, 4.375e-01, 34.7161),
    (1.750, 9.453e-01, 49.8686),
    (2.000, 2.980e-07, 0.0000),
]

# §E.2 — round trip, degrees of pose error, Shepperd against the naive route.
EXTRACT = [  # (turn deg, shepperd, naive)
    (0.00, 2.544e-14, 2.544e-14),
    (1.00, 8.665e-08, 8.665e-08),
    (45.00, 3.443e-06, 3.443e-06),
    (90.00, 6.750e-07, 6.750e-07),
    (120.00, 1.207e-06, 4.978e-06),
    (170.00, 1.131e-05, 1.559e-04),
    (179.00, 2.386e-07, 2.321e-04),
    (179.99, 8.886e-06, 1.800e+02),
    (180.00, 8.939e-06, 1.800e+02),
]
PIVOTS = [("w", 11225), ("x", 2943), ("y", 2974), ("z", 2858)]
SMALLEST_PIVOT = 1.038415

# §H — what a quaternion cannot hold.
SCALE_ROWS = [
    ("uniform x2", "det 8 in", "det 1.6531 out", "|q| 1.3337", "28.15 deg off"),
    ("non-uniform", "0.4 on y", "pose moves", "1.2089 deg", "silently"),
    ("decomposed", "scale out", "then extract", "0.000e+00", "exact"),
    ("shear 0.30", "columns all 1", "invisible", "0.1500", "defeats both"),
]


# ===========================================================================
# Small shared bits
# ===========================================================================

def bars(x, top, pw, ph, rows, peak, unit, title, sub, colours):
    """A horizontal bar panel. 7.3's helper, with the fixed inset it earned."""
    out = [label(x + pw / 2, top - 30, title, cls="sm"),
           label(x + pw / 2, top - 14, sub, cls="xs muted"),
           frame(x, top, pw, ph)]
    slot = ph / len(rows)
    for i, (name, value) in enumerate(rows):
        y = top + i * slot + 20.0
        hgt = slot * 0.34
        width = (pw - 78) * (value / peak)
        out.append(box(x + 8, y, max(width, 1.5), hgt, colours[i], opacity=0.85,
                       width=1.0, rx=2))
        out.append(label(x + 8, y - 7, name, cls="xs muted", anchor="start"))
        out.append(label(x + 12 + max(width, 1.5), y + hgt - 3, unit(value),
                         cls="xs mono", anchor="start"))
    return out


def render_panel(ppm, crop, x, y, px, cell=3, levels=3):
    """A real render, downsampled by PEAK and run-length encoded to rectangles.

    `peak_sample` and not `box_sample`: averaging a one-pixel debug line inside
    a 3x3 block costs it eight ninths of its brightness, and these two renders
    are almost entirely one-pixel debug lines. Returns the list of elements and
    the panel's size in page units, because a figure's height must be COMPUTED
    from both columns and never guessed.
    """
    _w, _h, data = read_ppm(os.path.join(OUT, ppm))
    grid_w, grid_h, grid = peak_sample(data, _w, crop, cell)
    body = rle_rects(grid, grid_w, grid_h, x, y, px, DEMO_PALETTE, levels=levels,
                     bg_class="fill-shot")
    return body, grid_w * px, grid_h * px


# ===========================================================================
# Figure 1 — isotropy forces the table  (§3)
# ===========================================================================
# ONE CLAIM: nothing about i, j, k was chosen. The single demand is that space
# has no preferred direction, so EVERY unit imaginary is a half-turn; three
# lines of algebra then fix every entry of the table including ijk = -1.
def fig_forced():
    uid = "f74a"
    W, H = 760, 400
    b = [cmarker(uid, "gold", GOLD), cmarker(uid, "teal", TEAL),
         cmarker(uid, "kblue", BLUE)]

    # ---- left: the demand, drawn ------------------------------------------
    view = View(178, 168, 74, az=34, el=22)
    b.append(label(178, 42, "the one demand", cls="sm"))
    b.append(label(178, 58, "space has no preferred direction", cls="xs muted"))

    # x/y/z = red/green/blue (Conventions §10). `arrow`'s kinds cover red ("b")
    # and green ("g") and there is NO blue one — figs_71's `cmarker` exists for
    # exactly this, and an arrow asking for a marker that does not exist renders
    # as a headless line, indistinguishable from a leader.
    for direction, kind, name in (((1, 0, 0), "b", "i"), ((0, 1, 0), "g", "j")):
        b.append(axis_arrow(view, uid, (0, 0, 0), direction, 1.0, kind))
        tip = view(tuple(c * 1.22 for c in direction))
        b.append(label(tip[0], tip[1] + 4, name, cls="xs mono"))
    b.append(carrow(*view((0, 0, 0)), *view((0, 0, 1.0)), uid, "kblue", BLUE, width=1.6))
    kt = view((0, 0, 1.22))
    b.append(label(kt[0], kt[1] + 4, "k", cls="xs mono"))

    # a generic unit imaginary, which must behave exactly like the three
    u = (0.52, 0.66, 0.54)
    ul = math.sqrt(sum(c * c for c in u))
    u = tuple(c / ul for c in u)
    b.append(carrow(*view((0, 0, 0)), *view(tuple(c * 1.0 for c in u)),
                    uid, "gold", GOLD, width=2.0))
    ut = view(tuple(c * 1.19 for c in u))
    b.append(label(ut[0] + 10, ut[1], "u", cls="xs mono"))
    b.append(poly([view(p) for p in circle_pts_raw(u, 0.94)], DEMO_GREY,
                  width=1.0, dash="3 3", close=True))
    b.append(label(178, 266, "u² = −1  for EVERY unit u", cls="sm t-hi"))
    b.append(label(178, 282, "measured over 20,000 axes: 2.980e−07", cls="xs mono muted"))

    # ---- middle: the three lines ------------------------------------------
    b.append(rule(346, 60, 346, 330, cls="grid"))
    lines = [
        "u = (i + j)/√2 has |u| = 1,",
        "so the demand gives u² = −1.",
        "",
        "Therefore (i + j)² = 2u² = −2.",
        "",
        "Now expand the left side:",
        "   i² + ij + ji + j²",
        "   = −2 + (ij + ji)",
        "",
        "Match the two:",
        "   ⇒  ij + ji = 0,   ji = −ij.",
    ]
    for n, text in enumerate(lines):
        b.append(label(372, 82 + n * 19, text, cls="xs mono", anchor="start"))
    b.append(label(372, 300, "ANTICOMMUTATIVITY IS NOT ASSUMED.", cls="xs t-hi",
                   anchor="start"))
    b.append(label(372, 316, "It is what isotropy costs.", cls="xs muted", anchor="start"))

    # ---- right: the table it forces ---------------------------------------
    b.append(rule(600, 60, 600, 330, cls="grid"))
    cols = ["1", "i", "j", "k"]
    table = [["1", "i", "j", "k"],
             ["i", "−1", "k", "−j"],
             ["j", "−k", "−1", "i"],
             ["k", "j", "−i", "−1"]]
    tx, ty, cw, ch = 632, 96, 28, 26
    b.append(label(698, 58, "the table, forced", cls="sm"))
    b.append(label(698, 74, "from associativity alone", cls="xs muted"))
    for c, name in enumerate(cols):
        b.append(label(tx + 30 + c * cw, ty - 6, name, cls="xs mono t-hi"))
        b.append(label(tx, ty + 18 + c * ch, name, cls="xs mono t-hi", anchor="start"))
    for r in range(4):
        for c in range(4):
            entry = table[r][c]
            hi = "xs mono t-hi" if (r, c) in ((1, 2), (2, 1)) else "xs mono"
            b.append(label(tx + 30 + c * cw, ty + 18 + r * ch, entry, cls=hi))
    b.append(rule(tx + 16, ty + 4, tx + 16 + 4 * cw, ty + 4, cls="grid"))
    b.append(rule(tx + 16, ty + 4, tx + 16, ty + 4 + 4 * ch, cls="grid"))
    b.append(label(700, 240, "ij = k   ji = −k", cls="xs mono t-hi"))
    b.append(label(698, 262, "ijk = −1", cls="sm t-hi"))
    b.append(label(698, 278, "Hamilton's bridge formula,", cls="xs muted"))
    b.append(label(698, 292, "here a consequence", cls="xs muted"))

    b.append(label(W / 2, 358, "Nothing on this page was postulated except that a "
                   "direction of space is like any other direction of space.",
                   cls="xs muted"))
    b.append(label(W / 2, 376, "The fourth component was already forced in Lesson 7.3 "
                   "§12; this is what its arithmetic has to be.", cls="xs muted"))

    return svg(uid, W, H, "Isotropy forces the multiplication table",
               "Three panels. On the left, the three coordinate axes drawn as red, green and "
               "blue arrows with a fourth gold arrow labelled u pointing in a generic "
               "direction, and the statement that u squared is minus one for every unit u, "
               "measured over twenty thousand axes at three times ten to the minus seven. In "
               "the middle, three lines of algebra: expanding i plus j squared gives minus two "
               "plus the quantity ij plus ji, and demanding that the unit imaginary square to "
               "minus one forces ij plus ji to be zero. On the right, the four by four "
               "multiplication table with ij equals k and ji equals minus k highlighted, and "
               "Hamilton's ijk equals minus one below it as a consequence rather than a "
               "definition.", b)


def circle_pts_raw(axis, radius, n=72):
    """A ring perpendicular to `axis`, in world coordinates."""
    a = axis
    helper = (0.0, 0.0, 1.0) if abs(a[2]) < 0.9 else (1.0, 0.0, 0.0)
    u = cross3(a, helper)
    u = norm3(u)
    v = cross3(a, u)
    return [tuple((u[k] * math.cos(2 * math.pi * i / n) +
                   v[k] * math.sin(2 * math.pi * i / n)) * radius for k in range(3))
            for i in range(n + 1)]


def cross3(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0])


def norm3(v):
    l = math.sqrt(sum(c * c for c in v))
    return tuple(c / l for c in v)


# ===========================================================================
# Figure 2 — the product, and the two vector operations inside it  (§4)
# ===========================================================================
def fig_product():
    uid = "f74b"
    W, H = 760, 414
    b = []

    b.append(label(W / 2, 40, "one multiplication, two famous halves", cls="sm"))

    # the expansion, laid out so the cross term is visibly the only one that moves
    rows = [
        ("(w₁ + v₁)(w₂ + v₂)", "=", "w₁w₂  +  w₁v₂  +  v₁w₂  +  v₁v₂", None),
        ("", "", "", None),
        ("v₁v₂", "=", "−(v₁ · v₂)  +  (v₁ × v₂)", "hi"),
        ("", "", "", None),
        ("q p", "=", "( w₁w₂ − v₁·v₂ ,   w₁v₂ + w₂v₁ + v₁×v₂ )", None),
    ]
    y = 78
    for lhs, eq, rhs, kind in rows:
        if lhs:
            b.append(label(236, y, lhs, cls="sm mono", anchor="end"))
            b.append(label(248, y, eq, cls="sm mono", anchor="middle"))
            b.append(label(262, y, rhs, cls="sm mono" + (" t-hi" if kind else ""),
                           anchor="start"))
        y += 26

    # BELOW THE LAST EQUATION ROW, NOT BESIDE IT. The rows run y = 78 to 182
    # in steps of 26; the first draft put these two notes at 156 and 172,
    # which is inside that range. check-page.js §4c reported it as a text
    # overlap, correctly — a line of prose through an equation reads as a
    # strikethrough, and the estimate-from-character-count check in this file
    # cannot see it because both boxes are individually fine.
    b.append(label(262, 206, "the dot product and the cross product are INSIDE the",
                   cls="xs muted", anchor="start"))
    b.append(label(262, 222, "quaternion product. Gibbs and Heaviside took them out.",
                   cls="xs muted", anchor="start"))

    # the swap
    b.append(rule(40, 248, 720, 248, cls="grid"))
    b.append(label(40, 272, "swap the operands and exactly one term changes sign:",
                   cls="xs muted", anchor="start"))
    b.append(label(40, 298, "q p − p q  =  ( 0 ,  2 (v₁ × v₂) )",
                   cls="sm mono t-hi", anchor="start"))

    b.append(label(430, 272, "worked, by hand:", cls="xs muted", anchor="start"))
    b.append(label(430, 294, "q = (2, 1, −1, 3)   p = (−1, 2, 0, 1)",
                   cls="xs mono", anchor="start"))
    b.append(label(430, 312, "q p = (−7,  2,  6,  1)", cls="xs mono", anchor="start"))
    b.append(label(430, 328, "p q = (−7,  4, −4, −3)", cls="xs mono", anchor="start"))
    b.append(label(430, 346, "difference = (0, −2, 10, 4) = 2 v₁×v₂",
                   cls="xs mono t-hi", anchor="start"))

    b.append(label(W / 2, 382, "Three sentences, one fact: quaternions do not commute, "
                   "the cross product is antisymmetric, and", cls="xs muted"))
    b.append(label(W / 2, 400, "rotations of space do not commute. The price the fourth "
                   "dimension charges is the cross product's sign.", cls="xs muted"))

    return svg(uid, W, H, "The quaternion product contains the dot and cross products",
               "The expansion of the quaternion product written out in three stages. First, "
               "distributing w1 plus v1 times w2 plus v2 into four terms. Second, the product "
               "of two pure quaternions equals minus their dot product plus their cross "
               "product, highlighted. Third, the collected result. Below, the difference "
               "between the two orders is shown to be exactly twice the cross product, with a "
               "worked example using the quaternions two one minus one three and minus one two "
               "zero one giving minus seven two six one and minus seven four minus four minus "
               "three, whose difference is zero minus two ten four.", b)


# ===========================================================================
# Figure 3 — two mirror planes, and where cos(theta/2) comes from  (§5)
# ===========================================================================
def fig_mirrors():
    uid = "f74c"
    W, H = 760, 420
    b = [cmarker(uid, "gold", GOLD), cmarker(uid, "teal", TEAL),
         cmarker(uid, "mag", MAGENTA)]

    # The two mirror planes both contain +z, which is therefore the axis. Drawn
    # with +z toward the reader so the 2 phi is measurable on the page.
    view = View(250, 216, 88, az=16, el=64)
    phi = 30.0

    b.append(label(250, 44, "two mirrors, 30° apart", cls="sm"))
    b.append(label(250, 60, "both planes contain the axis", cls="xs muted"))

    def plane(angle_deg, colour, name):
        """A mirror plane as a quad, plus its normal."""
        a = angle_deg * D
        inplane = (math.cos(a), math.sin(a), 0.0)
        out = []
        corners = [tuple(inplane[k] * s + (0, 0, 1)[k] * t for k in range(3))
                   for s, t in ((-1.15, -0.9), (1.15, -0.9), (1.15, 0.9), (-1.15, 0.9))]
        out.append(poly([view(c) for c in corners], colour, width=1.2, close=True))
        normal = (-math.sin(a), math.cos(a), 0.0)
        out.append(carrow(*view((0, 0, 0)), *view(tuple(c * 1.02 for c in normal)),
                          uid, "mag" if name == "n₀" else "gold",
                          MAGENTA if name == "n₀" else GOLD, width=1.7))
        tip = view(tuple(c * 1.22 for c in normal))
        out.append(label(tip[0], tip[1], name, cls="xs mono"))
        return out

    b.extend(plane(0.0, DEMO_GREY, "n₀"))
    b.extend(plane(phi, GHOST_B, "n₁"))

    # the axis: where the planes meet
    # SHORTER THAN IT LOOKS LIKE IT SHOULD BE, and the reason is projection. At
    # el = 64 the +z axis runs DOWN and to the left on the page, so an arrow out
    # to z = 1.35 put its label at y = 331 — directly on the caption below the
    # panel. The planes only span z in [-0.9, 0.9] anyway.
    b.append(carrow(*view((0, 0, -0.32)), *view((0, 0, 0.98)), uid, "teal", TEAL,
                    width=2.0))
    at = view((0, 0, 1.06))
    b.append(label(at[0] - 28, at[1] + 4, "n₀ × n₁", cls="xs mono"))

    # a probe, reflected once and twice, in the plane z = 0
    probe = (math.cos(-25 * D), math.sin(-25 * D), 0.0)

    def reflect(v, a_deg):
        a = a_deg * D
        n = (-math.sin(a), math.cos(a), 0.0)
        d = sum(v[k] * n[k] for k in range(3))
        return tuple(v[k] - 2 * d * n[k] for k in range(3))

    once = reflect(probe, 0.0)
    twice = reflect(once, phi)
    for v, colour, name in ((probe, "#e2e6ee", "v"), (once, DEMO_GREY, "v′"),
                            (twice, TEAL, "v″")):
        b.append(poly([view((0, 0, 0)), view(tuple(c * 1.28 for c in v))], colour,
                      width=2.0, close=False))
        tip = view(tuple(c * 1.44 for c in v))
        b.append(label(tip[0], tip[1], name, cls="xs mono"))

    # ---- right: the algebra ------------------------------------------------
    b.append(rule(470, 62, 470, 352, cls="grid"))
    lines = [
        ("reflect in the plane ⊥ n:", None),
        ("    v ↦ n v n", "hi"),
        ("", None),
        ("do it twice, in n₀ then n₁:", None),
        ("    v ↦ n₁ (n₀ v n₀) n₁", None),
        ("        = (n₁n₀) v (n₀n₁)", None),
        ("", None),
        ("and n₀n₁ = conj(n₁n₀), so", None),
        ("    v ↦ q v conj(q),  q = n₁n₀", "hi"),
        ("", None),
        ("q = −(n₁·n₀) + n₁×n₀", None),
        ("  = −( cos φ + sin φ · n̂ )", None),
        ("", None),
        ("negate — legal, §8 — and θ = 2φ:", None),
        ("    q = cos(θ/2) + sin(θ/2) n̂", "hi"),
    ]
    y = 84
    for text, kind in lines:
        b.append(label(496, y, text, cls="xs mono" + (" t-hi" if kind else ""),
                       anchor="start"))
        y += 18

    b.append(label(250, 330, "30° of mirror → 60° of turn", cls="sm t-hi"))
    b.append(label(250, 348, "rotor = (−0.866025, 0, 0, −0.5)", cls="xs mono muted"))
    b.append(label(250, 364, "|w| = cos 30° exactly (§C.3)", cls="xs mono muted"))

    b.append(label(W / 2, 396, "The sandwich was not chosen. It fell out of composing two "
                   "reflections — and so did the half-angle, and so did the sign", cls="xs muted"))
    b.append(label(W / 2, 412, "nobody could pin down, which is the double cover arriving "
                   "before anyone had named it.", cls="xs muted"))

    return svg(uid, W, H, "Two mirror planes generate a rotation of twice their angle",
               "On the left, two mirror planes drawn as quadrilaterals meeting along a teal "
               "axis arrow labelled n0 cross n1, with their normals drawn as magenta and gold "
               "arrows thirty degrees apart. A pale probe vector is reflected once to a grey "
               "vector and twice to a teal vector, sixty degrees from where it started. On the "
               "right, the algebra in five steps: reflection in the plane perpendicular to n "
               "is n v n, doing it twice gives n1 n0 times v times n0 n1, the right factor is "
               "the conjugate of the left, so the composite is q v conjugate q with q equal to "
               "n1 n0, and working q out gives minus cosine phi minus sine phi times the unit "
               "axis, which negated and with theta equal to two phi is cosine of theta over "
               "two plus sine of theta over two times the axis.", b)


# ===========================================================================
# Figure 4 — the two orders, in the demo  (§7)  — A REAL RENDER
# ===========================================================================
def fig_commute_render():
    uid = "f74d"
    # Crop measured from the render's own content, not chosen by eye: the bright
    # pixels span (284, 99) to (580, 427), so this is that box with a 22-pixel
    # margin, rounded to a multiple of the cell.
    CROP = (262, 78, 604, 450)
    PAD, CAP, PX = 18.0, 46.0, 2.34
    panel, PW, PH = render_panel("l74_commute90.ppm", CROP, PAD, CAP, PX)

    W = 760.0
    NOTES_H = CAP + 30.0 + 9 * 20.0 + 70.0
    H = max(CAP + PH + 44.0, NOTES_H) + 34.0

    b = list(panel)
    b.insert(0, label(PAD + PW / 2, 24, "./build/demos/gimbal --commute 90", cls="sm mono"))
    b.append(label(PAD + PW / 2, CAP + PH + 20,
                   "amber: pitch, then yaw  \u00b7  blue: yaw, then pitch", cls="xs muted"))
    b.append(label(PAD + PW / 2, CAP + PH + 36,
                   "magenta: the gap the two journeys leave open", cls="xs muted"))

    NX = PAD + PW + 34.0
    b.append(rule(NX - 18, CAP - 10, NX - 18, CAP + PH + 10, cls="grid"))
    b.append(label(NX, CAP + 6, "the program's own receipt", cls="sm", anchor="start"))
    # QUOTED VERBATIM from the program, not paraphrased. 7.3 narrowed its
    # printfs and then left fourteen blocks quoting output the program no
    # longer produced, which is worse than the overflow that prompted the
    # narrowing: STALE IS WORSE THAN WIDE.
    rows = [
        ("gimbal: commute 90.00 deg", None),
        ("  -> gap 120.0000 deg", "hi"),
        ("gimbal:   matrix 120.0000,", None),
        ("          closed form 120.0000", None),
        ("", None),
        ("c = cos 45\u00b0,  s = sin 45\u00b0", None),
        ("dot = c\u2074 + 2c\u00b2s\u00b2 \u2212 s\u2074 = 1/2", None),
        ("gap = 2 acos(1/2) = 120\u00b0", "hi"),
    ]
    y = CAP + 34
    for text, kind in rows:
        b.append(label(NX, y, text, cls="xs mono" + (" t-hi" if kind else ""),
                       anchor="start"))
        y += 20
    b.append(label(NX, y + 16, "Three routes, one number.", cls="xs t-hi", anchor="start"))
    b.append(label(NX, y + 34, "The closed form shares no code", cls="xs muted", anchor="start"))
    b.append(label(NX, y + 48, "with either measurement.", cls="xs muted", anchor="start"))

    b.append(label(W / 2, H - 14, "The four arcs are the same two turns in the two orders, "
                   "and they fail to close. That failure IS the cross product.",
                   cls="xs muted"))

    return svg(uid, int(W), int(H), "The same two turns, performed in both orders",
               "A real render from the gimbal demo. Two coloured journeys leave the same "
               "starting nose: the amber one pitches ninety degrees and then yaws ninety, the "
               "blue one yaws first and then pitches. They arrive at two different places, and "
               "a magenta arc joins the two arrivals, closing a quadrilateral that the two "
               "journeys leave open. Beside it, the program's printed receipt: the gap measured "
               "from the quaternions is one hundred and twenty degrees exactly, the same gap "
               "measured from the two matrices is one hundred and twenty, and the closed form "
               "two arccosine of c to the fourth plus two c squared s squared minus s to the "
               "fourth is also one hundred and twenty.", b)


# ===========================================================================
# Figure 5 — the double cover, in the demo  (§8)  — A REAL RENDER
# ===========================================================================
def fig_cover_render():
    uid = "f74e"
    # Content spans (235, 41) to (720, 409); this is that box plus a margin.
    CROP = (214, 22, 742, 430)
    PAD, CAP, PX = 18.0, 46.0, 1.78
    panel, PW, PH = render_panel("l74_cover360.ppm", CROP, PAD, CAP, PX)

    W = 760.0
    NX = PAD + PW + 30.0
    NOTES_H = CAP + 14 * 20.0
    H = max(CAP + PH + 46.0, NOTES_H) + 34.0

    b = list(panel)
    b.insert(0, label(PAD + PW / 2, 24, "./build/demos/gimbal --cover 360", cls="sm mono"))
    b.append(label(PAD + PW / 2, CAP + PH + 20,
                   "dial: w on the horizontal, v\u00b7n\u0302 on the vertical", cls="xs muted"))
    b.append(label(PAD + PW / 2, CAP + PH + 36,
                   "the craft is home; its quaternion is at \u22121", cls="xs muted"))

    b.append(rule(NX - 16, CAP - 10, NX - 16, CAP + PH + 10, cls="grid"))
    notes = [
        ("teal hand", "hi"),
        ("the craft's turn,", None),
        ("360\u00b0 \u2261 0\u00b0", "mono"),
        ("", None),
        ("gold hand", "hi"),
        ("the quaternion's,", None),
        ("at 180\u00b0", "mono"),
        ("", None),
        ("cover 360.00 deg", "mono"),
        ("  -> pose 0.0000 deg", "mono"),
        ("  q (\u22121.00000, \u22120, \u22120, \u22120)", "mono"),
        ("", None),
        ("720\u00b0 brings both", None),
        ("home together.", None),
    ]
    y = CAP + 16
    for text, kind in notes:
        cls = "xs muted"
        if kind == "hi":
            cls = "xs t-hi"
        elif kind == "mono":
            cls = "xs mono"
        b.append(label(NX, y, text, cls=cls, anchor="start"))
        y += 20

    b.append(label(W / 2, H - 14, "A rotation of 360\u00b0 is not the identity to a quaternion. "
                   "It takes two laps to bring both hands home.", cls="xs muted"))

    return svg(uid, int(W), int(H), "The double cover, on a dial",
               "A real render from the gimbal demo. On the right, an aircraft that has turned "
               "exactly three hundred and sixty degrees about a tilted axis and is therefore "
               "back where it started, with the teal rotation axis running through it. On the "
               "left, a dial whose horizontal coordinate is the quaternion's real part and "
               "whose vertical coordinate is its vector part along the axis. The teal hand, "
               "which tracks the craft, points right at zero degrees; the gold hand, which "
               "tracks the quaternion, points left at one hundred and eighty degrees, at the "
               "mark for w equals minus one. The printed receipt says q equals minus one zero "
               "zero zero and the pose is zero point zero degrees from the start.", b)


# ===========================================================================
# Figure 6 — extraction: four candidates, no bad case  (§9)
# ===========================================================================
def fig_extract():
    uid = "f74f"
    W, H = 760, 404
    b = []

    # ---- left: the four candidates ----------------------------------------
    PW, PH = 300.0, 176.0
    TOP = 84.0
    b.append(label(180, TOP - 30, "which component the pivot picked", cls="sm"))
    b.append(label(180, TOP - 14, "20,000 random rotations", cls="xs muted"))
    total = sum(n for _, n in PIVOTS)
    b.extend(bars(30, TOP, PW, PH, [(n, c) for n, c in PIVOTS], max(c for _, c in PIVOTS),
                  lambda v: f"{100.0 * v / total:.1f}%", "", "",
                  [GOLD, RED, GREEN, BLUE])[2:])
    b.append(frame(30, TOP, PW, PH))

    b.append(label(180, 292, "4w² + 4x² + 4y² + 4z² = 4, always", cls="sm mono t-hi"))
    b.append(label(180, 312, "so the largest is ≥ 1 and the divisor ≥ 2.", cls="xs muted"))
    b.append(label(180, 328, f"smallest pivot seen in 20,000: {SMALLEST_PIVOT:.6f}",
                   cls="xs mono muted"))
    b.append(label(180, 344, "There is no bad case. There is no threshold.", cls="xs t-hi"))

    # ---- right: the round trip, log scale ---------------------------------
    PX, PY, PPW, PPH = 430.0, TOP, 300.0, 176.0
    b.append(label(PX + PPW / 2, TOP - 30, "round trip, pose error", cls="sm"))
    b.append(label(PX + PPW / 2, TOP - 14, "matrix → quat → matrix, degrees", cls="xs muted"))
    b.append(frame(PX, PY, PPW, PPH))

    lo, hi = -14.0, 2.5

    def px(turn):
        return PX + 14 + (PPW - 28) * (turn / 180.0)

    def py(value):
        e = math.log10(max(value, 1e-14))
        return PY + PPH - 12 - (PPH - 26) * (e - lo) / (hi - lo)

    for e in (-12, -8, -4, 0):
        yy = py(10.0 ** e)
        b.append(rule(PX + 4, yy, PX + PPW - 4, yy, cls="grid", dash="2 4"))
        b.append(label(PX - 6, yy + 3, f"1e{e}", cls="xs mono muted", anchor="end"))

    for idx, colour, name in ((1, TEAL, "Shepperd"), (2, AMBER, "naive")):
        pts = [(px(t), py(row[idx])) for row in EXTRACT for t in (row[0],)]
        b.append(poly(pts, colour, width=1.6, close=False))
        for x, y in pts:
            b.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="2.4" fill="{colour}"/>')

    b.append(label(px(0) + 4, PY + PPH + 14, "0°", cls="xs mono muted", anchor="start"))
    b.append(label(px(180) - 4, PY + PPH + 14, "180°", cls="xs mono muted", anchor="end"))
    b.append(label(PX + 20, PY + 22, "naive: 180.0° of error at 179.99°",
                   cls="xs mono", anchor="start"))
    b.append(label(PX + 20, PY + PPH - 20, "Shepperd: never worse than 1.1e−05°",
                   cls="xs mono", anchor="start"))

    b.append(label(PX + PPW / 2, 292, "the naive route divides by 4w", cls="sm t-hi"))
    b.append(label(PX + PPW / 2, 312, "w = cos(θ/2), so at a half-turn it divides by nothing",
                   cls="xs muted"))
    b.append(label(PX + PPW / 2, 328, "and at exactly 180° it returns the identity.",
                   cls="xs muted"))
    b.append(label(PX + PPW / 2, 344, "Lesson 7.2 needed two routes and a crossover at 120°.",
                   cls="xs muted"))

    b.append(label(W / 2, 382, "Four candidates instead of three is what removes the bad case "
                   "— and four candidates is what having a", cls="xs muted"))
    b.append(label(W / 2, 398, "fourth component means. The representation with no singularity "
                   "has the unconditional extraction.", cls="xs muted"))

    return svg(uid, W, H, "Shepperd's four candidates, and why there is no bad case",
               "On the left, a bar chart of which of the four components the pivot chose over "
               "twenty thousand random rotations: w fifty-six percent, and x, y and z about "
               "fifteen percent each. Below it, the identity that the four candidates sum to "
               "four, so the largest is at least one; the smallest pivot seen was 1.038. On the "
               "right, a log-scale plot of round-trip pose error against turn angle for two "
               "routes. Shepperd's stays below one hundredth of a millionth of a degree across "
               "the whole range. The naive route tracks it up to about one hundred and twenty "
               "degrees and then diverges, reaching one hundred and eighty degrees of error at "
               "a turn of 179.99 degrees.", b)


# ===========================================================================
# Figure 7 — what it costs  (§10)
# ===========================================================================
def fig_cost():
    uid = "f74g"
    W, H = 760, 404
    b = []

    PW, PH, TOP = 210.0, 150.0, 76.0

    peak = max(v for _, v, _ in COMPOSE)
    b.extend(bars(30, TOP, PW, PH, [(n, v) for n, v, _ in COMPOSE], peak,
                  lambda v: f"{v:.3f} ns", "composing two rotations",
                  "16,384,000 operations, best of three", [TEAL, BLUE]))

    peak2 = max(v for _, v, _ in APPLY)
    b.extend(bars(276, TOP, PW, PH, [(n, v) for n, v, _ in APPLY], peak2,
                  lambda v: f"{v:.3f} ns", "applying one to a point",
                  "the row where the quaternion LOSES", [BLUE, TEAL]))

    peak3 = max(v for _, v in REPAIR)
    b.extend(bars(522, TOP, PW, PH, REPAIR, peak3, lambda v: f"{v:.3f} ns",
                  "repairing a drifted one", "one Taylor step against Gram-Schmidt",
                  [TEAL, BLUE]))

    b.append(label(135, 252, "1.57× cheaper", cls="xs t-hi"))
    b.append(label(381, 252, "1.57× dearer", cls="xs t-hi"))
    b.append(label(627, 252, "3.25× cheaper", cls="xs t-hi"))

    # the crossover, which is the number to design with
    b.append(rule(40, 276, 720, 276, cls="grid"))
    b.append(label(W / 2, 302, f"mat3_from_quat costs {CONVERT_NS:.3f} ns, and "
                   f"each vector saves {APPLY[1][1] - APPLY[0][1]:.3f} ns", cls="sm"))
    b.append(label(W / 2, 326, f"⇒  build the matrix once you are rotating more than "
                   f"{CROSSOVER:.1f} vectors by the same quaternion", cls="sm t-hi"))
    b.append(label(W / 2, 352, "Rotate one point, use the sandwich. Rotate a mesh, or a "
                   "skeleton's worth of vertices, build the matrix.", cls="xs muted"))
    b.append(label(W / 2, 370, "That single number is why every renderer that is handed "
                   "quaternions converts them before it draws — and why", cls="xs muted"))
    b.append(label(W / 2, 388, "`mat3_from_quat` is not an afterthought in this header. "
                   "Storage: 16 bytes against 36.", cls="xs muted"))

    return svg(uid, W, H, "What a quaternion costs, measured",
               "Three bar charts. Composing: a quaternion product at 1.666 nanoseconds against "
               "a three-by-three matrix product at 2.612, so 1.57 times cheaper. Applying to a "
               "point: a matrix times a vector at 1.070 against the quaternion sandwich at "
               "1.679, so 1.57 times dearer. Repairing a drifted rotation: one fast "
               "renormalisation step at 1.059 against Gram-Schmidt at 3.437, so 3.25 times "
               "cheaper. Below, the crossover: converting to a matrix costs 4.619 nanoseconds "
               "and each vector rotated saves 0.609, so build the matrix once you are rotating "
               "more than 7.6 vectors by the same quaternion.", b)


# ===========================================================================
# Figure 8 — drift, and the renormalise that is exact at both ends  (§10.4)
# ===========================================================================
def fig_drift():
    uid = "f74h"
    W, H = 760, 400
    b = []

    # ---- left: drift table -------------------------------------------------
    b.append(label(196, 44, "after 1,000,000 compositions", cls="sm"))
    b.append(label(196, 60, "max |MᵀM − I| — both asked the SAME question",
                   cls="xs muted"))

    cols = [("step", 64), ("quat raw", 150), ("mat3 raw", 248), ("tidied", 336)]
    for name, x in cols:
        b.append(label(x, 92, name, cls="xs muted", anchor="end"))
    b.append(rule(30, 100, 350, 100, cls="grid"))
    y = 122
    for step, q_raw, m_raw, tidy in DRIFT:
        b.append(label(64, y, f"{step:.2f}°", cls="xs mono", anchor="end"))
        b.append(label(150, y, f"{q_raw:.3e}", cls="xs mono", anchor="end"))
        b.append(label(248, y, f"{m_raw:.3e}", cls="xs mono", anchor="end"))
        b.append(label(336, y, f"{tidy:.3e}", cls="xs mono t-hi", anchor="end"))
        y += 24
    b.append(rule(30, y - 14, 350, y - 14, cls="grid"))

    b.append(label(196, 214, "THE RAW QUATERNION LOSES.", cls="sm t-hi"))
    b.append(label(196, 232, "In the plane (7.3) the complex number won by 6.15×.",
                   cls="xs muted"))
    b.append(label(196, 248, "Here the answer depends on the STEP, because an", cls="xs muted"))
    b.append(label(196, 264, "unrepaired norm error compounds as |q|ⁿ and the",
                   cls="xs muted"))
    b.append(label(196, 280, "sandwich then squares it. One `renormalised_fast`", cls="xs muted"))
    b.append(label(196, 296, "per step — no sqrt, no divide — ends the argument.",
                   cls="xs muted"))

    # ---- right: renormalised_fast outside its domain ----------------------
    b.append(rule(392, 60, 392, 330, cls="grid"))
    b.append(label(586, 44, "renormalised_fast outside its domain", cls="sm"))
    b.append(label(586, 60, "the same function 7.3 warned about", cls="xs muted"))

    for name, x in (("|q| in", 470), ("norm err", 584), ("pose err", 700)):
        b.append(label(x, 92, name, cls="xs muted", anchor="end"))
    b.append(rule(420, 100, 710, 100, cls="grid"))
    y = 122
    for q_in, norm_err, pose in RENORM:
        exact = pose < 1e-3
        cls = "xs mono t-hi" if exact else "xs mono"
        b.append(label(470, y, f"{q_in:.3f}", cls=cls, anchor="end"))
        b.append(label(584, y, f"{norm_err:.3e}", cls=cls, anchor="end"))
        b.append(label(700, y, f"{pose:.4f}°", cls=cls, anchor="end"))
        y += 24
    b.append(rule(420, y - 14, 710, y - 14, cls="grid"))

    b.append(label(586, 292, "EXACT at 1.0 and EXACT at 2.0.", cls="sm t-hi"))
    b.append(label(586, 310, "At 2 it returns −q, which is the same rotation.",
                   cls="xs muted"))
    b.append(label(586, 326, "A test sampling those two points certifies it.", cls="xs muted"))

    b.append(label(W / 2, 362, "7.3's warning does not carry up — it gets sharper. "
                   "There the worst input was |z| = 2; here that input is", cls="xs muted"))
    b.append(label(W / 2, 380, "perfect, and the damage is in the middle where nobody thinks "
                   "to look. The failure is not monotonic.", cls="xs muted"))

    return svg(uid, W, H, "Drift, and a renormalisation that is exact at both ends",
               "On the left, a table of the deviation from orthonormality after a million "
               "composed rotations at three step sizes. The raw quaternion walk is worse than "
               "the raw matrix walk at every step. A single fast renormalisation per step "
               "brings it to six times ten to the minus eight, better than both by five orders "
               "of magnitude. On the right, a table of what the fast renormalisation does "
               "outside its domain: at an input norm of one and again at exactly two the pose "
               "error is zero, and in between it rises to nearly fifty degrees. The failure is "
               "not monotonic, so a test that sampled only one and two would certify a broken "
               "function.", b)


# ===========================================================================
# Figure 9 — what a quaternion cannot hold  (§11)
# ===========================================================================
def fig_cannot_hold():
    uid = "f74i"
    W, H = 760, 392
    b = [cmarker(uid, "mag", MAGENTA)]

    b.append(label(W / 2, 40, "the field is called `rotation` and one caller does not put a "
                   "rotation in it", cls="sm"))

    # the pipeline, as three boxes and two arrows
    y = 78
    stages = [("world_transform", "a mat4 from the\nhierarchy, scale\nand all", 60),
              ("transform.rotation", "a mat3, which\nwill hold\nanything", 300),
              ("quat", "which will\nnot", 540)]
    for name, note, x in stages:
        b.append(hollow(x, y, 160, 92, GREY if name != "quat" else MAGENTA, width=1.3))
        b.append(label(x + 80, y + 24, name, cls="xs mono t-hi"))
        for n, ln in enumerate(note.split("\n")):
            b.append(label(x + 80, y + 46 + n * 15, ln, cls="xs muted"))
    b.append(arrow(224, y + 46, 294, y + 46, uid, kind="s", width=1.4))
    b.append(arrow(464, y + 46, 534, y + 46, uid, kind="s", width=1.4))
    b.append(label(259, y + 36, "linear_of", cls="xs mono muted"))
    b.append(label(499, y + 36, "7.5", cls="xs mono muted"))

    # the measured table
    b.append(rule(40, 196, 720, 196, cls="grid"))
    b.append(label(W / 2, 220, "what the narrower type finds, measured (§11)", cls="sm"))

    rows = [
        ("a uniform scale of 2", "det 8 in, det 1.6531 out, |q| = 1.3337", "hi"),
        ("", "not the rotation, and not a unit quaternion either", None),
        ("a non-uniform scale", "the pose itself moves by 1.2089°", "hi"),
        ("take the scale off first", "column lengths, then extract: 0.000e+00°", None),
        ("a shear of 0.30", "column lengths are all 1. Invisible to both.", "hi"),
    ]
    y = 250
    for lhs, rhs, kind in rows:
        if lhs:
            b.append(label(330, y, lhs, cls="xs mono", anchor="end"))
        b.append(label(348, y, rhs, cls="xs" + (" t-hi" if kind else " muted"),
                       anchor="start"))
        y += 22

    b.append(label(W / 2, 372, "A `mat3` reproduced the affine matrix to the bit because it "
                   "was storing the scale too. That is the bug the type finds.",
                   cls="xs muted"))

    return svg(uid, W, H, "What a quaternion cannot hold, and why the swap is not a rename",
               "A three-stage pipeline: an ECS world transform holding a four by four matrix "
               "with scale in it, feeding through linear-of into the transform struct's "
               "rotation field which is a three by three matrix and will hold anything, and "
               "then into a quaternion which will not. Below, four measured rows: a uniform "
               "scale of two goes in with determinant eight and comes out with determinant "
               "1.6531 and a quaternion of norm 1.3337, which is neither answer; a non-uniform "
               "scale moves the pose itself by 1.2089 degrees; taking the scale off as the "
               "column lengths first recovers the rotation exactly; and a shear of 0.30 leaves "
               "all three column lengths at one and is invisible to both routes.", b)


# ===========================================================================
# Figure 10 — the four representations, side by side  (§12)
# ===========================================================================
def fig_scorecard():
    uid = "f74j"
    W, H = 760, 386
    b = []

    b.append(label(W / 2, 40, "four ways to write down a rotation, after four lessons",
                   cls="sm"))

    cols = ["", "euler", "axis-angle", "mat3", "quat"]
    rows = [
        ("floats", ["3", "4", "9", "4"]),
        ("readings", ["24", "1", "1", "1"]),
        ("singularity", ["yes", "no axis at 0", "no", "no"]),
        ("composes", ["no", "no", "27 mul", "16 mul"]),
        ("applies", ["no", "trig", "9 mul", "18 mul"]),
        ("interpolates", ["off it", "on it", "no", "on it"]),
        ("repair", ["—", "normalise axis", "Gram-Schmidt", "one Taylor step"]),
        ("extraction", ["2 routes", "2 routes + 120°", "—", "1 route, no case"]),
    ]

    x0, cw = 252, 134
    b.append(rule(30, 74, 730, 74, cls="grid"))
    for c, name in enumerate(cols[1:]):
        hi = "sm mono t-hi" if name == "quat" else "sm mono"
        b.append(label(x0 + c * cw, 66, name, cls=hi))

    y = 100
    for name, cells in rows:
        b.append(label(212, y, name, cls="xs muted", anchor="end"))
        for c, cell in enumerate(cells):
            hi = "xs mono t-hi" if c == 3 else "xs mono"
            b.append(label(x0 + c * cw, y, cell, cls=hi))
        y += 26

    b.append(rule(30, y - 16, 730, y - 16, cls="grid"))
    b.append(hollow(x0 + 3 * cw - 62, 80, 124, y - 96, GOLD, width=1.2, dash="4 3"))

    b.append(label(W / 2, y + 16, "It loses exactly one row, and that row has a workaround "
                   "that costs 4.6 ns.", cls="sm t-hi"))
    b.append(label(W / 2, y + 40, "Every other representation in this table was built in the "
                   "three lessons before it, and every one of them", cls="xs muted"))
    b.append(label(W / 2, y + 58, "is still in the engine: Euler angles are an interface, "
                   "axis-angle is a reading, a matrix is what the", cls="xs muted"))
    b.append(label(W / 2, y + 76, "renderer multiplies by. The quaternion is what the engine "
                   "will STORE, from Lesson 7.5.", cls="xs muted"))

    return svg(uid, W, H, "The four rotation representations compared",
               "A table comparing Euler angles, axis-angle, a three by three matrix and a "
               "quaternion across eight rows. Floats: three, four, nine, four. Readings: "
               "twenty-four, one, one, one. Singularity: yes, no axis at zero, no, no. "
               "Composes: no, no, twenty-seven multiplies, sixteen multiplies. Applies: no, "
               "trigonometry, nine multiplies, eighteen multiplies. Interpolates: off the "
               "geodesic, on it, not at all, on it. Repair: nothing, normalise the axis, "
               "Gram-Schmidt, one Taylor step. Extraction: two routes, two routes and a "
               "crossover at a hundred and twenty degrees, nothing, one route with no bad "
               "case. The quaternion column is outlined in gold and loses exactly one row, "
               "applying to a point.", b)


# ===========================================================================
def main():
    figures = [
        ("l74_fig1.svg", fig_forced),
        ("l74_fig2.svg", fig_product),
        ("l74_fig3.svg", fig_mirrors),
        ("l74_fig4.svg", fig_commute_render),
        ("l74_fig5.svg", fig_cover_render),
        ("l74_fig6.svg", fig_extract),
        ("l74_fig7.svg", fig_cost),
        ("l74_fig8.svg", fig_drift),
        ("l74_fig9.svg", fig_cannot_hold),
        ("l74_fig10.svg", fig_scorecard),
    ]
    for name, fn in figures:
        text = fn()
        with open(os.path.join(OUT, name), "w") as fh:
            fh.write(text)
        print(f"wrote {name}  ({len(text):,} bytes)")


if __name__ == "__main__":
    main()
