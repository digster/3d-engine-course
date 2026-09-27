#!/usr/bin/env python3
"""scratch/figs_813.py — Lesson 8.13's diagrams.

Same rules as 5.1-8.12's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - filenames numbered by PAGE ORDER
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - LEGENDS AND ANNOTATION BOXES GO OUTSIDE THE PLOT
  - panel heights are COMPUTED, never guessed
  - one <text> per table column: SVG collapses runs of spaces (8.11 figure 2)
  - a curve is DROPPED beyond its axis, never clamped onto it (8.8)

Every number below is transcribed from scratch/verify_813.log. Nothing here is
estimated, and the harness section each block came from is named above it. The
two exceptions are named where they are used: figure 11's spiral is DRAWN from
the recurrence the harness measured (its endpoints are measured), and figure
12's "draft recover" row is an observation on the engine before §11's fix.

*** WRITTEN IN LITERAL UNICODE THROUGHOUT, NEVER \\uXXXX. ***

THE COLOUR RULE, inherited from 8.4 through 8.12, with this lesson's reading:
  GREEN  = the controller, and the decision that works — the lower bound, the
           original clipped, the snap, steering, carrying by transform
  RED    = the failure — the rigid capsule, the teleport, the remainder, the
           upper bound, carrying by velocity
  AMBER  = the round bottom: edges, curbs, and what a capsule does for free
  BLUE   = surfaces, normals, and the query itself
  PURPLE = the closed form a measurement is checked against
  GREY   = geometry, axes, and discarded work
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


def ring(x, y, colour, r=3.6, width=1.3, dash=None):
    d = f' stroke-dasharray="{dash}"' if dash else ""
    return (f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r}" fill="none" stroke="{colour}"'
            f' stroke-width="{width}"{d}/>')


def halo(x, y, colour, r=7.0):
    """A wide pale disc under a prediction, so a measurement drawn exactly on
    top of it still shows both (8.11's lesson)."""
    return f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r}" fill="{colour}" fill-opacity="0.22"/>'


def capsule2d(x0, y0, x1, y1, r, colour, width=1.5, opacity=None, dash=None):
    """A 2D capsule outline: two tangents and two end arcs."""
    dx, dy = x1 - x0, y1 - y0
    ln = math.hypot(dx, dy)
    da = f' stroke-dasharray="{dash}"' if dash else ""
    fill = f'fill="{colour}" fill-opacity="{opacity}"' if opacity else 'fill="none"'
    if ln < 1e-6:
        return (f'<circle cx="{x0:.1f}" cy="{y0:.1f}" r="{r:.1f}" {fill} stroke="{colour}"'
                f' stroke-width="{width}"{da}/>')
    nx, ny = -dy / ln * r, dx / ln * r
    d = (f"M {x0 + nx:.2f},{y0 + ny:.2f} L {x1 + nx:.2f},{y1 + ny:.2f} "
         f"A {r:.2f} {r:.2f} 0 0 0 {x1 - nx:.2f},{y1 - ny:.2f} "
         f"L {x0 - nx:.2f},{y0 - ny:.2f} A {r:.2f} {r:.2f} 0 0 0 {x0 + nx:.2f},{y0 + ny:.2f} Z")
    return f'<path d="{d}" {fill} stroke="{colour}" stroke-width="{width}"{da}/>'


def polygon(points, colour, opacity=0.12, width=1.2):
    d = "M " + " L ".join(f"{x:.2f},{y:.2f}" for x, y in points) + " Z"
    return (f'<path d="{d}" fill="{colour}" fill-opacity="{opacity}" stroke="{colour}"'
            f' stroke-width="{width}"/>')


def write(name, text):
    with open(os.path.join(OUT, name), "w") as fh:
        fh.write(text)
    print(f"{name}: {len(text)} bytes")


# ===========================================================================
# MEASUREMENTS — every one from scratch/verify_813.log, section named
# ===========================================================================

R_CAP, SKIN, H = 0.30, 0.01, 1.0 / 60.0
RS = R_CAP + SKIN

# §A — the rigid capsule
RAMP = (1.9824, 1.9714)                          # mu 0.5, slid in 2 s, predicted
HANG = [(3.0, 0.0000), (1.0, 0.0000), (0.5, 0.0000), (0.2, -2.7300)]
HANG_FREE = -9.8100
HANG_T = (0.2757, 0.2725)                        # measured, g h / mu
STOP = (2.9098, 3.0581, 2.9098)                  # slid, v^2/2mu g, the discrete sum
LEDGE_SPEEDS = [0.5, 1.0, 2.0, 3.0, 5.0, 8.0]
LEDGE = [(0.10, "nuuuuu"), (0.20, "nnnnnu"), (0.30, "nnnnnn")]
JUMP = (1.20, 4.8522, 1.1599, 1.1596)            # asked, v0, apex, v0^2/2g - v0 h/2

# §B — the cast
NEWTON = [(0, 0.0000000000, 2.0594117082, 3.9223227028, 5.500e-01, None),
          (1, 0.5250490243, 0.0815022253, 3.3279779982, 2.495e-02, 0.08248),
          (2, 0.5495390376, 0.0014756908, 3.2026492786, 4.610e-04, 0.74044),
          (3, 0.5499998095, 0.0000006097, 3.2000010974, 1.905e-07, 0.89661)]
SKIN0_SHORT_MM = 1.93
FLAT = (0.4966656, 0.4966667)
RANDOM = dict(casts=9996, hits=6052, missed=0, late=0, grazes=3, beyond=67, of=6049, loose_mm=2.077,
              stop_mm=1.813, below=9.23e-07, mean=2.75, worst=7, gjk=26.97, upper_late=377,
              upper_mm=0.1656)
HIST = [2118, 1279, 4064, 2102, 370, 57, 6]
UNION = (11194, 20000, 0)
TRACE = [(90, 2, 2), (30, 2, 6), (10, 2, 34), (3, 2, 97), (1, 2, 237)]
SKINS = [("0", 133.7552, 2336.2699, 82, 3967), ("0.1 mm", 108.1389, 2244.3466, 290, 3967),
         ("1 mm", 1.0581, 3.3384, 0, 3968), ("1 cm", 10.0433, 11.1133, 0, 3969)]

# §C — corners (travel in the last second, mm; blocked moves)
CORNER = [("60°", "the remainder, last plane", 0.000, 60), ("60°", "the remainder, every plane", 0.000, 0),
          ("60°", "the original, every plane", 0.000, 0),
          ("120°", "the remainder, last plane", 572.358, 0), ("120°", "the remainder, every plane", 574.597, 1),
          ("120°", "the original, every plane", 0.000, 0)]
PARALLEL = (0.0000, 3.0000)                       # min_approach 0, 1e-4

# §D — slopes
RAMPS = [(10, 3.0000, 3.0463, 0.1763, 0.1763), (20, 3.0000, 3.1925, 0.3640, 0.3640),
         (30, 3.0000, 3.4641, 0.5773, 0.5774), (40, 3.0000, 3.9162, 0.8391, 0.8391)]
CREEP = [(10, 0.028389, 0.028391), (20, 0.055916, 0.055920), (30, 0.081746, 0.081750),
         (40, 0.105092, 0.105096)]
HOP = [(50, 0.257, 0.638, -0.145), (60, 0.099, 0.328, -0.179)]
STEEP_SLIDE = (4.3897, 4.3895)

# §E — the round bottom
CURB = (0.09183, 0.09080)
OVER = (0.21979, 0.21920, 0.2150)
TILT = [(0.000, 0.000, 0.000), (0.050, 9.280, 9.282), (0.100, 19.008, 18.819), (0.150, 28.938, 28.939),
        (0.200, 40.136, 40.178), (0.250, 53.718, 53.751)]
TILT_WORST = 0.189

# §F — snapping
SKIP = [(0.50, 0, 0), (1.00, 0, 0), (2.00, 0, 0), (2.30, 42, 0), (2.45, 57, 0), (3.00, 57, 0), (6.00, 59, 0)]
SKIP_V, SKIP_V0 = 2.3617, 0.2832
ROLL = (0.1165, 6.990)
CREST = [(3.0, 0, 0.353, 0), (6.0, 0, 0.706, 0), (7.5, 50, 0.883, 0), (10.0, 68, 1.177, 0), (14.0, 97, 1.648, 0)]
STAIRS_DOWN = ((54, 0), (0, 9, 0.0777))
LEDGE_DROP = [(0.30, 0, 0.1960), (0.50, 16, 0.0000)]

# §G — stepping up
STAIRS_UP = (8, 0, 1.267, 0.0759)
FMIN = 0.0908
TALLEST = (0.39092, 0.39080)
LEAST_F = [(0.15, 0.26553, 0.04615, 0.04632), (0.25, 0.30414, 0.08467, 0.08494),
           (0.40, 0.31000, 0.09089, 0.09080)]

# §H — tunnelling
TUNNEL = [(10, 0.0, 0.0), (20, 0.0, 0.0), (25, 15.5, 16.0), (30, 30.0, 30.0), (42, 50.0, 50.0),
          (60, 65.0, 65.0), (100, 79.0, 79.0)]
CONTROLLER_GAPS = [(10, 0.010011), (100, 0.010051), (1000, 0.010004), (10000, 0.010004)]

# §I — carrying
CARRY = [(1.0, "by transform", 1.50000, 1.00000, 1.00000, "0.000°"),
         (1.0, "by velocity", 1.58062, 1.05375, 1.05375, "0.025°"),
         (1.0, "not at all", 1.50000, 1.00000, 1.00000, "360.000°"),
         (2.0, "by transform", 1.50000, 1.00000, 1.00000, "0.000°"),
         (2.0, "by velocity", 1.66597, 1.11065, 1.11035, "0.100°"),
         (2.0, "not at all", 1.50000, 1.00000, 1.00000, "360.930°")]
