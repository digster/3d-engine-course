#!/usr/bin/env python3
"""Compute every SVG figure for Lesson 3.8 from real geometry and real data.

Same discipline as 3.7's: nothing is placed by eye, and every number printed here
is a number the page quotes. Lesson 3.7 added a second rule after Figure 5 passed
every automated check while burying its own claim — so each figure below states,
in its docstring, the ONE thing a reader should be able to extract by measuring it.

Writes scratch/l38_fig{1..5}.svg.
"""
import math

# The measured cost sweep from scratch/verify_38.log §G. Kept here as data rather
# than retyped into the SVG, so the figure and the log cannot drift apart.
COST = [
    # (label, covered px, px per triangle, gouraud ms, per-pixel ms)
    ("320×180",   6346,   2.8,  0.341,  0.312),
    ("640×360",   25400,  11.0, 0.618,  0.873),
    ("1280×720",  101701, 44.1, 1.711,  3.149),
    ("2560×1440", 406687, 176.5, 5.785, 12.250),
    ("3840×2160", 914492, 396.9, 12.687, 27.242),
]


def f(v):
    return f"{v:.1f}"


DEFS = """  <defs>
    <marker id="d-n" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6"
            orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="var(--ok-bd)"/></marker>
    <marker id="d-s" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6"
            orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="var(--verify-bd)"/></marker>
    <marker id="d-a" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6"
            orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="var(--dia-ink-soft)"/></marker>
  </defs>
"""


def svg(w, h, title, desc, body, tid):
    return (f'<svg viewBox="0 0 {w} {h}" role="img" aria-labelledby="{tid}-t {tid}-d">\n'
            f'  <title id="{tid}-t">{title}</title>\n'
            f'  <desc id="{tid}-d">{desc}</desc>\n{DEFS}{body}\n</svg>\n')


def write(name, text):
    with open(f"scratch/{name}", "w") as fh:
        fh.write(text)
    print(f"  wrote scratch/{name}")


# =============================================================================
# Figure 1 — the two axes, as a grid
# =============================================================================
# ONE CLAIM: the six cells are a grid, not a list, and three of them collapse to
# the same picture — but only in the diffuse-only column.
def fig1():
    X0, Y0 = 168.0, 118.0
    CW, CH = 148.0, 74.0
    cols = ["flat\n(per triangle)", "Gouraud\n(per vertex)", "per-pixel\n(per fragment)"]
    rows = ["FACE\nnormal", "VERTEX\nnormal"]

    b = []
    b.append('<text x="24" y="34" class="sm" font-weight="700">Two questions, not one.</text>')
    b.append('<text x="24" y="52" class="xs muted">diffuse-only model</text>')

    for ci, c in enumerate(cols):
        cx = X0 + ci * CW + CW / 2
        for li, line in enumerate(c.split("\n")):
            b.append(f'<text x="{f(cx)}" y="{f(Y0 - 26 + li*13)}" class="xs muted" '
                     f'text-anchor="middle">{line}</text>')

    # The degenerate band: face-normal row, all three columns.
    b.append(f'<g class="grid"><rect x="{f(X0)}" y="{f(Y0)}" width="{f(CW*3)}" '
             f'height="{f(CH)}" rx="5" fill="var(--ok-bg)" fill-opacity="0.75"/></g>')

    for ri, r in enumerate(rows):
        ry = Y0 + ri * CH
        for li, line in enumerate(r.split("\n")):
            b.append(f'<text x="{f(X0 - 16)}" y="{f(ry + CH/2 - 4 + li*13)}" '
                     f'class="xs muted" text-anchor="end">{line}</text>')
        for ci in range(3):
            cx = X0 + ci * CW
            b.append(f'<rect x="{f(cx)}" y="{f(ry)}" width="{f(CW)}" height="{f(CH)}" '
                     f'fill="none" stroke="var(--rule-strong)" stroke-width="1"/>')

    # Cell contents.
    def cell(ci, ri, main, sub, cls="sm"):
        cx = X0 + ci * CW + CW / 2
        cy = Y0 + ri * CH
        b.append(f'<text x="{f(cx)}" y="{f(cy + 30)}" class="{cls}" text-anchor="middle" '
                 f'font-weight="700">{main}</text>')
        b.append(f'<text x="{f(cx)}" y="{f(cy + 48)}" class="xs muted" '
                 f'text-anchor="middle">{sub}</text>')

    cell(0, 0, "flat", "the classic")
    cell(1, 0, "= flat", "0 px differ")
    cell(2, 0, "= flat", "0 px differ")
    cell(0, 1, "faceted", "5,285 px vs per-pixel")
    cell(1, 1, "Gouraud", "4,344 px vs per-pixel")
    cell(2, 1, "Phong shading", "the reference")

    b.append(f'<text x="{f(X0 + CW*1.5)}" y="{f(Y0 - 48)}" class="xs t-ok" '
             f'text-anchor="middle">a constant normal makes all three agree — exactly</text>')

    print("Figure 1 — the 2x3 grid (numbers from verify_38 §A)")
    write("l38_fig1.svg", svg(700, 268,
        "The 2 by 3 grid of normal source against evaluation point",
        "A two-row, three-column grid. The rows are labelled FACE normal and VERTEX normal; the "
        "columns are flat (per triangle), Gouraud (per vertex) and per-pixel (per fragment). The "
        "whole FACE row is shaded to show that all three of its cells produce the identical "
        "picture, zero pixels differing. The VERTEX row does not: faceted differs from per-pixel "
        "by 5,285 pixels and Gouraud by 4,344. A note below records that the top row separates "
        "too once a view-dependent specular term is switched on, by 1,231 and 761 pixels.",
        "".join(b), "g1"))


