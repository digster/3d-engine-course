#!/usr/bin/env python3
"""Compute every SVG figure for Lesson 3.9 from real data.

Same discipline as 3.7's and 3.8's: nothing is placed by eye, and every number
drawn here is a number `scratch/verify_39.log` printed. Each figure's docstring
states the ONE claim a reader must be able to extract by MEASURING the picture —
the rule 3.8 arrived at after a figure passed every automated check while burying
its own point.

Writes scratch/l39_fig{1..6}.svg.
"""
import math

# ---- Measured inputs, from scratch/verify_39.log ----------------------------
# Kept here as data rather than retyped inside an SVG string, so a figure and the
# log it came from cannot drift apart.

# §H: the traced texture footprint down the screen, in texels per pixel.
FOOTPRINT = [(179, 0.60), (140, 1.02), (120, 1.87), (100, 4.45),
             (90, 8.38), (80, 21.30), (73, 62.46)]

# §H: pixels that change under a sub-pixel camera nudge, by texture fineness.
SPARKLE = [(4, 8.0), (8, 16.2), (16, 32.6), (32, 64.8)]

# §B: wrap_texel over n = 4.
WRAP_I = list(range(-5, 9))
WRAP = {
    "repeat":   [3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0],
    "mirrored": [3, 3, 2, 1, 0, 0, 1, 2, 3, 3, 2, 1, 0, 0],
    "clamp":    [0, 0, 0, 0, 0, 0, 1, 2, 3, 3, 3, 3, 3, 3],
}


def on_swatch(hexcol):
    """Which text class is legible on this swatch.

    `.t-inv` is fixed white and `.t-onlight` fixed dark, both deliberately
    theme-independent for the same reason course.css gives for `.t-inv`: the
    shape underneath is the same saturated colour in both themes, so the label
    must not follow the theme's ink. Chosen by relative luminance rather than by
    eye — the pale swatches (#e2ded2, #c2bdae) carried white numerals in the
    first draft and were barely readable.
    """
    r = int(hexcol[1:3], 16) / 255.0
    g = int(hexcol[3:5], 16) / 255.0
    b = int(hexcol[5:7], 16) / 255.0
    lum = 0.2126 * r + 0.7152 * g + 0.0722 * b
    return "t-onlight" if lum > 0.45 else "t-inv"


def f(v):
    return f"{v:.1f}"


def f2(v):
    return f"{v:.2f}"


# Marker ids are NAMESPACED PER FIGURE (`e-i-f391`, …) rather than global.
# Six figures on one page declaring the same four ids is invalid HTML, and
# `url(#e-i)` resolves to the FIRST match — so every figure would quietly use
# figure 1's markers. Byte-identical definitions make that harmless today and
# a silent, baffling bug the first time one figure wants a different arrowhead.
def defs(tid):
    return f"""  <defs>
    <marker id="e-i-{tid}" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6"
            orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="var(--dia-ink)"/></marker>
    <marker id="e-s-{tid}" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6"
            orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="var(--dia-ink-soft)"/></marker>
    <marker id="e-h-{tid}" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6"
            orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="var(--dia-hi)"/></marker>
    <marker id="e-b-{tid}" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6"
            orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="var(--verify-bd)"/></marker>
  </defs>
"""


def svg(w, h, title, desc, body, tid):
    for m in ("e-i", "e-s", "e-h", "e-b"):
        body = body.replace(f"url(#{m})", f"url(#{m}-{tid})")
    return (f'<svg viewBox="0 0 {w} {h}" role="img" aria-labelledby="{tid}-t {tid}-d">\n'
            f'  <title id="{tid}-t">{title}</title>\n'
            f'  <desc id="{tid}-d">{desc}</desc>\n{defs(tid)}{body}\n</svg>\n')


def write(name, text):
    with open(f"scratch/{name}", "w") as fh:
        fh.write(text)
    print(f"  wrote scratch/{name}")


