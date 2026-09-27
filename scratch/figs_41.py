#!/usr/bin/env python3
"""Compute every SVG figure for Lesson 4.1 from real data.

Same discipline as 3.7 through 3.10: nothing is placed by eye, and every number
drawn here is a number `scratch/verify_41.log` printed. Each figure's docstring
states the ONE claim a reader must be able to extract by MEASURING the picture.

Marker ids are namespaced per figure, and <text> never carries an inline `fill`
(course.css beats an SVG presentation attribute — apply-shared.py lints for it).

Writes scratch/l41_fig{1..6}.svg.
"""
import math

# ---- Measured inputs, from scratch/verify_41.log ----------------------------

# §A: (circumradius px, covered, shaded, helpers, quads, efficiency %, model %)
EFFICIENCY = [
    (64.0, 169938, 176484, 6546, 44121, 96.3, 94.1),
    (32.0,  42630,  45852, 3222, 11463, 93.0, 88.9),
    (16.0,  10638,  12264, 1626,  3066, 86.7, 80.0),
    ( 8.0,   2700,   3440,  740,   860, 78.5, 66.7),
    ( 4.0,    678,   1000,  322,   250, 67.8, 50.0),
    ( 2.0,    186,    448,  262,   112, 41.5, 33.3),
    ( 1.0,     48,    192,  144,    48, 25.0, 20.0),
]

# §D: efficiency % by block size, (radius, 2x2, 4x4, 8x8, 16x16)
BLOCKS = [
    (64, 96.1, 88.9, 77.6, 61.2),
    (32, 92.5, 80.4, 63.4, 42.7),
    (16, 85.8, 66.5, 44.0, 29.9),
    ( 8, 75.5, 48.7, 30.4,  8.9),
    ( 4, 62.4, 34.2, 10.5,  2.6),
    ( 2, 50.3, 17.7,  4.4,  1.1),
]

# §B / §C: the real scene.
SCENE_TRIS = 2306
SCENE_EFFICIENCY = 87.4
SCENE_HELPERS = 21005
SCENE_SHADED = 166360

# §C: (segments, triangles, efficiency %, scanline us, quad us, measured, predicted)
HELPER_COST = [
    (8,     66, 96.9, 4477.60, 4663.94, 1.04, 1.03),
    (16,   258, 94.6, 4251.23, 4923.96, 1.16, 1.06),
    (32,  1026, 90.6, 4291.54, 5241.85, 1.22, 1.10),
    (48,  2306, 87.4, 4374.69, 5440.48, 1.24, 1.14),
    (96,  9218, 80.0, 4534.29, 6087.21, 1.34, 1.25),
    (192,36866, 72.6, 5263.12, 7246.00, 1.38, 1.38),
]

# §E: (coherence px, cpu ns, simt ns, vs cpu, vs coherent)
DIVERGENCE = [
    (1024, 1.22, 3.14, 2.58, 1.00),
    (  64, 1.21, 3.22, 2.65, 1.03),
    (  16, 1.21, 4.78, 3.94, 1.52),
    (   8, 1.22, 6.05, 4.98, 1.93),
    (   4, 1.22, 6.06, 4.99, 1.93),
    (   2, 1.22, 6.06, 4.98, 1.93),
    (   1, 1.21, 6.06, 4.99, 1.93),
]
WARP = 32

# §F: (chains, ns per step, speedup vs 1)
LATENCY = [(1, 3.124, 1.00), (2, 1.558, 2.01), (4, 0.776, 4.03),
           (8, 0.443, 7.05), (16, 0.443, 7.05), (32, 0.112, 27.88)]

# §G: field counts, from the pinned SDL3 release-3.4.12 headers.
SDL_TOP, SDL_LEAVES, OURS_TOP = 9, 53, 10

INK = "var(--dia-ink)"
COVERED = "#78b4eb"     # a lane the triangle covers
HELPER = "#eb786e"      # a lane it does not
GRIDC = "#9aa0b0"


def f(v):
    return f"{v:.1f}"


def defs(tid):
    return f"""  <defs>
    <marker id="e-i-{tid}" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6"
            orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="var(--dia-ink)"/></marker>
    <marker id="e-s-{tid}" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6"
            orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="var(--dia-ink-soft)"/></marker>
    <marker id="e-h-{tid}" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6"
            orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="var(--dia-hi)"/></marker>
  </defs>
"""


def svg(w, h, title, desc, body, tid):
    for m in ("e-i", "e-s", "e-h"):
        body = body.replace(f"url(#{m})", f"url(#{m}-{tid})")
    return (f'<svg viewBox="0 0 {w} {h}" role="img" aria-labelledby="{tid}-t {tid}-d">\n'
            f'  <title id="{tid}-t">{title}</title>\n'
            f'  <desc id="{tid}-d">{desc}</desc>\n{defs(tid)}{body}\n</svg>\n')