# =============================================================================
# Figure 2 — where the equation is evaluated, on one triangle
# =============================================================================
# ONE CLAIM: the three modes differ in how many times shade() is called and where,
# and nothing else.
def fig2():
    b = []
    PANEL = 210.0
    tri = [(58.0, 150.0), (152.0, 150.0), (105.0, 58.0)]

    b.append('<text x="16" y="26" class="sm" font-weight="700">The same triangle, the same '
             'equation. Only the sample points move.</text>')

    labels = [("FLAT", "1 call, at the centroid"),
              ("GOURAUD", "3 calls, at the corners"),
              ("PER-PIXEL", "1 call per covered pixel")]

    for pi in range(3):
        ox = 16 + pi * PANEL
        pts = " ".join(f"{f(x+ox)},{f(y)}" for x, y in tri)
        b.append(f'<polygon points="{pts}" fill="var(--dia-fill)" '
                 f'stroke="var(--dia-ink)" stroke-width="1.6"/>')

        if pi == 0:
            cx = ox + sum(p[0] for p in tri) / 3
            cy = sum(p[1] for p in tri) / 3
            b.append(f'<circle cx="{f(cx)}" cy="{f(cy)}" r="5" fill="var(--verify-bd)"/>')
        elif pi == 1:
            for x, y in tri:
                b.append(f'<circle cx="{f(x+ox)}" cy="{f(y)}" r="5" '
                         f'fill="var(--verify-bd)"/>')
        else:
            # Sample every covered pixel on a coarse lattice, so the reader can
            # count them and see that the count is the point.
            n = 0
            step = 11
            y = 62
            while y < 150:
                x = 58
                while x < 152:
                    # inside test, barycentric sign
                    def edge(ax, ay, bx, by, px_, py_):
                        return (bx-ax)*(py_-ay) - (by-ay)*(px_-ax)
                    e0 = edge(*tri[0], *tri[1], x, y)
                    e1 = edge(*tri[1], *tri[2], x, y)
                    e2 = edge(*tri[2], *tri[0], x, y)
                    if (e0 >= 0 and e1 >= 0 and e2 >= 0) or (e0 <= 0 and e1 <= 0 and e2 <= 0):
                        b.append(f'<circle cx="{f(x+ox)}" cy="{f(y)}" r="2.4" '
                                 f'fill="var(--verify-bd)"/>')
                        n += 1
                    x += step
                y += step
            print(f"    per-pixel panel: {n} sample dots drawn on an 11px lattice")

        main, sub = labels[pi]
        b.append(f'<text x="{f(ox + 105)}" y="180" class="sm" text-anchor="middle" '
                 f'font-weight="700">{main}</text>')
        b.append(f'<text x="{f(ox + 105)}" y="196" class="xs muted" '
                 f'text-anchor="middle">{sub}</text>')

    b.append('<text x="16" y="226" class="xs muted">Red dots are calls to shade(). The '
             'equation, the normal matrix, the light and the material are identical in all '
             'three.</text>')

    print("Figure 2 — evaluation points")
    write("l38_fig2.svg", svg(660, 244,
        "Where the shading equation is evaluated, on one triangle, three ways",
        "Three copies of the same triangle side by side. The first, labelled FLAT, has a single "
        "red dot at its centroid: one call to the shading equation. The second, labelled GOURAUD, "
        "has three red dots, one at each corner. The third, labelled PER-PIXEL, is covered in a "
        "lattice of small red dots, one for every covered pixel. The caption notes that the "
        "equation itself is identical in all three.",
        "".join(b), "g2"))


