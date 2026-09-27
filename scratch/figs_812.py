#!/usr/bin/env python3
"""scratch/figs_812.py — Lesson 8.12's diagrams.

Same rules as 5.1-8.11's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - filenames numbered by PAGE ORDER
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - LEGENDS AND ANNOTATION BOXES GO OUTSIDE THE PLOT
  - panel heights are COMPUTED, never guessed
  - one <text> per table column: SVG collapses runs of spaces (8.11 figure 2)
  - a curve is DROPPED beyond its axis, never clamped onto it (8.8)

Every number below is transcribed from scratch/verify_812.log. Nothing here is
estimated, and the harness section each block came from is named above it.

*** WRITTEN IN LITERAL UNICODE THROUGHOUT, NEVER \\uXXXX. ***

THE COLOUR RULE, inherited from 8.4 through 8.11, with this lesson's reading:
  GREEN  = the answer that works — the half-way row, steering, sub-steps, the fix
  RED    = the failure — Euler's lock, the naive row, the teleport, 8.11's pass
  AMBER  = hinges, and rho
  BLUE   = cone-twist sockets, and the clip that owns an animated body
  PURPLE = the closed form a measurement is checked against
  GREY   = discarded work, axes, and passengers
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, rule, esc,          # noqa: E402,F401
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)
from figs_610 import cline                                          # noqa: E402
from figs_71 import cmarker, carrow, poly, frame                    # noqa: E402
from figs_76 import table                                           # noqa: E402
from figs_81 import legend                                          # noqa: E402

OUT = "scratch"


def para(x, y, text, cols=100, cls="xs muted", leading=15, anchor="start"):
    """Wrap `text` to `cols` characters and emit one label per line."""
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


def tbl(*args, **kwargs):
    return table(*args, **kwargs)


def logy(v, lo, hi, y0, height):
    t = (math.log10(v) - math.log10(lo)) / (math.log10(hi) - math.log10(lo))
    return y0 + height - t * height


def logx(v, lo, hi, x0, width):
    t = (math.log10(v) - math.log10(lo)) / (math.log10(hi) - math.log10(lo))
    return x0 + t * width


def dot(x, y, colour, r=3.2):
    return f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r}" fill="{colour}"/>'


def ring(x, y, colour, r=3.6, width=1.3):
    return (f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r}" fill="none" stroke="{colour}"'
            f' stroke-width="{width}"/>')


def halo(x, y, colour, r=7.0):
    """A wide pale disc under a prediction, so a measurement drawn exactly on
    top of it still shows both (8.11's lesson)."""
    return f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r}" fill="{colour}" fill-opacity="0.22"/>'


def capsule2d(x0, y0, x1, y1, r, colour, width=1.5, opacity=None):
    """A 2D capsule outline: two tangents and two end arcs."""
    dx, dy = x1 - x0, y1 - y0
    ln = math.hypot(dx, dy)
    if ln < 1e-6:
        return f'<circle cx="{x0:.1f}" cy="{y0:.1f}" r="{r:.1f}" fill="none" stroke="{colour}" stroke-width="{width}"/>'
    nx, ny = -dy / ln * r, dx / ln * r
    fill = f'fill="{colour}" fill-opacity="{opacity}"' if opacity else 'fill="none"'
    d = (f"M {x0 + nx:.2f},{y0 + ny:.2f} L {x1 + nx:.2f},{y1 + ny:.2f} "
         f"A {r:.2f} {r:.2f} 0 0 0 {x1 - nx:.2f},{y1 - ny:.2f} "
         f"L {x0 - nx:.2f},{y0 - ny:.2f} A {r:.2f} {r:.2f} 0 0 0 {x0 + nx:.2f},{y0 + ny:.2f} Z")
    return f'<path d="{d}" {fill} stroke="{colour}" stroke-width="{width}"/>'


def write(name, text):
    with open(os.path.join(OUT, name), "w") as fh:
        fh.write(text)
    print(f"{name}: {len(text)} bytes")


# ===========================================================================
# MEASUREMENTS — every one from scratch/verify_812.log, section named
# ===========================================================================

# §D — the humanoid and its parts (build_ragdoll's report)
RIG = [("root", -1, (0.00, 0.00)), ("hips", 0, (0.00, 0.95)), ("spine1", 1, (0.00, 0.12)),
       ("spine2", 2, (0.00, 0.14)), ("chest", 3, (0.00, 0.16)), ("neck", 4, (0.00, 0.18)),
       ("head", 5, (0.00, 0.10)), ("clavicle.L", 4, (0.05, 0.14)), ("upperarm.L", 7, (0.13, 0.00)),
       ("forearm.L", 8, (0.28, 0.00)), ("hand.L", 9, (0.25, 0.00)), ("clavicle.R", 4, (-0.05, 0.14)),
       ("upperarm.R", 11, (-0.13, 0.00)), ("forearm.R", 12, (-0.28, 0.00)), ("hand.R", 13, (-0.25, 0.00)),
       ("thigh.L", 1, (0.09, -0.05)), ("shin.L", 15, (0.00, -0.42)), ("foot.L", 16, (0.00, -0.41)),
       ("toe.L", 17, (0.00, -0.06)), ("thigh.R", 1, (-0.09, -0.05)), ("shin.R", 19, (0.00, -0.42)),
       ("foot.R", 20, (0.00, -0.41)), ("toe.R", 21, (0.00, -0.06))]
# (name, bone joint index, from, to, radius, kind, kg)
PARTS = [("pelvis", 1, (-0.07, 0.95), (0.07, 0.95), 0.11, "root", 11.36),
         ("torso", 2, (0.0, 1.20), (0.0, 1.40), 0.15, "cone", 28.40),
         ("head", 5, (0.0, 1.64), (0.0, 1.70), 0.10, "cone", 6.48),
         ("upperarm.L", 8, (0.22, 1.51), (0.42, 1.51), 0.045, "cone", 2.24),
         ("forearm.L", 9, (0.505, 1.51), (0.74, 1.51), 0.04, "hinge", 1.76),
         ("upperarm.R", 12, (-0.22, 1.51), (-0.42, 1.51), 0.045, "cone", 2.24),
         ("forearm.R", 13, (-0.505, 1.51), (-0.74, 1.51), 0.04, "hinge", 1.76),
         ("thigh.L", 15, (0.09, 0.83), (0.09, 0.55), 0.07, "cone", 8.00),
         ("shin.L", 16, (0.09, 0.43), (0.09, 0.12), 0.05, "hinge", 4.88),
         ("thigh.R", 19, (-0.09, 0.83), (-0.09, 0.55), 0.07, "cone", 8.00),
         ("shin.R", 20, (-0.09, 0.43), (-0.09, 0.12), 0.05, "hinge", 4.88)]

# §A — conditioning
EULER_MEAS = [(0.0, 1.0), (45.0, 1.4), (80.0, 5.8), (89.0, 57.3), (89.9, 609.9)]
ST_MEAS = [(0.0, 1.00), (45.0, 1.10), (80.0, 1.41), (89.0, 1.53), (89.9, 1.55), (135.0, 3.28),
           (170.0, 16.81), (179.0, 176.33)]
SPLIT_ERR = (4.956e-07, 1.461e-07, 8.663e-07, 1.272e-05)

# §B — the cone
CONE_ROW_ERR, CONE_FLIPPED = 1.097e-03, 2.001
SPEC_WORST, REACT_MEAN, REACT_WORST = 1.19e-07, 0.4977, 0.9952
RHO_ROD = 0.7614
REBOUND = [(1, 1.0693), (2, 0.5365), (4, 0.1850), (8, 0.0834), (16, 0.0119), (32, 0.0000)]

