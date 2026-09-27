#!/usr/bin/env python3
"""scratch/figs_69.py — Lesson 6.9's diagrams.

Same rules as 6.1-6.8's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - every element well inside the viewBox
  - ~5.2 units per character for `xs`, ~6.0 for `sm`
  - filenames numbered by PAGE ORDER
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - SVG collapses whitespace: two columns are two <text> elements

Every number comes from verify_69's output.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)

OUT = "scratch"

# ---- measured, from verify_69 ----------------------------------------------
CHECKS = 27
ONE_MAP_WPT = 0.0548        # §C, 6.8's single map over a 40 m scene
WPT = [0.01945, 0.03835, 0.06695, 0.14165]   # §C, per cascade
SPLITS = [7.78, 16.25, 28.57, 60.00]         # §C, lambda = 0.5
GAIN = 2.8                  # §C
TIGHT_WPT = 0.01508         # §C, a corner-fitted box
SPHERE_COST = 29            # per cent of resolution the sphere costs
YAW_SPREAD = 9.93e-08       # §D, sphere
BOX_SPREAD = 30.2           # §D, per cent, corner-fitted box
SNAP_ON = 7.63e-06          # §E, texels
SNAP_OFF = 0.499            # §E, texels
BIAS_WORLD_RATIO = 7.3      # §F
BIAS_DEVICE = 2.031e-03     # §F, cascades 1-3
BIAS_PREDICTED = 2.0716e-03 # §F, reach*slope/resolution
BIAS_C0_PCT = 64            # §F, cascade 0 as a fraction of cascade 1


# ===========================================================================
# Figure 1 — one map over a frustum: where the texels actually go      (§1)
# ===========================================================================
def fig_problem():
    W, H = 720, 300
    b = []
    b.append(label(W / 2, 22, "One map, stretched over everything the camera can see", "sm"))

    # The frustum, on its side: eye at the left, opening to the right.
    eye = (70, 165)
    far_x = 620
    half = 105
    b.append(f'<path d="M {eye[0]} {eye[1]} L {far_x} {eye[1] - half} '
             f'L {far_x} {eye[1] + half} Z" class="fill-soft grid" '
             f'fill-opacity="0.30" stroke-width="1.2"/>')
    b.append(label(eye[0] - 6, eye[1] + 4, "eye", "xs", "end"))

    # A uniform texel grid over the whole thing — which is what one box gives.
    n = 16
    for i in range(n + 1):
        x = eye[0] + (far_x - eye[0]) * i / n
        b.append(rule(x, eye[1] - half, x, eye[1] + half, "grid", 0.6))
    b.append(label((eye[0] + far_x) / 2, eye[1] + half + 26,
                   "one box around the SCENE: every texel the same size, everywhere", "xs"))

    # What the camera actually sees there: a near object is huge on screen.
    b.append(box(105, 140, 26, 50, BLUE, opacity=0.85))
    b.append(label(118, 132, "2 m", "xs"))
    b.append(box(545, 152, 26, 26, PURPLE, opacity=0.85))
    b.append(label(558, 145, "40 m", "xs"))

    b.append(label(118, 214, "fills half the screen", "xs"))
    b.append(label(118, 228, "gets 2 texels", "xs"))
    b.append(label(558, 200, "a few pixels", "xs"))
    b.append(label(558, 214, "gets 2 texels", "xs"))

    b.append(label(W / 2, 274,
                   f"{ONE_MAP_WPT:.4f} m per texel at both ends "
                   f"— the near object is starved and the far one is over-served", "xs"))
    return svg("f69a", W, H, "One shadow map over a whole frustum",
               "A camera frustum drawn on its side with a uniform texel grid over it. "
               "A near object and a far object receive the same number of texels, though "
               "the near one fills the screen and the far one covers a few pixels.", b)


# ===========================================================================
# Figure 2 — the three split schemes                                   (§4.1)
# ===========================================================================
def fig_splits():
    W, H = 720, 320
    b = []
    b.append(label(W / 2, 22, "Where to cut, and the two schemes that are both wrong", "sm"))

    x0, x1 = 90, 640
    near, far = 0.1, 60.0

    def px(d):
        return x0 + (x1 - x0) * (d - near) / (far - near)

    rows = [
        ("uniform", 80, [15.08, 30.05, 45.03, 60.00], GREY,
         "equal metres: cascade 0 covers 15 m and the near field is no better off"),
        ("logarithmic", 150, [0.55, 3.10, 17.32, 60.00], BLUE,
         "equal texels-per-pixel: correct, and cascade 0 is spent on your shoes"),
        ("practical", 220, [7.78, 16.25, 28.57, 60.00], GREEN,
         "the blend, lambda = 0.5 — each end explicable, the middle useful"),
    ]
    for name, y, cuts, colour, note in rows:
        b.append(rule(x0, y, x1, y, "grid", 1.0))
        b.append(label(x0 - 8, y + 4, name, "xs", "end"))
        prev = x0
        for i, d in enumerate(cuts):
            x = px(d)
            b.append(f'<rect x="{prev:.1f}" y="{y - 11}" width="{max(2.0, x - prev):.1f}" '
                     f'height="22" rx="2" fill="{colour}" stroke="{colour}" '
                     f'fill-opacity="{0.5 - i * 0.10:.2f}" stroke-width="1.0"/>')
            prev = x
        for d in cuts[:-1]:
            b.append(label(px(d), y - 16, f"{d:.1f}", "xs"))
        b.append(label(x0 + 4, y + 36, note, "xs", "start"))

    b.append(rule(x0, 268, x1, 268, "grid", 1.0))
    for d in (0.1, 15, 30, 45, 60):
        b.append(rule(px(d), 264, px(d), 272, "grid", 1.0))
        b.append(label(px(d), 288, f"{d:g}", "xs"))
    b.append(label(W / 2, 306, "distance from the eye, metres", "xs"))
    return svg("f69b", W, H, "Uniform, logarithmic and practical split schemes",
               "Three horizontal bars showing where each scheme cuts a frustum between "
               "0.1 and 60 metres. Uniform cuts at 15, 30 and 45. Logarithmic cuts at "
               "0.55, 3.1 and 17.3. The practical blend cuts at 7.8, 16.3 and 28.6.", b)


# ===========================================================================
# Figure 3 — the slice, and the sphere around it                       (§4.2)
# ===========================================================================
def fig_sphere():
    W, H = 720, 330
    b = []
    b.append(label(W / 2, 22, "Fitting the box: a sphere, not the eight corners", "sm"))

    # left: a corner-fitted box, at two yaws
    for k, (cx, yaw, tag) in enumerate(((185, 0.0, "camera facing along the light"),
                                        (520, 0.55, "the same camera, turned 30°"))):
        cy = 160
        r = 74
        # the slice, as a rotated quad
        pts = []
        for sx, sy in ((-1, -0.55), (1, -0.55), (1, 0.55), (-1, 0.55)):
            x = sx * r * 0.95
            y = sy * r * 0.95
            pts.append((cx + x * math.cos(yaw) - y * math.sin(yaw),
                        cy + x * math.sin(yaw) + y * math.cos(yaw)))
        b.append('<polygon points="' + " ".join(f"{x:.1f},{y:.1f}" for x, y in pts) +
                 f'" fill="{BLUE}" stroke="{BLUE}" fill-opacity="0.28" stroke-width="1.3"/>')

        # the corner-fitted box: axis-aligned in LIGHT space, so it grows with yaw
        xs = [p[0] for p in pts]
        ys = [p[1] for p in pts]
        bw = max(xs) - min(xs)
        bh = max(ys) - min(ys)
        side = max(bw, bh)
        b.append(hollow(cx - side / 2, cy - side / 2, side, side, RED, dash="5 4"))
        b.append(label(cx, cy - side / 2 - 10, f"box side {side / r:.2f}r", "xs"))

        # the sphere: identical in both panels
        b.append(f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="none" stroke="{GREEN}" '
                 f'stroke-width="1.6"/>')
        b.append(label(cx, cy + r + 20, tag, "xs"))

    # The two panels show ONE yaw, so their own ratio is smaller than the full
    # sweep's. Say both numbers rather than let a reader divide and disagree.
    b.append(label(185, 296, "at 30 deg the dashed box is already 14% wider", "xs"))
    b.append(label(520, 296, f"over a FULL turn it spans {BOX_SPREAD:.1f}%; the circle, "
                              f"{YAW_SPREAD:.1e}", "xs"))
    b.append(label(W / 2, 318,
                   "and world_per_texel is the box side over the resolution — so a box that "
                   "resizes resizes every texel in the map", "xs"))
    return svg("f69c", W, H, "A corner-fitted box against a bounding sphere",
               "Two panels showing the same frustum slice at two camera yaws. The dashed "
               "axis-aligned box that contains it changes size by 30 per cent between them; "
               "the bounding circle is identical in both.", b)


# ===========================================================================
# Figure 4 — texel snapping                                            (§4.3)
# ===========================================================================
def fig_snap():
    W, H = 720, 348
    b = []
    b.append(label(W / 2, 22, "The grid that slides, and the grid that jumps", "sm"))

    cell = 30.0
    # The camera walks 0.4 of a texel per frame. Geometry does NOT move: the
    # purple bar sits at the same screen x in every panel, because it is fixed in
    # the world. What moves is the light's box, and with it the texel grid.
    walk = [0.0, 0.4, 0.8, 1.2]
    bar_x = 66.0        # within a panel

    for y, snap, tag in ((82, False, "UNSNAPPED — the box centre slides by 0.4 texel each frame"),
                         (218, True, "SNAPPED — it is rounded down, so it holds, then jumps one whole texel")):
        b.append(label(96, y - 44, tag, "xs", "start"))
        for f, w in enumerate(walk):
            x0 = 96 + f * 152
            shift = (math.floor(w) if snap else w) * cell

            for i in range(6):
                gx = x0 + i * cell - shift
                if x0 - 2 <= gx <= x0 + 124:
                    b.append(rule(gx, y - 24, gx, y + 24, "grid", 0.9))

            # fixed geometry
            b.append(f'<rect x="{x0 + bar_x:.1f}" y="{y - 19}" width="4" height="38" rx="1" '
                     f'fill="{PURPLE}" stroke="{PURPLE}" stroke-width="1"/>')

            # the shadow edge lands on the grid line at or before the geometry
            k = math.floor((bar_x + shift) / cell)
            land = x0 + k * cell - shift
            b.append(rule(land, y - 30, land, y + 30, "hi", 2.2))

            gap = (bar_x + shift) / cell - k
            b.append(label(x0 + 62, y + 46, f"frame {f}", "xs"))
            b.append(label(x0 + 62, y + 60, f"edge off by {gap:.1f} texel", "xs"))

    b.append(label(W / 2, 320,
                   "the purple bar is fixed geometry; the highlighted line is where its shadow "
                   "edge lands", "xs"))
    b.append(label(W / 2, 340,
                   f"unsnapped the offset changes every frame — that is the crawl. Measured drift: "
                   f"{SNAP_OFF:.3f} texels against {SNAP_ON:.1e} snapped", "xs"))
    return svg("f69d", W, H, "Texel snapping",
               "Two rows of four frames as a camera walks 0.4 of a texel each frame. The purple "
               "bar is fixed geometry in both. In the unsnapped row the texel grid slides "
               "continuously and the shadow edge sits at a different offset from the geometry "
               "every frame. In the snapped row the grid holds still for two frames and then "
               "jumps a whole texel, so the offset repeats.", b)


# ===========================================================================
# Figure 5 — the bias that cancels                                     (§4.4)
# ===========================================================================
def fig_bias():
    W, H = 720, 320
    b = []
    b.append(label(W / 2, 22, "The same formula, four cascades, and what cancels", "sm"))

    x0 = 110
    colw = 132
    top = 62

    heads = ("cascade", "world_per_texel", "depth_range", "bias (device)")
    for i, h in enumerate(heads):
        b.append(label(x0 + i * colw, top, h, "xs"))
    b.append(rule(x0 - 56, top + 10, x0 + 3.4 * colw, top + 10, "grid", 1.0))

    ranges = [31.931, 40.060, 69.924, 147.952]
    biases = [1.292e-03, 2.031e-03, 2.031e-03, 2.031e-03]
    for i in range(4):
        y = top + 40 + i * 34
        cls = "t-bad" if i == 0 else "t-ok"
        b.append(label(x0, y, f"{i}", "xs"))
        b.append(label(x0 + colw, y, f"{WPT[i]:.5f} m", "xs"))
        b.append(label(x0 + 2 * colw, y, f"{ranges[i]:7.3f} m", "xs"))
        b.append(f'<text x="{x0 + 3 * colw}" y="{y}" class="xs {cls}" '
                 f'text-anchor="middle">{biases[i]:.3e}</text>')

    b.append(rule(x0 - 56, top + 40 + 4 * 34 - 22, x0 + 3.4 * colw, top + 40 + 4 * 34 - 22,
                  "grid", 1.0))
    b.append(label(W / 2, 236,
                   f"world_per_texel spans {BIAS_WORLD_RATIO:.1f}x — and three of the four "
                   f"biases are the same number", "xs"))
    b.append(label(W / 2, 258,
                   "because bias = reach·wpt/range, wpt = 2r/res and range ≈ 2r, "
                   "so the radius cancels:", "xs"))
    b.append(label(W / 2, 280, f"bias ≈ reach·tanθ/resolution = {BIAS_PREDICTED:.4e}", "xs"))
    b.append(label(W / 2, 302,
                   f"cascade 0 is the exception at {BIAS_C0_PCT}%: its range is set by the "
                   f"CASTERS, not its own sphere, so nothing cancels", "xs"))
    return svg("f69e", W, H, "Per-cascade bias",
               "A four-row table of world_per_texel, depth range and the resulting device-space "
               "bias for each cascade. The texel size spans a factor of 7.3 but cascades 1, 2 "
               "and 3 all need the same bias, because the sphere radius cancels between the "
               "texel size and the depth range.", b)


# ===========================================================================
# Figure 6 — the seam                                                  (§4.5)
# ===========================================================================
def fig_seam():
    W, H = 720, 280
    b = []
    b.append(label(W / 2, 22, "Where two cascades meet", "sm"))

    x0, x1 = 90, 640
    split = 360
    y = 120
    h = 66

    b.append(f'<rect x="{x0}" y="{y - h / 2}" width="{split - x0}" height="{h}" rx="3" '
             f'fill="{GREEN}" stroke="{GREEN}" fill-opacity="0.30" stroke-width="1.2"/>')
    b.append(f'<rect x="{split}" y="{y - h / 2}" width="{x1 - split}" height="{h}" rx="3" '
             f'fill="{BLUE}" stroke="{BLUE}" fill-opacity="0.30" stroke-width="1.2"/>')
    b.append(label((x0 + split) / 2, y + 4, "cascade 1", "xs"))
    b.append(label((split + x1) / 2, y + 4, "cascade 2", "xs"))

    # the fine and coarse grids either side
    for i in range(int((split - x0) / 12) + 1):
        b.append(rule(x0 + i * 12, y - h / 2, x0 + i * 12, y + h / 2, "grid", 0.5))
    for i in range(int((x1 - split) / 30) + 1):
        b.append(rule(split + i * 30, y - h / 2, split + i * 30, y + h / 2, "grid", 0.5))

    b.append(rule(split, y - h / 2 - 22, split, y + h / 2 + 22, "hi", 2.2))
    b.append(label(split, y - h / 2 - 30, f"{SPLITS[1]:.2f} m", "xs"))

    band = 54
    b.append(f'<rect x="{split - band}" y="{y - h / 2}" width="{band}" height="{h}" rx="0" '
             f'fill="{AMBER}" stroke="none" fill-opacity="0.40"/>')
    b.append(label(split - band / 2, y + h / 2 + 20, "blend band", "xs"))

    b.append(label(W / 2, 214,
                   "different texel grids and different biases, so the two answers disagree "
                   "along one line — and a line is exactly what the eye finds", "xs"))
    b.append(label(W / 2, 238,
                   "blend_fraction fades across a band: both answers are defensible, so "
                   "anything between them is too", "xs"))
    b.append(label(W / 2, 262,
                   "it costs a second lookup, but only for fragments inside the band", "xs"))
    return svg("f69f", W, H, "The seam between two cascades",
               "Two adjacent bands representing cascades 1 and 2, with a fine texel grid on "
               "the near side and a coarse one on the far side. A highlighted line marks the "
               "split distance, and an amber band either side of it marks where the two are "
               "blended together.", b)


def main():
    figs = {
        "l69_fig1.svg": fig_problem(),
        "l69_fig2.svg": fig_splits(),
        "l69_fig3.svg": fig_sphere(),
        "l69_fig4.svg": fig_snap(),
        "l69_fig5.svg": fig_bias(),
        "l69_fig6.svg": fig_seam(),
    }
    for name, body in figs.items():
        with open(os.path.join(OUT, name), "w") as fh:
            fh.write(body)
        print(f"  wrote {OUT}/{name}  ({len(body)} bytes)")


if __name__ == "__main__":
    main()