LIFT_NONE = (-4.01679, -3.18939)

# §J — pushing
PUSH = [("steered (the chord)", "2.0000", "2.0000", "5.00", "0.3562", GREEN),
        ("teleported, asleep", "0.0000", "0.0000", "0.00", "0.0000", RED),
        ("teleported, awake", "0.0000", "1.9941", "138.20", "0.1649", RED),
        ("teleported, Baumgarte", "1.9998", "1.9998", "138.28", "0.3565", AMBER)]
PUSH_PRED = dict(coast=0.3566, sunk=138.33, out=0.1667, slop=5.0)
BOULDER = (0.0688, 0.0)
DRAFT_RECOVER = (2.94150, 3.5662)                # an observation on the draft engine, §11

# §K — the bill (timings: minima, from the canonical log)
BILL = [("walking on a flat floor", 0.964, 2.01, 1.99, 1.99, 15.05),
        ("up eight stairs", 3.263, 2.56, 2.77, 4.20, 52.61),
        ("pressed into a 60° corner", 6.929, 4.95, 13.83, 13.86, 124.50)]
CROWD = [(10, 1.808), (100, 7.538), (1000, 84.820), (10000, 843.911)]


# ===========================================================================
# FIGURE 1 — what Newton does with a player                                 (§1)
# ===========================================================================

def fig1():
    uid = "h1"
    W = 980
    body = [cmarker(uid, "red", RED), cmarker(uid, "grey", GREY), cmarker(uid, "blue", BLUE)]

    # ---- LEFT: the wall hang ------------------------------------------------------
    body.append(label(26, 30, "A rigid capsule, pushed into a wall mid-air", cls="sm", anchor="start"))
    wx, top, bot = 250, 54, 300
    body.append(box(wx, top, 26, bot - top, None, rx=0))
    body.append(label(wx + 13, bot + 16, "wall, μ = 0.6", cls="xs muted"))
    cx, cy, rr = wx - 34, 170, 34
    body.append(capsule2d(cx, cy - 46, cx, cy + 46, rr, RED, width=1.6, opacity=0.08))
    body.append(carrow(cx - 110, cy, cx - rr - 6, cy, uid, "grey", GREY, width=1.6))
    body.append(label(cx - 110, cy - 8, "stick: v_push", cls="xs", anchor="start"))
    body.append(carrow(wx - 2, cy + 36, wx - 2, cy - 44, uid, "red", RED, width=2.2))
    body.append(label(wx - 8, cy - 92, "friction ≤ μ·m·v_push", cls="xs", anchor="end"))
    body.append(carrow(cx, cy + 90, cx, cy + 128, uid, "blue", BLUE, width=1.8))
    body.append(label(cx + 8, cy + 128, "gravity: m·g·h a step", cls="xs", anchor="start"))
    lines = ["Each step the contact cancels v_push, so its",
             "normal impulse is m·v_push; friction may hold",
             "μ·m·v_push of vertical momentum against g's",
             "m·g·h. It HANGS once μ·v_push ≥ g·h."]
    for i, s in enumerate(lines):
        body.append(label(26, bot + 44 + i * 15, s, cls="xs", anchor="start"))
    t, th = tbl(26, bot + 44 + 4 * 15 + 24, ["stick into the wall", "v_y after 1 s"],
                [[f"{v:.1f} m/s", f"{vy:.4f}"] for v, vy in HANG] + [["μ = 0 instead", f"{HANG_FREE:.4f}"]],
                [150, 110], title="measured, m/s")
    body.extend(t)
    note_y = bot + 44 + 4 * 15 + 24 + th + 14
    body.append(label(26, note_y, f"threshold: {HANG_T[0]:.4f} m/s measured, g·h/μ = {HANG_T[1]:.4f}",
                      cls="xs mono", anchor="start"))

    # ---- MIDDLE: the ledge grid ----------------------------------------------------
    gx, gy = 318, 58
    body.append(label(gx, 30, "Walking into a ledge, stick held, 3 s", cls="sm", anchor="start"))
    cw, ch_ = 36, 28
    for j, v in enumerate(LEDGE_SPEEDS):
        body.append(label(gx + 60 + j * cw + cw / 2, gy + 12, f"{v:g}", cls="xs mono"))
    body.append(label(gx + 60 + 3 * cw, gy - 4, "stick speed, m/s", cls="xs muted"))
    for i, (h, cells) in enumerate(LEDGE):
        y = gy + 22 + i * ch_
        body.append(label(gx + 50, y + 18, f"{h * 100:.0f} cm", cls="xs mono", anchor="end"))
        for j, c in enumerate(cells):
            col = GREEN if c == "u" else RED
            body.append(box(gx + 60 + j * cw + 2, y + 2, cw - 4, ch_ - 4, col, rx=2, opacity=0.35, width=0.8))
            body.append(label(gx + 60 + j * cw + cw / 2, y + 18, "up" if c == "u" else "no", cls="xs"))
    ly = gy + 22 + 3 * ch_ + 26
    txt, nh = para(gx, ly, "Whether a ledge can be climbed depends on how hard the capsule is driven "
                   "into it: the round bottom meets the edge with a tilted normal, and the solver turns "
                   "part of the stick into lift. 20 cm goes up at 8 m/s and at nothing slower; the "
                   "controller climbs all three at every speed (§8).", cols=46)
    body.extend(txt)

    # ---- RIGHT: the ledger -----------------------------------------------------------
    rx = 616
    body.append(label(rx, 30, "What the design asks, and what each gives", cls="sm", anchor="start"))
    rows = [["stand on a 30° ramp, μ 0.5", ("slides 1.98 m in 2 s", "xs mono t-bad"), ("0.0000 m", "xs mono t-ok")],
            ["release the stick at 6 m/s", ("slides 2.91 m", "xs mono t-bad"), ("stops: 0 m", "xs mono t-ok")],
            ["jump at a wall, push into it", ("hangs: v_y = 0", "xs mono t-bad"), ("falls: −9.81", "xs mono t-ok")],
            ["walk into a 20 cm ledge", ("only at 8 m/s", "xs mono t-bad"), ("every speed", "xs mono t-ok")],
            ["dash at 42 m/s into 10 cm", ("through: 50%", "xs mono t-bad"), ("never", "xs mono t-ok")]]
    t, th = tbl(rx, 58, ["the design", "rigid capsule", "controller"], rows, [152, 112, 90])
    body.extend(t)
    txt, nh2 = para(rx, 58 + th + 22, "Every red cell is Newton being right. Friction is one number for a "
                    "pair of materials and the design wants two; momentum is conserved and the design "
                    "wants a stop; a step is sampled at instants and a wall can fall between two of them. "
                    "None of it is a bug in 8.10's solver.", cols=52)
    body.extend(txt)

    H = max(note_y + 12, ly + nh, 58 + th + 22 + nh2) + 12
    return svg(uid, W, H,
               "Five things a designer asks of a character, and what a rigid capsule does with them",
               "Left: a capsule pushed into a wall in mid-air, with the stick, friction and gravity "
               "drawn as arrows, and a table of its vertical speed after one second for four stick "
               "speeds — zero above 0.27 m/s. Middle: a grid of stick speed against ledge height "
               "saying whether the rigid capsule got up. Right: a table of five design requests, the "
               "rigid capsule's result in red and the controller's in green.", body)


# ===========================================================================
# FIGURE 2 — one query, five decisions, one owner                           (§2)
# ===========================================================================