# §C — the twist row, and Codman
BINS = ["0–1", "1–30", "30–60", "60–90", "90–120", "120–150", "150–175"]
ERR_HALF = [6.30e-04, 9.95e-04, 9.73e-04, 1.27e-03, 1.45e-03, 2.98e-03, 5.79e-03]
ERR_BONE = [3.27e-02, 1.64, 2.58, 4.11, 5.14, 5.56, 4.97]
ERR_CONE = [3.27e-02, 1.76, 2.79, 4.12, 5.32, 6.16, 5.68]
BOUND_RATIO = 1.0045
CIRCLES = [(30, 48.231), (60, 180.000), (90, 360.000), (120, 540.000)]
CODMAN = -90.000
SOLVER_TWIST = [("half-way / cos, split impulse", 20.00, GREEN), ("about the bone, split impulse", 29.08, AMBER),
                ("about the bone, no position pass", 179.93, RED)]
PREDICTED_LAG = 9.55

# §D — a skeleton with mass
DEMPSTER = [("upper arm", 0.759, 0.647), ("forearm + hand", 0.781, 0.680),
            ("thigh", 0.772, 0.642), ("shin + foot", 0.770, 0.680)]
TOUCH = [("torso / forearm.R", 2, 9, 740, 94.8), ("torso / forearm.L", 2, 9, 667, 106.6),
         ("thigh.L / thigh.R", 2, 6, 414, 32.7), ("forearm.R / shin.R", 5, 5, 357, 20.2),
         ("head / forearm.L", 3, 4, 130, 42.2), ("forearm.R / thigh.R", 4, 4, 129, 24.4),
         ("forearm.L / shin.L", 5, 4, 187, 20.9), ("torso / thigh.R", 2, 2, 10, 14.9)]
RULES = [("two links", 189.9, 1503, 179.6), ("overlapping (default)", 106.6, 509, 61.4)]
NO_FILTER = (0.0429, 67.51)
DEFAULT_FILTER = (0.0015, 0.01)

# §E — the bug
KNEE_T = [0, 2, 4, 6, 8, 10]
KNEE = [("8.11's pass, cold", [0.0, -19.8455, -37.5492, -51.8791, -62.7588, -70.7129], RED, "8 4"),
        ("8.12's pass, cold", [0.0, -1.8915, -1.8915, -1.8915, -1.8915, -1.8915], GREEN, "8 4"),
        ("8.11's pass, warm", [0.0, -0.1691, -0.1691, -0.1691, -0.1691, -0.1691], RED, None),
        ("8.12's pass, warm", [0.0, -0.0000, -0.0000, -0.0000, -0.0000, -0.0000], GREEN, None)]
PASS_ROWS = [("lower stop", -0.002951, 0.03541, 0.006513), ("upper stop", 2.620945, 0.0, 0.006513)]
PSEUDO_SPIN = 3.648e-09

# §F — eight sweeps
BUDGETS = ["8 sweeps (shipped)", "16 sweeps", "32 sweeps", "8 sub-steps × 1", "CONTROL 32 × 4"]
PEAK = [92.47, 54.88, 32.75, 30.03, 3.69]
REST = [12.860, 7.204, 0.488, 6.741, 0.002]
HANG = [4.128, 3.319, 2.524, 0.001, 0.001]
CRUSH = [61.4, 28.8, 20.3, 3.3, 0.0]

# §G — handing over
LAND_ROD, LAND_LOG, TURN, THETA3 = 1.57e-07, 9.75e-05, 0.1054, 9.76e-05
CHORD_END, CHORD_MID = 0.2320, 0.0045
MOMENTUM, COM_SPEED = 204.40, 2.555
FWD = [("the chord (steer)", 1.179, 1.433, GREEN), ("at rest (control)", 0.010, 0.402, RED)]
RESID = [("upperarm.L", 0.01836, 0.01838), ("forearm.L", 0.01880, 0.01888),
         ("upperarm.R", 0.01835, 0.01835), ("forearm.R", 0.01900, 0.01895),
         ("thigh.L", 0.02943, 0.02945), ("shin.L", 0.05475, 0.05461),
         ("thigh.R", 0.02929, 0.02931), ("shin.R", 0.07269, 0.07276)]
RESID_WORST = 0.0043

# §H — disagreement
WALK_VIOL = [("forearm.L / .R", "32.00°", "twisted off the elbow's hinge"),
             ("shin.L / .R", "47.99°", "bent FORWARD past the knee's stop"),
             ("head", "14.83 mm", "neck anchor moved by passengers"),
             ("upperarm.L", "20.45 mm", "shoulder anchor moved by passengers")]
INTERNAL = [("split impulse", 0.971, 0.752, 3.28, 0.01), ("Baumgarte", 0.971, 1.108, 0.76, 0.01),
            ("none", 0.971, 0.751, 35.87, 24.12)]

# §I — handing back
RETURN = [("first-frame jump", "blend from read_pose", 0.0581, "snap to the clip", 1.6480, "m"),
          ("pelvis slide", "realigned", 0.000, "not realigned", 1.302, "m"),
          ("worst bone change", "local slerp blend", 0.06, "model-space blend", 208.06, "mm")]
WORST_ARC, NLERP_LAG = 115.5, 1.98

# §J — the fight
FIGHT = [("kinematic, steered", 0.000, 69.7, 4.78, 4.82, GREEN),
         ("dynamic, teleported", 20.626, 286.8, 3.67, 20.90, RED),
         ("dynamic, teleported + chord", 0.000, 159.3, 3.59, 4.82, AMBER)]


# ===========================================================================
# FIGURE 1 — a skeleton with mass, and one owner at a time                  (§2)
# ===========================================================================

