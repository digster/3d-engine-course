#!/usr/bin/env python3
"""scratch/figs_63.py — Lesson 6.3's diagrams.

Same rules as 6.1's and 6.2's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - every element well inside the viewBox, and in-bar labels checked against the bar
  - ~5.2 units per character for `xs`, ~6.0 for `sm`
  - filenames numbered by PAGE ORDER

Every number is either computed from the distributions defined here (which are the
same formulas microfacet.hpp implements) or copied from verify_63's output.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)

OUT = "scratch"


# ---- the distributions, so every plotted point is the real one --------------
def d_blinn(c, a):
    if c <= 0.0:
        return 0.0
    s = 2.0 / (a * a) - 2.0
    return ((s + 2.0) / (2.0 * math.pi)) * (c ** s)


def d_beckmann(c, a):
    if c <= 0.0:
        return 0.0
    c2 = c * c
    t2 = (1.0 - c2) / c2
    return math.exp(-t2 / (a * a)) / (math.pi * a * a * c2 * c2)


def d_ggx(c, a):
    if c <= 0.0:
        return 0.0
    a2 = a * a
    d = (1.0 - c) * (1.0 + c) + a2 * c * c
    return a2 / (math.pi * d * d)


def g1_ggx(c, a):
    if c <= 0.0:
        return 0.0
    a2 = a * a
    return 2.0 * c / (c + math.sqrt(a2 + (1.0 - a2) * c * c))


# ---- measured inputs, from verify_63 ----------------------------------------

# §B: shininess -> (engine 1/pi, normalised, ratio)
CONSTANTS = [(4, 0.318310, 0.954930, 3.0), (8, 0.318310, 1.591549, 5.0),
             (32, 0.318310, 5.411268, 17.0), (128, 0.318310, 20.690142, 65.0),
             (512, 0.318310, 81.805641, 257.0)]

# §C: theta_h -> (Beckmann, GGX) at alpha 0.3
TAIL = [(5, 3.29834, 3.05023), (10, 2.66178, 2.07712), (20, 1.0409, 0.742317),
        (30, 0.154877, 0.284188), (45, 2.11433e-4, 9.64494e-2),
        (60, 1.88905e-13, 4.8006e-2), (75, 0.0, 3.2488e-2)]

# §F: roughness -> R(v) at 0 and 75 degrees, for D*G/(4 nl nv) with F = 1
BUDGET = [(0.22, 0.9976, 0.9775), (0.50, 0.9158, 0.8393), (0.71, 0.6838, 0.7600),
          (0.87, 0.4587, 0.6722), (1.00, 0.3069, 0.5906)]

CHECKS = 27


# ===========================================================================
# Figure 1 — a surface is a landscape of tiny mirrors  (§3)
# ===========================================================================
def fig_landscape():
    uid = "l63f1"
    W, H = 720, 430
    b = [label(20, 20, "The whole model, in one picture: every microfacet is a perfect "
                       "mirror, so only the ones facing h send light at you.",
                       "sm", "start")]

    BASE = 214
    X0, X1 = 56, 664
    AMP = 1.9            # slope scale — the surface has to LOOK rough to make the point

    def height(x):
        t = (x - X0) / (X1 - X0)
        return AMP * (7.0 * math.sin(t * 23.0 + 0.4) + 5.0 * math.sin(t * 41.0 + 1.9)
                      + 3.5 * math.sin(t * 67.0 + 3.1))

    a_in = math.radians(211)
    a_out = math.radians(-42)
    hx = math.cos(a_in) + math.cos(a_out)
    hy = math.sin(a_in) + math.sin(a_out)
    hl = math.hypot(hx, hy)
    hx, hy = hx / hl, hy / hl

    N = 48
    xs = [X0 + (X1 - X0) * i / N for i in range(N + 1)]
    pts = [(x, BASE - height(x)) for x in xs]

    facets = []
    for i in range(N):
        (x0, y0), (x1, y1) = pts[i], pts[i + 1]
        nx, ny = -(y1 - y0), (x1 - x0)
        nl = math.hypot(nx, ny)
        nx, ny = nx / nl, ny / nl
        if ny > 0:
            nx, ny = -nx, -ny
        facets.append((i, (x0 + x1) / 2, (y0 + y1) / 2, nx, ny, nx * hx + ny * hy))

    # THE THRESHOLD IS TUNED SO THE PICTURE MAKES THE POINT IT CLAIMS. A first
    # draft counted 32 of 74 facets as aligned, which says "most of the surface
    # reflects at you" — the opposite of the model. 0.9975 is about a 4-degree
    # cone, which is a glossy surface, and it selects a handful.
    aligned = [f for f in facets if f[5] > 0.9975]

    d = "M {} {} ".format(X0, BASE + 52) + " ".join(
        f"L {x:.1f} {y:.1f}" for x, y in pts) + f" L {X1} {BASE + 52} Z"
    b.append(f'<path d="{d}" class="fill-soft grid" stroke-width="1.5"/>')

    # EVERY facet's normal, faint — the contrast is the message, so the ones that
    # do not count have to be visible too.
    for i, mx, my, nx, ny, cosang in facets:
        if cosang > 0.9975:
            continue
        b.append(rule(mx, my, mx + 13 * nx, my + 13 * ny, "grid", 1.0))

    for i, mx, my, nx, ny, _ in aligned:
        (x0, y0), (x1, y1) = pts[i], pts[i + 1]
        b.append(f'<line x1="{x0:.1f}" y1="{y0:.1f}" x2="{x1:.1f}" y2="{y1:.1f}" '
                 f'stroke="{PURPLE}" stroke-width="3.4"/>')
        b.append(rule(mx, my, mx + 27 * nx, my + 27 * ny, "hi", 1.8))
        b.append(f'<circle cx="{mx + 27 * nx:.1f}" cy="{my + 27 * ny:.1f}" r="2.6" '
                 f'fill="{PURPLE}" stroke="none"/>')

    anchor = aligned[len(aligned) // 2] if aligned else facets[N // 2]
    mx, my = anchor[1], anchor[2]
    b.append(arrow(mx + 152 * math.cos(a_in), my + 152 * math.sin(a_in),
                   mx + 8 * math.cos(a_in), my + 8 * math.sin(a_in), uid, "h", 1.8))
    b.append(label(mx + 164 * math.cos(a_in) - 6, my + 164 * math.sin(a_in),
                   "l", "sm mono t-hi"))
    b.append(arrow(mx + 8 * math.cos(a_out), my + 8 * math.sin(a_out),
                   mx + 152 * math.cos(a_out), my + 152 * math.sin(a_out),
                   uid, "s", 1.5, dash="5 3"))
    b.append(label(mx + 166 * math.cos(a_out) + 5, my + 166 * math.sin(a_out),
                   "v", "sm mono"))
    b.append(arrow(mx, my, mx + 86 * hx, my + 86 * hy, uid, "i", 1.7))
    b.append(label(mx + 98 * hx + 4, my + 98 * hy + 4, "h", "sm mono t-hi"))

    b.append(label(W / 2, BASE + 82,
                   f"{len(aligned)} of {N} facets face h closely enough to count",
                   "sm t-hi"))
    b.append(label(W / 2, BASE + 100,
                   "— and “how many?” is the only question the model asks",
                   "xs muted"))

    by = 344
    b.append(box(24, by, W - 48, 66, None, rx=4))
    b.append(label(W / 2, by + 24,
                   "So the shape of a highlight is a HISTOGRAM of surface slopes, "
                   "and roughness is that histogram’s width.", "sm t-hi"))
    b.append(label(W / 2, by + 46,
                   "Lesson 3.7 already derived that h is “the normal this surface would "
                   "need in order to bounce the light into the eye”. That stops being a "
                   "metaphor here.", "xs muted"))

    return svg(uid, W, H, "A surface as a landscape of microfacets",
               "A cross-section of a rough surface with every facet's normal drawn. "
               "Light arrives from l and leaves toward v; only the few facets whose own "
               "normal is close to the halfway vector h reflect between them, so the "
               "highlight's shape is a histogram of surface slopes.", b)


# ===========================================================================
# Figure 2 — the NDF is a probability density  (§4)
# ===========================================================================
def fig_density():
    uid = "l63f2"
    W, H = 720, 430
    b = [label(20, 20, "An NDF is not a curve chosen to look right. It is a "
                       "distribution, so it has to add up to one — and that can "
                       "be checked.", "sm", "start")]

    X0, Y0, PW, PH = 92, 58, 396, 208
    MAX_T = 90.0
    # The peaks differ 16x across these three roughnesses while the AREAS are all
    # exactly 1 — which is the point, so the axis is linear and sized to the
    # tallest curve rather than clipping it. A first draft picked a roughness
    # whose peak was 10 and cut it off at 3.2, which reads as a drawing error.
    MAX_D = 5.6

    b.append(rule(X0, Y0 + PH, X0 + PW, Y0 + PH, "grid", 1.2))
    b.append(rule(X0, Y0, X0, Y0 + PH, "grid", 1.2))
    b.append(label(X0 + PW / 2, Y0 + PH + 38,
                   "θₕ — how far a microfacet is tilted from the surface", "xs muted"))
    b.append(label(X0 - 34, Y0 - 12, "D(h) · cos θₕ", "xs muted", "start"))
    for frac, txt in ((0.0, "0°"), (1 / 3, "30°"), (2 / 3, "60°"), (1.0, "90°")):
        x = X0 + frac * PW
        b.append(rule(x, Y0 + PH, x, Y0 + PH + 5, "grid", 1.0))
        b.append(label(x, Y0 + PH + 18, txt, "xs muted"))

    curves = [(0.50, GREEN), (0.71, BLUE), (1.00, PURPLE)]
    for r, col in curves:
        a = r * r
        pts = []
        for k in range(181):
            th = MAX_T * k / 180.0
            c = math.cos(math.radians(th))
            y = d_ggx(c, a) * c
            pts.append((X0 + PW * th / MAX_T, Y0 + PH - PH * min(y, MAX_D) / MAX_D))
        # fill under the curve, faintly, because it is the AREA that is the claim
        fill = "M " + " L ".join(f"{x:.1f} {y:.1f}" for x, y in pts) + \
               f" L {X0 + PW} {Y0 + PH} L {X0} {Y0 + PH} Z"
        b.append(f'<path d="{fill}" fill="{col}" fill-opacity="0.10" stroke="none"/>')
        d = "M " + " L ".join(f"{x:.1f} {y:.1f}" for x, y in pts)
        b.append(f'<path d="{d}" fill="none" stroke="{col}" stroke-width="2.0"/>')
        # label at the curve's own peak height, to the left of the axis clutter
        peak = d_ggx(1.0, a)
        py = Y0 + PH - PH * min(peak, MAX_D) / MAX_D
        # THE COLOUR GOES ON THE LEADER, NOT ON THE TEXT, and that is not a
        # style preference — `figure.dia svg text { fill: ... }` in course.css
        # always beats an SVG presentation attribute, so `fill="{col}"` on a
        # <text> is silently dropped and the label renders in default ink.
        # Nothing errors; the association with the curve is simply lost.
        #
        # A coloured SHAPE beside plain text is the idiom this file already uses
        # for both its legends (see fig_constant and fig_budget below), and the
        # leader was already here — it only had to stop being grey. Six units of
        # dash is now the swatch.
        b.append(f'<text x="{X0 + PW + 10}" y="{py + 4:.1f}" class="xs" '
                 f'text-anchor="start">'
                 f'{esc(f"roughness {r:.2f}")}</text>')
        b.append(f'<line x1="{X0 + PW}" y1="{py}" x2="{X0 + PW + 6}" y2="{py}" '
                 f'stroke="{col}" stroke-width="2.0"/>')

    b.append(label(X0 + PW * 0.46, Y0 + 30,
                   "the peaks differ 16×; the AREAS are all exactly 1", "xs t-hi"))
    b.append(label(X0 + PW * 0.46, Y0 + 46,
                   "(measured: 1.0000004 at 400k samples)", "xs muted"))

    by = 318
    b.append(box(24, by, W - 48, 100, None, rx=4))
    b.append(label(W / 2, by + 26, "∫ D(h) · cos θₕ · dω  =  1", "sm mono t-hi"))
    b.append(label(W / 2, by + 48,
                   "Read backwards it says something better than a normalisation "
                   "rule: the microfacets’ PROJECTED AREAS add up to the flat area",
                   "xs muted"))
    b.append(label(W / 2, by + 64,
                   "they stand on. Nothing else could be true, which is why the cosine "
                   "is in there and not a convention.", "xs muted"))
    b.append(label(W / 2, by + 86,
                   "A bare cosˢ lobe integrates to 0.1848, not 1 — which is exactly why "
                   "it is a shape and not a model. The constant it lacks is 2π/(s+2).",
                   "xs t-hi"))

    return svg(uid, W, H, "The NDF as a probability density",
               "Three GGX distributions at roughness 0.50, 0.71 and 1.00, plotted "
               "against microfacet tilt and weighted by the cosine. The peaks differ by "
               "a factor of sixteen; the area under each is exactly 1, which is what "
               "makes it a distribution rather than a chosen curve.", b)


# ===========================================================================
# Figure 3 — Blinn-Phong was an NDF all along  (§5)
# ===========================================================================
def fig_constant():
    uid = "l63f3"
    W, H = 720, 400
    b = [label(20, 20, "Blinn-Phong is a microfacet distribution. It has only ever been "
                       "missing its constant — and the engine ships the wrong one.",
                       "sm", "start")]

    X0, ROW = 150, 26
    TOP = 74
    UNIT = 4.6      # px per unit of the constant; the largest bar is 81.8

    b.append(label(X0, TOP - 16, "the constant multiplying cosˢ:", "xs muted",
                   "start"))
    for i, (s, eng, norm, ratio) in enumerate(CONSTANTS):
        y = TOP + i * ROW
        b.append(label(X0 - 10, y + 12, f"shininess {s}", "xs mono muted", "end"))
        b.append(box(X0, y, max(2.0, eng * UNIT), 9, RED, rx=2, opacity=0.75))
        b.append(box(X0, y + 11, min(norm * UNIT, 400.0), 9, GREEN, rx=2, opacity=0.55))
        b.append(label(X0 + min(norm * UNIT, 400.0) + 10, y + 16,
                       f"{ratio:.0f}×", "xs mono t-bad", "start"))

    ly = TOP + len(CONSTANTS) * ROW + 8
    b.append(f'<rect x="{X0}" y="{ly}" width="26" height="9" rx="2" fill="{RED}" '
             f'fill-opacity="0.75" stroke="{RED}" stroke-width="1"/>')
    b.append(label(X0 + 32, ly + 8, "what the engine uses: 1/π, for every exponent",
                   "xs", "start"))
    b.append(f'<rect x="{X0}" y="{ly + 15}" width="26" height="9" rx="2" fill="{GREEN}" '
             f'fill-opacity="0.55" stroke="{GREEN}" stroke-width="1"/>')
    b.append(label(X0 + 32, ly + 23, "what a distribution needs: (s+2)/2π",
                   "xs", "start"))

    by = 300
    b.append(box(24, by, W - 48, 84, None, rx=4))
    b.append(label(W / 2, by + 24,
                   "The ratio is (s+2)/2 exactly — no π, because both carry "
                   "one and it cancels. At the default shininess of 32 that is "
                   "17×.", "sm t-hi"))
    b.append(label(W / 2, by + 46,
                   "So this engine’s highlights have always been seventeen times "
                   "dimmer than the distribution says, and every material has quietly",
                   "xs muted"))
    b.append(label(W / 2, by + 62,
                   "compensated with a specular colour larger than a real surface could "
                   "have. Nothing looked wrong; the parameter absorbed it.", "xs muted"))

    return svg(uid, W, H, "The constant Blinn-Phong was missing",
               "For five shininess values, the constant the engine multiplies the lobe "
               "by against the one a normalised distribution requires. The ratio is "
               "(s+2)/2, which is 17 at the default shininess of 32.", b)


# ===========================================================================
# Figure 4 — the tail  (§6)
# ===========================================================================
def fig_tail():
    uid = "l63f4"
    W, H = 720, 430
    b = [label(20, 20, "Same peak, same roughness, completely different tail — and "
                       "the tail is the whole reason GGX replaced Beckmann.",
                       "sm", "start")]

    X0, Y0, PW, PH = 92, 66, 420, 230
    MAX_T = 75.0
    LO, HI = -8.0, 1.0        # log10 range

    def ly(v):
        if v <= 0:
            return Y0 + PH
        return Y0 + PH - PH * (max(LO, min(HI, math.log10(v))) - LO) / (HI - LO)

    b.append(rule(X0, Y0 + PH, X0 + PW, Y0 + PH, "grid", 1.2))
    b.append(rule(X0, Y0, X0, Y0 + PH, "grid", 1.2))
    for e in range(int(LO), int(HI) + 1, 2):
        y = ly(10.0 ** e)
        b.append(rule(X0, y, X0 + PW, y, "grid", 0.7, dash="2 4"))
        b.append(label(X0 - 8, y + 4, f"10{'⁻' if e < 0 else ''}"
                       f"{str(abs(e)).translate(str.maketrans('0123456789','⁰¹²³⁴⁵⁶⁷⁸⁹'))}",
                       "xs mono muted", "end"))
    for deg in (0, 15, 30, 45, 60, 75):
        x = X0 + PW * deg / MAX_T
        b.append(rule(x, Y0 + PH, x, Y0 + PH + 5, "grid", 1.0))
        b.append(label(x, Y0 + PH + 18, f"{deg}°", "xs muted"))
    b.append(label(X0 + PW / 2, Y0 + PH + 36, "θₕ — degrees off the peak",
                   "xs muted"))
    b.append(label(X0 - 46, Y0 - 12, "D(h), log scale", "xs muted", "start"))

    a = 0.3
    for fn, col, name in ((d_beckmann, BLUE, "Beckmann"), (d_ggx, PURPLE, "GGX")):
        pts = []
        for k in range(151):
            th = MAX_T * k / 150.0
            pts.append((X0 + PW * th / MAX_T, ly(fn(math.cos(math.radians(th)), a))))
        d = "M " + " L ".join(f"{x:.1f} {y:.1f}" for x, y in pts)
        b.append(f'<path d="{d}" fill="none" stroke="{col}" stroke-width="2.0"/>')

    # Both labels go OFF their own curve — Beckmann's to the left of its plunge,
    # GGX's above its plateau. A first draft put "Beckmann" on the curve it names,
    # which check-page.js reports and the eye does not.
    # Each name gets a short dash in its own curve's colour, immediately to the
    # LEFT of the word. The colour cannot go on the <text> — course.css's
    # `figure.dia svg text` rule overrides a presentation attribute silently —
    # and it has to go somewhere, because both labels sit deliberately OFF their
    # curves and the position alone no longer says which is which.
    #
    # `xs` is 9.5px, so ~5.2 units per character: "Beckmann" is 8 characters and
    # anchored `end`, "GGX" is 3 and anchored `middle`. Both swatches are placed
    # from the measured left edge, computed rather than nudged.
    bx = X0 + PW * 0.44 - 10
    by_ = ly(d_beckmann(math.cos(math.radians(38)), a))
    b.append(f'<text x="{bx:.1f}" y="{by_:.1f}"'
             f' class="xs" text-anchor="end">Beckmann</text>')
    b.append(f'<line x1="{bx - 8 * 5.2 - 18:.1f}" y1="{by_ - 3:.1f}" '
             f'x2="{bx - 8 * 5.2 - 6:.1f}" y2="{by_ - 3:.1f}" '
             f'stroke="{BLUE}" stroke-width="2.0"/>')

    gx_ = X0 + PW * 0.86
    gy_ = ly(d_ggx(math.cos(math.radians(64)), a)) - 16
    b.append(f'<text x="{gx_:.1f}" y="{gy_:.1f}"'
             f' class="xs" text-anchor="middle">GGX</text>')
    b.append(f'<line x1="{gx_ - 1.5 * 5.2 - 18:.1f}" y1="{gy_ - 3:.1f}" '
             f'x2="{gx_ - 1.5 * 5.2 - 6:.1f}" y2="{gy_ - 3:.1f}" '
             f'stroke="{PURPLE}" stroke-width="2.0"/>')

    # the crossover and the 45-degree gap, both marked on the curves
    x45 = X0 + PW * 45.0 / MAX_T
    b.append(rule(x45, ly(2.11433e-4), x45, ly(9.64494e-2), "hi", 2.0))
    b.append(label(x45 + 8, (ly(2.11433e-4) + ly(9.64494e-2)) / 2 + 4, "456×",
                   "xs mono t-hi", "start"))

    by = 336
    b.append(box(24, by, W - 48, 80, None, rx=4))
    b.append(label(W / 2, by + 22,
                   "GGX is LOWER than Beckmann near the peak (0.71× at 20°) "
                   "and 456× higher at 45°. The tail is paid for.",
                   "sm t-hi"))
    b.append(label(W / 2, by + 44,
                   "Beckmann’s exponential is numerically zero by 60° "
                   "(1.9e−13); GGX’s rational function has a power-law tail "
                   "that never quite stops. Real", "xs muted"))
    b.append(label(W / 2, by + 60,
                   "surfaces have that glow around a highlight, which is why every "
                   "engine now ships GGX.", "xs muted"))

    return svg(uid, W, H, "GGX against Beckmann: the tail",
               "Both distributions at roughness alpha 0.3, on a log scale. They share a "
               "peak; GGX is lower near it and 456 times higher 45 degrees out, because "
               "a rational function has a power-law tail and an exponential does not.", b)


# ===========================================================================
# Figure 5 — shadowing and masking  (§7)
# ===========================================================================
def fig_masking():
    uid = "l63f5"
    W, H = 720, 420
    b = [label(20, 20, "The second term, and it is a consequence rather than a fudge: "
                       "facets hide behind each other.", "sm", "start")]

    # ---- left: a grazing light, and the shadows it casts -------------------
    BASE, X0, X1 = 196, 44, 336
    def height(x):
        t = (x - X0) / (X1 - X0)
        return 13.0 * math.sin(t * 15.0 + 0.6) + 8.0 * math.sin(t * 29.0 + 2.2)

    def surf_y(x):
        return BASE - height(x)

    # The light TRAVELS down and to the right at a shallow angle — grazing, which
    # is the whole point, since that is where masking bites.
    ang = math.radians(14.0)
    dx, dy = math.cos(ang), math.sin(ang)

    N = 46
    xs = [X0 + (X1 - X0) * i / N for i in range(N + 1)]
    pts = [(x, surf_y(x)) for x in xs]

    def is_lit(px, py):
        # Walk back along the ray. If the profile ever rises above it, this point
        # is in another facet's shadow.
        for k in range(1, 160):
            t = k * 2.0
            qx, qy = px - t * dx, py - t * dy
            if qx < X0:
                return True
            if surf_y(qx) < qy - 0.4:
                return False
        return True

    lit = [is_lit(px, py) for px, py in pts]

    d = "M {} {} ".format(X0, BASE + 44) + " ".join(
        f"L {x:.1f} {y:.1f}" for x, y in pts) + f" L {X1} {BASE + 44} Z"
    b.append(f'<path d="{d}" class="fill-soft grid" stroke-width="1.3"/>')
    for i in range(N):
        on = lit[i] and lit[i + 1]
        b.append(f'<line x1="{pts[i][0]:.1f}" y1="{pts[i][1]:.1f}" '
                 f'x2="{pts[i+1][0]:.1f}" y2="{pts[i+1][1]:.1f}" '
                 f'stroke="{AMBER if on else GREY}" stroke-width="{3.0 if on else 1.8}" '
                 f'stroke-opacity="{0.95 if on else 0.45}"/>')

    # Rays that actually land on the surface, drawn only where they land lit —
    # an arrow ending in mid-air is what the first draft did, and it read as a
    # drawing error rather than as light.
    drawn = 0
    for i in range(0, N + 1, 3):
        if drawn >= 5 or not lit[i]:
            continue
        px, py = pts[i]
        b.append(arrow(px - 86 * dx, py - 86 * dy, px - 5 * dx, py - 5 * dy,
                       uid, "s", 1.1))
        drawn += 1

    n_lit = sum(1 for i in range(N) if lit[i] and lit[i + 1])
    b.append(label((X0 + X1) / 2, BASE + 66,
                   f"{n_lit} of {N} facets are lit at all", "xs t-hi"))
    b.append(label((X0 + X1) / 2, BASE + 82,
                   "the rest are in each other's shadow", "xs muted"))

    # ---- right: G1 against angle, for four roughnesses ---------------------
    PX, PY, PW, PH = 432, 70, 216, 186
    b.append(rule(PX, PY + PH, PX + PW, PY + PH, "grid", 1.2))
    b.append(rule(PX, PY, PX, PY + PH, "grid", 1.2))
    b.append(label(PX + PW / 2, PY + PH + 36, "angle from the normal", "xs muted"))
    b.append(label(PX - 8, PY - 12, "G₁ — the fraction still visible",
                   "xs muted", "start"))
    for frac, txt in ((0.0, "0°"), (0.5, "45°"), (1.0, "90°")):
        x = PX + frac * PW
        b.append(rule(x, PY + PH, x, PY + PH + 5, "grid", 1.0))
        b.append(label(x, PY + PH + 18, txt, "xs muted"))
    for v, txt in ((0.0, "0"), (1.0, "1")):
        b.append(label(PX - 8, PY + PH - PH * v + 4, txt, "xs mono muted", "end"))

    # Labels are placed at each curve's value at 82 degrees, where the four have
    # spread out — at 75 the top two sat on each other.
    for r, col in ((0.22, GREEN), (0.45, BLUE), (0.71, PURPLE), (1.00, RED)):
        a = r * r
        pts2 = [(PX + PW * deg / 90.0,
                 PY + PH - PH * g1_ggx(math.cos(math.radians(deg)), a))
                for deg in [90.0 * k / 90.0 for k in range(91)]]
        dd = "M " + " L ".join(f"{x:.1f} {y:.1f}" for x, y in pts2)
        b.append(f'<path d="{dd}" fill="none" stroke="{col}" stroke-width="1.8"/>')
        g = g1_ggx(math.cos(math.radians(82)), a)
        ly2 = PY + PH - PH * g
        # Same as fig_density: the leader carries the colour and the text is
        # plain, because a fill on a <text> is silently overridden by
        # course.css. The long faint dash stays faint and the last six units of
        # it become the swatch, so the label still reads as an annotation rather
        # than as a legend entry.
        b.append(rule(PX + PW * 82 / 90.0, ly2, PX + PW, ly2, "grid", 0.8,
                      dash="2 3"))
        b.append(f'<line x1="{PX + PW}" y1="{ly2}" x2="{PX + PW + 6}" y2="{ly2}" '
                 f'stroke="{col}" stroke-width="2.0"/>')
        b.append(f'<text x="{PX + PW + 10}" y="{ly2 + 4:.1f}" class="xs" '
                 f'text-anchor="start">{esc(f"r {r:.2f}")}</text>')

    by = 312
    b.append(box(24, by, W - 48, 96, None, rx=4))
    b.append(label(W / 2, by + 24,
                   "At 75° a near-smooth surface still shows 0.9920 of itself and a "
                   "fully rough one only 0.4112.", "sm t-hi"))
    b.append(label(W / 2, by + 46,
                   "THIS IS THE TERM LESSON 6.2 WAS MISSING when it measured a 5.36× "
                   "view-angle swing it could not explain. A rough surface really does",
                   "xs muted"))
    b.append(label(W / 2, by + 62,
                   "hide part of itself from you, and a model with no G has no way to "
                   "say so — which is why its swing looked arbitrary.", "xs muted"))
    b.append(label(W / 2, by + 84,
                   "Separable Smith assumes “lit” and “visible” are independent. They "
                   "share a height field. Measured 1.715× apart at grazing.",
                   "xs t-hi"))

    return svg(uid, W, H, "Shadowing and masking",
               "Left: a rough profile under a grazing light, with the facets in shadow "
               "drawn faint and the lit ones highlighted. Right: Smith's G1 against "
               "angle for four roughnesses, falling from 1 at the normal toward 0 at "
               "grazing incidence, and falling faster the rougher the surface.", b)


# ===========================================================================
# Figure 6 — the energy budget, and what single scattering loses  (§8)
# ===========================================================================
def fig_budget():
    uid = "l63f6"
    W, H = 720, 420
    b = [label(20, 20, "6.2’s energy test, unchanged, pointed at the new "
                       "machinery. The shape of the answer is completely different.",
                       "sm", "start")]

    X0, ROW, TOP = 150, 30, 78
    UNIT = 320.0
    one_x = X0 + UNIT

    b.append(label(X0, TOP - 18, "R(v) for D·G / (4 n·l n·v), with F = 1:",
                   "xs muted", "start"))
    b.append(rule(one_x, TOP - 8, one_x, TOP + len(BUDGET) * ROW, "hi", 1.6, dash="4 3"))
    b.append(label(one_x + 6, TOP - 18, "1.0", "xs t-hi", "start"))

    for i, (r, r0, r75) in enumerate(BUDGET):
        y = TOP + i * ROW
        b.append(label(X0 - 10, y + 14, f"roughness {r:.2f}", "xs mono muted", "end"))
        b.append(box(X0, y, r0 * UNIT, 10, BLUE, rx=2, opacity=0.55))
        b.append(box(X0, y + 12, r75 * UNIT, 10, GREEN, rx=2, opacity=0.45))
        # the lost energy, hatched to the 1.0 line
        b.append(box(X0 + r0 * UNIT, y, (1.0 - r0) * UNIT, 10, RED, rx=2, opacity=0.16))
        b.append(label(one_x + 26, y + 16, f"{r0:.4f}", "xs mono", "start"))
        b.append(label(one_x + 82, y + 16, f"loses {(1.0 - r0) * 100:.0f}%",
                       "xs mono t-bad" if r0 < 0.9 else "xs mono muted", "start"))

    ly = TOP + len(BUDGET) * ROW + 10
    for lx, col, op, txt in ((X0, BLUE, 0.55, "eye at 0°"),
                             (X0 + 118, GREEN, 0.45, "eye at 75°")):
        b.append(f'<rect x="{lx}" y="{ly}" width="26" height="9" rx="2" fill="{col}" '
                 f'fill-opacity="{op}" stroke="{col}" stroke-width="1"/>')
        b.append(label(lx + 32, ly + 8, txt, "xs", "start"))

    by = 302
    b.append(box(24, by, W - 48, 102, None, rx=4))
    b.append(label(W / 2, by + 22,
                   "It NEVER exceeds 1 — unlike Blinn-Phong, which was over at "
                   "every shininess. That is what the machinery bought.", "sm t-hi"))
    b.append(label(W / 2, by + 44,
                   "But look at the other end. A fully rough surface returns only "
                   "0.3069 of what arrives, and that is not rounding: this is SINGLE-"
                   "scattering", "xs muted"))
    b.append(label(W / 2, by + 60,
                   "microfacet theory, which lets a facet bounce light once and then "
                   "forgets it. The missing 69% hit a second facet on the way out.",
                   "xs muted"))
    b.append(label(W / 2, by + 80,
                   "Rough metal rendered this way is visibly too dark, and multiple-"
                   "scattering compensation is the standard fix — named in §9, "
                   "not built.", "xs t-hi"))

    return svg(uid, W, H, "The microfacet model's energy budget",
               "Directional-hemispherical reflectance of the specular microfacet BRDF "
               "with Fresnel set to 1, against roughness. It never exceeds 1, but falls "
               "to 0.3069 at full roughness because single-scattering theory loses the "
               "light that would have bounced a second time.", b)


# ===========================================================================
def main():
    figs = [("l63_fig1.svg", fig_landscape),
            ("l63_fig2.svg", fig_density),
            ("l63_fig3.svg", fig_constant),
            ("l63_fig4.svg", fig_tail),
            ("l63_fig5.svg", fig_masking),
            ("l63_fig6.svg", fig_budget)]
    for name, fn in figs:
        text = fn()
        with open(os.path.join(OUT, name), "w") as fh:
            fh.write(text)
        print(f"  wrote {OUT}/{name}  ({len(text):,} bytes)")


if __name__ == "__main__":
    main()
