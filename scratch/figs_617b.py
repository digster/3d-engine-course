#!/usr/bin/env python3
"""scratch/figs_617b.py — Lesson 6.17b's diagrams.

Same rules as 6.1-6.18's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - ~5.2 units per character for `xs`, ~6.0 for `sm`
  - filenames numbered by PAGE ORDER
  - `rule()` takes a CSS CLASS; `cline()` takes a COLOUR
  - legends and annotation boxes go OUTSIDE the plot
  - viewBoxes at most 928 wide, labels no smaller than `xs`

Every number comes from verify_617b's output (scratch/_probe617b/run_O0.txt when
it was written), and the renders from its VERIFY617B_DUMP images and from
`gltf_view --lights --shot`, copied to scratch/l617b_*.ppm so the figures can be
regenerated from tracked files.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)
from figs_610 import cline                                          # noqa: E402
from figs_45 import read_ppm, box_sample, rle_rects, hexrgb         # noqa: E402
from figs_71 import cmarker, carrow, poly, frame                    # noqa: E402
from figs_511 import peak_sample                                    # noqa: E402

OUT = "scratch"

# ---- measured (verify_617b) ------------------------------------------------
CHECKS = 40

# §A
FLUX = 39.47852
FLUX_WANT = 39.47842
FLUX_ERR = 2.60e-06
FAR_DEV = {2: 16.1947, 10: 0.7453, 100: 0.0075}

# §B
WINDOW = [(0.25, 0.992203, 0.996094), (1 / 3, 0.975461, 0.987654), (0.5, 0.878906, 0.9375),
          (0.7, 0.577448, 0.7599), (0.9, 0.118267, 0.3439), (0.99, 0.001553, 0.039404)]
UE_PLUS1 = [(0.5, 0.2000), (1.0, 0.5000), (2.0, 0.8000), (5.0, 0.9615), (10.0, 0.9901)]

# §C
CONE_INNER = 0.30
CONE_OUTER = 0.45
CONE_MID = 0.29993

# §F
TEXEL_1M_MM = 1.652
BIAS_TABLE = [(0.5, 0.826, 1.177e-04, 1.469e-04), (1.0, 1.652, 5.883e-05, 1.469e-04),
              (2.0, 3.303, 2.939e-05, 1.469e-04), (4.0, 6.606, 1.472e-05, 1.469e-04),
              (8.0, 13.212, 7.331e-06, 1.469e-04)]
CROSSOVER = 0.40
JUDGED_SPOT = 67715
ACNE_NONE = 19502
LEAK_68 = 50
JUDGED_CUBE = 343360
FLOOR_JUDGED = 128122
FLOOR_ACNE_68 = 713

# §E
LOOKAT_MISS = 0.970

# §G
G_LIT = 61043
G_DIFFER = 259
G_EDGE_BAND = 4340
G_CONTROL = 9992

# §I
CHANNELS = 196608
CACHE_CONTROL = 18015

# the record
RECORD = [("position_range", "xyz position", "w range"),
          ("radiance", "xyz colour x intensity", "w kind"),
          ("axis_cone", "xyz spot axis", "w cone scale"),
          ("cone_shadow", "x cone offset, y slot", "z near, w far"),
          ("shadow_texel", "x texel at 1 m, y 1/res", "z pcf, w strength"),
          ("shadow_bias", "x constant, y mode", "z slope scale, w max"),
          ("shadow_normal", "x normal-offset scale", "y 1 = cube shadow; zw spare"),
          ("shadow_row[0]", "the spot map's", "clip_from_world"),
          ("shadow_row[1]", "matrix, stored", "as four ROWS"),
          ("shadow_row[2]", "", ""),
          ("shadow_row[3]", "", "")]

WARM = hexrgb("#ffc885")
COOL = hexrgb("#8cbeff")
FLOORG = hexrgb("#c8c8c8")
GOLD = hexrgb("#e6b44b")
PANEL_PALETTE = [WARM, COOL, FLOORG, hexrgb("#ffffff")]


def fmt(v, p=3):
    return f"{v:.{p}f}"


def render_panel(ppm, crop, x, y, px, cell, palette, levels=6, bg="fill-shot", gain=1.0):
    """A real render, averaged over `cell`x`cell` blocks and run-length encoded.

    `gain` brightens the stored (sRGB-encoded) codes for display, clamped at 255 —
    for a night scene that is legible on screen and not on the page. A figure that
    uses it says so in its own subtitle."""
    w, _h, data = read_ppm(os.path.join(OUT, ppm))
    if gain != 1.0:
        data = bytes(min(255, int(v * gain + 0.5)) for v in data)
    gw, gh, grid = box_sample(data, w, crop, cell)
    body = rle_rects(grid, gw, gh, x, y, px, palette, levels=levels, bg_class=bg)
    # crispEdges, so adjacent runs meet without the hairline seams antialiasing
    # draws between rows at a fractional scale (visible as stripes on a dark page,
    # and indistinguishable from the acne stripes Figure 4 exists to show). Local
    # to this file: figs_45.rle_rects is shared, and other lessons' figures must
    # not move.
    return ['<g shape-rendering="crispEdges">'] + body + ['</g>'], gw * px, gh * px


# ===========================================================================
# Figure 1 — the same power through every sphere  (§2)
# ===========================================================================
def fig_spheres():
    uid = "l617bf1"
    W, H = 800, 330
    cx, cy = 150.0, 175.0
    r1, r2 = 110.0, 220.0
    half = math.radians(13.0)
    b = [label(20, 24, "A lamp spreads its power over spheres, and a sphere of twice the radius "
                       "has four times the area.", cls="sm", anchor="start")]

    # the two shells, as arcs spanning +-40 degrees
    for r, dash in ((r1, None), (r2, "4 3")):
        pts = [(cx + r * math.cos(math.radians(a)), cy - r * math.sin(math.radians(a)))
               for a in range(-40, 41, 2)]
        b.append(poly(pts, GREY, width=1.2, dash=dash, close=False))
    # the beam: two rays bounding a solid angle
    for s in (-1, 1):
        b.append(cline(cx, cy, cx + r2 * math.cos(half), cy - s * r2 * math.sin(half),
                       AMBER, width=1.1))
    # the two patches the beam cuts from the shells
    for r in (r1, r2):
        b.append(cline(cx + r * math.cos(half), cy - r * math.sin(half),
                       cx + r * math.cos(half), cy + r * math.sin(half), AMBER, width=3.0))
    b.append(f'<circle cx="{cx}" cy="{cy}" r="7" fill="{AMBER}" stroke="none"/>')
    b.append(label(cx, cy + 26, "lamp, intensity I", cls="xs mono"))
    # Patch labels INSIDE the beam, between the two patches, where no line runs;
    # the far one just outside the dashed shell.
    b.append(label(cx + r1 * math.cos(half) + 6, cy + 4, "A at d", cls="xs", anchor="start"))
    b.append(label(cx + r2 + 8, cy + 4, "4A at 2d", cls="xs", anchor="start"))
    b.append(label(255, 262, "same rays, same power", cls="xs muted"))
    b.append(label(255, 280, "spread 4x as thin: E = I / d²", cls="xs t-hi"))

    # the numbers, outside the drawing
    tx = 480.0
    b.append(label(tx, 70, "measured, verify_617b §A", cls="sm", anchor="start"))
    rows = [("intensity pi, white surface facing it:", ""),
            ("  1 m", "E = 3.1416   renders 1.0000"),
            ("  2 m", "E = 0.7854   renders 0.2500"),
            ("  4 m", "E = 0.1963   renders 0.0625"),
            ("power through spheres of 0.5, 2, 8 m:", ""),
            ("", f"{FLUX:.5f} each (4πI = {FLUX_WANT:.5f})"),
            ("a lamp D m above a 1 m patch vs the sun:", ""),
            ("  D = 2", f"{FAR_DEV[2]:.2f}% off at the corner"),
            ("  D = 10", f"{FAR_DEV[10]:.3f}%  — (D/d)³, as derived"),
            ("  D = 100", f"{FAR_DEV[100]:.4f}%")]
    y = 92
    for a, bb in rows:
        if bb:
            b.append(label(tx + 8, y, a, cls="xs mono", anchor="start"))
            b.append(label(tx + 70, y, bb, cls="xs mono", anchor="start"))
        else:
            b.append(label(tx, y, a, cls="xs muted", anchor="start"))
        y += 17
    b.append(label(tx, y + 12, "at one metre a lamp of intensity π IS the engine's sun",
                   cls="xs t-hi", anchor="start"))
    return svg(uid, W, H, "The inverse-square law from a sphere",
               "A lamp at the left with two concentric spherical shells at distance d and 2d. "
               "A beam of rays cuts a patch of area A from the first and 4A from the second, "
               "so the same power is spread four times as thinly. The measured values from "
               "the harness sit to the right.", b)


# ===========================================================================
# Figure 2 — three published windows, and what the edge does  (§4)
# ===========================================================================
def fig_windows():
    uid = "l617bf2"
    W, H = 800, 360
    b = [label(20, 24, "Forcing a light to zero at its range, without leaving an edge you can see.",
               cls="sm", anchor="start")]

    # ---- left: the windows themselves, over x = d / range --------------------
    X0, Y0, PW, PH = 70.0, 60.0, 300.0, 210.0
    b.append(frame(X0, Y0, PW, PH))
    for i in range(1, 5):
        b.append(rule(X0 + PW * i / 5, Y0, X0 + PW * i / 5, Y0 + PH))
        b.append(rule(X0, Y0 + PH * i / 5, X0 + PW, Y0 + PH * i / 5))
    for i in range(0, 6):
        b.append(label(X0 + PW * i / 5, Y0 + PH + 16, f"{i / 5:.1f}", cls="xs mono"))
        b.append(label(X0 - 8, Y0 + PH - PH * i / 5 + 3, f"{i / 5:.1f}", cls="xs mono", anchor="end"))
    b.append(label(X0 + PW / 2, Y0 + PH + 34, "x = distance / range", cls="xs"))

    def sx(x):
        return X0 + PW * x

    def sy(v):
        return Y0 + PH * (1 - v)

    sq = [(sx(x / 200), sy(max(0.0, 1 - (x / 200) ** 4) ** 2)) for x in range(201)]
    un = [(sx(x / 200), sy(max(0.0, 1 - (x / 200) ** 4))) for x in range(201)]
    b.append(poly(un, BLUE, width=1.8, dash="5 3", close=False))
    b.append(poly(sq, AMBER, width=2.0, close=False))
    # measured dots
    for x, ours, gltf in WINDOW[:5]:
        b.append(f'<circle cx="{sx(x):.1f}" cy="{sy(ours):.1f}" r="2.6" fill="{AMBER}"/>')
    b.append(label(sx(0.5) - 7, sy(0.878906) + 14, "0.879", cls="xs mono", anchor="end"))
    b.append(label(sx(0.7) - 6, sy(0.577448) - 6, "0.577", cls="xs mono", anchor="end"))

    # ---- right: E(d) near the edge, scaled by r^2 ------------------------------
    X1, PW1 = 470.0, 300.0
    b.append(frame(X1, Y0, PW1, PH))
    lo, hi = 0.80, 1.04
    top = 1.0 / lo ** 2

    def ex(x):
        return X1 + PW1 * (x - lo) / (hi - lo)

    def ey(v):
        return Y0 + PH * (1 - v / top)

    for t in (0.8, 0.9, 1.0):
        b.append(rule(ex(t), Y0, ex(t), Y0 + PH))
        b.append(label(ex(t), Y0 + PH + 16, f"{t:.1f}", cls="xs mono"))
    b.append(label(X1 + PW1 / 2, Y0 + PH + 34, "x = distance / range", cls="xs"))
    inv = [(ex(x), ey(1 / x ** 2)) for x in [lo + (hi - lo) * i / 200 for i in range(201)]]
    g = [(ex(x), ey(max(0.0, 1 - x ** 4) / x ** 2)) for x in [lo + (hi - lo) * i / 200 for i in range(201)]]
    k = [(ex(x), ey(max(0.0, 1 - x ** 4) ** 2 / x ** 2)) for x in [lo + (hi - lo) * i / 200 for i in range(201)]]
    b.append(poly(inv, GREY, width=1.4, dash="2 3", close=False))
    b.append(poly(g, BLUE, width=1.8, dash="5 3", close=False))
    b.append(poly(k, AMBER, width=2.0, close=False))
    # Between the dotted inverse square (above) and the two windows (below),
    # which is the one clear band in this plot.
    b.append(label(ex(1.0) - 8, ey(0.0) - 120, "glTF's: a CREASE at x = 1,", cls="xs t-bad",
                   anchor="end"))
    b.append(label(ex(1.0) - 8, ey(0.0) - 106, "slope -4/r³, then 0", cls="xs muted",
                   anchor="end"))
    b.append(label(ex(1.03), ey(1.0 / 0.97 ** 2) - 14, "1/d² alone", cls="xs muted",
                   anchor="end"))

    # legend, outside both plots
    ly = H - 24
    b.append(cline(70, ly - 4, 100, ly - 4, AMBER, width=2.0))
    b.append(label(106, ly, "(1 - x⁴)² — Karis 2013, adopted by Frostbite (the engine's)",
                   cls="xs", anchor="start"))
    b.append(cline(470, ly - 4, 500, ly - 4, BLUE, width=1.8, dash="5 3"))
    b.append(label(506, ly, "1 - x⁴ — glTF's recommended recipe", cls="xs",
                   anchor="start"))
    return svg(uid, W, H, "Range windows compared",
               "Left: the squared and unsquared quartic windows against the fraction of the "
               "range; the squared one is 0.879 at half the range. Right: irradiance near the "
               "edge scaled by range squared, showing the unsquared window meeting zero with "
               "a crease and the squared window meeting it with zero slope.", b)


# ===========================================================================
# Figure 3 — the cone, and the ramp it makes  (§5)
# ===========================================================================
def fig_cone():
    uid = "l617bf3"
    W, H = 800, 360
    b = [label(20, 24, "A spot light is a point light with a mask, and the mask is ramped in "
                       "COSINE.", cls="sm", anchor="start")]
    # ---- left: geometry -----------------------------------------------------
    ax, ay = 180.0, 70.0
    L = 210.0
    b.append(cline(ax, ay, ax, ay + L + 10, GREY, width=1.0, dash="3 3"))
    for ang, col, w in ((CONE_OUTER, BLUE, 1.2), (CONE_INNER, AMBER, 1.6)):
        for s in (-1, 1):
            b.append(cline(ax, ay, ax + s * L * math.tan(ang), ay + L, col, width=w))
    b.append(cline(ax - L * math.tan(CONE_OUTER) - 20, ay + L, ax + L * math.tan(CONE_OUTER) + 20,
                   ay + L, GREY, width=1.4))
    b.append(f'<circle cx="{ax}" cy="{ay}" r="6" fill="{BLUE}"/>')
    b.append(label(ax, ay - 12, "lamp; `direction` points DOWN the axis", cls="xs"))
    lx0 = ax + L * math.tan(CONE_OUTER) + 12
    b.append(label(lx0, ay + L - 40, "outer 0.45 rad", cls="xs", anchor="start"))
    b.append(label(lx0, ay + L - 24, "inner 0.30 rad", cls="xs t-hi", anchor="start"))
    b.append(label(ax, ay + L + 22, "full inside the inner cone, 0 outside the outer", cls="xs muted"))

    # ---- right: the ramp -----------------------------------------------------
    X0, Y0, PW, PH = 440.0, 60.0, 320.0, 200.0
    b.append(frame(X0, Y0, PW, PH))
    a0, a1 = 0.25, 0.50

    def sx(a):
        return X0 + PW * (a - a0) / (a1 - a0)

    def sy(v):
        return Y0 + PH * (1 - v)

    for a in (0.25, 0.30, 0.35, 0.40, 0.45, 0.50):
        b.append(rule(sx(a), Y0, sx(a), Y0 + PH))
        b.append(label(sx(a), Y0 + PH + 16, f"{a:.2f}", cls="xs mono"))
    for v in (0.0, 0.25, 0.5, 0.75, 1.0):
        b.append(label(X0 - 8, sy(v) + 3, f"{v:.2f}", cls="xs mono", anchor="end"))
    b.append(label(X0 + PW / 2, Y0 + PH + 34, "angle from the axis, radians", cls="xs"))
    ci, co = math.cos(CONE_INNER), math.cos(CONE_OUTER)
    ours = []
    lin = []
    for i in range(201):
        a = a0 + (a1 - a0) * i / 200
        t = min(1.0, max(0.0, (math.cos(a) - co) / (ci - co)))
        ours.append((sx(a), sy(t * t)))
        u = min(1.0, max(0.0, (CONE_OUTER - a) / (CONE_OUTER - CONE_INNER)))
        lin.append((sx(a), sy(u * u)))
    b.append(poly(lin, GREY, width=1.4, dash="4 3", close=False))
    b.append(poly(ours, BLUE, width=2.0, close=False))
    mx = 0.375
    b.append(f'<circle cx="{sx(mx):.1f}" cy="{sy(CONE_MID):.1f}" r="3" fill="{BLUE}"/>')
    b.append(f'<circle cx="{sx(mx):.1f}" cy="{sy(0.25):.1f}" r="3" fill="{GREY}"/>')
    b.append(label(sx(mx) + 8, sy(CONE_MID) - 6, f"{CONE_MID:.4f}", cls="xs mono t-hi", anchor="start"))
    b.append(label(sx(mx) - 8, sy(0.25) + 14, "0.25", cls="xs mono", anchor="end"))
    ly = H - 12
    b.append(cline(440, ly - 4, 470, ly - 4, BLUE, width=2.0))
    b.append(label(476, ly, "saturate(cos·scale + offset)² — glTF's, the engine's",
                   cls="xs", anchor="start"))
    b.append(cline(440, ly - 20, 470, ly - 20, GREY, width=1.4, dash="4 3"))
    b.append(label(476, ly - 16, "the same ramp, linear in ANGLE (not used)", cls="xs",
                   anchor="start"))
    return svg(uid, W, H, "The spot cone and its falloff",
               "Left: a spot light's axis with inner and outer half-angles of 0.30 and 0.45 "
               "radians. Right: the falloff between them. Interpolating in cosine and squaring "
               "gives 0.2999 at the angular midpoint, where a ramp linear in angle would give "
               "0.25.", b)


# ===========================================================================
# Figure 4 — the failure first: a spot and a point light with no bias  (§6)
# ===========================================================================
def fig_acne():
    uid = "l617bf4"
    W = 760
    # The acne is a pattern a few texels across (nested squares from the cube's
    # faces, speckled arcs from the spot). Six tonal levels keep the squares'
    # bands apart, and render_panel's crispEdges removes the row seams that, at
    # four levels, read as horizontal streaks — a rendering bug rather than acne.
    # (A full-resolution crop was tried and cost 200-470 KB for less of the story.)
    CAP, PAD, PX, CELL = 52.0, 18.0, 2.68, 2
    crop = (0, 0, 256, 256)
    left, LW, LH = render_panel("l617b_gpu_nobias.ppm", crop, PAD, CAP, PX, CELL, PANEL_PALETTE,
                                levels=6)
    right, RW, _ = render_panel("l617b_gpu.ppm", crop, W - PAD - LW, CAP, PX, CELL, PANEL_PALETTE,
                                levels=6)
    H = CAP + LH + 56
    b = [label(W / 2, 22, "The same floor, the same two lamps, seen from straight above (GPU)",
               cls="sm")]
    b.append(label(W / 2, 38, "a warm point light at the right, a cool spot from the left; "
                              "a hovering box and a thin sign cast both", cls="xs muted"))
    b += left + right
    b.append(label(PAD + LW / 2, CAP + LH + 20, "no bias: the floor shadows ITSELF", cls="xs t-bad"))
    b.append(label(PAD + LW / 2, CAP + LH + 35, "nested squares under the bulb, speckle under the spot",
                   cls="xs muted"))
    b.append(label(W - PAD - RW / 2, CAP + LH + 20, "the bias this lesson derives", cls="xs t-ok"))
    b.append(label(W - PAD - RW / 2, CAP + LH + 35, f"judged against ray-cast truth: 0 acne, 0 leaks",
                   cls="xs muted"))
    return svg(uid, W, int(H), "Shadow acne without a bias",
               "Two real GPU renders of the harness scene from directly above. On the left, with "
               "no bias, the floor shadows itself across both lamps' pools: nested squares under the "
               "bulb, one family per cube face, and speckle under the spot. On the "
               "right, with the derived bias, the floor is clean and only the box's and the "
               "sign's shadows remain.", b)


# ===========================================================================
# Figure 5 — a pyramid, not a box  (§6.2)
# ===========================================================================
def fig_pyramid():
    uid = "l617bf5"
    W, H = 820, 360
    b = [label(20, 24, "A spot light's map is a pyramid: its texels grow with distance, and its "
                       "depth is a hyperbola.", cls="sm", anchor="start")]
    # ---- left: the pyramid with texel ticks ------------------------------------
    lx, ly = 50.0, 190.0
    scale = 34.0          # page units per metre
    b.append(f'<circle cx="{lx}" cy="{ly}" r="6" fill="{BLUE}"/>')
    b.append(label(lx, ly + 22, "lamp", cls="xs"))
    spread = 0.42         # half-width per metre, for the drawing
    b.append(cline(lx, ly, lx + 8.4 * scale, ly - 8.4 * scale * spread, GREY, width=1.1))
    b.append(cline(lx, ly, lx + 8.4 * scale, ly + 8.4 * scale * spread, GREY, width=1.1))
    for d, tex_mm, _dv, _nv in BIAS_TABLE:
        if d < 1.0:
            continue
        x = lx + d * scale
        half = d * scale * spread
        b.append(cline(x, ly - half, x, ly + half, AMBER, width=1.3))
        b.append(label(x, ly + half + 14, f"{d:.0f} m", cls="xs mono"))
        b.append(label(x, ly - half - 8, f"{tex_mm:.1f} mm", cls="xs mono t-hi"))
    b.append(label(lx + 4.3 * scale, 318, "one texel's footprint, 512 map", cls="xs muted"))
    b.append(label(lx + 4.3 * scale, 332, "1.652 mm at 1 m, in proportion beyond", cls="xs muted"))

    # ---- right: bias in device units, log-log ----------------------------------
    X0, Y0, PW, PH = 470.0, 56.0, 300.0, 220.0
    b.append(frame(X0, Y0, PW, PH))
    dlo, dhi = 0.25, 10.0
    blo, bhi = 3e-6, 1e-3

    def sx(d):
        return X0 + PW * (math.log10(d) - math.log10(dlo)) / (math.log10(dhi) - math.log10(dlo))

    def sy(v):
        return Y0 + PH * (1 - (math.log10(v) - math.log10(blo)) / (math.log10(bhi) - math.log10(blo)))

    for d in (0.25, 0.5, 1, 2, 4, 8):
        b.append(rule(sx(d), Y0, sx(d), Y0 + PH))
        b.append(label(sx(d), Y0 + PH + 15, f"{d:g}", cls="xs mono"))
    for v, t in ((1e-5, "1e-5"), (1e-4, "1e-4"), (1e-3, "1e-3")):
        b.append(rule(X0, sy(v), X0 + PW, sy(v)))
        b.append(label(X0 - 6, sy(v) + 3, t, cls="xs mono", anchor="end"))
    b.append(label(X0 + PW / 2, Y0 + PH + 32, "distance along the axis, m", cls="xs"))
    der = [(sx(d), sy(1.177e-04 * 0.5 / d)) for d in [dlo * (dhi / dlo) ** (i / 60) for i in range(61)]]
    b.append(poly(der, GREEN, width=2.0, close=False))
    b.append(cline(X0, sy(1.469e-04), X0 + PW, sy(1.469e-04), RED, width=1.6, dash="5 3"))
    for d, _t, dv, _nv in BIAS_TABLE:
        b.append(f'<circle cx="{sx(d):.1f}" cy="{sy(dv):.1f}" r="2.6" fill="{GREEN}"/>')
    b.append(f'<circle cx="{sx(CROSSOVER):.1f}" cy="{sy(1.469e-04):.1f}" r="3.2" fill="{RED}"/>')
    b.append(label(sx(CROSSOVER) + 6, sy(1.469e-04) - 8, "agree at 0.4 m only", cls="xs t-bad",
                   anchor="start"))
    b.append(cline(sx(8), sy(1.469e-04), sx(8), sy(7.331e-06), RED, width=0.8, dash="2 2"))
    b.append(label(sx(8) - 6, sy(1.469e-04) - 8, "20x too large at 8 m", cls="xs muted",
                   anchor="end"))
    ly = H - 20
    b.append(cline(470, ly - 20, 500, ly - 20, GREEN, width=2.0))
    b.append(label(506, ly - 16, "derived: the depth curve, per fragment", cls="xs", anchor="start"))
    b.append(cline(470, ly - 4, 500, ly - 4, RED, width=1.6, dash="5 3"))
    b.append(label(506, ly, "6.8's formula reused unchanged: a constant", cls="xs", anchor="start"))
    return svg(uid, W, H, "A perspective shadow map",
               "Left: a spot light's shadow frustum with one texel's footprint growing from "
               "1.7 mm at 1 m to 13.2 mm at 8 m. Right: the bias needed on a 45-degree surface "
               "in device-depth units against distance, falling as one over distance, and 6.8's "
               "orthographic formula as a constant that crosses it at 0.4 m.", b)


# ===========================================================================
# Figure 6 — axial depth is not radial distance  (§6.4)
# ===========================================================================
def fig_axial():
    uid = "l617bf6"
    W, H = 800, 360
    b = [cmarker(uid, "amber", AMBER), cmarker(uid, "grn", GREEN)]
    b.append(label(20, 24, "The map stores AXIAL depth. A wall facing the lamp squarely still "
                           "changes axial depth across a texel.", cls="sm", anchor="start"))
    lx, ly = 80.0, 250.0
    b.append(f'<circle cx="{lx}" cy="{ly}" r="6" fill="{AMBER}"/>')
    b.append(label(lx, ly + 24, "lamp", cls="xs"))
    # the axis, horizontal
    b.append(carrow(lx, ly, lx + 340, ly, uid, "grn", GREEN, width=1.4))
    b.append(label(lx + 346, ly + 4, "axis", cls="xs", anchor="start"))
    # A wall facing the lamp squarely, 30 degrees off the axis: perpendicular to the
    # ray through its centre, so theta = 0 there and 6.8's tan(theta) is zero.
    phi = math.radians(30.0)
    R = 200.0
    px, py = lx + R * math.cos(phi), ly - R * math.sin(phi)
    tx, ty = math.sin(phi), math.cos(phi)       # along the wall (perpendicular to the ray)
    b.append(cline(px - 62 * tx, py - 62 * ty, px + 62 * tx, py + 62 * ty, GREY, width=3.0))
    b.append(label(px - 62 * tx + 8, py - 62 * ty - 6, "a wall facing the lamp squarely",
                   cls="xs", anchor="start"))
    # two rays one texel apart (exaggerated), meeting the wall, and their axial feet
    feet = []
    for dphi in (-0.07, 0.07):
        a = phi + dphi
        t = R / math.cos(dphi)                  # the ray meets the tangent line here
        qx, qy = lx + t * math.cos(a), ly - t * math.sin(a)
        b.append(carrow(lx, ly, qx, qy, uid, "amber", AMBER, width=1.1))
        b.append(cline(qx, qy, qx, ly, GREY, width=0.8, dash="2 3"))
        feet.append(qx)
    b.append(label(lx + 66, ly - 10, "φ", cls="sm t-hi"))
    b.append(label(lx + 70, ly + 44, "one texel apart on the wall, the two feet on the axis differ:",
                   cls="xs t-bad", anchor="start"))
    b.append(label(lx + 70, ly + 58, "axial depth w changes, though θ = 0 and tan θ says it cannot",
                   cls="xs muted", anchor="start"))
    # the formula panel
    fx = 470.0
    rows = [("per texel, axial depth changes by", "sm", ""),
            ("texel · sin α · cos φ / cos θ", "sm mono t-hi", ""),
            ("α  normal vs the AXIS", "xs", ""),
            ("φ  ray vs the axis   (cos φ = w / distance)", "xs", ""),
            ("θ  normal vs the ray  (cos θ = n · l)", "xs", ""),
            ("", "xs", ""),
            ("on the axis (φ = 0), α = θ: it is 6.8's tan θ", "xs", ""),
            ("a wall facing the lamp (θ = 0): sin φ cos φ —", "xs", ""),
            ("where tan θ says no bias at all", "xs t-bad", ""),
            ("", "xs", ""),
            ("found as CPU/GPU disagreement five pixels from", "xs muted", ""),
            ("any shadow edge; verify_617b §G", "xs muted", "")]
    y = 90
    for text, cls, _ in rows:
        if text:
            b.append(label(fx, y, text, cls=cls, anchor="start"))
        y += 20 if "sm" in cls else 16
    return svg(uid, W, H, "Axial depth versus radial distance",
               "A lamp with its axis drawn horizontally and a wall that faces the lamp squarely "
               "30 degrees off the axis. Two rays a texel apart meet the wall at almost the same "
               "distance from the lamp but with different feet on the axis, so the stored axial "
               "depth changes across the texel; the perspective bias needs sine alpha times "
               "cosine phi over cosine theta rather than tangent theta.", b)


# ===========================================================================
# Figure 7 — the cube faces, and the mirror  (§7)
# ===========================================================================
def glyph(x0, y0, s, mirror, colour):
    """An asymmetric 'F'-like mark, so a left-right flip is unmistakable."""
    pts = [(0, 0), (0.7, 0), (0.7, 0.18), (0.2, 0.18), (0.2, 0.42), (0.55, 0.42), (0.55, 0.6),
           (0.2, 0.6), (0.2, 1.0), (0, 1.0)]
    if mirror:
        pts = [(1.0 - px, py) for px, py in pts]
    return poly([(x0 + px * s, y0 + py * s) for px, py in pts], colour, width=1.6)


def fig_cube():
    uid = "l617bf7"
    W, H = 820, 380
    b = [cmarker(uid, "red", RED), cmarker(uid, "blu", BLUE)]
    b.append(label(20, 24, "Six 90-degree cameras tile the sphere. Each must draw its face the way "
                           "the hardware will READ it.", cls="sm", anchor="start"))
    # the net: a cross of six squares
    s = 62.0
    ox, oy = 40.0, 70.0
    cells = {"+Y": (1, 0), "-X": (0, 1), "+Z": (1, 1), "+X": (2, 1), "-Z": (3, 1), "-Y": (1, 2)}
    for name, (i, j) in cells.items():
        x, y = ox + i * s, oy + j * s
        b.append(hollow(x, y, s, s, GREY if name != "+X" else AMBER, width=1.2 if name != "+X" else 2.0))
        b.append(label(x + s / 2, y + s / 2 + 4, name, cls="sm mono"))
    b.append(label(ox + 2 * s, oy + 3 * s + 22, "SDL's face order: +X -X +Y -Y +Z -Z",
                   cls="xs muted"))
    b.append(label(ox + 2 * s, oy + 3 * s + 36, "layer = 6 · cube + face", cls="xs mono"))

    # the +X face in detail: what the table says
    dx = 330.0
    b.append(label(dx + 100, 72, "the +X face, as the table defines it", cls="xs"))
    b.append(hollow(dx + 40, 84, 120, 120, AMBER, width=1.6))
    b.append(glyph(dx + 70, 108, 70, False, AMBER))
    b.append(carrow(dx + 40, 220, dx + 160, 220, uid, "blu", BLUE, width=1.6))
    b.append(label(dx + 100, 236, "u grows toward −z", cls="xs lbl-z"))
    b.append(carrow(dx + 26, 84, dx + 26, 204, uid, "red", RED, width=1.2))
    b.append(label(dx + 20, 150, "v", cls="xs", anchor="end"))
    b.append(label(dx + 100, 256, "(v grows DOWN the image, toward −y)", cls="xs muted"))

    # what look_at renders
    ex = 580.0
    b.append(label(ex + 100, 72, "what look_at draws there", cls="xs"))
    b.append(hollow(ex + 40, 84, 120, 120, RED, width=1.6))
    b.append(glyph(ex + 70, 108, 70, True, RED))
    b.append(carrow(ex + 160, 220, ex + 40, 220, uid, "blu", BLUE, width=1.6))
    b.append(label(ex + 100, 236, "its right is +z = −u", cls="xs lbl-z"))
    b.append(label(ex + 100, 256, "mirrored: misses by up to 0.970 of the face", cls="xs t-bad"))

    b.append(label(dx + 40, 300, "fit_cube_face: rows (u, −v, −major), determinant −1 — "
                                 "a MIRROR, on purpose", cls="xs mono t-hi", anchor="start"))
    b.append(label(dx + 40, 318, "look_at: rows (−u, −v, −major), determinant +1 — "
                                 "a rotation, and wrong", cls="xs mono", anchor="start"))
    b.append(label(dx + 40, 344, "measured on all six faces, 1,089 directions each: 1.8e-7 of a face",
                   cls="xs muted", anchor="start"))
    return svg(uid, W, H, "Cube shadow faces and the mirror",
               "Left: the six faces of a cube map laid out as a cross, +X highlighted. Middle: "
               "the +X face as the face table defines it, u growing toward minus z. Right: what a "
               "right-handed look_at camera renders there, the mirror image, which misses the "
               "lookup by up to 0.97 of the face.", b)


# ===========================================================================
# Figure 8 — the guard band  (§8)
# ===========================================================================
def fig_guard():
    uid = "l617bf8"
    W, H = 820, 330
    b = [label(20, 24, "A near-clipped corner is FINITE, not small — and a clamp that moves it "
                       "changes the triangle.", cls="sm", anchor="start")]
    # a horizontal, not-to-scale axis of screen x
    y0 = 150.0
    b.append(cline(40, y0, 780, y0, GREY, width=1.0, dash="2 4"))
    xs = {"-40,000 px": (70.0, "middle"), "-8,000": (326.0, "end"), "-7,936": (364.0, "start")}
    for t, (x, anchor) in xs.items():
        tick = x + (4.0 if anchor == "end" else -4.0 if anchor == "start" else 0.0)
        b.append(rule(tick, y0 - 5, tick, y0 + 5, cls="ink"))
        b.append(label(x, y0 + 20, t, cls="xs mono", anchor=anchor))
    for x in (560.0, 640.0):   # the map's edges, x = 0 and x = 512, named in its caption
        b.append(rule(x, y0 - 5, x, y0 + 5, cls="ink"))
    b.append(label(200, y0 + 38, "(not to scale)", cls="xs muted"))
    # the map
    b.append(hollow(560, 90, 80, 120, AMBER, width=1.8))
    b.append(label(600, 84, "the map, x = 0 to 512", cls="xs", anchor="middle"))
    # the true triangle edge from far vertex to a vertex beyond the map
    far = (70.0, 70.0)
    near = (720.0, 230.0)
    b.append(cline(far[0], far[1], near[0], near[1], GREEN, width=1.8))
    b.append(f'<circle cx="{far[0]}" cy="{far[1]}" r="4" fill="{GREEN}"/>')
    b.append(label(far[0] + 8, far[1] - 8, "true corner, on the near plane", cls="xs t-ok",
                   anchor="start"))
    # the clamped vertex: moved along x to -8000
    cl = (330.0, 70.0)
    b.append(f'<circle cx="{cl[0]}" cy="{cl[1]}" r="4" fill="{RED}"/>')
    b.append(cline(cl[0], cl[1], near[0], near[1], RED, width=1.6, dash="5 3"))
    b.append(label(cl[0] + 8, cl[1] - 8, "to_pixel's clamp moves it here", cls="xs t-bad",
                   anchor="start"))
    b.append(label(700, 270, "the edge through the map TILTS: every depth inside moves",
                   cls="xs t-bad", anchor="end"))
    # the guard plane
    b.append(cline(360, 84, 360, 240, BLUE, width=1.6))
    b.append(label(366, 250, "guard plane: clip here, a new corner ON the true edge", cls="xs",
                   anchor="start"))
    b.append(label(20, 302, "measured: a spot light's map read 0.950 where the floor was at 0.987 "
                            "— acne on 88% of it, which no bias could cure", cls="xs",
                   anchor="start"))
    b.append(label(20, 318, "after guard-band clipping: the reference render still byte-identical "
                            "(E917C06C) — no frame in Modules 3–6 ever reached the band",
                   cls="xs muted", anchor="start"))
    return svg(uid, W, H, "Guard-band clipping",
               "A schematic of screen x. A triangle corner on the near plane projects to about "
               "minus 40,000 pixels; clamping it to minus 8,000 moves it and tilts the edge that "
               "crosses the 512-pixel map. Clipping at a guard plane at minus 7,936 instead makes "
               "a new corner on the true edge.", b)


# ===========================================================================
# Figure 9 — the vertex snap, on a bare floor  (§9)
# ===========================================================================
def fig_floor():
    uid = "l617bf9"
    W = 700
    CAP, PAD, PX = 56.0, 30.0, 1.2
    crop = (0, 0, 256, 256)
    pal = [hexrgb("#ffffff"), hexrgb("#8c8c8c")]
    left, LW, LH = render_panel("l617b_floor_68.ppm", crop, PAD, CAP, PX, 1, pal, levels=3,
                                bg="fill-shot")
    right, RW, _ = render_panel("l617b_floor_snap.ppm", crop, W - PAD - LW, CAP, PX, 1, pal,
                                levels=3, bg="fill-shot")
    H = CAP + LH + 58
    b = [label(W / 2, 22, "A bare floor under a bulb, from above: white is lit, black is acne",
               cls="sm")]
    b.append(label(W / 2, 38, "nothing casts a shadow here, so every dark mark is the map "
                              "disagreeing with itself; grey is beyond the bulb's range",
                   cls="xs muted"))
    b += left + right
    b.append(label(PAD + LW / 2, CAP + LH + 20, f"6.8's reach: {FLOOR_ACNE_68} points of acne",
                   cls="xs t-bad"))
    b.append(label(PAD + LW / 2, CAP + LH + 35, "stripes along one cube face", cls="xs muted"))
    b.append(label(W - PAD - RW / 2, CAP + LH + 20, "+ half a texel for the vertex snap: 0",
                   cls="xs t-ok"))
    b.append(label(W - PAD - RW / 2, CAP + LH + 35, f"of {FLOOR_JUDGED:,} points", cls="xs muted"))
    return svg(uid, W, int(H), "Acne from the vertex snap",
               "Two top-down visibility maps of a bare floor under a point light, white where "
               "lit. With 6.8's reach, stripes of acne run along one cube face; with half a "
               "texel added for the rasterizer's vertex snapping, the floor is clean.", b)


# ===========================================================================
# Figure 10 — one light, as the shader reads it  (§10.3)
# ===========================================================================
def fig_record():
    uid = "l617bf10"
    W, H = 780, 380
    b = [label(20, 24, "gpu_local_light: eleven float4s, 176 bytes, one per lamp, in a "
                       "StructuredBuffer at t8.", cls="sm", anchor="start")]
    x0, y0, rh = 110.0, 50.0, 24.0
    cw = 64.0
    for i, (name, lo, hi) in enumerate(RECORD):
        y = y0 + i * rh
        col = AMBER if i < 3 else (BLUE if i < 7 else PURPLE)
        for k in range(4):
            b.append(box(x0 + k * cw, y, cw, rh - 3, col, opacity=0.18, width=1.0))
        b.append(label(x0 - 10, y + 15, f"{16 * i}", cls="xs mono", anchor="end"))
        b.append(label(x0 + 4 * cw + 12, y + 15, name, cls="xs mono", anchor="start"))
        if lo:
            b.append(label(x0 + 4 * cw + 118, y + 15, f"{lo}; {hi}" if hi else lo, cls="xs muted",
                           anchor="start"))
    for k, c in enumerate("xyzw"):
        b.append(label(x0 + k * cw + cw / 2, y0 - 6, c, cls="xs mono"))
    b.append(label(x0 - 10, y0 + 11 * rh + 12, "176", cls="xs mono", anchor="end"))
    fy = y0 + 11 * rh + 30
    b.append(label(20, fy, "every member a float4, so DXC, glslang and SPIRV-Cross lay it out "
                           "identically; the matrix by ROWS, so no default packing can transpose it",
                   cls="xs", anchor="start"))
    b.append(label(20, fy + 16, "the COUNT is not in here: it is 6.15's spare float at offset 180 of "
                                "the light block, set by the renderer, never by the caller",
                   cls="xs muted", anchor="start"))
    return svg(uid, W, H, "The light record layout",
               "Eleven rows of four floats: position and range, radiance and kind, spot axis and "
               "cone scale, cone offset and shadow slot and planes, texel and pcf and strength, "
               "the bias knobs, the normal-offset scale, and four rows of the spot shadow "
               "matrix, 176 bytes in all.", b)


# ===========================================================================
# Figure 11 — the shadow passes in the frame graph, and a frame without them  (§10.5)
# ===========================================================================
def fig_graph():
    uid = "l617bf11"
    W, H = 820, 360
    b = [label(20, 24, "Two frames. The first declares seven shadow passes; the second declares "
                       "none and reads last frame's maps.", cls="sm", anchor="start")]
    # frame 1
    b.append(label(200, 52, "frame 1: everything declared", cls="xs"))
    faces = ["+X", "-X", "+Y", "-Y", "+Z", "-Z"]
    bx, by, bw, bh = 40.0, 64.0, 50.0, 26.0
    for i, f in enumerate(faces):
        x = bx + i * (bw + 6)
        b.append(box(x, by, bw, bh, AMBER, opacity=0.22, width=1.0))
        b.append(label(x + bw / 2, by + 17, f"face {f}", cls="xs mono"))
        if i:
            b.append(cline(x - 6, by + bh / 2, x, by + bh / 2, GREY, width=1.0))
    b.append(box(bx, by + 44, 110, bh, BLUE, opacity=0.22, width=1.0))
    b.append(label(bx + 55, by + 61, "spot 1", cls="xs mono"))
    b.append(box(bx + 150, by + 100, 180, bh + 4, GREEN, opacity=0.22, width=1.0))
    b.append(label(bx + 240, by + 119, "scene: samples @6 and @1", cls="xs mono"))
    b.append(cline(bx + 5 * (bw + 6) + bw / 2, by + bh, bx + 240, by + 100, GREY, width=1.0))
    b.append(cline(bx + 55, by + 44 + bh, bx + 200, by + 100, GREY, width=1.0))
    b.append(label(bx, by + 160, "every face: CLEAR, STORE (derived)", cls="xs", anchor="start"))
    b.append(label(bx, by + 176, "the faces form a CHAIN: each clear consumes", cls="xs muted",
                   anchor="start"))
    b.append(label(bx, by + 190, "the previous version, so layers survive", cls="xs muted",
                   anchor="start"))
    b.append(label(bx, by + 214, f"vs the hand-recorded frame: 0 of {CHANNELS:,} channels differ",
                   cls="xs t-ok", anchor="start"))

    # frame 2
    fx = 470.0
    b.append(label(fx + 150, 52, "frame 2: the cache", cls="xs"))
    b.append(box(fx, 64, 130, 26, GREY, opacity=0.15, width=1.0, dash="3 3"))
    b.append(label(fx + 65, 81, "point shadows @0", cls="xs mono"))
    b.append(box(fx, 108, 130, 26, GREY, opacity=0.15, width=1.0, dash="3 3"))
    b.append(label(fx + 65, 125, "spot shadows @0", cls="xs mono"))
    b.append(box(fx + 170, 164, 150, 30, GREEN, opacity=0.22, width=1.0))
    b.append(label(fx + 245, 183, "scene: samples @0", cls="xs mono"))
    b.append(cline(fx + 130, 77, fx + 200, 164, GREY, width=1.0))
    b.append(cline(fx + 130, 121, fx + 190, 164, GREY, width=1.0))
    b.append(label(fx, 224, "an IMPORT's version 0 is real: whatever the", cls="xs", anchor="start"))
    b.append(label(fx, 238, "texture holds when the frame begins", cls="xs", anchor="start"))
    b.append(label(fx, 262, "1 live pass; 0 channels differ from frame 1", cls="xs t-ok",
                   anchor="start"))
    b.append(label(fx, 278, f"control: clear the maps first — {CACHE_CONTROL:,} change",
                   cls="xs muted", anchor="start"))
    return svg(uid, W, H, "Local shadows in the frame graph",
               "Frame one declares six cube-face passes in a chain and one spot pass, each "
               "clearing and storing, before the scene pass samples the final versions. Frame "
               "two declares no shadow passes and samples version zero of the imported maps, "
               "which is last frame's contents, and renders the same image.", b)


# ===========================================================================
# Figure 12 — both renderers  (§11)
# ===========================================================================
def fig_both():
    uid = "l617bf12"
    W = 820
    CAP, PAD, GAP, PX = 52.0, 14.0, 12.0, 2.0
    crop = (0, 0, 256, 256)
    a, AW, AH = render_panel("l617b_cpu.ppm", crop, PAD, CAP, PX, 2, PANEL_PALETTE, levels=4)
    g, GW, _ = render_panel("l617b_gpu.ppm", crop, PAD + AW + GAP, CAP, PX, 2, PANEL_PALETTE,
                            levels=4)
    # The mask by PEAK, not average: a one-pixel red edge inside a 2x2 block
    # would otherwise average to a quarter of its brightness and vanish.
    mw, _mh, mdata = read_ppm(os.path.join(OUT, "l617b_mask.ppm"))
    mgw, mgh, mgrid = peak_sample(mdata, mw, crop, 2)
    m = rle_rects(mgrid, mgw, mgh, PAD + AW + GW + 2 * GAP, CAP, PX, [hexrgb("#ff3030")],
                  levels=1, bg_class="fill-shot")
    MW = mgw * PX
    H = CAP + AH + 60
    b = [label(W / 2, 22, "The same lamps through light.hpp and scene.frag.hlsl, pixel for pixel",
               cls="sm")]
    b.append(label(W / 2, 38, "each pixel a known floor point; compared in linear light",
                   cls="xs muted"))
    b += a + g + m
    b.append(label(PAD + AW / 2, CAP + AH + 20, "CPU: shade_local + local_shadow_set", cls="xs"))
    b.append(label(PAD + AW + GAP + GW / 2, CAP + AH + 20, "GPU: storage buffer + cube array",
                   cls="xs"))
    b.append(label(PAD + AW + GW + 2 * GAP + MW / 2, CAP + AH + 20,
                   f"red: the {G_DIFFER} that differ", cls="xs t-bad"))
    b.append(label(W / 2, CAP + AH + 42,
                   f"of {G_LIT:,} lit pixels; every one within 1 px of a shadow edge, where the two "
                   f"FILTERS may differ; a 5% cone change moves {G_CONTROL:,}", cls="xs muted"))
    return svg(uid, W, int(H), "Both renderers compared",
               "Three panels from above: the CPU render, the GPU render, and a mask of the 259 "
               "pixels where they differ, which trace the shadow edges and nowhere else.", b)


# ===========================================================================
# Figure 13 — the demo  (Build & run)
# ===========================================================================
def fig_demo():
    uid = "l617bf13"
    W = 820
    CAP, PAD, GAP, PX = 44.0, 18.0, 16.0, 1.6
    crop = (40, 20, 280 + 40 + 120, 270)
    # The spot's pool comes out bluer on the left, and that is REAL, not the
    # quantiser: with shadows on, the slab blocks the bulb's warm light from the
    # pool (R and G drop four codes there, B does not). Checked against the renders
    # before the caption was allowed to say so.
    pal = [WARM, COOL, FLOORG, GOLD, hexrgb("#ffffff")]
    a, AW, AH = render_panel("l617b_demo.ppm", crop, PAD, CAP, PX, 2, pal, levels=6, gain=2.5)
    c, CW, _ = render_panel("l617b_demo_noshadow.ppm", crop, PAD + AW + GAP, CAP, PX, 2, pal,
                            levels=6, gain=2.5)
    H = CAP + AH + 50
    b = [label(W / 2, 20, "gltf_view --lights, and the same with --lamp-shadows 0 (CPU renderer)",
               cls="sm")]
    b.append(label(W / 2, 36, "both brightened 2.5x for the page: the scene is lit for night",
                   cls="xs muted"))
    b += a + c
    b.append(label(PAD + AW / 2, CAP + AH + 20, "both lamps shadowed", cls="xs"))
    b.append(label(PAD + AW / 2, CAP + AH + 35, "the bulb throws the slab's and the cube's shadows left",
                   cls="xs muted"))
    b.append(label(PAD + AW + GAP + CW / 2, CAP + AH + 20, "the same light, no occlusion",
                   cls="xs muted"))
    return svg(uid, W, int(H), "The demo with and without lamp shadows",
               "Two frames from the glTF viewer, brightened for the page, lit by a warm point "
               "light behind the model and a cool spot light in front of it, with the sun turned "
               "down. On the left both lamps cast shadows, and the bulb throws the slab's and the "
               "cube's shadows to the left; on the right the same light arrives with nothing "
               "occluding it.", b)


FIGS = [fig_spheres, fig_windows, fig_cone, fig_acne, fig_pyramid, fig_axial, fig_cube,
        fig_guard, fig_floor, fig_record, fig_graph, fig_both, fig_demo]


def main():
    for i, fn in enumerate(FIGS, start=1):
        path = os.path.join(OUT, f"l617b_fig{i}.svg")
        with open(path, "w", encoding="utf-8") as fh:
            fh.write(fn())
        print(f"wrote {path}")


if __name__ == "__main__":
    main()