def fig1():
    uid = "g1"
    W = 980
    body = [cmarker(uid, "blue", BLUE), cmarker(uid, "green", GREEN)]
    body.append(label(26, 30, "7.7's humanoid, T-posed, seen from the front", cls="sm", anchor="start"))

    s, cx, gy = 190.0, 220.0, 400.0

    def P(x, y):
        return cx + x * s, gy - y * s

    # joint positions in model space
    pos = []
    for name, parent, (ox, oy) in RIG:
        if parent < 0:
            pos.append((ox, oy))
        else:
            px, py = pos[parent]
            pos.append((px + ox, py + oy))
    part_bones = {p[1] for p in PARTS}
    colour = {"root": GREY, "cone": BLUE, "hinge": AMBER}
    for name, bone, a, b, r, kind, kg in PARTS:
        x0, y0 = P(*a)
        x1, y1 = P(*b)
        body.append(capsule2d(x0, y0, x1, y1, r * s, colour[kind], width=1.6, opacity=0.10))
    # bones
    for j, (name, parent, _) in enumerate(RIG):
        if parent > 0:
            x0, y0 = P(*pos[parent])
            x1, y1 = P(*pos[j])
            body.append(f'<line x1="{x0:.1f}" y1="{y0:.1f}" x2="{x1:.1f}" y2="{y1:.1f}" class="ink-soft" stroke-width="1.0"/>')
    for j, (name, parent, _) in enumerate(RIG):
        if j == 0:
            continue
        x, y = P(*pos[j])
        body.append(dot(x, y, AMBER if j in (9, 13, 16, 20) else BLUE, 3.4) if j in part_bones and j != 1
                    else (dot(x, y, GREY, 3.4) if j == 1 else ring(x, y, GREY, 2.8, 1.1)))
    body.append(rule(cx - 180, gy, cx + 180, gy, cls="grid", width=1.0))
    body.extend(legend(26, gy + 30, [
        (BLUE, "cone-twist socket: waist, neck, shoulders, hips"),
        (AMBER, "hinge: elbows and knees"),
        (GREY, "the root part (pelvis); hollow rings are passengers"),
    ]))

    # ---- RIGHT: who owns the bodies -------------------------------------------------
    rx = 500
    body.append(label(rx, 30, "Every body has exactly one owner at a time", cls="sm", anchor="start"))
    bw, bh = 200, 92
    ax_, ay_ = rx, 52
    bx_, by_ = rx + 270, 52
    body.append(hollow(ax_, ay_, bw, bh, BLUE, width=1.6))
    body.append(label(ax_ + bw / 2, ay_ + 22, "ANIMATED", cls="sm"))
    for i, t in enumerate(["the clip owns them", "kinematic: no mass", "steered by velocity",
                           "joints: nothing to do"]):
        body.append(label(ax_ + bw / 2, ay_ + 42 + i * 14, t, cls="xs muted"))
    body.append(hollow(bx_, by_, bw, bh, AMBER, width=1.6))
    body.append(label(bx_ + bw / 2, by_ + 22, "SIMULATED", cls="sm"))
    for i, t in enumerate(["the solver owns them", "dynamic: mass restored", "joints in the solver",
                           "read_pose → the skin"]):
        body.append(label(bx_ + bw / 2, by_ + 42 + i * 14, t, cls="xs muted"))
    body.append(carrow(ax_ + bw + 6, ay_ + 30, bx_ - 6, by_ + 30, uid, "green", GREEN))
    body.append(label((ax_ + bw + bx_) / 2, ay_ + 22, "simulate()", cls="xs mono"))
    body.append(carrow(bx_ - 6, by_ + 66, ax_ + bw + 6, ay_ + 66, uid, "blue", BLUE))
    body.append(label((ax_ + bw + bx_) / 2, ay_ + 84, "animate()", cls="xs mono"))

    t, th = tbl(rx, by_ + bh + 44, ["part", "follows", "link", "kg"],
                [[p[0], RIG[p[1]][0], {"root": "root", "cone": "cone-twist", "hinge": "hinge"}[p[5]],
                  f"{p[6]:.2f}"] for p in PARTS],
                [108, 108, 110, 70], title="The eleven parts: Dempster's fractions of 80 kg")
    body.extend(t)
    txt, nh = para(rx, by_ + bh + 44 + th + 18,
                   "Twelve of the 23 joints are passengers: a clavicle, a hand, a foot and a toe "
                   "ride on the nearest part above them at the local transform the clip last gave "
                   "them. Heaviest to lightest, torso to forearm, is 16.1 : 1.", cols=74)
    body.extend(txt)
    H = max(gy + 30 + 3 * 17 + 6, by_ + bh + 44 + th + 18 + nh) + 10
    return svg(uid, W, H,
               "The humanoid's ragdoll, and the two owners of its bodies",
               "Left: 7.7's 23-joint humanoid in its T-pose, with eleven capsules drawn over it — "
               "cone-twist sockets in blue, hinges in amber, the pelvis as the root — and the "
               "twelve passenger joints as hollow rings. Right: two states, animated and "
               "simulated, joined by simulate() and animate(), and a table of the eleven parts "
               "with the joint each follows, its link and its mass.", body)


# ===========================================================================
# FIGURE 2 — swing and twist, and where each split breaks                   (§3)
# ===========================================================================

def fig2():
    uid = "g2"
    W = 980
    body = [cmarker(uid, "blue", BLUE), cmarker(uid, "green", GREEN), cmarker(uid, "grey", GREY)]
    body.append(label(26, 30, "The swing is two mirrors", cls="sm", anchor="start"))
    cx, cy, R = 190, 180, 118
    body.append(f'<circle cx="{cx}" cy="{cy}" r="{R}" fill="none" class="grid" stroke-width="1"/>')
    phi = math.radians(70.0)
    t = (0.0, -1.0)
    c = (math.sin(phi), -math.cos(phi))
    h = (math.sin(phi / 2), -math.cos(phi / 2))

    def P(v, k=1.0):
        return cx + v[0] * R * k, cy - v[1] * R * k

    for v, col, name, dx in ((t, GREY, "t: the bone at rest", -8), (c, BLUE, "c: where q sends it", 10),
                             (h, GREEN, "h: half-way", 10)):
        x, y = P(v)
        body.append(carrow(cx, cy, x, y, uid, {GREY: "grey", BLUE: "blue", GREEN: "green"}[col], col, width=1.8))
        body.append(label(x + dx, y + 16, name, cls="xs", anchor="end" if dx < 0 else "start"))
    # mirror planes: perpendicular to t and to h
    for v, col in ((t, GREY), (h, GREEN)):
        px, py = -v[1], v[0]
        body.append(cline(cx - px * R * 1.12, cy + py * R * 1.12, cx + px * R * 1.12, cy - py * R * 1.12,
                          col, width=1.0, dash="5 4"))
    # the swing angle arc
    arc = [(cx + 42 * math.sin(a), cy + 42 * math.cos(a)) for a in [phi * k / 30 for k in range(31)]]
    body.append(poly(arc, BLUE, width=1.2, close=False))
    body.append(label(cx + 30, cy + 56, "φ", cls="xs"))
    lines = ["Reflect in the plane ⊥ t, then in the plane ⊥ h:",
             "two mirrors make a turn by twice the angle between",
             "them — 2·∠(t, h) = φ — about the line they share.",
             "That turn is the SWING: the smallest rotation that",
             "carries t to c. The TWIST is what is left, a turn",
             "about the bone itself: q = swing · twist."]
    for i, s in enumerate(lines):
        body.append(label(26, cy + R + 34 + i * 15, s, cls="xs", anchor="start"))

    # ---- RIGHT: conditioning ---------------------------------------------------------
    px0, py0, pw, ph = 510, 58, 420, 250
    body.append(label(px0 - 20, 30, "A hanging arm raised forward: how far each split's angles jump",
                      cls="sm", anchor="start"))
    body.append(frame(px0, py0, pw, ph))
    lo, hi = 0.8, 1000.0
    for e, lab in ((1, "1"), (10, "10"), (100, "100"), (1000, "1000")):
        yy = logy(e, lo, hi, py0, ph)
        body.append(rule(px0, yy, px0 + pw, yy, cls="grid", width=0.6))
        body.append(label(px0 - 6, yy + 4, lab, cls="xs mono", anchor="end"))

    def X(a):
        return px0 + a / 180.0 * pw

    for a in (0, 45, 90, 135, 180):
        body.append(label(X(a), py0 + ph + 16, f"{a}°", cls="xs mono"))
    body.append(label(px0 + pw / 2, py0 + ph + 32, "raised by (degrees)", cls="xs muted"))
    # Sampled every degree, and densely only where each curve turns up: the
    # first draft sampled every 0.05 degree and the figure weighed 65 KB.
    ea = [float(k) for k in range(0, 86)] + [85.0 + 0.05 * k for k in range(1, 100)]
    euler = [(X(a), logy(1.0 / math.cos(math.radians(a)), lo, hi, py0, ph))
             for a in ea if 1.0 / math.cos(math.radians(a)) <= hi]
    body.append(poly(euler, RED, width=1.6, close=False))
    sa = [float(k) for k in range(0, 171)] + [170.0 + 0.1 * k for k in range(1, 100)]
    st = [(X(a), logy(1.0 / math.cos(math.radians(a / 2)), lo, hi, py0, ph))
          for a in sa if 1.0 / math.cos(math.radians(a / 2)) <= hi]
    body.append(poly(st, GREEN, width=1.6, close=False))
    for a, v in EULER_MEAS:
        body.append(ring(X(a), logy(v, lo, hi, py0, ph), RED, 3.8))
    for a, v in ST_MEAS:
        body.append(ring(X(a), logy(v, lo, hi, py0, ph), GREEN, 3.8))
    for a, col in ((90, RED), (180, GREEN)):
        body.append(cline(X(a), py0, X(a), py0 + ph, col, width=0.9, dash="3 4"))
    body.extend(legend(px0, py0 + ph + 54, [
        (RED, "Euler (7.2's YXZ): 1/cos(pitch) — locks at 90°, an arm raised forward"),
        (GREEN, "swing–twist: grows as 1/cos(swing/2) — one singularity, at 180°"),
        (None, "rings are measured: the largest angle change per radian of random nudge;"),
        (None, "near 180° swing–twist's sit ≈1.5× above its curve, the swing vector's own blur"),
    ]))
    txt, nh = para(px0, py0 + ph + 54 + 4 * 17 + 12,
                   "609.9 against 573 at 89.9°: Euler's lock sits in the middle of a shoulder's "
                   "range. Swing–twist reads 1.55 there; its own singularity is the antipode of "
                   "the axis it measures from, which no cone contains.", cols=74)
    body.extend(txt)
    H = max(cy + R + 34 + 6 * 15, py0 + ph + 54 + 4 * 17 + 12 + nh) + 10
    return svg(uid, W, H,
               "The swing as two mirrors, and the conditioning of Euler angles against swing–twist",
               "Left: a unit circle with the bone at rest t, its image c, and the half-way vector "
               "h; mirror lines perpendicular to t and h; the swing angle phi. Right: on a log "
               "axis against the raise angle, Euler angles' sensitivity climbing to 610 at 89.9 "
               "degrees and swing–twist's staying near 1.5 until it climbs toward 180.", body)


