#!/usr/bin/env python3
"""scratch/figs_616.py — Lesson 6.16's diagrams.

Same rules as 6.1-6.15's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - ~5.2 units per character for `xs`, ~6.0 for `sm`
  - filenames numbered by PAGE ORDER
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - `rule()` takes a CSS CLASS; `cline()` takes a COLOUR (6.10's trap)
  - LEGENDS AND ANNOTATION BOXES GO OUTSIDE THE PLOT (6.11's, 6.13's, 6.15's trap
    — and since 6.15 check-page.js tests `rect[fill="none"]` edges too, so an
    annotation box laid across a label is finally visible to the checker)
  - a SHAPE can leave the viewBox where a label cannot

Every number comes from verify_616's output.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)
from figs_610 import cline                                          # noqa: E402

OUT = "scratch"

# ---- measured (verify_616) -------------------------------------------------
CHECKS = 60

# §A / §B — the extraction
NEAR_ERR = 2.980e-07
FAR_ERR = 1.358e-03
ERR_RATIO = 4557
A_COEFF = -1.003009081
A_PLUS_1 = -3.009081e-03
FAR_FLOAT = -99.998217257
AMPLIFY = 332

# §D — cost per outcome
PLANES_PER_SURVIVOR = 6.0
PLANES_PER_REJECT = 1.95
REJECTED_BY = [("left", 16970), ("right", 12250), ("bottom", 4297),
               ("top", 5302), ("near", 18), ("far", 0)]

# §E — conservatism
E_BOXES = 200000
E_KEPT = 45512
E_STRADDLING = 19653
E_FULLY = 25859
E_VISIBLE = 45196
E_FALSE = 316
E_FALSE_OF_KEPT = 0.69           # per cent

# §F — tight vs loose
LOOSE_WORST = 2.4109             # the icosahedron, spinning
LOOSE_SUM = 1.3994
LOOSE_TORUS = 1.9008

# §G — the sphere pre-test
PRE_ON = (58.625, 116.581, 1.99)     # ns box-only, ns with pre-test, ratio
PRE_OFF = (35.910, 57.798, 1.61)

# §H — the crossover
C_NS = 58.268                    # the test, per object
W_NS = 23821.2                   # collect_triangles, per object
BREAKEVEN = 100.0 * C_NS / W_NS  # per cent
ONE_IN = W_NS / C_NS
# cull rate %, no-cull ns, cull ns
SWEEP = [(0.0, 12192625.0, 12220125.0),
         (0.4, 12193458.3, 12165958.3),
         (1.2, 12214208.3, 12153958.3),
         (5.1, 12130875.0, 11603583.3),
         (25.0, 11852833.3, 9164833.3),
         (50.0, 11536625.0, 6138125.0)]

# §I / §K — batching
INSTANCE_BYTES = 112
OBJECT_UNIFORM_BYTES = 112
GPU_DRAWS = (16, 1)
GPU_UNIFORM = (3152, 640)
GPU_INSTANCE_BYTES = 1792
GPU_SAVING = 720
HAND_BUILT = (12, 12)            # items, batches
FOREST = (400, 1)


# ===========================================================================
# Figure 1 — where the rejection happens                                  (§1)
# ===========================================================================
def fig_problem():
    W, H = 720, 384
    b = [label(W / 2, 22, "Two places an off-screen object can be thrown away, and "
                          "everything between them is paid for", "sm")]

    stages = [("draw list", GREY), ("submit", GREY), ("vertex shader", RED),
              ("clip", AMBER), ("raster", GREY), ("fragment", GREY)]
    x0, y0 = 46, 104
    bw, bh, gap = 98, 44, 14
    for i, (name, colour) in enumerate(stages):
        x = x0 + i * (bw + gap)
        b.append(hollow(x, y0, bw, bh, colour, "4 3" if colour is GREY else None))
        b.append(label(x + bw / 2, y0 + 27, name, "xs"))
        if i < len(stages) - 1:
            b.append(arrow(x + bw, y0 + bh / 2, x + bw + gap, y0 + bh / 2, "f616a", "s", 1.1))

    # The clipper is where the GPU notices. Mark it.
    clip_x = x0 + 3 * (bw + gap)
    b.append(cline(clip_x + bw / 2, y0 + bh, clip_x + bw / 2, y0 + bh + 26, AMBER, 1.4))
    b.append(label(clip_x + bw / 2, y0 + bh + 42, "THE GPU NOTICES HERE", "xs"))
    b.append(label(clip_x + bw / 2, y0 + bh + 58, "— and every stage to its", "xs"))
    b.append(label(clip_x + bw / 2, y0 + bh + 74, "left already ran", "xs"))

    # Culling happens before the first box.
    b.append(cline(x0 + bw / 2, y0, x0 + bw / 2, y0 - 22, GREEN, 1.6))
    b.append(label(x0 + bw / 2 + 6, y0 - 44, "WE NOTICE HERE", "xs"))
    b.append(label(x0 + bw / 2 + 6, y0 - 29, "six dot products", "xs"))

    # The vertex shader is the expensive stage that runs for nothing.
    vs_x = x0 + 2 * (bw + gap)
    b.append(label(vs_x + bw / 2, y0 - 10, "runs on EVERY vertex", "xs"))

    # ---- The measured cost, as a bar pair, OUTSIDE the flow ----------------
    py, ph = 250, 82
    b.append(label(W / 2, py - 12, "One object's cost, measured on this engine's CPU path "
                                   "(verify_616 §H)", "xs"))
    bar_x = 150
    bar_w = 430
    rows = [("the frustum test", C_NS, GREEN),
            ("the work it skips", W_NS, RED)]
    top = max(v for _, v, _ in rows)
    for i, (name, v, colour) in enumerate(rows):
        y = py + 14 + i * 34
        # A log-ish scale: the ratio is 409x and a linear bar would make the
        # green one invisible, which is the OPPOSITE of the point.
        w = bar_w * (math.log10(v) / math.log10(top))
        b.append(f'<rect x="{bar_x}" y="{y}" width="{w:.1f}" height="20" rx="2"'
                 f' fill="{colour}" fill-opacity="0.42" stroke="{colour}"'
                 f' stroke-width="1.2"/>')
        b.append(label(bar_x - 8, y + 15, name, "xs", "end"))
        b.append(label(bar_x + w + 8, y + 15, f"{v:,.1f} ns", "xs", "start"))
    b.append(label(W / 2, py + ph + 12,
                   f"log scale — the ratio is {ONE_IN:.0f}x, so culling pays as soon as it "
                   f"rejects one object in {ONE_IN:.0f}", "xs"))
    return svg("f616a", W, H,
               "Where an off-screen object is rejected",
               "A pipeline diagram: draw list, submit, vertex shader, clip, raster, fragment. "
               "The GPU rejects off-screen geometry at the clipper, after the vertex shader has "
               "run. Frustum culling rejects it before submission. Below, a log-scale bar pair "
               "comparing the cost of the test with the cost of the work it skips.", b)


# ===========================================================================
# Figure 2 — a clip coordinate IS a signed distance                       (§3)
# ===========================================================================
def fig_planes():
    W, H = 720, 400
    b = [label(W / 2, 22, "The six inequalities that define the clip volume, and the six rows "
                          "that answer them", "sm")]

    # ---- LEFT: the frustum in 2D, seen from above --------------------------
    ex, ey = 128, 300                 # the eye (apex)
    half = math.radians(30.0)
    depth = 210
    lx = ex - math.tan(half) * depth
    rx = ex + math.tan(half) * depth
    ty = ey - depth

    b.append(label(ex, 48, "SEEN FROM ABOVE", "xs"))

    # the two side walls
    b.append(cline(ex, ey, lx, ty, BLUE, 1.6))
    b.append(cline(ex, ey, rx, ty, BLUE, 1.6))
    # near and far
    near_t = 0.28
    nl = (ex + (lx - ex) * near_t, ey + (ty - ey) * near_t)
    nr = (ex + (rx - ex) * near_t, ey + (ty - ey) * near_t)
    b.append(cline(nl[0], nl[1], nr[0], nr[1], GREEN, 1.6))
    b.append(cline(lx, ty, rx, ty, AMBER, 1.6))

    b.append(f'<circle cx="{ex}" cy="{ey}" r="4" fill="{RED}"/>')
    b.append(label(ex, ey + 18, "eye", "xs"))
    b.append(label(nl[0] - 26, nl[1] + 4, "near", "xs", "end"))
    b.append(label(lx - 6, ty - 10, "far", "xs", "start"))
    # Inside the walls, not outside them: at 30 degrees the left wall reaches
    # x = 6.8, so an "end"-anchored label at lx - 30 lands at -23 and is clipped.
    b.append(label(lx + 30, (ey + ty) / 2 + 34, "left", "xs"))
    b.append(label(rx - 30, (ey + ty) / 2 + 34, "right", "xs"))

    # The apex fact: d = 0 on all four side planes.
    b.append(hollow(ex - 106, ey + 30, 212, 48, RED, "4 3"))
    b.append(label(ex, ey + 48, "THE EYE IS THE APEX, so its", "xs"))
    b.append(label(ex, ey + 64, "distance to all four side planes is 0", "xs"))

    # ---- RIGHT: the table --------------------------------------------------
    tx = 340
    b.append(label(tx + 180, 48, "THE ROW COMBINATIONS", "xs"))
    head = [("inside when", 0), ("plane", 132), ("why", 218)]
    for name, dx in head:
        b.append(label(tx + dx, 72, name, "xs", "start"))
    b.append(rule(tx - 6, 80, tx + 358, 80, "grid", 1.0))

    rows = [("x ≥ −w", "row0 + row3", "left"),
            ("x ≤ +w", "row3 − row0", "right"),
            ("y ≥ −w", "row1 + row3", "bottom"),
            ("y ≤ +w", "row3 − row1", "top"),
            ("z ≥ 0", "row2", "near — SDL_GPU, not OpenGL"),
            ("z ≤ +w", "row3 − row2", "far — and it cancels")]
    for i, (ineq, comb, why) in enumerate(rows):
        y = 102 + i * 30
        cls = "xs"
        b.append(label(tx, y, ineq, cls, "start"))
        b.append(label(tx + 132, y, comb, cls, "start"))
        b.append(label(tx + 218, y, why, cls, "start"))
        if i in (4, 5):
            b.append(rule(tx - 6, y + 8, tx + 358, y + 8, "grid", 0.8, "3 3"))

    # The two annotations, OUTSIDE the table.
    b.append(hollow(tx - 8, 292, 372, 44, GREEN, "4 3"))
    b.append(label(tx + 178, 310, "row2 ALONE has no subtraction, so no cancellation:", "xs"))
    b.append(label(tx + 178, 326, f"the near plane is accurate to {NEAR_ERR:.1e}", "xs"))

    b.append(hollow(tx - 8, 344, 372, 44, AMBER, "4 3"))
    b.append(label(tx + 178, 362, f"row3 − row2 subtracts near-equal rows:", "xs"))
    b.append(label(tx + 178, 378, f"the far plane is off by {FAR_ERR:.1e} — {ERR_RATIO}x worse", "xs"))

    return svg("f616b", W, H,
               "Frustum planes from the rows of the view-projection matrix",
               "Left: a frustum seen from above, with the eye at the apex and the four side "
               "planes meeting there, so the eye's distance to each is zero. Right: a table of "
               "the six clip-space inequalities, the row combination each becomes, and which "
               "plane it is. The near plane is row 2 alone under SDL_GPU's zero-to-w depth "
               "range; the far plane is a difference of near-equal rows and loses precision.", b)


# ===========================================================================
# Figure 3 — the worked example, with real numbers                        (§3)
# ===========================================================================
def fig_worked():
    W, H = 720, 392
    b = [label(W / 2, 22, "A 90° frustum, near 1, far 11 — every number below is "
                          "checked in verify_616 §C", "sm")]

    # A 2D slice: the camera at the origin looking down -z, drawn with -z RIGHT
    # so the reader sees the pyramid opening left to right.
    ox, oy = 96, 214
    scale = 44.0                      # pixels per world unit along z
    yscale = 22.0                     # pixels per world unit across

    b.append(label(ox, 50, "x", "xs"))
    # axes
    b.append(rule(ox, oy, ox + 12.4 * scale, oy, "grid", 1.0, "4 3"))
    b.append(label(ox + 12.4 * scale, oy + 16, "−z", "xs"))
    b.append(rule(ox, oy - 3.6 * yscale, ox, oy + 3.6 * yscale, "grid", 1.0, "4 3"))

    # the 45-degree walls: at 90 degrees vertical fov with square aspect, x = -z
    for sign in (-1, 1):
        b.append(cline(ox, oy, ox + 11.6 * scale, oy + sign * 11.6 * yscale * 0.32,
                       BLUE, 1.6))
    b.append(f'<circle cx="{ox}" cy="{oy}" r="4" fill="{RED}"/>')
    b.append(label(ox - 4, oy + 20, "eye", "xs", "end"))

    # near and far planes
    for zv, colour, name in ((1.0, GREEN, "near = 1"), (11.0, AMBER, "far = 11")):
        x = ox + zv * scale
        h = zv * yscale * 0.32
        b.append(cline(x, oy - h, x, oy + h, colour, 1.6))
        b.append(label(x, oy - h - 10, name, "xs"))

    # the two sample points from §C
    pts = [(-5.0, 5.0, "(−5, 0, −5)", "ON the left plane: d = 0", RED),
           (-4.5, 5.0, "(−4.5, 0, −5)", "d = 0.353553 = 0.5/√2", GREEN)]
    for xv, zv, name, note, colour in pts:
        px = ox + zv * scale
        py = oy + xv * yscale * 0.32 * -1.0
        b.append(f'<circle cx="{px:.1f}" cy="{py:.1f}" r="3.6" fill="{colour}"/>')
    # Labels for those two, in a stack ABOVE the picture with leader lines down
    # to the dots. Placing them beside the points put the first one across the
    # 45-degree wall, which check-page.js's text-on-shape test caught.
    for k, (xv, zv, name, note, colour) in enumerate(pts):
        px = ox + zv * scale
        py = oy + xv * yscale * 0.32 * -1.0
        ly = 62 + k * 20
        b.append(label(ox + 7.2 * scale, ly, name + " — " + note, "xs", "start"))
        b.append(cline(ox + 7.1 * scale, ly - 4, px + 6, py - 4, colour, 0.8, "3 3"))

    # ---- The arithmetic, in a block BELOW the picture ----------------------
    ay = 292
    b.append(rule(60, ay - 18, W - 60, ay - 18, "grid", 1.0))
    lines = [
        "f = cot(45°) = 1, so row0 = (1, 0, 0, 0) and row3 = (0, 0, −1, 0)",
        "left = row0 + row3 = (1, 0, −1, 0);  |n| = √2, so the unit plane is "
        "(0.707107, 0, −0.707107 | 0)",
        "d(−5, 0, −5) = 0.707107·(−5) − 0.707107·(−5) + 0 = 0   ✓",
        "near = row2 = (0, 0, A, B) = (0, 0, −1.1, −1.1);  |n| = 1.1, so unit = "
        "(0, 0, −1 | −1) — i.e. −near   ✓",
    ]
    for i, ln in enumerate(lines):
        b.append(label(W / 2, ay + i * 22, ln, "xs"))

    return svg("f616c", W, H,
               "A worked frustum with round numbers",
               "A 90-degree frustum with near 1 and far 11, drawn as a slice with minus-z to the "
               "right. Two sample points are marked: one exactly on the left plane with signed "
               "distance zero, one half a unit inside at distance 0.353553. Below, the "
               "arithmetic: the left plane is row0 plus row3 normalised, and the near plane is "
               "row2 alone, whose constant term works out to minus the near distance.", b)


# ===========================================================================
# Figure 4 — the corner to test, and the three answers                    (§5)
# ===========================================================================
def fig_corner():
    W, H = 720, 372
    b = [label(W / 2, 22, "One corner per plane, chosen by the sign of the normal — and the "
                          "three answers it can give", "sm")]

    # ---- LEFT: choosing the corner ----------------------------------------
    bx, by, bw, bh = 92, 92, 132, 96
    b.append(label(bx + bw / 2, 54, "THE POSITIVE VERTEX", "xs"))
    b.append(hollow(bx, by, bw, bh, GREY))
    for i, (cx, cy) in enumerate([(bx, by + bh), (bx + bw, by + bh),
                                  (bx, by), (bx + bw, by)]):
        b.append(f'<circle cx="{cx}" cy="{cy}" r="3" fill="{GREY}"/>')
    # the plane normal, pointing up-right
    nx, ny = 0.62, -0.78
    axx, axy = bx + bw / 2, by + bh / 2
    b.append(arrow(axx, axy, axx + nx * 78, axy + ny * 78, "f616d", "h", 1.6))
    b.append(label(axx + nx * 96, axy + ny * 96 - 4, "n", "xs"))
    # the chosen corner: max on x (n.x > 0), min on y-down (n.y < 0 -> top)
    b.append(f'<circle cx="{bx + bw}" cy="{by}" r="5.5" fill="{GREEN}"/>')
    b.append(label(bx + bw + 12, by - 8, "chosen", "xs", "start"))
    b.append(f'<circle cx="{bx}" cy="{by + bh}" r="5.5" fill="{RED}"/>')
    b.append(label(bx - 12, by + bh + 16, "the other one", "xs", "end"))

    b.append(label(bx + bw / 2, by + bh + 54, "take max where the normal's", "xs"))
    b.append(label(bx + bw / 2, by + bh + 70, "component is positive, min where", "xs"))
    b.append(label(bx + bw / 2, by + bh + 86, "it is negative — three compares,", "xs"))
    b.append(label(bx + bw / 2, by + bh + 102, "one dot product, no loop over eight", "xs"))

    # ---- RIGHT: the three states ------------------------------------------
    px, py = 400, 96
    b.append(label(px + 150, 54, "THE THREE ANSWERS", "xs"))
    states = [("outside", RED, "the positive vertex is outside → all eight are"),
              ("intersecting", AMBER, "positive in, negative out → it straddles"),
              ("inside", GREEN, "the NEGATIVE vertex is inside → all eight are")]
    for i, (name, colour, note) in enumerate(states):
        y = py + i * 62
        b.append(cline(px - 24, y + 22, px + 324, y + 22, GREY, 1.0, "3 4"))
        b.append(hollow(px, y, 26, 26, colour))
        b.append(label(px + 42, y + 17, name, "xs", "start"))
        b.append(label(px + 42, y + 38, note, "xs", "start"))

    # The measured split, OUTSIDE the diagram.
    b.append(hollow(px - 60, py + 192, 348, 62, BLUE, "4 3"))
    b.append(label(px + 114, py + 213,
                   f"On {E_BOXES:,} random boxes: {E_KEPT:,} kept,", "xs"))
    b.append(label(px + 114, py + 229,
                   f"of which {E_STRADDLING:,} straddle and {E_FULLY:,} are", "xs"))
    b.append(label(px + 114, py + 245,
                   "wholly inside — what a hierarchy would save", "xs"))
    return svg("f616d", W, H,
               "The positive-vertex AABB test and its three outcomes",
               "Left: a box and a plane normal; the corner furthest along the normal is chosen by "
               "taking max on axes where the normal is positive and min where it is negative. "
               "Right: the three classifications - outside when the positive vertex is outside, "
               "intersecting when the box straddles, inside when even the negative vertex is in. "
               "Measured on two hundred thousand random boxes.", b)


# ===========================================================================
# Figure 5 — the false positive, and its size                             (§6)
# ===========================================================================
def fig_false():
    W, H = 720, 396
    b = [label(W / 2, 22, "A box can fail to be rejected while being entirely outside — "
                          "and the effect has a measured size", "sm")]

    # ---- LEFT: the corner case, drawn rather than asserted ------------------
    #
    # THE BOX BELOW IS A VERIFIED FALSE POSITIVE, not a sketch of one. Its corner
    # coordinates were solved for and then checked: the positive vertex for the
    # LEFT plane is (128, 50) at distance +17.4, the positive vertex for the FAR
    # plane is (40, 120) at distance +20.0, every other plane is trivially
    # satisfied, and a 400-step sweep confirms the box never touches the frustum.
    # A first draft simply parked a box beyond the far plane, which is REJECTED
    # by the far plane and therefore illustrated nothing.
    ex, ey = 206.0, 268.0
    half = math.radians(24.0)
    depth = 168.0
    lx = ex - math.tan(half) * depth
    rx = ex + math.tan(half) * depth
    ty = ey - depth
    slope = (ex - lx) / (ey - ty)

    # The planes, EXTENDED past the frustum as dashed rays — because a plane is
    # infinite and that infiniteness is the whole mechanism. Without these the
    # figure cannot show why the box passes.
    b.append(cline(ex, ey, ex - slope * (ey - 34), 34, GREY, 1.0, "4 4"))
    b.append(cline(30, ty, lx, ty, GREY, 1.0, "4 4"))

    # The frustum itself.
    b.append(cline(ex, ey, lx, ty, BLUE, 1.6))
    b.append(cline(ex, ey, rx, ty, BLUE, 1.6))
    b.append(cline(lx, ty, rx, ty, BLUE, 1.6))
    b.append(f'<circle cx="{ex}" cy="{ey}" r="3.6" fill="{RED}"/>')
    b.append(label(ex, ey + 18, "eye", "xs"))
    b.append(label((lx + rx) / 2, ty + 26, "the frustum", "xs"))
    b.append(label(rx + 4, ty - 10, "far plane", "xs", "start"))
    # No label on the extended ray itself: the top-left corner of this figure is
    # the busiest part of it, and three labels there collided. One legend line,
    # below the picture, says what dashed means for both rays at once.

    bx0, bx1, by0, by1 = 40.0, 128.0, 50.0, 120.0
    b.append(hollow(bx0, by0, bx1 - bx0, by1 - by0, AMBER, "5 4", width=1.6))

    # The two positive vertices — OPPOSITE CORNERS, which is the point.
    b.append(f'<circle cx="{bx1}" cy="{by0}" r="4.4" fill="{GREEN}"/>')
    b.append(f'<circle cx="{bx0}" cy="{by1}" r="4.4" fill="{PURPLE}"/>')
    b.append(label(bx1 + 10, by0 - 4, "inside the LEFT plane", "xs", "start"))
    b.append(label(bx0 - 6, by1 + 20, "inside the FAR plane", "xs", "start"))
    b.append(label((bx0 + bx1) / 2, by1 + 44, "…and outside the frustum", "xs"))
    b.append(label(50, ey + 30, "dashed = a plane, extended past the frustum",
                   "xs", "start"))

    for k, txt in enumerate(["Two DIFFERENT corners satisfy two different planes,",
                             "and every plane is satisfied by some corner — so no",
                             "single half-space test can reject the box, even",
                             "though their intersection never contains any of it."]):
        b.append(label(ex - 30, ey + 52 + k * 16, txt, "xs"))

    # ---- RIGHT: the numbers ------------------------------------------------
    nx = 452
    b.append(label(nx + 126, 54, "MEASURED (verify_616 §E)", "xs"))
    rows = [(f"{E_BOXES:,}", "boxes tested"),
            (f"{E_KEPT:,}", "kept by the six tests"),
            (f"{E_VISIBLE:,}", "genuinely intersect"),
            (f"{E_FALSE}", f"kept but invisible — {E_FALSE_OF_KEPT}%"),
            ("0", "VISIBLE boxes rejected")]
    for i, (n, what) in enumerate(rows):
        y = 88 + i * 30
        colour = GREEN if i == 4 else (AMBER if i == 3 else GREY)
        b.append(label(nx + 52, y, n, "xs", "end"))
        b.append(label(nx + 64, y, what, "xs", "start"))
        if i >= 3:
            b.append(cline(nx - 6, y + 8, nx + 248, y + 8, colour, 0.9, "3 3"))

    b.append(hollow(nx - 12, 246, 260, 96, GREEN, "4 3"))
    b.append(label(nx + 118, 266, "THE ASYMMETRY IS THE LICENCE.", "xs"))
    b.append(label(nx + 118, 286, "Keeping something invisible costs", "xs"))
    b.append(label(nx + 118, 304, "a draw call. Rejecting something", "xs"))
    b.append(label(nx + 118, 322, "visible is a hole in the picture.", "xs"))
    return svg("f616e", W, H,
               "The false-positive corner case and its measured rate",
               "Left: a frustum with its left and far planes extended as dashed rays past the "
               "frustum itself, and a box placed up and to the left. The box's top-right corner "
               "is inside the left plane's half-space and its bottom-left corner is inside the "
               "far plane's, so two different corners satisfy two different planes and no single "
               "test rejects it - yet the box never enters the region where all six hold at once. "
               "Right: over two hundred thousand random boxes, 316 such false positives, 0.69 per "
               "cent of those kept, and zero false rejections.", b)


def fig_crossover():
    W, H = 720, 436
    b = [label(W / 2, 22, "Culling is not free, and the break-even is a cull RATE, not a scene "
                          "size", "sm")]

    # plot frame
    px, py, pw, ph = 96, 62, 528, 232
    b.append(rule(px, py + ph, px + pw, py + ph, "grid", 1.2))
    b.append(rule(px, py, px, py + ph, "grid", 1.2))

    lo = min(min(a, c) for _, a, c in SWEEP) * 0.96
    hi = max(max(a, c) for _, a, c in SWEEP) * 1.02

    def ypix(v):
        return py + ph - (v - lo) / (hi - lo) * ph

    def xpix(i):
        return px + 42 + i * (pw - 84) / (len(SWEEP) - 1)

    # gridlines + y labels, in milliseconds because nanoseconds at 1e7 are noise
    for k in range(5):
        v = lo + (hi - lo) * k / 4.0
        y = ypix(v)
        b.append(rule(px, y, px + pw, y, "grid", 0.7, "3 4"))
        b.append(label(px - 10, y + 4, f"{v / 1e6:.1f}", "xs", "end"))
    b.append(label(px - 62, py + ph / 2, "ms / frame", "xs"))

    for series, colour, name in ((1, RED, "no culling"), (2, GREEN, "with culling")):
        pts = []
        for i, row in enumerate(SWEEP):
            pts.append((xpix(i), ypix(row[series])))
        b.append('<polyline points="' + " ".join(f"{x:.1f},{y:.1f}" for x, y in pts)
                 + f'" fill="none" stroke="{colour}" stroke-width="2.0"/>')
        for x, y in pts:
            b.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3.4" fill="{colour}"/>')

    for i, row in enumerate(SWEEP):
        b.append(label(xpix(i), py + ph + 18, f"{row[0]:.1f}%", "xs"))
    b.append(label(px + pw / 2, py + ph + 38, "cull rate — what fraction of the draw list "
                                              "is off screen", "xs"))

    # The one point where culling loses, marked.
    b.append(f'<circle cx="{xpix(0):.1f}" cy="{ypix(SWEEP[0][2]):.1f}" r="8" fill="none"'
             f' stroke="{AMBER}" stroke-width="1.8"/>')

    # Annotations, BELOW the plot and clear of it.
    b.append(hollow(px, py + ph + 52, 252, 78, AMBER, "4 3"))
    b.append(label(px + 126, py + ph + 72, "AT 0% CULLED, CULLING LOSES", "xs"))
    b.append(label(px + 126, py + ph + 88, f"by {C_NS:.0f} ns an object — which is", "xs"))
    b.append(label(px + 126, py + ph + 104, "exactly what the test costs. Nothing", "xs"))
    b.append(label(px + 126, py + ph + 120, "was skipped, so nothing was saved.", "xs"))

    b.append(hollow(px + 276, py + ph + 52, 252, 78, GREEN, "4 3"))
    b.append(label(px + 402, py + ph + 72, "BREAK-EVEN = c / w", "xs"))
    b.append(label(px + 402, py + ph + 88,
                   f"{C_NS:.1f} ns / {W_NS:,.0f} ns = {BREAKEVEN:.2f}%", "xs"))
    b.append(label(px + 402, py + ph + 104,
                   f"one object in {ONE_IN:.0f} — predicted first,", "xs"))
    b.append(label(px + 402, py + ph + 120, "then found between 0% and 0.4%.", "xs"))
    return svg("f616f", W, H,
               "Frame time with and without culling, swept over cull rate",
               "Two curves against cull rate from zero to fifty per cent. Without culling the "
               "frame time barely changes; with culling it falls proportionally. At a cull rate "
               "of zero the culling curve is above the other one, by the cost of the test. The "
               "predicted break-even, the test cost divided by the work it skips, is 0.24 per "
               "cent - one object in 409 - and the empirical crossing lies between the zero and "
               "0.4 per cent samples.", b)


# ===========================================================================
# Figure 7 — the batch key, and where the bytes go                        (§8)
# ===========================================================================
def fig_batch():
    W, H = 720, 404
    b = [label(W / 2, 22, "What every instance in one draw must agree about — and what "
                          "batching actually saves", "sm")]

    # ---- LEFT: the key -----------------------------------------------------
    kx, ky, kw = 60, 62, 300
    b.append(label(kx + kw / 2, ky - 8, "THE BATCH KEY", "xs"))
    fields = [("mesh", "modelling"),
              ("albedo + normal map", "atlasing"),
              ("surface style", "pipeline"),
              ("blend style", "pipeline"),
              ("material (32 B)", "parameters")]
    for i, (name, why) in enumerate(fields):
        y = ky + 10 + i * 42
        b.append(hollow(kx, y, kw, 32, BLUE))
        b.append(label(kx + 10, y + 20, name, "xs", "start"))
        # ANCHORED INSIDE THE BOX, not beside it. Set to the right of a 262-wide
        # box at x = 60, the longest of these reached x = 436 and crossed the
        # green annotation box that starts at 426 — which check-page.js's
        # rect[fill="none"] edge test (added in 6.15) is exactly there to catch.
        b.append(label(kx + kw - 10, y + 20, why, "xs", "end"))

    b.append(hollow(kx, ky + 226, kw, 58, AMBER, "4 3"))
    b.append(label(kx + kw / 2, ky + 246, "Differ in ANY of these and the run", "xs"))
    b.append(label(kx + kw / 2, ky + 262, "ends. Which is why the demo scene's", "xs"))
    b.append(label(kx + kw / 2, ky + 278,
                   f"{HAND_BUILT[0]} objects make {HAND_BUILT[1]} batches.", "xs"))

    # ---- RIGHT: the bytes --------------------------------------------------
    ox2 = 434
    b.append(label(ox2 + 130, ky - 8, "SIXTEEN CUBES, TWO ROADS (§K)", "xs"))

    bars = [("uniform push", GPU_UNIFORM[0], 0, RED),
            ("instanced", GPU_UNIFORM[1], GPU_INSTANCE_BYTES, GREEN)]
    top = GPU_UNIFORM[0]
    bar_w = 236
    for i, (name, uni, inst, colour) in enumerate(bars):
        y = ky + 18 + i * 62
        wu = bar_w * uni / top
        wi = bar_w * inst / top
        b.append(f'<rect x="{ox2}" y="{y}" width="{wu:.1f}" height="22" rx="2"'
                 f' fill="{colour}" fill-opacity="0.45" stroke="{colour}" stroke-width="1.1"/>')
        if inst:
            b.append(f'<rect x="{ox2 + wu:.1f}" y="{y}" width="{wi:.1f}" height="22" rx="2"'
                     f' fill="{PURPLE}" fill-opacity="0.45" stroke="{PURPLE}"'
                     f' stroke-width="1.1"/>')
        b.append(label(ox2, y - 6, name, "xs", "start"))
        total = uni + inst
        b.append(label(ox2 + bar_w + 12, y + 16, f"{total:,} B", "xs", "start"))

    b.append(label(ox2, ky + 152, "█ uniform pushes", "xs", "start"))
    b.append(label(ox2 + 122, ky + 152, "█ instance buffer", "xs", "start"))

    b.append(hollow(ox2 - 8, ky + 172, 274, 112, GREEN, "4 3"))
    b.append(label(ox2 + 129, ky + 192, f"{GPU_DRAWS[0]} draw calls became {GPU_DRAWS[1]}.", "xs"))
    b.append(label(ox2 + 129, ky + 210, f"The {INSTANCE_BYTES}-byte matrix block did not", "xs"))
    b.append(label(ox2 + 129, ky + 228, "shrink — it changed road. The whole", "xs"))
    b.append(label(ox2 + 129, ky + 246, f"{GPU_SAVING}-byte saving is 15 materials", "xs"))
    b.append(label(ox2 + 129, ky + 264, "not pushed. Instancing saves", "xs"))
    b.append(label(ox2 + 129, ky + 282, "SUBMISSION, not bandwidth.", "xs"))
    return svg("f616g", W, H,
               "The batch key and the byte accounting",
               "Left: the five components of a batch key - mesh, textures, surface style, blend "
               "style and the material block - each annotated with the kind of decision that "
               "changes it. Right: for sixteen identical cubes, the uniform road pushes 3152 "
               "bytes across sixteen draws while the instanced road pushes 640 and uploads 1792 "
               "instance bytes, for 2432 total across one draw. The 720-byte difference is "
               "fifteen material blocks not pushed.", b)


def main():
    figs = [("l616_fig1.svg", fig_problem()),
            ("l616_fig2.svg", fig_planes()),
            ("l616_fig3.svg", fig_worked()),
            ("l616_fig4.svg", fig_corner()),
            ("l616_fig5.svg", fig_false()),
            ("l616_fig6.svg", fig_crossover()),
            ("l616_fig7.svg", fig_batch())]
    for name, body in figs:
        path = os.path.join(OUT, name)
        with open(path, "w") as fh:
            fh.write(body)
        print(f"wrote {path}  ({len(body):,} bytes)")


if __name__ == "__main__":
    main()