# =============================================================================
# Figure 1 — a texel is a sample, not a square
# =============================================================================
# ONE CLAIM: four texels do not divide [0,1] into four pieces with values at the
# joins. They are four SAMPLES, at 1/8, 3/8, 5/8, 7/8 — and the reader must be
# able to measure those four positions off the axis and see they are NOT at
# 0, 1/4, 1/2, 3/4.
def fig1():
    N = 4
    X0, X1 = 96.0, 560.0
    span = X1 - X0

    def ux(u):
        return X0 + u * span

    # The four texel values, chosen so the reconstruction has something to say.
    vals = [0.18, 0.86, 0.42, 0.66]
    shades = ["#4a5570", "#e2ded2", "#8d93a6", "#c2bdae"]

    body = []

    # ---- Top band: the "little squares" mental model -------------------------
    ty = 36.0
    body.append('<text x="16" y="26" class="sm muted">the mental picture: four little squares</text>')
    for i in range(N):
        x = ux(i / N)
        w = span / N
        body.append(f'<rect x="{f(x)}" y="{f(ty)}" width="{f(w)}" height="26" '
                    f'fill="{shades[i]}" stroke="var(--dia-ink-soft)" stroke-width="0.8"/>')
        body.append(f'<text x="{f(x + w / 2)}" y="{f(ty + 18)}" '
                    f'class="xs mono {on_swatch(shades[i])}" '
                    f'text-anchor="middle">{i}</text>')

    # ---- Bottom band: the samples -------------------------------------------
    by = 180.0          # baseline of the value plot
    bh = 68.0           # its height
    body.append('<text x="16" y="94" class="sm muted">what the numbers actually are: four samples</text>')

    # axis
    body.append(f'<line x1="{f(X0 - 8)}" y1="{f(by)}" x2="{f(X1 + 26)}" y2="{f(by)}" '
                f'class="ink" stroke-width="1.1" marker-end="url(#e-i)"/>')
    for u, lab in [(0.0, "0"), (0.25, "&#188;"), (0.5, "&#189;"), (0.75, "&#190;"), (1.0, "1")]:
        x = ux(u)
        body.append(f'<line x1="{f(x)}" y1="{f(by - 4)}" x2="{f(x)}" y2="{f(by + 4)}" '
                    f'class="ink-soft" stroke-width="0.9"/>')
        body.append(f'<text x="{f(x)}" y="{f(by + 18)}" class="xs mono muted" '
                    f'text-anchor="middle">{lab}</text>')
    body.append(f'<text x="{f(X1 + 32)}" y="{f(by + 4)}" class="sm mono">u</text>')

    # the samples, as stems with dots at (i + 0.5)/N
    pts = []
    for i in range(N):
        u = (i + 0.5) / N
        x = ux(u)
        y = by - vals[i] * bh
        pts.append((x, y))
        body.append(f'<line x1="{f(x)}" y1="{f(by)}" x2="{f(x)}" y2="{f(y)}" '
                    f'class="hi" stroke-width="1.4"/>')
        body.append(f'<circle cx="{f(x)}" cy="{f(y)}" r="4" fill="var(--dia-hi)"/>')
        body.append(f'<text x="{f(x)}" y="{f(y - 11)}" class="xs mono t-hi" '
                    f'text-anchor="middle">{(2 * i + 1)}/8</text>')

    # the piecewise-linear reconstruction between sample points: THIS is bilinear
    # in one dimension, and it is only defined between the first and last centre.
    path = " ".join(f"{'M' if k == 0 else 'L'}{f(px)},{f(py)}" for k, (px, py) in enumerate(pts))
    body.append(f'<path d="{path}" fill="none" class="ink" stroke-width="1.6" '
                f'stroke-dasharray="none"/>')

    # what happens outside the outer centres — the addressing question, flagged
    # below the axis, where there is nothing to collide with.
    for x in (X0, X1):
        body.append(f'<line x1="{f(x)}" y1="{f(by + 24)}" x2="{f(x)}" y2="{f(by + 36)}" '
                    f'class="ink-soft" stroke-width="0.8" stroke-dasharray="3 3"/>')
    body.append(f'<text x="16" y="{f(by + 52)}" class="xs muted">'
                f'past the first and last centre there are no samples at all '
                f'&#8212; which is what an address mode decides</text>')

    return write("l39_fig1.svg", svg(600, 244,
        "A texel is a sample, not a square",
        "Above, four coloured squares dividing the unit interval. Below, the same four "
        "numbers drawn as samples at u = 1/8, 3/8, 5/8 and 7/8, joined by straight "
        "segments. The sample positions are the odd eighths, not the quarters, and the "
        "reconstruction is only defined between the first and last of them.",
        "\n".join(body), "f391"))