def fig2():
    uid = "h2"
    W = 980
    body = [cmarker(uid, "grey", GREY), cmarker(uid, "green", GREEN), cmarker(uid, "blue", BLUE)]
    body.append(label(26, 30, "move_character: five stages, one query", cls="sm", anchor="start"))
    stages = [("1  CARRY", "by the ground body's step, as a transform", "§10"),
              ("2  RECOVER", "out of fixed and kinematic bodies, by EPA", "§11"),
              ("3  SIDEWAYS", "along the ground · off walls · step up", "§4 §5 §8"),
              ("4  VERTICAL", "land on floor · slide off steep · ceilings", "§5"),
              ("5  GROUND", "probe 2 skins down · snap if it dropped", "§6 §7")]
    bx, by, bw, bh, gap = 26, 50, 360, 46, 14
    for i, (name, sub, sec) in enumerate(stages):
        y = by + i * (bh + gap)
        body.append(hollow(bx, y, bw, bh, GREEN if i in (2, 3) else BLUE, width=1.4))
        body.append(label(bx + 12, y + 19, name, cls="sm", anchor="start"))
        body.append(label(bx + 12, y + 36, sub, cls="xs muted", anchor="start"))
        body.append(label(bx + bw - 10, y + 19, sec, cls="xs mono", anchor="end"))
        if i:
            body.append(carrow(bx + bw / 2, y - gap + 1, bx + bw / 2, y - 2, uid, "grey", GREY, width=1.2))
    qy = by + 5 * (bh + gap) + 8
    body.append(hollow(bx, qy, bw, 58, AMBER, width=1.4, dash="5 4"))
    body.append(label(bx + 12, qy + 20, "sweep(position, d): the only question", cls="sm", anchor="start"))
    body.append(label(bx + 12, qy + 38, "one cast per candidate obstacle, smallest t kept", cls="xs muted",
                      anchor="start"))
    body.append(label(bx + 12, qy + 52, "every stage above is policy written on its answer", cls="xs muted",
                      anchor="start"))

    # ---- RIGHT: the fixed step, and who owns what ------------------------------------
    rx = 440
    body.append(label(rx, 30, "One fixed step, and one owner per body", cls="sm", anchor="start"))
    steps = [("steer_proxy", "the proxy's velocity = the chord to", "where the controller left the character"),
             ("the physics step", "8.10's: velocities, contacts, solve,", "positions — crates, platforms, the proxy"),
             ("move_character", "against the world as the step left it;", "carries by what a platform just did")]
    sy, sw, sh = 50, 250, 58
    for i, (a, b1, b2) in enumerate(steps):
        y = sy + i * (sh + 16)
        col = GREEN if i != 1 else GREY
        body.append(hollow(rx, y, sw, sh, col, width=1.4))
        body.append(label(rx + 10, y + 19, a, cls="sm mono", anchor="start"))
        body.append(label(rx + 10, y + 36, b1, cls="xs muted", anchor="start"))
        body.append(label(rx + 10, y + 50, b2, cls="xs muted", anchor="start"))
        if i:
            body.append(carrow(rx + sw / 2, y - 15, rx + sw / 2, y - 2, uid, "grey", GREY, width=1.2))

    ox = rx + sw + 40
    oy = 50
    body.append(label(ox, oy + 8, "THE CONTROLLER owns", cls="xs", anchor="start"))
    body.append(label(ox, oy + 24, "the character's position", cls="xs muted", anchor="start"))
    body.append(label(ox, oy + 56, "THE SOLVER owns", cls="xs", anchor="start"))
    body.append(label(ox, oy + 72, "every other body", cls="xs muted", anchor="start"))
    body.append(label(ox, oy + 104, "THE PROXY is where they meet:", cls="xs", anchor="start"))
    body.append(label(ox, oy + 120, "a kinematic capsule, steered", cls="xs muted", anchor="start"))
    body.append(label(ox, oy + 136, "onto the character by velocity", cls="xs muted", anchor="start"))
    body.append(label(ox, oy + 168, "≤ 40 kg dynamic: PUSHED", cls="xs", anchor="start"))
    body.append(label(ox, oy + 184, "sideways moves ignore it", cls="xs muted", anchor="start"))
    body.append(label(ox, oy + 200, "heavier: a WALL", cls="xs", anchor="start"))

    ny = sy + 3 * (sh + 16) + 14
    lines = [f"A designer's jump is a height, not a force: v₀ = √(2gH). Asked for {JUMP[0]:.2f} m, the",
             f"controller rises {JUMP[2]:.4f} m at 60 Hz — the discrete sum of semi-implicit Euler's steps,",
             f"v₀²/2g − v₀h/2 = {JUMP[3]:.4f}, 8.1's integrator showing through. Aim for the height the",
             "steps give, or accept four centimetres."]
    for i, s in enumerate(lines):
        body.append(label(rx, ny + i * 15, s, cls="xs", anchor="start"))

    H = max(qy + 58, ny + 4 * 15) + 14
    return svg(uid, W, H,
               "The controller's five stages, and the fixed step it runs in",
               "Left: move_character's five stages in order — carry, recover, sideways, vertical, "
               "ground — each labelled with the section that measures it, over the one query they all "
               "use: a sweep, which is one cast per candidate obstacle. Right: the fixed step — steer "
               "the proxy, run the physics step, move the character — and who owns which body, with a "
               "note on the discrete jump height.", body)


# ===========================================================================
# FIGURE 3 — conservative advancement is Newton's method                     (§3)
# ===========================================================================

def fig3():
    uid = "h3"
    W = 980
    body = [cmarker(uid, "grey", GREY), cmarker(uid, "blue", BLUE), cmarker(uid, "green", GREEN)]

    # ---- LEFT: the geometry, with each iterate's separating plane -----------------
    body.append(label(26, 30, "Two balls: from the origin along (4, 0), toward one at (3, 0.6)",
                      cls="sm", anchor="start"))
    s = 100.0
    ox, oy = 80, 200

    def P(x, y):
        return ox + x * s, oy - y * s

    body.append(carrow(*P(-0.6, 0), *P(3.9, 0), uid, "grey", GREY, width=1.0))
    body.append(label(P(3.9, 0)[0], oy + 16, "d = (4, 0)", cls="xs mono", anchor="end"))
    cx_, cy_ = P(3.0, 0.6)
    body.append(ring(cx_, cy_, BLUE, 0.5 * s, 1.6))
    body.append(label(cx_ + 0.5 * s + 6, cy_ - 0.35 * s, "obstacle", cls="xs", anchor="start"))
    shown = [(NEWTON[0], GREY, "4 3", "t₀ = 0"), (NEWTON[1], GREY, None, "t₁ = 0.5250"),
             ((3, 0.55, 0, 0, 0, None), GREEN, None, "t = 0.55: contact")]
    for k, (row, col, dash, name) in enumerate(shown):
        t = row[1]
        mx, my = P(4.0 * t, 0.0)
        body.append(ring(mx, my, col, 0.5 * s, 1.2 if col == GREY else 1.8, dash=dash))
        body.append(label(mx, my + 0.5 * s + 16 + 14 * (k == 2), name, cls="xs mono"))
    for row in NEWTON[:2]:
        t = row[1]
        x = 4.0 * t
        dx, dy = 3.0 - x, 0.6
        ln = math.hypot(dx, dy)
        nx, ny = dx / ln, dy / ln
        sx, sy = 3.0 - 0.5 * nx, 0.6 - 0.5 * ny
        tx, ty = -ny, nx
        body.append(cline(*P(sx - 0.7 * tx, sy - 0.7 * ty), *P(sx + 0.7 * tx, sy + 0.7 * ty), BLUE, width=1.0,
                          dash="3 3"))
    lines = ["Each dashed line is the plane GJK's direction separates the pair",
             "by at that iterate. The next ball goes where the mover, sliding",
             "along d, would first touch that plane — never further. t₂ and t₃",
             "are within half a millimetre of contact and are drawn as one."]
    for i, t in enumerate(lines):
        body.append(label(26, oy + 0.5 * s + 70 + i * 15, t, cls="xs", anchor="start"))

    # ---- RIGHT: f(t) and the tangents, then a zoom near the root ----------------------
    px0, py0, pw, ph = 560, 58, 380, 150
    body.append(label(px0 - 30, 30, "f(t) = |(4t − 3, −0.6)| − 1, and Newton's tangents", cls="sm",
                      anchor="start"))

    def F(t):
        return math.hypot(4 * t - 3, -0.6) - 1.0

    def plot(x0, y0, w, h, t_lo, t_hi, f_lo, f_hi, ticks_t, ticks_f, steps):
        out = [frame(x0, y0, w, h)]

        def X(t):
            return x0 + (t - t_lo) / (t_hi - t_lo) * w

        def Y(v):
            return y0 + h - (v - f_lo) / (f_hi - f_lo) * h

        out.append(rule(x0, Y(0), x0 + w, Y(0), cls="grid", width=0.8))
        for v, txt in ticks_f:
            out.append(label(x0 - 6, Y(v) + 4, txt, cls="xs mono", anchor="end"))
        for tt, txt in ticks_t:
            out.append(label(X(tt), y0 + h + 15, txt, cls="xs mono"))
        n = 240
        curve = [(X(t_lo + (t_hi - t_lo) * k / n), Y(F(t_lo + (t_hi - t_lo) * k / n))) for k in range(n + 1)
                 if f_lo <= F(t_lo + (t_hi - t_lo) * k / n) <= f_hi]
        out.append(poly(curve, BLUE, width=2.0, close=False))
        for t, gap, appr in steps:
            t_next = t + gap / appr
            if t >= t_lo:
                out.append(cline(X(t), Y(gap), X(t_next), Y(0.0), GREEN, width=1.4, dash="5 3"))
                out.append(dot(X(t), Y(gap), GREEN, 3.2))
        out.append(dot(X(0.55), Y(0.0), PURPLE, 3.6))
        return out

    steps = [(r[1], r[2], r[3]) for r in NEWTON[:3]]
    body.extend(plot(px0, py0, pw, ph, 0.0, 0.6, -0.1, 2.2, [(0, "0"), (0.2, "0.2"), (0.4, "0.4"), (0.6, "0.6")],
                     [(0.0, "0"), (1.0, "1"), (2.0, "2")], steps))
    zy = py0 + ph + 48
    body.append(label(px0 - 30, zy - 12, "near the root, magnified: the tangent and the curve all but coincide",
                      cls="xs muted", anchor="start"))
    body.append(label(px0 - 30, zy + 110 + 30, "— and that closeness IS quadratic convergence",
                      cls="xs muted", anchor="start"))
    body.extend(plot(px0, zy, pw, 110, 0.515, 0.565, -0.01, 0.09,
                     [(0.52, "0.52"), (0.53, "0.53"), (0.54, "0.54"), (0.55, "0.55"), (0.56, "0.56")],
                     [(0.0, "0"), (0.08, "0.08")], steps))
    ly = zy + 110 + 56
    body.extend(legend(px0 - 30, ly, [
        (BLUE, "the true distance: convex, so every tangent lies below it"),
        (GREEN, "Newton's steps: each lands short of the root, never past"),
        (PURPLE, "the root, t = 0.55"),
    ]))
    rows = [[str(n), f"{t:.10f}", f"{gap:.10f}", f"{err:.3e}", "" if r is None else f"{r:.5f}"]
            for n, t, gap, appr, err, r in NEWTON]
    ty0 = oy + 0.5 * s + 70 + 4 * 15 + 34
    t, th = tbl(26, ty0, ["n", "t", "gap", "error", "err / prev²"], rows, [30, 104, 104, 76, 90],
                title="by hand, in double precision; the ratio tends to f''/2|f'| = 0.9")
    body.extend(t)
    H = max(ty0 + th, ly + 3 * 17) + 12
    return svg(uid, W, H,
               "Conservative advancement on two balls, as geometry and as Newton's method",
               "Left: a ball moving along +x toward another at (3, 0.6), drawn at the start, at the "
               "first Newton iterate and at contact, with the separating plane of each iterate "
               "dashed, and a table of the iterates and their errors. Right: the distance f(t) as a "
               "convex curve with Newton's tangent steps landing short of the root at t = 0.55, "
               "and the same near the root, magnified.", body)


