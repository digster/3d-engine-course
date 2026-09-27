#!/usr/bin/env python3
"""scratch/figs_810.py — Lesson 8.10's diagrams.

Same rules as 5.1-8.9's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - no hardcoded colour on an `.ink` stroke either
  - filenames numbered by PAGE ORDER
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - `rule()` takes a CSS CLASS; `cline()` takes a COLOUR
  - LEGENDS AND ANNOTATION BOXES GO OUTSIDE THE PLOT
  - a SHAPE can leave the viewBox where a label cannot
  - panel heights are COMPUTED, never guessed
  - AND NO SUPPRESSED ZERO on any axis whose job is to show a ratio — 8.9's
    figure 5 shipped one and check-page.js could not see it.

Every number below is transcribed from scratch/verify_810.log. Nothing here is
estimated, and the harness section each block came from is named above it.

*** WRITTEN IN LITERAL UNICODE THROUGHOUT, NEVER \\uXXXX. ***

THE COLOUR RULE, inherited from 8.4 through 8.9, with this lesson's reading:
  GREEN  = the answer that works — warm, split impulse, islands, asleep
  RED    = the failure — cold, Baumgarte's launch, the bridged island, the lean
  AMBER  = friction and the cached basis, which is this lesson's friction bug
  BLUE   = the normal impulse, the bodies, and the velocity solve
  PURPLE = the closed form a measurement is checked against
  GREY   = discarded work, and axes

FIGURE 4's HISTOGRAM IS THE MEASUREMENT and it is the reason that figure exists:
the first draft drew one bar at `v*h` because the section claimed an equality,
and 200 drop heights per row said the arrival depth is UNIFORM on [0, v*h]. A
figure drawn from a claim rather than from a log is a figure that argues with
its own caption.
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
# MEASUREMENTS — every one from scratch/verify_810.log, section named
# ===========================================================================

# §A — Gauss-Seidel convergence, one crate on a floor, cold, no correction
RESID = [(1, 2.0296e-02), (2, 9.9972e-03), (4, 3.0210e-03), (8, 2.3918e-04),
         (16, 1.6755e-06), (32, 1.1418e-08), (64, 1.1420e-08)]
CONTRACTION = 0.5276
FLOAT_FLOOR = 1.1418e-08
SINK_BY_ITERS = [(1, 113.720), (2, 27.360), (4, 7.250), (8, 1.121),
                 (16, 1.171), (32, 2.155)]
ONE_POINT_RESID = 2.5247e-13

# §B — warm starting
WARM_ITERS = 89
COLD_ITERS_LIMIT = 400
SINK_COLD_MM = 2019.78
SINK_WARM_MM = 40.14
DEEP_COLD_MM = 199.919
DEEP_WARM_MM = 9.165
MATCH_NUM, MATCH_DEN, MATCH_PCT = 11708, 11994, 97.62
SCALE_SWEEP = [(0.0, 400, 2.3484e-02, 0.5975), (0.5, 400, 1.3542e-02, 0.3209),
               (1.0, 89, 1.3309e-04, 0.0048), (2.0, 400, 3.8701e-02, 0.6080),
               (5.0, 52, 2.7670e-01, 1.5402)]
KEEP_SLOP = [(0.000, 19.990, 97.62), (0.002, 19.985, 98.09), (0.010, 20.000, 98.17)]

# §B — the tangent-basis bug
FLIP_NUM, FLIP_DEN, FLIP_PCT, FLIP_WORST = 113, 11830, 0.96, 134.6
DRIFT_FIXED_MM, DRIFT_STALE_MM = 155.43, 379.54

# §C — the order within a sweep
ORDER = [(1, 3.798e-02, 2.122e-02), (4, 2.152e-02, 2.485e-02),
         (16, 1.090e-02, 1.156e-02), (64, 3.357e-03, 3.621e-03),
         (256, 3.623e-04, 3.676e-04)]
ORDER_WORTH_ITERS = 0.12

# §D — the arrival depth
ARRIVAL = [(0.10, 2.1255, 35.425, 0.005, 0.486, 0.919),
           (0.50, 3.5970, 59.950, 0.004, 0.489, 0.950),
           (2.00, 6.5400, 109.000, 0.002, 0.505, 0.972)]
FROZEN = [(1, 17.6959), (2, 18.1701), (4, 18.2592), (8, 17.7703),
          (16, 17.7503), (32, 17.7501), (64, 17.7501)]
FROZEN_SPREAD_PCT = 3.09
SLOW_CEILING_MM, SLOW_MEASURED_MM = 0.1667, 0.1630

# §E — Baumgarte
TAU = [(0.05, 324.93, 333.33), (0.10, 158.19, 166.67),
       (0.20, 74.69, 83.33), (0.40, 32.63, 33.33)]
DEPARTURE = [(10.0, 0.0600, 1.8003e-02, 0.2), (50.0, 0.5400, 1.4582e+00, 14.9),
             (100.0, 1.1401, 6.4987e+00, 66.2)]
PEAKS = [(10.0, -5.000, -5.000), (50.0, -5.000, -5.000), (100.0, -5.000, -5.000),
         (200.0, 98.886, -5.000), (300.0, 215.077, -5.000)]
CLAMP = [(100.0, 3.5400, 368.523), (3.0, 3.0000, 215.077), (1.0, 1.0000, -5.000)]

# §F — split impulse and the slop
SLOP = [(0.00, 0.00, 0.00, 1801.25, 1.204), (0.50, 0.00, 0.00, 9085.89, 3.109),
        (2.00, 0.00, 0.00, 0.00, 1.999), (5.00, 0.00, 0.00, 0.00, 4.999),
        (20.00, 0.00, 0.00, 0.00, 9.165)]
TAU_B_MS, TAU_S_MS = 83.33, 83.33

# §G — islands
ISLANDS_CORRECT, ISLANDS_BRIDGED = 20, 1
ISLAND_COST_US, ISLAND_NS_PER_BODY = 0.250, 2.48
YARD_BODIES, YARD_MANIFOLDS = 101, 100

# §H — sleeping
QUIET_PRE, QUIET_POST = 0, 96
FASTEST_PRE, FASTEST_POST = 0.1836, 0.0629
GH = 0.1635
ASLEEP_FRAME, ASLEEP_S = 168, 2.80
SOLVE_ASLEEP_US, SOLVE_AWAKE_US, SOLVE_RATIO = 3.041, 130.750, 43.0
NARROW_FULL_US, NARROW_SKIP_US = 51.500, 7.125
PER_BODY_FELL_M, PER_ISLAND_FELL_M = 0.0000, 0.5000
THRESHOLD = [(0.0005, 60, None), (0.0020, 70, None), (0.0100, 75, None),
             (0.0200, 100, 19.15), (0.0500, 100, 2.800), (0.1000, 100, 0.950),
             (0.2000, 100, 0.566)]
RESLEEP_NO_RESET, RESLEEP_RESET = 1, 30

# §I — the step slot
GH2_MM = 2.72500
LATE_SINK_MM = 654.0007
SPLIT_NS, MONO_NS, SPLIT_PCT = 19.938, 14.542, 37.1
IDENTICAL = 400

# §J — the budget
BROAD_US, NARROW_US, SOLVE_US = 17.208, 49.792, 135.000
PER_VEL_US, PER_POS_US, FIXED_US = 12.917, 5.903, 14.958
BIG_BROAD_MS, BIG_NARROW_MS, BIG_SOLVE_MS = 0.134, 1.118, 3.102
BIG_ASLEEP_MS = 1.324
BIG_BODIES, BIG_MANIFOLDS = 2001, 2619

# §K — what is left
NEEDED = [(2, 2, 3.13), (3, 2, 20.17), (5, 4, 105.86), (10, 20, 402.15),
          (15, None, None), (20, None, None)]
MASS_RATIO = [(1, 1.6860), (10, 51.3249), (100, 385.6325), (1000, 476.6946)]


# ===========================================================================
# FIGURE 1 — Gauss-Seidel: what a second sweep buys                    (§2, §3)
# ===========================================================================

def fig1():
    uid = "f1"
    W = 980
    body = [cmarker(uid, "blue", BLUE), cmarker(uid, "amber", AMBER),
            cmarker(uid, "green", GREEN)]

    # ---- LEFT: four points solved in turn, each disturbing the last -------
    lx, ly, lw = 26, 40, 430
    body.append(label(lx, ly, "One sweep, four points, in turn", cls="sm", anchor="start"))

    crate_y = ly + 34
    crate_h = 74
    body.append(hollow(lx + 40, crate_y, 300, crate_h, BLUE, width=1.4))
    body.append(label(lx + 190, crate_y + crate_h / 2 + 5, "one crate, one manifold",
                      cls="xs muted"))
    floor_y = crate_y + crate_h
    body.append(cline(lx + 10, floor_y, lx + 400, floor_y, GREY, width=2.0))

    # the four contact points, and the impulse each one applies
    px = [lx + 60, lx + 150, lx + 240, lx + 320]
    for i, x in enumerate(px):
        body.append(f'<circle cx="{x}" cy="{floor_y}" r="3.4" fill="{BLUE}"/>')
        body.append(carrow(x, floor_y, x, floor_y - 26, uid, "blue", BLUE, width=1.6))
        body.append(label(x, floor_y + 18, f"{i + 1}", cls="xs mono"))

    # the disturbance: solving 4 moves 1 off its target again
    dy = floor_y + 44
    body.append(label(lx, dy, "…and point 4's impulse moves the velocity point 1 had just",
                      cls="xs muted", anchor="start"))
    body.append(label(lx, dy + 16, "set exactly on target. That residue is not a rounding",
                      cls="xs muted", anchor="start"))
    body.append(label(lx, dy + 32, "error; it is the system disagreeing with itself.",
                      cls="xs muted", anchor="start"))

    sweep_y = dy + 62
    for s in range(3):
        sx = lx + 10 + s * 140
        body.append(hollow(sx, sweep_y, 112, 30, GREEN if s == 2 else BLUE, width=1.2))
        body.append(label(sx + 56, sweep_y + 20, f"sweep {s + 1}", cls="xs mono"))
        if s < 2:
            body.append(carrow(sx + 114, sweep_y + 15, sx + 136, sweep_y + 15, uid,
                               "green", GREEN, width=1.4))
    body.append(label(lx + 10, sweep_y + 52,
                      f"each sweep multiplies what is left by {CONTRACTION:.2f}",
                      cls="xs", anchor="start"))

    # ---- RIGHT: the measured residual, log axis --------------------------
    px0, py0, pw, ph = 530, 74, 380, 250
    body.append(label(px0, py0 - 34, "Residual after n sweeps, one crate, cold",
                      cls="sm", anchor="start"))
    body.append(frame(px0, py0, pw, ph))

    lo, hi = 1e-9, 1e-1
    for e in range(-9, 0):
        yy = logy(10.0 ** e, lo, hi, py0, ph)
        body.append(rule(px0, yy, px0 + pw, yy, cls="grid", width=0.7))
        body.append(label(px0 - 8, yy + 4, f"1e{e}", cls="xs mono", anchor="end"))

    def ix(n):
        return logspan(n, 1, 64, px0 + 16, pw - 32)

    pts = [(ix(n), logy(r, lo, hi, py0, ph)) for n, r in RESID]
    body.append(poly(pts, BLUE, width=2.0, close=False))
    for (n, r), (x, y) in zip(RESID, pts):
        body.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3.2" fill="{BLUE}"/>')
        body.append(label(x, py0 + ph + 16, str(n), cls="xs mono"))
    body.append(label(px0 + pw / 2, py0 + ph + 34, "velocity iterations", cls="xs muted"))

    # the ideal geometric line, from the first point at the measured rate
    ideal = [(ix(n), logy(RESID[0][1] * CONTRACTION ** (n - 1), lo, hi, py0, ph))
             for n in (1, 2, 4, 8, 16, 32)]
    body.append(poly(ideal, PURPLE, width=1.4, dash="5 4", close=False))

    # the float floor
    yfloor = logy(FLOAT_FLOOR, lo, hi, py0, ph)
    body.append(cline(px0, yfloor, px0 + pw, yfloor, GREY, width=1.2, dash="3 4"))
    body.append(label(px0 + pw - 6, yfloor - 7, "float floor", cls="xs muted", anchor="end"))

    body.extend(legend(px0, py0 + ph + 58, [
        (BLUE, "measured"),
        (PURPLE, f"geometric at {CONTRACTION:.2f} per iteration"),
    ]))

    # control
    body.append(label(26, 392,
                      f"CONTROL — a sphere on a floor is ONE point: {ONE_POINT_RESID:.4e} at one "
                      f"pass and the same at sixteen.", cls="xs muted", anchor="start"))

    H = 412
    return svg(uid, W, H,
               "Sequential impulses on a four-point manifold, and the measured contraction",
               "Left: four contact points solved one after another, each impulse disturbing "
               "the velocity the previous one set. Right: the residual after n sweeps on a "
               "log axis, falling by a constant factor of 0.53 per iteration until it reaches "
               "the float floor.", body)


# ===========================================================================
# FIGURE 2 — warm starting, and the loop that makes it possible         (§4)
# ===========================================================================

def fig2():
    uid = "f2"
    W = 980
    body = [cmarker(uid, "green", GREEN), cmarker(uid, "blue", BLUE)]

    # ---- LEFT: the four-lesson loop --------------------------------------
    lx = 26
    body.append(label(lx, 36, "The loop, and it took four lessons to close",
                      cls="sm", anchor="start"))

    stages = [("8.7  contact_id", "a name for a contact that survives a frame"),
              ("8.7  carry_impulses", "match this frame's points onto last frame's"),
              ("8.9  write_back", "put the accumulated impulse in the manifold"),
              ("8.10 prepare_contacts", "read it back as this frame's first guess")]
    sy = 64
    for i, (name, what) in enumerate(stages):
        y = sy + i * 62
        body.append(hollow(lx, y, 400, 44, GREEN if i == 3 else BLUE, width=1.3))
        body.append(label(lx + 12, y + 19, name, cls="xs mono", anchor="start"))
        body.append(label(lx + 12, y + 35, what, cls="xs muted", anchor="start"))
        if i < 3:
            body.append(carrow(lx + 200, y + 44, lx + 200, y + 60, uid, "green", GREEN,
                               width=1.4))
    # the wrap-around
    body.append(cline(lx + 400, sy + 3 * 62 + 22, lx + 424, sy + 3 * 62 + 22, GREEN, width=1.4))
    body.append(cline(lx + 424, sy + 3 * 62 + 22, lx + 424, sy + 22, GREEN, width=1.4))
    body.append(carrow(lx + 424, sy + 22, lx + 402, sy + 22, uid, "green", GREEN, width=1.4))

    body.append(label(lx, sy + 4 * 62 + 8,
                      f"{MATCH_NUM:,} of {MATCH_DEN:,} points ({MATCH_PCT:.2f}%) inherit an "
                      f"impulse on a settled tower.", cls="xs", anchor="start"))

    # ---- RIGHT: what it is worth -----------------------------------------
    rx = 520
    body.extend(table_body(
        rx, 40,
        ["five-crate tower", "cold", "warm"],
        [["iterations to a residual of 1e-04", f"> {COLD_ITERS_LIMIT}", f"{WARM_ITERS}"],
         ["top crate sink after 20 s, mm", f"{SINK_COLD_MM:,.0f}", f"{SINK_WARM_MM:.0f}"],
         ["deepest penetration, mm", f"{DEEP_COLD_MM:.1f}", f"{DEEP_WARM_MM:.1f}"]],
        [268, 82, 82], title="At the eight iterations this engine ships"))

    ry = 152
    body.extend(table_body(
        rx, ry,
        ["inherited impulse ×", "iters", "residual", "top m/s"],
        [[f"{s:.1f}", ("> 400" if n >= 400 else str(n)), f"{r:.2e}", f"{v:.3f}"]
         for s, n, r, v in SCALE_SWEEP],
        [140, 66, 100, 86], title="A guess that is wrong (control)"))

    body.append(label(rx, ry + 150,
                      "Read the last two columns together: the ×5 row",
                      cls="xs muted", anchor="start"))
    body.append(label(rx, ry + 166,
                      "converges quickly onto a tower it has launched.",
                      cls="xs muted", anchor="start"))

    H = 420
    return svg(uid, W, H,
               "Warm starting: the four-lesson loop that makes it possible, and what it is worth",
               "Left: contact ids from 8.7, carry_impulses, write_back from 8.9 and "
               "prepare_contacts from 8.10 form a cycle that carries each contact's impulse "
               "into the next frame as an initial guess. Right: two tables — the cold and warm "
               "iteration counts and sinks, and a control in which the inherited impulse is "
               "deliberately scaled wrong.", body)


# ===========================================================================
# FIGURE 3 — the cached basis, and the bug warm starting was hiding     (§4)
# ===========================================================================

def fig3():
    uid = "f3"
    W = 980
    body = [cmarker(uid, "amber", AMBER), cmarker(uid, "red", RED),
            cmarker(uid, "blue", BLUE)]

    body.append(label(26, 36, "tangent_basis picks its seed from the normal's SMALLEST component",
                      cls="sm", anchor="start"))

    # ---- LEFT: two frames, the basis snapping 90 degrees -----------------
    for k, (nx, nz, tag) in enumerate([(1.2e-5, 3.0e-5, "frame n"),
                                       (3.1e-5, 0.9e-5, "frame n+1")]):
        ox = 46 + k * 250
        oy = 96
        body.append(label(ox + 90, oy - 26, tag, cls="xs mono"))
        body.append(hollow(ox, oy, 180, 130, GREY, width=1.0))

        # the normal, essentially straight up either way
        body.append(carrow(ox + 90, oy + 110, ox + 90, oy + 22, uid, "blue", BLUE, width=1.8))
        body.append(label(ox + 104, oy + 40, "n", cls="xs mono", anchor="start"))
        body.append(label(ox + 90, oy + 126,
                          f"|n.x| {nx:.1e}   |n.z| {nz:.1e}", cls="xs mono"))

        # the tangent basis, which swaps between the two frames
        smaller_x = nx <= nz
        t1 = (1, 0) if smaller_x else (0, 1)
        for j, (dx, dy) in enumerate([t1, (t1[1], -t1[0])]):
            ex = ox + 90 + dx * 56
            ey = oy + 70 - dy * 40
            body.append(carrow(ox + 90, oy + 70, ex, ey, uid, "amber", AMBER, width=1.7))
            body.append(label(ex + (10 if dx else 0), ey - (8 if dy else -4),
                              f"t{j + 1}", cls="xs mono"))

    body.append(carrow(236, 160, 286, 160, uid, "red", RED, width=1.8))
    body.append(label(261, 148, "90°", cls="xs mono"))

    body.append(label(46, 268,
                      "Two numbers around 1e-05 decide which axis seeds the cross product, and",
                      cls="xs muted", anchor="start"))
    body.append(label(46, 284,
                      "they cross constantly. The basis is not stored with the impulses, so last",
                      cls="xs muted", anchor="start"))
    body.append(label(46, 300,
                      "frame's friction is applied in this frame's directions — sideways.",
                      cls="xs muted", anchor="start"))

    # ---- RIGHT: the measurement ------------------------------------------
    rx = 590
    body.extend(table_body(
        rx, 62,
        ["ten-crate tower, 20 s", "value"],
        [["manifold-frames sampled", f"{FLIP_DEN:,}"],
         ["…whose basis rotated > 30°", f"{FLIP_NUM} ({FLIP_PCT:.2f}%)"],
         ["worst rotation", f"{FLIP_WORST:.1f}°"],
         ["sideways drift, basis stored", f"{DRIFT_FIXED_MM:.0f} mm"],
         ["sideways drift, pre-8.10", f"{DRIFT_STALE_MM:.0f} mm"]],
        [212, 132], title="Measured"))

    body.append(label(rx, 234, "The fix is two dot products and it is exact:",
                      cls="xs", anchor="start"))
    body.append(label(rx, 252, "rebuild the impulse in the frame it was measured",
                      cls="xs muted", anchor="start"))
    body.append(label(rx, 268, "in, then re-measure it in this one.",
                      cls="xs muted", anchor="start"))
    body.append(label(rx, 294, "A CACHED NUMBER IS MEANINGLESS WITHOUT THE",
                      cls="xs", anchor="start"))
    body.append(label(rx, 310, "FRAME IT WAS MEASURED IN.", cls="xs", anchor="start"))

    H = 336
    return svg(uid, W, H,
               "The tangent basis flips between frames, and the cached friction goes sideways",
               "Left: two consecutive frames of a near-vertical contact. The normal barely "
               "moves, but the comparison between two components of order 1e-05 flips, so the "
               "tangent basis rotates by ninety degrees. Right: the measurement — 113 of 11830 "
               "manifold-frames rotate by more than thirty degrees, worst case 134.6, and "
               "storing the basis takes the tower's sideways drift from 380 mm to 155.", body)


# ===========================================================================
# FIGURE 4 — the arrival depth is a CEILING                             (§6)
# ===========================================================================

def fig4():
    uid = "f4"
    W = 980
    body = [cmarker(uid, "blue", BLUE), cmarker(uid, "red", RED)]

    # ---- LEFT: the picture ------------------------------------------------
    lx = 30
    body.append(label(lx, 36, "The body crosses the surface INSIDE a step",
                      cls="sm", anchor="start"))

    sy = 150
    body.append(cline(lx, sy, lx + 400, sy, GREY, width=2.0))
    body.append(label(lx + 4, sy - 8, "surface", cls="xs muted", anchor="start"))

    # three sample positions: before, the crossing, after
    for i, (x, y, tag, col) in enumerate([(lx + 60, sy - 62, "step n", BLUE),
                                          (lx + 200, sy - 6, "crossing", GREY),
                                          (lx + 330, sy + 44, "step n+1", RED)]):
        body.append(hollow(x - 30, y - 22, 60, 44, col, width=1.4,
                           dash="4 3" if i == 1 else None))
        body.append(label(x, y + 38 if i == 2 else y - 30, tag, cls="xs mono"))

    body.append(carrow(lx + 90, sy - 62, lx + 168, sy - 20, uid, "blue", BLUE, width=1.5))
    body.append(carrow(lx + 230, sy + 6, lx + 298, sy + 40, uid, "red", RED, width=1.5))

    body.append(cline(lx + 330, sy, lx + 330, sy + 22, RED, width=2.4))
    body.append(label(lx + 344, sy + 16, "depth", cls="xs mono", anchor="start"))

    body.append(label(lx, sy + 108,
                      "The narrow phase is asked once a step. How much of the step was left",
                      cls="xs muted", anchor="start"))
    body.append(label(lx, sy + 124,
                      "over when the surface was crossed is a property of where the body",
                      cls="xs muted", anchor="start"))
    body.append(label(lx, sy + 140,
                      "happened to be when the step began — which is to say, arbitrary.",
                      cls="xs muted", anchor="start"))

    # ---- RIGHT: the distribution -----------------------------------------
    px0, py0, pw, ph = 530, 62, 380, 150
    body.append(label(px0, py0 - 22, "Arrival depth as a fraction of v·h, 200 drops per row",
                      cls="sm", anchor="start"))
    body.append(frame(px0, py0, pw, ph))
    for t in (0.0, 0.25, 0.5, 0.75, 1.0):
        xx = px0 + t * pw
        body.append(rule(xx, py0, xx, py0 + ph, cls="grid", width=0.7))
        body.append(label(xx, py0 + ph + 16, f"{t:.2f}", cls="xs mono"))
    body.append(label(px0 + pw / 2, py0 + ph + 34, "fraction of v·h", cls="xs muted"))

    row_h = ph / len(ARRIVAL)
    for i, (drop, v, vh, lo_f, mean_f, hi_f) in enumerate(ARRIVAL):
        cy = py0 + (i + 0.5) * row_h
        body.append(cline(px0 + lo_f * pw, cy, px0 + hi_f * pw, cy, BLUE, width=6.0))
        body.append(f'<circle cx="{px0 + mean_f * pw:.1f}" cy="{cy:.1f}" r="4.2" '
                    f'fill="{PURPLE}"/>')
        body.append(label(px0 + 8, cy - 12, f"{drop:.2f} m drop  ·  v·h = {vh:.1f} mm",
                          cls="xs mono", anchor="start"))

    body.append(cline(px0 + pw, py0, px0 + pw, py0 + ph, RED, width=2.0))
    body.append(label(px0 + pw - 6, py0 - 6, "v·h, the ceiling", cls="xs", anchor="end"))

    body.extend(legend(px0, py0 + ph + 58, [
        (BLUE, "range over 200 finely spaced drop heights"),
        (PURPLE, "mean — 0.49, which is what UNIFORM means"),
    ]))

    # ---- and it does not move -------------------------------------------
    body.extend(table_body(
        px0, 336,
        ["velocity iterations", "1", "8", "64"],
        [["settled depth, mm", f"{FROZEN[0][1]:.3f}", f"{FROZEN[3][1]:.3f}",
          f"{FROZEN[6][1]:.3f}"]],
        [180, 66, 66, 66], title=f"Frozen: {FROZEN_SPREAD_PCT:.1f}% over a 64× range"))

    H = 412
    return svg(uid, W, H,
               "The arrival depth is a ceiling, not an equality",
               "Left: a body crosses the surface part way through a step and is not sampled "
               "until the next one, so the overlap it is first seen with is somewhere between "
               "zero and one step of travel. Right: 200 finely spaced drop heights per row show "
               "the arrival depth uniform on [0, v h] with a mean of 0.49 and a largest value "
               "of 0.97, and a table showing the settled depth changing by 3% across a "
               "sixty-four-fold range of iteration count.", body)


# ===========================================================================
# FIGURE 5 — Baumgarte against split impulse                        (§7, §8)
# ===========================================================================

def fig5():
    uid = "f5"
    W = 980
    body = [cmarker(uid, "red", RED), cmarker(uid, "green", GREEN)]

    body.append(label(26, 36, "The same correction, applied to two different velocities",
                      cls="sm", anchor="start"))

    # ---- LEFT: the two schematics ----------------------------------------
    for k, (tag, col, keeps) in enumerate([("Baumgarte", RED, True),
                                           ("split impulse", GREEN, False)]):
        oy = 74 + k * 152
        body.append(label(30, oy - 8, tag, cls="xs mono", anchor="start"))
        body.append(hollow(30, oy, 400, 118, col, width=1.3))

        body.append(label(46, oy + 26, "velocity solve", cls="xs", anchor="start"))
        body.append(hollow(46, oy + 36, 150, 30, BLUE, width=1.2))
        body.append(label(121, oy + 56, "state.velocity", cls="xs mono"))

        body.append(label(232, oy + 26, "correction target", cls="xs", anchor="start"))
        body.append(hollow(232, oy + 36, 174, 30, col, width=1.2))
        body.append(label(319, oy + 56,
                          "state.velocity" if keeps else "pseudo_velocity", cls="xs mono"))

        body.append(label(46, oy + 92,
                          "one velocity; the push survives the step"
                          if keeps else "two velocities; the push is thrown away",
                          cls="xs muted", anchor="start"))

    # ---- RIGHT: the measurement -------------------------------------------
    rx = 500
    body.extend(table_body(
        rx, 40,
        ["spawned inside the floor", "Baumgarte", "split"],
        [[f"{d:.0f} mm", (f"+{b:.0f} mm" if b > 0 else f"{b:.0f} mm"),
          f"{s:.0f} mm"] for d, b, s in PEAKS],
        [220, 112, 96],
        title="Peak height relative to the surface (negative = never surfaced)"))

    ry = 196
    body.extend(table_body(
        rx, ry,
        ["gravity off, 100 mm deep", "Baumgarte", "split"],
        [["departure speed, m/s", f"{DEPARTURE[2][1]:.5f}", "0.00000"],
         ["kinetic energy added, J", f"{DEPARTURE[2][2]:.3f}", "0.000"],
         ["time to remove 1/e, ms", f"{TAU_B_MS:.2f}", f"{TAU_S_MS:.2f}"]],
        [220, 112, 96], title="…and what it costs to get there"))

    body.append(label(rx, ry + 118,
                      "Not slower: the same arithmetic, the same rate. What differs",
                      cls="xs muted", anchor="start"))
    body.append(label(rx, ry + 134,
                      "is only whose velocity it pushes on.",
                      cls="xs muted", anchor="start"))

    H = 396
    return svg(uid, W, H,
               "Baumgarte pushes on the real velocity; split impulse pushes on a shadow",
               "Left: both corrections compute the same target, but Baumgarte adds it to the "
               "body's real velocity while split impulse drives a separate pseudo velocity that "
               "is discarded at the end of the step. Right: a crate spawned 200 mm inside the "
               "floor is thrown 99 mm clear of it by Baumgarte and settles at the slop under "
               "split impulse, and the two corrections work at the same rate.", body)


# ===========================================================================
# FIGURE 6 — islands, and the one rule                                  (§9)
# ===========================================================================

def fig6():
    uid = "f6"
    W = 980
    body = [cmarker(uid, "red", RED), cmarker(uid, "green", GREEN)]

    for k, (tag, bridge, col, count) in enumerate(
            [("A fixed body is NOT a bridge", False, GREEN, ISLANDS_CORRECT),
             ("…and if it is", True, RED, ISLANDS_BRIDGED)]):
        ox = 30 + k * 480
        body.append(label(ox, 36, tag, cls="sm", anchor="start"))

        floor_y = 210
        body.append(cline(ox, floor_y, ox + 420, floor_y, GREY, width=2.4))
        body.append(label(ox + 4, floor_y + 18, "one fixed floor, in every contact",
                          cls="xs muted", anchor="start"))

        palette = [GREEN, BLUE, AMBER, PURPLE] if not bridge else [RED] * 4
        for t in range(4):
            tx = ox + 40 + t * 96
            colour = palette[t % len(palette)]
            for c in range(3):
                cy = floor_y - 34 - c * 34
                body.append(hollow(tx, cy, 56, 30, colour, width=1.5))
            # the edge to the floor: drawn, but not an edge of the graph
            body.append(cline(tx + 28, floor_y - 34, tx + 28, floor_y, GREY,
                              width=1.0, dash="3 3"))
            if bridge:
                body.append(cline(tx + 28, floor_y, tx + 124, floor_y, RED, width=2.0))

        body.append(label(ox + 210, 264,
                          f"{count} island{'s' if count != 1 else ''}", cls="sm"))

    body.append(label(30, 306,
                      "An impulse applied to a fixed body changes nothing any other contact can "
                      "read, so it propagates nothing and joins nothing.",
                      cls="xs muted", anchor="start"))
    body.append(label(30, 324,
                      "Let it bridge and one rolling marble keeps every crate in the level awake "
                      "— which is not a slow simulation, it is no sleeping at all.",
                      cls="xs muted", anchor="start"))

    body.extend(table_body(
        30, 358,
        ["yard of twenty towers", "value"],
        [["bodies (one floor, 100 crates)", f"{YARD_BODIES}"],
         ["manifolds", f"{YARD_MANIFOLDS}"],
         ["islands, fixed bodies not bridges", f"{ISLANDS_CORRECT}"],
         ["islands, fixed bodies bridging", f"{ISLANDS_BRIDGED}"],
         ["union-find and labelling", f"{ISLAND_COST_US:.3f} µs "
                                      f"({ISLAND_NS_PER_BODY:.2f} ns/body)"]],
        [244, 168]))

    H = 500
    return svg(uid, W, H,
               "Islands: a fixed body is not a bridge",
               "Left: four towers standing on one floor make four islands, because an edge of "
               "the contact graph counts only when both ends can move. Right: the same scene "
               "with a union-find that lets a fixed body join its neighbours collapses to a "
               "single island, and a yard of twenty towers goes from twenty islands to one.",
               body)


# ===========================================================================
# FIGURE 7 — sleeping: where the test goes, and what it buys           (§10)
# ===========================================================================

def fig7():
    uid = "f7"
    W = 980
    body = [cmarker(uid, "red", RED), cmarker(uid, "green", GREEN),
            cmarker(uid, "blue", BLUE)]

    # ---- LEFT: the step, and the two places the test could go ------------
    body.append(label(26, 36, "A resting body is never quiet BEFORE the solve",
                      cls="sm", anchor="start"))

    sx, sy = 40, 74
    stages = [("integrate_velocities", BLUE, f"+ g·h = {GH:.4f} m/s"),
              ("contact solve", BLUE, "removes it again"),
              ("integrate_positions", BLUE, "")]
    for i, (name, col, note) in enumerate(stages):
        y = sy + i * 62
        body.append(hollow(sx, y, 250, 40, col, width=1.3))
        body.append(label(sx + 125, y + 24, name, cls="xs mono"))
        if note:
            body.append(label(sx + 262, y + 24, note, cls="xs muted", anchor="start"))
        if i < 2:
            body.append(carrow(sx + 125, y + 40, sx + 125, y + 58, uid, "blue", BLUE,
                               width=1.3))

    body.append(carrow(sx - 16, sy + 20, sx - 2, sy + 20, uid, "red", RED, width=1.8))
    body.append(label(sx - 22, sy + 24, "✗", cls="sm", anchor="end"))
    body.append(carrow(sx - 16, sy + 144, sx - 2, sy + 144, uid, "green", GREEN, width=1.8))
    body.append(label(sx - 22, sy + 148, "✓", cls="sm", anchor="end"))

    body.append(label(26, 268,
                      f"Read before: {QUIET_PRE} of 100 crates quiet, fastest "
                      f"{FASTEST_PRE:.4f} m/s.", cls="xs", anchor="start"))
    body.append(label(26, 286,
                      f"Read after:  {QUIET_POST} of 100 crates quiet, fastest "
                      f"{FASTEST_POST:.4f} m/s.", cls="xs", anchor="start"))
    body.append(label(26, 310,
                      f"g·h is {GH:.4f} m/s, which is {GH / 0.05:.1f}× the default threshold. A "
                      f"test in the", cls="xs muted", anchor="start"))
    body.append(label(26, 326,
                      "wrong place never fires, at any setting, and looks exactly like a",
                      cls="xs muted", anchor="start"))
    body.append(label(26, 342, "threshold that is merely too tight.",
                      cls="xs muted", anchor="start"))

    # ---- RIGHT: what it buys, and the per-body failure --------------------
    rx = 530
    body.extend(table_body(
        rx, 40,
        ["settled yard of 100 crates", "awake", "asleep"],
        [["solve", f"{SOLVE_AWAKE_US:.1f} µs", f"{SOLVE_ASLEEP_US:.1f} µs"],
         ["manifolds solved", f"{YARD_MANIFOLDS}", "0"],
         ["narrow phase (unchanged)", f"{NARROW_FULL_US:.1f} µs",
          f"{NARROW_FULL_US:.1f} µs"],
         ["…if the caller skips asleep pairs", "—", f"{NARROW_SKIP_US:.1f} µs"]],
        [244, 92, 92], title=f"What it buys — {SOLVE_RATIO:.0f}× on the solve"))

    ry = 186
    body.extend(table_body(
        rx, ry,
        ["crate thrown at a sleeping tower", "per island", "per body"],
        [["bottom crate moved, m", "70.6", "63.2"],
         ["top crate fell, m", f"{PER_ISLAND_FELL_M:.3f}", f"{PER_BODY_FELL_M:.3f}"]],
        [244, 92, 92], title="Why sleeping is decided per ISLAND (control)"))

    body.append(label(rx, ry + 104,
                      "A body asleep is, to everything else, an immovable body.",
                      cls="xs muted", anchor="start"))
    body.append(label(rx, ry + 120,
                      "Sleep one on its own and it hangs in the air when what",
                      cls="xs muted", anchor="start"))
    body.append(label(rx, ry + 136,
                      "it was standing on is knocked away.",
                      cls="xs muted", anchor="start"))

    H = 368
    return svg(uid, W, H,
               "Sleeping: the test has to run after the solve, and the decision has to be per island",
               "Left: the three stages of a step, with gravity adding 0.1635 m/s to every "
               "dynamic body before the solver looks — so a sleep test placed before the solve "
               "finds nothing quiet, measured at 0 of 100 against 96 of 100. Right: the saving, "
               "43 times on the solve, and the control showing that a body slept individually "
               "hangs in the air when its support is removed.", body)


# ===========================================================================
# FIGURE 8 — the budget, and the honest limit                    (§13, §14)
# ===========================================================================

def fig8():
    uid = "f8"
    W = 980
    body = [cmarker(uid, "green", GREEN), cmarker(uid, "red", RED)]

    # ---- LEFT: the frame, to scale ---------------------------------------
    body.append(label(26, 36, "A 2,001-body scene against a 16.67 ms frame",
                      cls="sm", anchor="start"))

    bx, bw = 30, 420
    budget = 16.667
    rows = [("awake", [(BIG_BROAD_MS, BLUE, "broadphase"),
                       (BIG_NARROW_MS, AMBER, "narrow phase"),
                       (BIG_SOLVE_MS, RED, "solve")]),
            ("settled and asleep", [(BIG_ASLEEP_MS, GREEN, "everything")])]
    for i, (tag, parts) in enumerate(rows):
        y = 78 + i * 76
        body.append(label(bx, y - 8, tag, cls="xs mono", anchor="start"))
        body.append(hollow(bx, y, bw, 26, GREY, width=1.0))
        cursor = bx
        for ms, col, name in parts:
            w = ms / budget * bw
            body.append(f'<rect x="{cursor:.1f}" y="{y}" width="{w:.1f}" height="26" '
                        f'fill="{col}" fill-opacity="0.75" stroke="{col}" stroke-width="1"/>')
            cursor += w
        total = sum(p[0] for p in parts)
        body.append(label(cursor + 8, y + 18, f"{total:.2f} ms  ({total / budget * 100:.0f}%)",
                          cls="xs mono", anchor="start"))

    body.extend(legend(bx, 238, [
        (BLUE, f"broadphase {BIG_BROAD_MS:.3f} ms"),
        (AMBER, f"narrow phase {BIG_NARROW_MS:.3f} ms"),
        (RED, f"solve {BIG_SOLVE_MS:.3f} ms — 71% of the frame's contact work"),
        (GREEN, f"the same scene asleep: {BIG_ASLEEP_MS:.3f} ms"),
    ]))

    body.append(label(bx, 326,
                      f"Per iteration over 100 manifolds: {PER_VEL_US:.1f} µs velocity, "
                      f"{PER_POS_US:.1f} µs position ({PER_POS_US / PER_VEL_US * 100:.0f}%),",
                      cls="xs muted", anchor="start"))
    body.append(label(bx, 342,
                      f"on a fixed cost of {FIXED_US:.1f} µs. Linear in the count to three "
                      f"figures.", cls="xs muted", anchor="start"))

    # ---- RIGHT: the honest limit -----------------------------------------
    rx = 530
    body.extend(table_body(
        rx, 40,
        ["tower of n crates", "sweeps needed", "drift there"],
        [[f"{n}", ("more than 64" if k is None else f"{k}"),
          ("—" if d is None else f"{d:.0f} mm")] for n, k, d in NEEDED],
        [180, 130, 108],
        title="How many sweeps a tower needs to stand for 20 s"))

    body.append(label(rx, 212,
                      "The shipped default is EIGHT, which holds five and not ten.",
                      cls="xs", anchor="start"))
    body.append(label(rx, 230,
                      "A sweep carries information across ONE contact, so a chain",
                      cls="xs muted", anchor="start"))
    body.append(label(rx, 246,
                      "of n needs about n sweeps — and the cost is linear in the",
                      cls="xs muted", anchor="start"))
    body.append(label(rx, 262,
                      "count, so a tall stack is quadratic.",
                      cls="xs muted", anchor="start"))

    body.extend(table_body(
        rx, 296,
        ["heavy on light, 8 sweeps", "squashed"],
        [[f"{r}:1 mass ratio", f"{d:.1f} mm"] for r, d in MASS_RATIO],
        [200, 118]))

    H = 420
    return svg(uid, W, H,
               "The budget, and the honest limit of this solver",
               "Left: a 2001-body scene costs 4.35 ms of a 16.67 ms frame awake and 1.32 ms "
               "asleep, with the solve 71% of the contact work. Right: the number of sweeps a "
               "tower of n crates needs in order to stand for twenty seconds, rising from two "
               "at n = 2 to more than sixty-four at n = 15, and the penetration a large mass "
               "ratio costs at the shipped eight.", body)


def main():
    write("l810_fig1.svg", fig1())
    write("l810_fig2.svg", fig2())
    write("l810_fig3.svg", fig3())
    write("l810_fig4.svg", fig4())
    write("l810_fig5.svg", fig5())
    write("l810_fig6.svg", fig6())
    write("l810_fig7.svg", fig7())
    write("l810_fig8.svg", fig8())


if __name__ == "__main__":
    main()