# =============================================================================
# Figure 2 — the half texel, and the shift it costs
# =============================================================================
# ONE CLAIM: the corner variant's ramp crosses half brightness half a texel
# early. The reader must be able to lay a ruler on the two crossings and read a
# gap of exactly half a texel cell.
def fig2():
    M = 8
    X0, X1 = 78.0, 546.0
    span = X1 - X0
    TOP, BOT = 60.0, 148.0          # brightness 1 and 0

    def ux(u):
        return X0 + u * span

    def uy(v):
        return BOT - v * (BOT - TOP)

    body = []

    # The texels along the top, as the image really is: 4 white then 4 black.
    ty = 24.0
    for i in range(M):
        x = ux(i / M)
        w = span / M
        c = "#f2efe8" if i < M // 2 else "#2a2c33"
        body.append(f'<rect x="{f(x)}" y="{f(ty)}" width="{f(w)}" height="20" '
                    f'fill="{c}" stroke="var(--dia-ink-soft)" stroke-width="0.7"/>')
    body.append(f'<text x="{f(X0 - 8)}" y="{f(ty + 14)}" class="xs mono muted" '
                f'text-anchor="end">image</text>')

    # texel centres, ticked on the plot's axis
    body.append(f'<line x1="{f(X0)}" y1="{f(BOT)}" x2="{f(X1 + 22)}" y2="{f(BOT)}" '
                f'class="ink" stroke-width="1.1" marker-end="url(#e-i)"/>')
    for i in range(M):
        x = ux((i + 0.5) / M)
        body.append(f'<line x1="{f(x)}" y1="{f(BOT - 3)}" x2="{f(x)}" y2="{f(BOT + 3)}" '
                    f'class="ink-soft" stroke-width="0.9"/>')
    body.append(f'<text x="{f(X1 + 28)}" y="{f(BOT + 4)}" class="sm mono">u</text>')

    # brightness gridline at 0.5
    body.append(f'<line x1="{f(X0)}" y1="{f(uy(0.5))}" x2="{f(X1)}" y2="{f(uy(0.5))}" '
                f'class="grid" stroke-width="1"/>')
    body.append(f'<text x="{f(X0 - 8)}" y="{f(uy(0.5) + 4)}" class="xs mono muted" '
                f'text-anchor="end">&#189;</text>')
    body.append(f'<text x="{f(X0 - 8)}" y="{f(uy(1.0) + 4)}" class="xs mono muted" '
                f'text-anchor="end">1</text>')
    body.append(f'<text x="{f(X0 - 8)}" y="{f(uy(0.0) + 4)}" class="xs mono muted" '
                f'text-anchor="end">0</text>')

    def ramp(half):
        """The bilinear profile of the step image, sampled finely."""
        pts = []
        for k in range(0, 481):
            u = k / 480.0
            x = u * M - half
            i0 = math.floor(x)
            t = x - i0
            def tex(i):
                i = max(0, min(M - 1, i))       # clamp addressing
                return 1.0 if i < M // 2 else 0.0
            val = tex(i0) * (1 - t) + tex(i0 + 1) * t
            pts.append((ux(u), uy(val)))
        return pts

    for half, cls, marker, dash in [(0.5, "ink", "e-i", "none"), (0.0, "hi", "e-h", "5 3")]:
        pts = ramp(half)
        path = " ".join(f"{'M' if k == 0 else 'L'}{f(px)},{f(py)}"
                        for k, (px, py) in enumerate(pts))
        body.append(f'<path d="{path}" fill="none" class="{cls}" stroke-width="1.8" '
                    f'stroke-dasharray="{dash}"/>')

    # the two crossings, measured by the harness: u = 0.5 and u = 0.4375
    for u, cls, tcls, lab, dy in [(0.5, "ink", "", "centre: u = 0.500", -1),
                                  (0.4375, "hi", "t-hi", "corner: u = 0.4375", 1)]:
        x = ux(u)
        body.append(f'<circle cx="{f(x)}" cy="{f(uy(0.5))}" r="3.6" '
                    f'fill="var(--dia-{"ink" if cls == "ink" else "hi"})"/>')

    body.append(f'<text x="{f(ux(0.5) + 8)}" y="{f(uy(0.5) - 10)}" class="xs mono">'
                f'centre  u = 0.5000</text>')
    body.append(f'<text x="{f(ux(0.4375) - 8)}" y="{f(uy(0.5) + 22)}" class="xs mono t-hi" '
                f'text-anchor="end">corner  u = 0.4375</text>')

    # the measured gap
    gy = 176.0
    body.append(f'<line x1="{f(ux(0.4375))}" y1="{f(gy)}" x2="{f(ux(0.5))}" y2="{f(gy)}" '
                f'class="ink" stroke-width="1.2" marker-start="url(#e-i)" '
                f'marker-end="url(#e-i)"/>')
    body.append(f'<line x1="{f(ux(0.4375))}" y1="{f(uy(0.5))}" x2="{f(ux(0.4375))}" '
                f'y2="{f(gy)}" class="ink-soft" stroke-width="0.7" stroke-dasharray="2 3"/>')
    body.append(f'<line x1="{f(ux(0.5))}" y1="{f(uy(0.5))}" x2="{f(ux(0.5))}" '
                f'y2="{f(gy)}" class="ink-soft" stroke-width="0.7" stroke-dasharray="2 3"/>')
    body.append(f'<text x="{f(ux(0.47))}" y="{f(gy + 18)}" class="xs mono" '
                f'text-anchor="middle">0.0625 = half a texel</text>')

    return write("l39_fig2.svg", svg(600, 200,
        "The half-texel offset, and the shift it causes",
        "A step image of eight texels, four white then four black, sampled bilinearly "
        "two ways. The solid curve subtracts half a texel and crosses half brightness at "
        "u = 0.5, exactly on the boundary between the two blocks. The dashed curve omits "
        "the half and crosses at u = 0.4375. The gap between the crossings is 0.0625 of "
        "the image, which is half of one texel.",
        "\n".join(body), "f392"))


