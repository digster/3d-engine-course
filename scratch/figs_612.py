#!/usr/bin/env python3
"""scratch/figs_612.py — Lesson 6.12's diagrams.

Same rules as 6.1-6.11's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - ~5.2 units per character for `xs`, ~6.0 for `sm`
  - filenames numbered by PAGE ORDER
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - `rule()` takes a CSS CLASS; `cline()` takes a COLOUR (6.10's trap)
  - LEGENDS GO OUTSIDE THE PLOT (6.11's trap: "empty space" judged by eye on a
    diagram whose curves cross most of the box is not a measurement)

Every number comes from verify_612's output.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)
from figs_610 import cline                                          # noqa: E402
from figs_611 import swatch                                         # noqa: E402

OUT = "scratch"

# ---- measured (verify_612, probe_612) --------------------------------------
DEMO_PEAK = 0.8676        # §A — the demo's own roughness 0.49
PEAKS = [(0.49, 0.8676), (0.35, 1.7370), (0.20, 11.5899),
         (0.10, 177.0252), (0.05, 2823.9907)]
METAL_PEAK = 55916.9       # §A — polished metal at the mirror angle
SUN_PEAK = 8333875.0       # §A — the same surface under 120,000 lux
REINHARD_WHITE_IN = 224    # §C — the input that finally reaches code 255
ACES_AT_ONE = 0.8038       # §C
ACES_DARK_LOSS = 13        # §C — codes lost at x = 0.02
SAT_IN, SAT_PER, SAT_LUM = 20.0, 3.11, 20.00   # §D
LUMA_BLUE_OUT = 5.0710     # §D — a channel that survived the curve and gets clipped
BLEND_HDR_NS = 9.25        # §F
BLEND_LDR_NS = 75.12       # §F
BLEND_RATIO = 8.1          # §F
ARITH_MEAN = 0.5500        # §G
LOG_MEAN = 0.05016         # §G
MEAN_RATIO = 11.0          # §G
DEMO_OVER = 126            # the demo: pixels over the lid
DEMO_PIXELS = 129600
DEMO_MAX = 411.82          # the demo: max channel, 8.7 stops
CHECKS = 45

CURVES = {
    "clamp":   [0.1000, 0.5000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000],
    "reinhard": [0.0909, 0.3333, 0.5000, 0.6667, 0.8000, 0.9412, 0.9961],
    "white":   [0.0915, 0.3438, 0.5312, 0.7500, 1.0000, 1.0000, 1.0000],
    "aces":    [0.1258, 0.6163, 0.8038, 0.9149, 0.9734, 1.0000, 1.0000],
}


def clamp_f(x):
    return min(x, 1.0)


def reinhard_f(x):
    return x / (1.0 + x)


def white_f(x, w=4.0):
    return min(x * (1.0 + x / (w * w)) / (1.0 + x), 1.0)


def aces_f(x):
    a, b, c, d, e = 2.51, 0.03, 2.43, 0.59, 0.14
    return max(0.0, min(1.0, (x * (a * x + b)) / (x * (c * x + d) + e)))


# ===========================================================================
# Figure 1 — the lid, and what is above it                              (§1)
# ===========================================================================
def fig_lid():
    W, H = 720, 400
    b = [label(W / 2, 22, "What an 8-bit pixel can hold, and what the shading equation returns", "sm")]

    # A log axis of linear value from 0.5 to 100,000.
    x0, x1 = 120, 640
    y_axis = 300

    def px(v):
        lo, hi = math.log10(0.5), math.log10(1.0e5)
        return x0 + (x1 - x0) * (math.log10(v) - lo) / (hi - lo)

    b.append(rule(x0, y_axis, x1, y_axis, "grid", 1.0))
    for v in (1, 10, 100, 1000, 10000, 100000):
        b.append(rule(px(v), y_axis - 4, px(v), y_axis + 5, "grid", 1.0))
        lab = f"{v:,}" if v < 100000 else "100,000"
        b.append(label(px(v), y_axis + 20, lab, "xs"))
    b.append(label((x0 + x1) / 2, y_axis + 44, "linear value the shading equation returns", "xs"))

    # THE LID.
    b.append(cline(px(1.0), 60, px(1.0), y_axis - 2, RED, 2.2))
    b.append(label(px(1.0) - 8, 54, "1.0 — the lid", "xs", "end"))
    b.append(box(px(1.0), 66, x1 - px(1.0), y_axis - 72, RED, rx=2, opacity=0.10))
    b.append(label((px(1.0) + x1) / 2, 92,
                   "everything in here is stored as code 255", "xs"))
    b.append(label((px(1.0) + x1) / 2, 110, "— the same code, for all of it —", "xs"))

    # The measured peaks.
    y = 140
    for r, peak in PEAKS:
        colour = GREEN if peak <= 1.0 else AMBER
        b.append(f'<circle cx="{px(peak):.1f}" cy="{y}" r="4" fill="{colour}"/>')
        b.append(label(px(peak) + 10, y + 4,
                       f"roughness {r:.2f}   {peak:,.2f}", "xs", "start"))
        y += 22

    b.append(f'<circle cx="{px(METAL_PEAK):.1f}" cy="{y}" r="4" fill="{RED}"/>')
    b.append(label(px(METAL_PEAK) - 10, y + 4,
                   f"polished METAL  {METAL_PEAK:,.0f}  (+15.8 stops)", "xs", "end"))

    b.append(label(W / 2, H - 42,
                   f"The demo's own roughness is 0.49, which peaks at {DEMO_PEAK} — "
                   "JUST under. That is not luck and it is not an accident:", "xs"))
    b.append(label(W / 2, H - 24,
                   "`k_reference_irradiance` = pi was chosen in 6.2 so a white surface "
                   "renders at exactly 1.0. The engine has been AVOIDING", "xs"))
    b.append(label(W / 2, H - 6,
                   "the question by construction, and one polished material is all it "
                   "takes to ask it.", "xs"))
    return svg("f612a", W, H, "The 8-bit lid and the values above it",
               "A logarithmic axis of linear values with the 1.0 clamp marked, and "
               "the measured specular peaks of surfaces at five roughnesses.", b)


# ===========================================================================
# Figure 2 — exposure as a scale of light                               (§3)
# ===========================================================================
def fig_exposure():
    W, H = 720, 330
    b = [label(W / 2, 22, "EV is a logarithmic scale of light, and one stop is a factor of two", "sm")]

    x0, x1 = 90, 630
    y = 130

    evs = [(0, "a very dim room"), (3, "a dim room"), (6, ""),
           (9, "indoors, bright"), (12, "an overcast day"), (15, "full daylight")]

    b.append(rule(x0, y, x1, y, "grid", 1.2))
    for i, (ev, name) in enumerate(evs):
        x = x0 + (x1 - x0) * i / (len(evs) - 1)
        b.append(rule(x, y - 6, x, y + 6, "grid", 1.0))
        b.append(label(x, y - 16, f"EV {ev}", "xs"))
        white = 1.2 * (2.0 ** ev)
        b.append(label(x, y + 28, f"white at", "xs"))
        b.append(label(x, y + 44, f"{white:,.1f}", "xs"))
        if name:
            b.append(label(x, y + 68, name, "xs"))

    b.append(arrow(x0 + 20, y + 94, x1 - 20, y + 94, "f612b", "s", 1.2))
    b.append(label((x0 + x1) / 2, y + 88, "each step DOUBLES the value that maps to white", "xs"))

    # The old behaviour, as a point on the scale.
    b.append(rule(x0, 232, x1, 232, "grid", 1.0))
    b.append(label(W / 2, 256,
                   "AND THE ENGINE'S OLD BEHAVIOUR IS A POINT ON THIS SCALE, which is what "
                   "turns a replacement into a generalisation:", "xs"))
    b.append(label(W / 2, 278,
                   "1.0 maps to white  =>  exposure x1  =>  EV = log2(1/1.2) = -0.263.", "xs"))
    b.append(label(W / 2, 300,
                   "6.2's choice that a white card renders at 1.0 WAS an exposure setting. "
                   "It was simply the only one available.", "xs"))
    return svg("f612b", W, H, "Exposure values and the luminance each maps to white",
               "A scale of exposure values from 0 to 15 with the luminance that maps "
               "to white at each, and the engine's previous behaviour located on it.", b)


# ===========================================================================
# Figure 3 — the four curves                                            (§3)
# ===========================================================================
def fig_curves():
    W, H = 720, 392
    b = [label(W / 2, 22, "Four ways to bring [0, infinity) down to [0, 1]", "sm")]

    x0, x1 = 92, 452
    y0, y1 = 300, 62

    # A log-ish x axis so the interesting part is visible: 0 to 8 linear.
    def px(v):
        return x0 + (x1 - x0) * min(v, 8.0) / 8.0

    def py(v):
        return y0 + (y1 - y0) * v

    b.append(rule(x0, y0, x1, y0, "grid", 1.0))
    b.append(rule(x0, y0, x0, y1, "grid", 1.0))
    for v in (0, 2, 4, 6, 8):
        b.append(rule(px(v), y0, px(v), y0 + 5, "grid", 1.0))
        b.append(label(px(v), y0 + 20, f"{v}", "xs"))
    for v in (0.0, 0.5, 1.0):
        b.append(rule(x0 - 5, py(v), x0, py(v), "grid", 1.0))
        b.append(label(x0 - 12, py(v) + 4, f"{v:g}", "xs", "end"))
    b.append(label((x0 + x1) / 2, y0 + 40, "linear value in", "xs"))
    b.append(label(x0 - 46, (y0 + y1) / 2, "out", "xs"))

    # the lid, for reference
    b.append(cline(x0, py(1.0), x1, py(1.0), GREY, 1.0, dash="4 3"))

    fns = [(clamp_f, RED, "6 4"), (reinhard_f, BLUE, None),
           (lambda v: white_f(v), GREEN, None), (aces_f, AMBER, None)]
    for fn, colour, dash in fns:
        pts = " ".join(f"{px(i * 8.0 / 160):.1f},{py(fn(i * 8.0 / 160)):.1f}"
                       for i in range(161))
        d = f' stroke-dasharray="{dash}"' if dash else ""
        b.append(f'<polyline points="{pts}" fill="none" stroke="{colour}" '
                 f'stroke-width="2.2"{d}/>')

    # legend, outside the plot
    lx = 492
    rows = [
        (RED, "6 4", "clamp", ["min(x, 1) — what the", "engine has always done"]),
        (BLUE, None, "reinhard", ["x / (1 + x) — never", f"reaches 1; needs {REINHARD_WHITE_IN}", "to make code 255"]),
        (GREEN, None, "reinhard-white", ["x(1 + x/W^2)/(1 + x)", "sends W to exactly 1"]),
        (AMBER, None, "aces", ["a FIT, not a model;", f"sends 1.0 to {ACES_AT_ONE}"]),
    ]
    ly = 74
    for colour, dash, name, lines in rows:
        swatch(b, lx, ly, colour, dash=dash)
        b.append(label(lx + 36, ly + 4, name, "xs", "start"))
        for k, ln in enumerate(lines):
            b.append(label(lx + 36, ly + 22 + k * 17, ln, "xs", "start"))
        ly += 26 + len(lines) * 17 + 8

    b.append(label(W / 2, H - 34,
                   "THREE PROPERTIES MAKE ONE LEGAL: monotonic (or a highlight can come out "
                   "darker than its own edge), close to the", "xs"))
    b.append(label(W / 2, H - 14,
                   "identity near zero (or the dark end is wrong), and bounded by 1 (or it "
                   "has not done its job). Everything else is taste.", "xs"))
    return svg("f612c", W, H, "Four tonemap operators",
               "Clamp, Reinhard, Reinhard with a white point and the ACES fit, plotted "
               "over linear inputs from 0 to 8.", b)


# ===========================================================================
# Figure 4 — the resolve, and the order of its three steps               (§3)
# ===========================================================================
def fig_resolve():
    W, H = 720, 330
    b = [label(W / 2, 22, "One pass over a finished image, and the order is not negotiable", "sm")]

    stages = [("the HDR buffer", "values of light,", "unbounded", GREY),
              ("x exposure", "still linear —", "light adds in time", BLUE),
              ("the curve", "still linear — it is", "about light, not codes", GREEN),
              ("encode", "LAST, and exactly", "once (6.1's rule)", AMBER)]
    bw, gap = 150, 22
    x0 = (W - (len(stages) * bw + (len(stages) - 1) * gap)) / 2
    y = 64
    for i, (name, l1, l2, colour) in enumerate(stages):
        x = x0 + i * (bw + gap)
        b.append(hollow(x, y, bw, 96, colour, rx=5))
        b.append(box(x, y, bw, 28, colour, rx=5, opacity=0.18))
        b.append(label(x + bw / 2, y + 19, name, "xs"))
        b.append(label(x + bw / 2, y + 50, l1, "xs"))
        b.append(label(x + bw / 2, y + 68, l2, "xs"))
        if i + 1 < len(stages):
            b.append(arrow(x + bw + 3, y + 48, x + bw + gap - 3, y + 48, "f612d", "s", 1.2))

    b.append(rule(60, 188, W - 60, 188, "grid", 1.0))
    b.append(label(W / 2, 212, "and each wrong order has its own signature:", "xs"))

    rows = [("exposure AFTER the curve", "code 203 becomes code 128 — and worse, the bright "
             "end can no longer be told apart,"),
            ("", "because everything above about 4 was squeezed into the same place before "
             "the multiply"),
            ("the curve AFTER the encode", "mid grey maps to 0.4244 instead of 0.3333 — a "
             "curve applied to CODES compresses what"),
            ("", "the encode had carefully spread out, and it crushes the shadows first")]
    yy = 238
    for lead, rest in rows:
        if lead:
            b.append(label(84, yy, lead, "xs", "start"))
            b.append(label(84 + 190, yy, rest, "xs", "start"))
        else:
            b.append(label(84 + 190, yy, rest, "xs", "start"))
        yy += 20
    return svg("f612d", W, H, "The three steps of a resolve",
               "Exposure, then the tonemap curve, then the sRGB encode, with the "
               "measured consequence of each wrong ordering.", b)


# ===========================================================================
# Figure 5 — per-channel against luminance-only                         (§3)
# ===========================================================================
def fig_hue():
    W, H = 720, 340
    b = [label(W / 2, 22, "Two ways to apply a curve to a colour, and they disagree", "sm")]

    inp = (8.0, 2.0, 0.4)
    per = (0.8889, 0.6667, 0.2857)
    lum = (1.9230, 0.4808, 0.0962)

    def bars(x, y, vals, cap, note, scale):
        b.append(label(x + 90, y, cap, "xs"))
        cols = [RED, GREEN, BLUE]
        names = ["R", "G", "B"]
        for i, v in enumerate(vals):
            bx = x + i * 62
            hgt = max(2.0, min(v / scale, 1.0) * 86)
            b.append(box(bx, y + 106 - hgt, 44, hgt, cols[i], rx=2, opacity=0.55))
            b.append(hollow(bx, y + 106 - hgt, 44, hgt, cols[i], rx=2))
            b.append(label(bx + 22, y + 122, names[i], "xs"))
            b.append(label(bx + 22, y + 138, f"{v:.3f}", "xs"))
        b.append(label(x + 90, y + 162, note, "xs"))

    bars(60, 52, inp, "the colour (8.0, 2.0, 0.4)", f"red:blue = {SAT_IN:.0f}", 8.0)
    bars(285, 52, per, "PER CHANNEL", f"red:blue = {SAT_PER:.2f} — desaturated", 1.0)
    bars(510, 52, lum, "LUMINANCE ONLY", f"red:blue = {SAT_LUM:.0f} — hue exact", 2.0)

    b.append(cline(510 + 0, 52 + 106 - 86, 510 + 132, 52 + 106 - 86, AMBER, 1.4, dash="4 3"))
    b.append(label(510 + 150, 52 + 106 - 82, "1.0", "xs", "start"))

    b.append(rule(60, 244, W - 60, 244, "grid", 1.0))
    b.append(label(W / 2, 268,
                   "Per-channel compresses the big channel more than the small one, so a "
                   "bright colour moves TOWARD WHITE as it brightens —", "xs"))
    b.append(label(W / 2, 288,
                   "which is what film does. Luminance-only preserves the hue exactly, and "
                   "can leave a channel ABOVE 1 for the encode to clip:", "xs"))
    b.append(label(W / 2, 308,
                   f"a pure blue of (0, 0, 8) sails through the curve and arrives at "
                   f"{LUMA_BLUE_OUT:.3f}, because blue's luminance weight is only 0.0722.", "xs"))
    b.append(label(W / 2, 328,
                   "So it preserves the hue right up to the point where it does not — at a "
                   "threshold nobody chose.", "xs"))
    return svg("f612e", W, H, "Per-channel against luminance-only tonemapping",
               "The same bright orange through both variants, with the resulting "
               "channel values and the red-to-blue ratio of each.", b)


# ===========================================================================
# Figure 6 — the two passes                                             (§4)
# ===========================================================================
def fig_passes():
    W, H = 720, 360
    b = [label(W / 2, 22, "The first pass in this engine that draws no geometry", "sm")]

    # Pass one
    b.append(hollow(60, 54, 270, 150, BLUE, rx=6))
    b.append(box(60, 54, 270, 30, BLUE, rx=6, opacity=0.18))
    b.append(label(195, 74, "PASS ONE — the scene", "xs"))
    for k, ln in enumerate(["nine pipelines (6.11), depth,",
                            "blending, meshes, materials",
                            "",
                            "cost scales with GEOMETRY",
                            "encode_output = 0 — a float",
                            "target has no transfer function"]):
        b.append(label(195, 104 + k * 17, ln, "xs"))

    # The buffer
    b.append(arrow(334, 128, 374, 128, "f612f", "s", 1.3))
    b.append(hollow(378, 88, 118, 80, AMBER, rx=6))
    b.append(box(378, 88, 118, 26, AMBER, rx=6, opacity=0.18))
    b.append(label(437, 105, "HDR target", "xs"))
    b.append(label(437, 130, "R16G16B16A16", "xs"))
    b.append(label(437, 147, "_FLOAT, 2x the", "xs"))
    b.append(label(437, 161, "bytes of LDR", "xs"))
    b.append(arrow(500, 128, 540, 128, "f612f", "s", 1.3))

    # Pass two
    b.append(hollow(544, 54, 130, 150, GREEN, rx=6))
    b.append(box(544, 54, 130, 30, GREEN, rx=6, opacity=0.18))
    b.append(label(609, 74, "PASS TWO", "xs"))
    for k, ln in enumerate(["the resolve", "", "no vertex input", "no depth", "no blending",
                            "one triangle"]):
        b.append(label(609, 104 + k * 17, ln, "xs"))

    b.append(rule(60, 222, W - 60, 222, "grid", 1.0))
    b.append(label(W / 2, 246,
                   "WHY NOT JUST TONEMAP AT THE END OF THE SCENE SHADER? It costs nothing "
                   "extra and it is wrong three ways,", "xs"))
    b.append(label(W / 2, 266,
                   "each of which becomes a real limitation within two lessons:", "xs"))
    for k, ln in enumerate([
        "an EXPOSURE DERIVED FROM THE FRAME cannot be known while the frame is still being drawn;",
        "BLOOM COMES BEFORE THE CURVE (6.13), and a per-fragment tonemap has already compressed what it needs;",
        "BLENDING WOULD COMPOSITE TONEMAPPED VALUES — and f(a) over f(b) is not f(a over b), because a curve is not linear."]):
        b.append(label(W / 2, 292 + k * 20, ln, "xs"))
    return svg("f612f", W, H, "The scene pass, the HDR target and the resolve pass",
               "Two render passes with a float colour target between them, and the "
               "three reasons the tonemap cannot live in the scene's fragment shader.", b)


def main():
    # FILENAMES FOLLOW PAGE ORDER, which is the rule at the top of this file and
    # which this lesson broke on its first build: the body puts §3.3's
    # per-channel comparison BEFORE §3.4's resolve, and the numbers said the
    # reverse. `check-page.js` gained a `figOrder` check the same day — it is the
    # second time this exact drift has happened (6.10 shipped `fig4.png`
    # captioned "Figure 5"), and nothing was checking it.
    figs = [("l612_fig1.svg", fig_lid()),
            ("l612_fig2.svg", fig_exposure()),
            ("l612_fig3.svg", fig_curves()),
            ("l612_fig4.svg", fig_hue()),
            ("l612_fig5.svg", fig_resolve()),
            ("l612_fig6.svg", fig_passes())]
    for name, body in figs:
        path = os.path.join(OUT, name)
        with open(path, "w") as fh:
            fh.write(body)
        print(f"wrote {path}  ({len(body):,} bytes)")


if __name__ == "__main__":
    main()
