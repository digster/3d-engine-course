#!/usr/bin/env python3
"""scratch/figs_64.py — Lesson 6.4's diagrams.

Same rules as 6.1's, 6.2's and 6.3's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - every element well inside the viewBox, and in-bar labels checked against the bar
  - ~5.2 units per character for `xs`, ~6.0 for `sm`
  - filenames numbered by PAGE ORDER

Every number is either computed from the formulas microfacet.hpp implements or
copied from verify_64's output.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)

OUT = "scratch"


# ---- the model, so every plotted point is the real one ----------------------
def d_ggx(c, a):
    if c <= 0.0:
        return 0.0
    a2 = a * a
    d = (1.0 - c) * (1.0 + c) + a2 * c * c
    return a2 / (math.pi * d * d)


def smith_g(nl, nv, a):
    if nl <= 0.0 or nv <= 0.0:
        return 0.0
    a2 = a * a
    lv = nl * math.sqrt(a2 + (1.0 - a2) * nv * nv)
    ll = nv * math.sqrt(a2 + (1.0 - a2) * nl * nl)
    return 2.0 * nl * nv / (lv + ll)


def schlick(c, f0):
    m = 1.0 - max(0.0, min(1.0, c))
    return f0 + (1.0 - f0) * (m ** 5)


def fresnel_exact(c, ior):
    s2 = (1.0 - c * c) / (ior * ior)
    if s2 >= 1.0:
        return 1.0
    ct = math.sqrt(1.0 - s2)
    rs = (c - ior * ct) / (c + ior * ct)
    rp = (ior * c - ct) / (ior * c + ct)
    return 0.5 * (rs * rs + rp * rp)


# ---- measured inputs, from verify_64 ----------------------------------------

# §E: roughness -> (uncoupled, coupled) at theta_v = 0 and 75
ENERGY_0 = [(0.10, 1.0400, 0.9177), (0.30, 1.0396, 0.9173), (0.50, 1.0367, 0.9144),
            (0.70, 1.0280, 0.9057), (1.00, 1.0123, 0.8900)]
ENERGY_75 = [(0.10, 1.2353, 0.9154), (0.30, 1.2018, 0.8819), (0.50, 1.1087, 0.7888),
             (0.70, 1.0625, 0.7426), (1.00, 1.0337, 0.7138)]

# §E full sweep, worst R(v)
WORST_TWO_CROSS = 0.9255
WORST_HALF = 1.3395
WORST_UNCOUPLED = 1.4300

CHECKS = 29


# ===========================================================================
# Figure 1 — the door: one photon, counted twice  (§1)
# ===========================================================================
def fig_door():
    uid = "l64f1"
    W, H = 720, 340
    b = [box(0, 0, W, H)]

    b.append(label(W / 2, 28, "One photon arrives. How many times may it leave?", "lg"))

    # The bar geometry. 1.0 sits at 62% of the track so the overflow has room.
    x0, track = 210, 330
    unit = track / 1.25
    rows = [
        ("Lessons 3.7 - 6.3", 1.0000, 0.1386, 1.1386, RED,
         "two lobes, added"),
        ("Lesson 6.4", 0.8777, 0.0367, 0.9144, GREEN,
         "one photon, spent once"),
    ]

    # the unity line, drawn first so the bars sit on top of it
    ux = x0 + unit
    b.append(rule(ux, 62, ux, 258, "ink", 1.6))
    b.append(label(ux, 52, "1.0  =  what arrived", "sm"))

    y = 92
    for name, diff, spec, total, colour, note in rows:
        b.append(label(x0 - 16, y + 20, name, "sm", "end"))
        b.append(label(x0 - 16, y + 38, note, "xs", "end"))

        dw = diff * unit
        sw = spec * unit
        b.append(box(x0, y, dw, 34, BLUE, opacity=0.45))
        b.append(box(x0 + dw, y, sw, 34, AMBER, opacity=0.75))
        b.append(label(x0 + dw / 2, y + 22, f"diffuse {diff:.4f}", "xs"))

        # The specular sliver is too narrow for an in-bar label at either scale.
        # Callouts go in a FIXED right-hand column rather than hanging off each
        # bar's end: at 0.9144 the bar ends just short of the unity line, so a
        # label hung off it lands on the line.
        b.append(label(x0 + track + 12, y + 22, f"+ specular {spec:.4f}", "xs", "start"))

        # the total, and whether it overran
        b.append(label(x0 + total * unit, y - 8, f"{total:.4f}", "sm"))
        if total > 1.0:
            b.append(box(ux, y, (total - 1.0) * unit, 34, RED, opacity=0.55))
        y += 84

    b.append(box(x0 - 190, 262, 590, 62, None))
    b.append(label(W / 2 - 5, 284,
                   "The fault is the PLUS SIGN, not either term.", "sm"))
    b.append(label(W / 2 - 5, 306,
                   "F bounces off, so 1 - F goes in — and only what goes in comes back out.",
                   "xs"))

    return svg(uid, W, H,
               "The energy budget, before and after Fresnel coupling",
               "Two stacked bars against a line marking the light that arrived. Before this "
               "lesson the diffuse lobe returned the full albedo and the specular lobe was added "
               "on top, totalling 1.1386 and overrunning the line. After coupling, the diffuse "
               "lobe receives only the light that got past the interface and the two together "
               "total 0.9144.", b)


# ===========================================================================
# Figure 2 — the doubling, and where the 4 comes from  (§4)
# ===========================================================================
def fig_jacobian():
    uid = "l64f2"
    W, H = 720, 400
    b = [box(0, 0, W, H)]
    b.append(label(W / 2, 26, "Tilt the mirror by θ, and the ray turns by 2θ", "lg"))

    cx, cy, R = 250, 300, 190
    b.append(rule(cx - 210, cy, cx + 210, cy, "ink", 1.6))

    # the fixed direction v, straight up
    b.append(arrow(cx, cy, cx, cy - R, uid, "i", 1.8))
    b.append(label(cx + 8, cy - R - 6, "v  (fixed)", "sm", "start"))

    # ONE facet normal h tilted by theta, and the reflected l at 2 theta. A
    # second pair was drawn first and cut: it put four labels and four strokes
    # into one quadrant and showed nothing the first pair does not.
    TH = 20.0
    th = math.radians(TH)
    hx, hy = cx + R * 0.72 * math.sin(th), cy - R * 0.72 * math.cos(th)
    b.append(arrow(cx, cy, hx, hy, uid, "g", 1.6, "4 3"))
    # Labels sit BEYOND each arrowhead, along the ray's own direction, so they
    # cannot land on the stroke they name — and the two rays diverge, so they
    # cannot land on each other's either.
    b.append(label(cx + 168 * math.sin(th), cy - 168 * math.cos(th) - 6, "h", "sm"))

    t2 = 2.0 * th
    lx, ly = cx + R * math.sin(t2), cy - R * math.cos(t2)
    b.append(arrow(cx, cy, lx, ly, uid, "b", 1.9))
    b.append(label(cx + 212 * math.sin(t2) + 10, cy - 212 * math.cos(t2), "l", "sm", "start"))

    # the angle arcs, each labelled just outside its own arc
    # The label angle is given explicitly rather than taken as the arc's
    # midpoint: the 2-theta arc's midpoint IS the h ray by construction, so the
    # obvious choice puts the label exactly on a stroke. 30 degrees sits in the
    # gap between h (20) and l (40).
    for th_deg, r, lab_deg, name in ((TH, 62, 10.0, "θ"), (2 * TH, 96, 30.0, "2θ")):
        a = math.radians(th_deg)
        b.append(f'<path d="M {cx} {cy - r} A {r} {r} 0 0 1 '
                 f'{cx + r * math.sin(a):.2f} {cy - r * math.cos(a):.2f}" '
                 f'class="grid" fill="none" stroke-width="1.2"/>')
        m = math.radians(lab_deg)
        b.append(label(cx + (r + 18) * math.sin(m),
                       cy - (r + 18) * math.cos(m) + 4, name, "sm"))

    # ---- the algebra, at the right ----------------------------------------
    px = 470
    b.append(box(px, 62, 232, 250, None))
    b.append(label(px + 116, 86, "the whole derivation", "sm"))
    lines = [
        ("θ_out  =  2 θ_h", "sm"),
        ("", "xs"),
        ("dω_out  =  sin(2θ) · 2 dθ dφ", "xs"),
        ("dω_h    =  sin(θ) · dθ dφ", "xs"),
        ("", "xs"),
        ("ratio  =  2 sin2θ / sinθ", "xs"),
        ("        =  4 cos θ", "sm"),
        ("        =  4 (v·h)", "sm"),
    ]
    y = 112
    for text, cls in lines:
        if text:
            b.append(label(px + 116, y, text, cls))
        y += 22 if cls == "xs" else 26

    b.append(box(px, 322, 232, 54, GREEN, opacity=0.10))
    b.append(label(px + 116, 342, "measured: 1.42e-03 worst", "xs"))
    b.append(label(px + 116, 360, "over sixteen configurations", "xs"))

    return svg(uid, W, H,
               "The half-vector Jacobian, derived from a double angle",
               "A microfacet normal h tilted by theta from the fixed direction v "
               "reflects v into a direction l at exactly 2 theta. Working in spherical "
               "coordinates centred on v, the solid angle around l is therefore "
               "stretched relative to the solid angle around h by 2 sin(2 theta) over "
               "sin(theta), which the double-angle identity turns into 4 cos theta, "
               "that is 4 times v dot h.", b)


# ===========================================================================
# Figure 3 — where every factor of the denominator comes from  (§4)
# ===========================================================================
def fig_denominator():
    uid = "l64f3"
    W, H = 720, 330
    b = [box(0, 0, W, H)]
    b.append(label(W / 2, 26, "D G F / 4 (n·l)(n·v):  four factors, four reasons", "lg"))

    cols = [
        ("4", "the Jacobian", "dω_h → dω_l stretches", "by 4(v·h). Figure 2.", GREEN),
        ("(v·h)", "CANCELS", "the facets' projected", "area toward l is (l·h),", GREY),
        ("(n·v)", "flux → radiance", "radiance is per unit", "PROJECTED area", BLUE),
        ("(n·l)", "the definition", "a BRDF is per unit", "irradiance", AMBER),
    ]
    w = 160
    gap = (W - 4 * w) / 5
    for i, (sym, title, l1, l2, col) in enumerate(cols):
        x = gap + i * (w + gap)
        b.append(box(x, 62, w, 190, col, opacity=0.10))
        b.append(hollow(x, 62, w, 190, col))
        b.append(label(x + w / 2, 100, sym, "lg"))
        b.append(label(x + w / 2, 128, title, "sm"))
        b.append(rule(x + 18, 142, x + w - 18, 142, "grid", 1.0))
        b.append(label(x + w / 2, 166, l1, "xs"))
        b.append(label(x + w / 2, 186, l2, "xs"))
        if sym == "(v·h)":
            b.append(label(x + w / 2, 208, "and l·h = v·h", "xs"))
            b.append(label(x + w / 2, 228, "because h bisects", "xs"))

    b.append(box(gap, 268, W - 2 * gap, 44, None))
    b.append(label(W / 2, 296,
                   "The (v·h) that the Jacobian introduces is cancelled by the one the "
                   "projected area introduces — which is why the formula looks unmotivated.",
                   "xs"))

    return svg(uid, W, H,
               "Every factor in the Cook-Torrance denominator, and its reason",
               "The 4 is the Jacobian of the map from light directions to microfacet "
               "normals. The (v dot h) that Jacobian introduces cancels against the "
               "projected area of the facets toward the light, which is (l dot h) and "
               "equal to it because h bisects l and v. The (n dot v) converts flux to "
               "radiance, which is defined per unit projected area. The (n dot l) is "
               "the BRDF's own definition as a quantity per unit irradiance.", b)


# ===========================================================================
# Figure 4 — Fresnel: the shape, and what Schlick costs  (§5)
# ===========================================================================
def fig_fresnel():
    uid = "l64f4"
    W, H = 720, 420
    b = [box(0, 0, W, H)]
    b.append(label(W / 2, 26, "Every surface is a mirror at grazing incidence", "lg"))

    px, py, pw, ph = 84, 60, 420, 280
    b.append(box(px, py, pw, ph, None))

    # axes
    for f in (0.0, 0.25, 0.5, 0.75, 1.0):
        y = py + ph - f * ph
        b.append(rule(px, y, px + pw, y, "grid", 0.8, "2 4"))
        b.append(label(px - 10, y + 4, f"{f:.2f}", "xs", "end"))
    for a in (0, 30, 60, 90):
        x = px + (a / 90.0) * pw
        b.append(rule(x, py, x, py + ph, "grid", 0.8, "2 4"))
        b.append(label(x, py + ph + 18, f"{a}°", "xs"))
    b.append(label(px + pw / 2, py + ph + 40, "angle of incidence", "sm"))
    b.append(label(px - 52, py + ph / 2, "F", "sm"))

    def path(fn, colour, dash=None):
        pts = []
        for i in range(181):
            a = i * 0.5
            c = math.cos(math.radians(a))
            v = fn(c)
            pts.append(f"{px + (a / 90.0) * pw:.2f},{py + ph - v * ph:.2f}")
        d = f' stroke-dasharray="{dash}"' if dash else ""
        return (f'<polyline points="{" ".join(pts)}" fill="none" stroke="{colour}" '
                f'stroke-width="2"{d}/>')

    # three materials, exact
    for ior, colour, name, ly in ((1.5, BLUE, "glass  F0 = 0.04", 0),
                                  (2.42, PURPLE, "diamond  F0 = 0.17", 1),
                                  (1.33, GREEN, "water  F0 = 0.02", 2)):
        b.append(path(lambda c, n=ior: fresnel_exact(c, n), colour))

    # Schlick for glass, dashed, so the gap is visible
    b.append(path(lambda c: schlick(c, 0.04), RED, "5 4"))

    # legend
    ly = py + 16
    for colour, name, dash in ((BLUE, "glass, EXACT", None),
                               (PURPLE, "diamond, exact", None),
                               (GREEN, "water, exact", None),
                               (RED, "glass, SCHLICK", "5 4")):
        b.append(rule(px + 18, ly, px + 44, ly, "grid", 2.4, dash) .replace(
            'class="grid"', f'stroke="{colour}"'))
        b.append(label(px + 52, ly + 4, name, "xs", "start"))
        ly += 20

    # the everything-is-a-mirror annotation
    b.append(arrow(px + pw - 40, py + 40, px + pw - 4, py + 6, uid, "s", 1.3))
    b.append(label(px + pw - 46, py + 46, "all reach 1", "xs", "end"))

    # ---- the error panel ---------------------------------------------------
    qx = 536
    b.append(box(qx, py, 156, 280, None))
    b.append(label(qx + 78, py + 24, "Schlick's error", "sm"))
    b.append(label(qx + 78, py + 42, "(glass, ior 1.5)", "xs"))
    rows = [("worst |err|", "0.0357"), ("  at", "85°"),
            ("", ""),
            ("worst rel err", "23.2%"), ("  at", "55°"),
            ("", ""),
            ("at 60° exact", "0.0892"), ("Schlick says", "0.0700")]
    y = py + 70
    for k, v in rows:
        if k:
            b.append(label(qx + 12, y, k, "xs", "start"))
            b.append(label(qx + 144, y, v, "xs", "end"))
        y += 22
    b.append(box(qx + 10, py + 236, 136, 34, GREEN, opacity=0.10))
    b.append(label(qx + 78, py + 258, "0.019 of a reflectance", "xs"))

    b.append(label(W / 2, H - 14,
                   "Exact at both ends by construction; fitted in between.", "xs"))

    return svg(uid, W, H,
               "Fresnel reflectance against angle, exact and approximated",
               "Exact unpolarised Fresnel reflectance for water, glass and diamond, "
               "all rising from their normal-incidence value F0 to exactly 1 at "
               "grazing incidence. Schlick's approximation for glass is drawn dashed "
               "over the exact curve: it is exact at both ends by construction, worst "
               "by 0.0357 in absolute terms near 85 degrees, and worst by 23.2 percent "
               "in relative terms around 55 degrees.", b)


# ===========================================================================
# Figure 5 — where the colour lives  (§6)
# ===========================================================================
def fig_metal():
    uid = "l64f5"
    W, H = 720, 404
    b = [box(0, 0, W, H)]
    b.append(label(W / 2, 26, "A metal is not a shinier plastic", "lg"))

    for k, (cx, title, sub) in enumerate(((190, "DIELECTRIC", "plastic, paint, skin, glass"),
                                          (530, "CONDUCTOR", "gold, copper, iron"))):
        top, surf_y = 62, 200
        b.append(label(cx, top, title, "sm"))
        b.append(label(cx, top + 18, sub, "xs"))

        # the interface
        b.append(rule(cx - 140, surf_y, cx + 140, surf_y, "ink", 1.8))
        # the body below
        b.append(box(cx - 140, surf_y, 280, 66,
                     AMBER if k == 0 else GREY, opacity=0.16))

        # incoming
        b.append(arrow(cx - 118, top + 42, cx - 24, surf_y - 4, uid, "i", 1.8))

        # the specular bounce, coloured by material
        b.append(arrow(cx - 20, surf_y - 6, cx + 74, top + 46, uid,
                       "s" if k == 0 else "b", 1.8))
        b.append(label(cx + 82, top + 44,
                       "F0 = 0.04" if k == 0 else "F0 = albedo", "xs", "start"))
        b.append(label(cx + 82, top + 60,
                       "grey" if k == 0 else "COLOURED", "xs", "start"))

        if k == 0:
            # light enters, scatters, comes back out carrying the pigment
            b.append(arrow(cx - 20, surf_y + 2, cx + 6, surf_y + 40, uid, "s", 1.4, "3 3"))
            b.append(arrow(cx + 6, surf_y + 40, cx - 66, top + 52, uid, "b", 1.6))
            b.append(label(cx - 74, top + 46, "diffuse", "xs", "end"))
            b.append(label(cx, surf_y + 88, "pigment scatters it back out", "xs"))
            b.append(label(cx, surf_y + 108, "COLOUR LIVES IN THE ALBEDO", "sm"))
        else:
            # light enters and is absorbed within a few atoms
            b.append(arrow(cx - 20, surf_y + 2, cx - 2, surf_y + 34, uid, "s", 1.4, "3 3"))
            b.append(f'<circle cx="{cx - 2}" cy="{surf_y + 38}" r="7" fill="{RED}" '
                     f'fill-opacity="0.5" stroke="{RED}" stroke-width="1.2"/>')
            b.append(label(cx + 12, surf_y + 42, "absorbed", "xs", "start"))
            b.append(label(cx, surf_y + 88, "no diffuse lobe AT ALL", "xs"))
            b.append(label(cx, surf_y + 108, "COLOUR LIVES IN F0", "sm"))

    b.append(box(60, 330, W - 120, 56, None))
    b.append(label(W / 2, 352,
                   "One albedo field serves both. `metallic` decides which question it answers:",
                   "xs"))
    b.append(label(W / 2 - 150, 372, "F0 = lerp(0.04, albedo, metallic)", "sm"))
    b.append(label(W / 2 + 150, 372, "diffuse = albedo × (1 - metallic)", "sm"))

    return svg(uid, W, H,
               "Why a metal's colour lives in F0 and a dielectric's in its albedo",
               "A dielectric reflects a grey four percent off its clear surface layer "
               "and lets the rest in, where pigment scatters it back out carrying the "
               "colour, so its diffuse lobe is coloured and its F0 is grey. A "
               "conductor absorbs everything that crosses the interface within a few "
               "atomic layers, so it has no diffuse lobe at all and the colour has "
               "nowhere to live but F0.", b)


# ===========================================================================
# Figure 6 — the energy budget, and what is left over  (§7)
# ===========================================================================
def fig_energy():
    uid = "l64f6"
    W, H = 720, 420
    b = [box(0, 0, W, H)]
    b.append(label(W / 2, 26, "The door shuts — and the two forms that do not", "lg"))

    px, py, pw, ph = 96, 62, 470, 250
    b.append(box(px, py, pw, ph, None))

    # ZERO-BASED, deliberately. The first draft started the axis at 0.6, which
    # made 0.71 and 1.24 look like a five-fold difference. A bar's length is its
    # value; a truncated baseline makes the drawing lie about the data it is
    # there to report.
    lo, hi = 0.0, 1.5
    def ytr(v):
        return py + ph - (v - lo) / (hi - lo) * ph

    for v in (0.0, 0.5, 1.0, 1.5):
        y = ytr(v)
        cls = "ink" if abs(v - 1.0) < 1e-9 else "grid"
        b.append(rule(px, y, px + pw, y, cls, 1.6 if cls == "ink" else 0.8,
                      None if cls == "ink" else "2 4"))
        b.append(label(px - 10, y + 4, f"{v:.1f}", "xs", "end"))
    b.append(label(px + pw + 8, ytr(1.0) + 4, "unity", "xs", "start"))
    b.append(label(px - 56, py + ph / 2, "R(v)", "sm"))

    n = len(ENERGY_0)
    slot = pw / n
    for i, ((r, un0, co0), (_, un75, co75)) in enumerate(zip(ENERGY_0, ENERGY_75)):
        x0 = px + i * slot
        b.append(label(x0 + slot / 2, py + ph + 20, f"{r:.2f}", "xs"))
        bw = 22
        # uncoupled at 75 (the worst case), then coupled
        for j, (v, colour, _lab) in enumerate(((un75, RED, "uncoupled, 75°"),
                                               (un0, AMBER, "uncoupled, 0°"),
                                               (co0, GREEN, "coupled, 0°"),
                                               (co75, BLUE, "coupled, 75°"))):
            x = x0 + slot / 2 - 2 * bw + j * bw + 2
            y = ytr(v)
            b.append(box(x, y, bw - 4, py + ph - y, colour, opacity=0.55))
    b.append(label(px + pw / 2, py + ph + 40, "roughness", "sm"))

    # The legend sits BELOW the plot, not inside it: at 1.2353 the tallest bar
    # reaches into the top-left corner, where the first draft put the legend.
    lx, ly = px + 4, py + ph + 62
    for colour, name in ((RED, "uncoupled, eye at 75°"), (AMBER, "uncoupled, eye at 0°"),
                         (GREEN, "COUPLED, eye at 0°"), (BLUE, "COUPLED, eye at 75°")):
        b.append(box(lx, ly - 9, 13, 10, colour, opacity=0.55))
        b.append(label(lx + 19, ly, name, "xs", "start"))
        lx += 122

    # ---- the worst-case panel ---------------------------------------------
    qx = 592
    b.append(box(qx, py, 108, 250, None))
    b.append(label(qx + 54, py + 22, "worst R(v)", "sm"))
    b.append(label(qx + 54, py + 38, "over the sweep", "xs"))
    rows = [("no coupling", f"{WORST_UNCOUPLED:.4f}", RED),
            ("1 - F(v·h)", f"{WORST_HALF:.4f}", AMBER),
            ("two-crossing", f"{WORST_TWO_CROSS:.4f}", GREEN)]
    y = py + 70
    for name, val, colour in rows:
        b.append(box(qx + 10, y - 14, 88, 46, colour, opacity=0.12))
        b.append(label(qx + 54, y + 2, name, "xs"))
        b.append(label(qx + 54, y + 22, val, "sm"))
        y += 62
    b.append(label(qx + 54, py + 236, "ours is the green", "xs"))

    b.append(label(W / 2, H - 14,
                   "The coupled model never reaches 1 — and never quite reaches it, "
                   "which is the other half of the story.", "xs"))

    return svg(uid, W, H,
               "Hemispherical reflectance with and without Fresnel coupling",
               "Total reflectance R of a white surface against roughness, for an eye "
               "at 0 and at 75 degrees. Without coupling the surface reflects more "
               "light than arrives at every roughness, worst 1.2353 at grazing. With "
               "the two-crossing coupling it never exceeds 1, worst 0.9255 over a full "
               "sweep, where the common half-vector form still reaches 1.3395.", b)


def main():
    figs = [("l64_fig1.svg", fig_door()),
            ("l64_fig2.svg", fig_jacobian()),
            ("l64_fig3.svg", fig_denominator()),
            ("l64_fig4.svg", fig_fresnel()),
            ("l64_fig5.svg", fig_metal()),
            ("l64_fig6.svg", fig_energy())]
    for name, body in figs:
        path = os.path.join(OUT, name)
        with open(path, "w") as fh:
            fh.write(body)
        print(f"  wrote {path}  ({len(body):,} bytes)")


if __name__ == "__main__":
    main()
