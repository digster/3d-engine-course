#!/usr/bin/env python3
"""scratch/figs_67.py — Lesson 6.7's diagrams.

Same rules as 6.1–6.6's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - every element well inside the viewBox
  - ~5.2 units per character for `xs`, ~6.0 for `sm`; compute column spans from
    the WIDEST entry, which is what 6.6's figure 5 got wrong
  - filenames numbered by PAGE ORDER
  - a truncated bar axis lies; bars start at zero (6.4's lesson)
  - no HTML tags inside <text>; use <tspan class="t-hi">

Every number comes from verify_67's output.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)

OUT = "scratch"

# ---- measured, from verify_67 ----------------------------------------------
CHECKS = 33
AS_DATA = 0.50196
AS_COLOUR = 0.21586
WRONG_TILT = 38.8
FLOOR_ERR = 5.5460e-03
FLOOR_TILT = 0.318
BUMP_ERR = 5.18e-03
BUMP_BOUND = 6.79e-03
SKEW_DEG = 36.9
VERTEX_BEFORE = 32
VERTEX_AFTER = 48


# ===========================================================================
# Figure 1 — the problem: geometry has one normal per vertex  (§1)
# ===========================================================================
def fig_problem():
    uid = "l67f1"
    W, H = 720, 356
    b = [box(0, 0, W, H)]
    b.append(label(W / 2, 26, "A surface is as flat as its triangles", "lg"))

    # Two panels: the geometry we have, and the surface we want.
    pw = 320
    for i, (title, col) in enumerate([("what the mesh says", GREY),
                                      ("what the surface is", GREEN)]):
        x = 24 + i * (pw + 32)
        b.append(hollow(x, 46, pw, 200, col))
        b.append(label(x + pw / 2, 68, title, "sm"))
        b.append(rule(x + 14, 78, x + pw - 14, 78, "grid", 1.0))

    # ---- left: a flat facet with three identical normals -------------------
    lx = 24
    base_y = 200
    b.append(rule(lx + 40, base_y, lx + pw - 40, base_y, "ink", 1.6))
    for j in range(5):
        px = lx + 50 + j * 55
        b.append(arrow(px, base_y - 4, px, base_y - 62, uid, "i", 1.3))
    b.append(label(lx + pw / 2, base_y + 24, "one normal, interpolated", "xs"))
    b.append(label(lx + pw / 2, base_y + 40, "— every pixel agrees", "xs"))

    # ---- right: the same facet, bumpy ---------------------------------------
    rx = 24 + pw + 32
    pts = []
    for j in range(41):
        t = j / 40.0
        px = rx + 40 + t * (pw - 80)
        py = base_y - 14 * math.sin(t * math.pi * 4.0)
        pts.append((px, py))
    d = " ".join(f"{px:.1f},{py:.1f}" for px, py in pts)
    b.append(f'<polyline points="{d}" class="ink" fill="none" stroke-width="1.6"/>')

    for j in range(5):
        t = (j + 0.5) / 5.0
        px = rx + 40 + t * (pw - 80)
        py = base_y - 14 * math.sin(t * math.pi * 4.0)
        # the true normal of the wiggle: (-h', 1) normalised
        slope = -14 * math.cos(t * math.pi * 4.0) * math.pi * 4.0 / (pw - 80)
        nx, ny = -slope, -1.0
        L = math.hypot(nx, ny)
        b.append(arrow(px, py, px + nx / L * 58, py + ny / L * 58, uid, "h", 1.3))
    b.append(label(rx + pw / 2, base_y + 24, "a normal PER PIXEL", "xs"))
    b.append(label(rx + pw / 2, base_y + 40, "— and no more triangles", "xs"))

    b.append(box(24, 268, W - 48, 74, None))
    b.append(label(W / 2, 292,
                   "The cheapest way to add detail is to lie about the normal, not to add geometry.",
                   "sm"))
    b.append(label(W / 2, 314,
                   "A normal map stores the difference — but a stored direction needs a FRAME to", "xs"))
    b.append(label(W / 2, 330,
                   "be expressed in, and finding that frame is the whole of this lesson.", "xs"))

    return svg(uid, W, H,
               "A flat facet against the same facet with per-pixel normals",
               "On the left a straight line with five identical upward arrows: "
               "the normal a triangle actually has, the same at every pixel. On "
               "the right the same span drawn as a wavy line with five arrows "
               "each perpendicular to the local slope, which is the surface a "
               "normal map describes without adding a single triangle.", b)


# ===========================================================================
# Figure 2 — colour or data  (§3)
# ===========================================================================
def fig_space():
    uid = "l67f2"
    W, H = 720, 390
    b = [box(0, 0, W, H)]
    b.append(label(W / 2, 26, "The byte 128, read two ways", "lg"))

    # A bar from 0 to 1 with the two readings marked. Zero-based, per 6.4.
    bx, bw, by = 90, W - 180, 74
    b.append(box(bx, by, bw, 26, GREY, opacity=0.10))
    b.append(hollow(bx, by, bw, 26, GREY))
    for t, lab in ((0.0, "0.0"), (0.5, "0.5"), (1.0, "1.0")):
        px = bx + t * bw
        b.append(rule(px, by, px, by + 26, "grid", 1.0))
        b.append(label(px, by + 44, lab, "xs"))

    for value, col, name, y in ((AS_DATA, GREEN, "as DATA", by - 12),
                                (AS_COLOUR, RED, "as COLOUR", by - 12)):
        px = bx + value * bw
        b.append(rule(px, by - 4, px, by + 30, "hi", 2.0))
        b.append(box(px - 46, y - 20, 92, 18, col, opacity=0.30))
        b.append(label(px, y - 7, f"{name}  {value:.3f}", "xs"))

    b.append(label(W / 2, by + 66,
                   "the same eight bits, and the gap is the sRGB transfer function", "xs"))

    # ---- the consequence, as two frames ------------------------------------
    b.append(label(W / 2, 172, "…and what that does to a FLAT normal map", "sm"))

    cw = 300
    cases = [("texel_space::linear", GREEN, "(0.004, 0.004, 1.0)",
              f"tilts the surface {FLOOR_TILT:.3f} deg", "the quantisation floor"),
             ("texel_space::srgb", RED, "(-0.568, -0.568, 1.0)",
              f"tilts the surface {WRONG_TILT:.1f} deg", "everywhere, one direction")]
    for i, (title, col, decoded, tilt, note) in enumerate(cases):
        x = 24 + i * (cw + 72)
        b.append(box(x, 194, cw, 122, col, opacity=0.09))
        b.append(hollow(x, 194, cw, 122, col))
        b.append(label(x + cw / 2, 216, title, "sm"))
        b.append(rule(x + 14, 226, x + cw - 14, 226, "grid", 1.0))
        b.append(label(x + cw / 2, 248, "(128,128,255) decodes to", "xs"))
        b.append(label(x + cw / 2, 268, decoded, "xs"))
        b.append(box(x + 40, 278, cw - 80, 20, col, opacity=0.30))
        b.append(label(x + cw / 2, 292, tilt, "xs"))
        b.append(label(x + cw / 2, 310, note, "xs"))

    b.append(label(W / 2, H - 46,
                   "The GPU has had this since 4.7 — create_sampled(…, srgb) picks _UNORM_SRGB or", "xs"))
    b.append(label(W / 2, H - 28,
                   "_UNORM. The software renderer had no counterpart until this lesson.", "xs"))
    b.append(label(W / 2, H - 10,
                   "Two halves of one idea, and no code path crossed between them.", "sm"))

    return svg(uid, W, H,
               "The byte 128 decoded as data and as colour, and the consequence",
               "A bar from 0 to 1 with two marks: 0.502 read as data and 0.216 "
               "read as colour, the gap being the sRGB transfer function. Below, "
               "two panels showing what a flat normal map becomes under each "
               "reading — a surface tilted 0.318 degrees, which is the "
               "quantisation floor, against one tilted 38.8 degrees.", b)


# ===========================================================================
# Figure 3 — the derivation  (§4)
# ===========================================================================
def fig_derivation():
    uid = "l67f3"
    W, H = 720, 420
    b = [box(0, 0, W, H)]
    b.append(label(W / 2, 26, "T and B are whatever makes the two pictures agree", "lg"))

    # ---- left: the triangle in space ---------------------------------------
    lx, lw = 24, 300
    b.append(hollow(lx, 46, lw, 208, BLUE))
    b.append(label(lx + lw / 2, 68, "in SPACE (metres)", "sm"))

    p0 = (lx + 70, 214)
    p1 = (lx + 236, 214)
    p2 = (lx + 70, 108)
    tri = f"{p0[0]},{p0[1]} {p1[0]},{p1[1]} {p2[0]},{p2[1]}"
    b.append(f'<polygon points="{tri}" fill="{BLUE}" fill-opacity="0.12" '
             f'stroke="{BLUE}" stroke-width="1.3"/>')
    b.append(arrow(p0[0], p0[1], p1[0], p1[1], uid, "h", 1.8))
    b.append(arrow(p0[0], p0[1], p2[0], p2[1], uid, "h", 1.8))
    b.append(label((p0[0] + p1[0]) / 2, p0[1] + 20, "e1 = p1 - p0 = (2,0,0)", "xs"))
    b.append(label(p0[0] - 6, (p0[1] + p2[1]) / 2, "e2 =", "xs", anchor="end"))
    b.append(label(p0[0] - 6, (p0[1] + p2[1]) / 2 + 16, "(0,0,-2)", "xs", anchor="end"))
    b.append(label(p0[0] - 4, p0[1] + 4, "p0", "xs", anchor="end"))
    b.append(label(p1[0] + 6, p1[1] + 4, "p1", "xs", anchor="start"))
    b.append(label(p2[0] - 4, p2[1] - 6, "p2", "xs", anchor="end"))

    # ---- right: the same triangle in the uv chart ---------------------------
    rx0, rw = 396, 300
    b.append(hollow(rx0, 46, rw, 208, AMBER))
    b.append(label(rx0 + rw / 2, 68, "in the uv CHART (texture units)", "sm"))

    q0 = (rx0 + 70, 214)
    q1 = (rx0 + 236, 214)
    q2 = (rx0 + 70, 108)
    tri2 = f"{q0[0]},{q0[1]} {q1[0]},{q1[1]} {q2[0]},{q2[1]}"
    b.append(f'<polygon points="{tri2}" fill="{AMBER}" fill-opacity="0.12" '
             f'stroke="{AMBER}" stroke-width="1.3"/>')
    b.append(arrow(q0[0], q0[1], q1[0], q1[1], uid, "h", 1.8))
    b.append(arrow(q0[0], q0[1], q2[0], q2[1], uid, "h", 1.8))
    b.append(label((q0[0] + q1[0]) / 2, q0[1] + 20, "(du1, dv1) = (1, 0)", "xs"))
    b.append(label(q0[0] - 6, (q0[1] + q2[1]) / 2, "(du2, dv2)", "xs", anchor="end"))
    b.append(label(q0[0] - 6, (q0[1] + q2[1]) / 2 + 16, "= (0, 1)", "xs", anchor="end"))

    b.append(arrow(lx + lw + 12, 150, rx0 - 12, 150, uid, "s", 1.4))
    b.append(label(W / 2, 140, "same two", "xs"))
    b.append(label(W / 2, 172, "directions", "xs"))

    # ---- the two equations and their answer ---------------------------------
    b.append(box(24, 268, W - 48, 138, None))
    b.append(label(W * 0.28, 292, "e1 = du1 * T + dv1 * B", "sm"))
    b.append(label(W * 0.72, 292, "e2 = du2 * T + dv2 * B", "sm"))
    b.append(label(W / 2, 314,
                   "Two equations, two unknowns — so invert the 2x2 of uv deltas:", "xs"))
    b.append(label(W * 0.28, 340, "det = du1*dv2 - du2*dv1 = 1", "xs"))
    b.append(label(W * 0.72, 340, "T = (dv2*e1 - dv1*e2)/det = (2,0,0)", "xs"))
    b.append(label(W * 0.72, 358, "B = (du1*e2 - du2*e1)/det = (0,0,-2)", "xs"))
    b.append(rule(150, 374, W - 150, 374, "grid", 1.0))
    b.append(label(W / 2, 394,
                   "T normalises to (1,0,0), B to (0,0,-1) — and verify_67 §B gets exactly that.",
                   "sm"))

    return svg(uid, W, H,
               "The tangent frame derived from two edges and two uv deltas",
               "The same triangle drawn twice: once in space with its two edges "
               "labelled e1 = (2,0,0) and e2 = (0,0,-2), and once in the uv chart "
               "with the corresponding deltas (1,0) and (0,1). Below, the two "
               "simultaneous equations that define T and B, and the 2x2 inversion "
               "that solves them.", b)


# ===========================================================================
# Figure 4 — the basis change, and the lavender  (§5)
# ===========================================================================
def fig_basis():
    uid = "l67f4"
    W, H = 720, 384
    b = [box(0, 0, W, H)]
    b.append(label(W / 2, 26, "Tangent space to world, in one multiply", "lg"))

    # ---- the encoding ladder ------------------------------------------------
    steps = [
        ("the image", "(128, 128, 255)", "three bytes", GREY),
        ("decoded", "(0.502, 0.502, 1.000)", "byte / 255 — NOT sRGB", BLUE),
        ("* 2 - 1", "(0.004, 0.004, 1.000)", "a DIRECTION has negatives", PURPLE),
        ("T*x + B*y + N*z", "= N, exactly", "the round trip, verify_67 §D", GREEN),
    ]
    sw = 156
    gap = (W - 48 - 4 * sw) / 3
    for i, (name, value, note, col) in enumerate(steps):
        x = 24 + i * (sw + gap)
        b.append(box(x, 52, sw, 104, col, opacity=0.10))
        b.append(hollow(x, 52, sw, 104, col))
        b.append(label(x + sw / 2, 74, name, "sm"))
        b.append(rule(x + 12, 82, x + sw - 12, 82, "grid", 1.0))
        b.append(label(x + sw / 2, 104, value, "xs"))
        b.append(label(x + sw / 2, 132, note, "xs"))
        if i < 3:
            b.append(arrow(x + sw + 4, 104, x + sw + gap - 4, 104, uid, "i", 1.4))

    # ---- why lavender -------------------------------------------------------
    b.append(box(24, 176, W - 48, 78, AMBER, opacity=0.07))
    b.append(hollow(24, 176, W - 48, 78, AMBER))
    b.append(label(W / 2, 200, "Why an unperturbed normal map is LAVENDER", "sm"))
    b.append(label(W / 2, 222,
                   "(0, 0, 1) means “no change”, and packing it into bytes gives "
                   "(128, 128, 255)", "xs"))
    b.append(label(W / 2, 242,
                   "— a pale blue-violet. It is not a convention somebody chose; "
                   "it is arithmetic.", "xs"))

    # ---- and the floor ------------------------------------------------------
    b.append(box(24, 268, W - 48, 100, RED, opacity=0.06))
    b.append(hollow(24, 268, W - 48, 100, RED))
    b.append(label(W / 2, 292, "…and “no change” is not exactly representable", "sm"))
    b.append(label(W / 2, 314,
                   "0.5 is not an 8-bit code. The nearest is 128, and 128/255*2-1 = 1/255,", "xs"))
    b.append(label(W / 2, 332,
                   f"so the flattest storable map still tilts its surface by "
                   f"{FLOOR_TILT:.3f} degrees.", "xs"))
    b.append(box(230, 342, 260, 20, RED, opacity=0.26))
    b.append(label(W / 2, 356,
                   f"measured floor  {FLOOR_ERR:.4e}  — matched to 7 digits", "xs"))

    return svg(uid, W, H,
               "The four steps from stored bytes to a world-space normal",
               "Four linked boxes: the image's three bytes (128, 128, 255), "
               "decoded by dividing by 255 rather than through the sRGB curve, "
               "then mapped from [0,1] to [-1,1] giving very nearly (0, 0, 1), "
               "and finally combined with the tangent frame to return the "
               "geometric normal. Below, why an unperturbed map is lavender, and "
               "why the round trip's error floor is not zero.", b)


# ===========================================================================
# Figure 5 — which matrix carries a tangent  (§6)
# ===========================================================================
def fig_matrix():
    uid = "l67f5"
    W, H = 720, 400
    b = [box(0, 0, W, H)]
    b.append(label(W / 2, 26, "A normal is perpendicular. A tangent is not.", "lg"))

    # Two panels: before and after a non-uniform scale.
    pw = 300
    lx, rx0 = 24, 396

    def surface(x, y, w, squash, col, title):
        out = [hollow(x, 46, pw, 236, col), label(x + pw / 2, 68, title, "sm")]
        out.append(rule(x + 14, 78, x + pw - 14, 78, "grid", 1.0))
        return out

    b += surface(lx, 0, pw, 1.0, GREY, "before")
    b += surface(rx0, 0, pw, 2.0, AMBER, "after a scale of (2, 1, 1)")

    # A little patch of surface with N and T drawn on it, twice.
    for x, sx, col in ((lx, 1.0, GREY), (rx0, 2.0, AMBER)):
        cx, cy = x + pw / 2, 210
        half = 78
        b.append(rule(cx - half, cy, cx + half, cy, "ink", 1.6))
        # N: straight up in both, because the surface is the same plane
        b.append(arrow(cx, cy, cx, cy - 76, uid, "i", 1.8))
        b.append(label(cx + 12, cy - 80, "N", "sm", anchor="start"))
        # T: at 45 degrees before; the model matrix stretches x
        tx, ty = 1.0 * sx, 1.0
        L = math.hypot(tx, ty)
        b.append(arrow(cx, cy, cx + tx / L * 76, cy - ty / L * 76, uid, "h", 1.8))
        b.append(label(cx + tx / L * 76 + 10, cy - ty / L * 76 - 6, "T", "sm", anchor="start"))
        ang = math.degrees(math.atan2(ty, tx))
        b.append(label(cx, cy + 26, f"T at {ang:.0f} deg from +u", "xs"))
        b.append(label(cx, cy + 44, "N unchanged", "xs"))

    # The verdict.
    b.append(box(24, 296, W - 48, 92, None))
    b.append(label(W / 2, 320,
                   "The normal did not move; the tangent did. So they are carried by "
                   "different matrices:", "sm"))
    rows = [("a NORMAL", "inverse transpose", "perpendicularity is what it must keep (3.6)", GREEN),
            ("a TANGENT", "the model matrix", "it is a difference of POSITIONS (6.7)", BLUE)]
    y = 344
    for what, which, why, col in rows:
        b.append(label(170, y, what, "xs", anchor="end"))
        b.append(box(180, y - 11, 132, 16, col, opacity=0.30))
        b.append(label(246, y, which, "xs"))
        b.append(label(326, y, why, "xs", anchor="start"))
        y += 22

    b.append(label(W / 2, H - 6,
                   f"Use the wrong one and the frame is SKEWED by {SKEW_DEG:.1f} degrees "
                   f"— and only under a non-uniform scale.", "xs"))

    return svg(uid, W, H,
               "A surface patch before and after a non-uniform scale",
               "Two panels each showing a horizontal surface with an upward "
               "normal N and a tangent T at an angle. After a scale of two in x "
               "the normal is unchanged and the tangent has rotated toward the "
               "stretched axis — which is why a normal needs the inverse "
               "transpose and a tangent needs the plain model matrix.", b)


# ===========================================================================
# Figure 6 — what you should see  (§10)
# ===========================================================================
def fig_result():
    uid = "l67f6"
    W, H = 720, 356
    b = [box(0, 0, W, H)]
    b.append(label(W / 2, 24, "gltf_view --model torus.obj", "lg"))

    fw, fh = 320, 200
    for i, (title, bumpy) in enumerate([("--bumps 0", False), ("default (6 cells)", True)]):
        fx = 24 + i * (fw + 32)
        fy = 44
        b.append(box(fx, fy, fw, fh, "#0c0e14", opacity=1.0))
        b.append(hollow(fx, fy, fw, fh, GREY))
        b.append(label(fx + fw / 2, fy + fh + 20, title, "sm"))

        cx, cy = fx + fw / 2, fy + fh / 2
        r_out, r_in, squash = 96, 38, 0.44

        b.append(f'<ellipse cx="{cx}" cy="{cy}" rx="{r_out}" ry="{r_out * squash:.1f}" '
                 f'fill="{AMBER}" fill-opacity="0.88" stroke="{AMBER}" '
                 f'stroke-opacity="0.9" stroke-width="1.2"/>')

        if bumpy:
            # Dimples on the BAND only, so none of them lands in the hole.
            for j in range(20):
                a = j * 2.0 * math.pi / 20.0
                for ring in (0.58, 0.84):
                    rr = r_in + (r_out - r_in) * ring
                    px = cx + math.cos(a) * rr
                    py = cy + math.sin(a) * rr * squash
                    b.append(f'<ellipse cx="{px:.1f}" cy="{py:.1f}" rx="7.5" ry="3.6" '
                             f'fill="#1a1206" fill-opacity="0.5" stroke="none"/>')
        else:
            # One broad highlight along the upper-left of the band.
            b.append(f'<ellipse cx="{cx - 44:.1f}" cy="{cy - 22:.1f}" rx="30" ry="11" '
                     f'fill="#fff3d0" fill-opacity="0.45" stroke="none" '
                     f'transform="rotate(-24 {cx - 44:.1f} {cy - 22:.1f})"/>')

        # THE HOLE, punched in the frame's own background colour — an outlined
        # inner ellipse reads as a ring painted on a plate, which is the wrong
        # shape for a figure whose subject is the silhouette staying smooth.
        b.append(f'<ellipse cx="{cx}" cy="{cy + 4}" rx="{r_in}" ry="{r_in * squash:.1f}" '
                 f'fill="#0c0e14" fill-opacity="1" stroke="#0c0e14" stroke-width="1"/>')
        b.append(f'<ellipse cx="{cx}" cy="{cy + 4}" rx="{r_in}" ry="{r_in * squash:.1f}" '
                 f'fill="none" stroke="{AMBER}" stroke-opacity="0.55" stroke-width="1.1"/>')

    b.append(box(24, 292, W - 48, 56, None))
    b.append(label(W / 2, 314,
                   "Same 2,304 triangles, same light, same material. Only the normal changed.",
                   "sm"))
    b.append(label(W / 2, 336,
                   "The tangents were DERIVED — OBJ has no tangent attribute at all.", "xs"))

    return svg(uid, W, H,
               "The torus rendered flat and normal-mapped, side by side",
               "Two dark framebuffers each containing an amber torus seen at a "
               "shallow angle. The left is smooth with a single broad highlight; "
               "the right carries a grid of dimples around the ring. The geometry "
               "is identical in both — 2,304 triangles — and only the shading "
               "normal differs.", b)


# ===========================================================================
def main():
    figures = [
        ("l67_fig1.svg", fig_problem()),
        ("l67_fig2.svg", fig_space()),
        ("l67_fig3.svg", fig_derivation()),
        ("l67_fig4.svg", fig_basis()),
        ("l67_fig5.svg", fig_matrix()),
        ("l67_fig6.svg", fig_result()),
    ]
    for name, body in figures:
        path = os.path.join(OUT, name)
        with open(path, "w") as fh:
            fh.write(body)
        print(f"wrote {path}  ({len(body)} bytes)")


if __name__ == "__main__":
    main()
