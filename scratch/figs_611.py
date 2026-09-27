#!/usr/bin/env python3
"""scratch/figs_611.py — Lesson 6.11's diagrams.

Same rules as 6.1-6.10's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - ~5.2 units per character for `xs`, ~6.0 for `sm`
  - filenames numbered by PAGE ORDER
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - `rule()` takes a CSS CLASS; `cline()` takes a COLOUR (6.10's trap)

Every number comes from verify_611's output.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)
from figs_610 import cline                                          # noqa: E402

OUT = "scratch"

# ---- measured (verify_611) -------------------------------------------------
CODE_CORRECT = 188       # §B — half-coverage white over black, in linear light
CODE_WRONG = 128         # §B — the same lerp on stored bytes
LIGHT_CORRECT = 0.5029   # §B
LIGHT_WRONG = 0.2159     # §B
DELIVERS = 42.9          # §B, per cent
WORST_LOSS = 0.2898      # §B, of full white
WORST_AT = 0.55          # §B
GPU_SRGB = 188           # §H — the hardware, on an _SRGB target
GPU_UNORM = 127          # §H — the hardware, on a UNORM target
GPU_DELIVERS = 42.2      # §H, per cent
EDGE_STRAIGHT = 0.2636   # §C — the filtered edge's green, straight alpha
EDGE_INTENDED = 0.5271   # §C — what the leaf actually is
COV_BASE = 0.3635        # §E
COV_PLAIN = [0.3635, 0.3665, 0.3662, 0.3672, 0.3438, 0.3750, 0.2500]
COV_KEPT = [0.3635, 0.3611, 0.3623, 0.3672, 0.3750, 0.3750, 0.2500]
COV_LOST_PCT = 31.2      # §E, at level 6
BLEND_NS = 68.87         # §G
ENCODED_NS = 21.94       # §G
MASK_VISIBLE = 1.05      # §G, x opaque
CHECKS = 49


def srgb_to_linear(c):
    """The exact transform, so the figures' light values match the engine's."""
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def linear_to_srgb(v):
    v = max(0.0, min(1.0, v))
    return 12.92 * v if v <= 0.0031308 else 1.055 * (v ** (1 / 2.4)) - 0.055


# ===========================================================================
# Figure 1 — the three modes, and what each does to the depth buffer   (§2)
# ===========================================================================
def fig_modes():
    W, H = 720, 400
    b = [label(W / 2, 22, "One glTF enum, three different machines", "sm")]

    names = ["OPAQUE", "MASK", "BLEND"]
    cols = [GREY, GREEN, BLUE]
    sub = ["alpha ignored", "alpha is a TEST", "alpha is a FRACTION"]
    lines = [
        ["test depth", "write depth", "write colour"],
        ["test depth", "SHADE FIRST", "discard, or", "write depth", "write colour"],
        ["test depth", "NEVER write depth", "read dst", "composite", "write colour"],
    ]
    verdict = [
        ["order-free", "(the z-buffer", "IS the sort)"],
        ["order-free", "(a discarded", "fragment", "occludes nothing)"],
        ["ORDER MATTERS", "(`over` is not", "commutative:", "sort back to front)"],
    ]
    cost = ["0 pipelines", "0 pipelines", "+6 pipelines"]

    x0, wcol, gap = 56, 190, 25
    ytop = 50
    for i in range(3):
        x = x0 + i * (wcol + gap)
        b.append(hollow(x, ytop, wcol, 300, cols[i], rx=6))
        b.append(box(x, ytop, wcol, 34, cols[i], rx=6, opacity=0.18))
        b.append(label(x + wcol / 2, ytop + 23, names[i], "sm"))
        b.append(label(x + wcol / 2, ytop + 54, sub[i], "xs"))

        y = ytop + 82
        for ln in lines[i]:
            hi = ln.isupper() or ln.startswith("NEVER")
            b.append(label(x + 14, y, ("> " if not hi else "! ") + ln, "xs", "start"))
            y += 19

        b.append(rule(x + 12, ytop + 196, x + wcol - 12, ytop + 196, "grid", 1.0))
        y = ytop + 216
        for ln in verdict[i]:
            b.append(label(x + wcol / 2, y, ln, "xs"))
            y += 17

        b.append(rule(x + 12, ytop + 274, x + wcol - 12, ytop + 274, "grid", 1.0))
        b.append(label(x + wcol / 2, ytop + 291, cost[i], "xs"))

    b.append(label(W / 2, H - 14,
                   "the line 6.5 drew — which half can be a number in a buffer? — "
                   "runs between MASK and BLEND", "xs"))
    return svg("f611a", W, H, "The three alpha modes",
               "Opaque, mask and blend as three different sequences of depth and "
               "colour operations, with the pipeline cost of each.", b)


# ===========================================================================
# Figure 2 — over, from coverage                                        (§3)
# ===========================================================================
def fig_over():
    W, H = 720, 330
    b = [label(W / 2, 22, "`over` is an area-weighted average, which is why it is a lerp", "sm")]

    # A single pixel, split by coverage.
    px, py, side = 70, 60, 150
    a = 0.6
    b.append(box(px, py, side, side, BLUE, rx=2, opacity=0.30))
    b.append(box(px, py, side * a, side, RED, rx=0, opacity=0.55))
    b.append(hollow(px, py, side, side, GREY, rx=2))
    b.append(label(px + side * a / 2, py + side / 2 + 4, "src", "xs"))
    b.append(label(px + side * a + (side - side * a) / 2, py + side / 2 + 4, "dst", "xs"))
    b.append(label(px + side / 2, py + side + 22, "one pixel", "xs"))
    b.append(label(px + side * a / 2, py - 10, f"a = {a:g}", "xs"))
    b.append(label(px + side * a + (side - side * a) / 2, py - 10, f"1 - a = {1 - a:.1f}", "xs"))

    # The formula, spelt as areas.
    fx = 290
    b.append(label(fx, py + 26, "result = src x a  +  dst x (1 - a)", "sm", "start"))
    b.append(label(fx, py + 50, "…and both operands must be LIGHT, not stored codes,", "xs", "start"))
    b.append(label(fx, py + 68, "because an average of encoded values is not an", "xs", "start"))
    b.append(label(fx, py + 86, "average of what they encode (Lesson 1.6, for the", "xs", "start"))
    b.append(label(fx, py + 104, "fourth time — and this time it is a whole feature).", "xs", "start"))

    # The worked case.
    b.append(rule(fx, py + 124, W - 40, py + 124, "grid", 1.0))
    b.append(label(fx, py + 146, "white over black, a = 0.5:", "xs", "start"))
    b.append(label(fx + 8, py + 168, f"in light   1.0 x 0.5 + 0.0 x 0.5  =  0.5", "xs", "start"))
    b.append(label(fx + 8, py + 186,
                   f"stored     code {CODE_CORRECT}   <- half the light", "xs", "start"))
    b.append(label(fx + 8, py + 210,
                   f"the byte lerp gives code {CODE_WRONG}, which emits {LIGHT_WRONG:.4f}", "xs", "start"))
    b.append(label(fx + 8, py + 228,
                   f"—  {DELIVERS:.1f}% of the light the composite should have", "xs", "start"))

    b.append(label(W / 2, H - 14,
                   f"the two integers, again: half the LIGHT is code {CODE_CORRECT}; "
                   f"half the CODE is code {CODE_WRONG}", "xs"))
    return svg("f611b", W, H, "The over operator",
               "A pixel divided into a source fraction and a destination fraction, "
               "with the worked composite of white over black at half coverage.", b)


# ===========================================================================
# Figure 3 — the whole sweep, correct against wrong                     (§3)
# ===========================================================================
def swatch(b, x, y, colour, dash=None):
    """A short line of the curve's own colour, for a legend row."""
    b.append(cline(x, y, x + 26, y, colour, 2.2, dash=dash))


