#!/usr/bin/env python3
"""scratch/figs_618.py — Lesson 6.18's diagrams.

Same rules as 6.1-6.17's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - ~5.2 units per character for `xs`, ~6.0 for `sm`
  - filenames numbered by PAGE ORDER
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - `rule()` takes a CSS CLASS; `cline()` takes a COLOUR (6.10's trap)
  - LEGENDS AND ANNOTATION BOXES GO OUTSIDE THE PLOT (6.11/6.13/6.15's trap)
  - a SHAPE can leave the viewBox where a label cannot

Every number comes from verify_618's output.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)
from figs_610 import cline                                          # noqa: E402

OUT = "scratch"

# ---- measured (verify_618) -------------------------------------------------
CHECKS = 98

# §A — the bake
ASCENT = 12.5509
DESCENT = -3.4491
LINE_GAP = 0.0
LINE_HEIGHT = 16.0
EM_PX = 13.6869
KERN_PAIRS = 186
KERN_TOTAL = 95 * 95

# §A — glyph anatomy (w, h, off_x, off_y, advance)
GLYPHS = {
    "H": (7, 9, 1.0, -9.0, 9.1565),
    "g": (8, 12, 0.0, -8.0, 7.7331),
    "j": (5, 14, -2.0, -10.0, 4.2977),
    ".": (3, 3, 0.0, -2.0, 2.4500),
}

# §B — the atlas
ATLAS = 128
USED_ROWS = 67
PACKED = 7145
EFFICIENCY = 83.3
SHELVES = [(1, 14, 17), (16, 11, 14), (28, 10, 17), (39, 9, 15), (49, 9, 16), (59, 7, 15)]
SOLID = (2, 2)

# §D — the two roundings, every fifth glyph
SNAP_SERIES = [(0, 0.0000, 0.0000), (5, -0.0205, -1.0205), (10, -0.3465, -1.3465),
               (15, -0.4209, -2.4209), (20, -0.2053, -3.2053), (25, 0.4594, -4.5406),
               (30, 0.2429, -4.7571), (35, -0.4765, -4.4765), (40, 0.4217, -4.5783)]
SNAP_WORST = 0.4765
ROUND_WORST = 5.2601
ROUND_FINAL = 4.3216
LINE_GLYPHS = 45

# §E — padding
BLEED_PC = 47.3

# §G — the coverage ramp: (coverage, correct light, encoded light)
RAMP = [(0.0, 0.0000, 0.0000), (0.1, 0.0999, 0.0103), (0.2, 0.2016, 0.0331),
        (0.3, 0.3005, 0.0742), (0.4, 0.4020, 0.1329), (0.5, 0.5029, 0.2159),
        (0.6, 0.5972, 0.3185), (0.7, 0.7011, 0.4508), (0.8, 0.7991, 0.6038),
        (0.9, 0.8963, 0.7913), (1.0, 1.0000, 1.0000)]

# §G — ink mass, as a percentage of correct
ENC_ON_DARK = 62.2
ENC_ON_LIGHT = 137.8
SRGB_ON_DARK = 62.1
SRGB_ON_LIGHT = 62.1
STEM_GAIN = 11.3

# §H — white through the curve
ACES_WHITE = 0.803797
ACES_CODE = 232
EXPOSURE_TABLE = [(0.25, 165), (0.50, 206), (1.00, 232), (2.00, 245), (4.00, 252)]

# §J — the two renderers
DIFFERING = 68
TOTAL_PIXELS = 256 * 64
CONTROL_DIFFERING = 934
UNORM_VS_WRONG = 0
UNORM_VS_RIGHT = 933

# §K
QUADS = 28
VERTEX_BYTES = 2240
INDEX_BYTES = 336
ATLAS_BYTES = 16384


# ===========================================================================
# Figure 1 — the anatomy of a glyph  (§3)
# ===========================================================================
def fig_glyph():
    uid = "l618f2"
    W, H = 760, 518
    b = [label(20, 22, "Two rectangles, and every text bug in this lesson is one of them "
                       "mistaken for the other.", "sm", "start")]

    # A magnified cell: 16 px of line, drawn at S px per pixel. `ox` is far enough
    # right that the longest right-anchored label on the left ("descent line",
    # 12 chars at ~5.2 px) fits; the first draft used ox = 120 and spilled 10 px.
    S = 15
    ox, oy = 178, 104                      # top of the line box
    base_y = oy + ASCENT * S              # the baseline
    desc_y = oy + (ASCENT - DESCENT) * S  # the descender line

    b.append(box(ox, oy, 13 * S, (ASCENT - DESCENT) * S, colour=GREY, opacity=0.07))

    for y, name, cls in ((oy, "ascent line", "muted"),
                         (base_y, "BASELINE", "t-hi"),
                         (desc_y, "descent line", "muted")):
        b.append(rule(ox - 62, y, ox + 13 * S + 24, y, "grid", 1.0,
                      None if name == "BASELINE" else "4 3"))
        b.append(label(ox - 68, y + 4, name, f"xs {cls}", "end"))

    # `j`, and the choice is deliberate. The first draft drew `g`, whose
    # `offset_x` is exactly 0.0 — so the dimension arrow for it had zero length
    # and the label pointed at nothing. `j` is the glyph that shows all three
    # teaching points at once: a NEGATIVE side bearing (ink starts left of the
    # pen), a descender (ink below the baseline), and ink WIDER than its own
    # advance. It is the glyph the caption already has to call out.
    gw, gh, gox, goy, gadv = GLYPHS["j"]
    pen_x = ox + 2 * S
    qx = pen_x + gox * S
    qy = base_y + goy * S

    b.append(box(qx, qy, gw * S, gh * S, colour=AMBER, opacity=0.18))
    b.append(hollow(qx, qy, gw * S, gh * S, AMBER, width=1.4))
    b.append(label(qx + gw * S / 2, qy + gh * S / 2 + 6, "j", "sm t-hi"))

    b.append(hollow(pen_x, oy, gadv * S, (ASCENT - DESCENT) * S, BLUE, dash="5 4", width=1.3))

    # The pen mark, and its label placed LEFT of the line box rather than under
    # it — a label under the pen sits inside the advance rectangle, which
    # check-page.js's text-on-shape check reports and is right to.
    b.append(cline(pen_x, base_y - 11, pen_x, base_y + 11, GREEN, 2.6))
    b.append(rule(pen_x - 4, base_y + 15, ox - 30, base_y + 32, "grid", 1.0))
    b.append(label(ox - 34, base_y + 36, "pen", "xs t-ok", "end"))

    # Dimension arrows, all outside the drawing.
    y_adv = desc_y + 42
    b.append(arrow(pen_x, y_adv, pen_x + gadv * S, y_adv, uid, "s", 1.1))
    b.append(arrow(pen_x + gadv * S, y_adv, pen_x, y_adv, uid, "s", 1.1))
    b.append(label(pen_x + gadv * S / 2, y_adv - 8, f"advance {gadv:.4f} px", "xs mono"))

    y_off = oy - 34
    b.append(rule(pen_x, y_off - 4, pen_x, base_y - 10, "grid", 1.0, "3 3"))
    b.append(rule(qx, y_off - 4, qx, qy - 6, "grid", 1.0, "3 3"))
    b.append(arrow(pen_x, y_off, qx, y_off, uid, "h", 1.3))
    b.append(label((pen_x + qx) / 2, y_off - 10, f"offset_x {gox:+.1f}", "xs mono"))
    b.append(label((pen_x + qx) / 2, y_off + 16, "ink starts LEFT of the pen", "xs t-hi"))

    x_off = ox + 13 * S + 34
    b.append(rule(qx + gw * S, qy, x_off + 8, qy, "grid", 1.0, "3 3"))
    b.append(rule(pen_x + gadv * S, base_y, x_off + 8, base_y, "grid", 1.0, "3 3"))
    b.append(arrow(x_off, base_y, x_off, qy, uid, "s", 1.1))
    b.append(label(x_off + 12, (base_y + qy) / 2, f"offset_y {goy:+.1f}", "xs mono", "start"))

    ly = desc_y + 76
    b.append(box(20, ly - 14, 14, 14, colour=AMBER, opacity=0.18))
    b.append(hollow(20, ly - 14, 14, 14, AMBER, width=1.2))
    b.append(label(42, ly - 3, "INK: the atlas rectangle. Its size is the blit's size.",
                   "xs", "start"))
    b.append(hollow(20, ly + 10, 14, 14, BLUE, dash="4 3", width=1.2))
    b.append(label(42, ly + 21, "ADVANCE: where the pen goes next. A different number.",
                   "xs", "start"))

    b.append(rule(20, ly + 38, W - 20, ly + 38, "grid", 1.2))
    b.append(label(20, ly + 58,
                   "offset_y is NEGATIVE for every glyph with ink above the baseline.",
                   "xs muted", "start"))
    b.append(label(20, ly + 76,
                   "offset_x is negative for 'j': its ink starts LEFT of the pen.",
                   "xs muted", "start"))

    return svg(uid, W, H,
               "The anatomy of one glyph: ink box, advance box, pen and baseline",
               "A magnified line of text showing the letter g placed by its metrics. The "
               "amber rectangle is the atlas ink; the dashed blue rectangle is the advance "
               "box; the pen sits on the baseline and the offsets carry it to the ink.",
               b)


# ===========================================================================
# Figure 2 — the atlas, and what measuring it wrongly looks like  (§5)
# ===========================================================================
def fig_atlas():
    uid = "l618f4"
    W, H = 760, 478
    b = [label(20, 22, "Six shelves, tallest first, and the number that looks like a verdict "
                       "on the packer but is not.", "sm", "start")]

    # The atlas, at 2.2 px per texel.
    S = 2.2
    ox, oy = 40, 56
    side = ATLAS * S
    b.append(box(ox, oy, side, side, colour=GREY, opacity=0.06))
    b.append(hollow(ox, oy, side, side, GREY, width=1.1))

    colours = [BLUE, AMBER, GREEN, PURPLE, BLUE, AMBER]
    for i, (y, h, n) in enumerate(SHELVES):
        b.append(box(ox + S, oy + y * S, side - 2 * S, h * S,
                     colour=colours[i % len(colours)], opacity=0.22))
        b.append(label(ox + side + 12, oy + (y + h / 2) * S + 4,
                       f"{h} px tall, {n} glyphs", "xs mono", "start"))

    # The solid block.
    b.append(box(ox + SOLID[0] * S - 2 * S, oy + SOLID[1] * S - 2 * S, 4 * S, 4 * S,
                 colour=RED, opacity=0.9))
    b.append(rule(ox + 4 * S, oy + 4 * S, ox + side + 8, oy - 14, "grid", 1.0))
    b.append(label(ox + side + 12, oy - 10, "the 2x2 solid block", "xs t-hi", "start"))

    # The unused rows.
    b.append(rule(ox, oy + USED_ROWS * S, ox + side, oy + USED_ROWS * S, "grid", 1.4))
    b.append(label(ox + side / 2, oy + (USED_ROWS + (ATLAS - USED_ROWS) / 2) * S + 4,
                   f"{ATLAS - USED_ROWS} rows the packer never reached", "xs muted"))

    # The two measurements, side by side, OUTSIDE the picture.
    my = oy + side + 46
    b.append(rule(20, my - 24, W - 20, my - 24, "grid", 1.2))

    naive = 100.0 * PACKED / (ATLAS * ATLAS)
    b.append(box(20, my - 12, 350, 62, colour=RED, opacity=0.09))
    b.append(label(32, my + 6, f'"occupancy" = {PACKED} / {ATLAS}x{ATLAS} = {naive:.1f}%',
                   "xs mono", "start"))
    b.append(label(32, my + 24, "The denominator is a power of two, fixed before the",
                   "xs t-bad", "start"))
    b.append(label(32, my + 38, "packer runs. A perfect packer scores the same.",
                   "xs t-bad", "start"))

    b.append(box(392, my - 12, 348, 62, colour=GREEN, opacity=0.09))
    b.append(label(404, my + 6,
                   f"shelf efficiency = {PACKED} / {ATLAS}x{USED_ROWS} = {EFFICIENCY:.1f}%",
                   "xs mono", "start"))
    b.append(label(404, my + 24, "Of the rows it reached for, how much did it use?",
                   "xs t-ok", "start"))
    b.append(label(404, my + 38, "Both halves now depend on what the packer did.",
                   "xs t-ok", "start"))

    b.append(label(20, my + 76,
                   f"The atlas is {ATLAS} square because {PACKED} texels of glyph will not "
                   f"fit in 64x64 = {64 * 64}. That is a fact about the FONT.",
                   "xs muted", "start"))

    return svg(uid, W, H,
               "The glyph atlas: six shelves, a solid block, and two ways to measure the packer",
               "A 128 by 128 atlas with six horizontal shelves of decreasing height, a small "
               "red solid block at the top left, and 61 unused rows at the bottom. Below, two "
               "boxed measurements: a misleading occupancy figure and the shelf efficiency "
               "that actually measures the packer.",
               b)


# ===========================================================================
# Figure 3 — the error that cancels and the error that accumulates  (§4)
# ===========================================================================
def fig_snap():
    uid = "l618f3"
    W, H = 760, 400
    b = [label(20, 22, "Round the position and the error is bounded forever. Round the "
                       "advance and it is a random walk.", "sm", "start")]

    ox, oy = 70, 60
    pw, ph = W - ox - 190, 220
    y_lo, y_hi = -6.0, 1.0

    def px(i):
        return ox + pw * i / LINE_GLYPHS

    def py(v):
        return oy + ph * (y_hi - v) / (y_hi - y_lo)

    # Axes and the zero line.
    b.append(rule(ox, oy, ox, oy + ph, "grid", 1.1))
    b.append(rule(ox, py(0.0), ox + pw, py(0.0), "grid", 1.2))
    for v in (1, 0, -1, -2, -3, -4, -5, -6):
        b.append(label(ox - 10, py(v) + 4, f"{v:+d}", "xs mono", "end"))
    b.append(label(ox - 52, oy - 14, "placement error, px", "xs muted", "start"))
    b.append(label(ox + pw / 2, oy + ph + 26, "glyph index along one 45-character line",
                   "xs muted"))

    # The half-pixel band, which is the whole claim about snapping.
    b.append(box(ox, py(0.5), pw - 6, py(-0.5) - py(0.5), colour=GREEN, opacity=0.12))
    b.append(label(ox + pw + 14, py(0.0) + 18, "+/- 0.5 px", "xs t-ok", "start"))

    for i in range(len(SNAP_SERIES) - 1):
        n0, s0, r0 = SNAP_SERIES[i]
        n1, s1, r1 = SNAP_SERIES[i + 1]
        b.append(cline(px(n0), py(s0), px(n1), py(s1), GREEN, 1.8))
        b.append(cline(px(n0), py(r0), px(n1), py(r1), RED, 1.8))
    for n, s, r in SNAP_SERIES:
        b.append(f'<circle cx="{px(n):.1f}" cy="{py(s):.1f}" r="2.6" fill="{GREEN}"/>')
        b.append(f'<circle cx="{px(n):.1f}" cy="{py(r):.1f}" r="2.6" fill="{RED}"/>')

    last = SNAP_SERIES[-1]
    b.append(label(px(last[0]) + 14, py(last[1]) - 6, "snap the POSITION", "xs t-ok", "start"))
    b.append(label(px(last[0]) + 14, py(last[2]) + 4, "round the ADVANCE", "xs t-bad", "start"))

    # The conclusion, outside the plot.
    cy = oy + ph + 54
    b.append(rule(20, cy - 18, W - 20, cy - 18, "grid", 1.2))
    b.append(label(20, cy,
                   f"Snapping the position: worst error {SNAP_WORST:.4f} px, and it can "
                   "never exceed 0.5 — the pen keeps its fraction.",
                   "xs t-ok", "start"))
    b.append(label(20, cy + 18,
                   f"Rounding the advance: worst {ROUND_WORST:.4f} px, still "
                   f"{ROUND_FINAL:.4f} px out at the end of the line. Each error is added "
                   "to the next.", "xs t-bad", "start"))
    b.append(label(20, cy + 40,
                   "Same rounding, applied to two different quantities, with completely "
                   "different consequences.", "xs muted", "start"))

    return svg(uid, W, H,
               "Two roundings: a bounded error and an accumulating one",
               "A plot of placement error in pixels against glyph index over one line of "
               "text. The green series, from snapping each glyph's position, stays inside a "
               "shaded half-pixel band. The red series, from rounding each advance, wanders "
               "down to nearly five pixels and does not return.",
               b)


# ===========================================================================
# Figure 4 — the coverage ramp, and the two bugs that look the same  (§7)
# ===========================================================================
def fig_gamma():
    uid = "l618f5"
    W, H = 760, 376
    b = [label(20, 22, "Compositing coverage in the wrong space, measured one pixel at a "
                       "time and one string at a time.", "sm", "start")]

    ox, oy = 70, 58
    pw, ph = 330, 210

    def px(a):
        return ox + pw * a

    def py(v):
        return oy + ph * (1.0 - v)

    b.append(rule(ox, oy, ox, oy + ph, "grid", 1.1))
    b.append(rule(ox, oy + ph, ox + pw, oy + ph, "grid", 1.1))
    for v in (0.0, 0.25, 0.5, 0.75, 1.0):
        b.append(label(ox - 10, py(v) + 4, f"{v:.2f}", "xs mono", "end"))
        b.append(label(px(v), oy + ph + 18, f"{v:.2f}", "xs mono"))
    b.append(label(ox - 46, oy - 14, "light emitted", "xs muted", "start"))
    b.append(label(ox + pw / 2, oy + ph + 38, "glyph coverage", "xs muted"))

    for i in range(len(RAMP) - 1):
        a0, r0, w0 = RAMP[i]
        a1, r1, w1 = RAMP[i + 1]
        b.append(cline(px(a0), py(r0), px(a1), py(r1), GREEN, 2.0))
        b.append(cline(px(a0), py(w0), px(a1), py(w1), RED, 2.0))

    # The worst case is at the FAINT end, which is where a glyph's edge lives.
    # The label is placed BESIDE the bracket rather than on it: the first draft
    # centred it on the dashed line and check-page.js's text-on-shape check
    # reported it, which is exactly what that check is for.
    a, right, wrong = RAMP[1]
    b.append(cline(px(a), py(right), px(a), py(wrong), AMBER, 1.4, "3 3"))
    # The label goes UP into the empty upper-left of the plot, not down: below the
    # bracket is the tick row, and the first attempt to move it off the dashed
    # line put it straight on top of "0.25" and "0.50". check-page.js has a
    # separate check for each, which is why both were caught.
    # The label goes into the empty upper-left of the plot. Below the bracket is
    # the tick row and level with it is the correct-light line, so the first two
    # placements landed on "0.25"/"0.50" and then on the green curve itself.
    # check-page.js has a separate check for each of those, which is why both
    # were caught rather than one hiding behind the other.
    b.append(rule(px(a), py(wrong), px(a) + 6, py(0.84), "grid", 1.0))
    b.append(label(px(0.06), py(0.88),
                   f"at 10% coverage: {100.0 * wrong / right:.1f}% of the light",
                   "xs t-hi", "start"))

    b.append(label(px(0.72), py(0.86), "correct (linear)", "xs t-ok"))
    b.append(label(px(0.78), py(0.42), "sRGB codes", "xs t-bad"))

    # The diagnostic table, entirely outside the plot.
    tx, ty = 452, 70
    b.append(label(tx, ty - 16, "INK MASS, AS A PERCENTAGE OF CORRECT", "xs", "start"))
    cols = [("", 0), ("on dark", 132), ("on light", 212)]
    for name, dx in cols:
        b.append(label(tx + dx, ty + 4, name, "xs muted", "start"))
    rows = [("encoded blending", ENC_ON_DARK, ENC_ON_LIGHT, RED),
            ("_SRGB atlas", SRGB_ON_DARK, SRGB_ON_LIGHT, PURPLE)]
    for i, (name, dark, light, colour) in enumerate(rows):
        yy = ty + 28 + i * 30
        b.append(box(tx - 8, yy - 14, 282, 24, colour=colour, opacity=0.10))
        b.append(label(tx, yy + 2, name, "xs mono", "start"))
        b.append(label(tx + 132, yy + 2, f"{dark:.1f}%", "xs mono", "start"))
        b.append(label(tx + 212, yy + 2, f"{light:.1f}%", "xs mono", "start"))

    b.append(label(tx - 8, ty + 106,
                   "Two bugs, in two different files, agree", "xs muted", "start"))
    b.append(label(tx - 8, ty + 122,
                   "exactly on dark. They are the same", "xs muted", "start"))
    b.append(label(tx - 8, ty + 138,
                   "function applied in two places.", "xs muted", "start"))
    b.append(label(tx - 8, ty + 162,
                   "The light column tells them apart:", "xs t-hi", "start"))
    b.append(label(tx - 8, ty + 178,
                   "the blend FATTENS, the atlas THINS.", "xs t-hi", "start"))

    cy = oy + ph + 66
    b.append(rule(20, cy - 18, W - 20, cy - 18, "grid", 1.2))
    b.append(label(20, cy,
                   "The error is worst where the coverage is LOWEST, and an antialiased "
                   "glyph at 16 px is mostly low coverage.", "xs", "start"))
    b.append(label(20, cy + 18,
                   "That is why this shows up as stem weight rather than as a colour shift: "
                   "it is the edges that are wrong.", "xs muted", "start"))

    return svg(uid, W, H,
               "Coverage composited in linear light and in sRGB codes",
               "A plot of emitted light against glyph coverage with two curves: a straight "
               "line for correct linear compositing and a curve bowing far below it for "
               "blending sRGB codes, worst at low coverage. Beside it, a table of ink mass "
               "showing that encoded blending and an sRGB atlas agree on dark backgrounds "
               "and disagree on light ones.",
               b)


# ===========================================================================
# Figure 5 — where in the frame the overlay goes  (§8)
# ===========================================================================
def fig_where():
    uid = "l618f6"
    W, H = 760, 324
    b = [label(20, 22, "The same white text, composited at two points in one frame.",
               "sm", "start")]

    stages = [("scene", BLUE), ("bloom", AMBER), ("tonemap", PURPLE), ("swapchain", GREEN)]
    ox, oy = 40, 86
    bw, gap = 138, 34
    for i, (name, colour) in enumerate(stages):
        x = ox + i * (bw + gap)
        b.append(box(x, oy, bw, 40, colour=colour, opacity=0.16))
        b.append(hollow(x, oy, bw, 40, colour, width=1.2))
        b.append(label(x + bw / 2, oy + 25, name, "xs mono"))
        if i + 1 < len(stages):
            b.append(arrow(x + bw + 4, oy + 20, x + bw + gap - 4, oy + 20, uid, "s", 1.2))
    b.append(label(ox + bw, oy - 14, "HDR float, scene-referred", "xs muted"))
    b.append(label(ox + 3 * (bw + gap) + bw / 2, oy - 14, "8-bit sRGB, display-referred",
                   "xs muted"))

    # The two insertion points.
    x_before = ox + 1.0 * (bw + gap) - gap / 2
    x_after = ox + 3 * (bw + gap) - gap / 2
    for x, colour, tag in ((x_before, RED, "BEFORE"), (x_after, GREEN, "AFTER")):
        b.append(cline(x, oy - 4, x, oy + 44, colour, 2.0, "4 3"))
        b.append(label(x, oy + 62, tag, "xs t-hi"))

    # What each costs.
    cy = oy + 92
    b.append(box(30, cy, 340, 96, colour=RED, opacity=0.09))
    b.append(label(44, cy + 20, "composited BEFORE the tonemap", "xs t-bad", "start"))
    b.append(label(44, cy + 40, f"white at linear 1.0 -> ACES -> {ACES_WHITE:.4f}",
                   "xs mono", "start"))
    b.append(label(44, cy + 58, f"-> sRGB code {ACES_CODE}, not 255", "xs mono", "start"))
    b.append(label(44, cy + 78, "and it blooms, and it moves with the exposure",
                   "xs muted", "start"))

    b.append(box(392, cy, 340, 96, colour=GREEN, opacity=0.09))
    b.append(label(406, cy + 20, "composited AFTER the tonemap", "xs t-ok", "start"))
    b.append(label(406, cy + 40, "white is code 255, and stays code 255", "xs mono", "start"))
    b.append(label(406, cy + 58, "the sRGB target's blender works in linear light",
                   "xs mono", "start"))
    b.append(label(406, cy + 78, "a UI colour is a CODE, not a quantity of light",
                   "xs muted", "start"))

    # The exposure table, under the left box, as a strip.
    ey = cy + 122
    b.append(label(30, ey, "the same white, as the auto-exposure moves:", "xs muted", "start"))
    for i, (e, code) in enumerate(EXPOSURE_TABLE):
        x = 300 + i * 88
        b.append(label(x, ey, f"x{e:g} -> {code}", "xs mono", "start"))

    return svg(uid, W, H,
               "Where the overlay composites: before the tonemap or after it",
               "The frame as four stages from scene to swapchain, with two dashed insertion "
               "points marked. Compositing before the tonemap turns white into sRGB code 232 "
               "and makes it move with the exposure; compositing after leaves it at 255.",
               b)


# ===========================================================================
# Figure 6 — the overlay as a frame-graph pass  (§9)
# ===========================================================================
def fig_pass():
    uid = "l618f7"
    W, H = 760, 298
    b = [label(20, 22, "One word from the pass author; four facts deduced by the graph.",
               "sm", "start")]

    # The version chain along the top.
    ys = 86
    versions = [("backbuffer@0", GREY, "whatever was there"),
                ("backbuffer@1", BLUE, "the resolve wrote it"),
                ("backbuffer@2", GREEN, "the overlay blended onto it")]
    ox, bw, gap = 44, 190, 46
    for i, (name, colour, note) in enumerate(versions):
        x = ox + i * (bw + gap)
        b.append(box(x, ys, bw, 34, colour=colour, opacity=0.16))
        b.append(hollow(x, ys, bw, 34, colour, width=1.2))
        b.append(label(x + bw / 2, ys + 22, name, "xs mono"))
        b.append(label(x + bw / 2, ys + 50, note, "xs muted"))

    passes = [("resolve", 0), ("overlay", 1)]
    for name, i in passes:
        x0 = ox + i * (bw + gap) + bw
        x1 = x0 + gap
        b.append(arrow(x0 + 4, ys + 17, x1 - 4, ys + 17, uid, "h", 1.4))
        b.append(label((x0 + x1) / 2, ys - 10, name, "xs t-hi"))

    # The declaration.
    dy = ys + 88
    b.append(rule(20, dy - 12, W - 20, dy - 12, "grid", 1.2))
    b.append(label(20, dy + 10, "WHAT THE OVERLAY SAYS", "xs", "start"))
    b.append(box(20, dy + 20, 330, 30, colour=GREEN, opacity=0.12))
    b.append(label(34, dy + 40, "fg.keep(overlay_pass, back1)", "xs mono", "start"))
    b.append(label(20, dy + 68,
                   "\"my output depends on what was already", "xs muted", "start"))
    b.append(label(20, dy + 84, "in this target\" — which for a blend is", "xs muted", "start"))
    b.append(label(20, dy + 100, "simply true: dst is an operand of `over`.",
                   "xs muted", "start"))

    b.append(label(392, dy + 10, "WHAT THE GRAPH DEDUCES", "xs", "start"))
    facts = [("the ORDER", "the overlay follows the resolve"),
             ("the LOAD op", "LOAD, because the old contents matter"),
             ("the STORE op", "the RESOLVE must STORE — a different pass"),
             ("the LIFETIME", "the imported target lives to the end")]
    for i, (name, note) in enumerate(facts):
        yy = dy + 30 + i * 24
        b.append(box(392, yy - 12, 118, 18, colour=BLUE, opacity=0.12, rx=3))
        b.append(label(451, yy + 1, name, "xs"))
        b.append(label(522, yy + 1, note, "xs muted", "start"))

    return svg(uid, W, H,
               "The overlay declared as a frame-graph pass, and the four facts that follow",
               "Three versions of the imported backbuffer resource in a chain, produced by "
               "the resolve pass and then the overlay pass. Below, the single keep() call the "
               "overlay makes, and the four facts the graph derives from it: the order, the "
               "load op, the previous pass's store op, and the lifetime.",
               b)


# ===========================================================================
# Figure 7 — three files, two renderers, one batch  (§2)
# ===========================================================================
def fig_split():
    uid = "l618f1"
    W, H = 760, 292
    b = [label(20, 22, "The split that makes the comparison in §11 possible at all.",
               "sm", "start")]

    ox, oy, bw, bh = 30, 62, 176, 54
    chain = [("font.hpp", "outlines -> coverage", AMBER),
             ("overlay.hpp", "strings -> quads", BLUE)]
    for i, (name, note, colour) in enumerate(chain):
        x = ox + i * (bw + 48)
        b.append(box(x, oy, bw, bh, colour=colour, opacity=0.16))
        b.append(hollow(x, oy, bw, bh, colour, width=1.2))
        b.append(label(x + bw / 2, oy + 24, name, "xs mono"))
        b.append(label(x + bw / 2, oy + 42, note, "xs muted"))
        if i == 0:
            b.append(arrow(x + bw + 6, oy + bh / 2, x + bw + 42, oy + bh / 2, uid, "s", 1.2))

    # The fork. Both consumers sit to the right of the second box and INSIDE the
    # viewBox — the first draft put them at x = 576 with a width of 224, which is
    # 40 px past the right edge, and check-page.js said so.
    fx = ox + (bw + 48) + bw
    cw = W - fx - 60
    consumers = [("composite_overlay", "a framebuffer, on the CPU", GREEN, -46),
                 ("gpu_overlay", "SDL_GPU, one draw call", PURPLE, 46)]
    for name, note, colour, dy in consumers:
        x = fx + 44
        y = oy + dy
        b.append(arrow(fx + 6, oy + bh / 2, x - 6, y + bh / 2, uid, "s", 1.2))
        b.append(box(x, y, cw, bh, colour=colour, opacity=0.16))
        b.append(hollow(x, y, cw, bh, colour, width=1.2))
        b.append(label(x + cw / 2, y + 22, name, "xs mono"))
        b.append(label(x + cw / 2, y + 40, note, "xs muted"))

    cy = 230
    b.append(rule(20, cy - 18, W - 20, cy - 18, "grid", 1.2))
    b.append(label(20, cy,
                   "Neither of the first two mentions SDL_GPU, which is why `hello_cube` — "
                   "a program with no GPU device — has text.", "xs", "start"))
    b.append(label(20, cy + 20,
                   f"And it is why the same batch can be rendered both ways and compared: "
                   f"{DIFFERING} of {TOTAL_PIXELS} pixels differ, by at most one sRGB code.",
                   "xs t-ok", "start"))
    b.append(label(20, cy + 40,
                   f"The control — the same comparison against a deliberately wrong CPU "
                   f"render — reports {CONTROL_DIFFERING} differing pixels.",
                   "xs muted", "start"))

    return svg(uid, W, H,
               "Three files: what each one knows about",
               "A chain from font.hpp to overlay.hpp forking into two consumers, the CPU "
               "compositor and the SDL_GPU overlay. Neither of the first two mentions "
               "SDL_GPU, so the same batch can be rendered by both and compared.",
               b)


# PAGE ORDER, and the filenames follow it. The first draft numbered these by the
# order they were written — the split diagram was fig 7 and appeared first — and
# check-page.js's figOrder check reported every one of them. A reader counts
# figures as they meet them.
FIGURES = [
    ("l618_fig1.svg", fig_split),
    ("l618_fig2.svg", fig_glyph),
    ("l618_fig3.svg", fig_snap),
    ("l618_fig4.svg", fig_atlas),
    ("l618_fig5.svg", fig_gamma),
    ("l618_fig6.svg", fig_where),
    ("l618_fig7.svg", fig_pass),
]

if __name__ == "__main__":
    for name, fn in FIGURES:
        path = os.path.join(OUT, name)
        with open(path, "w", encoding="utf-8") as fh:
            fh.write(fn())
        print(f"wrote {path}")
