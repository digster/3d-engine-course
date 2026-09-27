#!/usr/bin/env python3
"""scratch/figs_73.py — Lesson 7.3's diagrams.

Same rules as 5.1-7.2's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - ~5.2 units per character for `xs`, ~6.0 for `sm`
  - filenames numbered by PAGE ORDER (7.2 got two of nine wrong, and nothing in
    the pipeline can catch that for itself)
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - `rule()` takes a CSS CLASS; `cline()` takes a COLOUR
  - LEGENDS AND ANNOTATION BOXES GO OUTSIDE THE PLOT
  - a SHAPE can leave the viewBox where a label cannot
  - panel heights are COMPUTED, never guessed

THIS LESSON IS IN THE PLANE, which changes one thing about the drawing and it is
worth saying out loud: there is no `View`, no projection and no foreshortening
anywhere in this file. A length on the page IS the length in the maths and an
angle on the page IS the angle, so these figures can be checked with a ruler and
a protractor in a way that none of Module 7's other figures can. That is the
same reason the lesson exists.

Every number below comes from verify_73's output (scratch/verify_73.log) or from
the plane demo's own receipt, except where a figure's geometry is recomputed here
in Python.
"""
import math
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)
from figs_610 import cline                                          # noqa: E402
from figs_45 import read_ppm, rle_rects, hexrgb                     # noqa: E402
from figs_511 import peak_sample                                    # noqa: E402
from figs_71 import poly, frame, cmarker, carrow, D                 # noqa: E402

OUT = "scratch"

# THE PLANE DEMO'S OWN PALETTE, transcribed from demos/plane/main.cpp, so that
# page and render agree. This is not decoration: `rle_rects` SNAPS each sampled
# pixel to the nearest palette entry (by hue, then brightness — figs_45), so a
# colour the palette does not contain comes out as whichever entry is nearest,
# and the first draft of Figure 6 rendered the demo's light blue mirror as grey
# and its amber ticks as khaki. The render then disagreed with its own caption.
TEAL = "#60ded0"      # k_result — the answer
VIOLET = "#d676e2"    # k_mirror — a mirror line
GOLD = "#f8d678"      # k_rotor  — the half-angle object
DEMO_Z = "#eca856"    # k_z      — the rotation you set
DEMO_W = "#7ebcf8"    # k_w      — what it acts on
DEMO_GHOST = "#7884a8"  # k_ghost — construction
DEMO_PROBE = "#e2e6ee"  # k_probe — the point being moved

# The order matters only in that every entry is reachable; `rle_rects` picks the
# nearest, so the list is the set of colours the render is allowed to contain.
DEMO_PALETTE = [hexrgb(c) for c in (TEAL, VIOLET, GOLD, DEMO_Z, DEMO_W,
                                    DEMO_GHOST, DEMO_PROBE, "#464e60")]


# ===========================================================================
# Measured — every figure's data, with the line of verify_73.log it came from
# ===========================================================================

# §D.1 — composing, and applying, ns per operation. Best of three within a run,
# after a warm-up pass; the run-to-run spread is under 3% on every row except
# "add angles, then trig", which moves by about 15%.
COMPOSE = [  # (label, ns, note)
    ("complex x complex", 0.605, "4 mul, 2 add"),
    ("mat2 x mat2", 1.139, "8 mul, 4 add"),
    ("add the angles,\nthen cos + sin", 3.903, "1 add, 2 transcendentals"),
]
APPLY = [("complex x vec2", 0.610), ("mat2 x vec2", 0.637)]

# §D.3 — after 1,000,000 composed rotations, both asked the SAME question:
# max |MᵀM − I|.
DRIFT = [("complex", 3.934e-06), ("mat2", 2.420e-05)]

# §F.5 — nlerp's angular speed ratio, measured against sec^2(Omega/2), and
# §F's gap column: how far apart nlerp and slerp get at the same t.
SCHEDULE = [  # (Omega deg, measured ratio, sec^2(Omega/2), worst gap deg)
    (10.0, 1.010, 1.008, 0.0049),
    (30.0, 1.074, 1.072, 0.1337),
    (60.0, 1.334, 1.333, 1.1170),
    (90.0, 2.002, 2.000, 4.0746),
    (120.0, 4.002, 4.000, 10.9483),
    (150.0, 14.943, 14.928, 26.3424),
    (179.0, 13134.350, 13131.480, 76.6512),
]

# §G.1 — generating a circle, ns per point.
CIRCLE_COST = [
    ("cos + sin\nper point", 1.851, "independent"),
    ("one complex multiply\n(z = z * step)", 1.529, "serial chain"),
    ("four chains\ninterleaved", 0.526, "chain broken"),
]

# §G.2 — what the recurrence costs in accuracy, against a double-precision walk.
CIRCLE_ERROR = [  # (n, plain deg, tidied deg, plain |z|-1, tidied |z|-1)
    (64, 9.815e-06, 1.744e-06, 3.576e-07, 0.0),
    (4096, 9.102e-05, 1.366e-05, 6.765e-05, 5.960e-08),
    (262144, 2.835e-04, 2.414e-03, 7.570e-05, 5.960e-08),
    (16777216, 7.165e-01, 1.276e+00, 3.703e-03, 5.960e-08),
]



class Plane:
    """Plane coordinates to page coordinates. No projection — see the header.

    `scale` is pixels per unit, and it is the same on both axes, which is what
    makes a right angle in the maths a right angle on the page. +y runs UP, so
    the page's y is negated exactly once, here.
    """

    def __init__(self, cx, cy, scale):
        self.cx, self.cy, self.scale = cx, cy, scale

    def __call__(self, p):
        return (self.cx + p[0] * self.scale, self.cy - p[1] * self.scale)

    def at(self, radius, angle_deg):
        a = angle_deg * D
        return self((radius * math.cos(a), radius * math.sin(a)))


def circle_path(pl, radius, colour, dash=None, width=1.1, segments=96):
    pts = [pl.at(radius, 360.0 * k / segments) for k in range(segments)]
    return poly(pts, colour, width=width, dash=dash, close=True)


def arc_path(pl, radius, a0, a1, colour, width=1.4, dash=None):
    n = max(8, int(abs(a1 - a0) / 3.0))
    pts = [pl.at(radius, a0 + (a1 - a0) * k / n) for k in range(n + 1)]
    return poly(pts, colour, width=width, dash=dash, close=False)


def plane_grid(pl, xs, ys, extent_x, extent_y):
    """A square grid plus the two axes, drawn first so everything covers it.

    `extent_y` may be a pair `(below, above)`. It usually should be: a figure
    whose content all sits above the real axis does not need an axis running down
    through its own caption, and check-page.js §4c calls that a collision
    (correctly — a rule through a line of prose reads as a strikethrough).
    """
    lo, hi = (extent_y if isinstance(extent_y, tuple) else (extent_y, extent_y))
    out = []
    for gx in xs:
        a, b = pl((gx, -lo)), pl((gx, hi))
        out.append(rule(round(a[0], 1), round(a[1], 1), round(b[0], 1), round(b[1], 1),
                        cls="grid", width=0.8))
    for gy in ys:
        a, b = pl((-extent_x, gy)), pl((extent_x, gy))
        out.append(rule(round(a[0], 1), round(a[1], 1), round(b[0], 1), round(b[1], 1),
                        cls="grid", width=0.8))
    a, b = pl((-extent_x, 0.0)), pl((extent_x, 0.0))
    out.append(rule(round(a[0], 1), round(a[1], 1), round(b[0], 1), round(b[1], 1),
                    cls="ink-soft", width=1.0))
    a, b = pl((0.0, -lo)), pl((0.0, hi))
    out.append(rule(round(a[0], 1), round(a[1], 1), round(b[0], 1), round(b[1], 1),
                    cls="ink-soft", width=1.0))
    return out