# =============================================================================
# Figure 3 — the interpolated normal is short
# =============================================================================
# ONE CLAIM: lerping two unit vectors gives a short one, by cos(theta/2), and the
# shortfall is a brightness error if nobody renormalises.
def fig3():
    OX, OY = 330.0, 232.0
    R = 150.0
    THETA = 60.0

    def at(deg, r=R):
        a = math.radians(deg)
        return (OX + math.sin(a) * r, OY - math.cos(a) * r)

    a = at(-THETA / 2)
    c = at(THETA / 2)
    mid = ((a[0] + c[0]) / 2, (a[1] + c[1]) / 2)
    mid_len = math.cos(math.radians(THETA / 2))

    b = []
    b.append(f'<path class="arcline" d="M {f(a[0])},{f(a[1])} A {f(R)},{f(R)} 0 0 1 '
             f'{f(c[0])},{f(c[1])}" fill="none" stroke="var(--dia-ink-soft)" '
             f'stroke-width="1.2" stroke-dasharray="4 4"/>')
    b.append(f'<line x1="{f(a[0])}" y1="{f(a[1])}" x2="{f(c[0])}" y2="{f(c[1])}" '
             f'stroke="var(--verify-bd)" stroke-width="1.6"/>')

    for p, name in ((a, "n₀"), (c, "n₁")):
        b.append(f'<line x1="{f(OX)}" y1="{f(OY)}" x2="{f(p[0])}" y2="{f(p[1])}" '
                 f'stroke="var(--ok-bd)" stroke-width="2.4" marker-end="url(#d-n)"/>')
    b.append(f'<text x="{f(a[0]-24)}" y="{f(a[1]-8)}" class="lbl-n sm" '
             f'font-weight="700">n₀</text>')
    b.append(f'<text x="{f(c[0]+10)}" y="{f(c[1]-8)}" class="lbl-n sm" '
             f'font-weight="700">n₁</text>')

    b.append(f'<line x1="{f(OX)}" y1="{f(OY)}" x2="{f(mid[0])}" y2="{f(mid[1])}" '
             f'stroke="var(--verify-bd)" stroke-width="2.6" marker-end="url(#d-s)"/>')
    b.append(f'<text x="{f(OX)}" y="{f(OY + 28)}" class="t-bad sm" '
             f'text-anchor="middle" font-weight="700">the interpolated normal: '
             f'|n| = cos({THETA/2:.0f}°) = {mid_len:.5f}</text>')

    # The gap between the chord's midpoint and the arc.
    tip = at(0)
    b.append(f'<line x1="{f(mid[0])}" y1="{f(mid[1])}" x2="{f(tip[0])}" y2="{f(tip[1])}" '
             f'stroke="var(--dia-ink-soft)" stroke-width="1" stroke-dasharray="2 3"/>')
    b.append(f'<circle cx="{f(tip[0])}" cy="{f(tip[1])}" r="3" fill="var(--dia-ink-soft)"/>')
    b.append(f'<text x="{f(tip[0])}" y="{f(tip[1]-16)}" class="xs muted" '
             f'text-anchor="middle">unit length lives here</text>')

    b.append('<text x="16" y="30" class="sm" font-weight="700">A chord passes inside the '
             'circle.</text>')
    b.append(f'<text x="16" y="50" class="xs muted">{THETA:.0f}&#176; apart &#8594; '
             f'{100*(1-mid_len):.1f}% short</text>')

    print(f"Figure 3 — chord shortening at {THETA:.0f} deg: |mid| = {mid_len:.5f} "
          f"({100*(1-mid_len):.2f}% short)")
    write("l38_fig3.svg", svg(660, 286,
        "Interpolating two unit normals produces a shorter vector",
        "Two unit normal arrows leave a common origin, 60 degrees apart, their tips on a dashed "
        "circular arc. A straight red chord joins the two tips. A red arrow from the origin to "
        "the midpoint of that chord is visibly shorter than the two unit arrows: its length is "
        "the cosine of 30 degrees, 0.86603, which is 13.4 percent short. A dotted line marks the "
        "gap between the chord's midpoint and the arc where unit length would be.",
        "".join(b), "g3"))