# =============================================================================
# Figure 3 — bilinear as a double lerp
# =============================================================================
# ONE CLAIM: two lerps along u, then one along v — and the four resulting weights
# sum to 1. The reader must be able to add the four printed weights and get 1.
def fig3():
    X0, Y0 = 132.0, 62.0
    S = 150.0                       # spacing between texel centres

    tu, tv = 0.62, 0.35             # the sample point, as fractions
    px = X0 + tu * S
    py = Y0 + tv * S

    cols = ["#4a5570", "#e2ded2", "#c2bdae", "#8d93a6"]
    names = ["c00", "c10", "c01", "c11"]
    pos = [(X0, Y0), (X0 + S, Y0), (X0, Y0 + S), (X0 + S, Y0 + S)]

    w = [(1 - tu) * (1 - tv), tu * (1 - tv), (1 - tu) * tv, tu * tv]

    body = []

    # the square joining the four centres
    body.append(f'<rect x="{f(X0)}" y="{f(Y0)}" width="{f(S)}" height="{f(S)}" '
                f'class="fill-soft" stroke="var(--dia-grid)" stroke-width="1"/>')

    # the two u-lerps
    top_x, top_y = X0 + tu * S, Y0
    bot_x, bot_y = X0 + tu * S, Y0 + S
    body.append(f'<line x1="{f(X0)}" y1="{f(Y0)}" x2="{f(X0 + S)}" y2="{f(Y0)}" '
                f'class="ink-soft" stroke-width="1.4"/>')
    body.append(f'<line x1="{f(X0)}" y1="{f(Y0 + S)}" x2="{f(X0 + S)}" y2="{f(Y0 + S)}" '
                f'class="ink-soft" stroke-width="1.4"/>')
    # ...then the v-lerp between their results
    body.append(f'<line x1="{f(top_x)}" y1="{f(top_y)}" x2="{f(bot_x)}" y2="{f(bot_y)}" '
                f'class="hi" stroke-width="1.8"/>')

    for (cx, cy), c, nm, wt in zip(pos, cols, names, w):
        body.append(f'<circle cx="{f(cx)}" cy="{f(cy)}" r="7" fill="{c}" '
                    f'stroke="var(--dia-ink)" stroke-width="1"/>')

    # labels, pushed away from the square so nothing sits on a stroke
    body.append(f'<text x="{f(X0 - 12)}" y="{f(Y0 - 12)}" class="xs mono" '
                f'text-anchor="end">c00  w = {w[0]:.4f}</text>')
    body.append(f'<text x="{f(X0 + S + 12)}" y="{f(Y0 - 12)}" class="xs mono">'
                f'c10  w = {w[1]:.4f}</text>')
    body.append(f'<text x="{f(X0 - 12)}" y="{f(Y0 + S + 22)}" class="xs mono" '
                f'text-anchor="end">c01  w = {w[2]:.4f}</text>')
    body.append(f'<text x="{f(X0 + S + 12)}" y="{f(Y0 + S + 22)}" class="xs mono">'
                f'c11  w = {w[3]:.4f}</text>')

    # the two intermediate results
    body.append(f'<circle cx="{f(top_x)}" cy="{f(top_y)}" r="4.5" fill="var(--dia-ink-soft)"/>')
    body.append(f'<circle cx="{f(bot_x)}" cy="{f(bot_y)}" r="4.5" fill="var(--dia-ink-soft)"/>')
    body.append(f'<text x="{f(top_x + 10)}" y="{f(top_y + 16)}" class="xs mono muted">'
                f'lerp(c00, c10, tu)</text>')
    body.append(f'<text x="{f(bot_x + 10)}" y="{f(bot_y - 8)}" class="xs mono muted">'
                f'lerp(c01, c11, tu)</text>')

    # the sample point
    body.append(f'<circle cx="{f(px)}" cy="{f(py)}" r="5.5" fill="var(--dia-hi)" '
                f'stroke="var(--dia-bg)" stroke-width="1.4"/>')
    body.append(f'<text x="{f(px + 12)}" y="{f(py + 4)}" class="xs mono t-hi">'
                f'the sample</text>')

    # tu / tv measured off the square
    body.append(f'<line x1="{f(X0)}" y1="{f(Y0 + S + 40)}" x2="{f(px)}" '
                f'y2="{f(Y0 + S + 40)}" class="ink" stroke-width="1" '
                f'marker-start="url(#e-i)" marker-end="url(#e-i)"/>')
    body.append(f'<text x="{f((X0 + px) / 2)}" y="{f(Y0 + S + 56)}" class="xs mono" '
                f'text-anchor="middle">tu = {tu:.2f}</text>')
    body.append(f'<line x1="{f(X0 - 42)}" y1="{f(Y0)}" x2="{f(X0 - 42)}" y2="{f(py)}" '
                f'class="ink" stroke-width="1" marker-start="url(#e-i)" '
                f'marker-end="url(#e-i)"/>')
    body.append(f'<text x="{f(X0 - 48)}" y="{f((Y0 + py) / 2 + 4)}" class="xs mono" '
                f'text-anchor="end">tv = {tv:.2f}</text>')

    body.append(f'<text x="16" y="30" class="sm">two lerps along u, then one along v</text>')
    body.append(f'<text x="{f(X0 + S + 12)}" y="{f(Y0 + S / 2 + 26)}" class="xs mono t-hi">'
                f'&#8592; the v lerp</text>')
    total = sum(w)
    body.append(f'<text x="16" y="{f(Y0 + S + 56)}" class="xs mono muted">'
                f'sum = {total:.4f}</text>')

    return write("l39_fig3.svg", svg(600, 296,
        "Bilinear interpolation as two lerps then one",
        "Four texel centres at the corners of a square. Two interpolations along the u "
        "axis produce a point on the top edge and a point on the bottom edge; a third "
        "interpolation between those two, along v, produces the sample. The four printed "
        "weights are 0.2470, 0.4030, 0.1330 and 0.2170, and they sum to 1.",
        "\n".join(body), "f393"))