def marker(pl, p, colour, r=3.2):
    x, y = pl(p)
    return (f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{r}" fill="{colour}"/>')


# Glyph widths, in page units per character. MEASURED, not guessed: the header's
# "~5.2 units per character" has been carried forward since Lesson 5.1 and was
# never checked, so a calibration page was served and `getComputedTextLength()`
# asked directly. Monospace is exact at these sizes; the proportional face
# depends on the string, ranging from 4.36 for ordinary prose to 8.45 for twenty
# capital Ms, so the number below is a working upper bound for prose and this
# estimator is a CHEAP FIRST PASS. The authoritative check is check-page.js §4a,
# which measures the rendered boxes in a real browser.
CHAR_W = {("xs", True): 5.72, ("xs", False): 5.60,
          ("sm", True): 6.63, ("sm", False): 6.50}


def text_width(text, cls):
    parts = cls.split()
    size = "sm" if "sm" in parts else "xs"
    return len(text) * CHAR_W[(size, "mono" in parts)]


def place(pl, radius, angle_deg, text, cls="xs mono", pad=9.0):
    """A label beside a point on a circle, with the anchor chosen by direction.

    Hand-placed offsets are what made the first draft of this file unreadable:
    a `+14, +4` that suits a label at 25 degrees puts the one at 205 degrees on
    top of its own arrow. Anchoring by quadrant and pushing outward along the
    radius is one rule that works everywhere, and it is why every label in this
    file is computed.
    """
    a = angle_deg * D
    x, y = pl.at(radius, angle_deg)
    cos_a = math.cos(a)
    if cos_a > 0.3:
        anchor, dx = "start", pad
    elif cos_a < -0.3:
        anchor, dx = "end", -pad
    else:
        anchor, dx = "middle", 0.0
    # A near-horizontal radius puts the label straight onto the real axis, which
    # check-page.js §4c reports as text-on-shape and a reader experiences as a
    # line struck through the word. Lift those clear rather than centring them.
    sin_a = math.sin(a)
    if sin_a > 0.3:
        dy = -pad * 0.6
    elif sin_a < -0.3:
        dy = pad * 1.1
    else:
        dy = -pad * 0.8
    return label(round(x + dx, 1), round(y + dy, 1), text, cls=cls, anchor=anchor)


# ===========================================================================
# Figure 1 — the quarter turn, and i^2 = -1  (§3)
# ===========================================================================
# ONE CLAIM: i-squared-is-minus-one is not an axiom that happens to be useful in
# geometry. It is a MEASUREMENT of the plane — do a quarter turn twice and you
# have multiplied by -1 — and the algebra is the consequence.

def fig_quarter_turn():
    uid = "f73a"
    W, H = 760, 366
    b = []
    for name, colour in (("blue", BLUE), ("teal", TEAL), ("amber", AMBER)):
        b.append(cmarker(uid, name, colour))

    for panel, (title, sub, labels) in enumerate((
            ("the operator", "a quarter turn, applied once and then again",
             {0.0: "(1, 0)", 90.0: "(0, 1)", 180.0: "(\u22121, 0)"}),
            ("the same picture, read as numbers", "so the quarter turn squares to \u22121",
             {0.0: "1", 90.0: "i", 180.0: "\u22121"}))):
        cx = 194 + panel * 372
        pl = Plane(cx, 196, 62)
        b.append(label(cx, 28, title, cls="sm"))
        b.append(label(cx, 45, sub, cls="xs muted"))
        b.extend(plane_grid(pl, [-1.5, -1.0, -0.5, 0.5, 1.0, 1.5],
                            [-1.0, -0.5, 0.5, 1.0], 1.66, 1.2))
        b.append(circle_path(pl, 1.0, GREY, dash="3 3", width=0.9))

        o = pl((0.0, 0.0))
        for angle, colour, name in ((0.0, BLUE, "blue"), (90.0, TEAL, "teal"),
                                    (180.0, AMBER, "amber")):
            t = pl.at(1.0, angle)
            b.append(carrow(round(o[0], 2), round(o[1], 2), round(t[0], 2), round(t[1], 2),
                            uid, name, colour, width=2.0))
            cls = "sm mono t-hi" if panel else "xs mono"
            b.append(place(pl, 1.05, angle, labels[angle], cls=cls, pad=11.0))

        if panel == 0:
            b.append(arc_path(pl, 0.46, 4.0, 86.0, TEAL, width=1.6))
            b.append(arc_path(pl, 0.72, 94.0, 176.0, AMBER, width=1.6))
            b.append(place(pl, 0.46, 45.0, "90\u00b0", cls="xs muted", pad=13.0))
            b.append(place(pl, 0.72, 135.0, "90\u00b0", cls="xs muted", pad=13.0))

    y = 300
    b.append(hollow(118, y - 22, 524, 50, GREY, width=1.0))
    b.append(label(380, y, "turning a quarter circle twice  =  multiplying by \u22121",
                   cls="sm"))
    b.append(label(380, y + 19,
                   "so call the quarter turn i, and i\u00b2 = \u22121 is a measurement "
                   "of the plane", cls="xs muted"))

    return svg(uid, W, H, "The quarter turn and i squared",
               "Two panels showing the same three arrows on the unit circle. On the left they "
               "are labelled by coordinates: the point (1,0) in blue, turned a quarter circle "
               "anticlockwise to (0,1) in teal, and turned again to (-1,0) in amber, with both "
               "ninety-degree arcs marked. On the right the identical arrows are labelled 1, i "
               "and minus 1. A box underneath states the conclusion: turning a quarter circle "
               "twice is multiplication by minus one, so i squared equals minus one is a "
               "measurement of the plane rather than an axiom imposed on the algebra.", b)

# ===========================================================================
# Figure 2 — the product as a spiral similarity  (§4)
# ===========================================================================
# ONE CLAIM: multiplying by z scales by |z| and turns by arg z, and the proof is
# that the triangle (0, 1, w) and the triangle (0, z, zw) are the SAME SHAPE.
# Real numbers throughout: (3 + 4i)(1 + 2i) = -5 + 10i.

def fig_product():
    uid = "f73b"
    W, H = 760, 398
    b = []
    for name, colour in (("blue", BLUE), ("amber", AMBER), ("teal", TEAL)):
        b.append(cmarker(uid, name, colour))

    # TWO PANELS AT DIFFERENT SCALES, and the second scale is the whole point.
    # Drawn on one scale the image triangle is five times the size of the
    # original and the claim "same shape" is unreadable; drawn at one fifth, the
    # two are the same size on the page and the eye can do the comparison it is
    # being asked to do. The factor of five is then stated rather than shown,
    # which is the honest trade and is why it is written between the panels.
    one, w, z, zw = (1.0, 0.0), (1.0, 2.0), (3.0, 4.0), (-5.0, 10.0)

    for panel, (title, sub, scale, pts) in enumerate((
            ("the triangle 0, 1, w", "before", 62.0,
             ((one, GREY, None, "1"), (w, BLUE, "blue", "w = 1 + 2i"))),
            ("the triangle 0, z, zw", "after \u2014 drawn at one fifth the scale", 12.4,
             ((z, AMBER, "amber", "z = 3 + 4i"), (zw, TEAL, "teal", "zw = \u22125 + 10i"))))):
        cx = 150 + panel * 400
        pl = Plane(cx, 268, scale)
        b.append(label(cx, 28, title, cls="sm"))
        b.append(label(cx, 45, sub, cls="xs muted"))
        unit = 1.0 if not panel else 5.0
        b.extend(plane_grid(pl, [k * unit for k in (-1, 1, 2)], [k * unit for k in (1, 2)],
                            2.4 * unit, (0.55 * unit, 2.35 * unit)))

        corners = [pl((0.0, 0.0))] + [pl(pt) for pt, *_ in pts]
        b.append(poly(corners, GREY, width=1.3, dash=None if panel else "4 3"))
        o = pl((0.0, 0.0))
        for pt, colour, name, text in pts:
            t = pl(pt)
            if name:
                b.append(carrow(round(o[0], 2), round(o[1], 2), round(t[0], 2), round(t[1], 2),
                                uid, name, colour, width=1.9))
            b.append(marker(pl, pt, colour, r=3.0))
            angle = math.degrees(math.atan2(pt[1], pt[0]))
            radius = math.hypot(*pt) * 1.02
            b.append(place(pl, radius, angle, text, cls="xs mono", pad=10.0))

        arg = math.degrees(math.atan2(4.0, 3.0))
        base = 0.0 if not panel else math.degrees(math.atan2(2.0, 1.0))
        r = 1.5 if not panel else 7.0
        b.append(arc_path(pl, r, base, base + (arg if panel else arg), AMBER if not panel
                          else TEAL, width=1.6))

    # The factor, stated between the panels, where neither plot can collide.
    b.append(label(380, 180, "\u00d7 |z| = 5", cls="sm mono t-hi"))
    b.append(label(380, 200, "and turned", cls="xs muted"))
    b.append(label(380, 216, "by arg z = 53.130\u00b0", cls="xs mono t-hi"))
    b.append(label(380, 248, "the two arcs", cls="xs muted"))
    b.append(label(380, 264, "are equal", cls="xs muted"))

    b.append(hollow(150, 316, 460, 58, GREY, width=1.0))
    b.append(label(380, 336, "5 \u00d7 2.2360680 = 11.1803398", cls="xs mono t-hi"))
    b.append(label(380, 356, "53.13010\u00b0 + 63.43495\u00b0 = 116.56505\u00b0",
                   cls="xs mono t-hi"))
    b.append(label(380, 388,
                   "moduli multiply, arguments add \u2014 and neither fact was put into the "
                   "multiplication rule on purpose", cls="xs muted"))

    return svg(uid, W, H, "Complex multiplication is a spiral similarity",
               "Two panels. The left draws the triangle with vertices at the origin, at 1, and "
               "at w = 1 + 2i. The right draws the triangle with vertices at the origin, at "
               "z = 3 + 4i, and at zw = -5 + 10i, at one fifth the scale so that the two "
               "appear the same size on the page. They are the same shape: multiplying by z "
               "scales by its modulus, 5, and turns by its argument, 53.13 degrees, and an arc "
               "in each panel marks that same angle. Underneath, the arithmetic: 5 times "
               "2.2360680 is 11.1803398, and 53.13010 plus 63.43495 is 116.56505 degrees.", b)

# ===========================================================================
# Figure 3 — complex and mat2 are the same algebra  (§5)
# ===========================================================================
# ONE CLAIM: a mat2 holding a rotation stores cos and sin TWICE, once with a sign
# flipped. The complex number is the same object with the duplicates deleted.

def fig_layout():
    uid = "f73c"
    W, H = 760, 362
    b = []

    CELL = 66
    CH = 38
    LX = 34

    def slab(x, y, entries, title, sub, title_cx=None):
        # HEADINGS ARE CENTRED ON THE COLUMN, NOT ON THE SLAB. The two-float slab
        # is a third the width of its own title, so centring the text on it put
        # the first word off the left edge of the viewBox; the overflow check in
        # this file caught it, which is the first thing that check caught that a
        # screenshot had not already shown.
        tx = title_cx if title_cx is not None else x + len(entries) * CELL / 2 - 2
        out = [label(tx, y - 44, title, cls="sm", anchor="middle"),
               label(tx, y - 29, sub, cls="xs muted", anchor="middle")]
        for i, (text, colour, note) in enumerate(entries):
            cx = x + i * CELL
            out.append(box(cx, y, CELL - 5, CH, colour, opacity=0.24, width=1.2))
            out.append(label(cx + (CELL - 5) / 2, y + 24, text, cls="sm mono"))
            out.append(label(cx + (CELL - 5) / 2, y + CH + 14, note, cls="xs mono muted"))
        return out

    b.append(label(LX + 2 * CELL - 2, 24, "the same two numbers, stored twice",
                   cls="xs t-hi", anchor="middle"))
    b.extend(slab(LX, 104, [("cos \u03b8", AMBER, "c0.x"), ("sin \u03b8", TEAL, "c0.y"),
                           ("\u2212sin \u03b8", TEAL, "c1.x"), ("cos \u03b8", AMBER, "c1.y")],
                  "mat2, holding a rotation", "four floats, two of them redundant"))
    b.extend(slab(LX, 244, [("cos \u03b8", AMBER, "re"), ("sin \u03b8", TEAL, "im")],
                  "complex, holding the same rotation", "two floats, and nothing else",
                  title_cx=LX + 2 * CELL - 2))

    # The duplication, drawn ABOVE the slab so no bracket crosses a label.
    for src, dst, colour in ((0, 3, AMBER), (1, 2, TEAL)):
        x0 = LX + src * CELL + (CELL - 5) / 2
        x1 = LX + dst * CELL + (CELL - 5) / 2
        top = 104 - 6 - (8 if src == 0 else 18)
        b.append(poly([(x0, 104 - 4), (x0, top), (x1, top), (x1, 104 - 4)],
                      colour, width=1.3, close=False))

    # The consequences, in a table with columns wide enough for their contents.
    bx, bw = 356, 372
    b.append(hollow(bx, 76, bw, 214, GREY, width=1.0))
    b.append(label(bx + bw / 2, 100, "what the two extra floats buy", cls="sm"))
    c_name, c_mat, c_cpx = bx + 16, bx + 244, bx + 356
    b.append(label(c_mat, 126, "mat2", cls="xs mono muted", anchor="end"))
    b.append(label(c_cpx, 126, "complex", cls="xs mono t-hi", anchor="end"))
    lines = [
        ("storage", "16 bytes", "8 bytes"),
        ("compose two", "1.139 ns", "0.605 ns"),
        ("apply to a point", "0.637 ns", "0.610 ns"),
        ("drift, 10\u2076 products", "2.42e\u22125", "3.93e\u22126"),
        ("renormalise", "Gram\u2013Schmidt", "one divide"),
        ("interpolate", "not a rotation", "on the arc"),
    ]
    for i, (what, a, c) in enumerate(lines):
        y = 150 + i * 22
        b.append(label(c_name, y, what, cls="xs muted", anchor="start"))
        b.append(label(c_mat, y, a, cls="xs mono", anchor="end"))
        b.append(label(c_cpx, y, c, cls="xs mono t-hi", anchor="end"))

    b.append(label(W / 2, 320,
                   "applying is the one row where nothing is saved: both are four multiplies "
                   "and two adds, and they are", cls="xs muted"))
    b.append(label(W / 2, 338,
                   "the SAME four multiplies. The savings are in keeping a rotation and "
                   "combining two, not in using one.", cls="xs muted"))

    return svg(uid, W, H, "A rotation matrix stores cosine and sine twice",
               "A memory-layout diagram. The top row shows a two-by-two matrix holding a "
               "rotation as four floats: cosine theta, sine theta, minus sine theta, cosine "
               "theta. Brackets above link the first two to the last two, showing that the "
               "second pair is the first pair again with one sign flipped. The bottom row "
               "shows a complex number holding the same rotation in two floats. A table "
               "compares the two on storage, composition cost, application cost, drift, "
               "renormalisation and interpolation; the complex number wins every row except "
               "application, where the two are identical.", b)

# ===========================================================================
# Figure 4 — Euler's formula, from the velocity of a point on a circle  (§6)
# ===========================================================================
# ONE CLAIM: e^{i theta} is not a definition to be swallowed. A point going round
# the unit circle at unit speed has velocity PERPENDICULAR to its position, and
# "perpendicular" is exactly "times i" — so z' = iz, and the function whose
# derivative is itself times a constant is the exponential.

def fig_euler():
    uid = "f73d"
    W, H = 760, 378
    b = []
    b.append(cmarker(uid, "blue", BLUE))
    b.append(cmarker(uid, "teal", TEAL))

    pl = Plane(196, 186, 88)
    b.append(label(196, 28, "a point going round at unit speed", cls="sm"))
    b.append(label(196, 45, "blue is where it is, teal is where it is going", cls="xs muted"))
    b.extend(plane_grid(pl, [-1.5, -1.0, -0.5, 0.5, 1.0, 1.5],
                        [-1.5, -1.0, -0.5, 0.5, 1.0], 1.66, 1.45))
    b.append(circle_path(pl, 1.0, GREY, dash="3 3", width=0.9))

    ox, oy = pl((0.0, 0.0))
    for angle in (35.0, 125.0, 245.0):
        vx, vy = pl.at(1.0, angle)
        wx, wy = pl.at(1.0, angle + 90.0)
        b.append(carrow(round(ox, 2), round(oy, 2), round(vx, 2), round(vy, 2),
                        uid, "blue", BLUE, width=1.7))
        b.append(carrow(round(vx, 2), round(vy, 2),
                        round(vx + (wx - ox) * 0.46, 2), round(vy + (wy - oy) * 0.46, 2),
                        uid, "teal", TEAL, width=1.8))

    b.append(place(pl, 0.62, 35.0, "z", cls="sm mono", pad=12.0))
    b.append(place(pl, 1.40, 78.0, "i\u00b7z", cls="sm mono t-hi", pad=10.0))
    b.append(label(196, 332, "the teal arrow is the blue one, turned a quarter circle",
                   cls="xs muted"))
    b.append(label(196, 350, "and a quarter circle is \u00d7 i", cls="xs t-hi"))

    bx, bw = 406, 322
    b.append(hollow(bx, 56, bw, 248, GREY, width=1.0))
    b.append(label(bx + bw / 2, 80, "why the exponential turns up", cls="sm"))
    steps = [
        ("go round the unit circle at unit speed", "xs muted"),
        ("the velocity is perpendicular to", "xs muted"),
        ("the position, and always unit length", "xs muted"),
        ("perpendicular, in this algebra, is \u00d7 i", "xs muted"),
        ("", ""),
        ("z\u2032(\u03b8) = i \u00b7 z(\u03b8),    z(0) = 1", "xs mono t-hi"),
        ("", ""),
        ("the only function whose derivative is", "xs muted"),
        ("itself times a constant is exp", "xs muted"),
        ("", ""),
        ("z(\u03b8) = exp(i\u03b8) = cos \u03b8 + i sin \u03b8", "xs mono t-hi"),
    ]
    for i, (text, cls) in enumerate(steps):
        if text:
            b.append(label(bx + bw / 2, 106 + i * 19, text, cls=cls))

    b.append(label(bx + bw / 2, 332, "nothing here is a definition to be swallowed \u2014",
                   cls="xs muted"))
    b.append(label(bx + bw / 2, 350, "the equation is the picture, written down",
                   cls="xs muted"))

    return svg(uid, W, H, "Euler's formula from the velocity of a circling point",
               "The unit circle with three positions marked. At each, a blue arrow runs from "
               "the origin to the point and a teal arrow leaves the point at right angles to "
               "it, in the direction of travel. The teal arrow is the blue one turned a "
               "quarter circle, which in this algebra is multiplication by i. A panel beside "
               "the picture turns that into the differential equation z prime of theta equals "
               "i times z of theta with z of zero equal to one, whose solution is exp of i "
               "theta, which is cosine theta plus i sine theta.", b)

# ===========================================================================
# Figure 5 — two mirrors make a turn of twice the angle between them  (§7)
# ===========================================================================
# ONE CLAIM, and it is the one the whole of Lesson 7.4 rests on: the theta/2 in a
# quaternion is not a convention. Rotations are built out of REFLECTIONS, and a
# reflection doubles the angle of the mirror that makes it.

def fig_mirrors():
    uid = "f73e"
    W, H = 760, 400
    b = []
    for name, colour in (("probe", "#c8ccd6"), ("teal", TEAL), ("gold", GOLD)):
        b.append(cmarker(uid, name, colour))

    alpha, beta = 20.0, 50.0
    once, twice = 2 * alpha, 2 * beta - 2 * alpha

    # The plot lives in a bounded column and every word lives outside it. The
    # first draft put the mirror labels at the ends of the mirror lines, which is
    # where the annotation box was, and the result was four collisions.
    pl = Plane(214, 202, 96)
    b.append(label(214, 28, "reflect, then reflect again", cls="sm"))
    b.append(label(214, 45, "mirrors at 20\u00b0 and 50\u00b0 \u2014 30\u00b0 apart",
                   cls="xs muted"))
    b.extend(plane_grid(pl, [-1.0, -0.5, 0.5, 1.0, 1.5], [-0.5, 0.5, 1.0],
                        1.55, (1.05, 1.35)))
    b.append(circle_path(pl, 1.0, GREY, dash="3 3", width=0.9))

    for angle, colour in ((alpha, VIOLET), (beta, BLUE)):
        a0, a1 = pl.at(-1.44, angle), pl.at(1.44, angle)
        b.append(cline(round(a0[0], 2), round(a0[1], 2), round(a1[0], 2), round(a1[1], 2),
                       colour, width=1.7))

    o = pl((0.0, 0.0))
    for angle, colour, name in ((0.0, "#c8ccd6", "probe"), (once, GREY, None),
                                (twice, TEAL, "teal")):
        t = pl.at(1.2, angle)
        if name:
            b.append(carrow(round(o[0], 2), round(o[1], 2), round(t[0], 2), round(t[1], 2),
                            uid, name, colour, width=2.0))
        else:
            b.append(cline(round(o[0], 2), round(o[1], 2), round(t[0], 2), round(t[1], 2),
                           colour, width=1.6))

    t = pl.at(0.60, (twice) / 2.0)
    b.append(carrow(round(o[0], 2), round(o[1], 2), round(t[0], 2), round(t[1], 2),
                    uid, "gold", GOLD, width=2.0))

    b.append(arc_path(pl, 0.38, alpha, beta, VIOLET, width=1.9))
    b.append(arc_path(pl, 1.38, 0.0, twice, TEAL, width=1.9))
    b.append(place(pl, 1.38, twice / 2, "60\u00b0", cls="sm t-hi", pad=13.0))

    # The legend: one row per thing on the plot, in the order the reader meets it.
    lx = 402
    rows = [
        ("#c8ccd6", "v", "the probe, at 0\u00b0"),
        (VIOLET, "m\u2080", "the first mirror, at 20\u00b0"),
        (GREY, "m\u2080\u00b2 v\u0304", "v reflected once \u2014 lands at 40\u00b0"),
        (BLUE, "m\u2081", "the second mirror, at 50\u00b0"),
        (TEAL, "\u2192 60\u00b0", "reflected again \u2014 TWICE the 30\u00b0"),
        (GOLD, "R", "the rotor, m\u2081 conj(m\u2080), at 30\u00b0"),
    ]
    for i, (colour, sym, text) in enumerate(rows):
        y = 64 + i * 26
        b.append(box(lx, y - 9, 16, 9, colour, opacity=0.95, width=0.0, rx=2))
        b.append(label(lx + 26, y, sym, cls="xs mono", anchor="start"))
        b.append(label(lx + 78, y, text, cls="xs muted", anchor="start"))

    bx, bw = 402, 326
    b.append(hollow(bx, 226, bw, 92, GREY, width=1.0))
    b.append(label(bx + bw / 2, 248, "the algebra, in two lines", cls="sm"))
    b.append(label(bx + bw / 2, 272, "reflect in m:   v  \u21a6  m\u00b2 \u00b7 conj(v)",
                   cls="xs mono t-hi"))
    b.append(label(bx + bw / 2, 296,
                   "twice:   v  \u21a6  (m\u2081 conj(m\u2080))\u00b2 \u00b7 v",
                   cls="xs mono t-hi"))

    b.append(label(W / 2, 360,
                   "30\u00b0 of mirror separation produced 60\u00b0 of rotation \u2014 "
                   "measured over 4,000 pairs at 3.8e\u22126 (verify_73 \u00a7E.4)",
                   cls="xs muted"))
    b.append(label(W / 2, 382,
                   "the rotor carries HALF the turn, and that is where the \u03b8/2 in every "
                   "quaternion comes from", cls="xs t-hi"))

    return svg(uid, W, H, "Two mirrors make a turn of twice the angle between them",
               "The plane with two mirror lines through the origin, one violet at twenty "
               "degrees and one blue at fifty, thirty degrees apart. A pale probe arrow starts "
               "along the positive real axis. Reflected in the first mirror it lands at forty "
               "degrees, drawn grey; reflected again in the second it lands at sixty degrees, "
               "drawn teal, with an arc marking the sixty degrees travelled. A gold arrow at "
               "thirty degrees is the rotor, the object carrying half the turn, which is "
               "exactly what a quaternion stores. A legend names each element and a panel "
               "gives the algebra: a reflection is m squared times the conjugate, so two of "
               "them compose to the rotor squared.", b)

# ===========================================================================
# Figure 6 — the mirrors, in the demo  (§7)  — A REAL RENDER
# ===========================================================================

def fig_mirrors_render():
    uid = "f73f"
    CELL = 3
    PX = 2.80
    CROP = (332, 62, 728, 398)
    gw = (CROP[2] - CROP[0]) // CELL
    gh = (CROP[3] - CROP[1]) // CELL

    PAD = 16.0
    CAP = 46.0
    PANEL_W = gw * PX
    PANEL_H = gh * PX
    W = 760.0
    # HEIGHT FROM BOTH COLUMNS, never from the panel alone. The notes beside the
    # render are often the taller of the two, and a viewBox sized to the image
    # clips them — silently, because SVG does not clip, it just draws past the
    # edge of a box the browser has already sized. The overflow check at the foot
    # of this file is what turned that from invisible into a build failure.
    NOTES_H = 50.0 + 12 * 18.0
    H = max(CAP + PANEL_H, NOTES_H) + 26.0

    b = []
    _w, _h, data = read_ppm(os.path.join(OUT, "l73_mirrors.ppm"))
    grid_w, grid_h, grid = peak_sample(data, _w, CROP, CELL)
    b.append(label(PAD + PANEL_W / 2, 22, "./build/demos/plane --mode 3", cls="sm mono"))
    b.append(label(PAD + PANEL_W / 2, 38, "the same construction, running", cls="xs muted"))
    b.extend(rle_rects(grid, grid_w, grid_h, PAD, CAP, PX, DEMO_PALETTE, levels=3,
                       bg_class="fill-soft"))
    b.append(hollow(PAD, CAP, PANEL_W, PANEL_H, GREY, width=1.0))

    bx = PAD * 2 + PANEL_W
    bw = W - bx - PAD
    b.append(label(bx + bw / 2, 22, "the program's own receipt", cls="sm"))
    lines = [
        ("mirrors at", "+20.00 and +50.00 deg", "xs mono"),
        ("", "(30.00 apart)", "xs mono muted"),
        ("", "", ""),
        ("rotor", "+30.00 deg", "xs mono t-hi"),
        ("rotation", "+60.00 deg", "xs mono t-hi"),
        ("", "", ""),
        ("", "printed beside the picture it", "xs muted"),
        ("", "drew, from the same two floats", "xs muted"),
        ("", "", ""),
        ("", "hold [Up] and the gold arrow", "xs muted"),
        ("", "moves at HALF the rate of the", "xs muted"),
        ("", "teal one, forever", "xs muted"),
    ]
    for i, (lhs, rhs, cls) in enumerate(lines):
        if not rhs:
            continue
        y = 50 + i * 18
        if lhs:
            b.append(label(bx, y, lhs, cls="xs muted", anchor="start"))
        b.append(label(bx + 62, y, rhs, cls=cls, anchor="start"))

    return svg(uid, round(W, 1), round(H, 1), "The two-mirror construction in the plane demo",
               "A render from the plane demo. Two mirror lines cross at the origin, one violet "
               "at twenty degrees and one blue at fifty. A white arrow along the real axis is "
               "the probe; a grey arrow at forty degrees is it reflected once; a teal arrow at "
               "sixty degrees is it reflected twice. A gold arrow at thirty degrees is the "
               "rotor. A violet arc marks the thirty degrees between the mirrors and a teal "
               "arc marks the sixty degrees the probe travelled. Beside it, the program's own "
               "printed line reports the mirror angles, the rotor angle and the rotation.", b)

# ===========================================================================
# Figure 7 — what it costs  (§9)
# ===========================================================================
# ONE CLAIM: the complex number is cheaper to COMPOSE and to KEEP, and exactly as
# expensive to APPLY. The third panel is the honest one and it is why it is here.
def fig_cost():
    uid = "f73g"
    W, H = 760, 388
    b = []

    PW, PH = 210.0, 176.0
    TOP = 72.0

    def bars(x, rows, peak, unit, title, sub, colours):
        out = [label(x + PW / 2, TOP - 30, title, cls="sm"),
               label(x + PW / 2, TOP - 14, sub, cls="xs muted"),
               frame(x, TOP, PW, PH)]
        n = len(rows)
        slot = PH / n
        for i, (name, value) in enumerate(rows):
            # A FIXED INSET, not a fraction of the slot. With a fraction, a
            # two-row panel and a three-row panel put their first bar at
            # different heights, and in the three-row case the label above it
            # straddled the frame's top edge — which check-page.js §4d reports
            # and a reader sees as a rule struck through the first word.
            y = TOP + i * slot + 20.0
            hgt = slot * 0.34
            width = (PW - 76) * (value / peak)
            out.append(box(x + 8, y, max(width, 1.5), hgt, colours[i], opacity=0.85,
                           width=1.0, rx=2))
            out.append(label(x + 8, y - 7, name.replace("\n", " "), cls="xs muted",
                             anchor="start"))
            out.append(label(x + 12 + max(width, 1.5), y + hgt - 3,
                             unit(value), cls="xs mono", anchor="start"))
        return out

    peak = max(v for _, v, _ in COMPOSE)
    b.extend(bars(30, [(n, v) for n, v, _ in COMPOSE], peak, lambda v: f"{v:.3f} ns",
                  "composing two rotations", "10,240,000 operations, best of three",
                  [TEAL, BLUE, AMBER]))

    peak2 = max(v for _, v in APPLY)
    b.extend(bars(276, APPLY, peak2, lambda v: f"{v:.3f} ns",
                  "applying one to a point", "the row where nothing is saved",
                  [TEAL, BLUE]))

    peak3 = max(v for _, v in DRIFT)
    b.extend(bars(522, DRIFT, peak3, lambda v: f"{v:.2e}",
                  "drift after 10⁶ products", "max |MᵀM − I|, both asked the same",
                  [TEAL, BLUE]))

    b.append(label(W / 2, 292, "1.88× cheaper to compose · "
                   "1.05× — a wash — to apply · "
                   "6.15× less drift · half the bytes", cls="sm t-hi"))
    b.append(label(W / 2, 320,
                   "the middle panel is the one worth staring at: a representation can be "
                   "strictly better at keeping and", cls="xs muted"))
    b.append(label(W / 2, 338,
                   "combining rotations and not one instruction better at using them, because "
                   "z × v and M × v are", cls="xs muted"))
    b.append(label(W / 2, 356,
                   "the same four multiplies and the same two adds. "
                   "Lesson 7.4 measures the 3-D version of this trade.", cls="xs muted"))

    return svg(uid, W, H, "What the complex representation costs",
               "Three bar charts. The first compares composing two plane rotations: a complex "
               "product at 0.605 nanoseconds, a two-by-two matrix product at 1.139, and adding "
               "the angles then calling cosine and sine at 3.903. The second compares applying "
               "a rotation to a point, where the complex number at 0.610 and the matrix at "
               "0.637 are effectively identical. The third compares drift after a million "
               "composed rotations, where the matrix has drifted 6.15 times further. The "
               "summary is that the complex number is cheaper to compose and to keep and "
               "exactly as expensive to use.", b)


# ===========================================================================
# Figure 8 — slerp against nlerp, in the demo  (§10)  — A REAL RENDER
# ===========================================================================

def fig_blend_render():
    uid = "f73h"
    CELL = 3
    PX = 2.20
    CROP = (438, 60, 716, 470)
    gw = (CROP[2] - CROP[0]) // CELL
    gh = (CROP[3] - CROP[1]) // CELL

    PAD = 16.0
    CAP = 46.0
    PANEL_W = gw * PX
    PANEL_H = gh * PX
    W = 760.0
    NOTES_H = 48.0 + 17 * 17.0 + 10.0 + 34.0
    H = max(CAP + PANEL_H, NOTES_H) + 22.0

    b = []
    _w, _h, data = read_ppm(os.path.join(OUT, "l73_blend.ppm"))
    grid_w, grid_h, grid = peak_sample(data, _w, CROP, CELL)
    b.append(label(PAD + PANEL_W / 2, 22, "plane --mode 4 --t 0.24", cls="sm mono"))
    b.append(label(PAD + PANEL_W / 2, 38, "one arc, two schedules", cls="xs muted"))
    b.extend(rle_rects(grid, grid_w, grid_h, PAD, CAP, PX, DEMO_PALETTE, levels=3,
                       bg_class="fill-soft"))
    b.append(hollow(PAD, CAP, PANEL_W, PANEL_H, GREY, width=1.0))

    bx = PAD * 2 + PANEL_W
    bw = W - bx - PAD
    b.append(label(bx + bw / 2, 22, "what the ticks say", cls="sm"))
    notes = [
        ("outer, teal", "slerp, at t = 0, 1/16, \u2026 1"),
        ("", "EVENLY SPACED, because the"),
        ("", "angle covered is t \u00d7 145\u00b0"),
        ("", "and nothing else"),
        ("", ""),
        ("inner, amber", "nlerp, at the SAME sixteen"),
        ("", "values of t \u2014 crowded at both"),
        ("", "ends, spread through the middle"),
        ("", ""),
        ("the arrows", "where each is at t = 0.24;"),
        ("", "nlerp is 21\u00b0 behind, and it"),
        ("", "catches up by arriving in the"),
        ("", "same place at the same moment"),
        ("", ""),
        ("faint chord", "the straight line nlerp walks,"),
        ("", "before the normalise pushes"),
        ("", "it back out to the circle"),
    ]
    for i, (lhs, rhs) in enumerate(notes):
        if not rhs:
            continue
        y = 48 + i * 17
        if lhs:
            b.append(label(bx, y, lhs, cls="xs t-hi", anchor="start"))
        b.append(label(bx + 74, y, rhs, cls="xs muted", anchor="start"))

    y = 48 + len(notes) * 17 + 10
    b.append(label(bx + bw / 2, y, "excess turning: 0.00%", cls="sm mono t-hi"))
    b.append(label(bx + bw / 2, y + 18, "nlerp cannot leave the arc.", cls="xs muted"))
    b.append(label(bx + bw / 2, y + 34, "It only mistimes it.", cls="xs muted"))

    return svg(uid, round(W, 1), round(H, 1), "Slerp and nlerp on the same arc",
               "A render from the plane demo showing an arc of 145 degrees on the unit circle. "
               "Teal tick marks outside the circle show where spherical linear interpolation "
               "is at sixteen equally spaced values of t; they are evenly spaced. Amber tick "
               "marks inside the circle show where normalised linear interpolation is at the "
               "same sixteen values; they crowd together at both ends of the arc and spread "
               "out through the middle. Two arrows show the positions at t equals 0.24, where "
               "the amber one lags the teal one by about 21 degrees. A note explains that "
               "nlerp travels exactly the same arc and wastes no turning at all; what it gets "
               "wrong is the schedule.", b)

# ===========================================================================
# Figure 9 — the schedule, with a closed form  (§10)
# ===========================================================================
# ONE CLAIM: nlerp's speed ratio is exactly sec^2(Omega/2), which is derived in
# the lesson and measured here at seven arcs. The second panel is the number an
# animation programmer actually needs: how wrong is the cheap one, in degrees.
def fig_schedule():
    uid = "f73i"
    W, H = 760, 402
    b = []

    PW, PH = 300.0, 226.0
    LX, TOP = 62.0, 62.0

    # ---- left: the speed ratio, log y, measured dots on a computed curve ----
    b.append(label(LX + PW / 2, TOP - 30, "nlerp's fastest step ÷ its slowest", cls="sm"))
    b.append(label(LX + PW / 2, TOP - 14,
                   "measured at 20,000 steps; the curve is sec²(Ω/2)", cls="xs muted"))
    b.append(frame(LX, TOP, PW, PH))

    lo, hi = 0.0, 4.2          # log10 of the ratio
    def px(omega):
        return LX + PW * omega / 180.0
    def py(ratio):
        v = math.log10(max(ratio, 1.0 + 1e-9))
        return TOP + PH * (1.0 - (v - lo) / (hi - lo))

    for decade in range(0, 5):
        y = py(10.0 ** decade)
        b.append(rule(round(LX, 1), round(y, 1), round(LX + PW, 1), round(y, 1),
                      cls="grid", width=0.8))
        b.append(label(round(LX - 8, 1), round(y + 4, 1),
                       f"{10 ** decade:,}×" if decade else "1×",
                       cls="xs muted", anchor="end"))
    for omega in (0, 45, 90, 135, 180):
        x = px(omega)
        b.append(rule(round(x, 1), round(TOP, 1), round(x, 1), round(TOP + PH, 1),
                      cls="grid", width=0.8))
        b.append(label(round(x, 1), round(TOP + PH + 16, 1), f"{omega}°",
                       cls="xs muted"))

    curve = []
    for k in range(1, 360):
        omega = 180.0 * k / 360.0
        c = math.cos(0.5 * omega * D)
        ratio = 1.0 / (c * c)
        if ratio > 10 ** hi:
            break
        curve.append((px(omega), py(ratio)))
    b.append(poly(curve, BLUE, width=1.6, close=False))

    for omega, measured, closed, _gap in SCHEDULE:
        x, y = px(omega), py(measured)
        b.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="3.4" fill="{AMBER}"/>')
    b.append(label(LX + 12, TOP + 20, "closed form", cls="xs muted", anchor="start"))
    b.append(f'<circle cx="{LX + 96:.2f}" cy="{TOP + 16:.2f}" r="3.4" fill="{AMBER}"/>')
    b.append(label(LX + 104, TOP + 20, "measured", cls="xs muted", anchor="start"))
    b.append(rule(round(LX + 62, 1), round(TOP + 16, 1), round(LX + 88, 1), round(TOP + 16, 1),
                  cls="grid", width=0.0))
    b.append(cline(LX + 62, TOP + 16, LX + 88, TOP + 16, BLUE, width=1.6))

    # ---- right: how wrong, in degrees ------------------------------------
    RX = 452.0
    RW = 250.0
    b.append(label(RX + RW / 2, TOP - 30, "how far apart, at the same t", cls="sm"))
    b.append(label(RX + RW / 2, TOP - 14, "worst orientation gap over the blend",
                   cls="xs muted"))
    b.append(frame(RX, TOP, RW, PH))
    peak = max(g for *_, g in SCHEDULE)
    slot = PH / len(SCHEDULE)
    for i, (omega, _m, _c, gap) in enumerate(SCHEDULE):
        y = TOP + i * slot + slot * 0.22
        hgt = slot * 0.40
        width = (RW - 96) * (gap / peak)
        b.append(box(RX + 44, y, max(width, 1.5), hgt, AMBER, opacity=0.82, width=1.0, rx=2))
        b.append(label(RX + 38, y + hgt - 2, f"{omega:.0f}°", cls="xs mono muted",
                       anchor="end"))
        # THREE DECIMALS BELOW A TENTH OF A DEGREE. At two, the ten-degree row
        # printed "0.00 deg", which is a measured 0.0049 rounded into a claim
        # the measurement does not make — that the two blends agree exactly there.
        text = f"{gap:.2f}\u00b0" if gap >= 0.1 else f"{gap:.3f}\u00b0"
        b.append(label(RX + 50 + max(width, 1.5), y + hgt - 2, text,
                       cls="xs mono", anchor="start"))

    b.append(label(W / 2, 336,
                   "under 60° of arc the cheap blend is wrong by about a degree and "
                   "nobody will ever see it;", cls="xs muted"))
    b.append(label(W / 2, 354,
                   "at 150° it is wrong by 26° and the character's arm visibly "
                   "lurches through the middle of the turn.", cls="xs muted"))
    b.append(label(W / 2, 378,
                   "That is the whole decision, and it is a threshold on the ARC "
                   "rather than a preference between two functions.", cls="xs t-hi"))

    return svg(uid, W, H, "nlerp's speed ratio and its error, measured",
               "Two panels. The left plots nlerp's fastest step divided by its slowest against "
               "the size of the arc, on a logarithmic scale from one to ten thousand. A blue "
               "curve is the closed form, secant squared of half the arc; seven amber dots are "
               "measurements at ten, thirty, sixty, ninety, one hundred and twenty, one hundred "
               "and fifty and one hundred and seventy-nine degrees, and each lands on the "
               "curve. The right panel is a bar chart of how far nlerp and slerp get from each "
               "other at the same value of t: 0.005 degrees at a ten-degree arc, four degrees "
               "at ninety, and seventy-seven degrees at a hundred and seventy-nine.", b)


# ===========================================================================
# Figure 10 — the recurrence is latency-bound  (§11)
# ===========================================================================
# ONE CLAIM, and it is a warning about folklore: "replace the trig with a
# recurrence" is an optimisation from an era when a sine cost a hundred cycles.
# Measured on this machine it buys 1.21x, because the recurrence is a SERIAL
# CHAIN and the trig loop is not. Break the chain and it buys 3.5x.
def fig_recurrence():
    uid = "f73j"
    W, H = 760, 400
    b = []

    PW, PH = 330.0, 168.0
    LX, TOP = 34.0, 74.0
    peak = max(v for _, v, _ in CIRCLE_COST)
    slot = PH / len(CIRCLE_COST)

    b.append(label(LX + PW / 2, TOP - 30, "generating one point of a circle", cls="sm"))
    b.append(label(LX + PW / 2, TOP - 14, "4,096 points × 400 circles, best of three",
                   cls="xs muted"))
    b.append(frame(LX, TOP, PW, PH))
    for i, (name, value, note) in enumerate(CIRCLE_COST):
        y = TOP + i * slot + 20.0
        hgt = slot * 0.30
        width = (PW - 92) * (value / peak)
        colour = (BLUE, AMBER, TEAL)[i]
        b.append(box(LX + 10, y, max(width, 1.5), hgt, colour, opacity=0.85, width=1.0, rx=2))
        b.append(label(LX + 10, y - 7, name.replace("\n", " "), cls="xs muted", anchor="start"))
        b.append(label(LX + 14 + max(width, 1.5), y + hgt - 3, f"{value:.3f} ns",
                       cls="xs mono", anchor="start"))
        b.append(label(LX + 14 + max(width, 1.5), y + hgt + 12, note,
                       cls="xs muted", anchor="start"))

    # ---- right: what it costs in accuracy --------------------------------
    RX = 414.0
    RW = 312.0
    b.append(label(RX + RW / 2, TOP - 30, "and what the recurrence costs", cls="sm"))
    b.append(label(RX + RW / 2, TOP - 14, "worst angle error vs a double-precision walk",
                   cls="xs muted"))
    b.append(frame(RX, TOP, RW, PH))
    cols = (RX + 66, RX + 150, RX + 236, RX + 306)
    b.append(label(cols[0], TOP + 18, "points", cls="xs mono muted", anchor="end"))
    b.append(label(cols[1], TOP + 18, "plain", cls="xs mono muted", anchor="end"))
    b.append(label(cols[2], TOP + 18, "renormalised", cls="xs mono muted", anchor="end"))
    b.append(label(cols[3], TOP + 18, "|z|−1", cls="xs mono muted", anchor="end"))
    for i, (n, plain, tidied, mod, _mod_t) in enumerate(CIRCLE_ERROR):
        y = TOP + 40 + i * 24
        b.append(label(cols[0], y, f"{n:,}", cls="xs mono", anchor="end"))
        b.append(label(cols[1], y, f"{plain:.2e}°", cls="xs mono", anchor="end"))
        cls = "xs mono t-hi" if tidied > plain else "xs mono"
        b.append(label(cols[2], y, f"{tidied:.2e}°", cls=cls, anchor="end"))
        b.append(label(cols[3], y, f"{mod:.1e}", cls="xs mono muted", anchor="end"))
    b.append(label(RX + RW / 2, TOP + PH - 12,
                   "highlighted where renormalising made the ANGLE worse", cls="xs muted"))

    y = 286
    b.append(label(W / 2, y,
                   "the op count predicts 6× and the measurement says 1.21×, "
                   "because z = z × step is a SERIAL CHAIN:", cls="xs muted"))
    b.append(label(W / 2, y + 18,
                   "each multiply waits for the last, so the loop measures LATENCY while the "
                   "trig loop — every point independent —", cls="xs muted"))
    b.append(label(W / 2, y + 36,
                   "measures THROUGHPUT. Run four circles at once and the chain is broken: "
                   "0.526 ns, 3.5×.", cls="xs muted"))
    b.append(label(W / 2, y + 62,
                   "And renormalising fixes the modulus, does nothing for the angle, "
                   "and past a quarter of a million steps makes it worse.",
                   cls="xs t-hi"))

    return svg(uid, W, H, "The trig-free circle, measured",
               "Two panels. The left is a bar chart of the cost of generating one point of a "
               "circle: calling cosine and sine costs 1.851 nanoseconds, stepping a complex "
               "number by one multiply costs 1.529, and running four such chains interleaved "
               "costs 0.526. The right is a table of the worst angle error the recurrence "
               "accumulates against a double-precision walk, at 64, 4096, 262144 and 16.7 "
               "million points, with and without a cheap renormalise. The renormalised column "
               "is highlighted at the two longest walks, where it is worse than leaving the "
               "drift alone.", b)


# ===========================================================================
# Figure 11 — the fourth dimension is forced  (§12)
# ===========================================================================
# ONE CLAIM: Hamilton did not choose four dimensions for elegance. Three is
# impossible, and the proof is four lines that a reader can check.
def fig_forced():
    uid = "f73k"
    W, H = 760, 350
    b = []

    # The span, as a box the product tries and fails to land in.
    b.append(box(48, 74, 250, 150))
    b.append(label(173, 96, "everything a 3-D number could be", cls="sm"))
    b.append(label(173, 122, "a + b i + c j", cls="sm mono t-hi"))
    b.append(label(173, 146, "with a, b, c real", cls="xs muted"))
    b.append(label(173, 178, "i² = j² = −1", cls="xs mono muted"))
    b.append(label(173, 196, "and the product associative", cls="xs muted"))

    b.append(cmarker(uid, "red", RED))
    b.append(carrow(310, 150, 386, 150, uid, "red", RED, width=2.0))
    b.append(label(348, 138, "i · j", cls="sm mono"))
    b.append(label(348, 172, "must land", cls="xs muted"))
    b.append(label(348, 186, "somewhere", cls="xs muted"))

    # The contradiction, step by step.
    bx = 400
    b.append(hollow(bx, 74, 312, 150, RED, width=1.3))
    b.append(label(bx + 156, 96, "suppose it lands inside:  i j = a + b i + c j", cls="xs"))
    steps = [
        "i (i j)  =  (i i) j  =  −j",
        "i (a + b i + c j)  =  (−b + c a) + (a + c b) i + c² j",
        "match the j coefficient:   c² = −1",
    ]
    for i, text in enumerate(steps):
        cls = "xs mono t-bad" if i == 2 else "xs mono"
        b.append(label(bx + 156, 126 + i * 24, text, cls=cls))
    b.append(label(bx + 156, 206, "and c is REAL. There is no such c.", cls="xs t-bad"))

    b.append(label(W / 2, 262,
                   "so i j is not in the span of 1, i and j — a FOURTH basis element is "
                   "forced, and Hamilton spent thirteen years", cls="xs muted"))
    b.append(label(W / 2, 280,
                   "finding that out. The other thing that has to go is commutativity, and "
                   "Lesson 7.3 §12 shows why the plane", cls="xs muted"))
    b.append(label(W / 2, 298,
                   "could never have warned us: z v conj(z) = |z|² v here, exactly, "
                   "measured over 4,000 cases at 9.6e−7.", cls="xs muted"))
    b.append(label(W / 2, 326,
                   "The sandwich every quaternion text uses does NOTHING in two dimensions. "
                   "That is not a quirk — it is the reason.", cls="xs t-hi"))

    return svg(uid, W, H, "Why three dimensions cannot carry a rotation algebra",
               "On the left, a box containing every number a three-dimensional system spanned "
               "by one, i and j could hold, with the assumptions that i and j both square to "
               "minus one and the product is associative. A red arrow leaves the box labelled "
               "i times j, which must land somewhere. On the right, the contradiction in three "
               "lines: i times i j is minus j by associativity; expanding i times a plus b i "
               "plus c j gives c squared as the j coefficient; matching them forces c squared "
               "to equal minus one, and c is real. So the product escapes the space and a "
               "fourth basis element is forced.", b)


# ===========================================================================
# THE CHECK THAT SHOULD HAVE EXISTED SINCE 5.1
# ===========================================================================
# Three of this lesson's eleven figures shipped their first draft with a label
# running off the right edge of the viewBox, and every one of them LOOKED FINE
# in the generator: an SVG does not clip by default, so the text is still drawn,
# the browser still lays it out, and it simply disappears past the edge of the
# `figure` box. A shape may leave the viewBox — an axis line running off the side
# is often what you want — but a LABEL never may, because a half-visible word is
# indistinguishable from a rendering bug.
#
# So the generator now checks itself. It parses every `<text>` back out of the
# SVG it just produced, estimates the box from the anchor and the character
# count, and refuses to write a figure whose label crosses an edge.
TEXT_RE = re.compile(r'<text x="([-\d.]+)" y="([-\d.]+)" class="([^"]*)" '
                     r'text-anchor="(\w+)">(.*?)</text>', re.S)
TAG_RE = re.compile(r"<[^>]+>")
VIEWBOX_RE = re.compile(r'viewBox="0 0 ([\d.]+) ([\d.]+)"')


def overflowing(svg_text):
    w, h = (float(v) for v in VIEWBOX_RE.search(svg_text).groups())
    bad = []
    for x, y, cls, anchor, raw in TEXT_RE.findall(svg_text):
        x, y = float(x), float(y)
        text = TAG_RE.sub("", raw)
        width = text_width(text, cls)
        left = {"start": x, "middle": x - width / 2, "end": x - width}[anchor]
        right = left + width
        if left < -1.0 or right > w + 1.0 or y < 6.0 or y > h - 2.0:
            bad.append(f"{text[:44]!r} at x={x:.0f} y={y:.0f} "
                       f"spans {left:.0f}..{right:.0f} of 0..{w:.0f}")
    return bad


def main():
    figures = [
        ("l73_fig1.svg", fig_quarter_turn),
        ("l73_fig2.svg", fig_product),
        ("l73_fig3.svg", fig_layout),
        ("l73_fig4.svg", fig_euler),
        ("l73_fig5.svg", fig_mirrors),
        ("l73_fig6.svg", fig_mirrors_render),
        ("l73_fig7.svg", fig_cost),
        ("l73_fig8.svg", fig_blend_render),
        ("l73_fig9.svg", fig_schedule),
        ("l73_fig10.svg", fig_recurrence),
        ("l73_fig11.svg", fig_forced),
    ]
    failures = 0
    for name, fn in figures:
        body = fn()
        bad = overflowing(body)
        path = os.path.join(OUT, name)
        with open(path, "w") as fh:
            fh.write(body)
        note = ""
        if bad:
            failures += 1
            note = f"   <-- {len(bad)} LABEL(S) OUTSIDE THE VIEWBOX"
        print(f"wrote {path}  ({os.path.getsize(path):,} bytes){note}")
        for line in bad:
            print(f"      {line}")
    if failures:
        raise SystemExit(f"{failures} figure(s) have labels outside the viewBox")


if __name__ == "__main__":
    main()