# ===========================================================================
# FIGURE 4 — one obstacle at a time, the other conservative step, the skin (§3)
# ===========================================================================

def fig4():
    uid = "h4"
    W = 980
    body = [cmarker(uid, "red", RED), cmarker(uid, "green", GREEN), cmarker(uid, "grey", GREY)]

    # ---- LEFT: a union overshoots ---------------------------------------------------
    body.append(label(26, 30, "One cast against a union flies through", cls="sm", anchor="start"))
    s = 60.0
    ox, oy = 50, 200

    def P(x, y):
        return ox + x * s, oy - y * s

    body.append(ring(*P(0, 0), GREY, 0.4 * s, 1.4))
    body.append(label(P(0, 0)[0], oy + 0.4 * s + 14, "mover", cls="xs"))
    body.append(ring(*P(1.3, 1.25), BLUE, 0.45 * s, 1.4))
    body.append(label(P(1.3, 1.25)[0], P(1.3, 1.25)[1] - 0.45 * s - 8, "passed, grazing", cls="xs"))
    wx0, wx1 = 3.4, 3.6
    body.append(polygon([P(wx0, -1.6), P(wx1, -1.6), P(wx1, 1.2), P(wx0, 1.2)], BLUE, opacity=0.18))
    body.append(label(P(3.5, 1.2)[0], P(3.5, 1.2)[1] - 8, "wall", cls="xs"))
    body.append(carrow(*P(0.45, 0), *P(4.9, 0), uid, "red", RED, width=1.8))
    body.append(label(P(0.5, 0)[0], oy - 8, "one step on min(f₁, f₂): through", cls="xs", anchor="start"))
    body.append(carrow(*P(0.45, -0.35), *P(2.95, -0.35), uid, "green", GREEN, width=1.8))
    body.append(label(P(1.6, -0.35)[0], P(1.6, -0.35)[1] + 16, "one cast each: stops at the wall", cls="xs"))
    lines = [f"{UNION[0]:,} of {UNION[1]:,} grazing passes: through the wall",
             f"one cast per obstacle, smallest t: {UNION[2]}",
             "The nearer ball's plane is almost parallel to the",
             "motion, so its step is long — and it says nothing",
             "about the wall. A plane separates ONE pair."]
    ty = P(0, -1.6)[1] + 30
    for i, t in enumerate(lines):
        body.append(label(26, ty + i * 15, t, cls="xs mono" if i < 2 else "xs", anchor="start"))

    # ---- MIDDLE: Newton against sphere tracing ---------------------------------------
    mx0, my0, mw, mh = 390, 58, 230, 190
    body.append(label(mx0 - 20, 30, "Steps to a face at a grazing angle", cls="sm", anchor="start"))
    body.append(frame(mx0, my0, mw, mh))
    lo, hi = 1.0, 400.0
    for e in (1, 10, 100):
        yy = logy(e, lo, hi, my0, mh)
        body.append(rule(mx0, yy, mx0 + mw, yy, cls="grid", width=0.6))
        body.append(label(mx0 - 6, yy + 4, f"{e}", cls="xs mono", anchor="end"))
    slot = mw / len(TRACE)
    for i, (ang, newton, trace) in enumerate(TRACE):
        x = mx0 + i * slot + slot * 0.18
        bw = slot * 0.3
        for k, (v, col) in enumerate(((newton, GREEN), (trace, RED))):
            top = logy(v, lo, hi, my0, mh)
            body.append(box(x + k * bw, top, bw - 2, my0 + mh - top, col, opacity=0.7, rx=1, width=0.8))
            body.append(label(x + k * bw + (bw - 2) / 2, top - 4, str(v), cls="xs mono"))
        body.append(label(mx0 + i * slot + slot / 2, my0 + mh + 16, f"{ang}°", cls="xs mono"))
    body.append(label(mx0 + mw / 2, my0 + mh + 32, "angle between the motion and the face", cls="xs muted"))
    body.extend(legend(mx0 - 20, my0 + mh + 54, [
        (GREEN, "the cast: gap ÷ closing speed"),
        (RED, "sphere tracing: gap ÷ |d|"),
    ]))

    # ---- RIGHT: the skin ------------------------------------------------------------
    rx = 660
    body.append(label(rx, 30, "Cast, then slide from where it stopped", cls="sm", anchor="start"))
    rows = [[k, f"{m:.2f}", f"{mx:.1f}", (f"{100 * i / n:.1f}%", "xs mono t-bad" if i else "xs mono t-ok")]
            for k, m, mx, i, n in SKINS]
    t, th = tbl(rx, 58, ["skin", "mean gap", "worst gap", "starts inside"], rows, [64, 78, 80, 88],
                title="≈4,000 hits each; gaps in mm")
    body.extend(t)
    txt, nh = para(rx, 58 + th + 20, "With no skin the cast lands ON the contact — against a face, in one "
                   "step — GJK calls that intersecting, and the cast falls back to the step before: "
                   "often the start. A skin thinner than GJK's own margin is worse than none. A "
                   "millimetre, and nothing starts inside.", cols=48)
    body.extend(txt)
    uy = 58 + th + 20 + nh + 16
    body.append(label(rx, uy, "Stepping from GJK's distance (an UPPER bound):", cls="xs", anchor="start"))
    body.append(label(rx, uy + 15, f"{RANDOM['upper_late']} of {RANDOM['of']:,} hits late, up to "
                      f"{RANDOM['upper_mm']:.2f} mm inside the skin.", cls="xs mono t-bad", anchor="start"))
    body.append(label(rx, uy + 30, "From its certified LOWER bound: none.", cls="xs mono t-ok", anchor="start"))

    H = max(ty + 5 * 15, my0 + mh + 54 + 2 * 17, uy + 30) + 14
    return svg(uid, W, H,
               "Why one cast per obstacle, why divide by the closing speed, and why a skin",
               "Left: a mover passing a ball at a grazing angle toward a wall; one Newton step on the "
               "union flies through the wall, one cast per obstacle stops at it. Middle: on a log "
               "axis, Newton steps against sphere-tracing steps to reach a face at 90 down to 1 "
               "degree — 2 against 237. Right: a table of four skins with the gap a cast leaves and "
               "how often the next cast starts inside, and the upper-bound control.", body)


# ===========================================================================
# FIGURE 5 — what the rest of a move is clipped FROM                         (§4)
# ===========================================================================

def fig5():
    uid = "h5"
    W = 980
    body = [cmarker(uid, "red", RED), cmarker(uid, "green", GREEN), cmarker(uid, "grey", GREY),
            cmarker(uid, "blue", BLUE)]
    body.append(label(26, 30, "A 120° corner, the stick 12° off its bisector: one move, three rules",
                      cls="sm", anchor="start"))
    half = math.radians(60.0)
    c, s_ = math.cos(half), math.sin(half)
    dir_a, dir_b = (-c, s_), (-c, -s_)
    n_a, n_b = (-s_, -c), (-s_, c)
    head = math.radians(12.0)
    d = (math.cos(head), math.sin(head))

    def sub(u, v):
        return (u[0] - v[0], u[1] - v[1])

    def mul(u, k):
        return (u[0] * k, u[1] * k)

    def dt(u, v):
        return u[0] * v[0] + u[1] * v[1]

    def slide(v, n):
        into = dt(v, n)
        return sub(v, mul(n, into)) if into < 0 else v

    def unit(v):
        ln = math.hypot(*v)
        return (v[0] / ln, v[1] / ln) if ln > 1e-9 else (0.0, 0.0)

    d_a = slide(d, n_a)                  # after the first wall: along A, toward the corner
    last = slide(d_a, n_b)               # the remainder against the last plane only
    rem = slide(d_a, n_b)                # the remainder against B alone passes the check
    panels = [("the remainder, last plane", last, RED, "red", "back OUT along B"),
              ("the remainder, every plane", rem, RED, "red", "B alone passes the check: OUT"),
              ("the original, every plane", (0.0, 0.0), GREEN, "green", "both fail; the crease is vertical: 0")]
    pw, gap = 300, 26
    L, A = 120.0, 64.0
    for k, (name, res, col, mk, note) in enumerate(panels):
        x0 = 26 + k * (pw + gap)
        ax, ay = x0 + 240, 190
        body.append(label(x0, 62, name, cls="sm", anchor="start"))
        for dv, nm, dy_ in ((dir_a, "A", -8), (dir_b, "B", 16)):
            ex, ey = ax + dv[0] * L, ay - dv[1] * L
            body.append(cline(ax, ay, ex, ey, GREY, width=3.0))
            body.append(label(ex - 10, ey + dy_, nm, cls="xs", anchor="end"))
        cx0, cy0 = ax - 70, ay
        body.append(ring(cx0, cy0, GREY, 9.0, 1.2))
        u = unit(d)
        body.append(carrow(cx0, cy0, cx0 + u[0] * A, cy0 - u[1] * A, uid, "grey", GREY, width=1.4))
        if k < 2:
            ua = unit(d_a)
            body.append(carrow(cx0, cy0, cx0 + ua[0] * A, cy0 - ua[1] * A, uid, "blue", BLUE, width=1.4,
                               dash="4 3"))
        if math.hypot(*res) > 1e-6:
            ur = unit(res)
            body.append(carrow(cx0, cy0, cx0 + ur[0] * A, cy0 - ur[1] * A, uid, mk, col, width=2.2))
        else:
            body.append(dot(cx0, cy0, GREEN, 5.0))
        body.append(label(x0, 300, note, cls="xs", anchor="start"))
    ty = 336
    body.extend(legend(26, ty, [(GREY, "the stick, d"), (BLUE, "after wall A: slid along it, toward the corner"),
                                (RED, "what the rule does with that next"), (GREEN, "Quake 1: clip the ORIGINAL")]))
    rows = [[o, r, (f"{t:.0f} mm/s", "xs mono t-bad" if t > 1 else "xs mono t-ok"), str(b)]
            for o, r, t, b in CORNER]
    t, th = tbl(470, ty + 4, ["corner", "the rest of the move is", "travel", "blocked"], rows,
                [60, 230, 100, 80], title="5 s pressed into the corner; the last second measured")
    body.extend(t)
    H = max(ty + 4 * 17, ty + 4 + th) + 12
    return svg(uid, W, H,
               "Three ways to clip the rest of a move, in an obtuse corner",
               "Three panels of the same 120-degree corner with the stick 12 degrees off the "
               "bisector. The motion is first slid along wall A toward the corner. Clipping that "
               "remainder against the last plane, or against every plane, sends it back out along "
               "wall B; clipping the original motion against both planes leaves only the vertical "
               "crease, so the character stops. A table gives each rule's travel in 60 and 120 "
               "degree corners: 572 to 575 mm a second for the remainder rules, zero for the original.",
               body)


