#!/usr/bin/env python3
"""scratch/figs_610.py — Lesson 6.10's diagrams.

Same rules as 6.1-6.9's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - ~5.2 units per character for `xs`, ~6.0 for `sm`
  - filenames numbered by PAGE ORDER
  - no HTML tags inside <text>; use <tspan class="t-hi">

Every number comes from verify_610's output, or from Lesson 3.9's.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)

OUT = "scratch"


def cline(x1, y1, x2, y2, colour, width=1.4, dash=None):
    """A line with an explicit stroke COLOUR.

    `rule()` from figs_510 takes a CSS CLASS, not a colour — pass it a hex
    string and you get class="#e05c5c", which matches no rule and renders
    invisibly. Nothing errors, the line is simply not there, and only a visual
    pass catches it. Hence this helper.
    """
    d = f' stroke-dasharray="{dash}"' if dash else ""
    return (f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" '
            f'stroke="{colour}" stroke-width="{width}" fill="none"{d}/>')

# ---- measured --------------------------------------------------------------
CHECKS = 24
NEAR_TEXELS = 0.60        # 3.9: texels per pixel at the bottom of the frame
FAR_TEXELS = 62.46        # 3.9: two rows below the horizon
SEEN_PCT = 6.4            # 3.9: fraction of the footprint four taps can see
HORIZON_LEVEL = 5.965     # §C
CODE_LINEAR = 188         # §B
CODE_NAIVE = 127          # §B
LIT_LINEAR = 0.5029       # §B
LIT_NAIVE = 0.2122        # §B
TOO_DARK = 57.8           # §B, per cent
DELIVERS = 42.2           # §B, per cent of the light it should
GRAD_ERR = 5.79e-06       # §D
JUMP_NEAREST = 0.3470     # §E
JUMP_TRILIN = 0.0124      # §E
CHAIN_PCT = 33.33         # §A / §G
DEMO_PCT = 13.1           # pixels changed on the demo floor


# ===========================================================================
# Figure 1 — 3.9's measurement, and the gap it named                   (§1)
# ===========================================================================
def fig_problem():
    W, H = 720, 392
    b = []
    b.append(label(W / 2, 22, "What one pixel covers, and what four taps can see", "sm"))

    x0, x1 = 90, 640
    y_base = 250
    top = 62

    # A log axis of texels-per-pixel from 0.5 to 64.
    def px(t):
        lo, hi = math.log2(0.5), math.log2(64.0)
        return x0 + (x1 - x0) * (math.log2(t) - lo) / (hi - lo)

    b.append(rule(x0, y_base, x1, y_base, "grid", 1.0))
    for t in (0.5, 1, 2, 4, 8, 16, 32, 64):
        b.append(rule(px(t), y_base - 4, px(t), y_base + 5, "grid", 1.0))
        b.append(label(px(t), y_base + 20, f"{t:g}", "xs"))
    b.append(label(W / 2, y_base + 76, "texels covered by one screen pixel", "xs"))

    # What bilinear reads: always four.
    b.append(rule(x0, top + 30, x1, top + 30, "hi", 2.0))
    b.append(label(x0 + 6, top + 20, "what bilinear READS: four texels, always", "xs", "start"))

    # What the pixel covers: the diagonal.
    b.append(f'<path d="M {px(0.5):.1f} {y_base - 6} L {px(64):.1f} {top + 8}" '
             f'stroke="{BLUE}" stroke-width="1.8" fill="none"/>')
    # Clear of the diagonal: to its right and well below, so the label does not
    # sit on the line it names.
    b.append(label(px(26), top + 74, "what it COVERS", "xs"))

    # The crossover, and 3.9's two measurements.
    # The two 3.9 measurements. Their captions go BELOW the axis, beside the
    # tick numbers, because above it they land on the frustum path.
    for t, tag, colour, dy in ((NEAR_TEXELS, "bottom of frame", GREEN, 40),
                               (FAR_TEXELS, "below the horizon", RED, 56)):
        b.append(cline(px(t), top, px(t), y_base, colour, 1.4, "4 4"))
        b.append(label(px(t), top - 10, f"{t:g}", "xs"))
        b.append(label(px(t), y_base + dy, tag, "xs"))

    b.append(rule(px(4), top + 30, px(4), y_base, "grid", 1.2, dash="3 3"))
    b.append(label(px(4), top + 48, "they cross here", "xs"))

    # Two short lines rather than one long one: at 390px the SVG scales down but
    # the text does not, so a long string overflows whatever the viewBox is.
    b.append(label(W / 2, 356, f"past the crossover the sampler sees a fraction of the truth", "xs"))
    b.append(label(W / 2, 374, f"— {SEEN_PCT}% of it at the horizon. That gap IS the sparkle.", "xs"))
    return svg("f610a", W, H, "Footprint against what bilinear reads",
               "A logarithmic axis of texels covered by one screen pixel. Bilinear always reads "
               "four texels, a flat line. What the pixel actually covers rises from 0.60 at the "
               "bottom of the frame to 62.46 below the horizon, crossing the flat line at four.",
               b)


# ===========================================================================
# Figure 2 — the chain, and the 33%                                    (§3.1)
# ===========================================================================
def fig_chain():
    W, H = 720, 360
    b = []
    b.append(label(W / 2, 22, "The image, at every size at once", "sm"))

    x = 80
    y_top = 60
    size = 150
    for lvl in range(6):
        s = size / (2 ** lvl)
        b.append(box(x, y_top + (size - s) / 2, s, s, BLUE, opacity=0.30 + 0.09 * lvl))
        b.append(label(x + s / 2, y_top + (size - s) / 2 - 8, f"L{lvl}", "xs"))
        n = 256 // (2 ** lvl)
        b.append(label(x + s / 2, y_top + size + 22, f"{n}x{n}", "xs"))
        frac = 1.0 / (4 ** lvl)
        b.append(label(x + s / 2, y_top + size + 38,
                       "1" if lvl == 0 else f"1/{4 ** lvl}", "xs"))
        x += s + 26

    b.append(label(W / 2, y_top + size + 66,
                   "each level is a quarter of the one above, so the extra is", "xs"))
    b.append(label(W / 2, y_top + size + 88,
                   "1/4 + 1/16 + 1/64 + ...  =  1/3", "xs"))
    b.append(label(W / 2, y_top + size + 112,
                   f"measured on a 256x256 checker: {CHAIN_PCT}%", "xs"))
    b.append(label(W / 2, y_top + size + 130, "in 9 levels (1 + log2 256)", "xs"))
    return svg("f610b", W, H, "A mip chain",
               "Six squares of halving size labelled L0 to L5, from 256x256 down to 8x8, each a "
               "quarter the area of the one before. The series 1/4 + 1/16 + 1/64 sums to one "
               "third, which is the 33 per cent memory cost.", b)


# ===========================================================================
# Figure 3 — averaging in the wrong space                              (§3.4)
# ===========================================================================
def fig_linear():
    W, H = 720, 340
    b = []
    b.append(label(W / 2, 22, "Half of black and white, computed two ways", "sm"))

    # the 2x2 source
    cx = 130
    cy = 90
    q = 34
    for i, (dx, dy, white) in enumerate(((0, 0, True), (1, 0, False),
                                         (0, 1, False), (1, 1, True))):
        col = "#f2f2f2" if white else "#141414"
        b.append(f'<rect x="{cx + dx * q}" y="{cy + dy * q}" width="{q}" height="{q}" '
                 f'fill="{col}" stroke="{GREY}" stroke-width="1"/>')
    b.append(label(cx + q, cy - 12, "one 2x2 block", "xs"))
    b.append(label(cx + q, cy + 2 * q + 18, "codes 255, 0, 0, 255", "xs"))

    rows = [
        (160, "averaged as sRGB BYTES", CODE_NAIVE, LIT_NAIVE, "#7d7d7d", RED,
         "(255+0+0+255)/4 = 127"),
        (245, "averaged in LINEAR LIGHT", CODE_LINEAR, LIT_LINEAR, "#bcbcbc", GREEN,
         "decode, average, re-encode"),
    ]
    for y, tag, code, lit, swatch, colour, how in rows:
        b.append(label(300, y - 16, tag, "xs", "start"))
        b.append(f'<rect x="300" y="{y - 8}" width="52" height="34" fill="{swatch}" '
                 f'stroke="{GREY}" stroke-width="1"/>')
        b.append(label(372, y + 6, f"code {code}", "xs", "start"))
        b.append(label(450, y + 6, f"= {lit:.4f} of white", "xs", "start"))
        b.append(label(300, y + 40, how, "xs", "start"))
        b.append(rule(296, y - 12, 296, y + 46, colour, 2.4))

    b.append(label(W / 2, 312,
                   f"the naive average delivers {DELIVERS}% of the light it should — "
                   f"{TOO_DARK}% TOO DARK at level 1 alone", "xs"))
    b.append(label(W / 2, 332,
                   "and it compounds down the chain, so the surface dims as it recedes", "xs"))
    return svg("f610c", W, H, "sRGB averaging against linear-light averaging",
               "A 2x2 block of two white and two black texels. Averaged as sRGB bytes it gives "
               "code 127, which is 0.2122 of white. Averaged in linear light it gives code 188, "
               "which is 0.5029 — half the light, as it should be. The naive answer delivers 42 "
               "per cent of the light it should.", b)


# ===========================================================================
# Figure 4 — choosing the level                                        (§3.2)
# ===========================================================================
def fig_level():
    W, H = 720, 330
    b = []
    b.append(label(W / 2, 22, "Which level: log2 of the footprint", "sm"))

    x0, x1 = 100, 620
    y = 170

    def px(t):
        lo, hi = 0.0, 6.0
        return x0 + (x1 - x0) * (math.log2(t) - lo) / (hi - lo)

    b.append(rule(x0, y, x1, y, "grid", 1.0))
    for t in (1, 2, 4, 8, 16, 32, 64):
        b.append(rule(px(t), y - 5, px(t), y + 5, "grid", 1.0))
        b.append(label(px(t), y + 22, f"{t:g}", "xs"))
        b.append(label(px(t), y - 18, f"L{int(math.log2(t))}", "xs"))
    b.append(label(W / 2, y + 46, "footprint, in texels", "xs"))
    b.append(label(x0 - 34, y - 18, "level", "xs", "end"))

    # 3.9's horizon measurement, landing between levels.
    # 62.46 and 64 are three pixels apart on a log axis, so a vertical rule at
    # the measurement would sit on the "64" tick whatever we do with it. A short
    # leader out to clear space says the same thing and collides with nothing.
    b.append(cline(px(FAR_TEXELS), y - 34, px(FAR_TEXELS) - 96, y - 60, RED, 1.4))
    b.append(f'<circle cx="{px(FAR_TEXELS):.1f}" cy="{y - 34}" r="3.5" fill="{RED}" '
             f'stroke="none"/>')
    b.append(label(px(FAR_TEXELS) - 100, y - 64, f"Lesson 3.9: {FAR_TEXELS} texels", "xs", "end"))
    b.append(label(px(FAR_TEXELS) - 100, y - 48, f"= level {HORIZON_LEVEL}", "xs", "end"))

    b.append(label(W / 2, 262,
                   "a level halves the texture, so level L has texels 2^L wide:", "xs"))
    b.append(label(W / 2, 284, "2^L = footprint   =>   L = log2(footprint)", "xs"))
    b.append(label(W / 2, 310,
                   "the level is CONTINUOUS, and the fraction is what trilinear blends", "xs"))
    return svg("f610d", W, H, "Level selection",
               "A logarithmic footprint axis from 1 to 64 texels with the mip level marked above "
               "each power of two. Lesson 3.9's measured 62.46 texels per pixel lands at level "
               "5.965, between levels 5 and 6.", b)


# ===========================================================================
# Figure 5 — the derivative the CPU has to compute                     (§3.3)
# ===========================================================================
def fig_gradient():
    W, H = 720, 330
    b = []
    b.append(label(W / 2, 22, "Where the footprint comes from", "sm"))

    # left: the GPU's quad
    b.append(label(170, 58, "GPU: shade in 2x2 quads", "xs"))
    q = 30
    for dx in range(2):
        for dy in range(2):
            b.append(hollow(140 + dx * q, 74 + dy * q, q, q, BLUE))
    b.append(f'<circle cx="{140 + q / 2}" cy="{74 + q / 2}" r="4" fill="{RED}" stroke="none"/>')
    b.append(arrow(140 + q / 2, 74 + q / 2, 140 + q + q / 2, 74 + q / 2, "f610e", "h"))
    b.append(arrow(140 + q / 2, 74 + q / 2, 140 + q / 2, 74 + q + q / 2, "f610e", "h"))
    b.append(label(170, 74 + 2 * q + 22, "ddx / ddy are a SUBTRACTION:", "xs"))
    b.append(label(170, 74 + 2 * q + 38, "the neighbour is always there", "xs"))

    # right: the CPU's scanline
    b.append(label(520, 58, "CPU: one fragment at a time", "xs"))
    for i in range(5):
        b.append(hollow(455 + i * q, 74, q, q, GREY, dash="3 3"))
    b.append(f'<circle cx="{455 + 2 * q + q / 2}" cy="{74 + q / 2}" r="4" fill="{RED}" stroke="none"/>')
    b.append(label(520, 74 + q + 22, "no neighbour to subtract —", "xs"))
    b.append(label(520, 74 + q + 38, "the row above is long gone", "xs"))

    b.append(rule(90, 196, 630, 196, "grid", 1.0))
    b.append(label(W / 2, 220, "so the CPU computes it from the TRIANGLE, exactly:", "xs"))
    b.append(label(W / 2, 246, "u = U / W,  with U and W both AFFINE in screen space", "xs"))
    b.append(label(W / 2, 268, "du/dx = (dU/dx  −  u · dW/dx) / W", "xs"))
    b.append(label(W / 2, 294,
                   "dU/dx and dW/dx are constant over the triangle, so they hoist out of the "
                   "loop", "xs"))
    b.append(label(W / 2, 316,
                   f"measured against a central difference of the real interpolation: "
                   f"{GRAD_ERR:.2e}", "xs"))
    return svg("f610e", W, H, "The screen-space uv derivative",
               "On the left, a GPU shades fragments in 2x2 quads so a neighbour is always "
               "available and ddx is a subtraction. On the right, a scanline rasterizer has no "
               "neighbour, so the derivative is computed from the triangle analytically by the "
               "quotient rule, agreeing with a numerical difference to 5.79e-06.", b)


# ===========================================================================
# Figure 6 — anisotropy                                                (§3.6)
# ===========================================================================
def fig_aniso():
    W, H = 720, 310
    b = []
    b.append(label(W / 2, 22, "A footprint that is not square", "sm"))

    cx, cy = 200, 130
    b.append(f'<ellipse cx="{cx}" cy="{cy}" rx="96" ry="12" fill="{BLUE}" stroke="{BLUE}" '
             f'fill-opacity="0.35" stroke-width="1.4"/>')
    b.append(label(cx, cy - 30, "what the pixel covers: 32 texels by 2", "xs"))

    # isotropic: a square containing it
    b.append(hollow(cx - 96, cy - 96, 192, 192, RED, dash="5 4"))
    b.append(label(cx, cy + 116, "isotropic: level 5, from the LONG axis", "xs"))
    b.append(label(cx, cy + 134, "the short axis is blurred 16x", "xs"))

    # anisotropic: several samples along the long axis
    ax = 520
    b.append(f'<ellipse cx="{ax}" cy="{cy}" rx="96" ry="12" fill="{BLUE}" stroke="{BLUE}" '
             f'fill-opacity="0.35" stroke-width="1.4"/>')
    for i in range(8):
        sx = ax - 84 + i * 24
        b.append(f'<circle cx="{sx}" cy="{cy}" r="11" fill="none" stroke="{GREEN}" '
                 f'stroke-width="1.4"/>')
    b.append(label(ax, cy + 116, "anisotropic: level 1, from the SHORT axis", "xs"))
    b.append(label(ax, cy + 134, "several taps ALONG the long one", "xs"))

    b.append(label(W / 2, 282,
                   "isotropic must choose: the long axis blurs, the short axis aliases. "
                   "Anisotropy refuses.", "xs"))
    b.append(label(W / 2, 302,
                   "and on a SQUARE footprint it costs nothing — measured identical to six "
                   "decimals", "xs"))
    return svg("f610f", W, H, "Isotropic against anisotropic filtering",
               "A long thin elliptical footprint, 32 texels by 2. On the left an isotropic "
               "square containing it selects level 5 from the long axis and blurs the short axis "
               "by 16 times. On the right, anisotropic filtering selects level 1 from the short "
               "axis and takes eight samples along the long one.", b)


def main():
    figs = {
        "l610_fig1.svg": fig_problem(),
        "l610_fig2.svg": fig_chain(),
        # NUMBERED BY PAGE ORDER, not authoring order: the page shows level
        # selection and the gradient before the linear-light bug, so those are
        # figures 3 and 4 and the bug is 5. Nothing verifies this — the numbers
        # live in build_610.py's FIGURES dict and the order lives in the body
        # fragments — so it is a thing to check by reading the captions.
        "l610_fig3.svg": fig_level(),
        "l610_fig4.svg": fig_gradient(),
        "l610_fig5.svg": fig_linear(),
        "l610_fig6.svg": fig_aniso(),
    }
    for name, body in figs.items():
        with open(os.path.join(OUT, name), "w") as fh:
            fh.write(body)
        print(f"  wrote {OUT}/{name}  ({len(body)} bytes)")


if __name__ == "__main__":
    main()