def fig_space():
    W, H = 720, 392
    b = [label(W / 2, 22, "What the composite delivers, swept over coverage", "sm")]

    # THE PLOT IS NARROW AND THE ANNOTATIONS LIVE OUTSIDE IT. The first draft put
    # them in the empty regions above and below the curves; `check-page.js`
    # reported five texts sitting on a polyline and one colliding with the axis
    # label, which is what happens when "empty" is judged by eye on a diagram
    # whose two curves between them cross most of the box.
    x0, x1 = 92, 452
    y0, y1 = 286, 62

    def px(a):
        return x0 + (x1 - x0) * a

    def py(v):
        return y0 + (y1 - y0) * v

    b.append(rule(x0, y0, x1, y0, "grid", 1.0))
    b.append(rule(x0, y0, x0, y1, "grid", 1.0))
    for a in (0.0, 0.25, 0.5, 0.75, 1.0):
        b.append(rule(px(a), y0, px(a), y0 + 5, "grid", 1.0))
        b.append(label(px(a), y0 + 20, f"{a:g}", "xs"))
    for v in (0.0, 0.5, 1.0):
        b.append(rule(x0 - 5, py(v), x0, py(v), "grid", 1.0))
        b.append(label(x0 - 12, py(v) + 4, f"{v:g}", "xs", "end"))
    b.append(label((x0 + x1) / 2, y0 + 40, "coverage  a", "xs"))
    b.append(label(x0 - 44, (y0 + y1) / 2, "light", "xs"))

    # correct: a straight line in light
    pts = " ".join(f"{px(i / 40):.1f},{py(i / 40):.1f}" for i in range(41))
    b.append(f'<polyline points="{pts}" fill="none" stroke="{GREEN}" stroke-width="2.2"/>')

    # wrong: lerp the STORED values, then read the result back as light
    wpts = []
    for i in range(41):
        a = i / 40
        wpts.append(f"{px(a):.1f},{py(srgb_to_linear(a)):.1f}")
    b.append(f'<polyline points="{" ".join(wpts)}" fill="none" stroke="{RED}" '
             f'stroke-width="2.2" stroke-dasharray="6 4"/>')

    # the worst gap, marked in the plot and explained in the legend
    ga = WORST_AT
    b.append(cline(px(ga), py(srgb_to_linear(ga)), px(ga), py(ga), AMBER, 1.8, dash="4 3"))

    # ---- the legend, clear of every stroke --------------------------------
    lx = 492
    ly = 92
    swatch(b, lx, ly, GREEN)
    b.append(label(lx + 36, ly + 4, "in LINEAR light", "xs", "start"))
    b.append(label(lx + 36, ly + 22, "a straight line, because", "xs", "start"))
    b.append(label(lx + 36, ly + 40, "coverage is linear in area", "xs", "start"))

    ly = 168
    swatch(b, lx, ly, RED, dash="6 4")
    b.append(label(lx + 36, ly + 4, "on the STORED BYTES", "xs", "start"))
    b.append(label(lx + 36, ly + 22, "the sRGB curve, arrived at", "xs", "start"))
    b.append(label(lx + 36, ly + 40, "entirely by accident", "xs", "start"))

    ly = 244
    swatch(b, lx, ly, AMBER, dash="4 3")
    b.append(label(lx + 36, ly + 4, f"the gap: {WORST_LOSS:.4f} of full", "xs", "start"))
    b.append(label(lx + 36, ly + 22, f"white, worst at a = {ga:g}", "xs", "start"))

    b.append(label(W / 2, H - 34,
                   "EXACT AT BOTH ENDS, worst in the middle — which is why a fade that starts "
                   "and finishes correctly still looks like a fade,", "xs"))
    b.append(label(W / 2, H - 14,
                   "and why this bug survives review. The gap is the whole of what compositing "
                   "in the wrong space costs.", "xs"))
    return svg("f611c", W, H, "Correct and incorrect compositing, swept",
               "Two curves of delivered light against coverage: a straight line for "
               "linear-light compositing and the sRGB curve for byte compositing, with "
               "the widest gap marked.", b)