# ===========================================================================
# FIGURE 6 — slopes: along the ground, standing still, too steep            (§5)
# ===========================================================================

def fig6():
    uid = "h6"
    W = 980
    body = [cmarker(uid, "green", GREEN), cmarker(uid, "red", RED), cmarker(uid, "grey", GREY),
            cmarker(uid, "blue", BLUE)]

    # ---- LEFT: along_ground ------------------------------------------------------------
    body.append(label(26, 30, "A step laid along a 30° ramp, horizontal part kept", cls="sm",
                      anchor="start"))
    a = math.radians(30.0)
    ox, oy, s = 60, 250, 250.0

    def P(x, y):
        return ox + x * s, oy - y * s

    body.append(cline(*P(-0.05, -0.05 * math.tan(a)), *P(1.05, 1.05 * math.tan(a)), GREY, width=3.0))
    n = (-math.sin(a), math.cos(a))
    base = (0.2, 0.2 * math.tan(a))
    L = 0.6
    body.append(carrow(*P(*base), *P(base[0] + L, base[1]), uid, "grey", GREY, width=1.6))
    body.append(label(P(base[0] + L, base[1])[0] + 6, P(base[0] + L, base[1])[1] + 4, "d: the stick",
                      cls="xs", anchor="start"))
    laid = (L, L * math.tan(a))
    body.append(carrow(*P(*base), *P(base[0] + laid[0], base[1] + laid[1]), uid, "green", GREEN, width=2.2))
    body.append(cline(*P(base[0] + L, base[1]), *P(base[0] + L, base[1] + laid[1]), GREEN, width=1.0, dash="3 3"))
    proj_len = L * math.cos(a)
    proj = (proj_len * math.cos(a), proj_len * math.sin(a))
    off = (n[0] * -0.045, n[1] * -0.045)          # drawn just below the ramp, so both show
    body.append(carrow(*P(base[0] + off[0], base[1] + off[1]),
                       *P(base[0] + proj[0] + off[0], base[1] + proj[1] + off[1]), uid, "red", RED, width=1.4,
                       dash="5 3"))
    lx, ly = P(base[0] + laid[0] * 0.5 + n[0] * 0.07, base[1] + laid[1] * 0.5 + n[1] * 0.07)
    body.append(label(lx, ly, "laid: all of it", cls="xs", anchor="end"))
    body.append(label(P(base[0] + proj[0] * 0.55, base[1] + proj[1] * 0.55 - 0.1)[0],
                      P(base[0] + proj[0] * 0.55, base[1] + proj[1] * 0.55 - 0.1)[1],
                      "slid: cos²α of it", cls="xs", anchor="start"))
    body.append(carrow(*P(0.75, 0.75 * math.tan(a)), *P(0.75 + 0.18 * n[0], 0.75 * math.tan(a) + 0.18 * n[1]),
                       uid, "blue", BLUE, width=1.4))
    body.append(label(*P(0.75 + 0.2 * n[0] - 0.03, 0.75 * math.tan(a) + 0.2 * n[1] + 0.02), "n", cls="xs"))
    lines = ["t = d − ŷ·(d·n)/n.y   keeps d's horizontal part exactly",
             "d − (d·n)·n, the slide, keeps only cos²α of it: 2.25 of 3 m/s"]
    for i, t in enumerate(lines):
        body.append(label(26, oy + 30 + i * 15, t, cls="xs mono" if i == 0 else "xs", anchor="start"))
    rows = [[f"{d}°", f"{hz:.4f}", f"{al:.4f}", f"{ri:.4f}", f"{ta:.4f}"] for d, hz, al, ri, ta in RAMPS]
    t, th = tbl(26, oy + 30 + 2 * 15 + 22, ["ramp", "horizontal", "along it", "rise / m", "tan α"], rows,
                [60, 80, 80, 74, 66], title="walking up at 3 m/s, measured")
    body.extend(t)

    # ---- MIDDLE: standing still ------------------------------------------------------
    mx = 440
    body.append(label(mx, 30, "Standing still: stop gravity, or slide it", cls="sm", anchor="start"))
    rows = [[f"{d}°", ("0.000000", "xs mono t-ok"), (f"{m:.6f}", "xs mono t-bad"), (f"{p:.6f}", "xs mono")]
            for d, m, p in CREEP]
    t, th2 = tbl(mx, 58, ["slope", "stopped", "slid", "g·h·sin α"], rows, [52, 72, 72, 76],
                 title="creep speed, m/s, over 2 s")
    body.extend(t)
    txt, nh = para(mx, 58 + th2 + 20, "A grounded character still falls g·h² a step before the ground "
                   "stops it. Slid along the slope instead of stopped by it, that drop has a downhill "
                   "part g·h²·sin α — every step, forever: 8.2 cm/s on 30°, five metres a minute.", cols=44)
    body.extend(txt)

    # ---- RIGHT: too steep ----------------------------------------------------------------
    rx = 740
    body.append(label(rx, 30, "Too steep to walk (limit 45°)", cls="sm", anchor="start"))
    rows = [[f"{d}°", (f"{f:+.3f}", "xs mono t-ok"), (f"{u:+.3f}", "xs mono t-bad")] for d, f, u, rest in HOP]
    t, th3 = tbl(rx, 58, ["slope", "flattened", "not"], rows, [56, 74, 74],
                 title="hopping 0.5 m into it: how far in, m")
    body.extend(t)
    txt, nh3 = para(rx, 58 + th3 + 20, "On the ground, laying the move along the floor already throws "
                    "the climb away; in the air only the flattened normal does. And a slope too steep to "
                    f"stand on is slid down at {STEEP_SLIDE[0]:.4f} m/s after 0.5 s on 60°: g·t·sin α = "
                    f"{STEEP_SLIDE[1]:.4f}, a frictionless incline, from a projection alone.", cols=34)
    body.extend(txt)
    H = max(oy + 30 + 2 * 15 + 22 + th, 58 + th2 + 20 + nh, 58 + th3 + 20 + nh3) + 12
    return svg(uid, W, H,
               "Slopes: laying a step along the ground, standing still on it, and what is too steep",
               "Left: a 30-degree ramp with the stick's horizontal step, the step laid along the ramp "
               "keeping its horizontal part, and the shorter plain projection; a table shows the "
               "horizontal speed is exactly 3 m/s on every walkable ramp. Middle: a table of creep "
               "speeds when gravity is slid along the slope instead of stopped, matching g h sin "
               "alpha. Right: how far a character hopping into a steep slope gets with and without "
               "flattening the wall normal.", body)


# ===========================================================================
# FIGURE 7 — the round bottom on an edge                                     (§6)
# ===========================================================================

