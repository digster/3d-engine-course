#!/usr/bin/env python3
"""scratch/figs_62.py — Lesson 6.2's diagrams.

Same rules as 5.10's, 5.11's and 6.1's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - every element well inside the viewBox, and in-bar labels checked against the bar
  - ~5.2 units per character for `xs`, ~6.0 for `sm` (calibrated on shipped figures)
  - filenames numbered by PAGE ORDER

Every number in here is either derived from the geometry in this file or copied
from verify_62's output, which prints each table for exactly this purpose.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)

OUT = "scratch"

# ---- Measured inputs, from verify_62 ----------------------------------------

# §A: the two-patch experiment. distance, solid angle, irradiance, recovered L.
INVARIANCE = [
    (1.0, 1.000e-4, 7.500e-4, 7.5),
    (2.0, 2.500e-5, 1.875e-4, 7.5),
    (4.0, 6.250e-6, 4.688e-5, 7.5),
]

# §D, first table: the RAW cos^s lobe with no 1/pi at all.
RAW_LOBE = [(1, 2.6650), (2, 2.3038), (4, 1.8012), (8, 1.2451),
            (12, 0.9500), (16, 0.7681), (32, 0.4354)]

# §D, second table: on the engine's 1/pi scale, white albedo + white highlight.
BUDGET = [(2, 1.0, 0.7333), (4, 1.0, 0.5733), (8, 1.0, 0.3963),
          (32, 1.0, 0.1386), (128, 1.0, 0.0385)]

CHECKS = 32


# ===========================================================================
# Figure 1 — three quantities, and the one a pixel holds  (§2)
# ===========================================================================
def fig_quantities():
    uid = "l62f1"
    W, H = 720, 430
    b = [label(20, 20, "Three quantities, each the one before it divided by something. "
                       "Only the third can be stored in a pixel.", "sm", "start")]

    PANEL_W, GAP = 218, 18
    X = [24 + i * (PANEL_W + GAP) for i in range(3)]
    TOP, PH = 42, 230
    # The vertical budget, fixed once so nothing can drift into anything else:
    ART_Y = TOP + 32          # top of the drawing region
    ART_B = TOP + 142         # bottom of it — the surface line sits here
    PROSE = TOP + 164         # two lines of prose
    SPLIT = TOP + 188         # a rule, then the formula band
    SYM = TOP + 206
    UNIT = TOP + 220

    for i, (title, sym, unit) in enumerate((
            ("1. FLUX", "Φ", "watts"),
            ("2. IRRADIANCE", "E = dΦ / dA", "W / m²"),
            ("3. RADIANCE", "L = d²Φ / (dA⊥ dω)", "W / (m² sr)"))):
        b.append(hollow(X[i], TOP, PANEL_W, PH, GREY, dash="4 4"))
        b.append(label(X[i] + PANEL_W / 2, TOP + 18, title, "xs t-hi"))
        b.append(rule(X[i] + 16, SPLIT, X[i] + PANEL_W - 16, SPLIT, "grid", 1.0))
        b.append(label(X[i] + PANEL_W / 2, SYM, sym, "sm mono"))
        b.append(label(X[i] + PANEL_W / 2, UNIT, unit, "xs muted"))

    # --- panel 1: a lamp throwing light in EVERY direction -------------------
    cx, cy = X[0] + PANEL_W / 2, (ART_Y + ART_B) / 2
    b.append(f'<circle cx="{cx}" cy="{cy}" r="10" fill="{AMBER}"'
             f' stroke="{AMBER}" stroke-width="1.2"/>')
    for k in range(12):
        a = 2 * math.pi * k / 12.0
        b.append(arrow(cx + 13 * math.cos(a), cy + 13 * math.sin(a),
                       cx + 48 * math.cos(a), cy + 48 * math.sin(a), uid, "s", 1.0))
    b.append(label(cx, PROSE, "everything the lamp emits, in every", "xs muted"))
    b.append(label(cx, PROSE + 13, "direction, added up. One number.", "xs muted"))

    # --- panel 2: all of it landing on one patch -----------------------------
    cx = X[1] + PANEL_W / 2
    pw = 108
    b.append(rule(cx - pw / 2, ART_B, cx + pw / 2, ART_B, "ink", 2.0))
    b.append(label(cx + pw / 2 + 8, ART_B + 4, "dA", "xs mono muted", "start"))
    # Arrivals from several directions at once — that is the whole point of
    # irradiance. Tips and sources are both ordered left to right so no two
    # arrows cross, which would read as one beam bouncing rather than four
    # arriving.
    for ang, off in ((-118, -42), (-100, -18), (-80, 6), (-62, 30)):
        a = math.radians(ang)          # negative angles point UP the screen
        tipx, tipy = cx + off, ART_B - 4
        b.append(arrow(tipx + 62 * math.cos(a), tipy + 62 * math.sin(a),
                       tipx, tipy, uid, "s", 1.0))
    b.append(label(cx, PROSE, "everything ARRIVING per square metre", "xs muted"))
    b.append(label(cx, PROSE + 13, "— from every direction at once", "xs muted"))

    # --- panel 3: one direction only, through one small cone -----------------
    cx = X[2] + PANEL_W / 2
    b.append(rule(cx - pw / 2, ART_B, cx + pw / 2, ART_B, "ink", 2.0))
    b.append(label(cx - pw / 2 - 8, ART_B + 4, "dA⊥", "xs mono muted", "end"))
    apex = (cx, ART_B)
    half, axis, reach = math.radians(11), math.radians(-66), 92
    p1 = (apex[0] + reach * math.cos(axis - half), apex[1] + reach * math.sin(axis - half))
    p2 = (apex[0] + reach * math.cos(axis + half), apex[1] + reach * math.sin(axis + half))
    b.append(f'<path d="M {apex[0]:.1f} {apex[1]:.1f} L {p1[0]:.1f} {p1[1]:.1f} '
             f'L {p2[0]:.1f} {p2[1]:.1f} Z" fill="{BLUE}" fill-opacity="0.22" '
             f'stroke="{BLUE}" stroke-width="1.3"/>')
    b.append(label(cx + 46, ART_Y + 6, "dω", "xs mono t-hi"))
    b.append(label(cx, PROSE, "what arrives from ONE direction,", "xs muted"))
    b.append(label(cx, PROSE + 13, "through one steradian of sky", "xs muted"))

    # --- the punchline -------------------------------------------------------
    by = TOP + PH + 24
    b.append(box(24, by, W - 48, 58, None, rx=4))
    b.append(label(W / 2, by + 22,
                   "A PIXEL IS A TINY CONE FROM THE EYE, so a pixel measures RADIANCE.",
                   "sm t-hi"))
    b.append(label(W / 2, by + 40,
                   "That is why one number per pixel is enough: radiance does not change "
                   "along a ray, so the renderer need not ask how far away anything is.",
                   "xs muted"))

    return svg(uid, W, H, "Flux, irradiance and radiance",
               "Three panels. A lamp emitting flux in all directions; the same light "
               "landing on a patch as irradiance, watts per square metre; and radiance, "
               "the part arriving from one direction through one steradian. A pixel "
               "measures radiance.", b)


# ===========================================================================
# Figure 2 — radiance is invariant, irradiance is not  (§2.4)
# ===========================================================================
def fig_invariance():
    uid = "l62f2"
    W, H = 720, 400
    b = [label(20, 20, "Move the receiver twice as far away. Two of these three numbers "
                       "fall by four; the third does not move.", "sm", "start")]

    EMIT_X, EMIT_H = 58, 17
    ROW_Y = (128, 224)
    COL = (470, 570, 672)          # irradiance, solid angle, radiance
    HEAD_Y = 86

    for j, txt in enumerate(("irradiance E", "solid angle ω", "L = E / ω")):
        b.append(label(COL[j], HEAD_Y, txt, "xs muted", "end"))
        b.append(rule(COL[j] - 76, HEAD_Y + 7, COL[j], HEAD_Y + 7, "grid", 1.0))

    for i, (d, omega, e, lrec) in enumerate(INVARIANCE[:2]):
        y = ROW_Y[i]
        rx = EMIT_X + 168 * d       # the receiver, placed to scale

        b.append(box(EMIT_X - 5, y - EMIT_H, 10, 2 * EMIT_H, AMBER, rx=2))
        b.append(rule(rx, y - 22, rx, y + 22, "ink", 2.4))
        b.append(label(rx, y + 38, f"receiver, d = {d:.0f}", "xs muted"))

        # The cone the emitter subtends AT THE RECEIVER: apex on the receiver,
        # base on the emitter. Its opening angle is what halves when d doubles.
        b.append(f'<path d="M {rx} {y} L {EMIT_X + 6} {y - EMIT_H} '
                 f'L {EMIT_X + 6} {y + EMIT_H} Z" fill="{BLUE}" fill-opacity="0.16" '
                 f'stroke="{BLUE}" stroke-width="1.1"/>')

        # A CLASS, NEVER fill="...". `figure.dia svg text { fill: ... }` in
        # course.css is CSS and always beats an SVG presentation attribute, so
        # an inline fill on a <text> is silently ignored — the label renders in
        # the default ink and nothing anywhere errors. This figure shipped that
        # way for four labels until apply-shared.py's lint caught it.
        #
        # The right classes are the SEMANTIC ones, because that is what the
        # colour means here: these two quantities fell and this one did not.
        # The verdict row fifteen lines below already says exactly that with
        # `t-bad` and `t-ok`, so the values now match the verdict under them.
        vals = ((f"{e:.3e}", "t-bad" if i else None),
                (f"{omega:.3e}", "t-bad" if i else None),
                (f"{lrec:.4f}", "t-ok"))
        for j, (val, tone) in enumerate(vals):
            cls = f"xs mono {tone}" if tone else "xs mono muted"
            b.append(f'<text x="{COL[j]}" y="{y + 4}" class="{cls}" '
                     f'text-anchor="end">{esc(val)}</text>')

    b.append(label(EMIT_X, ROW_Y[0] - 34, "emitter", "xs muted"))

    # The comparison, exactly between the two rows.
    mid = (ROW_Y[0] + ROW_Y[1]) / 2 + 4
    for j, (txt, cls) in enumerate((("÷ 4", "xs t-bad"), ("÷ 4", "xs t-bad"),
                                    ("unchanged", "xs t-ok"))):
        b.append(label(COL[j] - 34, mid, txt, cls))
        b.append(rule(COL[j] - 34, ROW_Y[0] + 14, COL[j] - 34, mid - 11, "grid", 1.0,
                      dash="2 3"))
        b.append(rule(COL[j] - 34, mid + 5, COL[j] - 34, ROW_Y[1] - 10, "grid", 1.0,
                      dash="2 3"))

    by = 302
    b.append(box(24, by, W - 48, 76, None, rx=4))
    b.append(label(W / 2, by + 24,
                   "Both fall as 1/d², so their RATIO is constant — that is the whole "
                   "of radiance invariance.", "sm t-hi"))
    b.append(label(W / 2, by + 46,
                   "Store irradiance and every shading equation needs to know how far "
                   "away the light is. Store radiance and the ray carries its own",
                   "xs muted"))
    b.append(label(W / 2, by + 61,
                   "answer — which is why a pixel can be one number.", "xs muted"))

    return svg(uid, W, H, "Radiance is invariant along a ray",
               "An emitter and a receiver at two distances. Doubling the distance divides "
               "both the irradiance and the subtended solid angle by four, so radiance, "
               "their ratio, is unchanged at 7.5.", b)


# ===========================================================================
# Figure 3 — the cosine is a statement about area  (§3)
# ===========================================================================
def fig_projected_area():
    uid = "l62f3"
    W, H = 720, 450
    b = [label(20, 20, "The cosine is not a fudge for “less light”. The same beam lands "
                       "on a LARGER patch, so each square metre gets less.",
                       "sm", "start")]

    BEAM_W = 44.0
    TOP, BASE = 74, 208
    for i, deg in enumerate((0, 45, 60)):
        cx = 128 + i * 232
        th = math.radians(deg)
        foot = BEAM_W / math.cos(th)

        # The surface, tilted about its centre. Its endpoints are at
        # (cx ± foot/2·cos θ), which is exactly (cx ± BEAM_W/2) — the beam and the
        # patch it lands on cover the same horizontal span, by construction. That
        # is precisely why the stretch is invisible from directly above, and why
        # the unrolled bar below is needed to show it.
        dx, dy = foot / 2 * math.cos(th), foot / 2 * math.sin(th)

        # The beam is a POLYGON, not a rectangle: its bottom edge is the surface
        # it lands on. Drawn as a rectangle it stops short at one corner and
        # hangs past the other, which reads as a drawing mistake rather than as
        # a tilt.
        b.append(f'<path d="M {cx - BEAM_W / 2:.1f} {TOP} L {cx + BEAM_W / 2:.1f} {TOP} '
                 f'L {cx + dx:.1f} {BASE - dy:.1f} L {cx - dx:.1f} {BASE + dy:.1f} Z" '
                 f'fill="{AMBER}" fill-opacity="0.16" stroke="none"/>')
        # TWO arrows, not three: at theta = 0 the normal points straight back up
        # the beam, so a centre arrow and the "n" label want the same pixels.
        # check-page.js reported it as a label-on-shape, which is exactly the class
        # of defect the eye slides over because the drawing still looks fine.
        for k in (-1, 1):
            u = k * (BEAM_W / 2 - 7)
            b.append(arrow(cx + u, TOP + 4, cx + u, BASE - u * math.tan(th) - 5,
                           uid, "s", 1.0))

        # The surface runs on past the lit patch, so it reads as a surface; the
        # lit part is the heavy segment.
        ex, ey = (dx + 14 * math.cos(th)), (dy + 14 * math.sin(th))
        b.append(rule(cx - ex, BASE + ey, cx + ex, BASE - ey, "grid", 1.4))
        b.append(rule(cx - dx, BASE + dy, cx + dx, BASE - dy, "ink", 2.6))

        # The normal, and the angle it makes with the beam.
        nx, ny = math.sin(th), -math.cos(th)
        b.append(arrow(cx, BASE, cx + 52 * nx, BASE + 52 * ny, uid, "i", 1.4))
        b.append(label(cx + 60 * nx, BASE + 60 * ny - 2, "n", "xs mono t-hi"))
        b.append(label(cx, 54, f"θ = {deg}°", "sm t-hi"))

        # ---- unrolled: the same power, on two bars, to scale ----------------
        # Drawn at 1.6x so the difference is visible at reading size; both bars
        # share the scale, so the RATIO is the true one.
        uy, k_bar = 286, 1.6
        b.append(label(cx, uy - 12, "the beam", "xs muted"))
        b.append(box(cx - k_bar * BEAM_W / 2, uy, k_bar * BEAM_W, 13, AMBER, rx=2,
                     opacity=0.55))
        b.append(label(cx, uy + 40, "the patch it lands on", "xs muted"))
        b.append(box(cx - k_bar * foot / 2, uy + 46, k_bar * foot, 13, GREY, rx=2,
                     opacity=0.40))
        b.append(label(cx, uy + 80, f"{foot / BEAM_W:.4f}× the area", "xs mono"))
        b.append(label(cx, uy + 96, f"so cos θ = {math.cos(th):.4f}", "xs mono t-ok"))

    by = 392
    b.append(box(24, by, W - 48, 50, None, rx=4))
    b.append(label(W / 2, by + 20,
                   "1 / (footprint area) = dot(n, l) at every angle, to 1e−6 — verify_62 "
                   "§B computes the two sides independently.", "sm t-hi"))
    b.append(label(W / 2, by + 38,
                   "Which is why the cosine belongs on the LIGHT’s side of the shading "
                   "equation: it says how much arrives, not what the surface does next.",
                   "xs muted"))

    return svg(uid, W, H, "The cosine factor as projected area",
               "One beam of fixed width striking a surface at 0, 45 and 60 degrees. Below "
               "each, two bars to scale: the beam's own cross-section and the patch it "
               "lands on, which is 1 over cosine theta times larger.", b)


# ===========================================================================
# Figure 4 — a BRDF is a ratio, and its unit is inverse steradians  (§4.1)
# ===========================================================================
def fig_brdf():
    uid = "l62f4"
    W, H = 720, 430
    b = [label(20, 20, "A BRDF answers one question: of the light arriving from l, how "
                       "much leaves toward v — per steradian?", "sm", "start")]

    BASE = 250
    A_IN = math.radians(220)                     # toward the light, up and left
    A_MIRROR = math.radians(-40)                 # its mirror about the normal
    CONE = math.radians(20)                      # the glossy surface's half-angle

    # BOTH NUMBERS ARE EXACT, and that is the point of choosing these two shapes.
    # A surface that reflects everything uniformly into a cone of half-angle a
    # spreads it over a PROJECTED solid angle of pi*sin^2(a), so its BRDF is the
    # reciprocal. Lambert is the same formula at a = 90 degrees.
    f_lambert = 1.0 / math.pi
    f_glossy = 1.0 / (math.pi * math.sin(CONE) ** 2)

    for i, (name, note) in enumerate((
            ("LAMBERT — scatters into the whole hemisphere", "half-angle 90°"),
            ("GLOSSY — concentrates into a narrow cone", "half-angle 20°"))):
        cx = 190 + i * 340
        b.append(label(cx, 52, name, "sm t-hi"))
        b.append(label(cx, 68, note, "xs muted"))

        # ---- the lobe, drawn FIRST so the vectors sit on top of it ----------
        if i == 0:
            R = 66.0
            b.append(f'<path d="M {cx - R} {BASE} A {R} {R} 0 0 1 {cx + R} {BASE} Z" '
                     f'fill="{BLUE}" fill-opacity="0.20" stroke="{BLUE}" '
                     f'stroke-width="1.4"/>')
            a_out = math.radians(-58)
        else:
            R = 116.0
            p0 = (cx + R * math.cos(A_MIRROR - CONE), BASE + R * math.sin(A_MIRROR - CONE))
            p1 = (cx + R * math.cos(A_MIRROR + CONE), BASE + R * math.sin(A_MIRROR + CONE))
            b.append(f'<path d="M {cx} {BASE} L {p0[0]:.1f} {p0[1]:.1f} '
                     f'A {R} {R} 0 0 1 {p1[0]:.1f} {p1[1]:.1f} Z" '
                     f'fill="{PURPLE}" fill-opacity="0.22" stroke="{PURPLE}" '
                     f'stroke-width="1.4"/>')
            a_out = A_MIRROR

        b.append(rule(cx - 148, BASE, cx + 148, BASE, "ink", 2.0))

        # ---- the three directions -------------------------------------------
        lx, ly = cx + 128 * math.cos(A_IN), BASE + 128 * math.sin(A_IN)
        b.append(arrow(lx, ly, cx + 6 * math.cos(A_IN), BASE + 6 * math.sin(A_IN),
                       uid, "h", 1.8))
        b.append(label(lx - 12, ly - 4, "l", "sm mono t-hi"))

        b.append(arrow(cx, BASE, cx, BASE - 104, uid, "i", 1.2))
        b.append(label(cx + 9, BASE - 106, "n", "xs mono muted", "start"))

        b.append(arrow(cx + 8 * math.cos(a_out), BASE + 8 * math.sin(a_out),
                       cx + 140 * math.cos(a_out), BASE + 140 * math.sin(a_out),
                       uid, "s", 1.4, dash="5 3"))
        b.append(label(cx + 152 * math.cos(a_out), BASE + 152 * math.sin(a_out) + 4,
                       "v", "sm mono"))

        # ---- the number ------------------------------------------------------
        val = f_lambert if i == 0 else f_glossy
        expr = "fᵣ = 1/π = 0.3183 sr⁻¹" if i == 0 \
            else "fᵣ = 1/(π sin²20°) = 2.7210 sr⁻¹"
        b.append(label(cx, BASE + 44, expr, "xs mono " + ("t-ok" if i == 0 else "t-bad")))
        assert abs(val - (0.3183 if i == 0 else 2.7210)) < 5e-4
        b.append(label(cx, BASE + 60, "reflects 100% of what arrives", "xs muted"))

    by = 336
    b.append(box(24, by, W - 48, 78, None, rx=4))
    b.append(label(W / 2, by + 22,
                   "BOTH SURFACES REFLECT EVERYTHING. The right one is not brighter "
                   "overall — it is brighter in one direction and darker in the rest.",
                   "sm t-hi"))
    b.append(label(W / 2, by + 44,
                   "A BRDF is what leaves PER STERADIAN, so its value is one over the "
                   "solid angle the light is spread across — and that is large exactly",
                   "xs muted"))
    b.append(label(W / 2, by + 60,
                   "when the spread is small. A mirror’s BRDF is unbounded, and still "
                   "reflects at most what arrived. Exceeding 1 costs nothing.",
                   "xs muted"))

    return svg(uid, W, H, "A BRDF is a ratio with units of inverse steradian",
               "Two surfaces: one scattering into the whole hemisphere with a BRDF of "
               "one over pi, one concentrating into a 20 degree cone with a BRDF of "
               "2.72 per steradian. Both reflect all the arriving light.", b)


# ===========================================================================
# Figure 5 — where the pi comes from  (§4.2)
# ===========================================================================
def fig_the_pi():
    uid = "l62f5"
    W, H = 720, 440
    b = [label(20, 20, "The pi is not a convention. It is the area of a disc, and here "
                       "is the disc.", "sm", "start")]

    # The theta bands, shared by both panels so the highlighted cell on the right
    # really is the projection of the highlighted wedge on the left.
    BANDS = [0, 18, 36, 54, 72, 90]
    SLICES = 12
    HI_BAND = 2                       # the 36..54 degree band
    HI_SLICE = 1

    # ---- left: the hemisphere in cross-section, one patch and its shadow -----
    cx, cy, R = 196, 250, 130
    b.append(rule(cx - R - 26, cy, cx + R + 26, cy, "ink", 1.8))
    b.append(f'<path d="M {cx - R} {cy} A {R} {R} 0 0 1 {cx + R} {cy}" fill="none" '
             f'class="grid" stroke-width="1.4" stroke-dasharray="5 4"/>')

    t0 = math.radians(BANDS[HI_BAND])
    t1 = math.radians(BANDS[HI_BAND + 1])
    q0 = (cx + R * math.sin(t0), cy - R * math.cos(t0))
    q1 = (cx + R * math.sin(t1), cy - R * math.cos(t1))
    b.append(f'<path d="M {cx} {cy} L {q0[0]:.1f} {q0[1]:.1f} A {R} {R} 0 0 1 '
             f'{q1[0]:.1f} {q1[1]:.1f} Z" fill="{PURPLE}" fill-opacity="0.32" '
             f'stroke="{PURPLE}" stroke-width="1.3"/>')
    tm = (t0 + t1) / 2
    b.append(label(cx + (R + 22) * math.sin(tm), cy - (R + 22) * math.cos(tm) + 4,
                   "dω", "xs mono t-hi", "start"))

    b.append(arrow(cx, cy, cx, cy - R - 18, uid, "i", 1.2))
    b.append(label(cx + 10, cy - R - 20, "n", "xs mono muted", "start"))
    b.append(f'<path d="M {cx} {cy - 42} A 42 42 0 0 1 {cx + 42 * math.sin(tm):.1f} '
             f'{cy - 42 * math.cos(tm):.1f}" fill="none" class="ink-soft" '
             f'stroke-width="1.1"/>')
    b.append(label(cx + 24, cy - 50, "θ", "xs mono"))

    # The shadow: drop each end of the arc straight down onto the base plane.
    for q in (q0, q1):
        b.append(rule(q[0], q[1], q[0], cy, "grid", 1.0, dash="3 3"))
    b.append(rule(q0[0], cy + 5, q1[0], cy + 5, "hi", 3.4))
    b.append(label((q0[0] + q1[0]) / 2 + 6, cy + 24, "cos θ · dω", "xs mono t-hi",
                   "start"))
    b.append(label(cx, cy + 52, "every patch of the hemisphere casts a shadow", "xs muted"))
    b.append(label(cx, cy + 66, "of its own size times cos θ", "xs muted"))

    # ---- right: the shadows ARE a polar grid, and it tiles the disc ----------
    dx, dy, DR = 536, 202, 98
    b.append(f'<circle cx="{dx}" cy="{dy}" r="{DR}" fill="{GREEN}" fill-opacity="0.12" '
             f'stroke="{GREEN}" stroke-width="1.6"/>')

    # The cell that is the highlighted wedge's own shadow, drawn first.
    r0, r1 = DR * math.sin(t0), DR * math.sin(t1)
    a0 = 2 * math.pi * HI_SLICE / SLICES
    a1 = 2 * math.pi * (HI_SLICE + 1) / SLICES
    def pt(r, a):
        return f"{dx + r * math.cos(a):.1f} {dy + r * math.sin(a):.1f}"
    b.append(f'<path d="M {pt(r0, a0)} A {r0} {r0} 0 0 1 {pt(r0, a1)} '
             f'L {pt(r1, a1)} A {r1} {r1} 0 0 0 {pt(r1, a0)} Z" fill="{PURPLE}" '
             f'fill-opacity="0.45" stroke="{PURPLE}" stroke-width="1.2"/>')

    # …and the rest of the grid it belongs to. Radii are sin(theta), because that
    # is exactly where a patch at polar angle theta lands when it is projected.
    for deg in BANDS[1:-1]:
        rr = DR * math.sin(math.radians(deg))
        b.append(f'<circle cx="{dx}" cy="{dy}" r="{rr:.1f}" fill="none" class="grid" '
                 f'stroke-width="1.0"/>')
    for k in range(SLICES):
        a = 2 * math.pi * k / SLICES
        b.append(rule(dx, dy, dx + DR * math.cos(a), dy + DR * math.sin(a), "grid", 1.0))

    b.append(label(dx, dy - DR - 16, "the shadows tile this disc, exactly once", "xs t-hi"))
    b.append(rule(dx - DR, dy + DR + 20, dx + DR, dy + DR + 20, "grid", 1.2))
    b.append(rule(dx - DR, dy + DR + 15, dx - DR, dy + DR + 25, "grid", 1.2))
    b.append(rule(dx + DR, dy + DR + 15, dx + DR, dy + DR + 25, "grid", 1.2))
    b.append(label(dx, dy + DR + 38, "radius 1", "xs mono muted"))

    by = 352
    b.append(box(24, by, W - 48, 76, None, rx=4))
    b.append(label(W / 2, by + 24,
                   "∫ cos θ dω  over the hemisphere  =  area of a unit disc  =  π",
                   "sm mono t-hi"))
    b.append(label(W / 2, by + 46,
                   "So a constant BRDF k sends back k·π of everything that arrives. "
                   "Demanding that this equal the albedo — no more, no less — fixes",
                   "xs muted"))
    b.append(label(W / 2, by + 62,
                   "k = albedo/π, and nothing else will do. Measured at 1024×2048 "
                   "samples: 1.0000004.", "xs muted"))

    return svg(uid, W, H, "Deriving the pi in the Lambert BRDF",
               "A hemisphere in cross-section: each patch of solid angle projects onto "
               "the base plane with area cosine theta times its own. The projections are "
               "a polar grid at radius sine theta, and they tile a unit disc exactly "
               "once — so the cosine-weighted hemisphere measures pi.", b)


# ===========================================================================
# Figure 6 — energy conservation, measured  (§6)
# ===========================================================================
def fig_energy():
    uid = "l62f6"
    W, H = 720, 500
    b = [label(20, 20, "Energy conservation is a number you can compute, and this engine "
                       "fails it in two different places.", "sm", "start")]

    X0, ROW = 118, 21
    # Value labels go in a FIXED column, not at the end of each bar: a bar just
    # short of the 1.0 rule puts its label straight on top of the rule, which is
    # the one place the reader is looking. The column is COMPUTED from the longest
    # bar plus a gap plus the label's own width at ~5.2 units per character —
    # hand-placing it put "2.6650" on the end of its own bar.
    LBL_W = 6 * 5.2

    # ---- top: the raw lobe, no 1/pi at all ---------------------------------
    TOP, UNIT = 62, 132.0
    VAL_A = X0 + max(r for _, r in RAW_LOBE) * UNIT + 10 + LBL_W
    b.append(label(20, TOP - 12, "The cosˢ lobe with NO constant at all:", "xs muted",
                   "start"))
    one_x = X0 + UNIT
    b.append(rule(one_x, TOP - 4, one_x, TOP + len(RAW_LOBE) * ROW, "hi", 1.6, dash="4 3"))
    b.append(label(one_x + 4, TOP - 12, "1.0 = all the light there is", "xs t-hi", "start"))
    for i, (s_, r) in enumerate(RAW_LOBE):
        y = TOP + 4 + i * ROW
        over = r > 1.0
        b.append(box(X0, y, r * UNIT, ROW - 8, RED if over else GREY, rx=2, opacity=0.55))
        b.append(label(X0 - 8, y + 11, f"shininess {s_}", "xs mono muted", "end"))
        b.append(label(VAL_A, y + 11, f"{r:.4f}",
                       "xs mono " + ("t-bad" if over else "muted"), "end"))
    note_y = TOP + 4 + len(RAW_LOBE) * ROW + 16
    b.append(label(X0, note_y, "a “rough plastic” brighter than the lamp that lit it",
                   "xs t-bad", "start"))

    # ---- bottom: on the 1/pi scale, diffuse + specular ---------------------
    TOP2, UNIT2 = note_y + 40, 210.0
    VAL_B = X0 + max(rd + rs for _, rd, rs in BUDGET) * UNIT2 + 10 + LBL_W
    b.append(label(20, TOP2 - 12,
                   "On the engine’s 1/π scale — white albedo PLUS a white highlight:",
                   "xs muted", "start"))
    one_x2 = X0 + UNIT2
    b.append(rule(one_x2, TOP2 - 4, one_x2, TOP2 + len(BUDGET) * ROW, "hi", 1.6,
                  dash="4 3"))
    for i, (s_, rd, rs) in enumerate(BUDGET):
        y = TOP2 + 4 + i * ROW
        b.append(box(X0, y, rd * UNIT2, ROW - 8, BLUE, rx=2, opacity=0.50))
        b.append(box(X0 + rd * UNIT2, y, rs * UNIT2, ROW - 8, PURPLE, rx=2, opacity=0.65))
        b.append(label(X0 - 8, y + 11, f"shininess {s_}", "xs mono muted", "end"))
        b.append(label(VAL_B, y + 11, f"{rd + rs:.4f}", "xs mono t-bad", "end"))

    leg_y = TOP2 + 4 + len(BUDGET) * ROW + 12
    for lx, col, op, txt in ((X0, BLUE, 0.50, "diffuse"),
                             (X0 + 108, PURPLE, 0.65, "specular")):
        b.append(f'<rect x="{lx}" y="{leg_y}" width="28" height="9" rx="2" '
                 f'fill="{col}" fill-opacity="{op}" stroke="{col}" stroke-width="1"/>')
        b.append(label(lx + 34, leg_y + 8, txt, "xs", "start"))

    by = leg_y + 28
    b.append(box(24, by, W - 48, 62, None, rx=4))
    b.append(label(W / 2, by + 22,
                   "The 1/π fixes the runaway and leaves the real defect standing: the "
                   "two lobes are ADDED, with no coupling.", "sm t-hi"))
    b.append(label(W / 2, by + 42,
                   "Lesson 6.4 fixes it with a mechanism rather than a constant — Fresnel "
                   "decides what fraction bounces, and only the rest is left to scatter.",
                   "xs muted"))
    assert by + 62 <= H - 8, f"the callout runs past the viewBox: {by + 62} > {H - 8}"
    assert max(VAL_A, VAL_B) <= W - 24, "the value column runs past the right margin"

    return svg(uid, W, H, "Energy conservation, measured",
               "Two bar charts. The raw Blinn-Phong lobe exceeds unit reflectance below "
               "shininess 12. On the engine's one-over-pi scale a white diffuse plus a "
               "white specular exceeds 1 at every shininess, from 1.0385 to 1.7333.", b)


# ===========================================================================
def main():
    figs = [("l62_fig1.svg", fig_quantities),
            ("l62_fig2.svg", fig_invariance),
            ("l62_fig3.svg", fig_projected_area),
            ("l62_fig4.svg", fig_brdf),
            ("l62_fig5.svg", fig_the_pi),
            ("l62_fig6.svg", fig_energy)]
    for name, fn in figs:
        text = fn()
        with open(os.path.join(OUT, name), "w") as fh:
            fh.write(text)
        print(f"  wrote {OUT}/{name}  ({len(text):,} bytes)")


if __name__ == "__main__":
    main()