# =============================================================================
# Figure 4 — the three address modes
# =============================================================================
# ONE CLAIM: three different, useful answers to "what is texel 5 of a 4-texel
# image". The reader must be able to read the period off each row: 4 for repeat,
# 8 for mirrored, and none at all for clamp.
def fig4():
    n = 4
    X0 = 96.0
    CW = 33.0
    rows = [("repeat", 66.0), ("mirrored", 116.0), ("clamp", 166.0)]

    shades = ["#4a5570", "#e2ded2", "#8d93a6", "#c2bdae"]

    body = []
    body.append('<text x="16" y="26" class="sm">what index i maps to, for a 4-texel image</text>')

    # the i axis along the top
    for k, i in enumerate(WRAP_I):
        x = X0 + k * CW
        inside = 0 <= i < n
        cls = "xs mono" if inside else "xs mono muted"
        body.append(f'<text x="{f(x + CW / 2)}" y="46" class="{cls}" '
                    f'text-anchor="middle">{i}</text>')
    body.append(f'<text x="{f(X0 - 10)}" y="46" class="xs mono muted" '
                f'text-anchor="end">i</text>')

    # the image's own extent, marked once
    body.append(f'<rect x="{f(X0 + 5 * CW)}" y="52" width="{f(4 * CW)}" height="146" '
                f'fill="none" stroke="var(--dia-hi)" stroke-width="1.2" '
                f'stroke-dasharray="4 3"/>')
    body.append(f'<text x="{f(X0 + 7 * CW)}" y="212" class="xs mono t-hi" '
                f'text-anchor="middle">the image</text>')

    for name, y in rows:
        body.append(f'<text x="{f(X0 - 10)}" y="{f(y + 20)}" class="xs mono" '
                    f'text-anchor="end">{name}</text>')
        for k, i in enumerate(WRAP_I):
            x = X0 + k * CW
            v = WRAP[name][k]
            body.append(f'<rect x="{f(x + 1)}" y="{f(y)}" width="{f(CW - 2)}" height="30" '
                        f'fill="{shades[v]}" stroke="var(--dia-ink-soft)" stroke-width="0.7"/>')
            body.append(f'<text x="{f(x + CW / 2)}" y="{f(y + 20)}" '
                        f'class="xs mono {on_swatch(shades[v])}" '
                        f'text-anchor="middle">{v}</text>')

    return write("l39_fig4.svg", svg(600, 230,
        "The three address modes, tabulated",
        "For texel indices from -5 to 8 on a four-texel image, the index each address "
        "mode maps them to. Repeat cycles 0,1,2,3 with period four. Mirrored repeat runs "
        "0,1,2,3,3,2,1,0 with period eight, doubling the edge texel at every fold. "
        "Clamp-to-edge holds 0 to the left and 3 to the right forever.",
        "\n".join(body), "f394"))


