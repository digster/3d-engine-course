#!/usr/bin/env python3
"""scratch/figs_68.py — Lesson 6.8's diagrams.

Same rules as 6.1-6.7's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - every element well inside the viewBox
  - ~5.2 units per character for `xs`, ~6.0 for `sm`; compute column spans from
    the WIDEST entry
  - filenames numbered by PAGE ORDER
  - a truncated bar axis lies; bars start at zero
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - SVG COLLAPSES WHITESPACE (6.7's finding): a single <text> cannot hold two
    columns separated by spaces. Two labels, two x positions.

Every number comes from verify_68's output.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)

OUT = "scratch"

# ---- measured, from verify_68 ----------------------------------------------
CHECKS = 53
ACNE_PCT = 50.1          # §C, with no bias
WORST_ERR = 1.931e-03    # §C, device depth
BOUND = 2.101e-03        # §C, derived
TIGHTNESS = 92           # per cent of the bound the measurement reaches
THETA = 49.69            # degrees, the test plane against the light
TAN_THETA = 1.1786
REACH_1 = 0.7071
REACH_3 = 2.1213
PCF_WRONG = 79.6         # §E, 3x3 with a one-tap bias
PERSP_RATIO = 100        # §A, near/far depth resolution under perspective
ACNE_PX = 36786          # the render, pixels differing from the fixed one
LIGHT_BLOCK = 176        # bytes


# ===========================================================================
# Figure 1 — the problem: n.l cannot see the rest of the scene   (§1)
# ===========================================================================
def fig_problem():
    """Two panels: the question the shading equation asks, and the one we want.

    Every label is placed OUTSIDE the ray field and off every filled shape —
    `check-page.js` flags text sitting on a line or a rect, and it is right to:
    a caption on a dashed amber ray is unreadable in either theme.
    """
    uid = "l68f1"
    W, H = 720, 360
    b = [f'<rect x="0" y="0" width="{W}" height="{H}" class="bg"/>']

    # 60 degrees from the horizontal, so a ray crosses only 94 px of x on its way
    # down to the ground and the whole fan stays inside its panel.
    ang = math.radians(60.0)
    dx, dy = math.cos(ang), math.sin(ang)
    top = 58.0
    gy = 214.0
    ox_rel, oy, ow, oh = 128, 124, 62, 44

    for side, x0 in ((0, 20), (1, 372)):
        b.append(hollow(x0, 34, 328, 254, GREY, dash="4 4"))
        b.append(label(x0 + 164, 24,
                       "What the shading equation asks" if side == 0
                       else "What it needs to ask", "sm"))

        ox = x0 + ox_rel
        b.append(rule(x0 + 20, gy, x0 + 308, gy, "ink-soft", 1.6))

        # The fan, each ray stopping at whatever it meets first. A proper slab
        # test rather than "does it cross the top face": a ray that passes to the
        # LEFT of the box at the top can still enter through its left face lower
        # down, and one drawn straight through a solid caster is a diagram that
        # contradicts its own caption.
        for k in range(5):
            sx = x0 + 34.0 + k * 42.0
            t = (gy - top) / dy
            tx0 = (ox - sx) / dx
            tx1 = (ox + ow - sx) / dx
            ty0 = (oy - top) / dy
            ty1 = (oy + oh - top) / dy
            t_in = max(tx0, ty0)
            t_out = min(tx1, ty1)
            if t_in < t_out and 0.0 < t_in < t:
                t = t_in
            b.append(arrow(sx, top, sx + dx * t, top + dy * t, uid, "h", 1.0, dash="3 3"))
        b.append(label(x0 + 236, 48, "the sun", "xs", "start"))

        b.append(box(ox, oy, ow, oh, AMBER, opacity=0.30))
        b.append(hollow(ox, oy, ow, oh, AMBER))

        # A is in the open; B is under the caster's shadow.
        drop = (gy - (oy + oh)) / dy
        sh_l = ox + dx * drop
        sh_r = (ox + ow) + dx * drop
        for px, tag in ((x0 + 70, "A"), ((sh_l + sh_r) / 2.0, "B")):
            b.append(f'<circle cx="{px:.1f}" cy="{gy}" r="4" fill="{BLUE}"/>')
            b.append(label(px, gy + 20, tag, "xs"))

        if side == 0:
            b.append(label(x0 + 164, 252, "n.l is the same at A and at B", "sm"))
            b.append(label(x0 + 164, 274, "so both are drawn fully lit", "xs"))
        else:
            b.append(f'<rect x="{sh_l:.1f}" y="{gy - 7}" width="{sh_r - sh_l:.1f}"'
                     f' height="7" fill="{PURPLE}" fill-opacity="0.6"/>')
            b.append(label(x0 + 164, 252, "B cannot SEE the light", "sm"))
            b.append(label(x0 + 164, 274, "and nothing in the equation knows", "xs"))

    b.append(label(W / 2, 330,
                   "Every light in this engine is unoccluded: shading consults one normal and",
                   "sm"))
    b.append(label(W / 2, 350,
                   "one direction, and never the rest of the scene. That is why objects float.",
                   "xs"))
    return svg(uid, W, H, "Why nothing casts a shadow",
               "Two panels, each showing parallel light rays falling on a ground line past a "
               "rectangular caster. In the left panel points A and B are both drawn lit because "
               "the shading equation only asks whether a surface faces the light. In the right "
               "panel B lies in the caster's shadow, which nothing in the equation can detect.", b)


# ===========================================================================
# Figure 2 — the idea: a z-buffer, pointed at the light          (§2)
# ===========================================================================
def fig_idea():
    """Two passes, side by side. Same placement rules as figure 1.

    Rays stop at the caster by a slab test, every label sits below the ground
    line or in a corner the fan cannot reach, and nothing leaves its panel.
    """
    uid = "l68f2"
    W, H = 720, 356
    b = [f'<rect x="0" y="0" width="{W}" height="{H}" class="bg"/>']

    b.append(label(W / 2, 22, "One idea, two passes, and the engine already owns both", "sm"))

    ang = math.radians(60.0)
    dx, dy = math.cos(ang), math.sin(ang)
    gy = 232.0
    box_rel, oy, ow, oh = 105, 140, 60, 40

    # --- PASS 1 -------------------------------------------------------------
    x0 = 24
    b.append(hollow(x0, 40, 312, 248, BLUE))
    b.append(label(x0 + 156, 62, "PASS 1 — from the light", "sm"))
    b.append(label(x0 + 156, 80, "colour: none.  depth: kept.", "xs"))

    ox = x0 + box_rel
    b.append(rule(x0 + 18, gy, x0 + 294, gy, "ink-soft", 1.6))

    # The "film" the light records onto: a line perpendicular to the rays. Rays
    # start ON it, one per texel, which is what a shadow map's resolution IS.
    px_, py_ = dy, -dx                       # perpendicular, up and to the right
    f0x, f0y = x0 + 16.0, 168.0
    flen = 118.0
    b.append(rule(f0x, f0y, f0x + px_ * flen, f0y + py_ * flen, "ink", 2.0))
    b.append(label(x0 + 126, 96, "the map", "xs", "start"))

    for k in range(6):
        L = 8.0 + k * 22.0
        sx = f0x + px_ * L
        sy = f0y + py_ * L
        t = (gy - sy) / dy
        tx0 = (ox - sx) / dx
        tx1 = (ox + ow - sx) / dx
        ty0 = (oy - sy) / dy
        ty1 = (oy + oh - sy) / dy
        t_in, t_out = max(tx0, ty0), min(tx1, ty1)
        kind = "h"
        if t_in < t_out and 0.0 < t_in < t:
            t = t_in
            kind = "b"                        # this one is stopped by the caster
        b.append(arrow(sx, sy, sx + dx * t, sy + dy * t, uid, kind, 1.1))

    b.append(box(ox, oy, ow, oh, AMBER, opacity=0.30))
    b.append(hollow(ox, oy, ow, oh, AMBER))
    b.append(label(x0 + 156, 258, "each texel stores the NEAREST depth along its ray", "xs"))
    b.append(label(x0 + 156, 278, "and the red ray is the one that stops early", "xs"))

    # --- PASS 2 -------------------------------------------------------------
    x1 = 384
    b.append(hollow(x1, 40, 312, 248, GREEN))
    b.append(label(x1 + 156, 62, "PASS 2 — from the camera", "sm"))
    b.append(label(x1 + 156, 80, "for each fragment: project, then compare", "xs"))

    ox2 = x1 + box_rel
    b.append(rule(x1 + 18, gy, x1 + 294, gy, "ink-soft", 1.6))
    b.append(box(ox2, oy, ow, oh, AMBER, opacity=0.30))
    b.append(hollow(ox2, oy, ow, oh, AMBER))

    drop = (gy - (oy + oh)) / dy
    fxp = (ox2 + dx * drop + ox2 + ow + dx * drop) / 2.0
    b.append(f'<circle cx="{fxp:.1f}" cy="{gy}" r="4.5" fill="{BLUE}"/>')
    b.append(label(fxp, gy + 20, "fragment", "xs"))

    # its own ray, back toward the light, dashed: the lookup direction
    b.append(arrow(fxp, gy, fxp - dx * 150.0, gy - dy * 150.0, uid, "i", 1.3, dash="4 3"))
    b.append(label(x1 + 208, 132, "project it into the", "xs", "start"))
    b.append(label(x1 + 208, 150, "light's space, and look", "xs", "start"))
    b.append(label(x1 + 208, 168, "up what was stored", "xs", "start"))
    b.append(label(x1 + 156, 278, "further than stored  =  something is in front", "xs"))

    b.append(label(W / 2, 322,
                   "The first pass is Lesson 3.1's z-buffer with the camera moved. The second is "
                   "one", "sm"))
    b.append(label(W / 2, 342,
                   "matrix multiply and one comparison. Everything hard is in the comparison.",
                   "xs"))
    return svg(uid, W, H, "A shadow map is a z-buffer rendered from the light",
               "Two panels. Pass 1 renders depth only from the light's viewpoint: parallel rays "
               "leave a film, one per texel, and each records the nearest surface it meets — one "
               "of them stopping early at a caster. Pass 2 takes a camera fragment on the ground, "
               "projects it back along the light's direction, and compares its depth against what "
               "the map stored.", b)


# ===========================================================================
# Figure 3 — orthographic: a box, not a pyramid                  (§3)
# ===========================================================================
def fig_ortho():
    uid = "l68f3"
    W, H = 720, 340
    b = [f'<rect x="0" y="0" width="{W}" height="{H}" class="bg"/>']

    b.append(label(W / 2, 22, "A directional light has no position, so its frustum is a box", "sm"))

    # --- left: perspective, for contrast
    b.append(label(176, 46, "perspective (2.10)", "sm"))
    ex, ey = 60, 168
    b.append(f'<circle cx="{ex}" cy="{ey}" r="5" fill="{RED}"/>')
    b.append(label(ex, ey + 22, "eye", "xs"))
    for sy in (96, 240):
        b.append(rule(ex, ey, 300, sy, "ink-soft", 1.3))
    b.append(rule(140, 133, 140, 203, "ink", 1.6))
    b.append(rule(300, 96, 300, 240, "ink", 1.6))
    b.append(label(140, 124, "near", "xs"))
    b.append(label(300, 88, "far", "xs"))
    b.append(label(176, 274, "rays converge, so w = -z", "xs"))
    b.append(label(176, 292, "and depth is crowded near", "xs"))

    # --- right: orthographic
    b.append(label(536, 46, "orthographic (6.8)", "sm"))
    ang = 0.0
    for k in range(5):
        y = 106 + k * 30
        b.append(arrow(392, y, 664, y, uid, "h", 1.1, dash="3 3"))
    b.append(rule(432, 96, 432, 246, "ink", 1.6))
    b.append(rule(636, 96, 636, 246, "ink", 1.6))
    b.append(hollow(432, 96, 204, 150, BLUE, dash="5 4"))
    b.append(label(432, 88, "near", "xs"))
    b.append(label(636, 88, "far", "xs"))
    b.append(label(536, 274, "rays are parallel, so w = 1 EXACTLY", "xs"))
    b.append(label(536, 292, "and depth is spread evenly", "xs"))

    b.append(label(W / 2, 322,
                   f"Checked: one metre of depth is worth the same device depth at both planes; "
                   f"under perspective the ratio is over {PERSP_RATIO}x.", "xs"))
    return svg(uid, W, H, "Perspective versus orthographic",
               "Left: a perspective frustum's rays converge on an eye, so w equals minus z and "
               "depth resolution crowds against the near plane. Right: an orthographic box's rays "
               "are parallel, w stays exactly 1, and depth is spread evenly.", b)


# ===========================================================================
# Figure 4 — THE ARTEFACT: what the first working build looks like  (§4)
# ===========================================================================
def fig_artefact():
    """A mock of `gltf_view --bias none`, and the fringes are COMPUTED.

    Acne appears wherever a fragment falls on the downhill half of its texel, so
    the dark bands are the level sets of the light-space texel coordinate seen
    through the camera's projection. On a ground plane that coordinate is a
    PROJECTIVE function of screen position — (a*x + b) / (y - horizon) — whose
    level sets are hyperbolae that bunch toward the horizon. That is the whole
    reason real acne swirls rather than striping evenly, and drawing the fringes
    from the actual formula is the difference between a diagram and a doodle.
    """
    uid = "l68f4"
    W, H = 720, 392
    b = [f'<rect x="0" y="0" width="{W}" height="{H}" class="bg"/>']

    b.append(label(W / 2, 22,
                   "What the first correct implementation puts on screen", "sm"))

    # --- the frame
    fx, fy, fw, fh = 60, 40, 600, 292
    b.append(f'<rect x="{fx}" y="{fy}" width="{fw}" height="{fh}" rx="3"'
             f' fill="#14161c" stroke="{GREY}" stroke-width="1.2"/>')
    b.append(f'<clipPath id="{uid}-clip"><rect x="{fx}" y="{fy}" width="{fw}"'
             f' height="{fh}" rx="3"/></clipPath>')
    b.append(f'<g clip-path="url(#{uid}-clip)">')

    # --- the ground plane: a trapezoid, near edge wide, far edge narrow
    horizon = fy + 74.0
    near_y = fy + fh - 26.0
    near_half, far_half = 300.0, 96.0
    cx = fx + fw / 2.0

    def half_at(y):
        t = (y - horizon) / (near_y - horizon)
        return far_half * (1 - t) + near_half * t

    b.append(f'<path d="M {cx - far_half:.1f} {horizon + 22:.1f}'
             f' L {cx + far_half:.1f} {horizon + 22:.1f}'
             f' L {cx + near_half:.1f} {near_y:.1f}'
             f' L {cx - near_half:.1f} {near_y:.1f} Z"'
             f' fill="#6f7178" stroke="none"/>')

    # --- THE FRINGES, as level sets of a projective coordinate.
    #
    # u(x, y) = K * (x - cx) / (y - horizon) + S / (y - horizon) is the texel
    # coordinate a ground fragment maps to; each contour u = const is one edge of
    # the acne banding. Two families, because the map has two axes.
    # d = y - horizon is the projective denominator; a contour of
    # u = (K*(x-cx) + S)/d is the line x = cx + (u*d - S)/K. K sets the fringe
    # SPACING at the near edge (d/K px apart) and therefore how fast they crowd
    # toward the horizon; S slides the whole family sideways. Two families,
    # because the light's uv grid has two axes and both alias.
    ys = [horizon + 22 + i * 4.0 for i in range(int((near_y - horizon - 22) / 4.0))]
    for family, (K, S, phase) in enumerate(((9.6, 0.0, 0.0),
                                            (-6.0, 300.0, 0.5))):
        for k in range(-40, 41):
            pts = []
            target = k + phase
            for y in ys:
                d = y - horizon
                # solve K*(x-cx)/d + S/d = target  ->  x = cx + (target*d - S)/K
                x = cx + (target * d - S) / K
                if abs(x - cx) <= half_at(y) + 1.0:
                    pts.append(f"{x:.1f},{y:.1f}")
            if len(pts) > 3:
                b.append(f'<polyline points="{" ".join(pts)}" fill="none"'
                         f' stroke="#20222a" stroke-width="1.5" stroke-opacity="0.85"/>')

    # --- the torus, and its (correct) shadow
    tx, ty = cx - 10, horizon + 118
    b.append(f'<ellipse cx="{tx - 46:.0f}" cy="{ty + 44:.0f}" rx="104" ry="34"'
             f' fill="#3c3e46"/>')
    b.append(f'<ellipse cx="{tx:.0f}" cy="{ty:.0f}" rx="86" ry="46"'
             f' fill="{AMBER}" fill-opacity="0.88"/>')
    b.append(f'<ellipse cx="{tx:.0f}" cy="{ty:.0f}" rx="34" ry="17" fill="#14161c"/>')
    b.append('</g>')

    b.append(label(W / 2, 352,
                   f"{ACNE_PX:,} pixels of a 480x270 frame, and not one line of code "
                   "misbehaved.", "sm"))
    b.append(label(W / 2, 370,
                   "The fringes bunch toward the horizon because they are level sets of a "
                   "projective", "xs"))
    b.append(label(W / 2, 386,
                   "coordinate. An SVG mock, drawn from that formula; the real frame is a PPM.",
                   "xs"))
    return svg(uid, W, H, "Shadow acne, as it actually appears",
               "A mock of the renderer's output with no depth bias: a torus casting a correct "
               "shadow onto a ground plane that is itself covered in a dense moire of dark "
               "curved fringes, bunching toward the horizon.", b)


# ===========================================================================
# Figure 5 — THE DERIVATION: why acne exists and how big it is   (§4)
# ===========================================================================
def fig_acne():
    uid = "l68f5"
    W, H = 720, 476
    b = [f'<rect x="0" y="0" width="{W}" height="{H}" class="bg"/>']

    b.append(label(W / 2, 22,
                   "The map samples ONE point per texel; the fragment is somewhere else", "sm"))

    # the tilted surface, in the light's frame: light travels straight down.
    x0, y0 = 70, 122
    span = 460

    # THE SURFACE FALLS TO THE RIGHT, and the sign matters. The light travels
    # DOWN the page, so larger y is further from it; a fragment must be on the
    # downhill side of its texel's sample to fail the comparison. Draw the slope
    # the other way and the picture illustrates a fragment that passes.
    slope = 0.40                        # fall per unit run, on screen
    def sy(x):
        return y0 + slope * (x - x0)

    # light rays, straight down
    for k in range(11):
        x = x0 + k * (span / 10.0)
        b.append(arrow(x, 66, x, sy(x) - 6, uid, "h", 0.9, dash="3 3"))
    b.append(label(x0 - 4, 60, "the light, straight down in its own frame", "xs", "start"))

    # the surface
    b.append(f'<line x1="{x0}" y1="{sy(x0):.1f}" x2="{x0 + span}" y2="{sy(x0 + span):.1f}"'
             f' class="ink" stroke-width="2.2"/>')
    b.append(label(x0 + span + 6, sy(x0 + span) + 4, "surface", "xs", "start"))

    # texel boundaries and the stored samples (at the boundaries: the CPU
    # rasterizer evaluates attributes at integer pixel coordinates)
    tw = span / 6.0
    for k in range(7):
        x = x0 + k * tw
        b.append(rule(x, 66, x, 330, "grid", 1.0, dash="2 4"))
        b.append(f'<circle cx="{x}" cy="{sy(x):.1f}" r="4" fill="{GREEN}"/>')
    b.append(rule(x0, 336, x0 + tw, 336, "ink-soft", 1.2))
    b.append(label(x0 + tw * 0.5, 350, "one texel", "xs"))

    # a fragment inside a texel, downhill of its nearest sample
    k = 3
    xs_ = x0 + k * tw
    fx = xs_ + tw * 0.42
    b.append(f'<circle cx="{fx:.1f}" cy="{sy(fx):.1f}" r="4.5" fill="{RED}"/>')
    b.append(f'<line x1="{fx:.1f}" y1="{sy(xs_):.1f}" x2="{fx:.1f}" y2="{sy(fx):.1f}"'
             f' stroke="{RED}" stroke-width="2.4"/>')
    b.append(f'<line x1="{xs_:.1f}" y1="{sy(xs_):.1f}" x2="{fx:.1f}" y2="{sy(xs_):.1f}"'
             f' stroke="{GREEN}" stroke-width="1.6" stroke-dasharray="3 3"/>')
    # THE TWO POINTS ARE NAMED IN A KEY BELOW, not labelled in place. Every
    # position near them is crossed by something — the descending surface, a
    # light ray, or a texel boundary — and a caption sitting on a line is
    # unreadable in one theme or the other. The colours carry the identification
    # and the key spells them out; `check-page.js` enforces the rule.

    # the arithmetic, as two labelled columns (SVG collapses whitespace)
    b.append(rule(60, 364, 660, 364, "grid", 1.0))
    b.append(f'<circle cx="70" cy="381" r="4.5" fill="{GREEN}"/>')
    b.append(label(84, 385, "the depth the map STORED, at this texel's sample point",
                   "xs", "start"))
    b.append(f'<circle cx="70" cy="403" r="4.5" fill="{RED}"/>')
    b.append(label(84, 407, "the FRAGMENT being tested, downhill inside the same texel",
                   "xs", "start"))
    b.append(rule(60, 424, 660, 424, "grid", 1.0))
    b.append(label(64, 444, "lateral travel  ≤  reach x world_per_texel", "xs", "start"))
    b.append(label(64, 462, "depth error     =  that, times tan(theta)", "xs", "start"))
    b.append(label(656, 444, f"measured  {WORST_ERR:.3e}", "xs", "end"))
    b.append(label(656, 462, f"bound     {BOUND:.3e}   ({TIGHTNESS}%)", "xs", "end"))
    return svg(uid, W, H, "Where shadow acne comes from",
               "A surface tilted relative to the light, divided into shadow-map texels. The map "
               "stores the depth at one sample per texel; a fragment elsewhere in the texel is "
               "downhill of it, and the difference is the acne. The error is bounded by the "
               "lateral travel times the tangent of the tilt.", b)


# ===========================================================================
# Figure 5 — the three cures, and what each one costs            (§5)
# ===========================================================================
def fig_cures():
    uid = "l68f6"
    W, H = 720, 372
    b = [f'<rect x="0" y="0" width="{W}" height="{H}" class="bg"/>']

    b.append(label(W / 2, 22, "Three ways to make the comparison pass, and three prices", "sm"))

    panels = [
        (18, "constant", RED,
         "one number everywhere",
         "sized for the steepest",
         "surface in the scene, so",
         "every flatter one is lifted",
         "too far  →  PETER-PANNING"),
        (248, "slope-scaled", GREEN,
         "reach x texel x tan(theta)",
         "each surface gets exactly",
         "what its own geometry",
         "needs  —  and diverges as",
         "tan(theta) does, so it clamps"),
        (478, "normal-offset", BLUE,
         "move the POINT, not the depth",
         "along the GEOMETRIC normal",
         "(6.7 made that different",
         "from the shading one). Trades",
         "depth error for lateral error"),
    ]

    for x0, title, col, l1, l2, l3, l4, l5 in panels:
        b.append(hollow(x0, 40, 224, 300, col, dash="4 4"))
        b.append(label(x0 + 112, 62, title, "sm"))

        # a little picture: surface, sample, and where the test point moved to
        sx0, sy0 = x0 + 26, 168
        w = 172
        sl = -0.36
        def yy(x, sx0=sx0, sy0=sy0, sl=sl):
            return sy0 + sl * (x - sx0)
        b.append(f'<line x1="{sx0}" y1="{yy(sx0):.1f}" x2="{sx0 + w}" y2="{yy(sx0 + w):.1f}"'
                 f' class="ink" stroke-width="2"/>')
        mid = sx0 + w * 0.5
        b.append(f'<circle cx="{mid:.1f}" cy="{yy(mid):.1f}" r="4" fill="{GREY}"/>')

        if title == "constant":
            b.append(arrow(mid, yy(mid), mid, yy(mid) - 30, uid, "b", 1.6))
        elif title == "slope-scaled":
            b.append(arrow(mid, yy(mid), mid, yy(mid) - 17, uid, "g", 1.6))
        else:
            # along the normal of the tilted line
            nx, ny = -sl, -1.0
            n = math.hypot(nx, ny)
            b.append(arrow(mid, yy(mid), mid + nx / n * 20, yy(mid) + ny / n * 20,
                           uid, "i", 1.6))

        for i, ln in enumerate((l1, l2, l3, l4, l5)):
            b.append(label(x0 + 112, 224 + i * 17, ln, "xs"))

    b.append(label(W / 2, 362,
                   f"Measured: {ACNE_PCT}% of an unoccluded plane shadows itself with no bias; "
                   f"0.0% with slope-scaled.", "xs"))
    return svg(uid, W, H, "The three bias policies",
               "Three panels. A constant bias lifts every surface by the same amount and causes "
               "peter-panning. A slope-scaled bias lifts each surface by what its own tilt "
               "requires. A normal-offset moves the sample point along the geometric normal "
               "instead of moving its depth.", b)


# ===========================================================================
# Figure 6 — PCF: compare, THEN filter                           (§6)
# ===========================================================================
def fig_pcf():
    uid = "l68f7"
    W, H = 720, 340
    b = [f'<rect x="0" y="0" width="{W}" height="{H}" class="bg"/>']

    b.append(label(W / 2, 22, "Averaging depths is not a blurrier answer. It is a wrong one.", "sm"))

    # shared setup: two stored depths and one receiver
    b.append(hollow(20, 40, 328, 232, RED, dash="4 4"))
    b.append(label(184, 60, "filter, then compare", "sm"))
    b.append(hollow(372, 40, 328, 232, GREEN, dash="4 4"))
    b.append(label(536, 60, "compare, then filter", "sm"))

    for x0, wrong in ((20, True), (372, False)):
        # two texels, drawn as depth bars from the light plane downward
        b.append(rule(x0 + 40, 86, x0 + 288, 86, "ink", 1.6))
        b.append(label(x0 + 164, 78, "the light", "xs"))

        for i, (d, tag) in enumerate(((0.3, "0.3"), (0.9, "0.9"))):
            bx = x0 + 82 + i * 116
            by = 86 + d * 128
            b.append(f'<line x1="{bx}" y1="86" x2="{bx}" y2="{by:.0f}"'
                     f' stroke="{AMBER}" stroke-width="3"/>')
            b.append(f'<circle cx="{bx}" cy="{by:.0f}" r="4" fill="{AMBER}"/>')
            b.append(label(bx + 12, by + 4, tag, "xs", "start"))

        # the receiver at 0.5
        ry = 86 + 0.5 * 128
        b.append(rule(x0 + 52, ry, x0 + 276, ry, "grid", 1.4, dash="5 4"))
        b.append(label(x0 + 286, ry + 4, "0.5", "xs", "start"))

        if wrong:
            my = 86 + 0.6 * 128
            b.append(rule(x0 + 60, my, x0 + 268, my, "ink", 2.0))
            b.append(label(x0 + 164, 232, "mean depth 0.6 > 0.5  →  FULLY LIT", "xs"))
            b.append(label(x0 + 164, 250, "…with half its taps occluded", "xs"))
        else:
            # TWO LABELS, TWO x POSITIONS. SVG collapses runs of whitespace, so a
            # single <text> cannot hold two columns separated by spaces — 6.7's
            # figure 3 learned that the hard way.
            b.append(label(x0 + 88, 232, "0.3 < 0.5 → 0", "xs"))
            b.append(label(x0 + 232, 232, "0.9 > 0.5 → 1", "xs"))
            b.append(label(x0 + 164, 250, "mean visibility 0.5  →  HALF SHADOWED", "xs"))

    b.append(rule(60, 288, 660, 288, "grid", 1.0))
    b.append(label(64, 306,
                   "That is the whole reason SamplerComparisonState exists: the hardware tests "
                   "each texel", "xs", "start"))
    b.append(label(64, 324,
                   f"before it filters. And a 3x3 kernel reaches {REACH_3} texels, not "
                   f"{REACH_1} — so the bias must widen with it.", "xs", "start"))
    return svg(uid, W, H, "Why the comparison comes before the filter",
               "Two panels comparing a receiver at depth 0.5 against stored occluders at 0.3 and "
               "0.9. Filtering first gives a mean depth of 0.6, which reports the receiver fully "
               "lit. Comparing first gives visibilities of 0 and 1, whose mean is 0.5.", b)


def main():
    figs = [
        ("l68_fig1.svg", fig_problem),
        ("l68_fig2.svg", fig_idea),
        ("l68_fig3.svg", fig_ortho),
        ("l68_fig4.svg", fig_artefact),
        ("l68_fig5.svg", fig_acne),
        ("l68_fig6.svg", fig_cures),
        ("l68_fig7.svg", fig_pcf),
    ]
    for name, fn in figs:
        path = os.path.join(OUT, name)
        with open(path, "w") as fh:
            fh.write(fn())
        print(f"wrote {path}")


if __name__ == "__main__":
    main()