# ===========================================================================
# Figure 4 — premultiplied alpha and the halo                           (§5)
# ===========================================================================
def fig_premul():
    W, H = 720, 400
    b = [label(W / 2, 22, "Why a cutout gets a dark outline, and what removes it", "sm")]

    # Four texels being filtered: two opaque green, two transparent black.
    def panel(x, y, title, texels, note1, note2, result, resnote):
        b.append(label(x + 96, y, title, "sm"))
        s = 44
        for i, (col, alpha, cap) in enumerate(texels):
            tx = x + i * (s + 6)
            b.append(box(tx, y + 16, s, s, col, rx=2,
                         opacity=0.95 if alpha else 0.16))
            b.append(label(tx + s / 2, y + 16 + s / 2 + 4, cap, "xs"))
        b.append(label(x + 96, y + 82, note1, "xs"))
        b.append(label(x + 96, y + 100, note2, "xs"))
        b.append(arrow(x + 96, y + 108, x + 96, y + 130, "f611d", "s", 1.2))
        b.append(box(x + 96 - s / 2, y + 134, s, s, result, rx=2, opacity=0.95))
        b.append(label(x + 96, y + 134 + s + 18, resnote, "xs"))

    g = "#4E9A3C"
    dark = "#2b4a24"
    panel(70, 56, "STRAIGHT alpha",
          [(g, True, "leaf"), (g, True, "leaf"), ("#000000", False, "clear"),
           ("#000000", False, "clear")],
          "average the colours independently:",
          "(green + green + black + black) / 4",
          dark,
          f"green {EDGE_STRAIGHT:.4f} — HALF the leaf's {EDGE_INTENDED:.4f}")

    panel(420, 56, "PREMULTIPLIED",
          [(g, True, "leaf"), (g, True, "leaf"), ("#000000", False, "0,0,0,0"),
           ("#000000", False, "0,0,0,0")],
          "weight each by its own coverage:",
          "(g*1 + g*1 + 0*0 + 0*0) / (1+1+0+0)",
          g,
          f"green {EDGE_INTENDED:.4f} — the leaf, intact")

    b.append(rule(60, 296, W - 60, 296, "grid", 1.0))
    b.append(label(W / 2, 320,
                   "The transparent texels are BLACK, because that is what an image editor "
                   "leaves in a cleared region.", "xs"))
    b.append(label(W / 2, 340,
                   "A straight-alpha filter has no way to know they are invisible, so it "
                   "averages that black in at full weight —", "xs"))
    b.append(label(W / 2, 360,
                   "and the darkening is proportional to how transparent the neighbours "
                   "are, which is a halo that follows every edge.", "xs"))
    b.append(label(W / 2, 382,
                   "Premultiplied storage is the representation in which averaging is "
                   "already the right operation.", "xs"))
    return svg("f611d", W, H, "Straight versus premultiplied alpha under a filter",
               "Four texels averaged two ways: independently, which drags invisible "
               "black into the edge, and weighted by coverage, which does not.", b)