# =============================================================================
# Figure 5 — the uv origin
# =============================================================================
# ONE CLAIM: the coordinate (0.25, 0.25) names the TOP-left area of the image in
# texture space and the BOTTOM-left area in OBJ space. The reader must be able to
# see the same number land on two different colours.
def fig5():
    S = 132.0
    quads = [("#d8484c", "TL"), ("#5fbf6a", "TR"), ("#4c7fd8", "BL"), ("#e0b24c", "BR")]

    def image(x0, y0, tag):
        out = []
        for k, (c, _) in enumerate(quads):
            qx = x0 + (k % 2) * S / 2
            qy = y0 + (k // 2) * S / 2
            out.append(f'<rect x="{f(qx)}" y="{f(qy)}" width="{f(S / 2)}" '
                       f'height="{f(S / 2)}" fill="{c}"/>')
        out.append(f'<rect x="{f(x0)}" y="{f(y0)}" width="{f(S / 8)}" height="{f(S / 8)}" '
                   f'fill="#f2efe8"/>')
        out.append(f'<rect x="{f(x0)}" y="{f(y0)}" width="{f(S)}" height="{f(S)}" '
                   f'fill="none" stroke="var(--dia-ink)" stroke-width="1.2"/>')
        return out

    body = []
    LX, RX, TY = 92.0, 372.0, 76.0

    body.append('<text x="16" y="30" class="sm">the same image, and the same number, '
                'under two conventions</text>')

    body.extend(image(LX, TY, "tex"))
    body.extend(image(RX, TY, "obj"))

    body.append(f'<text x="{f(LX + S / 2)}" y="{f(TY - 32)}" class="sm mono" '
                f'text-anchor="middle">texture space</text>')
    body.append(f'<text x="{f(LX + S / 2)}" y="{f(TY - 18)}" class="xs muted" '
                f'text-anchor="middle">SDL_GPU: v down from the top</text>')
    body.append(f'<text x="{f(RX + S / 2)}" y="{f(TY - 32)}" class="sm mono" '
                f'text-anchor="middle">OBJ space</text>')
    body.append(f'<text x="{f(RX + S / 2)}" y="{f(TY - 18)}" class="xs muted" '
                f'text-anchor="middle">Wavefront: v up from the bottom</text>')

    # the v axes: down on the left, up on the right
    body.append(f'<line x1="{f(LX - 18)}" y1="{f(TY)}" x2="{f(LX - 18)}" y2="{f(TY + S)}" '
                f'class="ax-y" stroke-width="1.5" marker-end="url(#e-i)"/>')
    body.append(f'<text x="{f(LX - 24)}" y="{f(TY + 10)}" class="xs mono lbl-y" '
                f'text-anchor="end">v=0</text>')
    body.append(f'<text x="{f(LX - 24)}" y="{f(TY + S)}" class="xs mono lbl-y" '
                f'text-anchor="end">v=1</text>')

    body.append(f'<line x1="{f(RX - 18)}" y1="{f(TY + S)}" x2="{f(RX - 18)}" y2="{f(TY)}" '
                f'class="ax-y" stroke-width="1.5" marker-end="url(#e-i)"/>')
    body.append(f'<text x="{f(RX - 24)}" y="{f(TY + S + 4)}" class="xs mono lbl-y" '
                f'text-anchor="end">v=0</text>')
    body.append(f'<text x="{f(RX - 24)}" y="{f(TY + 10)}" class="xs mono lbl-y" '
                f'text-anchor="end">v=1</text>')

    # the SAME coordinate, marked on both
    for x0, vfrac in [(LX, 0.25), (RX, 0.75)]:
        mx = x0 + 0.25 * S
        my = TY + vfrac * S
        body.append(f'<circle cx="{f(mx)}" cy="{f(my)}" r="6" fill="none" '
                    f'stroke="var(--dia-ink)" stroke-width="2"/>')
        body.append(f'<circle cx="{f(mx)}" cy="{f(my)}" r="2" fill="var(--dia-ink)"/>')

    body.append(f'<text x="{f(LX + S / 2)}" y="{f(TY + S + 24)}" class="xs mono" '
                f'text-anchor="middle">(0.25, 0.25) &#8594; red</text>')
    body.append(f'<text x="{f(RX + S / 2)}" y="{f(TY + S + 24)}" class="xs mono" '
                f'text-anchor="middle">(0.25, 0.25) &#8594; blue</text>')

    # the reconciliation
    ay = TY + S / 2
    body.append(f'<line x1="{f(LX + S + 22)}" y1="{f(ay)}" x2="{f(RX - 46)}" y2="{f(ay)}" '
                f'class="hi" stroke-width="1.6" marker-end="url(#e-h)" '
                f'marker-start="url(#e-h)"/>')
    body.append(f'<text x="{f((LX + S + RX - 24) / 2)}" y="{f(ay - 12)}" class="xs mono t-hi" '
                f'text-anchor="middle">v &#8594; 1 &#8722; v</text>')
    body.append(f'<text x="{f((LX + S + RX - 24) / 2)}" y="{f(ay + 20)}" class="xs muted" '
                f'text-anchor="middle">once, at import</text>')

    return write("l39_fig5.svg", svg(600, 258,
        "Two uv conventions, and the flip between them",
        "The same four-quadrant image drawn twice. On the left, texture space, where v "
        "increases downwards from the top as SDL_GPU specifies; the coordinate (0.25, "
        "0.25) lands in the red top-left quadrant. On the right, OBJ space, where v "
        "increases upwards from the bottom; the same coordinate lands in the blue "
        "bottom-left quadrant. An arrow between them is labelled v maps to 1 minus v, "
        "applied once at import.",
        "\n".join(body), "f395"))


# =============================================================================
# Figure 6 — minification: the footprint outruns the sampler
# =============================================================================
# ONE CLAIM: the footprint grows without bound down the page while the sampler's
# reach stays flat at 4. The reader must be able to see the two curves cross and
# then diverge — and read off WHERE they cross.
def fig6():
    X0, X1 = 82.0, 466.0
    TOP, BOT = 52.0, 190.0

    rows = [r for r, _ in FOOTPRINT][::-1]      # 73 .. 179
    vals = [v for _, v in FOOTPRINT][::-1]

    lo, hi = 0.5, 64.0

    def px(row):
        # screen row 73 (far) at the left, 179 (near) at the right
        t = (row - 73) / (179 - 73)
        return X0 + t * (X1 - X0)

    def py(v):
        t = (math.log(v) - math.log(lo)) / (math.log(hi) - math.log(lo))
        return BOT - t * (BOT - TOP)

    body = []
    body.append('<text x="16" y="28" class="sm">texels covered by one pixel, down the floor</text>')

    # axes
    body.append(f'<line x1="{f(X0)}" y1="{f(BOT)}" x2="{f(X1 + 20)}" y2="{f(BOT)}" '
                f'class="ink" stroke-width="1.1" marker-end="url(#e-i)"/>')
    body.append(f'<line x1="{f(X0)}" y1="{f(BOT)}" x2="{f(X0)}" y2="{f(TOP - 14)}" '
                f'class="ink" stroke-width="1.1" marker-end="url(#e-i)"/>')

    for v in [1, 4, 16, 64]:
        y = py(v)
        body.append(f'<line x1="{f(X0)}" y1="{f(y)}" x2="{f(X1)}" y2="{f(y)}" '
                    f'class="grid" stroke-width="1"/>')
        body.append(f'<text x="{f(X0 - 8)}" y="{f(y + 4)}" class="xs mono muted" '
                    f'text-anchor="end">{v}</text>')

    # the measured curve
    pts = [(px(r), py(v)) for r, v in zip(rows, vals)]
    path = " ".join(f"{'M' if k == 0 else 'L'}{f(a)},{f(b)}" for k, (a, b) in enumerate(pts))
    body.append(f'<path d="{path}" fill="none" class="hi" stroke-width="2"/>')
    for (a, b) in pts:
        body.append(f'<circle cx="{f(a)}" cy="{f(b)}" r="3" fill="var(--dia-hi)"/>')

    # what bilinear actually reads: 4, flat, forever
    y4 = py(4.0)
    body.append(f'<line x1="{f(X0)}" y1="{f(y4)}" x2="{f(X1)}" y2="{f(y4)}" '
                f'class="ink" stroke-width="1.8" stroke-dasharray="6 4"/>')
    body.append(f'<text x="{f(X1 - 4)}" y="{f(y4 - 9)}" class="xs mono" '
                f'text-anchor="end">bilinear reads 4, always</text>')

    # x labels
    for r, lab in [(179, "bottom of screen"), (100, "row 100"), (73, "horizon")]:
        x = px(r)
        body.append(f'<line x1="{f(x)}" y1="{f(BOT)}" x2="{f(x)}" y2="{f(BOT + 4)}" '
                    f'class="ink-soft" stroke-width="0.9"/>')
    body.append(f'<text x="{f(px(179))}" y="{f(BOT + 18)}" class="xs muted" '
                f'text-anchor="end">bottom of screen</text>')
    body.append(f'<text x="{f(px(73))}" y="{f(BOT + 18)}" class="xs muted" '
                f'text-anchor="start">horizon</text>')

    # the crossing: interpolate the measured curve for where it passes 4
    cross = None
    for k in range(len(rows) - 1):
        a, b = vals[k], vals[k + 1]
        if (a - 4.0) * (b - 4.0) <= 0 and a != b:
            t = (4.0 - a) / (b - a)
            cross = rows[k] + t * (rows[k + 1] - rows[k])
            break
    if cross is not None:
        cx = px(cross)
        body.append(f'<circle cx="{f(cx)}" cy="{f(y4)}" r="4.5" fill="none" '
                    f'stroke="var(--dia-ink)" stroke-width="2"/>')
        body.append(f'<line x1="{f(cx)}" y1="{f(y4)}" x2="{f(cx)}" y2="{f(TOP - 8)}" '
                    f'class="ink-soft" stroke-width="0.8" stroke-dasharray="2 3"/>')
        body.append(f'<text x="{f(cx + 6)}" y="{f(TOP - 12)}" class="xs mono">'
                    f'row {cross:.0f}: enough stops being enough</text>')

    body.append(f'<text x="{f(px(78))}" y="{f(py(48))}" class="xs mono t-hi">'
                f'62.5 texels/px</text>')

    return write("l39_fig6.svg", svg(600, 226,
        "The footprint outruns the sampler",
        "A logarithmic plot of how many texels one screen pixel covers, traced by ray "
        "casting, from 0.60 at the bottom of the screen to 62.46 two rows below the "
        "horizon. A flat dashed line at 4 marks how many texels bilinear filtering "
        "actually reads. The two cross near screen row 101, and past that point the "
        "sampler is reading a shrinking fraction of what the pixel covers.",
        "\n".join(body), "f396"))


if __name__ == "__main__":
    print("figs_39.py — Lesson 3.9")
    fig1()
    fig2()
    fig3()
    fig4()
    fig5()
    fig6()