def write(name, text):
    with open(f"scratch/{name}", "w") as fh:
        fh.write(text)
    print(f"  wrote scratch/{name}")


# =============================================================================
# Figure 1 — the quad, and why it cannot be smaller
# =============================================================================
# ONE CLAIM: a small triangle's lanes are mostly NOT covered, and the uncovered
# ones run anyway because ddx and ddy are differences between neighbours. A
# reader must be able to count the blue and red cells and get the ratio, and see
# the two derivative arrows connecting lane 0 to lanes 1 and 2.
def fig1():
    CELL = 26.0
    X0, Y0 = 60.0, 56.0
    N = 8                      # 8x8 pixels = 4x4 quads
    body = []

    # A triangle small enough that the waste is visible, in pixel coordinates.
    tri = [(1.4, 1.2), (6.6, 2.4), (2.6, 6.4)]

    def sx(px_):
        return X0 + px_ * CELL

    def inside(px_, py_):
        def edge(a, b, p):
            return (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0])
        p = (px_ + 0.5, py_ + 0.5)
        e = [edge(tri[0], tri[1], p), edge(tri[1], tri[2], p), edge(tri[2], tri[0], p)]
        return all(v >= 0 for v in e) or all(v <= 0 for v in e)

    body.append('<text x="16" y="26" class="sm muted">'
                'one triangle, one 2&#215;2 quad grid &#8212; lanes the shader runs on</text>')

    covered = helpers = quads = 0
    for qy in range(0, N, 2):
        for qx in range(0, N, 2):
            lanes = [(qx, qy), (qx + 1, qy), (qx, qy + 1), (qx + 1, qy + 1)]
            hit = [inside(a, b) for a, b in lanes]
            if not any(hit):
                continue
            quads += 1
            for (lx, ly), h in zip(lanes, hit):
                col = COVERED if h else HELPER
                if h:
                    covered += 1
                else:
                    helpers += 1
                body.append(f'<rect x="{f(sx(lx))}" y="{f(Y0 + ly * CELL)}" '
                            f'width="{f(CELL)}" height="{f(CELL)}" fill="{col}" '
                            f'opacity="0.85"/>')

    # The pixel grid, then the heavier quad grid on top of it.
    for i in range(N + 1):
        body.append(f'<line x1="{f(sx(i))}" y1="{f(Y0)}" x2="{f(sx(i))}" '
                    f'y2="{f(Y0 + N * CELL)}" class="grid" stroke-width="0.7"/>')
        body.append(f'<line x1="{f(X0)}" y1="{f(Y0 + i * CELL)}" x2="{f(X0 + N * CELL)}" '
                    f'y2="{f(Y0 + i * CELL)}" class="grid" stroke-width="0.7"/>')
    for i in range(0, N + 1, 2):
        body.append(f'<line x1="{f(sx(i))}" y1="{f(Y0)}" x2="{f(sx(i))}" '
                    f'y2="{f(Y0 + N * CELL)}" class="ink-soft" stroke-width="1.6"/>')
        body.append(f'<line x1="{f(X0)}" y1="{f(Y0 + i * CELL)}" x2="{f(X0 + N * CELL)}" '
                    f'y2="{f(Y0 + i * CELL)}" class="ink-soft" stroke-width="1.6"/>')

    pts = " ".join(f"{f(sx(a))},{f(Y0 + b * CELL)}" for a, b in tri)
    body.append(f'<polygon points="{pts}" fill="none" class="ink" stroke-width="2.4"/>')

    # The derivative arrows, on the one quad that shows them best.
    qx, qy = 2, 2
    cx0, cy0 = sx(qx) + CELL / 2, Y0 + qy * CELL + CELL / 2
    body.append(f'<line x1="{f(cx0)}" y1="{f(cy0)}" x2="{f(cx0 + CELL - 6)}" y2="{f(cy0)}" '
                f'class="hi" stroke-width="2" marker-end="url(#e-h)"/>')
    body.append(f'<line x1="{f(cx0)}" y1="{f(cy0)}" x2="{f(cx0)}" y2="{f(cy0 + CELL - 6)}" '
                f'class="hi" stroke-width="2" marker-end="url(#e-h)"/>')


    # Legend and the count, which is the measurable claim.
    lx = X0 + N * CELL + 26
    body.append(f'<rect x="{f(lx)}" y="{f(Y0 + 4)}" width="14" height="14" fill="{COVERED}"/>')
    body.append(f'<text x="{f(lx + 20)}" y="{f(Y0 + 15)}" class="xs">covered &#8212; {covered}</text>')
    body.append(f'<rect x="{f(lx)}" y="{f(Y0 + 26)}" width="14" height="14" fill="{HELPER}"/>')
    body.append(f'<text x="{f(lx + 20)}" y="{f(Y0 + 37)}" class="xs">helper &#8212; {helpers}</text>')
    body.append(f'<text x="{f(lx)}" y="{f(Y0 + 64)}" class="xs mono">{quads} quads issued</text>')
    body.append(f'<text x="{f(lx)}" y="{f(Y0 + 80)}" class="xs mono">'
                f'{covered + helpers} lanes shaded</text>')
    body.append(f'<text x="{f(lx)}" y="{f(Y0 + 96)}" class="xs mono t-hi">'
                f'{100.0 * covered / (covered + helpers):.0f}% efficient</text>')
    body.append(f'<text x="{f(lx)}" y="{f(Y0 + 128)}" class="xs mono t-hi">'
                f'&#8594;  ddx = lane 1 &#8722; lane 0</text>')
    body.append(f'<text x="{f(lx)}" y="{f(Y0 + 144)}" class="xs mono t-hi">'
                f'&#8595;  ddy = lane 2 &#8722; lane 0</text>')

    body.append(f'<text x="16" y="{f(Y0 + N * CELL + 28)}" class="sm">'
                f'The red lanes run the fragment shader and their results are thrown away.</text>')
    body.append(f'<text x="16" y="{f(Y0 + N * CELL + 44)}" class="sm muted">'
                f'They have to: <tspan class="mono">ddx</tspan> is lane 1 minus lane 0 and '
                f'<tspan class="mono">ddy</tspan> is lane 2 minus lane 0, so a lane needs its '
                f'neighbours to</text>')
    body.append(f'<text x="16" y="{f(Y0 + N * CELL + 60)}" class="sm muted">'
                f'have run the same code &#8212; whether or not the triangle covers them. '
                f'That is where mipmap selection comes from.</text>')

    return svg(640, int(Y0 + N * CELL + 76),
               "A 2x2 quad grid over a small triangle",
               "An 8 by 8 pixel grid divided into 2 by 2 quads, with a triangle drawn over "
               "it. Lanes the triangle covers are blue and lanes it does not are red; the red "
               "ones still run the fragment shader. Arrows on one quad show ddx as the "
               "difference between the two lanes in x and ddy as the difference in y, which "
               "is why the uncovered lanes have to run at all.",
               "\n".join("  " + s for s in body), "f411")


