#!/usr/bin/env python3
"""scratch/figs_61.py — Lesson 6.1's diagrams.

Same rules as 5.10's and 5.11's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - every element well inside the viewBox, and in-bar labels checked against the bar
  - ~5.2 units per character for `xs` (calibrated against shipped figures)
  - filenames numbered by PAGE ORDER

Every number in here is either computed from the sRGB definition below or copied
from verify_61's output, which prints the ramp for exactly this purpose.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)

OUT = "scratch"


# ---- The transfer function, so every coordinate here is the real one ---------
def encode(linear):
    """Linear light -> sRGB-encoded value in [0,1]. The exact piecewise form."""
    if linear <= 0.0031308:
        return 12.92 * linear
    return 1.055 * (linear ** (1.0 / 2.4)) - 0.055


def decode(enc):
    """sRGB-encoded value in [0,1] -> linear light."""
    if enc <= 0.04045:
        return enc / 12.92
    return ((enc + 0.055) / 1.055) ** 2.4


def code_of(linear):
    return int(round(255.0 * encode(linear)))


# ---- Measured inputs, from verify_61 ----------------------------------------
# linear, raw (the pre-6.1 engine), hardware-encoded, shader-encoded, CPU
RAMP = [
    (0.02, 5, 39, 39, 39),
    (0.05, 13, 63, 63, 63),
    (0.10, 26, 89, 89, 89),
    (0.20, 51, 124, 124, 124),
    (0.35, 89, 160, 160, 160),
    (0.50, 128, 188, 188, 188),
    (0.65, 166, 211, 211, 211),
    (0.80, 204, 231, 231, 231),
    (0.95, 242, 249, 249, 249),
]
CHECKS = 34
EVEN_DARK, SRGB_DARK = 26, 90
EVEN_BRIGHT, SRGB_BRIGHT = 128, 68


# ===========================================================================
# Figure 1 — the two things called colour  (§2)
# ===========================================================================
def fig_two_things():
    uid = "l61f1"
    W, H = 720, 470
    b = [label(20, 20, "A stored value is not an amount of light. It is a CODE for one, "
                       "and this is the curve between them.", "sm", "start")]

    # Axes: x = stored code 0..255, y = emitted light 0..1
    X0, Y0, PW, PH = 90, 60, 380, 300
    b.append(rule(X0, Y0 + PH, X0 + PW, Y0 + PH, "grid", 1.2))
    b.append(rule(X0, Y0, X0, Y0 + PH, "grid", 1.2))
    # The axis titles sit CLEAR of the tick labels at +18. They used to be at +34
    # and +50, one of which landed on the prose below and the other under it —
    # inside the viewBox, overlapping nothing the geometry check measures, and
    # obvious the moment the figure was looked at.
    b.append(label(X0 + PW / 2, Y0 + PH + 38, "stored code — what is in the byte",
                   "xs muted"))
    # And the y-axis title goes ABOVE the axis rather than beside it, where it was
    # sitting on the 0.5 tick.
    b.append(label(X0 - 34, Y0 - 10, "light emitted", "xs muted", "start"))

    for frac, txt in ((0.0, "0"), (0.5, "128"), (1.0, "255")):
        x = X0 + frac * PW
        b.append(rule(x, Y0 + PH, x, Y0 + PH + 5, "grid", 1.0))
        b.append(label(x, Y0 + PH + 18, txt, "xs muted"))
    for frac, txt in ((0.0, "0"), (0.5, "0.5"), (1.0, "1.0")):
        y = Y0 + PH - frac * PH
        b.append(rule(X0 - 5, y, X0, y, "grid", 1.0))
        b.append(label(X0 - 10, y + 4, txt, "xs muted", "end"))

    # The assumption: a straight line.
    b.append(rule(X0, Y0 + PH, X0 + PW, Y0, "grid", 1.2, dash="4 4"))
    b.append(label(X0 + PW * 0.74, Y0 + PH * 0.20, "what everyone assumes", "xs muted"))

    # The truth.
    pts = []
    for i in range(0, 256, 2):
        e = i / 255.0
        pts.append((X0 + e * PW, Y0 + PH - decode(e) * PH))
    path = " ".join(f"{x:.1f},{y:.1f}" for x, y in pts)
    b.append(f'<polyline points="{path}" fill="none" stroke="{AMBER}" stroke-width="2.2"/>')
    # OUT OF THE PLOT ENTIRELY. Two placements were tried inside it and both sat
    # on something — first the polyline it names, then a dashed leader — because
    # the interior of this plot is almost entirely lines. When a label cannot find
    # clear space among the data, the answer is to stop looking for space among
    # the data. It goes in the right-hand column, below the two readings.

    # The two readings that make it concrete.
    x128 = X0 + 0.5 * PW
    y128 = Y0 + PH - decode(0.5) * PH
    b.append(rule(x128, Y0 + PH, x128, y128, "ink-soft", 1.0, dash="3 3"))
    b.append(rule(X0, y128, x128, y128, "ink-soft", 1.0, dash="3 3"))
    b.append(f'<circle cx="{x128:.1f}" cy="{y128:.1f}" r="3.4" fill="{RED}"/>')

    e188 = 188 / 255.0
    x188 = X0 + e188 * PW
    y188 = Y0 + PH - 0.5 * PH
    b.append(rule(x188, Y0 + PH, x188, y188, "ink-soft", 1.0, dash="3 3"))
    b.append(rule(X0, y188, x188, y188, "ink-soft", 1.0, dash="3 3"))
    b.append(f'<circle cx="{x188:.1f}" cy="{y188:.1f}" r="3.4" fill="{GREEN}"/>')
    b.append(label(x188, y188 - 12, "188", "xs mono"))

    # The two sentences, to the right.
    tx = X0 + PW + 42
    b.append(label(tx, 96, "code 128 emits", "sm t-bad", "start"))
    b.append(label(tx, 116, f"{decode(0.5):.4f}", "sm mono", "start"))
    b.append(label(tx, 134, "of white's light —", "xs muted", "start"))
    b.append(label(tx, 150, "not half of it.", "xs muted", "start"))

    b.append(label(tx, 194, "half the light is", "sm t-ok", "start"))
    b.append(label(tx, 214, "code 188", "sm mono", "start"))
    b.append(label(tx, 232, "and the gap between", "xs muted", "start"))
    b.append(label(tx, 248, "those two numbers is", "xs muted", "start"))
    b.append(label(tx, 264, "this whole lesson.", "xs muted", "start"))

    b.append(label(tx, 306, "the curve above is the", "xs t-hi", "start"))
    b.append(label(tx, 322, "sRGB transfer function;", "xs t-hi", "start"))
    b.append(label(tx, 338, "the dashed line is the", "xs muted", "start"))
    b.append(label(tx, 354, "assumption it replaces.", "xs muted", "start"))

    b.append(rule(24, 424, 696, 424, "grid", 1.2))
    b.append(label(24, 446, "EVERYTHING THE RENDERER COMPUTES IS ON THE VERTICAL AXIS. A "
                            "reflectance multiplies light; two lamps add their",
                   "xs t-hi", "start"))
    b.append(label(24, 462, "light; an average of two colours is an average of their light. "
                            "Do any of that on the horizontal axis and the answer",
                   "xs muted", "start"))

    return svg(uid, W, H, "The sRGB transfer function between stored codes and emitted light",
               "A plot with stored code on the x axis and emitted light on the y axis. The "
               "assumed straight line is dashed; the true sRGB curve bows below it. Code 128 "
               "emits 0.2159 of white, and half the light is stored as code 188.", b)


# ===========================================================================
# Figure 2 — the code budget  (§2)
# ===========================================================================
def fig_budget():
    uid = "l61f2"
    W, H = 720, 400
    b = [label(20, 20, "Why the curve exists at all: 256 codes are a BUDGET, and the eye "
                       "does not spend attention evenly.", "sm", "start")]

    X0, BW = 60, 600

    # Row 1: codes spaced evenly in LIGHT.
    b.append(label(X0, 60, "spaced evenly in LIGHT", "xs t-bad", "start"))
    y = 72
    b.append(box(X0, y, BW, 26, colour=GREY, opacity=0.10))
    for i in range(0, 256, 4):
        lin = i / 255.0
        x = X0 + lin * BW
        b.append(rule(x, y, x, y + 26, "ink-soft", 0.7))
    b.append(hollow(X0, y, BW * 0.1, 26, RED, width=1.6))
    b.append(label(X0 + BW * 0.05, y + 46, f"{EVEN_DARK} codes", "xs t-bad"))
    b.append(label(X0 + BW * 0.55, y + 46, f"{EVEN_BRIGHT} codes in the brightest half — "
                                           "where the eye can barely tell two apart",
                   "xs muted"))

    # Row 2: sRGB.
    b.append(label(X0, 172, "spaced by the sRGB curve", "xs t-ok", "start"))
    y2 = 184
    b.append(box(X0, y2, BW, 26, colour=GREY, opacity=0.10))
    for i in range(0, 256, 4):
        lin = decode(i / 255.0)
        x = X0 + lin * BW
        b.append(rule(x, y2, x, y2 + 26, "ink-soft", 0.7))
    b.append(hollow(X0, y2, BW * 0.1, 26, GREEN, width=1.6))
    b.append(label(X0 + BW * 0.05, y2 + 46, f"{SRGB_DARK} codes", "xs t-ok"))
    b.append(label(X0 + BW * 0.55, y2 + 46, f"{SRGB_BRIGHT} codes in the brightest half",
                   "xs muted"))

    b.append(label(X0 + BW / 2, 262, "← both rows are the same 256 codes, positioned by how "
                                     "much LIGHT each one emits →", "xs muted"))

    b.append(rule(24, 288, 696, 288, "grid", 1.2))
    b.append(label(24, 312, "THE EYE JUDGES RATIOS, NOT DIFFERENCES.", "xs t-hi", "start"))
    b.append(label(24, 330, "The step from 0.01 to 0.02 of white is obvious; the step from "
                            "0.90 to 0.91 is invisible. Both are 0.01 of light. So codes",
                   "xs muted", "start"))
    b.append(label(24, 346, "should be spaced so that each STEP is a similar ratio — which "
                            "is what a power curve does, and it is why the encoding is",
                   "xs muted", "start"))
    b.append(label(24, 362, "not an accident of old CRT hardware but a compression scheme "
                            "that outlived the hardware it was named for.", "xs muted",
                   "start"))
    b.append(label(24, 386, "8 bits of LINEAR light would need about 12 to look as smooth. "
                            "That is what the curve buys.", "xs t-hi", "start"))

    return svg(uid, W, H, "How 256 codes are distributed across the light range",
               "Two strips, each showing 256 codes positioned by the light they emit. "
               "Spaced evenly in light, only 26 fall in the darkest tenth; spaced by the sRGB "
               "curve, 90 do.", b)


# ===========================================================================
# Figure 3 — the shape of the curve, and the toe  (§3)
# ===========================================================================
def fig_curve():
    uid = "l61f3"
    W, H = 720, 420
    b = [label(20, 20, "The definition is piecewise, and the small straight piece is the "
                       "part people leave out.", "sm", "start")]

    # A zoomed plot of the DARK end, where the toe lives: linear 0 .. 0.02.
    X0, Y0, PW, PH = 80, 56, 300, 250
    LIM = 0.02
    b.append(rule(X0, Y0 + PH, X0 + PW, Y0 + PH, "grid", 1.2))
    b.append(rule(X0, Y0, X0, Y0 + PH, "grid", 1.2))
    b.append(label(X0 + PW / 2, Y0 + PH + 32, "linear light (0 to 0.02)", "xs muted"))
    # "encoded" used to sit at (X0 - 10, Y0 - 8) and the join label sits at the
    # same height a little to the right; at this zoom they touched. Moving the
    # axis title to the LEFT edge separates them without moving the plot.
    b.append(label(24, Y0 - 8, "encoded", "xs muted", "start"))

    def plot(fn, colour, width=2.0, dash=None):
        pts = []
        for i in range(0, 101):
            lin = LIM * i / 100.0
            v = fn(lin)
            pts.append((X0 + (lin / LIM) * PW, Y0 + PH - min(1.0, v / 0.25) * PH))
        d = f' stroke-dasharray="{dash}"' if dash else ""
        path = " ".join(f"{x:.1f},{y:.1f}" for x, y in pts)
        return (f'<polyline points="{path}" fill="none" stroke="{colour}" '
                f'stroke-width="{width}"{d}/>')

    b.append(plot(encode, AMBER, 2.2))
    b.append(plot(lambda x: x ** (1.0 / 2.2), RED, 1.8, dash="5 4"))

    # The join.
    jx = X0 + (0.0031308 / LIM) * PW
    b.append(rule(jx, Y0, jx, Y0 + PH, "ink-soft", 1.0, dash="3 3"))
    b.append(label(jx, Y0 - 8, "0.0031308", "xs mono"))

    b.append(label(X0 + PW * 0.42, Y0 + PH * 0.72, "12.92 x", "xs mono"))
    b.append(label(X0 + PW * 0.72, Y0 + PH * 0.30, "1.055 x^(1/2.4) - 0.055", "xs mono"))
    b.append(label(X0 + PW * 0.60, Y0 + PH * 0.06, "pow(x, 1/2.2)", "xs t-bad"))

    tx = X0 + PW + 46
    b.append(label(tx, 76, "WHY A STRAIGHT PIECE AT ALL", "xs t-hi", "start"))
    b.append(label(tx, 96, "The slope of x^(1/2.4) at zero is", "xs muted", "start"))
    b.append(label(tx, 112, "INFINITE. A curve with infinite", "xs muted", "start"))
    b.append(label(tx, 128, "gain at the origin turns sensor", "xs muted", "start"))
    b.append(label(tx, 144, "noise into visible banding and", "xs muted", "start"))
    b.append(label(tx, 160, "cannot be inverted stably.", "xs muted", "start"))
    b.append(label(tx, 184, "So the definition replaces it with", "xs muted", "start"))
    b.append(label(tx, 200, "a line of slope 12.92, and picks", "xs muted", "start"))
    b.append(label(tx, 216, "the join so the two pieces meet.", "xs muted", "start"))

    b.append(label(tx, 250, "AND THE COST OF SKIPPING IT", "xs t-bad", "start"))
    worst, at = 0, 0.0
    for i in range(1001):
        lin = i / 1000.0
        d = abs(code_of(lin) - int(round(255.0 * (lin ** (1.0 / 2.2)))))
        if d > worst:
            worst, at = d, lin
    b.append(label(tx, 270, f"pow(x, 1/2.2) is off by up to", "xs muted", "start"))
    b.append(label(tx, 286, f"{worst} codes, worst near linear {at:.4f}.", "xs muted",
                   "start"))
    b.append(label(tx, 302, "Close in the midtones; wrong", "xs muted", "start"))
    b.append(label(tx, 318, "exactly where the toe is.", "xs muted", "start"))

    b.append(rule(24, 344, 696, 344, "grid", 1.2))
    b.append(label(24, 366, "encode(x) = 12.92 x                       for x <= 0.0031308",
                   "xs mono", "start"))
    b.append(label(24, 384, "          = 1.055 x^(1/2.4) - 0.055       otherwise",
                   "xs mono", "start"))
    b.append(label(24, 408, "Four constants, and all four matter. Two implementations that "
                            "disagree about any of them differ by a code somewhere.",
                   "xs t-hi", "start"))

    return svg(uid, W, H, "The sRGB encoding curve near zero, showing the linear toe",
               "A zoomed plot over linear 0 to 0.02. The sRGB curve is a straight line of "
               "slope 12.92 below linear 0.0031308 and a shifted power above it. A pow(x, "
               "1/2.2) approximation departs from it sharply near zero.", b)


# ===========================================================================
# Figure 4 — the pipeline, and the hole that was in it  (§4, §5)
# ===========================================================================
def fig_pipeline():
    uid = "l61f4"
    W, H = 720, 420
    b = [label(20, 20, "Two conversions. Not three, not one per operation — two, at the "
                       "edges, with light in between.", "sm", "start")]

    # Inputs
    ins = [("texture", "_SRGB format: the", "sampler decodes"),
           ("a tint (Uint32)", "to_linear()", "once, at load"),
           ("a light's colour", "authored linear", "no conversion")]
    for i, (name, how, note) in enumerate(ins):
        y = 62 + i * 62
        b.append(box(24, y, 150, 24, colour=BLUE, opacity=0.18))
        b.append(label(99, y + 16, name, "xs mono"))
        b.append(label(99, y + 38, how, "xs muted"))
        b.append(label(99, y + 52, note, "xs muted"))
        b.append(arrow(178, y + 12, 236, y + 12, uid, "h", 1.2))

    b.append(label(99, 44, "INPUTS — decode here", "xs t-ok"))

    # The linear middle
    b.append(box(242, 56, 220, 194, colour=AMBER, opacity=0.20))
    b.append(label(352, 84, "LIGHT", "sm mono"))
    b.append(label(352, 108, "shading", "xs muted"))
    b.append(label(352, 126, "interpolation", "xs muted"))
    b.append(label(352, 144, "blending", "xs muted"))
    b.append(label(352, 162, "mipmaps, filtering", "xs muted"))
    b.append(label(352, 180, "anti-aliasing", "xs muted"))
    b.append(label(352, 204, "every one of these", "xs t-hi"))
    b.append(label(352, 220, "is arithmetic, and", "xs t-hi"))
    b.append(label(352, 236, "needs real numbers", "xs t-hi"))

    # Outputs
    b.append(label(600, 44, "OUTPUT — encode ONCE", "xs t-ok"))
    b.append(arrow(466, 100, 520, 100, uid, "h", 1.3))
    b.append(box(526, 88, 170, 24, colour=GREEN, opacity=0.20))
    b.append(label(611, 104, "CPU: to_encoded()", "xs mono"))
    b.append(label(611, 126, "correct since 1.6", "xs muted"))

    b.append(arrow(466, 176, 520, 176, uid, "h", 1.3))
    b.append(box(526, 164, 170, 24, colour=RED, opacity=0.20))
    b.append(label(611, 180, "GPU: …nothing", "xs mono"))
    b.append(label(611, 202, "THE HOLE. Light went", "xs t-bad"))
    b.append(label(611, 218, "straight into an SDR", "xs muted"))
    b.append(label(611, 234, "swapchain, whose bytes", "xs muted"))
    b.append(label(611, 250, "MEAN codes.", "xs muted"))

    b.append(rule(24, 286, 696, 286, "grid", 1.2))
    b.append(label(24, 308, "THE RULE IS NOT “CONVERT CAREFULLY”. IT IS “CONVERT TWICE, AT "
                            "THE EDGES”.", "xs t-hi", "start"))
    b.append(label(24, 328, "Converting per operation — which is what this engine did through "
                            "Module 3, and said so — is slower, lossier at 8 bits, and",
                   "xs muted", "start"))
    b.append(label(24, 344, "gets one operation wrong the week somebody adds one. Converting "
                            "at the edges makes the middle a place where the ordinary",
                   "xs muted", "start"))
    b.append(label(24, 360, "rules of arithmetic hold, which is the only kind of middle a "
                            "renderer can be written in.", "xs muted", "start"))
    b.append(label(24, 386, "The fix in this lesson is not new maths. It is ONE MISSING EDGE.",
                   "xs t-hi", "start"))

    return svg(uid, W, H, "The two conversion points of a colour pipeline",
               "Inputs decode to linear light on the left, all rendering arithmetic happens in "
               "linear in the middle, and the result is encoded once on the right. The CPU "
               "path encoded correctly; the GPU path had no encode at all.", b)


# ===========================================================================
# Figure 5 — the measured ramp  (§6)
# ===========================================================================
def fig_ramp():
    uid = "l61f5"
    W, H = 720, 462
    b = [label(20, 20, "The bug, measured on real GPU pixels: nine light levels, rendered "
                       "twice by the same shader.", "sm", "start")]

    X0, CW = 130, 62
    b.append(label(X0 - 14, 66, "light asked for", "xs muted", "end"))
    for i, (lin, raw, hw, sw, cpu) in enumerate(RAMP):
        b.append(label(X0 + i * CW + CW / 2, 66, f"{lin:.2f}", "xs mono"))

    # Two swatch rows, drawn with the ACTUAL stored codes.
    def swatch_row(y, label_text, values, cls):
        b.append(label(X0 - 14, y + 20, label_text, "xs mono", "end"))
        for i, v in enumerate(values):
            g = int(v)
            b.append(f'<rect x="{X0 + i * CW}" y="{y}" width="{CW - 4}" height="32" '
                     f'fill="rgb({g},{g},{g})" stroke="none"/>')
            b.append(label(X0 + i * CW + (CW - 4) / 2, y + 48, str(v), "xs mono"))
        b.append(label(X0 - 14, y + 36, cls, "xs muted", "end"))

    swatch_row(88, "before 6.1", [r[1] for r in RAMP], "linear, written raw")
    swatch_row(172, "after 6.1", [r[2] for r in RAMP], "encoded on write")

    # The ratio bars start BELOW the second row's code labels (at y = 220). They
    # used to grow up from y = 262 to a height of 58, i.e. to y = 204, straight
    # through them — a collision no geometry check sees, because a bar and a label
    # inside the same viewBox are two shapes that happen to overlap.
    BASE = 300
    b.append(label(X0 - 14, BASE - 6, "times too dark", "xs muted", "end"))
    for i, (lin, raw, hw, sw, cpu) in enumerate(RAMP):
        shown = decode(raw / 255.0)
        ratio = lin / shown if shown > 0 else 0.0
        x = X0 + i * CW
        h = min(56.0, 4.0 * ratio)
        b.append(box(x + 6, BASE - h, CW - 16, h, colour=RED, opacity=0.30))
        b.append(label(x + (CW - 4) / 2, BASE + 16, f"{ratio:.1f}x", "xs mono"))

    b.append(rule(24, 334, 696, 334, "grid", 1.2))
    b.append(label(24, 356, "READ THE BOTTOM ROW RIGHT TO LEFT.", "xs t-hi", "start"))
    b.append(label(24, 374, "Near white the error is 1.1x and looks like nothing. In shadow "
                            "it is 13x. That shape is why four modules of looking at",
                   "xs muted", "start"))
    b.append(label(24, 390, "this picture did not find it: the bright half is nearly right, "
                            "and the dark half reads as a deliberate mood rather than",
                   "xs muted", "start"))
    b.append(label(24, 406, "a defect. An error that is a RATIO hides wherever there is "
                            "least contrast to spare.", "xs muted", "start"))
    b.append(label(24, 432, "Both rows are real downloaded pixels — verify_61 §F renders each "
                            "level into both target formats and prints this table. The",
                   "xs t-hi", "start"))
    b.append(label(24, 448, "swatches ARE the stored codes, so the top row is literally what "
                            "the engine used to display.", "xs muted", "start"))

    return svg(uid, W, H, "A nine-step light ramp rendered into both target formats",
               "For nine linear light levels, the code stored by the pre-6.1 engine and by the "
               "fixed one, with the ratio by which the display was too dark: 12 times in "
               "shadow, 1.2 times near white.", b)


# ===========================================================================
# Figure 6 — one shader, three destinations  (§7)
# ===========================================================================
def fig_targets():
    uid = "l61f6"
    W, H = 720, 400
    b = [label(20, 20, "One fragment shader, one quad, one quantity of light — and three "
                       "places to put it.", "sm", "start")]

    b.append(box(270, 48, 180, 26, colour=AMBER, opacity=0.24))
    b.append(label(360, 65, "lit = 0.5 (light)", "xs mono"))
    b.append(label(360, 90, "scene.frag.hlsl returns this", "xs muted"))

    cases = [
        ("UNORM target", "encode_output = 0", 128, RED,
         "the value verbatim. The display", "reads it as a CODE.", "WRONG"),
        ("_SRGB target", "encode_output = 0", 188, GREEN,
         "the hardware applies the curve", "during the write. Free, exact.", "RIGHT"),
        ("UNORM target", "encode_output = 1", 188, BLUE,
         "the shader applies it itself.", "The fallback, when SDR is all", "RIGHT*"),
    ]
    for i, (dest, flag, code, colour, n1, n2, verdict) in enumerate(cases):
        x = 30 + i * 228
        b.append(arrow(360, 104, x + 100, 130, uid, "s", 1.1) if i != 1
                 else arrow(360, 104, x + 100, 130, uid, "s", 1.1))
        b.append(box(x, 136, 200, 26, colour=colour, opacity=0.20))
        b.append(label(x + 100, 153, dest, "xs mono"))
        b.append(label(x + 100, 176, flag, "xs mono"))

        g = code
        b.append(f'<rect x="{x + 60}" y="{190}" width="80" height="40" '
                 f'fill="rgb({g},{g},{g})" stroke="none"/>')
        b.append(label(x + 100, 248, f"stored code {code}", "xs mono"))
        b.append(label(x + 100, 268, n1, "xs muted"))
        b.append(label(x + 100, 284, n2, "xs muted"))
        b.append(label(x + 100, 306, verdict,
                       "xs t-bad" if verdict == "WRONG" else "xs t-ok"))

    b.append(rule(24, 326, 696, 326, "grid", 1.2))
    b.append(label(24, 348, "* THE TWO RIGHT ANSWERS ARE NOT EQUALLY RIGHT.", "xs t-hi",
                   "start"))
    b.append(label(24, 366, "Hardware blending happens AFTER the fragment shader. Encode in "
                            "the shader and the blender adds codes instead of light —",
                   "xs muted", "start"))
    b.append(label(24, 382, "correct for opaque geometry, wrong the moment anything is "
                            "transparent. Prefer the swapchain; keep the fallback.",
                   "xs muted", "start"))

    return svg(uid, W, H, "The same shader output written to three different targets",
               "Linear 0.5 leaving the fragment shader is stored as code 128 in a UNORM "
               "target, 188 in an _SRGB target, and 188 in a UNORM target when the shader "
               "encodes. Only the last two are correct.", b)


FIGS = {
    "l61_fig1.svg": fig_two_things,   # §2
    "l61_fig2.svg": fig_budget,       # §2
    "l61_fig3.svg": fig_curve,        # §3
    "l61_fig4.svg": fig_pipeline,     # §4
    "l61_fig5.svg": fig_ramp,         # §6
    "l61_fig6.svg": fig_targets,      # §7
}


def main():
    for name, fn in FIGS.items():
        with open(os.path.join(OUT, name), "w") as fh:
            fh.write(fn())
        size = os.path.getsize(os.path.join(OUT, name))
        print(f"wrote {os.path.join(OUT, name)}  ({size:,} bytes)")


if __name__ == "__main__":
    main()