def fig7():
    uid = "h7"
    W = 980
    body = [cmarker(uid, "blue", BLUE), cmarker(uid, "amber", AMBER), cmarker(uid, "grey", GREY)]
    th_max = math.radians(45.0)
    s = 230.0

    # ---- LEFT: the free curb -------------------------------------------------------------
    body.append(label(26, 30, "A curb meets the round bottom as a ramp", cls="sm", anchor="start"))
    ox, oy = 90, 240
    curb = RS * (1 - math.cos(th_max))

    def P(x, y):
        return ox + x * s, oy - y * s

    body.append(rule(ox - 60, oy, ox + 0.82 * s, oy, cls="ink-soft", width=1.4))
    edge = (0.36, curb)
    body.append(polygon([P(0.36, 0), P(0.78, 0), P(0.78, curb), P(0.36, curb)], GREY, opacity=0.25))
    ctr = (edge[0] - RS * math.sin(th_max), curb + RS * math.cos(th_max))
    body.append(ring(*P(*ctr), AMBER, RS * s, 1.6))
    body.append(ring(*P(*ctr), AMBER, R_CAP * s, 1.0, dash="3 3"))
    body.append(carrow(*P(*edge), *P(*ctr), uid, "blue", BLUE, width=1.6))
    body.append(dot(*P(*edge), BLUE, 3.4))
    body.append(cline(*P(*ctr), *P(ctr[0], ctr[1] - 0.14), GREY, width=1.0, dash="3 3"))
    body.append(label(P(0.78, curb / 2)[0] + 6, P(0.78, curb / 2)[1] + 4, f"s = {curb * 1000:.1f} mm",
                      cls="xs mono", anchor="start"))
    lines = ["n runs from the edge to the centre: n.y = (r+s − s)/(r+s).",
             "Walkable while n.y ≥ cos 45°, so the tallest curb climbed",
             "with NO step-up is (r + skin)(1 − cos θ).",
             f"measured {CURB[0] * 1000:.2f} mm, predicted {CURB[1] * 1000:.2f}",
             "(dashed: the capsule; solid: plus the skin, which touches)"]
    for i, t in enumerate(lines):
        body.append(label(26, oy + 30 + i * 15, t, cls="xs mono" if i == 3 else ("xs muted" if i == 4 else "xs"),
                          anchor="start"))

    # ---- MIDDLE: the overhang --------------------------------------------------------------
    mx = 370
    body.append(label(mx, 30, "Standing past an edge", cls="sm", anchor="start"))
    ox2, oy2 = mx + 110, 240

    def Q(x, y):
        return ox2 + x * s, oy2 - y * s

    body.append(polygon([Q(-0.4, -0.06), Q(0, -0.06), Q(0, 0), Q(-0.4, 0)], GREY, opacity=0.25))
    over = RS * math.sin(th_max)
    c2 = (over, RS * math.cos(th_max))
    body.append(ring(*Q(*c2), AMBER, RS * s, 1.6))
    body.append(carrow(*Q(0, 0), *Q(*c2), uid, "blue", BLUE, width=1.6))
    body.append(dot(*Q(0, 0), BLUE, 3.4))
    body.append(cline(*Q(0, 0), *Q(0, 0.5), GREY, width=1.0, dash="3 3"))
    body.append(cline(*Q(0, c2[1] + 0.35), *Q(over, c2[1] + 0.35), AMBER, width=1.2))
    body.append(label(Q(over / 2, c2[1] + 0.35)[0], Q(over / 2, c2[1] + 0.35)[1] - 6, "overhang", cls="xs"))
    lines = ["tilt = asin(overhang / (r + skin)); it stands",
             "while that is walkable: (r + skin)·sin θ.",
             f"placed: {OVER[0] * 1000:.2f} mm; walking off: {OVER[2] * 1000:.1f}",
             f"predicted {OVER[1] * 1000:.2f} mm"]
    for i, t in enumerate(lines):
        body.append(label(mx, oy + 30 + i * 15, t, cls="xs mono" if i >= 2 else "xs", anchor="start"))

    # ---- RIGHT: the normal it reports ------------------------------------------------------
    px0, py0, pw, ph = 706, 58, 244, 190
    body.append(label(px0 - 20, 30, "The tilt reported on an edge", cls="sm", anchor="start"))
    body.append(frame(px0, py0, pw, ph))

    def X(x):
        return px0 + x / 0.3 * pw

    def Y(dg):
        return py0 + ph - dg / 60.0 * ph

    for dg in (0, 15, 30, 45, 60):
        body.append(rule(px0, Y(dg), px0 + pw, Y(dg), cls="grid", width=0.6))
        body.append(label(px0 - 6, Y(dg) + 4, f"{dg}°", cls="xs mono", anchor="end"))
    for x in (0.0, 0.1, 0.2, 0.3):
        body.append(label(X(x), py0 + ph + 16, f"{x:g}", cls="xs mono"))
    body.append(label(px0 + pw / 2, py0 + ph + 32, "centre past the edge, m", cls="xs muted"))
    curve = []
    for k in range(0, 101):
        x = k * 0.003
        a = math.degrees(math.asin(min(1.0, x / RS)))
        if a <= 60.0:
            curve.append((X(x), Y(a)))
    body.append(poly(curve, PURPLE, width=1.4, dash="5 3", close=False))
    body.append(cline(px0, Y(45), px0 + pw, Y(45), RED, width=1.0, dash="2 3"))
    for x, meas, pred in TILT:
        body.append(halo(X(x), Y(pred), PURPLE, 6.0))
        body.append(dot(X(x), Y(meas), AMBER, 3.2))
    body.extend(legend(px0 - 20, py0 + ph + 54, [
        (PURPLE, "asin(x / (r + skin))"),
        (AMBER, "measured, from the cast's normal"),
        (RED, "45°: the walkable limit"),
        (None, f"worst {TILT_WORST:.3f}° — GJK's, at a centimetre"),
    ]))
    H = max(oy + 30 + 5 * 15, py0 + ph + 54 + 4 * 17) + 12
    return svg(uid, W, H,
               "A capsule's round bottom on an edge: a free curb, an overhang, and the normal",
               "Left: a capsule's bottom sphere, inflated by the skin, touching the top edge of a "
               "curb; the normal from the edge to the centre is tilted, and the curb is climbed "
               "while the tilt is walkable, up to (r + skin)(1 - cos 45) = 90.8 mm. Middle: the "
               "sphere resting on an edge with its centre past it; it stands until the overhang is "
               "(r + skin) sin 45 = 219.2 mm. Right: the tilt reported against the overhang, "
               "measured points on the arcsine curve, with the 45-degree limit.", body)


# ===========================================================================
# FIGURE 8 — snapping                                                         (§7)
# ===========================================================================

def fig8():
    uid = "h8"
    W = 980
    body = [cmarker(uid, "green", GREEN), cmarker(uid, "red", RED), cmarker(uid, "grey", GREY),
            cmarker(uid, "blue", BLUE)]

    # ---- LEFT: skipping down a slope ---------------------------------------------------
    body.append(label(26, 30, "Down a 30° slope, moved flat, gravity to catch up", cls="sm", anchor="start"))
    rows = [[f"{v:.2f} m/s", (str(a), "xs mono t-bad" if a else "xs mono"), (str(b), "xs mono t-ok")]
            for v, a, b in SKIP]
    t, th = tbl(26, 58, ["speed", "flat + gravity", "laid on it"], rows, [80, 104, 90],
                title="frames of 60 in the air")
    body.extend(t)
    lines = ["The ground drops v·h·tan α a step; one step of",
             "gravity drops g·h², and the ground probe reaches",
             "2 skins more. It skips above",
             f"(g·h² + 2·skin)/(h·tan α) = {SKIP_V:.4f} m/s,",
             f"and with no probe at all above {SKIP_V0:.4f}.",
             "Laid along the slope, it never leaves it."]
    for i, s in enumerate(lines):
        body.append(label(26, 58 + th + 20 + i * 15, s, cls="xs mono" if i in (3, 4) else "xs", anchor="start"))

    # ---- MIDDLE: the crest ---------------------------------------------------------------
    mx = 340
    body.append(label(mx, 30, "Over a crest: the round bottom rolls, then flies", cls="sm", anchor="start"))
    rows = [[f"{v:.1f}", (f"{fr * H:.3f} s" if fr else "0", "xs mono t-bad" if fr else "xs mono t-ok"),
             f"{p:.3f} s", (str(sn), "xs mono t-ok")] for v, fr, p, sn in CREST]
    t, th2 = tbl(mx, 58, ["m/s", "no snap", "projectile", "snap"], rows, [52, 80, 90, 56],
                 title="time in the air past a crest onto 30°")
    body.extend(t)
    lines = ["While one step's travel keeps the drop to the edge",
             "within the probe's reach, the capsule rolls over it:",
             f"v·h ≤ √((r+s)² − (r+s−reach)²) = {ROLL[0]:.4f} m,",
             f"v < {ROLL[1]:.3f} m/s. Faster, it flies 2v·tan α/g.",
             "The snap catches every one of them."]
    for i, s in enumerate(lines):
        body.append(label(mx, 58 + th2 + 20 + i * 15, s, cls="xs mono" if i in (2, 3) else "xs",
                          anchor="start"))

    # ---- RIGHT: rolling off a stair edge ----------------------------------------------------
    rx = 690
    body.append(label(rx, 30, "Off a stair edge: roll, then drop", cls="sm", anchor="start"))
    s = 180.0
    ox, oy = rx + 80, 150

    def P(x, y):
        return ox + x * s, oy - y * s

    body.append(polygon([P(-0.4, -0.1), P(0.0, -0.1), P(0.0, 0.0), P(-0.4, 0.0)], GREY, opacity=0.25))
    body.append(polygon([P(0.0, -0.28), P(0.8, -0.28), P(0.8, -0.18), P(0.0, -0.18)], GREY, opacity=0.25))
    over = RS * math.sin(math.radians(52.0))
    c0 = (over, RS * math.cos(math.radians(52.0)))
    body.append(ring(*P(*c0), RED, RS * s, 1.2, dash="4 3"))
    c1 = (RS + 0.004, c0[1])
    body.append(carrow(*P(c0[0], c0[1]), *P(c1[0], c1[1]), uid, "green", GREEN, width=1.8))
    c2 = (c1[0], -0.18 + RS)
    body.append(ring(*P(*c2), GREEN, RS * s, 1.4))
    body.append(carrow(*P(c1[0], c1[1] - 0.03), *P(c2[0], c2[1] + 0.03), uid, "green", GREEN, width=1.4,
                       dash="4 3"))
    body.append(dot(*P(0, 0), BLUE, 3.2))
    body.append(label(P(c0[0] - RS, c0[1] + RS)[0] - 4, P(c0[0] - RS, c0[1] + RS)[1] + 4, "on the edge",
                      cls="xs", anchor="end"))
    body.append(label(P(c2[0] + RS, c2[1])[0] + 6, P(c2[0] + RS, c2[1])[1] + 4, "on the tread",
                      cls="xs", anchor="start"))
    lines = ["Resting on the edge it walked off, the capsule's",
             "first hit going down is the EDGE. It rolls off",
             "sideways by (r+s) − overhang ≤ (r+s)(1 − sin θ),",
             "then casts straight down.",
             f"8 stairs down: {STAIRS_DOWN[0][0]} frames in the air",
             f"without the snap, {STAIRS_DOWN[1][0]} with; worst extra",
             f"forward {STAIRS_DOWN[1][2] * 1000:.1f} mm. A 0.30 m drop is",
             f"snapped; 0.50 m falls ({LEDGE_DROP[1][1]} frames)."]
    for i, s_ in enumerate(lines):
        body.append(label(rx, 250 + i * 15, s_, cls="xs mono" if i >= 4 else "xs", anchor="start"))
    Hh = max(58 + th + 20 + 6 * 15, 58 + th2 + 20 + 5 * 15, 250 + 8 * 15) + 12
    return svg(uid, W, Hh,
               "Snapping: skipping down a slope, flying off a crest, and rolling off a stair",
               "Left: a table of frames in the air walking down a 30-degree slope at seven speeds, "
               "moved flat against laid along the slope, with the predicted skip speed of 2.36 m/s. "
               "Middle: time in the air past a crest at five speeds without the snap, against a "
               "projectile over the incline, and none with it. Right: a capsule resting on a stair "
               "edge rolling off it sideways and dropping to the tread below.", body)


