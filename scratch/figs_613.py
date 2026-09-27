#!/usr/bin/env python3
"""scratch/figs_613.py — Lesson 6.13's diagrams.

Same rules as 6.1-6.12's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - ~5.2 units per character for `xs`, ~6.0 for `sm`
  - filenames numbered by PAGE ORDER
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - `rule()` takes a CSS CLASS; `cline()` takes a COLOUR (6.10's trap)
  - LEGENDS GO OUTSIDE THE PLOT (6.11's trap)

Every number comes from verify_613's output.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)
from figs_610 import cline                                          # noqa: E402

OUT = "scratch"

# ---- measured (verify_613) -------------------------------------------------
LIDS = [("clamp", 0.9955, 15.78), ("reinhard", 223.4789, 7.97),
        ("reinhard_white", 3.9556, 13.79), ("aces", 6.3774, 13.10)]
METAL_PEAK = 55917.0
DISPLAY_STOPS = 11.69
DISPLAY_MIN = 0.00030353

# §E — the delta, through the real chain, radially averaged
TAIL_R = [4, 8, 16, 32, 64]
TAIL_PYRAMID = [3.116390, 0.861392, 0.213734, 0.046928, 0.007804]
TAIL_GAUSS = [0.96923323, 0.88249690, 0.60653066, 0.13533528, 0.00033546]
PYRAMID_FALL = 6.0        # over the octave 32 -> 64
GAUSS_FALL = 403.0
SLOPE_16 = -2.01

# §F — area encodes luminance
AREA = [(100.0, 115, 1.150, 6.05), (1000.0, 1238, 1.238, 19.85),
        (10000.0, 11399, 1.140, 60.24)]
AREA_SPREAD = 8.6         # per cent

# §D — the pyramid at 960x540
LEVELS = [(0, 480, 270, 129600), (1, 240, 135, 32400), (2, 120, 67, 8040),
          (3, 60, 33, 1980), (4, 30, 16, 480), (5, 15, 8, 120)]
PYRAMID_FRACTION = 0.3330
PYRAMID_MB = 1.32
HDR_MB = 3.96
BLOOM_COST = 0.666        # of one full-screen pass

# §G — fireflies
FIREFLY_SHARE = 62.8
CLAMP_DRIFT = 1.19
PRE_CLAMP_DRIFT = 0.30

# §H — the order
CLIP_AFTER = 324
CLIP_BEFORE = 32

# the demo
DEMO_OVER = 69
DEMO_CHANGED = 64015
DEMO_PIXELS = 129600
CHECKS = 39


def knee(x, t=1.0, k=0.5):
    if x <= t - k:
        return 0.0
    if x >= t + k:
        return x - t
    return (x - t + k) ** 2 / (4.0 * k)


def hard(x, t=1.0):
    return max(x - t, 0.0)


# ===========================================================================
# Figure 1 — the lid did not go away, it moved                          (§1)
# ===========================================================================
def fig_lid():
    W, H = 720, 380
    b = [label(W / 2, 22, "Where 6.12 put the clipping point, and what is still above it", "sm")]

    x0, x1 = 130, 640
    y_axis = 262

    def px(v):
        lo, hi = math.log10(0.5), math.log10(1.0e5)
        return x0 + (x1 - x0) * (math.log10(v) - lo) / (hi - lo)

    b.append(rule(x0, y_axis, x1, y_axis, "grid", 1.0))
    for v in (1, 10, 100, 1000, 10000, 100000):
        b.append(rule(px(v), y_axis - 4, px(v), y_axis + 5, "grid", 1.0))
        b.append(label(px(v), y_axis + 20, f"{v:,}", "xs"))
    b.append(label((x0 + x1) / 2, y_axis + 42,
                   "linear value arriving at the resolve", "xs"))

    # THE OPERATOR ROWS FIRST, in clear space. The shaded band went here in the
    # first draft and visually ENCLOSED the reinhard row — which is factually
    # true (223.5 is above ACES's lid) but reads as if the band were about
    # reinhard. Separate bands from rows.
    y = 66
    for name, lid, stops in LIDS:
        colour = GREEN if lid > 100 else AMBER
        b.append(cline(px(lid), y - 9, px(lid), y + 9, colour, 2.0))
        b.append(label(px(lid) - 9, y + 4, f"{name}  {lid:,.4f}", "xs", "end"))
        b.append(label(px(lid) + 9, y + 4, f"{stops:.2f} stops above", "xs", "start"))
        y += 26

    # THE BAND, in its own strip between the rows and the axis.
    band_y = 176
    band_h = 74
    b.append(box(px(LIDS[3][1]), band_y, x1 - px(LIDS[3][1]), band_h, RED, rx=2, opacity=0.12))
    b.append(cline(px(LIDS[3][1]), band_y, px(LIDS[3][1]), y_axis, RED, 1.8))
    b.append(label((px(LIDS[3][1]) + x1) / 2, band_y + 22,
                   f"above ACES's lid: {LIDS[3][2]:.2f} STOPS,", "xs"))
    b.append(label((px(LIDS[3][1]) + x1) / 2, band_y + 38, "all of it code 255", "xs"))

    b.append(f'<circle cx="{px(METAL_PEAK):.1f}" cy="{band_y + 58}" r="4.5" fill="{RED}"/>')
    b.append(label(px(METAL_PEAK) - 12, band_y + 62,
                   f"the polished metal of 6.12   {METAL_PEAK:,.0f}", "xs", "end"))

    b.append(label(W / 2, H - 44,
                   "Reinhard has the HIGHEST lid and looks the worst, which is the "
                   "whole paradox in one line: it spends its codes", "xs"))
    b.append(label(W / 2, H - 26,
                   f"reaching a white it never gets to. And the display itself holds "
                   f"only {DISPLAY_STOPS} stops (code 1 = {DISPLAY_MIN:.8f}),", "xs"))
    b.append(label(W / 2, H - 8,
                   "so no curve can fix this. The constraint is not the curve.", "xs"))
    return svg("f613a", W, H, "Where each tonemap operator starts clipping",
               "A logarithmic axis of linear value with the saturation point of four "
               "tonemap operators marked, and the engine's measured specular peak far "
               "beyond all of them.", b)


# ===========================================================================
# Figure 2 — the tail, measured                                       (§2, §3.3)
# ===========================================================================
def fig_tail():
    W, H = 720, 420
    b = [label(W / 2, 22, "What a point of light becomes: the pyramid's tail against one Gaussian", "sm")]

    # log-log
    x0, x1 = 108, 516
    y0, y1 = 60, 300

    def px(r):
        lo, hi = math.log10(3.0), math.log10(90.0)
        return x0 + (x1 - x0) * (math.log10(r) - lo) / (hi - lo)

    def py(v):
        lo, hi = math.log10(1.0e-4), math.log10(10.0)
        t = (math.log10(max(v, 1.0e-5)) - lo) / (hi - lo)
        return y1 - (y1 - y0) * t

    b.append(hollow(x0, y0, x1 - x0, y1 - y0, GREY, dash="3 3"))
    for r in TAIL_R:
        b.append(rule(px(r), y1, px(r), y1 + 5, "grid", 1.0))
        b.append(label(px(r), y1 + 20, str(r), "xs"))
    b.append(label((x0 + x1) / 2, y1 + 42, "radius from the source, in texels", "xs"))
    for v, txt in ((10.0, "10"), (1.0, "1"), (0.1, "0.1"), (0.01, "0.01"),
                   (0.001, "0.001"), (0.0001, "0.0001")):
        b.append(rule(x0 - 5, py(v), x0, py(v), "grid", 1.0))
        b.append(label(x0 - 10, py(v) + 4, txt, "xs", "end"))

    pts_p = " ".join(f"{px(r):.1f},{py(v):.1f}" for r, v in zip(TAIL_R, TAIL_PYRAMID))
    pts_g = " ".join(f"{px(r):.1f},{py(v):.1f}" for r, v in zip(TAIL_R, TAIL_GAUSS))
    b.append(f'<polyline points="{pts_g}" fill="none" stroke="{GREY}" stroke-width="2.0" '
             f'stroke-dasharray="5 3"/>')
    b.append(f'<polyline points="{pts_p}" fill="none" stroke="{AMBER}" stroke-width="2.4"/>')
    for r, v in zip(TAIL_R, TAIL_PYRAMID):
        b.append(f'<circle cx="{px(r):.1f}" cy="{py(v):.1f}" r="3.2" fill="{AMBER}"/>')
    for r, v in zip(TAIL_R, TAIL_GAUSS):
        b.append(f'<circle cx="{px(r):.1f}" cy="{py(v):.1f}" r="2.6" fill="{GREY}"/>')

    # LEGEND OUTSIDE THE PLOT — 6.11's trap.
    lx = x1 + 24
    b.append(cline(lx, y0 + 14, lx + 22, y0 + 14, AMBER, 2.4))
    b.append(label(lx + 28, y0 + 18, "the pyramid", "xs", "start"))
    b.append(label(lx + 28, y0 + 34, f"slope {SLOPE_16:+.2f} at r = 16", "xs", "start"))
    b.append(label(lx + 28, y0 + 48, "— an inverse-square tail", "xs", "start"))

    b.append(cline(lx, y0 + 82, lx + 22, y0 + 82, GREY, 2.0, ))
    b.append(label(lx + 28, y0 + 86, "one Gaussian, sigma 16", "xs", "start"))
    b.append(label(lx + 28, y0 + 102, "exp(-r^2) has no tails:", "xs", "start"))
    b.append(label(lx + 28, y0 + 116, "it falls off a cliff", "xs", "start"))

    b.append(hollow(lx - 6, y0 + 140, 168, 60, AMBER, rx=4))
    b.append(label(lx + 78, y0 + 160, "over one octave, 32 to 64:", "xs"))
    b.append(label(lx + 78, y0 + 178, f"pyramid falls {PYRAMID_FALL:.1f}x", "xs"))
    b.append(label(lx + 78, y0 + 192, f"Gaussian falls {GAUSS_FALL:.0f}x", "xs"))

    b.append(label(W / 2, H - 60,
                   "Both curves are the SAME delta through the SAME measurement. The "
                   "octave ratio is normalisation-independent, so no", "xs"))
    b.append(label(W / 2, H - 42,
                   "choice of scale can move it. Measured glare in a real eye falls "
                   "roughly as 1/r^2, which is what a SUM of Gaussians", "xs"))
    b.append(label(W / 2, H - 24,
                   "whose widths double produces and what no single Gaussian can. The "
                   "pyramid is cheaper AND closer to the physics —", "xs"))
    b.append(label(W / 2, H - 6, "which is a coincidence worth noticing when it happens.", "xs"))
    return svg("f613b", W, H, "The pyramid's kernel against a single Gaussian",
               "A log-log plot of radially averaged intensity against radius for a "
               "single bright pixel blurred by the pyramid, compared with one "
               "Gaussian of sigma 16.", b)


# ===========================================================================
# Figure 3 — the chain                                                  (§3.3)
# ===========================================================================
def fig_chain():
    W, H = 740, 440
    b = [label(W / 2, 22, "The chain: down with a box, up with a tent, and ADD on the way back", "sm")]

    # THE LEVELS ARE DRAWN AT A FLOOR, not at true relative size. Level 5 is
    # 15x8, which at any scale that fits level 0 on the page is three pixels —
    # so true proportion would make the last two levels invisible and the
    # diagram would stop being about the last two levels. The SIZES ARE PRINTED
    # instead, which is the honest way to show a range this wide.
    base_y = 176
    x = 168          # room for the `bright` label on its own arrow
    gap = 30
    slots = []
    for i, (lv, w, h, _t) in enumerate(LEVELS):
        bw = max(88.0 * (0.5 ** (i * 0.62)), 26.0)
        bh = max(bw * 9.0 / 16.0, 16.0)
        slots.append((x, base_y - bh, bw, bh, lv, w, h))
        b.append(hollow(x, base_y - bh, bw, bh, AMBER if i == 0 else BLUE, rx=2))
        b.append(label(x + bw / 2, base_y + 18, f"{w}x{h}", "xs"))
        b.append(label(x + bw / 2, base_y + 33, f"lv {lv}", "xs"))
        x += bw + gap

    # The scene target, left, and visibly the biggest thing on the row.
    sw, sh = 96, 54
    b.append(hollow(18, base_y - sh, sw, sh, RED, rx=2))
    b.append(label(18 + sw / 2, base_y + 18, "960x540", "xs"))
    b.append(label(18 + sw / 2, base_y + 33, "the scene", "xs"))

    # DOWN — the bright pass, then the halvings. Arrows sit on the boxes' row.
    b.append(arrow(18 + sw + 3, base_y - 20, slots[0][0] - 3, base_y - 20, "f613c", "i", 1.5))
    b.append(label((18 + sw + slots[0][0]) / 2, base_y - 30, "bright", "xs"))
    for i in range(len(slots) - 1):
        x_from = slots[i][0] + slots[i][2]
        x_to = slots[i + 1][0]
        b.append(arrow(x_from + 3, base_y - 12, x_to - 3, base_y - 12, "f613c", "s", 1.2))
    b.append(label(W / 2, base_y - 84,
                   "DOWN   one bilinear tap per texel IS the 2x2 box average — free, "
                   "in the texture unit", "xs"))

    # UP — from the smallest back to level 0, each added into the one below.
    up_y = base_y + 62
    for i in range(len(slots) - 1, 0, -1):
        x_from = slots[i][0] + slots[i][2] / 2
        x_to = slots[i - 1][0] + slots[i - 1][2] / 2
        b.append(arrow(x_from - 4, up_y, x_to + 4, up_y, "f613c", "g", 1.5))
        b.append(rule(x_from, base_y + 40, x_from, up_y - 6, "grid", 0.9, dash="2 3"))
    b.append(rule(slots[0][0] + slots[0][2] / 2, base_y + 40,
                  slots[0][0] + slots[0][2] / 2, up_y - 6, "grid", 0.9, dash="2 3"))
    b.append(label(W / 2, up_y + 24,
                   "UP   a 1-2-1 tent, ADDED into the level below by the hardware "
                   "(src*1 + dst*1)", "xs"))
    b.append(label(W / 2, up_y + 40,
                   "so level 0 holds the SUM of every level — Gaussians whose widths double", "xs"))

    # The arithmetic, in two boxes below everything.
    by = 328
    b.append(hollow(40, by, 300, 76, GREY, rx=4))
    b.append(label(190, by + 20, "MEMORY", "xs"))
    b.append(label(190, by + 37,
                   f"{PYRAMID_FRACTION:.4f} of one full target = {PYRAMID_MB} MB", "xs"))
    b.append(label(190, by + 52, f"against the HDR target's {HDR_MB} MB", "xs"))
    b.append(label(190, by + 67, "1/4 + 1/16 + ... = 1/3, the mip number again", "xs"))

    b.append(hollow(376, by, 324, 76, GREY, rx=4))
    b.append(label(538, by + 20, "COST", "xs"))
    b.append(label(538, by + 37,
                   f"the WHOLE bloom is {BLOOM_COST:.3f} of ONE full-screen pass", "xs"))
    b.append(label(538, by + 52, "one sigma-64 separable Gaussian would be 385 taps", "xs"))
    b.append(label(538, by + 67, "per axis — 399 million texel fetches for one frame", "xs"))

    b.append(label(W / 2, H - 8,
                   "Eleven render passes for six levels (2n-1): a pass writes ONE "
                   "target, and none may read what it writes.", "xs"))
    return svg("f613c", W, H, "The bloom pyramid, down and back up",
               "The six pyramid levels drawn left to right at decreasing size, with the "
               "downsample chain running right along the top and the additive upsample "
               "chain running left below them, and the memory and cost arithmetic.", b)


# ===========================================================================
# Figure 4 — the soft knee                                              (§3.6)
# ===========================================================================
def fig_knee():
    W, H = 700, 400
    b = [label(W / 2, 22, "The bright pass: a hard threshold, and the quadratic that replaces its corner", "sm")]

    x0, x1 = 110, 500
    y0, y1 = 56, 270

    def px(v):
        return x0 + (x1 - x0) * (v - 0.0) / 2.2

    def py(v):
        return y1 - (y1 - y0) * (v - 0.0) / 1.3

    b.append(hollow(x0, y0, x1 - x0, y1 - y0, GREY, dash="3 3"))
    for v in (0.0, 0.5, 1.0, 1.5, 2.0):
        b.append(rule(px(v), y1, px(v), y1 + 5, "grid", 1.0))
        b.append(label(px(v), y1 + 20, f"{v:.1f}", "xs"))
    b.append(label((x0 + x1) / 2, y1 + 42, "incoming luminance", "xs"))
    for v in (0.0, 0.5, 1.0):
        b.append(rule(x0 - 5, py(v), x0, py(v), "grid", 1.0))
        b.append(label(x0 - 10, py(v) + 4, f"{v:.1f}", "xs", "end"))

    # T-k and T+k.
    for v, txt in ((0.5, "T-k"), (1.0, "T"), (1.5, "T+k")):
        b.append(rule(px(v), y0, px(v), y1, "grid", 1.0, dash="2 4"))
        b.append(label(px(v), y0 - 8, txt, "xs"))

    n = 160
    hp = " ".join(f"{px(0.0 + 2.2 * i / n):.1f},{py(hard(0.0 + 2.2 * i / n)):.1f}"
                  for i in range(n + 1))
    kp = " ".join(f"{px(0.0 + 2.2 * i / n):.1f},{py(knee(0.0 + 2.2 * i / n)):.1f}"
                  for i in range(n + 1))
    b.append(f'<polyline points="{hp}" fill="none" stroke="{RED}" stroke-width="2.0" '
             f'stroke-dasharray="5 3"/>')
    b.append(f'<polyline points="{kp}" fill="none" stroke="{GREEN}" stroke-width="2.4"/>')

    # The three joins.
    for v in (0.5, 1.0, 1.5):
        b.append(f'<circle cx="{px(v):.1f}" cy="{py(knee(v)):.1f}" r="3.4" fill="{GREEN}"/>')
    # NOT annotated in place: "beside the point" on a plot whose curve passes
    # through that point is exactly 6.11's trap. The value goes in the legend
    # column, with a leader line to the marker it describes.

    # LEGEND OUTSIDE.
    lx = x1 + 26
    b.append(cline(lx, y0 + 14, lx + 22, y0 + 14, RED, 2.0))
    b.append(label(lx + 28, y0 + 18, "hard: max(x - T, 0)", "xs", "start"))
    b.append(label(lx + 28, y0 + 34, "f' jumps 0 -> 1 at T", "xs", "start"))
    b.append(cline(lx, y0 + 62, lx + 22, y0 + 62, GREEN, 2.4))
    b.append(label(lx + 28, y0 + 66, "soft: (x-T+k)^2 / 4k", "xs", "start"))
    b.append(label(lx + 28, y0 + 82, "f'(T-k) = 0, f'(T+k) = 1", "xs", "start"))

    b.append(hollow(lx - 6, y0 + 100, 172, 40, GREEN, rx=4))
    b.append(label(lx + 80, y0 + 118, "at x = T exactly:", "xs"))
    b.append(label(lx + 80, y0 + 132, "f = k/4 = 0.125, f' = 0.5", "xs"))
    b.append(rule(px(1.0) + 4, py(knee(1.0)), lx - 10, y0 + 118, "grid", 0.9, dash="2 3"))

    b.append(hollow(lx - 6, y0 + 156, 172, 62, GREY, rx=4))
    b.append(label(lx + 80, y0 + 174, "THE ARTEFACT", "xs"))
    b.append(label(lx + 80, y0 + 190, "0.999 contributes 0.000000", "xs"))
    b.append(label(lx + 80, y0 + 204, "1.001 contributes 0.001000", "xs"))

    b.append(label(W / 2, H - 62,
                   "The quadratic is not chosen, it is the only one that fits: three "
                   "conditions (value 0 and slope 0 at T-k, slope 1 at T+k)", "xs"))
    b.append(label(W / 2, H - 44,
                   "and three coefficients. Matching the SLOPE is what matters — a "
                   "curve that is merely continuous still has a kink, and a", "xs"))
    b.append(label(W / 2, H - 26,
                   "kink in the bright pass is a contour in the image that crawls as "
                   "the camera moves. The symptom is a shimmering edge", "xs"))
    b.append(label(W / 2, H - 8, "along every gradient that happens to pass through the threshold.", "xs"))
    return svg("f613d", W, H, "The soft knee against a hard threshold",
               "A plot of surviving luminance against incoming luminance for a hard "
               "threshold and for the quadratic soft knee, with the two joins marked.", b)


# ===========================================================================
# Figure 5 — the two filter identities                                  (§3.4-3.5)
# ===========================================================================
def fig_identities():
    W, H = 720, 360
    b = [label(W / 2, 22, "Two identities, and the hardware that makes them free", "sm")]

    # (a) the bilinear tap at the block corner
    ax, ay, cell = 70, 62, 52
    vals = [[1, 2], [4, 8]]
    b.append(label(ax + cell, ay - 14, "ONE TAP = FOUR TEXELS", "xs"))
    for j in range(2):
        for i in range(2):
            b.append(hollow(ax + i * cell, ay + j * cell, cell, cell, BLUE, rx=2))
            b.append(label(ax + i * cell + cell / 2, ay + j * cell + cell / 2 + 5,
                           str(vals[j][i]), "sm"))
    b.append(f'<circle cx="{ax + cell}" cy="{ay + cell}" r="5" fill="{AMBER}"/>')
    b.append(label(ax + cell, ay + 2 * cell + 20,
                   "sample at the shared corner", "xs"))
    b.append(label(ax + cell, ay + 2 * cell + 36,
                   "all four weights are 1/2 x 1/2", "xs"))
    b.append(label(ax + cell, ay + 2 * cell + 54, "= 3.75, the mean", "xs"))
    b.append(label(ax + cell, ay + 2 * cell + 72, "(1+2+4+8)/4 = 3.75", "xs"))

    # (b) box * box = tent
    bx = 300
    b.append(label(bx + 150, ay - 14, "BOX CONVOLVED WITH BOX = TENT", "xs"))
    bar_w = 26
    for i, v in enumerate((1, 1)):
        b.append(box(bx + i * bar_w, ay + 40, bar_w - 3, v * 30, BLUE, rx=1, opacity=0.35))
    b.append(label(bx + bar_w, ay + 88, "[1 1]", "xs"))
    b.append(label(bx + 59, ay + 58, "*", "sm"))
    for i, v in enumerate((1, 1)):
        b.append(box(bx + 66 + i * bar_w, ay + 40, bar_w - 3, v * 30, BLUE, rx=1, opacity=0.35))
    b.append(label(bx + 66 + bar_w, ay + 88, "[1 1]", "xs"))
    b.append(label(bx + 140, ay + 58, "=", "sm"))
    for i, v in enumerate((1, 2, 1)):
        b.append(box(bx + 160 + i * bar_w, ay + 70 - v * 15, bar_w - 3, v * 15,
                     GREEN, rx=1, opacity=0.40))
    b.append(label(bx + 160 + 1.5 * bar_w, ay + 88, "[1 2 1]", "xs"))
    b.append(label(bx + 150, ay + 112,
                   "so a BILINEAR UPSAMPLE already IS a tent filter —", "xs"))
    b.append(label(bx + 150, ay + 128,
                   "the 3x3 kernel is that vector's outer product,", "xs"))
    b.append(label(bx + 150, ay + 144, "summing to 16/16 = 1 exactly", "xs"))

    # The 3x3 kernel itself.
    kx, ky, kc = bx + 108, ay + 156, 26
    ker = [[1, 2, 1], [2, 4, 2], [1, 2, 1]]
    for j in range(3):
        for i in range(3):
            b.append(hollow(kx + i * kc, ky + j * kc, kc, kc, GREEN, rx=2))
            b.append(label(kx + i * kc + kc / 2, ky + j * kc + kc / 2 + 4,
                           str(ker[j][i]), "xs"))
    b.append(label(kx + 3 * kc + 46, ky + 1.5 * kc + 4, "/ 16", "xs"))

    b.append(label(W / 2, H - 44,
                   "Neither identity is a trick for its own sake. The first is why the "
                   "downsample shader is one line and why its sampler is", "xs"))
    b.append(label(W / 2, H - 26,
                   "LINEAR where the 6.12 resolve's is NEAREST — here the filter mode "
                   "is doing arithmetic rather than smoothing. The second", "xs"))
    b.append(label(W / 2, H - 8,
                   "is why the upsample's weights are 1-2-1 and not something that "
                   "merely looks symmetric.", "xs"))
    return svg("f613e", W, H, "The bilinear-tap and box-convolution identities",
               "Left: a two by two block of texels with a bilinear sample at the shared "
               "corner returning their mean. Right: two unit boxes convolved into a "
               "one-two-one tent, and the resulting three by three kernel.", b)


# ===========================================================================
# Figure 6 — the payoff                                                 (§4)
# ===========================================================================
def fig_area():
    W, H = 720, 400
    b = [label(W / 2, 22, "The payoff: a display that cannot show the value shows the AREA", "sm")]

    # Three glows, radii to scale.
    cy = 140
    xs = [150, 340, 560]
    max_r = max(a[3] for a in AREA)
    for (L, area, per_l, r), cx in zip(AREA, xs):
        rr = 52.0 * r / max_r
        for k, alpha in ((1.0, 0.30), (0.62, 0.45), (0.34, 0.70)):
            b.append(f'<circle cx="{cx}" cy="{cy}" r="{rr * k:.1f}" fill="{AMBER}" '
                     f'fill-opacity="{alpha}"/>')
        b.append(f'<circle cx="{cx}" cy="{cy}" r="2.4" fill="{RED}"/>')
        b.append(label(cx, cy + 74, f"L = {L:,.0f}", "xs"))
        b.append(label(cx, cy + 90, f"area {area:,} texels", "xs"))
        b.append(label(cx, cy + 106, f"radius {r:.2f}", "xs"))
        b.append(label(cx, cy + 122, f"area / L = {per_l:.3f}", "xs"))

    # And the same three under a clamp.
    cy2 = 292
    b.append(label(70, cy2 + 4, "under a clamp:", "xs", "start"))
    for cx in xs:
        b.append(f'<circle cx="{cx}" cy="{cy2}" r="3.0" fill="{RED}"/>')
        b.append(label(cx, cy2 + 20, "code 255", "xs"))
    b.append(label(W / 2, cy2 + 40,
                   "three identical white dots — the information was in the HDR "
                   "buffer and there was no way to show it", "xs"))

    b.append(label(W / 2, H - 44,
                   f"THE PREDICTION AND ITS TEST. If the tail goes as r^-2, the radius "
                   f"at which the glow crosses any fixed visibility", "xs"))
    b.append(label(W / 2, H - 26,
                   f"threshold goes as sqrt(L), so the AREA goes as L. Measured over "
                   f"three decades: area/L constant to within {AREA_SPREAD}%.", "xs"))
    b.append(label(W / 2, H - 8,
                   "The conversion from an intensity the display cannot represent into "
                   "an area it can is very nearly exact.", "xs"))
    return svg("f613f", W, H, "Glow area against source luminance",
               "Three glows from single pixels of increasing luminance, drawn with "
               "radii to scale, above the same three values rendered through a clamp as "
               "identical single dots.", b)


# ===========================================================================
# Figure 7 — the order                                                  (§5)
# ===========================================================================
def fig_order():
    W, H = 720, 340
    b = [label(W / 2, 22, "Why the composite happens before the curve, and not after", "sm")]

    def stage(x, y, w, h, title, sub, colour):
        out = [hollow(x, y, w, h, colour, rx=4)]
        out.append(label(x + w / 2, y + 24, title, "xs"))
        out.append(label(x + w / 2, y + 42, sub, "xs"))
        return out

    # The right order.
    y = 62
    row_x = (W - (4 * 132 + 3 * 14)) / 2
    b.append(label(row_x, y - 14, "CORRECT", "xs", "start"))
    xs = row_x
    for title, sub, colour in (("scene x exposure", "linear light", BLUE),
                               ("+ intensity x bloom", "still linear", GREEN),
                               ("curve", "-> [0, 1]", AMBER),
                               ("encode", "-> 8 bits", PURPLE)):
        b.extend(stage(xs, y, 132, 56, title, sub, colour))
        if xs > row_x:
            b.append(arrow(xs - 14, y + 28, xs - 3, y + 28, "f613g", "s", 1.2))
        xs += 146
    b.append(label(W / 2, y + 74,
                   f"the bloom is light, so it is photographed by the same curve as the "
                   f"light that did not scatter — {CLIP_BEFORE} pixels clip", "xs"))

    # The wrong order.
    y2 = 190
    b.append(label(row_x, y2 - 14, "WRONG", "xs", "start"))
    xs = row_x
    for title, sub, colour in (("scene x exposure", "linear light", BLUE),
                               ("curve", "-> [0, 1]", AMBER),
                               ("+ intensity x bloom", "ABOVE 1", RED),
                               ("encode", "clips", RED)):
        b.extend(stage(xs, y2, 132, 56, title, sub, colour))
        if xs > row_x:
            b.append(arrow(xs - 14, y2 + 28, xs - 3, y2 + 28, "f613g", "s", 1.2))
        xs += 146
    b.append(label(W / 2, y2 + 74,
                   f"the curve's output is already in [0, 1], so anything added has "
                   f"nowhere to go — {CLIP_AFTER} pixels clip, ten times as many", "xs"))

    b.append(label(W / 2, H - 44,
                   "Every glow grows a flat white core, and the brighter the source the "
                   "bigger that core: which is exactly the artefact", "xs"))
    b.append(label(W / 2, H - 26,
                   "bloom was introduced to remove, reintroduced one pass later. The "
                   "ordering is not a policy — it is where the light", "xs"))
    b.append(label(W / 2, H - 8, "actually scatters, which is before the sensor responds to it.", "xs"))
    return svg("f613g", W, H, "Compositing the bloom before and after the curve",
               "Two pipelines of four stages each, the first compositing the bloom in "
               "linear light before the tonemap curve and the second after it, with the "
               "measured count of clipped pixels for each.", b)


def main():
    figs = [("l613_fig1.svg", fig_lid()),
            ("l613_fig2.svg", fig_tail()),
            ("l613_fig3.svg", fig_chain()),
            ("l613_fig4.svg", fig_knee()),
            ("l613_fig5.svg", fig_identities()),
            ("l613_fig6.svg", fig_area()),
            ("l613_fig7.svg", fig_order())]
    for name, body in figs:
        path = os.path.join(OUT, name)
        with open(path, "w") as fh:
            fh.write(body)
        print(f"wrote {path}  ({len(body):,} bytes)")


if __name__ == "__main__":
    main()