# =============================================================================
# Figure 4 — Mach bands: continuous value, discontinuous slope
# =============================================================================
# ONE CLAIM: Gouraud's intensity is C0 but not C1. The first draft plotted only the
# VALUE, and at six facets the dashed chord hugs the true curve so closely that the
# figure appeared to disprove its own caption. The content is in the DERIVATIVE, so
# the derivative is now plotted underneath — where the staircase is unmissable.
def fig4():
    X0 = 78.0
    W = 500.0
    TOP_Y, TOP_H = 190.0, 118.0     # value panel baseline and height
    BOT_Y, BOT_H = 300.0, 64.0      # slope panel baseline and height
    N = 6
    SPAN = 150.0

    def intensity(deg):
        return max(0.0, math.cos(math.radians(deg - 75.0)))

    b = []

    # ---- value panel ------------------------------------------------------
    b.append(f'<g class="grid" stroke="var(--dia-grid)">'
             f'<line x1="{f(X0)}" y1="{f(TOP_Y-TOP_H)}" x2="{f(X0+W)}" y2="{f(TOP_Y-TOP_H)}" '
             f'stroke-width="1"/></g>')
    b.append(f'<line x1="{f(X0)}" y1="{f(TOP_Y)}" x2="{f(X0+W+8)}" y2="{f(TOP_Y)}" '
             f'stroke="var(--dia-ink)" stroke-width="1.4"/>')
    b.append(f'<line x1="{f(X0)}" y1="{f(TOP_Y)}" x2="{f(X0)}" y2="{f(TOP_Y-TOP_H-10)}" '
             f'stroke="var(--dia-ink)" stroke-width="1.4"/>')
    b.append(f'<text x="{f(X0-8)}" y="{f(TOP_Y-TOP_H+4)}" class="xs muted" '
             f'text-anchor="end">1.0</text>')
    b.append(f'<text x="{f(X0-8)}" y="{f(TOP_Y+4)}" class="xs muted" '
             f'text-anchor="end">0</text>')
    b.append(f'<text x="{f(X0-64)}" y="{f(TOP_Y-TOP_H/2)}" class="xs muted" '
             f'text-anchor="middle" transform="rotate(-90 {f(X0-64)} '
             f'{f(TOP_Y-TOP_H/2)})">brightness</text>')

    pts = []
    for i in range(241):
        t = i / 240.0
        pts.append(f"{f(X0 + t*W)},{f(TOP_Y - intensity(t*SPAN)*TOP_H)}")
    b.append(f'<polyline points="{" ".join(pts)}" fill="none" stroke="var(--ok-bd)" '
             f'stroke-width="2.4"/>')

    gpts = []
    knots = []
    for i in range(N + 1):
        t = i / N
        x = X0 + t * W
        y = TOP_Y - intensity(t * SPAN) * TOP_H
        gpts.append(f"{f(x)},{f(y)}")
        knots.append((x, y, t * SPAN, intensity(t * SPAN)))
    b.append(f'<polyline points="{" ".join(gpts)}" fill="none" stroke="var(--verify-bd)" '
             f'stroke-width="2.2" stroke-dasharray="7 4"/>')
    for x, y, _, _ in knots:
        b.append(f'<circle cx="{f(x)}" cy="{f(y)}" r="3.2" fill="var(--verify-bd)"/>')
        b.append(f'<line x1="{f(x)}" y1="{f(TOP_Y)}" x2="{f(x)}" y2="{f(BOT_Y-BOT_H-6)}" '
                 f'stroke="var(--dia-grid)" stroke-width="0.8" stroke-dasharray="2 3"/>')

    b.append(f'<text x="{f(X0+W)}" y="{f(TOP_Y-TOP_H-22)}" class="xs t-ok" '
             f'text-anchor="end">solid: the true curve (per-pixel)</text>')
    b.append(f'<text x="{f(X0+W)}" y="{f(TOP_Y-TOP_H-8)}" class="xs t-bad" '
             f'text-anchor="end">dashed: Gouraud, {N} samples — they MEET at every knot</text>')

    # ---- slope panel — the actual content ---------------------------------
    slopes = []
    for i in range(N):
        s0 = knots[i][3]
        s1 = knots[i + 1][3]
        slopes.append((s1 - s0) * N)          # per unit of the horizontal span
    smax = max(abs(v) for v in slopes) * 1.25

    def sy(v):
        return BOT_Y - BOT_H / 2 - (v / smax) * (BOT_H / 2)

    b.append(f'<line x1="{f(X0)}" y1="{f(sy(0))}" x2="{f(X0+W+8)}" y2="{f(sy(0))}" '
             f'stroke="var(--dia-ink)" stroke-width="1.4"/>')
    b.append(f'<text x="{f(X0-8)}" y="{f(sy(0)+4)}" class="xs muted" '
             f'text-anchor="end">0</text>')
    b.append(f'<text x="{f(X0-64)}" y="{f(sy(0))}" class="xs muted" text-anchor="middle" '
             f'transform="rotate(-90 {f(X0-64)} {f(sy(0))})">slope</text>')

    # The true slope: smooth.
    tpts = []
    for i in range(241):
        t = i / 240.0
        d = (intensity(t * SPAN + 0.35) - intensity(t * SPAN - 0.35)) / 0.7 * SPAN
        tpts.append(f"{f(X0 + t*W)},{f(sy(d))}")
    b.append(f'<polyline points="{" ".join(tpts)}" fill="none" stroke="var(--ok-bd)" '
             f'stroke-width="2.2"/>')

    # Gouraud's slope: a staircase, one tread per facet.
    step = []
    for i, v in enumerate(slopes):
        step.append(f"{f(X0 + i/N*W)},{f(sy(v))}")
        step.append(f"{f(X0 + (i+1)/N*W)},{f(sy(v))}")
    b.append(f'<polyline points="{" ".join(step)}" fill="none" stroke="var(--verify-bd)" '
             f'stroke-width="2.4"/>')
    # The jumps between treads, drawn so the discontinuity is a line and not a gap.
    for i in range(1, N):
        x = X0 + i / N * W
        b.append(f'<line x1="{f(x)}" y1="{f(sy(slopes[i-1]))}" x2="{f(x)}" '
                 f'y2="{f(sy(slopes[i]))}" stroke="var(--verify-bd)" stroke-width="1.4" '
                 f'stroke-dasharray="2 2"/>')

    worst_i, worst = 1, 0.0
    for i in range(1, N):
        if abs(slopes[i] - slopes[i - 1]) > worst:
            worst = abs(slopes[i] - slopes[i - 1])
            worst_i = i
    wx = X0 + worst_i / N * W
    b.append(f'<circle cx="{f(wx)}" cy="{f((sy(slopes[worst_i-1])+sy(slopes[worst_i]))/2)}" '
             f'r="9" fill="none" stroke="var(--verify-bd)" stroke-width="1.5"/>')
    b.append(f'<text x="{f(wx)}" y="{f(BOT_Y + 18)}" class="xs t-bad" '
             f'text-anchor="middle">biggest jump — a Mach band appears here</text>')

    b.append('<text x="16" y="28" class="sm" font-weight="700">Continuous value, '
             'discontinuous slope — and the eye responds to the slope.</text>')
    b.append(f'<text x="{f(X0+W/2)}" y="{f(BOT_Y + 40)}" class="sm muted" '
             f'text-anchor="middle">across the surface — {N} flat facets spanning '
             f'{SPAN:.0f}&#176;</text>')

    print(f"Figure 4 — Mach bands. {N} facets over {SPAN:.0f} deg.")
    print(f"    Gouraud slope treads: {['%.3f' % v for v in slopes]}")
    print(f"    biggest slope jump at knot {worst_i}: {worst:.4f} "
          f"(true slope is continuous everywhere)")
    write("l38_fig4.svg", svg(660, 356,
        "Gouraud shading gives a continuous brightness but a discontinuous slope",
        "Two stacked graphs sharing a horizontal axis across a curved surface split into six flat "
        "facets. The upper graph shows brightness: a smooth solid green curve and a dashed red "
        "polyline that touches it exactly at each of the seven facet boundaries, so the two are "
        "nearly indistinguishable. The lower graph shows the slope of each. The true slope is a "
        "smooth descending green curve; Gouraud's is a red staircase, constant across each facet "
        "and jumping abruptly at every join. The largest jump is circled and labelled as the "
        "place a Mach band appears.",
        "".join(b), "g4"))