# ===========================================================================
# FIGURE 9 — stepping up                                                      (§8)
# ===========================================================================

def fig9():
    uid = "h9"
    W = 980
    body = [cmarker(uid, "green", GREEN), cmarker(uid, "grey", GREY), cmarker(uid, "blue", BLUE),
            cmarker(uid, "amber", AMBER)]
    body.append(label(26, 30, "Up, across, down — and how far across", cls="sm", anchor="start"))
    th_max = math.radians(45.0)
    s = 250.0
    ox, oy = 90, 340
    riser = 0.40

    def P(x, y):
        return ox + x * s, oy - y * s

    body.append(rule(ox - 60, oy, ox + 1.4 * s, oy, cls="ink-soft", width=1.4))
    body.append(polygon([P(0.42, 0), P(1.35, 0), P(1.35, riser), P(0.42, riser)], GREY, opacity=0.25))
    edge = (0.42, riser)
    c0 = (edge[0] - RS, SKIN + R_CAP)
    body.append(ring(*P(*c0), GREY, RS * s, 1.2, dash="4 3"))
    up = riser + 0.05
    c1 = (c0[0], c0[1] + up)
    body.append(carrow(*P(c0[0] - RS - 0.06, c0[1]), *P(c1[0] - RS - 0.06, c1[1]), uid, "grey", GREY,
                       width=1.4))
    body.append(label(P(c0[0] - RS - 0.06, (c0[1] + c1[1]) / 2)[0] - 6,
                      P(c0[0] - RS - 0.06, (c0[1] + c1[1]) / 2)[1], "up", cls="xs", anchor="end"))
    f = RS - RS * math.sin(th_max)
    c2 = (c1[0] + f, c1[1])
    body.append(ring(*P(*c1), GREY, RS * s, 1.0, dash="2 3"))
    body.append(carrow(*P(c1[0], c1[1] + RS + 0.04), *P(c2[0], c2[1] + RS + 0.04), uid, "amber", AMBER,
                       width=2.0))
    body.append(label(P(c1[0] + f / 2, c1[1] + RS + 0.04)[0], P(c1[0] + f / 2, c1[1] + RS + 0.04)[1] - 8,
                      "f", cls="xs"))
    c3 = (c2[0], riser + RS * math.cos(th_max))
    body.append(ring(*P(*c3), GREEN, RS * s, 1.8))
    body.append(carrow(*P(*edge), *P(*c3), uid, "blue", BLUE, width=1.5))
    body.append(dot(*P(*edge), BLUE, 3.4))
    body.append(label(P(0.9, riser / 2)[0], P(0.9, riser / 2)[1], f"riser {riser:.2f} m", cls="xs mono"))
    lines = ["At the riser the centre is up to r + skin short of the edge;",
             "it must land within (r + skin)·sin θ of it to be walkable.",
             f"So the step carries it at least (r + skin)(1 − sin θ) = {FMIN * 1000:.1f} mm",
             "across — a lurch, on the frame the step is taken."]
    for i, t in enumerate(lines):
        body.append(label(26, oy + 30 + i * 15, t, cls="xs mono" if i == 2 else "xs", anchor="start"))

    rx = 560
    body.append(label(rx, 30, "Measured", cls="sm", anchor="start"))
    rows = [[f"{r_:.2f} m", f"{sh:.5f}", f"{lf:.5f}", f"{pr:.5f}"] for r_, sh, lf, pr in LEAST_F]
    t, th = tbl(rx, 58, ["riser", "short of edge", "least f", "short − (r+s)·sin θ"], rows, [70, 100, 90, 130],
                title="the least f that lands walkable, by bisection, m")
    body.extend(t)
    rows2 = [["eight 18 cm stairs at 2 m/s", f"{STAIRS_UP[0]} step-ups, {STAIRS_UP[1]} in the air"],
             ["  at the top after", f"{STAIRS_UP[2]:.3f} s"],
             ["  worst extra forward", f"{STAIRS_UP[3] * 1000:.1f} mm ≤ {FMIN * 1000:.1f}"],
             ["tallest ledge, step_height 0.30", f"{TALLEST[0]:.5f} m"],
             ["  = step_height + free curb", f"{TALLEST[1]:.5f} m"],
             ["into a 60° slope", "0 steps taken"]]
    t2, th2 = tbl(rx, 58 + th + 30, ["", ""], rows2, [230, 160])
    body.extend(t2)
    txt, nh = para(rx, 58 + th + 30 + th2 + 16, "The step-up and the round bottom ADD: after rising "
                   "step_height the capsule rolls over whatever of the ledge is left, exactly as it rolls "
                   "over a curb. And a step that lands on a steep slope is refused, or a staircase of "
                   "tiny steps would climb anything.", cols=58)
    body.extend(txt)
    Hh = max(oy + 30 + 4 * 15, 58 + th + 30 + th2 + 16 + nh) + 12
    return svg(uid, W, Hh,
               "The step-up: up, across, down, and the least forward move that lands",
               "Left: a capsule at a 40 cm riser rising, moving across by f, and dropping onto the "
               "edge with a walkable normal; f must be at least (r + skin)(1 - sin 45) = 90.8 mm. "
               "Right: a table of the least f found by bisection at three risers against the "
               "prediction, and the staircase, tallest-ledge and steep-slope results.", body)


# ===========================================================================
# FIGURE 10 — tunnelling                                                      (§9)
# ===========================================================================

def fig10():
    uid = "h10"
    W = 980
    body = [cmarker(uid, "red", RED), cmarker(uid, "grey", GREY)]

    # ---- LEFT: the arrival depth ------------------------------------------------------
    body.append(label(26, 30, "Sampled at instants: where the step boundary lands", cls="sm", anchor="start"))
    s = 200.0
    ox, oy = 110, 170
    wall = 0.10

    def P(x, y):
        return ox + x * s, oy - y * s

    body.append(polygon([P(0.3, -0.35), P(0.3 + wall, -0.35), P(0.3 + wall, 0.35), P(0.3, 0.35)], BLUE,
                        opacity=0.2))
    for k, (x, col) in enumerate(((0.3 - R_CAP - 0.04, GREY), (0.3 - R_CAP + 0.28, RED))):
        body.append(ring(*P(x, 0.0), col, R_CAP * s, 1.4, dash=None if k else "4 3"))
    body.append(carrow(*P(-0.04, -0.42), *P(0.24, -0.42), uid, "grey", GREY, width=1.2))
    body.append(label(P(0.1, -0.42)[0], P(0.1, -0.42)[1] - 6, "v·h", cls="xs"))
    body.append(cline(*P(0.3 + wall / 2, -0.45), *P(0.3 + wall / 2, 0.45), GREY, width=1.0, dash="2 3"))
    lines = ["The first sampled overlap is d deep, uniform on [0, v·h]",
             "(8.10 §6). EPA pushes the capsule out the NEAR side",
             "only while its centre has not passed the wall's middle:",
             "d < w/2 + r. So it passes with probability",
             "1 − (w/2 + r)/(v·h), and at 21 m/s it starts to."]
    for i, t in enumerate(lines):
        body.append(label(26, oy + 130 + i * 15, t, cls="xs mono" if i == 4 else "xs", anchor="start"))

    # ---- RIGHT: the plot ----------------------------------------------------------------
    px0, py0, pw, ph = 480, 58, 440, 230
    body.append(label(px0 - 30, 30, "Through a 10 cm wall, 200 phases per speed", cls="sm", anchor="start"))
    body.append(frame(px0, py0, pw, ph))
    lo, hi = 5.0, 20000.0

    def X(v):
        return logx(v, lo, hi, px0, pw)

    def Y(p):
        return py0 + ph - p / 100.0 * ph

    for p in (0, 25, 50, 75, 100):
        body.append(rule(px0, Y(p), px0 + pw, Y(p), cls="grid", width=0.6))
        body.append(label(px0 - 6, Y(p) + 4, f"{p}%", cls="xs mono", anchor="end"))
    for v in (10, 100, 1000, 10000):
        body.append(label(X(v), py0 + ph + 16, f"{v:g}", cls="xs mono"))
    body.append(label(px0 + pw / 2, py0 + ph + 32, "speed, m/s (60 Hz)", cls="xs muted"))
    curve = []
    for k in range(0, 301):
        v = lo * (hi / lo) ** (k / 300)
        p = max(0.0, 1.0 - (wall / 2 + R_CAP) / (v * H)) * 100.0
        curve.append((X(v), Y(p)))
    body.append(poly(curve, PURPLE, width=1.4, dash="5 3", close=False))
    for v, meas, pred in TUNNEL:
        body.append(halo(X(v), Y(pred), PURPLE, 6.5))
        body.append(dot(X(v), Y(meas), RED, 3.4))
    for v, gap in CONTROLLER_GAPS:
        body.append(dot(X(v), Y(0.0), GREEN, 3.6))
    body.extend(legend(px0 - 30, py0 + ph + 54, [
        (PURPLE, "1 − (w/2 + r)/(v·h)"),
        (RED, "the rigid capsule, measured"),
        (GREEN, "the controller at a 1 cm wall: never — it stops a skin short"),
    ]))
    rows = [[f"{v:,} m/s", f"{v * H:.3f} m", f"{gap:.6f} m"] for v, gap in CONTROLLER_GAPS]
    ty = py0 + ph + 54 + 3 * 17 + 22
    t, th = tbl(px0 - 30, ty, ["the controller", "a step", "stopped from the face"], rows, [120, 100, 150])
    body.extend(t)
    Hh = max(oy + 130 + 5 * 15, ty + th) + 12
    return svg(uid, W, Hh,
               "Tunnelling: a rigid capsule's odds against a controller that casts",
               "Left: a capsule sampled at two step boundaries either side of a 10 cm wall, the "
               "second overlapping it; the wall's midplane is dashed. Right: pass-through percentage "
               "against speed on a log axis — the rigid capsule's measurements on the closed-form "
               "curve, zero below 21 m/s, and the controller at zero at every speed up to 10,000 m/s, "
               "with a table of how far short of the wall it stopped.", body)