# =============================================================================
# Figure 2 — efficiency against size, and against block width
# =============================================================================
# ONE CLAIM: efficiency collapses as triangles shrink, and it collapses FASTER
# for wider blocks. A reader must be able to read 2x2 at r=8 (75.5%) against
# 16x16 at r=8 (8.9%) off the chart.
def fig2():
    X0, X1 = 74.0, 380.0
    Y0, Y1 = 44.0, 232.0
    body = []

    radii = [b[0] for b in BLOCKS]

    def sx(r):
        return X1 - (math.log2(r) - math.log2(min(radii))) / \
               (math.log2(max(radii)) - math.log2(min(radii))) * (X1 - X0)

    def sy(pct):
        return Y1 - pct / 100.0 * (Y1 - Y0)

    body.append(f'<rect x="{f(X0)}" y="{f(Y0)}" width="{f(X1 - X0)}" height="{f(Y1 - Y0)}" '
                f'class="fill-soft grid" stroke-width="1"/>')
    for pct in (0, 25, 50, 75, 100):
        body.append(f'<line x1="{f(X0)}" y1="{f(sy(pct))}" x2="{f(X1)}" y2="{f(sy(pct))}" '
                    f'class="grid" stroke-width="0.8"/>')
        body.append(f'<text x="{f(X0 - 6)}" y="{f(sy(pct) + 3)}" class="xs mono muted" '
                    f'text-anchor="end">{pct}%</text>')
    for r in radii:
        body.append(f'<text x="{f(sx(r))}" y="{f(Y1 + 15)}" class="xs mono muted" '
                    f'text-anchor="middle">{r}</text>')
    body.append(f'<text x="{f((X0 + X1) / 2)}" y="{f(Y1 + 32)}" class="xs muted" '
                f'text-anchor="middle">triangle circumradius, pixels (smaller to the right)</text>')
    body.append(f'<text x="{f(X0 - 40)}" y="{f((Y0 + Y1) / 2)}" class="xs muted" '
                f'transform="rotate(-90 {f(X0 - 40)} {f((Y0 + Y1) / 2)})" '
                f'text-anchor="middle">lane efficiency</text>')

    series = [("2&#215;2", 1, "z-quad2"), ("4&#215;4", 2, "z-quad4"),
              ("8&#215;8", 3, "z-quad8"), ("16&#215;16", 4, "z-quad16")]
    cols = {"z-quad2": COVERED, "z-quad4": "#ebc864", "z-quad8": "#f0a06a", "z-quad16": HELPER}
    for label, idx, cls in series:
        pts = " ".join(f"{f(sx(b[0]))},{f(sy(b[idx]))}" for b in BLOCKS)
        body.append(f'<polyline points="{pts}" fill="none" stroke="{cols[cls]}" '
                    f'stroke-width="2.2"/>')
        for b in BLOCKS:
            body.append(f'<circle cx="{f(sx(b[0]))}" cy="{f(sy(b[idx]))}" r="2.6" '
                        f'fill="{cols[cls]}"/>')
        last = BLOCKS[-1]
        # 8x8 finishes at 4.4% and 16x16 at 1.1%, which is three pixels apart —
        # so the last two labels are staggered rather than placed on their curves.
        nudge = {1: 3.0, 2: 3.0, 3: -4.0, 4: 12.0}[idx]
        body.append(f'<text x="{f(sx(last[0]) + 6)}" y="{f(sy(last[idx]) + nudge)}" '
                    f'class="xs mono {cls}">{label}</text>')

    # The one comparison the figure exists to support.
    body.append(f'<line x1="{f(sx(8))}" y1="{f(sy(78))}" x2="{f(sx(8))}" y2="{f(sy(8.9))}" '
                f'class="hi" stroke-width="1.4" stroke-dasharray="3 3"/>')
    body.append(f'<text x="{f(sx(8))}" y="{f(sy(92))}" class="xs mono t-hi" '
                f'text-anchor="middle">r = 8</text>')

    # The real scene's number, as a reference line.
    body.append(f'<line x1="{f(X0)}" y1="{f(sy(SCENE_EFFICIENCY))}" x2="{f(X1)}" '
                f'y2="{f(sy(SCENE_EFFICIENCY))}" class="ink-soft" stroke-width="1" '
                f'stroke-dasharray="2 4"/>')


    # The cost, beside it.
    CX0, CX1 = 470.0, 618.0
    body.append(f'<text x="{f(CX0)}" y="{f(Y0 - 26)}" class="xs muted">'
                f'dashed line: the Module 3</text>')
    body.append(f'<text x="{f(CX0)}" y="{f(Y0 - 14)}" class="xs muted">'
                f'scene, at {SCENE_EFFICIENCY}%</text>')
    body.append(f'<text x="{f(CX0)}" y="{f(Y0 + 12)}" class="sm">what it costs</text>')
    body.append(f'<text x="{f(CX0)}" y="{f(Y0 + 28)}" class="xs muted">'
                f'quad vs scanline, real scene</text>')
    ry = Y0 + 48
    body.append(f'<text x="{f(CX0)}" y="{f(ry)}" class="xs mono muted">tris</text>')
    body.append(f'<text x="{f(CX1)}" y="{f(ry)}" class="xs mono muted" text-anchor="end">slower by</text>')
    ry += 15
    for seg, tris, eff, _, _, meas, pred in HELPER_COST:
        body.append(f'<text x="{f(CX0)}" y="{f(ry)}" class="xs mono">{tris:,}</text>')
        body.append(f'<text x="{f(CX1)}" y="{f(ry)}" class="xs mono" text-anchor="end">'
                    f'{meas:.2f}&#215;</text>')
        ry += 15
    body.append(f'<text x="{f(CX0)}" y="{f(ry + 10)}" class="xs muted">and 1/efficiency</text>')
    body.append(f'<text x="{f(CX0)}" y="{f(ry + 24)}" class="xs muted">predicts every row</text>')
    body.append(f'<text x="{f(CX0)}" y="{f(ry + 38)}" class="xs muted">to within 0.09&#215;.</text>')

    return svg(640, 300,
               "Lane efficiency against triangle size, for four block widths",
               "Four curves of lane efficiency against triangle circumradius, on a log axis "
               "running from 64 pixels down to 2. The 2 by 2 curve falls from 96 per cent to "
               "50 per cent; the 16 by 16 curve falls from 61 per cent to 1 per cent. At a "
               "circumradius of 8 pixels the two differ by 75.5 against 8.9 per cent. A "
               "dashed reference line marks the Module 3 scene at 87.4 per cent, and a table "
               "beside the chart gives the measured slowdown of the quad traversal against "
               "the scanline one, from 1.04 to 1.38 times.",
               "\n".join("  " + s for s in body), "f412")