# ===========================================================================
# FIGURE 3 — the swing cone is a limit like any other                       (§4)
# ===========================================================================

def fig3():
    uid = "g3"
    W = 980
    body = [cmarker(uid, "blue", BLUE), cmarker(uid, "purple", PURPLE), cmarker(uid, "grey", GREY)]
    body.append(label(26, 30, "The cone's row", cls="sm", anchor="start"))
    ax, ay, L = 190, 70, 190
    cone = math.radians(40)
    for sgn in (-1, 1):
        body.append(cline(ax, ay, ax + sgn * L * math.sin(cone), ay + L * math.cos(cone), BLUE, width=1.0, dash="5 4"))
    body.append(f'<ellipse cx="{ax}" cy="{ay + L * math.cos(cone):.1f}" rx="{L * math.sin(cone):.1f}" ry="22" '
                f'fill="none" stroke="{BLUE}" stroke-width="1.0" stroke-dasharray="5 4"/>')
    body.append(carrow(ax, ay, ax, ay + L * 1.05, uid, "grey", GREY, width=1.6))
    body.append(label(ax - 8, ay + L * 1.05 + 4, "a₁: the cone's axis", cls="xs", anchor="end"))
    b = math.radians(33)
    bx, by = ax + L * 0.95 * math.sin(b), ay + L * 0.95 * math.cos(b)
    body.append(carrow(ax, ay, bx, by, uid, "blue", BLUE, width=2.0))
    body.append(label(bx + 20, by + 16, "b₁: the bone", cls="xs", anchor="start"))
    arc = [(ax + 60 * math.sin(a), ay + 60 * math.cos(a)) for a in [b * k / 20 for k in range(21)]]
    body.append(poly(arc, BLUE, width=1.2, close=False))
    body.append(label(ax + 22, ay + 82, "φ", cls="xs"))
    body.append(ring(ax, ay, PURPLE, 7.0, 1.4))
    body.append(dot(ax, ay, PURPLE, 2.2))
    body.append(label(ax + 14, ay - 8, "n̂ = a₁ × b₁ / |a₁ × b₁|, out of the page", cls="xs", anchor="start"))
    lines = ["cos φ = a₁ · b₁. Differentiate, and divide by |a₁ × b₁| = sin φ:",
             "dφ/dt = (ω_b − ω_a) · n̂   — exactly, with no correction factor.",
             "C = cone − φ ≥ 0, so the row is angular_row(−n̂): the hinge's",
             "upper stop, with n̂ for its axis."]
    for i, s in enumerate(lines):
        body.append(label(26, ay + L + 60 + i * 15, s, cls="xs mono" if i == 1 else "xs", anchor="start"))

    # ---- RIGHT: rebound vs sweeps --------------------------------------------------
    px0, py0, pw, ph = 560, 58, 360, 220
    body.append(label(px0 - 30, 30, "A rod on a socket at its end, swung into its cone", cls="sm", anchor="start"))
    body.append(frame(px0, py0, pw, ph))
    lo, hi = 1e-4, 2.0
    for e in (1e-4, 1e-3, 1e-2, 1e-1, 1.0):
        yy = logy(e, lo, hi, py0, ph)
        body.append(rule(px0, yy, px0 + pw, yy, cls="grid", width=0.6))
        body.append(label(px0 - 6, yy + 4, f"{e:g}", cls="xs mono", anchor="end"))

    def X(n):
        return px0 + 20 + math.log2(n) / 5.0 * (pw - 40)

    for n in (1, 2, 4, 8, 16, 32):
        body.append(label(X(n), py0 + ph + 16, str(n), cls="xs mono"))
    body.append(label(px0 + pw / 2, py0 + ph + 32, "sweeps per step", cls="xs muted"))
    pred = [(X(n), logy(RHO_ROD ** n, lo, hi, py0, ph)) for n in (1, 2, 4, 8, 16, 32) if RHO_ROD ** n >= lo]
    body.append(poly(pred, PURPLE, width=1.4, dash="5 4", close=False))
    for n, v in REBOUND:
        if v >= lo:
            body.append(dot(X(n), logy(v, lo, hi, py0, ph), AMBER, 3.8))
    body.extend(legend(px0, py0 + ph + 54, [
        (PURPLE, f"ρⁿ, ρ = m·d² / I_pivot = {RHO_ROD:.4f} for this rod"),
        (AMBER, "measured rebound speed / arrival speed, restitution zero"),
    ]))
    t, th = tbl(px0, py0 + ph + 54 + 2 * 17 + 22, ["socket through the centre", "overshoot"],
                [["speculative, worst", "1.19e-07 rad"], ["reactive, mean", "0.4977 of ω·h"],
                 ["reactive, worst", "0.9952 of ω·h"]], [220, 140],
                title="200 arrival phases, 3 rad/s, 40° cone")
    body.extend(t)
    H = max(ay + L + 60 + 4 * 15, py0 + ph + 54 + 2 * 17 + 22 + th) + 12
    return svg(uid, W, H,
               "The swing cone's row, and its rebound against rho to the n",
               "Left: a cone about axis a1 with the bone b1 on it at swing angle phi, and n-hat "
               "out of the page; the swing row is angular_row(-n). Right: on log axes, the "
               "rebound off the cone at 1 to 32 sweeps lying near rho to the n, and a table of "
               "speculative and reactive overshoot.", body)


# ===========================================================================
# FIGURE 4 — the twist row is the half-way axis                             (§5)
# ===========================================================================