# ===========================================================================
# FIGURE 11 — carrying a rider                                               (§10)
# ===========================================================================

def fig11():
    uid = "h11"
    W = 980
    body = [cmarker(uid, "grey", GREY)]
    body.append(label(26, 30, "A rider on a turntable, ω = 2 rad/s, three revolutions", cls="sm",
                      anchor="start"))
    s = 58.0
    cx, cy = 200, 214
    body.append(hollow(cx - 2.8 * s, cy - 2.8 * s, 5.6 * s, 5.6 * s, GREY, width=0.8))
    # DRAWN from the recurrence the harness measured: each step moves the rider
    # along the tangent, h·ω×p, while the plate turns by 2·atan(ωh/2).
    omega = 2.0
    turn = 2.0 * math.atan(0.5 * omega * H)
    n = int(round(3 * 2 * math.pi / turn))
    px, pz = 1.5, 0.0
    spiral = [(cx + px * s, cy - pz * s)]
    for _ in range(n):
        vx, vz = -omega * pz, omega * px          # ω×p for ω along the view normal
        px, pz = px + H * vx, pz + H * vz
        spiral.append((cx + px * s, cy - pz * s))
    body.append(poly(spiral, RED, width=1.3, close=False))
    circ = [(cx + 1.5 * s * math.cos(k * math.pi / 60), cy - 1.5 * s * math.sin(k * math.pi / 60))
            for k in range(121)]
    body.append(poly(circ, GREEN, width=2.0, close=False))
    body.append(dot(cx, cy, GREY, 3.0))
    body.append(dot(cx + 1.5 * s, cy, GREEN, 4.0))
    body.append(label(cx, cy + 2.8 * s + 18, "the plate (drawn to 2.8 m of its 3.5 m half-width)", cls="xs muted"))
    lines = ["Carried by the point velocity under it, the rider moves",
             "along a TANGENT each step: |r|² grows by 1 + (ωh)².",
             "Carried by the plate's step as a transform, it stays put."]
    for i, t in enumerate(lines):
        body.append(label(26, cy + 2.8 * s + 44 + i * 15, t, cls="xs", anchor="start"))

    rx = 440
    body.append(label(rx, 30, "One revolution, rider at 1.5 m", cls="sm", anchor="start"))
    rows = [[f"{w:.1f}", name, f"{r:.5f}", (f"{g:.5f}", "xs mono t-bad" if g > 1.0001 else "xs mono t-ok"),
             f"{p:.5f}", lag] for w, name, r, g, p, lag in CARRY]
    t, th = tbl(rx, 58, ["ω", "carried", "radius", "growth", "(1+(ωh)²)^(N/2)", "left behind"], rows,
                [36, 110, 76, 76, 120, 88])
    body.extend(t)
    ty = 58 + th + 22
    lines = ["N is the steps in a revolution: 2π / 2·atan(ωh/2), the turn of ONE",
             "linearised step (8.3 §7) — not ωh. The growth per revolution is ≈ e^(πωh):",
             "5.4% at 1 rad/s, 11.1% at 2. The same drift as 8.11's velocity joint, for",
             "the same reason: a velocity is a tangent, and a tangent leaves a circle.",
             "",
             "A lift that only translates is carried exactly by either rule (drift",
             f"under 10 µm in 2 s); not carried, the rider is left ({LIFT_NONE[0]:.2f}, {LIFT_NONE[1]:.2f}) m",
             "behind — and off it."]
    for i, t_ in enumerate(lines):
        if t_:
            body.append(label(rx, ty + i * 15, t_, cls="xs", anchor="start"))
    body.extend(legend(rx, ty + 8 * 15 + 12, [(GREEN, "by transform: the plate's step, exactly"),
                                               (RED, "by velocity: drawn from the recurrence, 3 revolutions")]))
    Hh = max(cy + 2.8 * s + 44 + 3 * 15, ty + 8 * 15 + 12 + 2 * 17) + 12
    return svg(uid, W, Hh,
               "Carrying a rider on a turntable: by transform, and by velocity",
               "Left: a top-down view of a turntable with a rider 1.5 m from the axis; carried by "
               "transform it stays on its circle, carried by the point velocity it spirals outward "
               "over three revolutions. Right: a table of one revolution at 1 and 2 rad/s for three "
               "carry rules — radius, growth, the predicted growth and how far behind the plate the "
               "rider is left.", body)


# ===========================================================================
# FIGURE 12 — one owner: the proxy                                            (§11)
# ===========================================================================

def fig12():
    uid = "h12"
    W = 980
    body = [cmarker(uid, "green", GREEN), cmarker(uid, "red", RED), cmarker(uid, "grey", GREY)]
    body.append(label(26, 30, "A crate pushed through a proxy, seen from above", cls="sm", anchor="start"))
    s = 220.0

    def draw(y0, title, depth, col, mk, note1, note2):
        ox = 90
        body.append(label(26, y0, title, cls="xs", anchor="start"))
        cy = y0 + 80
        px_ = ox + 0.3 * s
        body.append(ring(px_, cy, GREEN, 0.3 * s, 1.4))
        body.append(dot(px_, cy, GREEN, 2.5))
        cx0 = px_ + 0.3 * s - depth * s
        body.append(polygon([(cx0, cy - 0.3 * s), (cx0 + 0.6 * s, cy - 0.3 * s), (cx0 + 0.6 * s, cy + 0.3 * s),
                             (cx0, cy + 0.3 * s)], AMBER, opacity=0.15))
        body.append(carrow(px_, cy, px_ + 50, cy, uid, mk, col, width=2.0))
        body.append(label(cx0 + 0.3 * s, cy + 0.3 * s + 16, f"{note1}; {note2}", cls="xs"))

    draw(58, "STEERED: velocity = the chord", 0.005, GREEN, "green", "crate v = 2 m/s", "overlap 5 mm: the slop")
    draw(246, "TELEPORTED: velocity = 0, position written", 0.1383, RED, "red", "crate v = 0",
         "overlap 138 mm")
    lines = ["Teleported, only the position pass moves the crate: β·(d − slop)",
             "a step. To move it v·h a step the proxy must start each step",
             "v·h/β + slop deep, and ends it v·h shallower:",
             f"0.005 + 0.0333/0.2 − 0.0333 = {PUSH_PRED['sunk']:.2f} mm, measured 138.20."]
    for i, t in enumerate(lines):
        body.append(label(26, 446 + i * 15, t, cls="xs mono" if i == 3 else "xs", anchor="start"))

    rx = 470
    body.append(label(rx, 30, "20 kg, pushed at 2 m/s for 1.5 s, then left", cls="sm", anchor="start"))
    rows = [[(name, "xs"), v, mv, ov, co] for name, v, mv, ov, co, col in PUSH]
    t, th = tbl(rx, 58, ["the proxy", "crate v", "moves at", "overlap mm", "coast m"], rows,
                [160, 70, 76, 88, 70])
    body.extend(t)
    ty = 58 + th + 20
    lines = [f"steered: the coast is the discrete v²/(2μg), plus ONE step at v — the",
             f"proxy follows the character a step late: predicted {PUSH_PRED['coast']:.4f} m.",
             "teleported, asleep: a teleported proxy moves at zero speed, so 8.12's",
             "wake rule never wakes the crate — the character walks THROUGH it.",
             "teleported, awake: moved, with a velocity of zero; on release it is",
             f"pushed out of the overlap (≈ v·h/β = {PUSH_PRED['out']:.4f} m) and stops dead.",
             "Baumgarte gives the velocity back, and leaves the character 138 mm in.",
             "",
             f"a 100 kg crate (limit 40): a wall — it moves 0.00000 m/s.",
             f"a 200 kg boulder at 5 m/s into a standing character: {BOULDER[0]:.4f} m/s after",
             "1.5 s. The proxy is kinematic: infinitely heavy. A draft recover()",
             f"that also pushed out of heavy DYNAMIC bodies let it carry the character",
             f"{DRAFT_RECOVER[0]:.2f} m — the solver moving a body the controller owns."]
    for i, t_ in enumerate(lines):
        if t_:
            body.append(label(rx, ty + i * 15, t_, cls="xs", anchor="start"))
    Hh = max(446 + 4 * 15, ty + len(lines) * 15) + 12
    return svg(uid, W, Hh,
               "A crate pushed by a steered proxy and by a teleported one",
               "Left: two diagrams of a character's proxy pushing a crate — steered, overlapping it by "
               "the solver's 5 mm slop at 2 m/s; teleported, sunk 138 mm into it with zero velocity — "
               "and the derivation of the 138 mm. Right: a table of the crate's velocity, apparent "
               "speed, overlap and coast for four proxy modes, and notes on the heavy crate and the "
               "boulder.", body)


FIGS = [fig1, fig2, fig3, fig4, fig5, fig6, fig7, fig8, fig9, fig10, fig11, fig12]


def main():
    for i, f in enumerate(FIGS, start=1):
        write(f"l813_fig{i}.svg", f())


if __name__ == "__main__":
    main()
