#!/usr/bin/env python3
"""scratch/figs_615.py — Lesson 6.15's diagrams.

Same rules as 6.1-6.14's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - ~5.2 units per character for `xs`, ~6.0 for `sm`
  - filenames numbered by PAGE ORDER
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - `rule()` takes a CSS CLASS; `cline()` takes a COLOUR (6.10's trap)
  - LEGENDS AND ANNOTATION BOXES GO OUTSIDE THE PLOT (6.11's trap, 6.13's again)
  - a SHAPE can leave the viewBox where a label cannot; check-page.js tests
    closed shapes too since 6.14, so both are checked

Every number comes from verify_615's output.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)
from figs_610 import cline                                          # noqa: E402

OUT = "scratch"

# ---- measured (verify_615) -------------------------------------------------
CORNER_RATIO = 0.19245           # 1/(3*sqrt(3)), the asymptote
CORNER_AT_128 = 0.195535
CENTRE_WORTH = 5.196
FLAT_BIAS = 1.010066             # equal weights, 64x64 — a BIAS, not an error
FLAT_BIAS_16 = 1.010005

IRRADIANCE = [(8, 3.14378262, 6.971e-4), (16, 3.14212966, 1.709e-4),
              (32, 3.14172959, 4.359e-5), (64, 3.14162707, 1.096e-5)]

# roughness, alpha, half-angle (deg), lobe (sr), derived level, linear level
LEVELS = [(0.00, 0.0010, 0.04, 0.000001, 0.000, 0.000),
          (0.10, 0.0100, 0.37, 0.000130, 0.000, 0.700),
          (0.20, 0.0400, 1.48, 0.002086, 1.548, 1.400),
          (0.30, 0.0900, 3.33, 0.010636, 2.723, 2.100),
          (0.40, 0.1600, 5.99, 0.034282, 3.567, 2.800),
          (0.50, 0.2500, 9.57, 0.087360, 4.242, 3.500),
          (0.60, 0.3600, 14.38, 0.196842, 4.828, 4.200),
          (0.70, 0.4900, 21.21, 0.425571, 5.384, 4.900),
          (0.80, 0.6400, 32.42, 0.979074, 5.985, 5.600),
          (0.90, 0.8100, 62.74, 3.405552, 6.884, 6.300),
          (1.00, 1.0000, 90.00, 6.283185, 7.000, 7.000)]
WORST_LEVEL_GAP = 0.770
WORST_LEVEL_AT = 0.45
SQRT_GAP = 2.214

# roughness, n.v, truth, split-sum, ratio   (smooth sky, 1e6 samples)
SPLIT = [(0.10, 0.9, 0.019015, 0.019054, 1.0020),
         (0.10, 0.4, 0.068871, 0.068817, 0.9992),
         (0.25, 0.9, 0.019177, 0.019118, 0.9969),
         (0.25, 0.4, 0.067442, 0.066044, 0.9793),
         (0.50, 0.9, 0.018627, 0.018099, 0.9717),
         (0.50, 0.4, 0.042710, 0.037936, 0.8882),
         (0.75, 0.9, 0.013536, 0.012746, 0.9417),
         (0.75, 0.4, 0.023234, 0.020140, 0.8669),
         (1.00, 0.9, 0.007219, 0.006896, 0.9554),
         (1.00, 0.4, 0.013964, 0.009915, 0.7101)]
SPLIT_NORMAL_WORST = 5.8         # per cent dark, n.v = 0.9
SPLIT_GRAZING_WORST = 29.0       # per cent dark, n.v = 0.4
SPLIT_SUN_WORST = 73.0           # per cent dark, with a sun disc
SPLIT_LOW_ROUGH = 0.08           # per cent, roughness 0.10

NOISE_FACTOR = 1.98              # 8,192 samples against 1,000,000
NOISE_AT = (0.75, 0.6)
NOISE_SMOOTH = 0.040             # per cent, same test with the sun removed

# roughness, n.v, scale+bias, lost %
ENERGY = [(0.10, 0.9, 1.000012, 0.0), (0.10, 0.4, 0.999852, 0.0),
          (0.50, 0.9, 0.907461, 9.3), (0.50, 0.4, 0.844594, 15.5),
          (1.00, 0.9, 0.327579, 67.2), (1.00, 0.4, 0.498982, 50.1)]
ENERGY_63 = 0.3069               # Lesson 6.3's probe, from the other direction

SEAM_AZIMUTH = 12.09             # per cent, 16x16, azimuthally varying probe
SEAM_SKY = 1.06e-7               # the same probe on THIS sky

HALF_WORST = 0.000485            # 2^-11
HALF_MAX = 65504

BAKE_MS = (4282.0, 501.2, 372.7)   # irradiance 32^2, prefilter 6 levels, lut 64^2
BAKE_MB = (0.070, 1.500, 0.016)
SOURCE_MB = 1.125
CHAIN_RATIO = 131040 / 98304        # the famous 4/3

LUT_SAMPLES = [(16, 2.10e-2), (64, 1.21e-2), (256, 2.32e-3),
               (1024, 8.39e-4), (4096, 3.72e-4)]
LUT_QUANT = 0.01118               # absolute, 8-bit table vs the integral

CHECKS = 54


# ===========================================================================
# Figure 1 — what the ambient term cannot do                             (§1)
# ===========================================================================
def fig_problem():
    W, H = 720, 402
    b = [label(W / 2, 22, "Two things a constant ambient cannot do, and only one of "
                          "them was ever written down", "sm")]

    # ---- LEFT: no direction ------------------------------------------------
    cx, cy, r = 178, 140, 44
    b.append(label(cx, 48, "IT HAS NO DIRECTION", "xs"))

    # arrows arriving equally from every direction
    for k in range(12):
        a = math.pi * k / 11.0
        x0 = cx + math.cos(a) * (r + 38)
        y0 = cy - math.sin(a) * (r + 38)
        x1 = cx + math.cos(a) * (r + 8)
        y1 = cy - math.sin(a) * (r + 8)
        b.append(arrow(x0, y0, x1, y1, "f615a", "s", 1.0))

    b.append(f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="{BLUE}" fill-opacity="0.30"'
             f' stroke="{BLUE}" stroke-width="1.4"/>')
    b.append(rule(cx - r - 56, cy, cx + r + 56, cy, "grid", 1.0, "4 3"))
    b.append(label(cx, cy + 5, "same fill", "xs"))
    b.append(label(cx, cy + 20, "everywhere", "xs"))
    b.append(label(cx, cy + r + 66, "A face turned to the sky and one turned", "xs"))
    b.append(label(cx, cy + r + 82, "to the floor receive identical light, so", "xs"))
    b.append(label(cx, cy + r + 98, "nothing in the scene is ever grounded.", "xs"))

    # ---- RIGHT: no specular half -------------------------------------------
    mx, my = 534, 140
    b.append(label(mx, 48, "IT HAS NO SPECULAR HALF AT ALL", "xs"))

    # a chrome sphere, black except for one highlight
    b.append(f'<circle cx="{mx}" cy="{my}" r="46" fill="#101216"'
             f' stroke="{GREY}" stroke-width="1.2"/>')
    b.append(f'<circle cx="{mx - 16}" cy="{my - 18}" r="9" fill="{AMBER}"'
             f' fill-opacity="0.95" stroke="none"/>')
    b.append(arrow(mx - 96, my - 62, mx - 26, my - 26, "f615a", "h", 1.3))
    b.append(label(mx - 104, my - 68, "the one key light", "xs", "end"))
    b.append(label(mx, my + 66, "`base * ambient` goes through NO BRDF, and a metal", "xs"))
    b.append(label(mx, my + 82, "has no `base` to multiply — so chrome in a bright", "xs"))
    b.append(label(mx, my + 98, "room is black except where one lamp reaches it.", "xs"))

    # ---- the sentence on record --------------------------------------------
    b.append(hollow(56, 296, 608, 92, AMBER, rx=4))
    b.append(label(W / 2, 318, "ON RECORD SINCE LESSON 6.4, in a comment shipped in "
                               "both light.hpp and scene.frag.hlsl:", "xs"))
    b.append(label(W / 2, 340, "“a mirror in a bright uniform room renders black "
                               "except where the key light reaches it — that", "xs"))
    b.append(label(W / 2, 376, "missing term is Lesson 6.15’s image-based "
                               "lighting.” This is the lesson that closes it.", "xs"))

    return svg("f615a", W, H, "The two failures of a constant ambient term",
               "On the left, uniform light arriving from every direction so no "
               "surface orientation is distinguished. On the right, a chrome "
               "sphere rendered black except for a single specular highlight from "
               "the one directional light. Below, the comment shipped in the "
               "engine since Lesson 6.4 naming this lesson as the fix.", b)


# ===========================================================================
# Figure 2 — the cube, its faces, and the texel that is worth 5.2 others  (§3)
# ===========================================================================
def fig_cube():
    W, H = 720, 420
    b = [label(W / 2, 22, "A direction picks a face by its largest component — "
                          "and the texels are not the same size", "sm")]

    # ---- LEFT: the unfolded cross ------------------------------------------
    cell = 54
    ox, oy = 58, 62
    faces = [("+Y", 1, 0, GREEN), ("-X", 0, 1, RED), ("+Z", 1, 1, BLUE),
             ("+X", 2, 1, RED), ("-Z", 3, 1, BLUE), ("-Y", 1, 2, GREEN)]
    for name, gx, gy, colour in faces:
        x, y = ox + gx * cell, oy + gy * cell
        b.append(box(x, y, cell - 2, cell - 2, colour, rx=2, opacity=0.22))
        b.append(label(x + cell / 2 - 1, y + cell / 2 + 4, name, "xs"))
    b.append(label(ox + 2 * cell, oy + 3 * cell + 22,
                   "the six faces, in SDL’s order:", "xs"))
    b.append(label(ox + 2 * cell, oy + 3 * cell + 38,
                   "+X -X +Y -Y +Z -Z — index 0..5,", "xs"))
    b.append(label(ox + 2 * cell, oy + 3 * cell + 54,
                   "handed straight to `layer`", "xs"))

    # ---- RIGHT: the solid-angle argument -----------------------------------
    px, py = 470, 78
    side = 150
    b.append(label(px + side / 2, py - 22, "WHY A CENTRE TEXEL IS BIGGER", "xs"))

    # the face, seen edge-on, with the origin below it
    b.append(rule(px, py, px + side, py, "ink", 1.6))
    ox2, oy2 = px + side / 2, py + 118
    b.append(f'<circle cx="{ox2}" cy="{oy2}" r="3.5" class="fill-ink"/>')
    b.append(label(ox2, oy2 + 18, "origin", "xs"))

    # rays to the centre and to a corner
    b.append(cline(ox2, oy2, px + side / 2, py, GREEN, 1.6))
    b.append(cline(ox2, oy2, px + side, py, RED, 1.6))
    b.append(cline(ox2, oy2, px, py, RED, 1.6))

    # the texel widths at each end
    b.append(cline(px + side / 2 - 11, py, px + side / 2 + 11, py, GREEN, 4.0))
    b.append(cline(px + side - 22, py, px + side, py, RED, 4.0))

    b.append(label(px + side / 2, py - 8, "r = 1", "xs"))
    b.append(label(px + side + 4, py - 8, "r = √3", "xs", "start"))

    b.append(hollow(396, py + 150, 296, 106, GREEN, rx=4))
    b.append(label(544, py + 170, "dω = dA · cosθ / r² = dA / r³", "xs"))
    b.append(label(544, py + 188, "because the tilt cosθ is itself 1/r", "xs"))
    b.append(label(544, py + 208, f"corner : centre = 1 / 3√3 = {CORNER_RATIO:.5f}", "xs"))
    b.append(label(544, py + 226,
                   f"so the CENTRE texel is worth {CENTRE_WORTH:.3f} corner texels", "xs"))
    b.append(label(544, py + 246,
                   f"(measured {CORNER_AT_128:.6f} at 128, approaching it)", "xs"))

    b.append(label(W / 2, H - 32,
                   f"Averaging texels with EQUAL weights is therefore "
                   f"{(FLAT_BIAS - 1) * 100:.2f}% too bright — and refining the cube does "
                   f"not help", "xs"))
    b.append(label(W / 2, H - 14,
                   f"({FLAT_BIAS_16:.6f} at 16×16, {FLAT_BIAS:.6f} at 64×64). "
                   f"It is a BIAS, not a discretisation error.", "xs"))

    return svg("f615b", W, H, "Cube faces and the solid angle of a texel",
               "On the left, the six cube faces unfolded into a cross and labelled "
               "in SDL's order. On the right, a face seen edge-on from the origin, "
               "showing that a corner texel is further away and more steeply tilted "
               "than a centre one, so it subtends about a fifth of the solid angle.", b)


# ===========================================================================
# Figure 3 — the split, and which half is exact                          (§4)
# ===========================================================================
def fig_split():
    W, H = 720, 400
    b = [label(W / 2, 22, "One integral, split two ways — and only the first "
                          "split is exact", "sm")]

    # the equation at the top
    b.append(label(W / 2, 50, "Lₒ(v) = ∫ fᵣ(l, v) · Lᵢ(l) "
                              "· (n·l) dω", "sm"))

    # ---- LEFT: diffuse, exact ----------------------------------------------
    lx, ly, lw, lh = 44, 76, 306, 200
    b.append(box(lx, ly, lw, lh, GREEN, rx=4, opacity=0.10))
    b.append(hollow(lx, ly, lw, lh, GREEN, rx=4))
    b.append(label(lx + lw / 2, ly + 24, "THE DIFFUSE HALF — EXACT", "xs"))
    b.append(label(lx + lw / 2, ly + 50, "fᵣ = albedo/π is a CONSTANT,", "xs"))
    b.append(label(lx + lw / 2, ly + 66, "so it comes out of the integral:", "xs"))
    b.append(label(lx + lw / 2, ly + 92, "Lₒ = (albedo/π) · E(n)", "sm"))
    b.append(label(lx + lw / 2, ly + 118, "and E(n) depends ONLY on the normal.", "xs"))
    b.append(label(lx + lw / 2, ly + 134, "Precompute it into a small cube map;", "xs"))
    b.append(label(lx + lw / 2, ly + 150, "one fetch at run time.", "xs"))
    b.append(label(lx + lw / 2, ly + 174,
                   f"No approximation — only discretisation,", "xs"))
    b.append(label(lx + lw / 2, ly + 190,
                   f"{IRRADIANCE[-1][2]:.1e} relative at 64×64.", "xs"))

    # ---- RIGHT: specular, approximate --------------------------------------
    rx, ry = 370, 76
    b.append(box(rx, ry, lw, lh, AMBER, rx=4, opacity=0.10))
    b.append(hollow(rx, ry, lw, lh, AMBER, rx=4))
    b.append(label(rx + lw / 2, ry + 24, "THE SPECULAR HALF — WE PRETEND", "xs"))
    b.append(label(rx + lw / 2, ry + 50, "fᵣ depends on l AND v, so NOTHING", "xs"))
    b.append(label(rx + lw / 2, ry + 66, "comes out. The split sum does it anyway:", "xs"))
    b.append(label(rx + lw / 2, ry + 94, "[ prefiltered Lᵢ ] × [ ∫ fᵣ (n·l) dω ]", "xs"))
    b.append(label(rx + lw / 2, ry + 120, "a mip chain indexed by", "xs"))
    b.append(label(rx + lw / 2, ry + 136, "ROUGHNESS, times a 2-D table", "xs"))
    b.append(label(rx + lw / 2, ry + 152, "of (n·v, roughness) that depends", "xs"))
    b.append(label(rx + lw / 2, ry + 168, "on NEITHER the environment nor", "xs"))
    b.append(label(rx + lw / 2, ry + 184, "the material.", "xs"))

    # ---- what it costs -----------------------------------------------------
    b.append(hollow(44, 292, 632, 92, RED, rx=4))
    b.append(label(W / 2, 312, "AND THE PRICE IS A MEASUREMENT, NOT A HEDGE", "xs"))
    b.append(label(W / 2, 332,
                   f"roughness 0.10: {SPLIT_LOW_ROUGH:.2f}% error   ·   "
                   f"near-normal, any roughness: ≤ {SPLIT_NORMAL_WORST:.1f}% dark   ·   "
                   f"grazing, roughness 1: {SPLIT_GRAZING_WORST:.1f}% dark", "xs"))
    b.append(label(W / 2, 352,
                   f"with a sun disc in the sky: {SPLIT_SUN_WORST:.1f}% dark. The "
                   f"error of this technique depends on the ENVIRONMENT,", "xs"))
    b.append(label(W / 2, 372, "not only on the material — which is not "
                               "something you can read off the paper.", "xs"))

    return svg("f615c", W, H, "The split-sum approximation, and which half is exact",
               "The rendering equation over an environment, split into a diffuse "
               "half that factors exactly because the Lambert BRDF is constant, and "
               "a specular half that does not factor and is approximated anyway. "
               "Below, the measured error of the approximation.", b)


# ===========================================================================
# Figure 4 — the level a lobe wants, against the one everybody ships     (§5)
# ===========================================================================
def fig_level():
    W, H = 720, 446
    b = [label(W / 2, 22, "Which mip level a roughness should read — derived from "
                          "the lobe, and the mapping everybody ships", "sm")]

    px, py, pw, ph = 92, 56, 470, 250
    b.append(box(px, py, pw, ph, None, rx=3))

    # axes
    for k in range(8):
        y = py + ph - k / 7.0 * ph
        b.append(rule(px, y, px + pw, y, "grid", 0.8, "3 4"))
        b.append(label(px - 10, y + 4, str(k), "xs", "end"))
    for k in range(6):
        x = px + k / 5.0 * pw
        b.append(rule(x, py, x, py + ph, "grid", 0.8, "3 4"))
        b.append(label(x, py + ph + 18, f"{k / 5.0:.1f}", "xs"))
    b.append(label(px - 40, py + ph / 2, "level", "xs"))
    b.append(label(px + pw / 2, py + ph + 38, "roughness", "xs"))

    def to_xy(rough, level):
        return (px + rough * pw, py + ph - level / 7.0 * ph)

    # the derived curve
    pts = " ".join(f"{x:.1f},{y:.1f}" for x, y in
                   (to_xy(r, d) for r, _, _, _, d, _ in LEVELS))
    b.append(f'<polyline points="{pts}" fill="none" stroke="{GREEN}" stroke-width="2.2"/>')
    # the linear one
    x0, y0 = to_xy(0.0, 0.0)
    x1, y1 = to_xy(1.0, 7.0)
    b.append(cline(x0, y0, x1, y1, AMBER, 2.0, "6 4"))

    # the worst gap, marked
    gx = px + WORST_LEVEL_AT * pw
    derived_at = 3.920
    linear_at = WORST_LEVEL_AT * 7.0
    _, gy1 = to_xy(WORST_LEVEL_AT, derived_at)
    _, gy2 = to_xy(WORST_LEVEL_AT, linear_at)
    b.append(cline(gx, gy1, gx, gy2, RED, 2.6))
    # THE CALLOUT GOES OUTSIDE THE PLOT (6.11's trap): a label placed at the
    # marker's own midpoint lands on the gridlines it is measuring between.
    b.append(cline(gx, gy2, 574, 236, RED, 1.0, "3 3"))
    b.append(label(578, 240, f"{WORST_LEVEL_GAP:.2f} levels", "xs", "start"))

    # legend OUTSIDE the plot
    b.append(hollow(578, 56, 128, 74, GREY, rx=4))
    b.append(cline(592, 78, 616, 78, GREEN, 2.2))
    b.append(label(622, 82, "derived", "xs", "start"))
    b.append(cline(592, 104, 616, 104, AMBER, 2.0, "6 4"))
    b.append(label(622, 108, "shipped", "xs", "start"))

    b.append(hollow(52, 354, 652, 82, GREEN, rx=4))
    b.append(label(W / 2, 376,
                   "k = ½·log₂( lobe solid angle / texel solid angle ) — "
                   "6.14’s closed-form lobe width, cashed.", "xs"))
    b.append(label(W / 2, 396,
                   f"The two never differ by more than {WORST_LEVEL_GAP:.2f} of a level "
                   f"(at roughness {WORST_LEVEL_AT:.2f}). THE FOLKLORE IS NOT ARBITRARY.", "xs"))
    b.append(label(W / 2, 416,
                   f"For comparison √roughness, also seen in the wild, is off by "
                   f"{SQRT_GAP:.2f} levels. And note the direction: a near-mirror is "
                   f"OVER-blurred.", "xs"))

    return svg("f615d", W, H, "Mip level against roughness: derived and shipped",
               "A plot of prefiltered mip level against roughness. The derived "
               "curve, from matching the GGX lobe's solid angle to a texel's, runs "
               "above the straight line of the linear mapping over most of the "
               "range and below it near zero, never differing by more than 0.77 of "
               "a level.", b)


# ===========================================================================
# Figure 5 — the error has a shape                                       (§5)
# ===========================================================================
def fig_shape():
    W, H = 720, 400
    b = [label(W / 2, 22, "The split sum’s error is not noise: it has a shape, "
                          "and the shape names the assumption", "sm")]

    px, py, pw, ph = 84, 54, 430, 214
    b.append(box(px, py, pw, ph, None, rx=3))

    lo, hi = 0.65, 1.05
    for frac in (0.7, 0.8, 0.9, 1.0):
        y = py + ph - (frac - lo) / (hi - lo) * ph
        b.append(rule(px, y, px + pw, y, "grid", 0.8, "3 4"))
        b.append(label(px - 10, y + 4, f"{frac:.1f}", "xs", "end"))
    roughs = [0.10, 0.25, 0.50, 0.75, 1.00]
    for i, r in enumerate(roughs):
        x = px + i / 4.0 * pw
        b.append(rule(x, py, x, py + ph, "grid", 0.8, "3 4"))
        b.append(label(x, py + ph + 18, f"{r:.2f}", "xs"))
    b.append(label(px + pw / 2, py + ph + 38, "roughness", "xs"))
    b.append(label(px - 46, py + ph / 2, "split / true", "xs"))

    def pt(i, ratio):
        return (px + i / 4.0 * pw,
                py + ph - (max(lo, min(hi, ratio)) - lo) / (hi - lo) * ph)

    for ndv, colour in ((0.9, BLUE), (0.4, RED)):
        rows = [(i, ratio) for i, (_, n, _, _, ratio)
                in enumerate([(r, n, t, s, q) for (r, n, t, s, q) in SPLIT])
                if n == ndv]
        # re-derive indices per roughness
        rows = []
        for i, r in enumerate(roughs):
            for (rr, nn, _t, _s, ratio) in SPLIT:
                if abs(rr - r) < 1e-6 and abs(nn - ndv) < 1e-6:
                    rows.append((i, ratio))
        pts = " ".join(f"{x:.1f},{y:.1f}" for x, y in (pt(i, q) for i, q in rows))
        b.append(f'<polyline points="{pts}" fill="none" stroke="{colour}"'
                 f' stroke-width="2.2"/>')
        for i, q in rows:
            x, y = pt(i, q)
            b.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3" fill="{colour}"/>')

    # the perfect line
    _, yone = pt(0, 1.0)
    b.append(cline(px, yone, px + pw, yone, GREY, 1.2, "4 4"))

    b.append(hollow(530, 54, 170, 78, GREY, rx=4))
    b.append(cline(544, 76, 568, 76, BLUE, 2.2))
    b.append(label(574, 80, "n·v = 0.9", "xs", "start"))
    b.append(cline(544, 104, 568, 104, RED, 2.2))
    b.append(label(574, 108, "n·v = 0.4", "xs", "start"))

    b.append(hollow(530, 148, 170, 120, AMBER, rx=4))
    b.append(label(615, 168, "AT GRAZING IT IS", "xs"))
    b.append(label(615, 186, f"{SPLIT_GRAZING_WORST:.0f}% DARK;", "xs"))
    b.append(label(615, 204, "head-on, never worse", "xs"))
    b.append(label(615, 222, f"than {SPLIT_NORMAL_WORST:.1f}%. More", "xs"))
    b.append(label(615, 240, "than four times the", "xs"))
    b.append(label(615, 258, "error, same material.", "xs"))

    b.append(hollow(84, 296, 616, 90, RED, rx=4))
    b.append(label(W / 2, 316, "THAT ASYMMETRY IS THE ASSUMPTION n = v = r, "
                               "MADE VISIBLE.", "xs"))
    b.append(label(W / 2, 336, "A prefiltered value is indexed by ONE direction "
                               "while the true integral depends on two, so the", "xs"))
    b.append(label(W / 2, 354, "prefilter bakes its lobe around the REFLECTION. At "
                               "a grazing view that points across the horizon and", "xs"))
    b.append(label(W / 2, 372, "averages in ground the real surface never sees. What "
                               "it discards is the stretched highlight.", "xs"))

    return svg("f615e", W, H, "The split sum's error against roughness and view angle",
               "Two curves of the ratio between the split-sum result and the true "
               "integral. At near-normal incidence the ratio stays close to one at "
               "every roughness; at a grazing view it falls to 0.71 by roughness 1.", b)


# ===========================================================================
# Figure 6 — the bake, and what each part costs                          (§6)
# ===========================================================================
def fig_bake():
    W, H = 720, 398
    b = [label(W / 2, 22, "One sky in, three precomputed things out — and they "
                          "are only correct together", "sm")]

    # source
    sx, sy, sw, sh = 40, 66, 138, 92
    b.append(box(sx, sy, sw, sh, BLUE, rx=4, opacity=0.22))
    b.append(label(sx + sw / 2, sy + 26, "RADIANCE", "xs"))
    b.append(label(sx + sw / 2, sy + 44, "128² × 6 faces", "xs"))
    b.append(label(sx + sw / 2, sy + 62, f"{SOURCE_MB:.3f} MB", "xs"))
    b.append(label(sx + sw / 2, sy + 80, "the skybox draws this", "xs"))

    outs = [("IRRADIANCE", "32² × 6", BAKE_MB[0], BAKE_MS[0],
             "indexed by NORMAL", GREEN),
            ("PREFILTERED", "128² × 6 × 6 lvl", BAKE_MB[1], BAKE_MS[1],
             "indexed by ROUGHNESS", AMBER),
            ("BRDF TABLE", "64², 2-D", BAKE_MB[2], BAKE_MS[2],
             "environment-independent", PURPLE)]
    ox, oy, ow, oh = 268, 54, 200, 78
    for i, (name, dims, mb, ms, note, colour) in enumerate(outs):
        y = oy + i * (oh + 10)
        b.append(box(ox, y, ow, oh, colour, rx=4, opacity=0.18))
        b.append(label(ox + ow / 2, y + 20, name, "xs"))
        b.append(label(ox + ow / 2, y + 38, dims, "xs"))
        b.append(label(ox + ow / 2, y + 56, f"{mb:.3f} MB · {ms / 1000:.2f} s to bake", "xs"))
        b.append(arrow(sx + sw + 6, sy + sh / 2, ox - 6, y + oh / 2, "f615f", "s", 1.1))
        b.append(label(ox + ow + 10, y + 44, note, "xs", "start"))

    b.append(hollow(40, 322, 660, 66, AMBER, rx=4))
    b.append(label(W / 2, 342, "THEY ARE HELD IN ONE STRUCT BECAUSE THEY ARE ONLY "
                               "CORRECT TOGETHER: an irradiance map baked from one", "xs"))
    b.append(label(W / 2, 360, "sky and a prefiltered chain baked from another give "
                               "a diffuse and a specular half that disagree about", "xs"))
    b.append(label(W / 2, 378, f"where the sun is. The chain costs "
                               f"{CHAIN_RATIO:.4f}× the source — the famous "
                               f"4/3, from 1 + ¼ + 1/16 + …", "xs"))

    return svg("f615f", W, H, "The environment bake and what each output costs",
               "One radiance cube map feeding three precomputed outputs: a small "
               "irradiance map indexed by surface normal, a prefiltered mip chain "
               "indexed by roughness, and an environment-independent two-dimensional "
               "BRDF table, with the memory and bake time of each.", b)


# ===========================================================================
# Figure 7 — the measurement that lied                                   (§7)
# ===========================================================================
def fig_noise():
    W, H = 720, 320
    b = [label(W / 2, 22, "The first version of this measurement was wrong, and it "
                          "looked completely reasonable", "sm")]

    # two bars
    bx, by, bw = 110, 72, 200
    b.append(label(bx + bw / 2, by - 14, "SUN PRESENT (6000:1)", "xs"))
    for i, (n, val, colour) in enumerate((("8,192 samples", 1.98, RED),
                                          ("1,000,000", 1.00, GREEN))):
        y = by + i * 52
        w = bw * val / 2.0
        b.append(box(bx, y, w, 34, colour, rx=3, opacity=0.35))
        b.append(label(bx + 6, y + 22, n, "xs", "start"))
    b.append(label(bx + bw / 2, by + 126, f"a factor of {NOISE_FACTOR:.2f} apart,", "xs"))
    b.append(label(bx + bw / 2, by + 144,
                   f"at roughness {NOISE_AT[0]:.2f}, n·v {NOISE_AT[1]:.1f}", "xs"))

    bx2 = 420
    b.append(label(bx2 + bw / 2, by - 14, "SUN REMOVED (smooth sky)", "xs"))
    for i, (n, val, colour) in enumerate((("8,192 samples", 1.0004, GREEN),
                                          ("1,000,000", 1.00, GREEN))):
        y = by + i * 52
        w = bw * val / 2.0
        b.append(box(bx2, y, w, 34, colour, rx=3, opacity=0.35))
        b.append(label(bx2 + 6, y + 22, n, "xs", "start"))
    b.append(label(bx2 + bw / 2, by + 126,
                   f"{NOISE_SMOOTH:.3f}% apart — the same", "xs"))
    b.append(label(bx2 + bw / 2, by + 144, "estimator, converged", "xs"))

    b.append(hollow(60, 236, 600, 74, AMBER, rx=4))
    b.append(label(W / 2, 256, "THE SAMPLE COUNT WAS NEVER THE PROBLEM — THE "
                               "DYNAMIC RANGE WAS.", "xs"))
    b.append(label(W / 2, 276, "Lesson 6.14’s rule was to check a measurement "
                               "CAN produce a non-null result before believing a", "xs"))
    b.append(label(W / 2, 296, "null one. This is its mirror: check a non-null "
                               "result has CONVERGED before believing it.", "xs"))

    return svg("f615g", W, H, "An unconverged Monte Carlo estimate against a converged one",
               "Two pairs of bars. With a bright sun disc in the environment, 8,192 "
               "importance samples give a result nearly twice the converged "
               "one-million-sample answer; with the sun removed the same two sample "
               "counts agree to four hundredths of a per cent.", b)


def main():
    figs = [("l615_fig1.svg", fig_problem()),
            ("l615_fig2.svg", fig_cube()),
            ("l615_fig3.svg", fig_split()),
            ("l615_fig4.svg", fig_level()),
            ("l615_fig5.svg", fig_shape()),
            ("l615_fig6.svg", fig_bake()),
            ("l615_fig7.svg", fig_noise())]
    for name, body in figs:
        path = os.path.join(OUT, name)
        with open(path, "w") as fh:
            fh.write(body)
        print(f"wrote {path}  ({len(body):,} bytes)")


if __name__ == "__main__":
    main()