def fig4():
    uid = "g4"
    W = 980
    body = [cmarker(uid, "blue", BLUE), cmarker(uid, "green", GREEN), cmarker(uid, "grey", GREY)]
    body.append(label(26, 30, "What the twist's rate is about", cls="sm", anchor="start"))
    cx, cy, R = 170, 70, 120
    phi = math.radians(80)
    t = (0.0, -1.0)
    c = (math.sin(phi), -math.cos(phi))
    hh = (math.sin(phi / 2), -math.cos(phi / 2))

    def P(v, k=1.0):
        return cx + v[0] * R * k, cy - v[1] * R * k

    for v, col, name, k in ((t, GREY, "a₁ (t)", 1.0), (c, BLUE, "b₁ (c)", 1.0),
                            (hh, GREEN, "(a₁ + b₁)/(1 + a₁·b₁)", 1.0 / math.cos(phi / 2))):
        x, y = P(v, k)
        body.append(carrow(cx, cy, x, y, uid, {GREY: "grey", BLUE: "blue", GREEN: "green"}[col], col, width=1.9))
        body.append(label(x + 8, y + 14, name, cls="xs", anchor="start"))
    lines = ["The swing's own angular velocity is 2h × ḣ — perpendicular to",
             "h, so it has no component along a₁ + b₁. Every other bit of the",
             "relative spin along that axis is twist:",
             "",
             "    dθ/dt = (ω_b − ω_a) · (a₁ + b₁) / (1 + a₁·b₁)",
             "",
             "the half-way axis over cos(φ/2). A row about the bone is wrong",
             "by −(ω⊥ · a₁)/(1 + cos φ): at most |ω⊥|·tan(φ/2) — measured to",
             f"{BOUND_RATIO:.4f} of that bound."]
    for i, s in enumerate(lines):
        if s:
            body.append(label(26, cy + R + 36 + i * 15, s.strip(), cls="xs mono" if i == 4 else "xs",
                              anchor="start"))

    # ---- RIGHT: error by swing bin -----------------------------------------------------
    px0, py0, pw, ph = 520, 58, 420, 230
    body.append(label(px0 - 30, 30, "40,000 random joints: row J·V against dθ/dt, by swing", cls="sm",
                      anchor="start"))
    body.append(frame(px0, py0, pw, ph))
    lo, hi = 1e-4, 10.0
    for e in (1e-4, 1e-3, 1e-2, 1e-1, 1.0, 10.0):
        yy = logy(e, lo, hi, py0, ph)
        body.append(rule(px0, yy, px0 + pw, yy, cls="grid", width=0.6))
        body.append(label(px0 - 6, yy + 4, f"{e:g}", cls="xs mono", anchor="end"))
    slot = pw / len(BINS)
    bw = slot * 0.22
    for i, b in enumerate(BINS):
        x = px0 + i * slot + slot * 0.14
        for k, (vals, col) in enumerate(((ERR_HALF, GREEN), (ERR_BONE, RED), (ERR_CONE, AMBER))):
            v = vals[i]
            top = logy(v, lo, hi, py0, ph)
            body.append(box(x + k * bw, top, bw - 2, py0 + ph - top, col, opacity=0.7, rx=1, width=0.8))
        body.append(label(px0 + i * slot + slot / 2, py0 + ph + 16, b, cls="xs mono"))
    body.append(label(px0 + pw / 2, py0 + ph + 32, "swing (degrees)", cls="xs muted"))
    body.extend(legend(px0, py0 + ph + 54, [
        (GREEN, "half-way / cos(φ/2) — the engine's row: float-difference resolution"),
        (RED, "about the bone, b₁"),
        (AMBER, "about the cone's axis, a₁"),
        (None, "worst |row − rate| / max(1, |rate|); at exactly zero swing all three agree to 9.5e-07"),
    ]))
    H = max(cy + R + 36 + 9 * 15, py0 + ph + 54 + 4 * 17) + 12
    return svg(uid, W, H,
               "The twist row's axis, and the error of the obvious ones",
               "Left: the cone axis, the bone, and the twist row's axis — the half-way vector "
               "scaled by 1/cos(phi/2) — with the formula for the twist rate and the bound on "
               "the bone row's error. Right: on a log axis, the worst relative error of three "
               "candidate rows in seven swing bins: the half-way row near 1e-3 everywhere, the "
               "bone and cone-axis rows above 1 beyond a degree of swing.", body)


# ===========================================================================
# FIGURE 5 — Codman's paradox, and what the naive row does in a solver      (§5)
# ===========================================================================

def fig5():
    uid = "g5"
    W = 980
    body = [cmarker(uid, "amber", AMBER)]
    body.append(label(26, 30, "Codman: forward, out to the side, down again", cls="sm", anchor="start"))
    cx, cy, R = 180, 190, 120
    yaw, pitch = math.radians(-40), math.radians(18)

    def V(p):
        x, y, z = p
        x1 = x * math.cos(yaw) + z * math.sin(yaw)
        z1 = -x * math.sin(yaw) + z * math.cos(yaw)
        y1 = y * math.cos(pitch) - z1 * math.sin(pitch)
        return cx + x1 * R, cy - y1 * R

    body.append(f'<circle cx="{cx}" cy="{cy}" r="{R}" fill="none" class="grid" stroke-width="1"/>')
    eq = [V((math.cos(a), 0, math.sin(a))) for a in [2 * math.pi * k / 96 for k in range(97)]]
    body.append(poly(eq, GREY, width=0.8, dash="3 4", close=False))
    legs = []
    for k in range(91):
        f = math.radians(k)
        legs.append((0.0, -math.cos(f), math.sin(f)))
    for k in range(91):
        f = math.radians(k)
        legs.append((math.sin(f), 0.0, math.cos(f)))
    for k in range(91):
        f = math.radians(k)
        legs.append((math.cos(f), -math.sin(f), 0.0))
    body.append(poly([V(p) for p in legs], AMBER, width=2.2, close=False))
    for i in (60, 150, 240):
        x0, y0 = V(legs[i])
        x1, y1 = V(legs[i + 4])
        body.append(carrow(x0, y0, x1, y1, uid, "amber", AMBER, width=2.2))
    # Labels sit OUTSIDE the sphere, pushed radially out from its centre:
    # placed beside their dots they landed on the path (check-page.js).
    for p, name in (((0, -1, 0), "down"), ((0, 0, 1), "forward"), ((1, 0, 0), "out to the side")):
        x, y = V(p)
        body.append(dot(x, y, AMBER, 3.4))
        ox, oy = x - cx, y - cy
        n = math.hypot(ox, oy) or 1.0
        lx, ly = cx + ox / n * (R + 18), cy + oy / n * (R + 18) + 4
        body.append(label(lx, ly, name, cls="xs", anchor="start" if ox >= 0 else "end"))
    lines = ["Carried round with NO spin about itself, the arm comes home",
             f"turned {abs(CODMAN):.3f}° about its own axis — the solid angle of the",
             "octant it enclosed, π/2. A circle at swing φ encloses 2π(1 − cos φ)."]
    for i, s in enumerate(lines):
        body.append(label(26, cy + R + 34 + i * 15, s, cls="xs", anchor="start"))

    # ---- RIGHT: circles, then the solver --------------------------------------------------
    rx = 470
    t, th = tbl(rx, 52, ["circle at swing", "twist after one loop", "2π(1 − cos φ)"],
                [[f"{p}°", f"{v:.3f}°", f"{math.degrees(2 * math.pi * (1 - math.cos(math.radians(p)))):.3f}°"]
                 for p, v in CIRCLES],
                [150, 170, 150], title="Measured by carrying a body round each loop, 8,000 steps")
    body.extend(t)
    y2 = 52 + th + 44
    body.append(label(rx, y2 - 12, "A rod circling the rim of a 60° cone, twist stop at ±20°", cls="sm",
                      anchor="start"))
    peak = 190.0
    bx0, bw = rx + 230, 200
    for i, (name, deg_, col) in enumerate(SOLVER_TWIST):
        y = y2 + 8 + i * 34
        w = deg_ / peak * bw
        body.append(box(bx0, y, w, 20, col, opacity=0.6, rx=2))
        body.append(label(rx, y + 14, name, cls="xs", anchor="start"))
        body.append(label(bx0 + w + 8, y + 14, f"{deg_:.2f}°", cls="xs mono", anchor="start"))
    stop_x = bx0 + 20.0 / peak * bw
    body.append(cline(stop_x, y2, stop_x, y2 + 3 * 34 + 8, PURPLE, width=1.2, dash="4 3"))
    body.append(label(stop_x, y2 + 3 * 34 + 22, "the stop, 20°", cls="xs muted"))
    txt, nh = para(rx, y2 + 3 * 34 + 44,
                   "The bone row never sees the twist move — the rod has no spin about itself — so "
                   "only the position pass, which measures the TRUE twist, pushes back. It "
                   "removes β of the violation a step while the circling adds α̇(1 − cos φ)·h, and "
                   f"settles where they balance: {PREDICTED_LAG:.2f}° past the stop predicted, "
                   f"{29.08 - 20.0:.2f}° measured. With no position pass it walks straight through.",
                   cols=80)
    body.extend(txt)
    H = max(cy + R + 34 + 3 * 15, y2 + 3 * 34 + 44 + nh) + 12
    return svg(uid, W, H,
               "Codman's paradox, and the naive twist row in a solver",
               "Left: a sphere with the octant path an arm takes, down, forward, out to the side "
               "and down again, which leaves it turned 90 degrees about itself. Right: a table of "
               "circles at four swings, each leaving exactly 2 pi (1 - cos phi) of twist; and "
               "bars of the worst twist a rod reaches against a 20-degree stop under the half-way "
               "rows, the bone rows with split impulse, and the bone rows with no position pass.",
               body)