# =============================================================================
# Figure 5 — the cost crossover
# =============================================================================
# ONE CLAIM: per-pixel is not simply "more expensive". It is cheaper than Gouraud
# when triangles are smaller than a few pixels, and about 2.15x at high resolution.
def fig5():
    X0, Y0 = 96.0, 260.0
    W, H = 500.0, 190.0

    xs = [c[2] for c in COST]          # pixels per triangle
    ratios = [c[4] / c[3] for c in COST]

    lo, hi = math.log10(min(xs)) - 0.15, math.log10(max(xs)) + 0.15
    rmax = 2.4

    def gx(v):
        return X0 + (math.log10(v) - lo) / (hi - lo) * W

    def gy(r):
        return Y0 - (r / rmax) * H

    b = []
    g = []
    for r in (0.5, 1.0, 1.5, 2.0):
        g.append(f'<line x1="{f(X0)}" y1="{f(gy(r))}" x2="{f(X0+W)}" y2="{f(gy(r))}" '
                 f'stroke-width="1"/>')
    for v in (1, 10, 100, 1000):
        if lo <= math.log10(v) <= hi:
            g.append(f'<line x1="{f(gx(v))}" y1="{f(Y0)}" x2="{f(gx(v))}" '
                     f'y2="{f(gy(rmax))}" stroke-width="1"/>')
    b.append(f'<g class="grid" stroke="var(--dia-grid)">{"".join(g)}</g>')

    b.append(f'<line x1="{f(X0)}" y1="{f(Y0)}" x2="{f(X0+W+10)}" y2="{f(Y0)}" '
             f'stroke="var(--dia-ink)" stroke-width="1.6"/>')
    b.append(f'<line x1="{f(X0)}" y1="{f(Y0)}" x2="{f(X0)}" y2="{f(gy(rmax))}" '
             f'stroke="var(--dia-ink)" stroke-width="1.6"/>')

    for r in (0.5, 1.0, 1.5, 2.0):
        b.append(f'<text x="{f(X0-10)}" y="{f(gy(r)+4)}" class="xs muted" '
                 f'text-anchor="end">{r:.1f}×</text>')
    for v in (1, 10, 100, 1000):
        if lo <= math.log10(v) <= hi:
            b.append(f'<text x="{f(gx(v))}" y="{f(Y0+18)}" class="xs muted" '
                     f'text-anchor="middle">{v}</text>')
    b.append(f'<text x="{f(X0+W/2)}" y="{f(Y0+38)}" class="sm muted" text-anchor="middle">'
             f'pixels covered per triangle (log scale)</text>')
    b.append(f'<text x="{f(X0-58)}" y="{f(gy(1.2))}" class="sm muted" text-anchor="middle" '
             f'transform="rotate(-90 {f(X0-58)} {f(gy(1.2))})">per-pixel / Gouraud</text>')

    # The break-even line.
    b.append(f'<line x1="{f(X0)}" y1="{f(gy(1.0))}" x2="{f(X0+W)}" y2="{f(gy(1.0))}" '
             f'stroke="var(--dia-ink-soft)" stroke-width="1.6" stroke-dasharray="6 4"/>')
    b.append(f'<text x="{f(X0+W)}" y="{f(gy(1.0)-8)}" class="xs muted" text-anchor="end">'
             f'break even — same cost</text>')

    pts = " ".join(f"{f(gx(x))},{f(gy(r))}" for x, r in zip(xs, ratios))
    b.append(f'<polyline points="{pts}" fill="none" stroke="var(--note-bd)" '
             f'stroke-width="2.6"/>')
    for (label, _cov, ppt, _mg, _mp), r in zip(COST, ratios):
        b.append(f'<circle cx="{f(gx(ppt))}" cy="{f(gy(r))}" r="4.2" '
                 f'fill="var(--note-bd)"/>')
        dy = 24 if r < 1.05 else -16
        b.append(f'<text x="{f(gx(ppt))}" y="{f(gy(r)+dy)}" class="xs muted" '
                 f'text-anchor="middle">{label}</text>')
        b.append(f'<text x="{f(gx(ppt))}" y="{f(gy(r)+dy+13)}" class="xs t-hi" '
                 f'text-anchor="middle">{r:.2f}&#215;</text>')

    b.append('<text x="16" y="30" class="sm" font-weight="700">“Per-pixel is expensive” '
             'has a precondition.</text>')
    b.append('<text x="16" y="50" class="xs muted">one mesh, resolution swept</text>')

    print("Figure 5 — cost crossover")
    for (label, cov, ppt, mg, mp), r in zip(COST, ratios):
        print(f"    {label:>11}  {cov:>7} px  {ppt:>6.1f} px/tri  "
              f"gouraud {mg:6.3f} ms  per-pixel {mp:6.3f} ms  {r:.2f}x")
    write("l38_fig5.svg", svg(660, 320,
        "The cost of per-pixel shading relative to Gouraud, against pixels per triangle",
        "A graph with pixels covered per triangle on a logarithmic horizontal axis from about 2 "
        "to about 400, and the ratio of per-pixel cost to Gouraud cost on the vertical axis. A "
        "dashed horizontal line marks break-even at 1.0. The measured curve starts below that "
        "line at 0.91 times for 2.8 pixels per triangle, crosses it, and climbs to 1.41, 1.84, "
        "2.12 and 2.15 times as the resolution rises to 4K. Per-pixel shading is cheaper than "
        "Gouraud when triangles are smaller than about three pixels.",
        "".join(b), "g5"))


if __name__ == "__main__":
    print("figs_38 — computing Lesson 3.8's figures\n")
    fig1()
    fig2()
    fig3()
    fig4()
    fig5()