# ===========================================================================
# Figure 5 — coverage down the mip chain                                (§5)
# ===========================================================================
def fig_coverage():
    W, H = 720, 372
    b = [label(W / 2, 22, "Foliage that thins out as it recedes", "sm")]

    # Legend outside the plot, for the reason figure 3 gives.
    x0, x1 = 96, 456
    y0, y1 = 268, 70
    n = len(COV_PLAIN)

    def px(i):
        return x0 + (x1 - x0) * i / (n - 1)

    def py(c):
        return y0 + (y1 - y0) * (c - 0.20) / (0.40 - 0.20)

    b.append(rule(x0, y0, x1, y0, "grid", 1.0))
    b.append(rule(x0, y0, x0, y1, "grid", 1.0))
    for i in range(n):
        b.append(rule(px(i), y0, px(i), y0 + 5, "grid", 1.0))
        b.append(label(px(i), y0 + 20, f"{i}", "xs"))
    b.append(label((x0 + x1) / 2, y0 + 40, "mip level", "xs"))
    for c in (0.20, 0.25, 0.30, 0.35, 0.40):
        b.append(rule(x0 - 5, py(c), x0, py(c), "grid", 1.0))
        b.append(label(x0 - 12, py(c) + 4, f"{c:.2f}", "xs", "end"))
    b.append(label(x0 - 56, (y0 + y1) / 2, "coverage", "xs"))

    b.append(cline(x0, py(COV_BASE), x1, py(COV_BASE), GREY, 1.2, dash="5 4"))

    plain = " ".join(f"{px(i):.1f},{py(c):.1f}" for i, c in enumerate(COV_PLAIN))
    kept = " ".join(f"{px(i):.1f},{py(c):.1f}" for i, c in enumerate(COV_KEPT))
    b.append(f'<polyline points="{kept}" fill="none" stroke="{GREEN}" stroke-width="2.2"/>')
    b.append(f'<polyline points="{plain}" fill="none" stroke="{RED}" stroke-width="2.2" '
             f'stroke-dasharray="6 4"/>')
    for i, c in enumerate(COV_PLAIN):
        b.append(f'<circle cx="{px(i):.1f}" cy="{py(c):.1f}" r="3" fill="{RED}"/>')
    for i, c in enumerate(COV_KEPT):
        b.append(f'<circle cx="{px(i):.1f}" cy="{py(c):.1f}" r="3" fill="{GREEN}"/>')

    lx = 496
    ly = 96
    swatch(b, lx, ly, GREY, dash="5 4")
    b.append(label(lx + 36, ly + 4, f"the source: {COV_BASE:.4f}", "xs", "start"))
    b.append(label(lx + 36, ly + 22, "of its area passes a", "xs", "start"))
    b.append(label(lx + 36, ly + 40, "cutoff of 0.5", "xs", "start"))

    ly = 172
    swatch(b, lx, ly, GREEN)
    b.append(label(lx + 36, ly + 4, "with coverage_cutoff set:", "xs", "start"))
    b.append(label(lx + 36, ly + 22, "within 0.0115 down to 8x8", "xs", "start"))

    ly = 228
    swatch(b, lx, ly, RED, dash="6 4")
    b.append(label(lx + 36, ly + 4, "an ordinary chain:", "xs", "start"))
    b.append(label(lx + 36, ly + 22, f"{COV_LOST_PCT:.1f}% of the leaves", "xs", "start"))
    b.append(label(lx + 36, ly + 40, "gone by level 6", "xs", "start"))

    b.append(label(W / 2, H - 36,
                   "Nothing in the chain is wrong, and nothing in the alpha test is wrong.", "xs"))
    b.append(label(W / 2, H - 14,
                   "AVERAGING AND THRESHOLDING DO NOT COMMUTE — and only one of the two is the "
                   "mip chain's business.", "xs"))
    return svg("f611e", W, H, "Alpha coverage down a mip chain",
               "Coverage per mip level for an ordinary chain, which falls away, and "
               "for a coverage-preserving one, which holds the source's value.", b)