# ===========================================================================
# FIGURE 6 — a capsule as a limb, and which pairs touch                     (§6)
# ===========================================================================

def fig6():
    uid = "g6"
    W = 980
    body = []
    body.append(label(26, 30, "ρ about the proximal joint: capsule against Dempster", cls="sm", anchor="start"))
    x0, top, bwmax = 150, 56, 260
    for i, (name, cap, dem) in enumerate(DEMPSTER):
        y = top + i * 56
        body.append(label(26, y + 14, name, cls="xs", anchor="start"))
        for k, (v, col, tag) in enumerate(((cap, AMBER, "capsule"), (dem, PURPLE, "Dempster"))):
            w = v * bwmax
            body.append(box(x0, y + k * 22, w, 16, col, opacity=0.55, rx=2))
            body.append(label(x0 + w + 8, y + k * 22 + 12, f"{v:.3f}  {tag}", cls="xs mono", anchor="start"))
    body.append(rule(x0, top - 6, x0, top + 4 * 56 - 10, cls="grid"))
    txt, nh = para(26, top + 4 * 56 + 14,
                   "A capsule puts the centre of mass at the middle and too little of its mass far "
                   "from it, so every limb's ρ comes out about 0.1 high: slower convergence against "
                   "its pin, more dissipation per step. The engine keeps capsules — the error is "
                   "in the direction of settling — and says so.", cols=70)
    body.extend(txt)

    rx = 500
    rows = [[n, str(l), str(f), str(fr), f"{d:.1f}", "EXCLUDED" if l <= 2 else "collides"] for n, l, f, fr, d in TOUCH]
    t, th = tbl(rx, 52, ["pair", "links", "falls", "frames", "deepest mm", "two-links rule"], rows,
                [132, 46, 46, 56, 84, 96], title="Twelve trips from the jog: which pairs touched")
    body.extend(t)
    y2 = 52 + th + 36
    t2, th2 = tbl(rx, y2, ["rule, same falls", "frames > 20 mm", "deepest at rest"],
                  [[n, str(fr), f"{r:.1f} mm"] for n, _, fr, r in RULES],
                  [190, 130, 140], title="Does the overlap persist?")
    body.extend(t2)
    txt2, nh2 = para(rx, y2 + th2 + 20,
                     "At rest — bind pose, the jog, 7.7's walk — no unjointed pair comes within 4 cm, "
                     "so the default excludes exactly the ten jointed pairs. Two links excludes "
                     "thirteen more, and three of them are the pairs that touch most. The default's "
                     "61.4 mm is a chest lying on an arm, which is §8's problem, not the filter's.",
                     cols=80)
    body.extend(txt2)
    H = max(top + 4 * 56 + 14 + nh, y2 + th2 + 20 + nh2) + 12
    return svg(uid, W, H,
               "Capsules against Dempster's segments, and the self-collision audit",
               "Left: bars of the lever ratio rho about the proximal joint for four limbs, capsule "
               "near 0.77 and Dempster near 0.66. Right: a table of the pairs of parts that "
               "touched in twelve falls, with the links between them and whether the two-links "
               "rule would exclude them; and a comparison of how long overlaps persist under the "
               "two-links rule and the default.", body)


# ===========================================================================
# FIGURE 7 — the bug                                                         (§7)
# ===========================================================================

