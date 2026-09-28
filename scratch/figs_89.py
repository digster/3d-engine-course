#!/usr/bin/env python3
"""scratch/figs_89.py — Lesson 8.9's diagrams.

Same rules as 5.1-8.8's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - no hardcoded colour on an `.ink` stroke either
  - filenames numbered by PAGE ORDER
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - `rule()` takes a CSS CLASS; `cline()` takes a COLOUR
  - LEGENDS AND ANNOTATION BOXES GO OUTSIDE THE PLOT
  - a SHAPE can leave the viewBox where a label cannot
  - panel heights are COMPUTED, never guessed

Every number below is transcribed from scratch/verify_89.log. Nothing here is
estimated, and the harness section each block came from is named above it.

*** WRITTEN IN LITERAL UNICODE THROUGHOUT, NEVER \\uXXXX. ***

THE COLOUR RULE, inherited from 8.4 through 8.8, with this lesson's reading:
  GREEN  = the impulse solver, and the answer that works
  RED    = the failure — the penalty spring, the wrong order, the creep
  AMBER  = friction, everywhere it appears
  BLUE   = the normal impulse, and the bodies
  PURPLE = the closed form a measurement is checked against
  GREY   = discarded work, and axes

FIGURE 5's POLAR PLOT IS THE MEASUREMENT, not a drawing of the idea: the box
lobe's nine radii are §F's nine measured stopping distances, mirrored through the
symmetry the model actually has (four-fold, because a square has four-fold
symmetry), rather than nine points of a cos curve.

FIGURE 7's TWO LINES ARE CLOSED FORMS, deliberately — v(t) = v0 - mu*g*t and
omega*r = (1/c)*mu*g*t are exact until they meet, and the MEASURED end point is
plotted on top of them as a marker. A figure that plots the simulation against
itself proves nothing.
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


def table_body(*args, **kwargs):
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
# MEASUREMENTS — every one from scratch/verify_89.log, section named
# ===========================================================================

# §A — the penalty spring
SINKS = [(50.0, 3924.0, 14.01, 7.0), (10.0, 19620.0, 31.32, 15.7),
         (1.0, 196200.0, 99.05, 49.5), (0.1, 1962000.1, 313.21, 156.6)]
BEST_60HZ_SINK_MM = 0.681
SWEEP = [("50 mm", 0.40, 108.600, 0.0000), ("10 mm", 0.20, 50.092, 0.0000),
         ("1 mm", 0.35, 18.910, 275.4744), ("0.1 mm", 0.30, 12.550, 366.7545)]
IMPULSE_DEPTH_MM = 12.553
IMPULSE_SWING_MM = 0.000
DYNAMIC_SINK_MM = 20.000
DYNAMIC_MEASURED_MM = 44.061
K_WANTED = 7.848e7
HZ_WANTED = 990

# §B — the effective mass
PLANK_CENTRE_KG = 10.000
PLANK_END_KG = 2.702
PLANK_RATIO = 3.70
TWO_FORMS = 5.016e-07
REDUCED = 2.100000

# §C — one impulse
CLOSED_FORM_WORST = 2.384e-07
ENERGY_REL = 1.21e-07
ENERGY_BROKEN = 9.55e-01

# §D — restitution
PEAKS = [(0, 0.80037, 0.85487, 0.80037), (1, 0.51224, 0.58227, 0.51152),
         (2, 0.32783, 0.39832, 0.31380), (3, 0.20981, 0.27785, 0.19691),
         (4, 0.13428, 0.20044, 0.12473), (5, 0.08594, 0.15189, 0.07783),
         (6, 0.05500, 0.11269, 0.04875), (7, 0.03520, 0.09125, 0.02901),
         (8, 0.02253, 0.07134, 0.01962), (9, 0.01442, 0.05649, 0.01170)]
TERMINALS = [(0.50, 0.1635, 0.1635, 1.36), (0.80, 0.6540, 0.6540, 21.80),
             (0.95, 3.1065, 3.3759, 580.87)]
GH = 9.81 / 60.0

# §F — friction
SWEEP_THETA = [(0.0, 1.5977), (11.2, 1.5372), (22.5, 1.3801), (33.8, 1.2022),
               (45.0, 1.1202), (56.3, 1.2022), (67.5, 1.3800), (78.8, 1.5372),
               (90.0, 1.5977)]
BOX_SPREAD_PCT = 42.68
CONE_SPREAD_PCT = 0.00
RATIO_MEASURED = 0.70085
RATIO_PREDICTED = 0.70711

# §G — the slope
SLOPES = [(0.20, 11.3099, 11.3063, 11.3026), (0.35, 19.2900, 19.2494, 19.2760),
          (0.50, 26.5651, 26.1909, 26.5471), (0.75, 36.8699, 35.7921, 36.7552),
          (1.00, 45.0000, 41.2495, 44.5711)]
TIP_CUBE = 45.0
TIP_SLAB = 73.3
TIP_MEASURED = 45.0032

# §I — rolling
ROLL_V0 = 6.0
ROLL_MU = 0.40
ROLL_C = 2.0 / 5.0
ROLL_FRACTION_EXACT = 5.0 / 7.0
ROLL_FRACTION_SIM = 0.713877
ROLL_T_EXACT = 0.4369
ROLL_T_SIM = 0.4500
ROLL_PREDICTION_WORST = 7.75e-07

# §J — what is left behind
PASSES = [(1, 274.5697, 15.48667, 2.633e-02), (2, 61.1845, 3.40328, 2.898e-02),
          (4, 22.5282, 0.76687, 5.506e-03), (8, 10.2386, 0.20159, 2.347e-04),
          (16, 7.2457, 0.00138, 1.585e-06), (32, 7.2252, 0.00001, 9.866e-09)]
RESIDUAL_SOURCES = [(1, 3.576e-07, 8.457e-01, 8.017e-02),
                    (2, 4.822e-01, 8.712e-01, 2.642e-01),
                    (3, 1.372e+00, 1.708e+00, 3.866e-01),
                    (4, 7.410e-01, 1.356e+00, 2.600e-01)]
GH2_MM = 2.7250

# §K — the budget
NS_CONE = 139.4
NS_BOX = 130.5
NS_NONE = 89.8
NS_PREPARE = 52.3
NS_UNHOISTED = 134.1
SOLVER_MS = 0.137


# ===========================================================================
# Figure 1 — the spring you cannot afford  (§1)
# ===========================================================================

def fig1():
    uid = "f89a"
    w, h = 980, 470
    body = [cmarker(uid, "blue", BLUE), cmarker(uid, "amber", AMBER),
            cmarker(uid, "grey", GREY)]

    # ---- left panel: the rate the allowed sink demands --------------------
    px, py, pw, ph = 56, 56, 400, 250
    body.append(frame(px, py, pw, ph))
    body.append(label(px, py - 30, "What a penalty spring costs, before anything moves",
                      "sm", "start"))
    body.append(label(px, py - 12, "allowed static sink → stiffness → rate → minimum step rate",
                      "xs muted", "start"))

    lo_hz, hi_hz = 4.0, 1400.0
    lo_mm, hi_mm = 0.05, 100.0

    for hz in (10, 100, 1000):
        x = px + logspan(hz, lo_hz, hi_hz, 0, pw)
        body.append(rule(x, py, x, py + ph, "grid", dash="3 4"))
        body.append(label(x, py + ph + 16, f"{hz} Hz", "xs muted"))
    for mm in (0.1, 1.0, 10.0, 100.0):
        y = logy(mm, lo_mm, hi_mm, py, ph)
        body.append(rule(px, y, px + pw, y, "grid", dash="3 4"))
        txt = f"{mm:g} mm" if mm >= 1 else "0.1 mm"
        body.append(label(px - 8, y + 4, txt, "xs muted", "end"))

    pts = []
    for mm, k, omega, hz in SINKS:
        x = px + logspan(hz, lo_hz, hi_hz, 0, pw)
        y = logy(mm, lo_mm, hi_mm, py, ph)
        pts.append((x, y))
    body.append(poly(pts, RED, width=2.0, close=False))
    for (x, y), (mm, k, omega, hz) in zip(pts, SINKS):
        body.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3.4" fill="{RED}"/>')

    # the 60 Hz line, and the finest sink it allows
    x60 = px + logspan(60.0, lo_hz, hi_hz, 0, pw)
    body.append(cline(x60, py, x60, py + ph, GREEN, width=1.8, dash="5 4"))
    body.append(label(x60 - 6, py + 16, "60 Hz", "xs", "end"))
    y_best = logy(BEST_60HZ_SINK_MM, lo_mm, hi_mm, py, ph)
    body.append(f'<circle cx="{x60:.1f}" cy="{y_best:.1f}" r="4" fill="{GREEN}"/>')

    # the impact point, far off to the right
    x_impact = px + logspan(HZ_WANTED, lo_hz, hi_hz, 0, pw)
    y_impact = logy(1.0, lo_mm, hi_mm, py, ph)
    body.append(f'<circle cx="{x_impact:.1f}" cy="{y_impact:.1f}" r="4" fill="{AMBER}"/>')
    body.append(carrow(x60 + 8, y_impact, x_impact - 8, y_impact, uid, "amber", AMBER,
                       width=1.6, dash="4 3"))

    # BOTH ANNOTATIONS BELOW THE PLOT, never on it. check-page.js's onShape test
    # caught the green one sitting on its own dashed 60 Hz line.
    ann = py + ph + 40
    body.append(cline(px, ann - 4, px + 20, ann - 4, GREEN, width=2.4))
    body.append(label(px + 28, ann,
                      f"at 60 Hz the finest a STABLE spring can hold is "
                      f"{BEST_60HZ_SINK_MM} mm", "xs", "start"))
    body.append(cline(px, ann + 17 - 4, px + 20, ann + 17 - 4, AMBER, width=2.4))
    body.append(label(px + 28, ann + 17,
                      f"1 mm under a 1.96 m/s IMPACT instead needs {HZ_WANTED} Hz",
                      "xs", "start"))

    # ---- right panel: the sweep -------------------------------------------
    tx = 520
    rows = [[s, f"{z:.2f}", f"{d:.1f}", ("0.00" if sw < 0.01 else f"{sw:.0f}")]
            for s, z, d, sw in SWEEP]
    rows.append(["impulse, 16×", "—", f"{IMPULSE_DEPTH_MM:.2f}", "0.00"])
    body += table_body(tx, py - 20,
                       ["tuned for", "best ζ", "depth mm", "bounce mm"],
                       rows, [150, 80, 100, 110],
                       title="The best of 164 penalty settings, per stiffness")

    ay = py + 150
    lines, dh = para(tx, ay,
                     "Read it as a TRADE. At 60 Hz you may have a crate that settles, 108 mm "
                     "into the floor; or one 12.6 mm in that bounces 367 mm forever. Not both. "
                     "The impulse solver is the last row: it takes both, and there is no knob "
                     "on it to turn.", cols=54, cls="xs", leading=16)
    body += lines
    lines2, _ = para(tx, ay + dh + 14,
                     "And the tuning is a property of the SCENE. The same spring under a "
                     "200 kg crate lets it 962 mm in — through a half-metre floor.",
                     cols=54, cls="xs muted", leading=16)
    body += lines2

    body.append(label(px, h - 22,
                      "§1 and harness §A. Left: every point is m·g/x, ω = √(g/x) and 8.1's "
                      "h·ω < 2, computed rather than drawn.", "xs muted", "start"))

    return svg(uid, w, h, "The rate a penalty spring demands",
               "Left: a log-log plot of allowed penetration against the minimum step rate a "
               "penalty spring needs to stay stable. One millimetre of static sink needs at "
               "least 49.5 Hz; at 60 Hz the finest achievable is 0.681 mm. A millimetre under "
               "a 1.96 m/s impact needs 990 Hz. Right: a table of the best result from 164 "
               "penalty settings at each of four stiffnesses, showing that a settled crate "
               "costs 108 mm of penetration while a shallow one bounces 367 mm forever, and "
               "that the impulse solver achieves 12.55 mm with no residual bounce.", body)


# ===========================================================================
# Figure 2 — one contact, three demands  (§3)
# ===========================================================================

def fig2():
    uid = "f89b"
    w, h = 980, 470
    body = [cmarker(uid, "blue", BLUE), cmarker(uid, "amber", AMBER),
            cmarker(uid, "green", GREEN), cmarker(uid, "red", RED)]

    # ---- left: the geometry ------------------------------------------------
    gx, gy = 70, 60
    floor_y = gy + 220
    body.append(label(gx, gy - 24, "The geometry of one contact point", "sm", "start"))

    # floor
    body.append(cline(gx - 10, floor_y, gx + 330, floor_y, GREY, width=2.2))
    for i in range(0, 34):
        x = gx - 10 + i * 10
        body.append(cline(x, floor_y, x - 7, floor_y + 8, GREY, width=0.8))
    body.append(label(gx - 10, floor_y + 56, "body a — the floor, immovable", "xs muted",
                      "start"))

    # the box, tilted
    cxb, cyb = gx + 150, floor_y - 74
    ang = math.radians(-12.0)
    hx, hy = 92.0, 58.0
    corners = []
    for sx_, sy_ in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        dx = sx_ * hx * math.cos(ang) - sy_ * hy * math.sin(ang)
        dy = sx_ * hx * math.sin(ang) + sy_ * hy * math.cos(ang)
        corners.append((cxb + dx, cyb + dy))
    body.append(poly(corners, BLUE, width=1.8))
    body.append(f'<circle cx="{cxb}" cy="{cyb}" r="3.5" fill="{BLUE}"/>')
    body.append(label(cxb + 104, cyb - 30, "centre of mass of b", "xs", "start"))
    body.append(cline(cxb + 5, cyb - 3, cxb + 100, cyb - 34, GREY, width=0.9))

    # the contact point: the lower-left corner, dropped onto the floor
    px, py = corners[0]
    py = floor_y
    body.append(f'<circle cx="{px:.1f}" cy="{py:.1f}" r="4.5" fill="{GREEN}"/>')
    body.append(cline(px - 4, py + 4, px - 34, py + 30, GREY, width=0.9))
    body.append(label(px - 38, py + 34, "contact point p", "xs", "end"))

    # lever arm r_b
    body.append(carrow(cxb, cyb, px, py, uid, "blue", BLUE, width=1.5))
    body.append(label((cxb + px) / 2 - 16, (cyb + py) / 2 + 4, "r_b", "xs", "end"))

    # normal and tangents
    body.append(carrow(px, py, px, py - 88, uid, "green", GREEN, width=2.0))
    # LEFT of the arrow: the right-hand side is where the velocity annotation's
    # leader runs, and check-page.js found this label sitting on it.
    body.append(label(px - 10, py - 58, "n (a → b)", "xs", "end"))
    body.append(carrow(px, py, px + 84, py, uid, "amber", AMBER, width=2.0))
    body.append(label(px + 88, py + 14, "t₁", "xs", "start"))
    body.append(carrow(px, py, px - 44, py + 26, uid, "amber", AMBER, width=1.6,
                       dash="4 3"))
    body.append(label(px + 92, py + 34, "t₂ points out of the page", "xs muted", "start"))

    # the relative velocity, decomposed
    vx, vy = 52.0, -46.0
    body.append(carrow(px, py, px + vx, py - vy * 0 + vy, uid, "red", RED, width=2.0))
    # The label block sits well above the arrow with a leader to it. Two earlier
    # placements failed check-page.js — one sat on the normal arrow, one on its
    # own leader — which is what the onShape test is for.
    body.append(cline(px + vx + 4, py + vy - 4, px + 145, py - 167, GREY, width=0.9))
    body.append(label(px + 150, py - 175, "u = velocity of b's point,", "xs", "start"))
    body.append(label(px + 150, py - 160, "relative to a's", "xs", "start"))
    body.append(cline(px + vx, py + vy, px + vx, py, RED, width=1.0, dash="3 3"))
    body.append(cline(px, py + vy, px + vx, py + vy, RED, width=1.0, dash="3 3"))

    # ---- right: the three demands -----------------------------------------
    tx = 470
    ty = 40
    body.append(label(tx, ty, "Three demands on one number each", "sm", "start"))

    demands = [
        (GREEN, "NON-PENETRATION",
         "u·n must not be negative afterwards. The impulse that enforces it may only "
         "PUSH: j ≥ 0. That clamp is the only nonlinearity in the whole normal solve."),
        (BLUE, "RESTITUTION",
         "u·n must be −e times the speed it arrived with. It rides on the SAME equation: "
         "change the target from 0 to −e·(arrival) and nothing else moves. So e never "
         "appears in the solver at all — only in the target."),
        (AMBER, "FRICTION",
         "u·t₁ and u·t₂ both want to be zero, which is what 'not sliding' means — but only "
         "as far as |j_t| ≤ μ·j_n allows. The cone's RADIUS is the answer to the demand "
         "above it, which is why the two cannot be solved independently."),
    ]
    yy = ty + 26
    for colour, head, text in demands:
        body.append(cline(tx, yy - 4, tx + 18, yy - 4, colour, width=3.0))
        body.append(label(tx + 26, yy, head, "xs", "start"))
        lines, dh = para(tx + 26, yy + 18, text, cols=54, cls="xs muted", leading=16)
        body += lines
        yy += 18 + dh + 22

    lines, _ = para(tx, yy + 6,
                    "Each is one linear equation in one scalar unknown, because the change in "
                    "u along any direction is proportional to the impulse along it. That "
                    "constant of proportionality is the effective mass — Figure 3.",
                    cols=58, cls="xs", leading=16)
    body += lines

    body.append(label(gx, h - 16,
                      "§3. The sign convention is collide.hpp's, unchanged since 8.4: n points "
                      "from a toward b, so u·n < 0 means approaching.", "xs muted", "start"))

    return svg(uid, w, h, "The geometry of one contact, and the three demands on it",
               "Left: a tilted box resting on a floor, with the contact point marked, the "
               "lever arm from the box's centre of mass to it, the contact normal pointing "
               "up from the floor toward the box, the two tangent directions spanning the "
               "contact plane, and the relative velocity at the point decomposed into its "
               "normal and tangential parts. Right: the three demands a contact makes — "
               "non-penetration, restitution and friction — each one linear equation in one "
               "scalar unknown, with friction's limit set by the normal impulse.", body)


# ===========================================================================
# Figure 3 — the effective mass  (§4)
# ===========================================================================

def fig3():
    uid = "f89c"
    w, h = 980, 400
    body = [cmarker(uid, "green", GREEN), cmarker(uid, "blue", BLUE)]

    body.append(label(60, 40, "The same 10 kg plank, pushed in two places", "sm", "start"))

    for idx, (label_text, offset, mass) in enumerate(
            [("pushed at the centre", 0.0, PLANK_CENTRE_KG),
             ("pushed 1.9 m out", 1.9, PLANK_END_KG)]):
        ox = 70 + idx * 300
        oy = 150
        half_px = 120.0
        body.append(f'<rect x="{ox}" y="{oy - 14}" width="{2 * half_px}" height="28" rx="3" '
                    f'fill="none" stroke="{BLUE}" stroke-width="1.8"/>')
        cx = ox + half_px
        body.append(f'<circle cx="{cx}" cy="{oy}" r="3.5" fill="{BLUE}"/>')
        body.append(label(cx, oy - 24, "centre of mass", "xs muted"))

        px = cx + (offset / 2.0) * half_px
        body.append(carrow(px, oy + 78, px, oy + 20, uid, "green", GREEN, width=2.2))
        body.append(f'<circle cx="{px:.1f}" cy="{oy + 14}" r="4" fill="{GREEN}"/>')
        body.append(label(px, oy + 96, label_text, "xs", "middle"))

        if offset > 0:
            body.append(cline(cx, oy + 44, px, oy + 44, GREY, width=1.0, dash="3 3"))
            body.append(label((cx + px) / 2, oy + 40, "r", "xs muted"))
            # the rotation it causes
            body.append(f'<path d="M {cx - 40} {oy - 40} A 46 46 0 0 1 {cx + 26} {oy - 52}" '
                        f'fill="none" stroke="{AMBER}" stroke-width="1.6" '
                        f'marker-end="url(#e-blue-{uid})"/>')
            body.append(label(cx - 6, oy - 62, "and it turns", "xs muted"))

        body.append(label(cx, oy + 130, f"effective mass {mass:.3f} kg", "sm"))

    body.append(f'<text x="{70 + 300 + 120}" y="{150 + 160}" class="xs" '
                f'text-anchor="middle">{esc(f"{PLANK_RATIO}× lighter")}</text>')

    tx = 640
    lines, dh = para(tx, 60,
                     "A contact does not feel the body's mass. It feels how hard the body is "
                     "to move ALONG THE CONTACT NORMAL, at that point — and an off-centre push "
                     "partly turns the body instead of moving it, so less of the impulse "
                     "arrives where the contact is.", cols=52, cls="xs", leading=16)
    body += lines
    lines2, dh2 = para(tx, 60 + dh + 16,
                       "That is the whole content of the angular terms in k. Set r × n to zero "
                       "and they vanish exactly: a contact straight through both centres of "
                       "mass has k = 1/m_a + 1/m_b, whose reciprocal is the reduced mass of "
                       "elementary mechanics — measured at "
                       f"{REDUCED:.6f} kg for 3 kg against 7 kg.",
                       cols=52, cls="xs muted", leading=16)
    body += lines2
    lines3, _ = para(tx, 60 + dh + 16 + dh2 + 16,
                     "Written as w·(I⁻¹w) with w = r × n the angular terms are manifestly "
                     "non-negative, which PROVES k ≥ 1/m_a + 1/m_b and therefore that the "
                     "division is always safe. Measured over 200,000 random contacts, the gap "
                     "never once went negative.", cols=52, cls="xs muted", leading=16)
    body += lines3

    body.append(label(60, h - 22,
                      "§4 and harness §B. Both masses are measured, not computed for the "
                      "figure; the two algebraic forms of k agree to "
                      f"{TWO_FORMS:.1e} relative.", "xs muted", "start"))

    return svg(uid, w, h, "The effective mass of a contact",
               "The same ten-kilogram plank pushed upward in two places. Pushed at its centre "
               "of mass the contact sees the full ten kilograms. Pushed 1.9 metres out, part "
               "of the impulse turns the plank instead of lifting it, and the contact sees "
               "2.702 kilograms — 3.7 times lighter. The angular terms of the effective mass "
               "are exactly this effect, and they vanish when the lever arm is parallel to "
               "the normal.", body)


# ===========================================================================
# Figure 4 — restitution, and the gravity already in the velocity  (§6)
# ===========================================================================

def fig4():
    uid = "f89d"
    w, h = 980, 500
    body = [cmarker(uid, "red", RED), cmarker(uid, "green", GREEN)]

    px, py, pw, ph = 66, 64, 470, 280
    body.append(frame(px, py, pw, ph))
    body.append(label(px, py - 32, "Ten bounces, against the closed form e²ⁿ·h₀", "sm", "start"))
    body.append(label(px, py - 14,
                      "e = 0.80, 60 Hz. The purple line is arithmetic, not a simulation.",
                      "xs muted", "start"))

    lo, hi = 0.008, 1.0
    for v in (0.01, 0.1, 1.0):
        y = logy(v, lo, hi, py, ph)
        body.append(rule(px, y, px + pw, y, "grid", dash="3 4"))
        body.append(label(px - 8, y + 4, f"{v:g} m", "xs muted", "end"))
    step = pw / 9.5
    for n in range(10):
        x = px + 14 + n * step
        body.append(label(x, py + ph + 18, str(n), "xs muted"))
    body.append(label(px + pw / 2, py + ph + 38, "bounce number", "xs muted"))

    exact = [(px + 14 + n * step, logy(e, lo, hi, py, ph)) for n, e, u, c in PEAKS]
    raw = [(px + 14 + n * step, logy(u, lo, hi, py, ph)) for n, e, u, c in PEAKS]
    fixed = [(px + 14 + n * step, logy(c, lo, hi, py, ph)) for n, e, u, c in PEAKS]
    body.append(poly(exact, PURPLE, width=2.4, close=False, dash="6 4"))
    body.append(poly(raw, RED, width=2.0, close=False))
    body.append(poly(fixed, GREEN, width=2.0, close=False))
    for pts, colour in ((raw, RED), (fixed, GREEN)):
        for x, y in pts:
            body.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="2.8" fill="{colour}"/>')

    # OUTSIDE the plot: the first draft put this at py + 20 and check-page.js's
    # onShape test found two of its labels sitting on the plotted curves.
    body += legend(px, py + ph + 58,
                   [(PURPLE, "e²ⁿ·h₀ — the closed form"),
                    (GREEN, "restitution_bias set"),
                    (RED, "bias left at zero")])

    # ---- right: the fixed point -------------------------------------------
    tx = 590
    body.append(label(tx, py - 32, "Where the uncorrected bounce is GOING", "sm", "start"))
    rows = [[f"{e:.2f}", f"{pred:.4f}", f"{meas:.4f}", f"{hop:.1f}"]
            for e, pred, meas, hop in TERMINALS]
    body += table_body(tx, py - 14, ["e", "predicted", "measured", "hop mm"],
                       rows, [70, 110, 110, 90])

    yy = py + 110
    lines, dh = para(tx, yy,
                     "A semi-implicit step applies gravity BEFORE the solver looks at the "
                     f"contact, so the approach speed is already too large by g·h = "
                     f"{GH * 100:.2f} cm/s. Restitution returns e times a speed that includes "
                     "it, every bounce, and the error does not shrink as the bounce does.",
                     cols=50, cls="xs", leading=16)
    body += lines
    lines2, dh2 = para(tx, yy + dh + 14,
                       "So the bounce has a FIXED POINT: v = e·(v + g·h) solves to "
                       "e·g·h/(1 − e). The ball stops decaying and hops forever — 2.2 cm at "
                       "e = 0.8, and 58 cm at e = 0.95, which no restitution threshold will "
                       "hide. Subtracting the bias removes it exactly.",
                       cols=50, cls="xs muted", leading=16)
    body += lines2

    body.append(label(px, h - 22,
                      "§6 and harness §D. The predicted and measured terminal speeds agree to "
                      "1.000 at e = 0.5 and e = 0.8; the 0.95 row has not finished converging "
                      "in the thirty seconds it was given.", "xs muted", "start"))

    return svg(uid, w, h, "Restitution applied to a velocity that already contains gravity",
               "Left: ten bounce heights on a logarithmic axis. The closed form e to the power "
               "2n times the first peak is drawn as a dashed line; a run with the restitution "
               "bias set tracks it down to a millimetre, while a run without the bias climbs "
               "away from it and flattens out. Right: a table of the predicted and measured "
               "terminal bounce speeds at three restitutions, which agree exactly at 0.5 and "
               "0.8, and an explanation of the fixed point e times g times h over one minus "
               "e that the uncorrected bounce converges to.", body)


# ===========================================================================
# Figure 5 — the friction cone, and the square that is not one  (§7)
# ===========================================================================

def fig5():
    uid = "f89e"
    w, h = 980, 560
    body = [cmarker(uid, "amber", AMBER), cmarker(uid, "grey", GREY)]

    # ---- left: the admissible set -----------------------------------------
    cx, cy, r = 220, 262, 130
    body.append(label(66, 40, "The admissible set for the tangential impulse", "sm", "start"))
    body.append(label(66, 58, "in the contact plane, in units of μ·j_n", "xs muted", "start"))

    body.append(cline(cx - r - 40, cy, cx + r + 40, cy, GREY, width=1.0))
    body.append(cline(cx, cy - r - 40, cx, cy + r + 40, GREY, width=1.0))
    body.append(label(cx + r + 46, cy + 4, "t₁", "xs muted", "start"))
    body.append(label(cx, cy - r - 48, "t₂", "xs muted"))

    body.append(f'<rect x="{cx - r}" y="{cy - r}" width="{2 * r}" height="{2 * r}" '
                f'fill="none" stroke="{RED}" stroke-width="2.0"/>')
    body.append(f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="none" stroke="{GREEN}" '
                f'stroke-width="2.4"/>')

    # the diagonal, where the square is sqrt(2) out
    dx = r
    body.append(carrow(cx, cy, cx + dx, cy - dx, uid, "amber", AMBER, width=2.0))
    body.append(label(cx + dx + 10, cy - dx - 8, "√2 · μ·j_n", "xs", "start"))
    body.append(carrow(cx, cy, cx + r, cy, uid, "grey", GREY, width=1.6, dash="4 3"))
    body.append(label(cx + r + 10, cy + 20, "μ·j_n", "xs muted", "start"))

    body += legend(66, cy + r + 56,
                   [(GREEN, "cone: |j_t| ≤ μ·j_n — a disc, and round"),
                    (RED, "box: each axis clipped alone — a square, and not")])

    # ---- right: the measured polar sweep ----------------------------------
    ox, oy, rr = 700, 252, 132
    body.append(label(540, 48, "What that costs, measured", "sm", "start"))
    body.append(label(540, 66,
                      "sliding distance of a 5 kg crate at 4 m/s, μ = 0.50, by direction",
                      "xs muted", "start"))

    d_min = min(d for _t, d in SWEEP_THETA)
    d_max = max(d for _t, d in SWEEP_THETA)

    def radius_of(d):
        # *** PROPORTIONAL TO THE DISTANCE, WITH THE ORIGIN AT ZERO. *** The
        # first version scaled the smallest measured distance to 0.45 of the
        # radius, which made the lobe dramatic and the RATIO — the one thing
        # this plot exists to show — a lie. A polar plot with a suppressed zero
        # cannot be read as a shape.
        return rr * d / d_max

    # The predicted floor, 1/sqrt(2) of the outer ring. The measured lobe should
    # touch it on the diagonals and nowhere else.
    body.append(f'<circle cx="{ox}" cy="{oy}" r="{rr * RATIO_PREDICTED:.1f}" fill="none" '
                f'stroke="{GREY}" stroke-width="1.0" stroke-dasharray="4 4"/>')
    body.append(cline(ox - rr - 24, oy, ox + rr + 24, oy, GREY, width=0.9))
    body.append(cline(ox, oy - rr - 24, ox, oy + rr + 24, GREY, width=0.9))

    # The cone is a circle: one measured value at every angle.
    cone_r = radius_of(SWEEP_THETA[0][1])
    body.append(f'<circle cx="{ox}" cy="{oy}" r="{cone_r:.1f}" fill="none" stroke="{GREEN}" '
                f'stroke-width="2.4"/>')

    # The box lobe: the nine measured radii, extended over the full turn by the
    # four-fold symmetry a square has.
    lobe = []
    for quadrant in range(4):
        for idx in range(len(SWEEP_THETA) - 1):
            theta_deg, d = SWEEP_THETA[idx]
            a = math.radians(quadrant * 90.0 + theta_deg)
            rad = radius_of(d)
            lobe.append((ox + rad * math.cos(a), oy - rad * math.sin(a)))
    body.append(poly(lobe, RED, width=2.0, close=True))
    for x, y in lobe:
        body.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="2.4" fill="{RED}"/>')

    body.append(label(ox + cone_r + 10, oy - 10, f"{SWEEP_THETA[0][1]:.3f} m", "xs", "start"))
    diag_r = radius_of(SWEEP_THETA[4][1])
    ax = ox + diag_r * math.cos(math.radians(225))
    ay = oy - diag_r * math.sin(math.radians(225))
    body.append(cline(ax, ay, ox - rr - 16, oy + rr + 6, GREY, width=0.9))
    body.append(label(ox - rr - 20, oy + rr + 10, f"{SWEEP_THETA[4][1]:.3f} m", "xs", "end"))
    body.append(label(ox + rr * RATIO_PREDICTED * 0.72, oy - rr * RATIO_PREDICTED * 0.72 - 6,
                      "1/√2 predicted", "xs muted", "middle"))

    yy = oy + rr + 54
    lines, _ = para(540, yy,
                    f"Over 361 directions the box model spreads {BOX_SPREAD_PCT}% and the cone "
                    f"{CONE_SPREAD_PCT:.2f}%. The extreme ratio is {RATIO_MEASURED:.5f} against "
                    f"a predicted 1/√2 = {RATIO_PREDICTED:.5f}. The tangent basis was chosen "
                    "from the normal's smallest component, so under the box model a floor "
                    "decides which way is slippery.", cols=48, cls="xs", leading=16)
    body += lines

    body.append(label(66, h - 14,
                      f"§7 and harness §F. The cone costs {NS_CONE - NS_BOX:.1f} ns per "
                      f"manifold more than the box — {100 * (NS_CONE - NS_BOX) / NS_BOX:.1f}% — "
                      "to remove a 43% anisotropy.", "xs muted", "start"))

    return svg(uid, w, h, "The friction cone, and the square that circumscribes it",
               "Left: the admissible set for the tangential impulse. Coulomb's condition is a "
               "disc of radius mu times the normal impulse; clipping each tangent component "
               "independently gives a square that circumscribes it, so up to the square root "
               "of two times as much friction is available along the diagonals. Right: a polar "
               "plot of measured sliding distance against sliding direction. The cone model is "
               "a perfect circle; the box model is a four-lobed figure whose shortest radius "
               "is 0.70 of its longest, matching one over root two.", body)


# ===========================================================================
# Figure 6 — sliding, and tipping  (§8)
# ===========================================================================

def fig6():
    uid = "f89f"
    w, h = 980, 530
    body = [cmarker(uid, "green", GREEN), cmarker(uid, "red", RED)]

    px, py, pw, ph = 66, 70, 420, 250
    body.append(frame(px, py, pw, ph))
    body.append(label(px, py - 34, "The critical angle, bisected against the engine", "sm",
                      "start"))
    body.append(label(px, py - 16, "two shapes, same μ, same solver", "xs muted", "start"))

    lo_mu, hi_mu = 0.1, 1.15
    lo_deg, hi_deg = 5.0, 50.0

    def mx(mu):
        return px + (mu - lo_mu) / (hi_mu - lo_mu) * pw

    def my(deg):
        return py + ph - (deg - lo_deg) / (hi_deg - lo_deg) * ph

    for deg in (10, 20, 30, 40, 50):
        y = my(deg)
        body.append(rule(px, y, px + pw, y, "grid", dash="3 4"))
        body.append(label(px - 8, y + 4, f"{deg}°", "xs muted", "end"))
    for mu in (0.2, 0.4, 0.6, 0.8, 1.0):
        x = mx(mu)
        body.append(rule(x, py, x, py + ph, "grid", dash="3 4"))
        body.append(label(x, py + ph + 18, f"{mu:.1f}", "xs muted"))
    body.append(label(px + pw / 2, py + ph + 38, "coefficient of friction μ", "xs muted"))

    # the tipping ceiling for a cube
    y_tip = my(TIP_CUBE)
    body.append(cline(px, y_tip, px + pw, y_tip, GREY, width=1.6, dash="6 4"))
    body.append(label(px + pw - 6, y_tip - 8, "a cube tips at 45°", "xs muted", "end"))

    exact = [(mx(mu), my(deg)) for mu, deg, _c, _s in SLOPES]
    cube = [(mx(mu), my(c)) for mu, _deg, c, _s in SLOPES]
    slab = [(mx(mu), my(s)) for mu, _deg, _c, s in SLOPES]
    body.append(poly(exact, PURPLE, width=2.4, close=False, dash="6 4"))
    body.append(poly(cube, RED, width=2.0, close=False))
    body.append(poly(slab, GREEN, width=2.0, close=False))
    for pts, colour in ((cube, RED), (slab, GREEN)):
        for x, y in pts:
            body.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="2.8" fill="{colour}"/>')

    body += legend(px + 10, py + 22,
                   [(PURPLE, "atan(μ) — the closed form"),
                    (GREEN, "a 10:3 slab, which tips at 73°"),
                    (RED, "a cube, which tips at 45°")])

    # ---- right --------------------------------------------------------------
    tx = 540
    body.append(label(tx, py - 34, "Two limits, and a block obeys whichever comes first",
                      "sm", "start"))

    # two little block pictures
    for idx, (title, w_h, tip) in enumerate([("cube, w/h = 1", 1.0, TIP_CUBE),
                                             ("slab, w/h = 10/3", 10.0 / 3.0, TIP_SLAB)]):
        bx = tx + idx * 210
        by = py + 40
        bw = 74.0
        bh = bw / w_h
        a = math.radians(20.0)
        body.append(cline(bx - 20, by + 70, bx + 150, by + 70 - 170 * math.tan(a) + 60,
                          GREY, width=1.6))
        cxx, cyy = bx + 60, by + 40
        pts = []
        for sx_, sy_ in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            ddx = sx_ * bw / 2 * math.cos(-a) - sy_ * bh / 2 * math.sin(-a)
            ddy = sx_ * bw / 2 * math.sin(-a) + sy_ * bh / 2 * math.cos(-a)
            pts.append((cxx + ddx, cyy + ddy))
        body.append(poly(pts, BLUE, width=1.6))
        body.append(label(cxx, by + 96, title, "xs"))
        body.append(label(cxx, by + 114, f"tips at {tip:.1f}°", "xs muted"))

    yy = py + 200
    lines, dh = para(tx, yy,
                     "A block slides when tan θ > μ and TIPS when tan θ > w/h, and it does "
                     "whichever happens first. A cube's two limits coincide at 45°, so the "
                     "cube column above is not measuring μ at all past about 0.75 — it is "
                     "measuring the fixture.", cols=47, cls="xs", leading=16)
    body += lines
    lines2, _ = para(tx, yy + dh + 14,
                     "The tell is that the error GROWS with the parameter: 0.004° out at "
                     "μ = 0.2 and 3.75° at μ = 1.0. A wrong constant would have been wrong "
                     "at both. The control is the same instrument measuring the other "
                     f"formula — μ = 5, where sliding is impossible, lets go at "
                     f"{TIP_MEASURED:.4f}° against a predicted 45°.",
                     cols=47, cls="xs muted", leading=16)
    body += lines2

    body.append(label(px, h - 14,
                      "§8 and harness §G. Both columns are bisections of the engine's own "
                      "behaviour over sixteen steps, 2 s of drift, a 5 mm threshold.",
                      "xs muted", "start"))

    return svg(uid, w, h, "The critical slope, and the two limits that compete for it",
               "Left: the angle at which a block starts to move, plotted against its "
               "coefficient of friction. The closed form arctangent of mu is drawn as a dashed "
               "line. A flat slab tracks it to within half a degree across the range; a cube "
               "falls away from it above mu of about 0.5 and is nearly four degrees low at mu "
               "of one. Right: the reason — a block slides when the tangent of the angle "
               "exceeds mu and tips when it exceeds its width over its height, and a cube's "
               "two limits coincide at 45 degrees.", body)


# ===========================================================================
# Figure 7 — sliding becomes rolling  (§9)
# ===========================================================================

def fig7():
    uid = "f89g"
    w, h = 980, 440
    body = [cmarker(uid, "amber", AMBER), cmarker(uid, "green", GREEN)]

    px, py, pw, ph = 66, 64, 470, 250
    body.append(frame(px, py, pw, ph))
    body.append(label(px, py - 32, "A ball landing at 6 m/s with no spin, μ = 0.40", "sm",
                      "start"))
    body.append(label(px, py - 14,
                      "closed forms; the measured end point is the marker on top of them",
                      "xs muted", "start"))

    t_max = 0.62
    v_max = 6.6

    def tx_(t):
        return px + t / t_max * pw

    def vy_(v):
        return py + ph - v / v_max * ph

    for v in (0, 2, 4, 6):
        y = vy_(v)
        body.append(rule(px, y, px + pw, y, "grid", dash="3 4"))
        body.append(label(px - 8, y + 4, f"{v} m/s", "xs muted", "end"))
    for t in (0.1, 0.2, 0.3, 0.4, 0.5, 0.6):
        x = tx_(t)
        body.append(rule(x, py, x, py + ph, "grid", dash="3 4"))
        body.append(label(x, py + ph + 18, f"{t:.1f}", "xs muted"))
    body.append(label(px + pw / 2, py + ph + 38, "seconds after landing", "xs muted"))

    g = 9.81
    a_lin = ROLL_MU * g
    a_ang = ROLL_MU * g / ROLL_C
    t_meet = ROLL_V0 / (a_lin + a_ang)
    v_meet = ROLL_V0 - a_lin * t_meet

    v_line = [(tx_(0), vy_(ROLL_V0)), (tx_(t_meet), vy_(v_meet)),
              (tx_(t_max), vy_(v_meet))]
    w_line = [(tx_(0), vy_(0.0)), (tx_(t_meet), vy_(v_meet)), (tx_(t_max), vy_(v_meet))]
    body.append(poly(v_line, BLUE, width=2.2, close=False))
    body.append(poly(w_line, AMBER, width=2.2, close=False))

    body.append(rule(px, vy_(ROLL_V0 * ROLL_FRACTION_EXACT), px + pw,
                     vy_(ROLL_V0 * ROLL_FRACTION_EXACT), "grid", dash="2 5"))
    body.append(label(px + pw - 6, vy_(ROLL_V0 * ROLL_FRACTION_EXACT) - 8,
                      "5/7 · v₀ = 4.2857 m/s", "xs", "end"))
    body.append(f'<circle cx="{tx_(ROLL_T_SIM):.1f}" '
                f'cy="{vy_(ROLL_V0 * ROLL_FRACTION_SIM):.1f}" r="4.5" fill="{GREEN}"/>')
    body.append(cline(tx_(ROLL_T_SIM), vy_(ROLL_V0 * ROLL_FRACTION_SIM) + 6,
                      tx_(ROLL_T_SIM), py + ph, GREY, width=0.9, dash="3 3"))

    # OUTSIDE the plot, for figure 4's reason.
    body += legend(px, py + ph + 58,
                   [(BLUE, "v — the centre, slowing at μ·g"),
                    (AMBER, "ω·r — the surface, spinning up at μ·g/c"),
                    (GREEN, f"measured: {ROLL_V0 * ROLL_FRACTION_SIM:.4f} m/s at "
                            f"{ROLL_T_SIM:.4f} s, against 5/7·v₀ = "
                            f"{ROLL_V0 * ROLL_FRACTION_EXACT:.4f}")])

    tx = 590
    lines, dh = para(tx, 60,
                     "Friction acts BELOW the centre of mass, so it does two things at once: "
                     "it slows the ball down and it spins the ball up. The gap closes at "
                     "μ·g·(1 + 1/c) and when it reaches zero the contact point is stationary, "
                     "so friction has nothing left to act on.", cols=50, cls="xs", leading=16)
    body += lines
    lines2, dh2 = para(tx, 60 + dh + 14,
                       "Where they meet does NOT depend on μ: cancel it and the answer is "
                       "v₀/(1 + c), which is 5/7 for a solid sphere and 3/5 for a shell. "
                       "μ decides only how long the transition takes — measured at 0.88 s, "
                       "0.45 s and 0.23 s for μ of 0.2, 0.4 and 0.8, with the same end speed "
                       "to six decimal places.", cols=50, cls="xs muted", leading=16)
    body += lines2
    lines3, _ = para(tx, 60 + dh + 14 + dh2 + 14,
                     "The 0.06% the measurement misses 5/7 by is not noise. The contact point "
                     "sits depth/2 inside the ball, so the lever arm is R − depth/2, and the "
                     "corrected closed form tracks the measurement to "
                     f"{ROLL_PREDICTION_WORST:.0e} across a 150× sweep of penetration.",
                     cols=50, cls="xs muted", leading=16)
    body += lines3

    body.append(label(px, h - 14,
                      "§9 and harness §I. The two lines are algebra; only the green marker is "
                      "a simulation.", "xs muted", "start"))

    return svg(uid, w, h, "Friction turning a slide into a roll",
               "The speed of a ball's centre and the speed of its surface, against time after "
               "it lands at six metres per second with no spin. The centre slows linearly and "
               "the surface speeds up linearly; where they meet, at five sevenths of the "
               "initial speed, the contact point is stationary and the sliding stops. The "
               "meeting point does not depend on the coefficient of friction, which sets only "
               "how quickly the two lines converge.", body)


# ===========================================================================
# Figure 8 — what one pass leaves behind  (§12)
# ===========================================================================

def fig8():
    uid = "f89h"
    w, h = 980, 450
    body = [cmarker(uid, "red", RED), cmarker(uid, "green", GREEN)]

    px, py, pw, ph = 66, 68, 400, 250
    body.append(frame(px, py, pw, ph))
    body.append(label(px, py - 34, "A crate at rest, fifteen seconds in", "sm", "start"))
    body.append(label(px, py - 16, "the only thing changing is the number of solver passes",
                      "xs muted", "start"))

    lo, hi = 5.0, 400.0
    for v in (10, 100):
        y = logy(v, lo, hi, py, ph)
        body.append(rule(px, y, px + pw, y, "grid", dash="3 4"))
        body.append(label(px - 8, y + 4, f"{v} mm", "xs muted", "end"))
    step = pw / 6.0
    xs = []
    for i, (n, depth, creep, resid) in enumerate(PASSES):
        x = px + 28 + i * step
        xs.append(x)
        body.append(label(x, py + ph + 18, str(n), "xs muted"))
    body.append(label(px + pw / 2, py + ph + 38, "solver passes per step", "xs muted"))

    pts = [(x, logy(d, lo, hi, py, ph)) for x, (_n, d, _c, _r) in zip(xs, PASSES)]
    body.append(poly(pts, RED, width=2.2, close=False))
    for i, (x, y) in enumerate(pts):
        colour = RED if PASSES[i][0] < 8 else GREEN
        body.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3.6" fill="{colour}"/>')

    y_floor = logy(PASSES[-1][1], lo, hi, py, ph)
    body.append(cline(px, y_floor, px + pw, y_floor, GREEN, width=1.4, dash="5 4"))
    body.append(label(px + pw - 6, y_floor + 18,
                      f"{PASSES[-1][1]:.2f} mm — the depth it ARRIVED with", "xs", "end"))

    # Beside the first point, not centred over it: centred, the words overhung
    # the frame's left edge.
    body.append(label(xs[0] + 9, pts[0][1] - 5, "sinks forever", "xs", "start"))

    # ---- right --------------------------------------------------------------
    tx = 540
    rows = [[str(n), f"{c:.3f}", f"{r:.1e}"] for n, _d, c, r in PASSES]
    body += table_body(tx, py - 34, ["passes", "creep mm/s", "residual m/s"],
                       rows, [110, 150, 150],
                       title="One pass is not a converged solve")

    yy = py + 150
    lines, dh = para(tx, yy,
                     "One pass over a four-point manifold leaves a few centimetres per second "
                     "of residual approach speed, and the crate sinks for as long as you watch "
                     "it. Sixteen passes converge and the depth FREEZES — but it freezes at "
                     "the penetration the crate arrived with, which is one step of approach "
                     "travel and does not go away.", cols=54, cls="xs", leading=16)
    body += lines
    lines2, _ = para(tx, yy + dh + 14,
                     "Those are 8.10's two jobs, and they need different machinery: more "
                     "iterations for the first, a POSITION correction for the second. No "
                     "velocity solve can undo an overlap that is already there. And solving at "
                     "the END of the step instead of the middle adds a third failure — "
                     f"g·h² = {GH2_MM:.4f} mm of sink every step, whatever the solver does.",
                     cols=54, cls="xs muted", leading=16)
    body += lines2

    body.append(label(px, h - 22,
                      "§12 and harness §J. Depth is measured at t = 15 s; creep is the change "
                      "between t = 5 s and t = 15 s.", "xs muted", "start"))

    return svg(uid, w, h, "What one solver pass leaves behind",
               "The penetration of a resting crate after fifteen seconds, against the number "
               "of solver passes per step, on a logarithmic axis. At one pass the crate is 275 "
               "millimetres into the floor and still sinking; at sixteen and thirty-two passes "
               "it has frozen at 7.2 millimetres, which is the penetration it arrived with on "
               "the frame it was first detected. A table gives the creep rate and the residual "
               "velocity at each pass count.", body)


def main():
    write("l89_fig1.svg", fig1())
    write("l89_fig2.svg", fig2())
    write("l89_fig3.svg", fig3())
    write("l89_fig4.svg", fig4())
    write("l89_fig5.svg", fig5())
    write("l89_fig6.svg", fig6())
    write("l89_fig7.svg", fig7())
    write("l89_fig8.svg", fig8())


if __name__ == "__main__":
    main()