# =============================================================================
# Figure 3 — divergence
# =============================================================================
# ONE CLAIM: a warp in which lanes disagree executes BOTH sides, and the measured
# penalty steps up exactly where the run length crosses the warp width. A reader
# must be able to find 32 on the x axis and see the step there.
def fig3():
    body = []

    # ---- Top: one warp, both sides, masked -----------------------------------
    LANE = 13.5
    X0, Y0 = 96.0, 46.0
    body.append('<text x="16" y="24" class="sm muted">'
                'one warp of 32 lanes, executing <tspan class="mono">if (c) A(); else B();</tspan></text>')

    sides = [(i // 5) % 2 == 0 for i in range(WARP)]

    rows = [("lanes", None), ("A()", True), ("B()", False)]
    for ri, (label, want) in enumerate(rows):
        y = Y0 + ri * 26.0
        body.append(f'<text x="{f(X0 - 8)}" y="{f(y + 11)}" class="xs mono" '
                    f'text-anchor="end">{label}</text>')
        for i in range(WARP):
            x = X0 + i * LANE
            if want is None:
                col = COVERED if sides[i] else HELPER
                body.append(f'<rect x="{f(x)}" y="{f(y)}" width="{f(LANE - 1.5)}" height="15" '
                            f'fill="{col}" opacity="0.9"/>')
            else:
                active = (sides[i] == want)
                body.append(f'<rect x="{f(x)}" y="{f(y)}" width="{f(LANE - 1.5)}" height="15" '
                            f'fill="{COVERED if want else HELPER}" '
                            f'opacity="{0.9 if active else 0.16}"/>')
        if want is not None:
            body.append(f'<text x="{f(X0 + WARP * LANE + 8)}" y="{f(y + 11)}" class="xs muted">'
                        f'all 32 run, {sum(1 for i in range(WARP) if sides[i] == want)} kept</text>')

    body.append(f'<text x="16" y="{f(Y0 + 3 * 26 + 16)}" class="sm">'
                f'Both rows execute. The pale lanes are <tspan class="mono">masked</tspan> '
                f'&#8212; computed, then discarded &#8212; so the warp pays for A AND B.</text>')

    # ---- Bottom: the measured curve ------------------------------------------
    CX0, CX1 = 90.0, 470.0
    CY0, CY1 = 176.0, 286.0
    xs = [d[0] for d in DIVERGENCE]

    def sx(v):
        return CX0 + (math.log2(v) - math.log2(min(xs))) / \
               (math.log2(max(xs)) - math.log2(min(xs))) * (CX1 - CX0)

    def sy(v):
        return CY1 - (v - 1.0) / 1.1 * (CY1 - CY0)

    body.append(f'<text x="16" y="{f(CY0 - 12)}" class="sm muted">'
                f'measured cost against a warp that never diverges</text>')
    body.append(f'<rect x="{f(CX0)}" y="{f(CY0)}" width="{f(CX1 - CX0)}" '
                f'height="{f(CY1 - CY0)}" class="fill-soft grid" stroke-width="1"/>')
    for v in (1.0, 1.25, 1.5, 1.75, 2.0):
        body.append(f'<line x1="{f(CX0)}" y1="{f(sy(v))}" x2="{f(CX1)}" y2="{f(sy(v))}" '
                    f'class="grid" stroke-width="0.8"/>')
        body.append(f'<text x="{f(CX0 - 6)}" y="{f(sy(v) + 3)}" class="xs mono muted" '
                    f'text-anchor="end">{v:.2f}&#215;</text>')
    for v in xs:
        body.append(f'<text x="{f(sx(v))}" y="{f(CY1 + 15)}" class="xs mono muted" '
                    f'text-anchor="middle">{v}</text>')
    body.append(f'<text x="{f((CX0 + CX1) / 2)}" y="{f(CY1 + 32)}" class="xs muted" '
                f'text-anchor="middle">coherence: pixels in a row taking the same side</text>')

    pts = " ".join(f"{f(sx(d[0]))},{f(sy(d[4]))}" for d in DIVERGENCE)
    body.append(f'<polyline points="{pts}" fill="none" stroke="var(--dia-hi)" stroke-width="2.4"/>')
    for d in DIVERGENCE:
        body.append(f'<circle cx="{f(sx(d[0]))}" cy="{f(sy(d[4]))}" r="3" fill="var(--dia-hi)"/>')

    # The warp width, which is where the step happens.
    body.append(f'<line x1="{f(sx(WARP))}" y1="{f(CY0)}" x2="{f(sx(WARP))}" y2="{f(CY1)}" '
                f'class="ink" stroke-width="1.4" stroke-dasharray="3 3"/>')
    body.append(f'<text x="{f(sx(WARP) + 6)}" y="{f(CY0 + 14)}" class="xs mono">'
                f'the warp width, 32</text>')

    body.append(f'<text x="{f(CX1 + 14)}" y="{f(CY0 + 26)}" class="xs">'
                f'Above 32, most warps</text>')
    body.append(f'<text x="{f(CX1 + 14)}" y="{f(CY0 + 40)}" class="xs">'
                f'are uniform: 1.00&#215;.</text>')
    body.append(f'<text x="{f(CX1 + 14)}" y="{f(CY0 + 62)}" class="xs">'
                f'Below it, every warp</text>')
    body.append(f'<text x="{f(CX1 + 14)}" y="{f(CY0 + 76)}" class="xs">'
                f'is mixed: 1.93&#215;.</text>')

    return svg(640, 328,
               "Divergence in a warp, and the measured cost of it",
               "The top shows one warp of 32 lanes running an if-else. Both the A row and the "
               "B row execute across all 32 lanes, with the lanes that do not take that side "
               "drawn pale to mean masked off. Beneath, a chart of measured cost against "
               "coherence: the penalty is 1.00 times when runs are 1024 pixels long, 1.03 at "
               "64, and steps up to 1.93 at 8 pixels and below. A dashed line marks the warp "
               "width of 32, exactly where the step happens.",
               "\n".join("  " + s for s in body), "f413")


# =============================================================================
# Figure 4 — latency hiding
# =============================================================================
# ONE CLAIM: the work per step is identical in every row; only the number of
# independent chains changes, and the throughput rises 27x. A reader must be able
# to see the idle gaps in the top timeline disappear in the bottom one.
def fig4():
    body = []
    X0 = 128.0
    UNIT = 17.0

    body.append('<text x="16" y="24" class="sm muted">'
                'one dependent chain: each step needs the previous step&#8217;s result</text>')

    # A timeline: work, then a wait for the result, then work.
    y = 42.0
    for step in range(5):
        x = X0 + step * UNIT * 4
        body.append(f'<rect x="{f(x)}" y="{f(y)}" width="{f(UNIT)}" height="16" '
                    f'fill="{COVERED}"/>')
        body.append(f'<rect x="{f(x + UNIT)}" y="{f(y)}" width="{f(UNIT * 3)}" height="16" '
                    f'fill="none" class="grid" stroke-width="1" stroke-dasharray="2 2"/>')
    body.append(f'<text x="{f(X0 - 8)}" y="{f(y + 12)}" class="xs mono" '
                f'text-anchor="end">chain 1</text>')
    body.append(f'<text x="{f(X0 + 5 * UNIT * 4 + 10)}" y="{f(y + 12)}" class="xs muted">'
                f'dashed = waiting</text>')

    body.append(f'<text x="16" y="{f(y + 44)}" class="sm muted">'
                f'four independent chains, interleaved &#8212; the same work, none of the waiting</text>')
    y2 = y + 60
    for c in range(4):
        for step in range(5):
            x = X0 + step * UNIT * 4 + c * UNIT
            body.append(f'<rect x="{f(x)}" y="{f(y2 + c * 18)}" width="{f(UNIT - 1)}" '
                        f'height="16" fill="{COVERED}" opacity="{0.95 - 0.13 * c}"/>')
        body.append(f'<text x="{f(X0 - 8)}" y="{f(y2 + c * 18 + 12)}" class="xs mono" '
                    f'text-anchor="end">chain {c + 1}</text>')

    # The measured table, as bars.
    BY = y2 + 4 * 18 + 38
    BX0, BX1 = 128.0, 470.0
    body.append(f'<text x="16" y="{f(BY - 14)}" class="sm muted">'
                f'measured throughput, same arithmetic in every row</text>')
    top = max(l[2] for l in LATENCY)
    for i, (chains, ns, speed) in enumerate(LATENCY):
        ry = BY + i * 17
        w = (BX1 - BX0) * speed / top
        body.append(f'<text x="{f(BX0 - 8)}" y="{f(ry + 10)}" class="xs mono" '
                    f'text-anchor="end">{chains}</text>')
        body.append(f'<rect x="{f(BX0)}" y="{f(ry)}" width="{f(w)}" height="12" '
                    f'fill="{COVERED}"/>')
        body.append(f'<text x="{f(BX0 + w + 6)}" y="{f(ry + 10)}" class="xs mono">'
                    f'{speed:.2f}&#215;  ({ns:.3f} ns/step)</text>')
    body.append(f'<text x="{f(BX0 - 8)}" y="{f(BY - 2)}" class="xs muted" '
                f'text-anchor="end">chains</text>')

    return svg(640, int(BY + len(LATENCY) * 17 + 18),
               "Latency hiding: the same work, with and without something else to do",
               "A timeline of one dependent chain shows short bars of work separated by long "
               "dashed gaps of waiting. Beneath it, four independent chains interleaved fill "
               "those gaps completely with the same total work. A bar chart of the measured "
               "result follows: one chain runs at 3.124 nanoseconds per step and 32 chains at "
               "0.112, a speedup of 27.88 times, with no change to the arithmetic.",
               "\n".join("  " + s for s in body), "f414")


# =============================================================================
# Figure 5 — the pipeline, mapped to what we already built
# =============================================================================
# ONE CLAIM: every stage of a GPU pipeline is a thing this course already wrote.
# A reader must be able to follow one row across and find the file it lives in.
def fig5():
    ROWS = [
        ("vertex shader", "collect_triangles()", "model &#8594; clip, per vertex", "2.8 &#8211; 2.10"),
        ("primitive assembly", "index triples", "three vertices become a triangle", "2.12"),
        ("clipping", "clip_polygon_near()", "against the near plane", "3.3"),
        ("perspective divide", "&#247; w", "clip &#8594; NDC", "2.10"),
        ("viewport transform", "viewport::to_screen()", "NDC &#8594; pixels, and the y-flip", "2.11"),
        ("face culling", "is_front_facing()", "the sign of the signed area", "3.4"),
        ("rasterization", "edge functions + quads", "which lanes are covered", "2.2, 4.1"),
        ("interpolation", "barycentric &#215; 1/w", "the varyings arrive", "2.4, 3.2"),
        ("depth test", "depth_buffer", "an attachment, not a branch", "3.1"),
        ("fragment shader", "the fragment lambda", "colour from varyings", "3.6 &#8211; 3.9"),
        ("blend / write", "row[x] = colour", "the only write there is", "1.5"),
    ]
    body = []
    X0, X1, X2, X3 = 16.0, 172.0, 348.0, 566.0
    Y0 = 46.0
    RH = 22.0

    body.append(f'<text x="{f(X0)}" y="26" class="sm muted">GPU pipeline stage</text>')
    body.append(f'<text x="{f(X1)}" y="26" class="sm muted">what you already wrote</text>')
    body.append(f'<text x="{f(X2)}" y="26" class="sm muted">doing what</text>')
    body.append(f'<text x="{f(X3)}" y="26" class="sm muted">lesson</text>')
    body.append(f'<line x1="{f(X0)}" y1="32" x2="624" y2="32" class="ink-soft" stroke-width="1"/>')

    for i, (stage, ours, what, lesson) in enumerate(ROWS):
        y = Y0 + i * RH
        if i % 2 == 1:
            body.append(f'<rect x="{f(X0 - 4)}" y="{f(y - 14)}" width="628" height="{f(RH)}" '
                        f'class="fill-soft" stroke="none"/>')
        body.append(f'<text x="{f(X0)}" y="{f(y)}" class="xs">{stage}</text>')
        body.append(f'<text x="{f(X1)}" y="{f(y)}" class="xs mono t-hi">{ours}</text>')
        body.append(f'<text x="{f(X2)}" y="{f(y)}" class="xs muted">{what}</text>')
        body.append(f'<text x="{f(X3)}" y="{f(y)}" class="xs mono muted">{lesson}</text>')

    body.append(f'<text x="{f(X0)}" y="{f(Y0 + len(ROWS) * RH + 12)}" class="sm">'
                f'Eleven stages. <tspan class="t-hi">You have written all eleven.</tspan> '
                f'Module 4 does not teach you a pipeline &#8212; it hands yours to hardware.</text>')

    return svg(640, int(Y0 + len(ROWS) * RH + 28),
               "The GPU pipeline, stage by stage, against the code this course already wrote",
               "A four column table listing eleven GPU pipeline stages - vertex shader, "
               "primitive assembly, clipping, perspective divide, viewport transform, face "
               "culling, rasterization, interpolation, depth test, fragment shader and blend "
               "- each paired with the function in this engine that already performs it and "
               "the lesson that built it. Every stage has an entry.",
               "\n".join("  " + s for s in body), "f415")


# =============================================================================
# Figure 6 — fill_style is a pipeline object
# =============================================================================
# ONE CLAIM: the two structs are the same size at the top level (10 and 9). A
# reader must be able to count both columns.
def fig6():
    OURS = [("interpolation", "interp"), ("shading", "shade"), ("blend_space", "space"),
            ("cull_mode", "cull"), ("const lighting*", "lights"), ("specular", "surface"),
            ("specular_model", "model"), ("vec3", "eye"), ("texture_binding", "albedo"),
            ("encode_mode", "encode")]
    SDL = [("SDL_GPUShader*", "vertex_shader"), ("SDL_GPUShader*", "fragment_shader"),
           ("…VertexInputState", "vertex_input_state"), ("…PrimitiveType", "primitive_type"),
           ("…RasterizerState", "rasterizer_state"), ("…MultisampleState", "multisample_state"),
           ("…DepthStencilState", "depth_stencil_state"), ("…PipelineTargetInfo", "target_info"),
           ("SDL_PropertiesID", "props")]
    body = []
    LX, RX = 16.0, 330.0
    Y0 = 62.0
    RH = 19.0

    body.append(f'<text x="{f(LX)}" y="26" class="sm">engine::fill_style</text>')
    body.append(f'<text x="{f(LX)}" y="42" class="xs muted">src/gfx/raster.hpp, Lessons 3.2 &#8211; 4.1</text>')
    body.append(f'<text x="{f(RX)}" y="26" class="sm">SDL_GPUGraphicsPipelineCreateInfo</text>')
    body.append(f'<text x="{f(RX)}" y="42" class="xs muted">SDL3/SDL_gpu.h, release-3.4.12</text>')

    for i, (t, n) in enumerate(OURS):
        y = Y0 + i * RH
        body.append(f'<rect x="{f(LX - 4)}" y="{f(y - 13)}" width="292" height="{f(RH - 2)}" '
                    f'fill="{COVERED}" opacity="0.14"/>')
        body.append(f'<text x="{f(LX)}" y="{f(y)}" class="xs mono muted">{t}</text>')
        body.append(f'<text x="{f(LX + 168)}" y="{f(y)}" class="xs mono">{n}</text>')

    for i, (t, n) in enumerate(SDL):
        y = Y0 + i * RH
        body.append(f'<rect x="{f(RX - 4)}" y="{f(y - 13)}" width="298" height="{f(RH - 2)}" '
                    f'fill="{HELPER}" opacity="0.14"/>')
        body.append(f'<text x="{f(RX)}" y="{f(y)}" class="xs mono muted">{t}</text>')
        body.append(f'<text x="{f(RX + 152)}" y="{f(y)}" class="xs mono">{n}</text>')

    ty = Y0 + max(len(OURS), len(SDL)) * RH + 8
    body.append(f'<text x="{f(LX)}" y="{f(ty)}" class="xs mono t-hi">'
                f'{OURS_TOP} fields</text>')
    body.append(f'<text x="{f(RX)}" y="{f(ty)}" class="xs mono t-hi">'
                f'{SDL_TOP} fields ({SDL_LEAVES} once the nested state is expanded)</text>')

    body.append(f'<text x="{f(LX)}" y="{f(ty + 26)}" class="sm">'
                f'We did not approximate a pipeline object. '
                f'<tspan class="t-hi">We built one</tspan>, and it is the same size.</text>')
    body.append(f'<text x="{f(LX)}" y="{f(ty + 44)}" class="sm muted">'
                f'Every field on the left has a home on the right: '
                f'<tspan class="mono">cull</tspan> in rasterizer_state, '
                f'<tspan class="mono">shade</tspan> in fragment_shader,</text>')
    body.append(f'<text x="{f(LX)}" y="{f(ty + 60)}" class="sm muted">'
                f'<tspan class="mono">albedo</tspan> split between a binding and a sampler. '
                f'The port is a rename, and Lesson 4.8 is where it happens.</text>')

    return svg(640, int(ty + 76),
               "fill_style beside SDL_GPUGraphicsPipelineCreateInfo",
               "Two field lists side by side. On the left engine fill_style with ten fields: "
               "interp, shade, space, cull, lights, surface, model, eye, albedo and encode. On "
               "the right SDL_GPUGraphicsPipelineCreateInfo with nine: vertex_shader, "
               "fragment_shader, vertex_input_state, primitive_type, rasterizer_state, "
               "multisample_state, depth_stencil_state, target_info and props, which expand to "
               "53 leaf fields. The two structs are the same size at the top level.",
               "\n".join("  " + s for s in body), "f416")


def main():
    print("Lesson 4.1 figures")
    # RENUMBERED 2026-09-11 to match page order — see build_41.py. The
    # fig<N>() names are the ORIGINAL draft order and are left alone; what
    # matters is which FILE each one writes, because that is what the page
    # embeds. Content named per line so the pairing can be checked by eye.
    write("l41_fig2.svg", fig1())   # the 2x2 quad
    write("l41_fig4.svg", fig2())   # lane efficiency
    write("l41_fig3.svg", fig3())   # warp divergence
    write("l41_fig1.svg", fig4())   # latency and occupancy
    write("l41_fig5.svg", fig5())   # the pipeline
    write("l41_fig6.svg", fig6())   # state as an object


if __name__ == "__main__":
    main()