def fig7():
    uid = "g7"
    W = 980
    body = [cmarker(uid, "green", GREEN), cmarker(uid, "red", RED), cmarker(uid, "grey", GREY)]
    body.append(label(26, 30, "A shin loaded into its stop, and 8.11's position pass", cls="sm", anchor="start"))
    kx, ky = 250, 150
    body.append(capsule2d(kx, ky - 90, kx, ky - 8, 12, GREY, width=1.6, opacity=0.15))
    body.append(label(kx + 18, ky - 60, "thigh, fixed", cls="xs muted", anchor="start"))
    body.append(capsule2d(kx - 12, ky, kx - 170, ky, 10, BLUE, width=1.6, opacity=0.15))
    body.append(label(kx - 90, ky + 30, "shin, lying back", cls="xs muted"))
    body.append(dot(kx, ky, AMBER, 4.0))
    body.append(carrow(kx - 150, ky + 8, kx - 150, ky + 52, uid, "grey", GREY, width=1.6))
    body.append(label(kx - 144, ky + 60, "g", cls="xs", anchor="start"))
    # stops
    body.append(cline(kx, ky, kx - 200, ky, GREEN, width=1.0, dash="5 4"))
    body.append(label(kx - 200, ky - 18, "lower stop: C = θ ≥ 0", cls="xs", anchor="start"))
    # The upper stop: 150 degrees round from the shin's rest direction (left),
    # which is 30 degrees above the horizontal on the other side of the knee.
    ux, uy = math.cos(math.radians(30)), math.sin(math.radians(30))
    body.append(cline(kx, ky, kx + 130 * ux, ky - 130 * uy, GREY, width=1.0, dash="2 4"))
    body.append(label(kx + 130 * ux + 6, ky - 130 * uy + 4, "upper stop, 150° round", cls="xs muted",
                      anchor="start"))
    t, th = tbl(26, 250, ["row", "C rad", "bias", "pseudo impulse"],
                [[n, f"{c:+.6f}", f"{b:.5f}", f"{p:+.6f}"] for n, c, b, p in PASS_ROWS],
                [110, 100, 70, 110], title="Inside 8.11's pass, five seconds in")
    body.extend(t)
    txt, nh = para(26, 250 + th + 18,
                   "The upper stop is not violated, so its bias is zero — and 8.11 solved it against "
                   "that zero, as a hard 'no pseudo-velocity toward me'. It cancels the lower stop's "
                   f"correction exactly: the knee's pseudo spin after the pass is {PSEUDO_SPIN:.1e} rad/s. "
                   "8.12 gives a satisfied row its speculative target, −C/h.", cols=72)
    body.extend(txt)

    # ---- RIGHT: the knee over ten seconds ------------------------------------------------
    px0, py0, pw, ph = 560, 56, 360, 230
    body.append(frame(px0, py0, pw, ph))
    ylo, yhi = -75.0, 5.0

    def Y(v):
        return py0 + (yhi - v) / (yhi - ylo) * ph

    def X(t):
        return px0 + t / 10.0 * pw

    for v in (0, -20, -40, -60):
        body.append(rule(px0, Y(v), px0 + pw, Y(v), cls="grid", width=0.6))
        body.append(label(px0 - 6, Y(v) + 4, f"{v}°", cls="xs mono", anchor="end"))
    for t in (0, 2, 4, 6, 8, 10):
        body.append(label(X(t), py0 + ph + 16, f"{t} s", cls="xs mono"))
    body.append(label(px0 + pw / 2, 30, "Knee angle; the stop is at 0°", cls="sm"))
    for name, vals, col, dash in KNEE:
        pts = [(X(t), Y(v)) for t, v in zip(KNEE_T, vals)]
        body.append(poly(pts, col, width=1.8, dash=dash, close=False))
        for x, y in pts[1:]:
            body.append(dot(x, y, col, 2.6))
    # THE INSET. At −75° to +5° the warm 8.11 line (−0.1691°) is drawn exactly
    # under the fixed pass's 0°, which hides the figure's point; magnify the
    # top 2.5°, dropping — not clamping — whatever leaves it.
    iy0, ih = 250 + th + 18 + nh + 44, 110
    ix0, iw = 70, 390
    body.append(label(26, iy0 - 14, "The same, magnified: the top 2.5°", cls="sm", anchor="start"))
    body.append(frame(ix0, iy0, iw, ih))
    ilo, ihi = -2.5, 0.5

    def IY(v):
        return iy0 + (ihi - v) / (ihi - ilo) * ih

    def IX(t):
        return ix0 + t / 10.0 * iw

    for v in (0.0, -1.0, -2.0):
        body.append(rule(ix0, IY(v), ix0 + iw, IY(v), cls="grid", width=0.6))
        body.append(label(ix0 - 6, IY(v) + 4, f"{v:g}°", cls="xs mono", anchor="end"))
    for name, vals, col, dash in KNEE:
        pts = [(IX(t), IY(v)) for t, v in zip(KNEE_T, vals) if ilo <= v <= ihi]
        if len(pts) >= 2:
            body.append(poly(pts, col, width=1.8, dash=dash, close=False))
        for x, y in pts[1:]:
            body.append(dot(x, y, col, 2.6))
    body.append(label(IX(10) + 6, IY(-0.1691) + 4, "−0.1691", cls="xs mono", anchor="start"))
    body.append(label(IX(10) + 6, IY(-1.8915) + 4, "−1.8915", cls="xs mono", anchor="start"))
    for t in (0, 5, 10):
        body.append(label(IX(t), iy0 + ih + 14, f"{t} s", cls="xs mono"))
    body.extend(legend(px0, py0 + ph + 44, [
        (RED, "8.11's pass, no warm start: gives way without bound (−70.7° at 10 s)"),
        (GREEN, "8.12's pass, no warm start: a steady lag, −1.8915°"),
        (RED, "8.11's pass, warm: frozen at −0.1691°, never repaired"),
        (GREEN, "8.12's pass, warm: 0.0000°, as Baumgarte always managed"),
    ]))
    H = max(iy0 + ih + 26, py0 + ph + 44 + 4 * 17) + 12
    return svg(uid, W, H,
               "A knee loaded into its stop, and the position pass that could not repair it",
               "Left: a fixed thigh and a shin lying back, pushed by gravity into the lower stop, "
               "with the upper stop 150 degrees away; a table of the two rows inside 8.11's "
               "position pass, whose pseudo impulses are equal and cancel. Right: the knee angle "
               "over ten seconds under 8.11's and 8.12's passes, warm and cold.", body)


# ===========================================================================
# FIGURE 8 — what eight sweeps do to a ragdoll                              (§8)
# ===========================================================================