# ===========================================================================
# Figure 6 — draw order, and the case with no right answer              (§6)
# ===========================================================================
def fig_order():
    W, H = 720, 420
    b = [label(W / 2, 22, "The sort, and where sorting runs out", "sm")]

    # ---- top: the two buckets ------------------------------------------
    b.append(label(W / 2, 52, "one frame's draw list, bucketed", "xs"))
    items = [("opaque", GREY, 1.0), ("blend", BLUE, 5.0), ("mask", GREEN, 3.0),
             ("blend", BLUE, 12.0), ("opaque", GREY, 2.0), ("blend", BLUE, 8.0)]
    bw, bh, bg = 92, 30, 8
    x0 = (W - (len(items) * bw + (len(items) - 1) * bg)) / 2
    for i, (kind, col, d) in enumerate(items):
        x = x0 + i * (bw + bg)
        b.append(box(x, 64, bw, bh, col, rx=4, opacity=0.28))
        b.append(hollow(x, 64, bw, bh, col, rx=4))
        b.append(label(x + bw / 2, 84, f"{kind} {d:g}", "xs")) 
    b.append(arrow(W / 2, 100, W / 2, 124, "f611f", "s", 1.2))

    order = [("opaque", GREY, 1.0), ("mask", GREEN, 3.0), ("opaque", GREY, 2.0),
             ("blend", BLUE, 12.0), ("blend", BLUE, 8.0), ("blend", BLUE, 5.0)]
    for i, (kind, col, d) in enumerate(order):
        x = x0 + i * (bw + bg)
        b.append(box(x, 130, bw, bh, col, rx=4, opacity=0.28))
        b.append(hollow(x, 130, bw, bh, col, rx=4))
        b.append(label(x + bw / 2, 150, f"{kind} {d:g}", "xs"))
    split = x0 + 3 * (bw + bg) - bg / 2
    b.append(cline(split, 124, split, 172, AMBER, 1.6, dash="4 3"))
    b.append(label(x0 + 1.5 * (bw + bg) - bg / 2, 184,
                   "unsorted — the z-buffer sorts these", "xs"))
    b.append(label(x0 + 4.5 * (bw + bg) - bg / 2, 184, "sorted BACK TO FRONT", "xs"))
    b.append(label(W / 2, 204, "masked geometry goes with the OPAQUE geometry: "
                   "a discarded fragment writes no depth, so it occludes nothing", "xs"))

    # ---- bottom: the intersecting quads --------------------------------
    b.append(rule(60, 226, W - 60, 226, "grid", 1.0))
    b.append(label(W / 2, 250, "and the case no per-object order can fix", "xs"))

    cx, cy, s = 250, 330, 74
    # two crossing quads, seen edge-on
    b.append(f'<polygon points="{cx - s},{cy - s * 0.5} {cx + s},{cy + s * 0.5} '
             f'{cx + s},{cy + s * 0.5 + 18} {cx - s},{cy - s * 0.5 + 18}" '
             f'fill="{RED}" fill-opacity="0.45" stroke="{RED}" stroke-width="1.2"/>')
    b.append(f'<polygon points="{cx - s},{cy + s * 0.5} {cx + s},{cy - s * 0.5} '
             f'{cx + s},{cy - s * 0.5 + 18} {cx - s},{cy + s * 0.5 + 18}" '
             f'fill="{BLUE}" fill-opacity="0.45" stroke="{BLUE}" stroke-width="1.2"/>')
    b.append(label(cx, cy + 76, "two blended quads, intersecting", "xs"))
    b.append(cline(cx, cy - 44, cx, cy + 44, AMBER, 1.4, dash="4 3"))
    b.append(label(cx + 8, cy - 52, "the crossing", "xs", "start"))

    tx = 430
    b.append(label(tx, 292, "LEFT of the crossing, red is in front.", "xs", "start"))
    b.append(label(tx, 312, "RIGHT of it, blue is.", "xs", "start"))
    b.append(label(tx, 340, "A per-object order can draw red first or", "xs", "start"))
    b.append(label(tx, 358, "blue first. The correct picture needs BOTH,", "xs", "start"))
    b.append(label(tx, 376, "on opposite sides of one line — so it is not", "xs", "start"))
    b.append(label(tx, 394, "a better sort that is missing. It is OIT.", "xs", "start"))
    return svg("f611f", W, H, "Bucketing, sorting, and the limit of per-object order",
               "A draw list partitioned into an unsorted pass and a back-to-front pass, "
               "and two intersecting transparent quads for which no per-object order "
               "is correct.", b)


def main():
    figs = [("l611_fig1.svg", fig_modes()),
            ("l611_fig2.svg", fig_over()),
            ("l611_fig3.svg", fig_space()),
            ("l611_fig4.svg", fig_premul()),
            ("l611_fig5.svg", fig_coverage()),
            ("l611_fig6.svg", fig_order())]
    for name, body in figs:
        path = os.path.join(OUT, name)
        with open(path, "w") as fh:
            fh.write(body)
        print(f"wrote {path}  ({len(body):,} bytes)")


if __name__ == "__main__":
    main()