def fig8():
    uid = "g8"
    W = 980
    body = []
    cols = [RED, AMBER, AMBER, GREEN, GREY]
    panels = [("peak joint gap in a fall", PEAK, 1.0, 200.0, "mm"),
              ("joint gap at rest after it", REST, 1e-3, 100.0, "mm"),
              ("hung from one hand, at rest", HANG, 1e-4, 10.0, "mm"),
              ("chest lying on an arm, overlap", CRUSH, 0.1, 100.0, "mm")]
    pw, ph = 430, 150
    for p, (title, vals, lo, hi, unit) in enumerate(panels):
        x0 = 26 + (p % 2) * 480
        y0 = 40 + (p // 2) * (ph + 70)
        body.append(label(x0, y0, title, cls="sm", anchor="start"))
        bx = x0 + 150
        bwid = pw - 150 - 70
        body.append(rule(bx, y0 + 10, bx, y0 + 10 + 5 * 26, cls="grid"))
        for i, (name, v) in enumerate(zip(BUDGETS, vals)):
            y = y0 + 14 + i * 26
            body.append(label(x0, y + 13, name, cls="xs", anchor="start"))
            if v <= 0.0:
                body.append(label(bx + 6, y + 13, "0.0 — nothing overlaps", cls="xs mono", anchor="start"))
                continue
            w = max(1.5, logx(max(v, lo), lo, hi, 0, bwid))
            body.append(box(bx, y + 2, w, 16, cols[i], opacity=0.6, rx=2))
            text = f"{v:.3f}" if v < 1 else f"{v:.1f}"
            body.append(label(bx + w + 6, y + 14, text, cls="xs mono", anchor="start"))
        body.append(label(bx, y0 + 10 + 5 * 26 + 16, f"log scale, {lo:g} to {hi:g} {unit}", cls="xs muted",
                          anchor="start"))
    H = 40 + 2 * (ph + 70) - 20
    return svg(uid, W, H,
               "Joint gaps and a chest-on-arm overlap against the solver's budget",
               "Four panels of log-scale bars for five budgets — 8, 16 and 32 sweeps, eight "
               "sub-steps of one sweep, and a 32-by-4 control: the peak joint gap in a fall, the "
               "gap at rest after it, the gap with the character hanging from one hand, and the "
               "overlap of a chest lying on a forearm.", body)


# ===========================================================================
# FIGURE 9 — handing a character to the solver                              (§9)
# ===========================================================================

def fig9():
    uid = "g9"
    W = 980
    body = [cmarker(uid, "green", GREEN), cmarker(uid, "purple", PURPLE)]
    body.append(label(26, 30, "The chord is the velocity", cls="sm", anchor="start"))
    # a curve and a chord
    def C(u):
        return 50 + u * 300, 190 - 110 * math.sin(u * 2.6)
    pts = [C(k / 60) for k in range(61)]
    body.append(poly(pts, GREY, width=1.6, close=False))
    u0, u1 = 0.35, 0.62
    a = C(u0)
    b = C(u1)
    body.append(dot(*a, BLUE, 4.0))
    body.append(dot(*b, BLUE, 4.0))
    body.append(label(a[0] - 10, a[1] - 10, "x(n−1)", cls="xs mono", anchor="end"))
    body.append(label(b[0] + 8, b[1] + 18, "x(n)", cls="xs mono", anchor="start"))
    body.append(carrow(a[0], a[1], b[0], b[1], uid, "green", GREEN, width=2.0))
    um = 0.5 * (u0 + u1)
    m = C(um)
    e = 1e-3
    d = ((C(um + e)[0] - C(um - e)[0]) / (2 * e), (C(um + e)[1] - C(um - e)[1]) / (2 * e))
    k = 0.12
    body.append(cline(m[0] - d[0] * k, m[1] - d[1] * k, m[0] + d[0] * k, m[1] + d[1] * k, PURPLE, width=1.2, dash="5 4"))
    body.append(label(m[0] + 30, m[1] - 34, "the tangent at the midpoint: parallel", cls="xs", anchor="start"))
    lines = ["Semi-implicit Euler moves a body by x(n) = x(n−1) + h·v(n).",
             "So the chord (x(n) − x(n−1))/h is not an approximation of the",
             "velocity the solver should inherit — it IS that velocity.",
             f"Against the clip's own: {CHORD_MID:.4f} m/s at the step's middle,",
             f"{CHORD_END:.4f} m/s at its end — second order, and first.",
             "Rotation: the linearised spin inverts to the Rodrigues vector",
             f"(2/h)·Δq.v/Δq.w; it lands to {LAND_ROD:.2e} rad, the logarithm",
             f"misses by {LAND_LOG:.2e} against θ³/12 = {THETA3:.2e}."]
    for i, s in enumerate(lines):
        body.append(label(26, 250 + i * 15, s, cls="xs", anchor="start"))

    # ---- MIDDLE: the momentum ------------------------------------------------------------
    mx = 420
    body.append(label(mx, 30, "Tripped at 2.5 m/s", cls="sm", anchor="start"))
    for i, (name, f05, f15, col) in enumerate(FWD):
        y = 56 + i * 44
        w = f05 / 1.3 * 100
        body.append(box(mx + 110, y, max(w, 1.5), 18, col, opacity=0.6, rx=2))
        body.append(label(mx, y + 13, name, cls="xs", anchor="start"))
        body.append(label(mx + 116 + max(w, 1.5), y + 13, f"{f05:.3f} m", cls="xs mono", anchor="start"))
    body.append(label(mx, 56 + 2 * 44 + 4, "centre of mass forward, first 0.5 s", cls="xs muted", anchor="start"))
    txt, nh = para(mx, 56 + 2 * 44 + 30,
                   f"Momentum handed over: {MOMENTUM:.2f} kg·m/s = 80 kg × {COM_SPEED:.3f} m/s, "
                   "exactly the clip's. At rest, the character stops dead and topples.", cols=44)
    body.extend(txt)

    # ---- RIGHT: the residual per joint ------------------------------------------------------
    rx, top = 700, 56
    body.append(label(rx, 30, "Each joint at the instant of handoff", cls="sm", anchor="start"))
    peak = 0.08
    bw = 150
    for i, (name, meas, pred) in enumerate(RESID):
        y = top + i * 26
        w = meas / peak * bw
        body.append(label(rx, y + 13, name, cls="xs", anchor="start"))
        body.append(box(rx + 84, y + 2, w, 16, BLUE, opacity=0.55, rx=2))
        px = rx + 84 + pred / peak * bw
        body.append(halo(px, y + 10, PURPLE, 6.5))
        body.append(dot(px, y + 10, PURPLE, 2.4))
        body.append(label(rx + 84 + bw + 16, y + 13, f"{meas * 1000:.1f}", cls="xs mono", anchor="end"))
    body.append(label(rx + 84 + bw + 16, top - 4, "mm/s", cls="xs muted", anchor="end"))
    body.extend(legend(rx, top + 8 * 26 + 24, [
        (BLUE, "anchor velocity mismatch, measured"),
        (PURPLE, "½h·|ω_b×(ω_b×r_b) − ω_a×(ω_a×r_a)|"),
    ]))
    txt2, nh2 = para(rx, top + 8 * 26 + 24 + 2 * 17 + 10,
                     f"Worst disagreement {RESID_WORST * 100:.2f}%. The chord of a rotation is not "
                     "its tangent; the solver's first sweep removes the difference.", cols=46)
    body.extend(txt2)
    H = max(250 + 8 * 15, 56 + 2 * 44 + 30 + nh, top + 8 * 26 + 24 + 2 * 17 + 10 + nh2) + 12
    return svg(uid, W, H,
               "The chord as the handed-over velocity, the momentum it keeps, and the joint residual",
               "Left: a curved path with two consecutive positions and the chord between them, "
               "parallel to the tangent at the midpoint. Middle: the centre of mass travels 1.179 m "
               "forward in half a second with the chord and 0.010 m handed over at rest. Right: for "
               "eight limb joints, the measured anchor velocity mismatch at handoff against the "
               "centripetal prediction.", body)


# ===========================================================================
# FIGURE 10 — handing it back, and the fight                                (§11, §12)
# ===========================================================================

def fig10():
    uid = "g10"
    W = 980
    body = []
    body.append(label(26, 30, "The return, after a trip and three seconds on the floor", cls="sm", anchor="start"))
    y = 56
    for name, good, gv, bad, bv, unit in RETURN:
        body.append(label(26, y + 12, name, cls="xs", anchor="start"))
        peak = max(gv, bv)
        for k, (tag, v, col) in enumerate(((good, gv, GREEN), (bad, bv, RED))):
            yy = y + 20 + k * 22
            w = max(1.5, v / peak * 180)
            body.append(box(160, yy, w, 16, col, opacity=0.6, rx=2))
            body.append(label(160 + w + 8, yy + 12, f"{v:g} {unit}   {tag}", cls="xs mono", anchor="start"))
        y += 76
    txt, nh = para(26, y + 4,
                   f"The worst joint crosses {WORST_ARC:.1f}° of arc on the way back, where nlerp lags "
                   f"slerp by up to {NLERP_LAG:.2f}° — a hundred times what 7.7 found between two keys "
                   "of a clip. The return blends with transform_blend_slerp.", cols=72)
    body.extend(txt)

    rx = 500
    body.append(label(rx, 30, "Jogging into a 5 kg crate: three ways to follow the clip", cls="sm", anchor="start"))
    t, th = tbl(rx, 52, ["bodies are", "velocity lie", "limb in crate", "crate top", "let go at"],
                [[n, f"{l:.3f} m/s", f"{p:.1f} mm", f"{c:.2f} m/s", f"{r:.2f} m/s"] for n, l, p, c, r, _ in FIGHT],
                [170, 76, 80, 70, 70], title="Two seconds, then released")
    body.extend(t)
    txt2, nh2 = para(rx, 52 + th + 20,
                     "Steered bodies tell the truth: their velocity is their motion, to the bit. A "
                     "teleported body's velocity is whatever the solver last left in it — 20.6 m/s of "
                     "accumulated junk here — and that is what a contact sees and what the next "
                     "handoff inherits. Giving it the chord fixes the lie and not the fight: the "
                     "solve still pushes the limb out of the crate and the teleport puts it back, "
                     "159 mm deep. The 69.7 mm of the steered limb is one step of a fast leg's "
                     "arrival, 8.10's depth.", cols=80)
    body.extend(txt2)
    H = max(y + 4 + nh, 52 + th + 20 + nh2) + 12
    return svg(uid, W, H,
               "The return to animation, and three ways of making bodies follow a clip",
               "Left: three pairs of bars — the first-frame jump when blending from the read-back "
               "pose against snapping, the pelvis slide with and without realignment, and the "
               "worst bone length change under a local and a model-space blend. Right: a table of "
               "steered, teleported, and teleported-with-chord bodies: the velocity lie, how deep a "
               "limb goes into a crate, the crate's top speed and the bodies' speed when let go.",
               body)


def main():
    figs = [fig1, fig2, fig3, fig4, fig5, fig6, fig7, fig8, fig9, fig10]
    for i, f in enumerate(figs, start=1):
        write(f"l812_fig{i}.svg", f())


if __name__ == "__main__":
    main()
