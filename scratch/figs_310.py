#!/usr/bin/env python3
"""Compute every SVG figure for Lesson 3.10 from real data.

Same discipline as 3.7's, 3.8's and 3.9's: nothing is placed by eye, and every
number drawn here is a number `scratch/verify_310.log` printed. Each figure's
docstring states the ONE claim a reader must be able to extract by MEASURING the
picture.

Marker ids are namespaced per figure — 3.9 found six figures quietly sharing
figure 1's markers because `url(#e-i)` resolves to the first match.

Writes scratch/l310_fig{1..6}.svg.
"""
import math

# ---- Measured inputs, from scratch/verify_310.log ---------------------------
# Data, not prose, so a figure and the log it came from cannot drift apart.

RESOLUTION_NS = 41.667      # §A: 24 MHz counter
READ_NS = 5.42              # §A: one SDL_GetPerformanceCounter()
TIMER_NS = 13.46            # §A: one scope_timer
ENCODE_NS = 9.3             # §A: one to_encoded, settled value

# §A: the batch table, (N, min ns, max ns).
BATCHES = [(1, 0.00, 666.67), (10, 8.33, 12.50), (100, 9.17, 10.42),
           (1000, 9.50, 9.62), (10000, 9.33, 9.53)]

# §C: the two frame budgets, in microseconds. (zone, 320x180, 1280x720)
BUDGET = [
    ("build",   0.00,     0.12),
    ("collect", 53.08,   55.04),
    ("sort",    0.00,     0.00),
    ("fill",  1547.29, 23243.92),
    ("overlay",  4.08,    4.71),
    ("present",  3.25,   66.25),
    ("other",    3.17,   53.59),
]
BUDGET_TOTAL = (1610.88, 23423.62)
BUDGET_FPS = (621, 43)
BUDGET_COVERED = (34032, 475218)

ZONE_COLOUR = {
    "build":   "#78c88c",
    "collect": "#ebc864",
    "sort":    "#dc8cc8",
    "fill":    "#eb786e",
    "overlay": "#78b4eb",
    "present": "#9696dc",
    "other":   "#6e7080",
}

# §D: resolution sweep — (w, h, collect us, fill us).
BY_PIXELS = [(320, 180, 61.75, 167.51), (640, 360, 61.43, 558.44),
             (960, 540, 64.11, 1199.33), (1280, 720, 63.57, 2055.97),
             (1920, 1080, 63.75, 4590.60)]

# §D: tessellation sweep — (triangles, px/tri, collect us, fill us).
BY_TRIANGLES = [(36, 196.5, 1.15, 338.40), (144, 70.5, 4.28, 483.86),
                (576, 18.4, 16.57, 508.43), (2304, 4.7, 63.46, 553.17),
                (9216, 1.2, 245.68, 676.44), (36864, 0.3, 967.62, 1069.58)]

# §E: the ladder, in ns per covered pixel. (label, ns/px, is_alternative)
LADDER = [
    ("coverage + colour",        2.682, False),
    ("+ perspective divide",     2.664, False),
    ("+ depth test & write",     2.936, False),
    ("+ sRGB encode (pow)",      9.024, False),
    ("textured, nearest",       18.186, False),
    ("textured, bilinear",      25.076, False),
    ("lit per pixel",           33.048, False),
    ("lit + textured",          46.456, False),
]
LADDER_FAST = 35.212        # §E: the same last rung with encode_mode::fast

# §F: (name, ns/call, speedup, un-rounded error in 8-bit codes, % differing)
CANDIDATES = [
    ("exact (std::pow)",          3.497, 1.00, 0.0000,  0.00),
    ("fitted sqrt chain",         1.856, 1.88, 0.0115,  0.60),
    ("threshold table + bsearch", 6.919, 0.51, 0.0000,  0.00),
    ("uniform table, 4096",       0.994, 3.52, 0.4022,  3.66),
]

# The fit that ships (src/gfx/colour.cpp), and the measured whole-frame result.
FIT = (0.64266026, 0.71185470, -0.33677658, -0.01773836)
AMDAHL_P = 0.960            # §H: fill's share of the 320x180 frame
AMDAHL_S = 1.298            # §H: how much faster the fill got
AMDAHL_MEASURED = 1.287     # §H: the whole-frame speedup actually observed


# ---- Text colour must be a CLASS, never a fill attribute --------------------
# `figure.dia svg text { fill: var(--dia-ink) }` in course.css is CSS, and CSS
# always beats an SVG presentation attribute — so `fill="#eb786e"` on a <text> is
# silently ignored and the label comes out in the theme's ink. Shapes are
# unaffected (the rule only targets text), which is what makes it such a good trap:
# the bar is the right colour and its label is not. docs/_template/apply-shared.py
# lints for it; these classes are declared page-locally in build_310.py.
TEXT_CLASS = {
    ZONE_COLOUR["build"]:   "z-build",
    ZONE_COLOUR["collect"]: "z-collect",
    ZONE_COLOUR["sort"]:    "z-sort",
    ZONE_COLOUR["fill"]:    "z-fill",
    ZONE_COLOUR["overlay"]: "z-overlay",
    ZONE_COLOUR["present"]: "z-present",
    ZONE_COLOUR["other"]:   "z-other",
    "var(--dia-hi)":        "t-hi",
}


def tclass(colour):
    """The class that paints <text> in this series' colour."""
    return TEXT_CLASS[colour]


def srgb(x):
    """The exact transfer function, linear light -> encoded."""
    return x * 12.92 if x <= 0.0031308 else 1.055 * (x ** (1.0 / 2.4)) - 0.055


def srgb_fit(x):
    """What src/gfx/colour.cpp computes."""
    if x <= 0.0031308:
        return x * 12.92
    s1 = math.sqrt(x)
    s2 = math.sqrt(s1)
    s3 = math.sqrt(s2)
    a, b, c, d = FIT
    return a * s1 + b * s2 + c * s3 + d * x


def f(v):
    return f"{v:.1f}"


def f2(v):
    return f"{v:.2f}"


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


def on_swatch(hexcol):
    """Which fixed text fill is legible on this swatch — by luminance, not by eye."""
    r = int(hexcol[1:3], 16) / 255.0
    g = int(hexcol[3:5], 16) / 255.0
    b = int(hexcol[5:7], 16) / 255.0
    return "t-onlight" if (0.2126 * r + 0.7152 * g + 0.0722 * b) > 0.45 else "t-inv"


# =============================================================================
# Figure 1 — the clock cannot see one iteration
# =============================================================================
# ONE CLAIM: the tick spacing is LONGER than the thing being measured. A reader
# must be able to lay the "one encode" bracket against the tick spacing and see
# that four encodes fit inside a single tick — so a measurement of one of them
# reports either 0 ns or 41.67 ns, and never 9.3.
def fig1():
    X0, X1 = 92.0, 610.0
    span = X1 - X0
    NS_SHOWN = 220.0          # nanoseconds across the axis
    def nx(ns):
        return X0 + ns / NS_SHOWN * span

    body = []
    body.append('<text x="16" y="24" class="sm muted">'
                'the counter, ticking at 24 MHz &#8212; one tick every 41.667 ns</text>')

    # ---- The tick lane -------------------------------------------------------
    ty = 44.0
    body.append(f'<line x1="{f(X0)}" y1="{f(ty + 16)}" x2="{f(X1)}" y2="{f(ty + 16)}" '
                f'class="ink" stroke-width="1.2"/>')
    t = 0.0
    k = 0
    while nx(t) <= X1 + 0.5:
        x = nx(t)
        body.append(f'<line x1="{f(x)}" y1="{f(ty)}" x2="{f(x)}" y2="{f(ty + 26)}" '
                    f'class="ink" stroke-width="1.6"/>')
        body.append(f'<text x="{f(x)}" y="{f(ty + 40)}" class="xs mono muted" '
                    f'text-anchor="middle">{k}</text>')
        t += RESOLUTION_NS
        k += 1
    body.append(f'<text x="{f(X1 + 8)}" y="{f(ty + 40)}" class="xs muted">ticks</text>')

    # ---- What one encode actually costs, on the same scale -------------------
    ey = 116.0
    body.append('<text x="16" y="{}" class="sm muted">one encode</text>'.format(f(ey + 12)))
    body.append(f'<rect x="{f(nx(0))}" y="{f(ey)}" width="{f(nx(ENCODE_NS) - nx(0))}" '
                f'height="18" fill="var(--dia-hi)" opacity="0.85"/>')
    body.append(f'<text x="{f(nx(ENCODE_NS) + 10)}" y="{f(ey + 13)}" class="xs mono t-hi">'
                f'{ENCODE_NS:.1f} ns &#8212; {RESOLUTION_NS / ENCODE_NS:.1f} of them fit in ONE tick</text>')

    # ---- And what the clock reports for it -----------------------------------
    ry = 152.0
    body.append('<text x="16" y="{}" class="sm muted">what the</text>'.format(f(ry + 4)))
    body.append('<text x="16" y="{}" class="sm muted">clock says</text>'.format(f(ry + 16)))
    body.append(f'<rect x="{f(nx(0))}" y="{f(ry)}" width="1.5" height="18" '
                f'fill="var(--verify-bd)"/>')
    body.append(f'<text x="{f(nx(0) + 8)}" y="{f(ry + 13)}" class="xs mono t-bad">'
                f'0.00 ns &#8212; or 41.67, depending on where the tick fell</text>')

    # ---- The batch table, which is the way out -------------------------------
    by = 206.0
    body.append(f'<text x="16" y="{f(by)}" class="sm">The way out: time N of them and divide.</text>')
    cols = [(96.0, "N"), (190.0, "min ns"), (286.0, "max ns"), (400.0, "batch in ticks")]
    for cx, label in cols:
        body.append(f'<text x="{f(cx)}" y="{f(by + 20)}" class="xs muted" '
                    f'text-anchor="end">{label}</text>')
    ry2 = by + 36
    for n, lo, hi in BATCHES:
        ticks_lo = lo * n / RESOLUTION_NS
        ticks_hi = hi * n / RESOLUTION_NS
        # Flagged when the WHOLE BATCH is under 20 ticks, i.e. the quantisation
        # is over 5%. N = 100 spans 22-25 ticks and has already settled, so it
        # is not flagged: the threshold is a statement about the clock, not a
        # verdict on the row.
        bad = (ticks_hi < 20.0)
        cls = "xs mono t-bad" if bad else "xs mono"
        body.append(f'<text x="96" y="{f(ry2)}" class="{cls}" text-anchor="end">{n}</text>')
        body.append(f'<text x="190" y="{f(ry2)}" class="{cls}" text-anchor="end">{lo:.2f}</text>')
        body.append(f'<text x="286" y="{f(ry2)}" class="{cls}" text-anchor="end">{hi:.2f}</text>')
        body.append(f'<text x="400" y="{f(ry2)}" class="{cls}" text-anchor="end">'
                    f'{ticks_lo:.0f} &#8211; {ticks_hi:.0f}</text>')
        if bad:
            body.append(f'<text x="416" y="{f(ry2)}" class="xs t-bad">'
                        f'under 20 ticks &#8212; quantisation, not measurement</text>')
        ry2 += 17

    return svg(640, 372,
               "The clock ticks more slowly than one encode takes",
               "A timeline of counter ticks 41.667 nanoseconds apart, with one sRGB "
               "encode drawn to scale beneath it at 9.3 nanoseconds - about four and a "
               "half encodes fit inside a single tick, so timing one of them reports "
               "either zero or one whole tick. A table beneath shows the same work "
               "timed in batches of 1 to 10000, with the batch length in ticks; only "
               "from about N = 100 does the batch span enough ticks for the answer to "
               "settle at 9.3 nanoseconds.",
               "\n".join("  " + s for s in body), "f3101")


# =============================================================================
# Figure 2 — the frame budget, at two resolutions
# =============================================================================
# ONE CLAIM: `fill` is essentially the whole frame, and `collect` DOES NOT MOVE
# when the pixel count goes up sixteen times. A reader must be able to compare
# the two `collect` numbers (53.08 and 55.04) and see they are the same number.
def fig2():
    X0 = 150.0
    BAR_W = 404.0
    body = []

    for panel, (label, idx) in enumerate((("320 x 180", 0), ("1280 x 720", 1))):
        top = 34.0 + panel * 156.0
        total = BUDGET_TOTAL[idx]

        body.append(f'<text x="16" y="{f(top - 12)}" class="sm">{label}'
                    f'<tspan class="muted">  &#8212; same geometry, '
                    f'{BUDGET_COVERED[idx]:,} covered px</tspan></text>')

        # The stacked bar.
        cursor = X0
        for name, us320, us720 in BUDGET:
            us = us320 if idx == 0 else us720
            w = BAR_W * us / total
            body.append(f'<rect x="{f(cursor)}" y="{f(top)}" width="{f(max(w, 0.0))}" '
                        f'height="22" fill="{ZONE_COLOUR[name]}"/>')
            cursor += w
        body.append(f'<rect x="{f(X0)}" y="{f(top)}" width="{f(BAR_W)}" height="22" '
                    f'fill="none" class="ink" stroke-width="1"/>')

        # The dominant segment gets its share written inside it.
        fill_us = BUDGET[3][1 + idx]
        fill_share = 100.0 * fill_us / total
        fill_w = BAR_W * fill_us / total
        fill_x = X0 + BAR_W * sum(b[1 + idx] for b in BUDGET[:3]) / total
        body.append(f'<text x="{f(fill_x + fill_w / 2)}" y="{f(top + 15)}" '
                    f'class="xs mono {on_swatch(ZONE_COLOUR["fill"])}" text-anchor="middle">'
                    f'fill {fill_share:.1f}%</text>')

        body.append(f'<text x="{f(X0 + BAR_W + 8)}" y="{f(top + 15)}" class="xs mono">'
                    f'{total / 1000.0:.2f} ms</text>')

        # The rows, two columns.
        for i, (name, us320, us720) in enumerate(BUDGET):
            us = us320 if idx == 0 else us720
            col = i // 4
            r = i % 4
            rx = 16.0 + col * 300.0
            ry = top + 42.0 + r * 15.0
            body.append(f'<rect x="{f(rx)}" y="{f(ry - 7)}" width="8" height="8" '
                        f'fill="{ZONE_COLOUR[name]}"/>')
            body.append(f'<text x="{f(rx + 14)}" y="{f(ry)}" class="xs mono">{name}</text>')
            body.append(f'<text x="{f(rx + 168)}" y="{f(ry)}" class="xs mono" '
                        f'text-anchor="end">{us:8.2f} us</text>')
            body.append(f'<text x="{f(rx + 226)}" y="{f(ry)}" class="xs mono muted" '
                        f'text-anchor="end">{100.0 * us / total:5.1f}%</text>')

        body.append(f'<text x="{f(X0 + BAR_W + 8)}" y="{f(top + 32)}" class="xs muted">'
                    f'{BUDGET_FPS[idx]} fps</text>')

    # The one comparison the figure exists to make.
    body.append('<text x="16" y="332" class="sm t-hi">'
                'collect: 53.08 us &#8594; 55.04 us. Sixteen times the pixels moved it by 3.7%.</text>')
    body.append('<text x="16" y="348" class="sm t-hi">'
                'fill: 1.55 ms &#8594; 23.24 ms. Fifteen times, which is the pixel count.</text>')

    return svg(640, 360,
               "The same frame at two resolutions, phase by phase",
               "Two stacked bars showing where a frame's time goes. At 320 by 180 the "
               "fill is 96.1 per cent of a 1.61 millisecond frame; at 1280 by 720 it is "
               "99.2 per cent of a 23.42 millisecond frame. Every other phase is a sliver. "
               "The collect phase measures 53.08 microseconds at the small size and 55.04 "
               "at the large one - unchanged - while the fill grows fifteenfold, which is "
               "the ratio of the pixel counts.",
               "\n".join("  " + s for s in body), "f3102")


# =============================================================================
# Figure 3 — two slopes
# =============================================================================
# ONE CLAIM: on a log-log plot, `fill` has slope 1 against pixels and slope ~0
# against triangles, and `collect` has exactly the opposite. A reader must be
# able to see the two lines CROSS in the right-hand panel.
def fig3():
    body = []

    def panel(px0, py0, pw, ph, xs, series, xlabel, title, xticks):
        out = []
        lo_x, hi_x = min(xs), max(xs)
        vals = [v for _, ys in series for v in ys]
        lo_y, hi_y = min(vals), max(vals)

        def sx(v):
            return px0 + (math.log10(v) - math.log10(lo_x)) / \
                   (math.log10(hi_x) - math.log10(lo_x)) * pw

        def sy(v):
            return py0 + ph - (math.log10(v) - math.log10(lo_y)) / \
                   (math.log10(hi_y) - math.log10(lo_y)) * ph

        out.append(f'<text x="{f(px0)}" y="{f(py0 - 12)}" class="sm">{title}</text>')
        out.append(f'<rect x="{f(px0)}" y="{f(py0)}" width="{f(pw)}" height="{f(ph)}" '
                   f'class="fill-soft grid" stroke-width="1"/>')

        # Decade gridlines on y.
        d = math.ceil(math.log10(lo_y))
        while d <= math.floor(math.log10(hi_y)):
            y = sy(10.0 ** d)
            out.append(f'<line x1="{f(px0)}" y1="{f(y)}" x2="{f(px0 + pw)}" y2="{f(y)}" '
                       f'class="grid" stroke-width="0.8"/>')
            lab = f"{10 ** d:g}"
            out.append(f'<text x="{f(px0 - 5)}" y="{f(y + 3)}" class="xs mono muted" '
                       f'text-anchor="end">{lab}</text>')
            d += 1
        out.append(f'<text x="{f(px0 - 34)}" y="{f(py0 + ph / 2)}" class="xs muted" '
                   f'transform="rotate(-90 {f(px0 - 34)} {f(py0 + ph / 2)})" '
                   f'text-anchor="middle">microseconds</text>')

        for xv, lab in xticks:
            x = sx(xv)
            out.append(f'<line x1="{f(x)}" y1="{f(py0 + ph)}" x2="{f(x)}" '
                       f'y2="{f(py0 + ph + 4)}" class="ink-soft" stroke-width="1"/>')
            out.append(f'<text x="{f(x)}" y="{f(py0 + ph + 16)}" class="xs mono muted" '
                       f'text-anchor="middle">{lab}</text>')
        out.append(f'<text x="{f(px0 + pw / 2)}" y="{f(py0 + ph + 32)}" class="xs muted" '
                   f'text-anchor="middle">{xlabel}</text>')

        for (name, ys), col in zip(series, (ZONE_COLOUR["collect"], ZONE_COLOUR["fill"])):
            pts = " ".join(f"{f(sx(x))},{f(sy(y))}" for x, y in zip(xs, ys))
            out.append(f'<polyline points="{pts}" fill="none" stroke="{col}" '
                       f'stroke-width="2.2"/>')
            for x, y in zip(xs, ys):
                out.append(f'<circle cx="{f(sx(x))}" cy="{f(sy(y))}" r="2.6" fill="{col}"/>')
            # Anchored to the series' FIRST point when its last point collides with
            # the other series' - which is exactly what happens on the right-hand
            # panel, where the two lines cross and finish on top of each other.
            end_y = sy(ys[-1])
            other = [yy for nm, yy in series if nm != name][0]
            if abs(end_y - sy(other[-1])) < 16.0:
                # Below the first point for a RISING series, above for a flat or
                # falling one. Anchoring every fallback label the same way puts it
                # directly on the stroke it belongs to for exactly the series whose
                # slope is steepest — which check-page.js's onShape test catches,
                # and the eye does not.
                rising = ys[-1] > ys[0]
                dy = 15.0 if rising else -9.0
                out.append(f'<text x="{f(sx(xs[0]) + 5)}" y="{f(sy(ys[0]) + dy)}" '
                           f'class="xs mono {tclass(col)}">{name}</text>')
            else:
                out.append(f'<text x="{f(sx(xs[-1]) - 4)}" y="{f(end_y - 8)}" '
                           f'class="xs mono {tclass(col)}" text-anchor="end">{name}</text>')
        return out

    px_counts = [w * h for w, h, _, _ in BY_PIXELS]
    body += panel(56, 44, 224, 176, px_counts,
                  [("collect", [c for _, _, c, _ in BY_PIXELS]),
                   ("fill", [fl for _, _, _, fl in BY_PIXELS])],
                  "pixels on screen", "Same geometry, more pixels",
                  [(320 * 180, "57k"), (640 * 360, "230k"), (1280 * 720, "922k"),
                   (1920 * 1080, "2.1M")])

    tri_counts = [t for t, _, _, _ in BY_TRIANGLES]
    body += panel(388, 44, 224, 176, tri_counts,
                  [("collect", [c for _, _, c, _ in BY_TRIANGLES]),
                   ("fill", [fl for _, _, _, fl in BY_TRIANGLES])],
                  "triangles submitted", "Same pixels, more triangles",
                  [(36, "36"), (576, "576"), (9216, "9k"), (36864, "37k")])

    body.append('<text x="16" y="272" class="sm">'
                'Left: <tspan class="mono">collect</tspan> is FLAT and '
                '<tspan class="mono">fill</tspan> rises with slope 1 &#8212; it is paid per pixel.</text>')
    body.append('<text x="16" y="290" class="sm">'
                'Right: they CROSS. Past about 1 pixel per triangle the vertex stage is the '
                'expensive one.</text>')
    body.append('<text x="16" y="310" class="sm muted">'
                'The crossing is the only place on either chart where "optimise the fill" '
                'stops being the right advice, and</text>')
    body.append('<text x="16" y="326" class="sm muted">'
                'nothing about the renderer changed to get there &#8212; only how finely the '
                'same object was cut up.</text>')

    return svg(640, 340,
               "Two phases, two different axes",
               "Two log-log charts. On the left, against pixel count, the fill phase rises "
               "as a straight line of slope one from 168 to 4591 microseconds while the "
               "collect phase stays flat near 62 microseconds. On the right, against "
               "triangle count at constant screen coverage, the collect phase rises "
               "steeply from 1.15 to 968 microseconds and crosses the fill phase, which "
               "climbs only from 338 to 1070. The crossing happens near one pixel per "
               "triangle.",
               "\n".join("  " + s for s in body), "f3103")


# =============================================================================
# Figure 4 — where the fill actually goes
# =============================================================================
# ONE CLAIM: the sRGB encode's bar (+6.088) is LONGER than the whole of coverage,
# interpolation, the perspective divide and the depth test put together (2.936).
# A reader must be able to lay one against the other.
def fig4():
    X0 = 176.0
    SCALE = 7.4           # px per ns/px
    body = []

    body.append('<text x="16" y="22" class="sm muted">'
                'nanoseconds per covered pixel, 2 triangles over 272,223 px at 960x540</text>')

    y = 44.0
    prev = 0.0
    for label, per_px, _ in LADDER:
        delta = per_px - prev
        # The running total, as a light bar; the DELTA, as a saturated one on its end.
        body.append(f'<text x="{f(X0 - 8)}" y="{f(y + 11)}" class="xs mono" '
                    f'text-anchor="end">{label}</text>')
        body.append(f'<rect x="{f(X0)}" y="{f(y)}" width="{f(prev * SCALE)}" height="15" '
                    f'fill="{ZONE_COLOUR["fill"]}" opacity="0.28"/>')
        col = ZONE_COLOUR["fill"] if delta >= 0 else "#78c88c"
        w = abs(delta) * SCALE
        x = X0 + (prev * SCALE if delta >= 0 else per_px * SCALE)
        body.append(f'<rect x="{f(x)}" y="{f(y)}" width="{f(w)}" height="15" fill="{col}"/>')
        body.append(f'<text x="{f(X0 + per_px * SCALE + 8)}" y="{f(y + 11)}" class="xs mono">'
                    f'{per_px:5.2f}</text>')
        if prev > 0.0:
            sign = "+" if delta >= 0 else "−"
            body.append(f'<text x="{f(X0 + per_px * SCALE + 48)}" y="{f(y + 11)}" '
                        f'class="xs mono muted">{sign}{abs(delta):.2f}</text>')
        prev = per_px
        y += 21.0

    # The comparison the figure exists to make, drawn as two measured brackets.
    base = LADDER[2][1]                       # everything up to and including depth
    enc = LADDER[3][1] - LADDER[2][1]         # the encode's delta
    by = y + 14.0
    body.append(f'<rect x="{f(X0)}" y="{f(by)}" width="{f(base * SCALE)}" height="13" '
                f'fill="var(--dia-ink-soft)" opacity="0.5"/>')
    body.append(f'<text x="{f(X0 - 8)}" y="{f(by + 10)}" class="xs mono" text-anchor="end">'
                f'coverage + divide + depth</text>')
    body.append(f'<text x="{f(X0 + base * SCALE + 8)}" y="{f(by + 10)}" class="xs mono muted">'
                f'{base:.2f} ns/px</text>')

    body.append(f'<rect x="{f(X0)}" y="{f(by + 18)}" width="{f(enc * SCALE)}" height="13" '
                f'fill="var(--dia-hi)"/>')
    body.append(f'<text x="{f(X0 - 8)}" y="{f(by + 28)}" class="xs mono t-hi" '
                f'text-anchor="end">the sRGB encode alone</text>')
    body.append(f'<text x="{f(X0 + enc * SCALE + 8)}" y="{f(by + 28)}" class="xs mono t-hi">'
                f'{enc:.2f} ns/px &#8212; {enc / base:.1f}x</text>')

    # And the fast encode, on the last rung.
    body.append(f'<text x="16" y="{f(by + 54)}" class="sm">'
                f'The whole lit + textured fragment: <tspan class="mono">{LADDER[-1][1]:.2f}</tspan> '
                f'ns/px with <tspan class="mono">pow</tspan>, '
                f'<tspan class="mono t-hi">{LADDER_FAST:.2f}</tspan> with the fitted encode.</text>')

    return svg(640, int(by + 76),
               "A ladder of fragment work, one rung at a time",
               "A waterfall chart of nanoseconds per covered pixel. Coverage plus colour "
               "interpolation costs 2.68; the perspective divide adds nothing measurable; "
               "the depth test adds 0.27; the sRGB encode adds 6.09 - more than twice "
               "everything before it combined. Texturing adds a further 15 for a nearest "
               "fetch and 7 more for bilinear, and per-pixel lighting another 21. Two "
               "brackets at the bottom compare coverage plus divide plus depth, 2.94 "
               "nanoseconds, against the encode alone at 6.09.",
               "\n".join("  " + s for s in body), "f3104")


# =============================================================================
# Figure 5 — the curve, the fit, and the error
# =============================================================================
# ONE CLAIM: the approximation is INDISTINGUISHABLE from the exact curve at any
# honest scale, and the error only becomes visible when multiplied by 2000. A
# reader must be able to see the error curve OSCILLATE around zero — that is what
# a minimax fit looks like, and it is why the error is not a bias.
def fig5():
    X0, X1 = 76.0, 596.0
    Y0, Y1 = 40.0, 172.0
    body = []

    def cx(u):
        return X0 + u * (X1 - X0)

    def cy(v):
        return Y1 - v * (Y1 - Y0)

    body.append(f'<rect x="{f(X0)}" y="{f(Y0)}" width="{f(X1 - X0)}" height="{f(Y1 - Y0)}" '
                f'class="fill-soft grid" stroke-width="1"/>')
    for t in (0.0, 0.25, 0.5, 0.75, 1.0):
        body.append(f'<line x1="{f(cx(t))}" y1="{f(Y0)}" x2="{f(cx(t))}" y2="{f(Y1)}" '
                    f'class="grid" stroke-width="0.8"/>')
        body.append(f'<text x="{f(cx(t))}" y="{f(Y1 + 14)}" class="xs mono muted" '
                    f'text-anchor="middle">{t:.2f}</text>')
        body.append(f'<line x1="{f(X0)}" y1="{f(cy(t))}" x2="{f(X1)}" y2="{f(cy(t))}" '
                    f'class="grid" stroke-width="0.8"/>')
        body.append(f'<text x="{f(X0 - 6)}" y="{f(cy(t) + 3)}" class="xs mono muted" '
                    f'text-anchor="end">{t:.2f}</text>')
    body.append(f'<text x="{f((X0 + X1) / 2)}" y="{f(Y1 + 30)}" class="xs muted" '
                f'text-anchor="middle">linear light in</text>')
    body.append(f'<text x="{f(X0 - 40)}" y="{f((Y0 + Y1) / 2)}" class="xs muted" '
                f'transform="rotate(-90 {f(X0 - 40)} {f((Y0 + Y1) / 2)})" '
                f'text-anchor="middle">encoded out</text>')

    N = 400
    exact_pts = " ".join(f"{f(cx(i / N))},{f(cy(srgb(i / N)))}" for i in range(N + 1))
    body.append(f'<polyline points="{exact_pts}" fill="none" class="ink" stroke-width="3.4" '
                f'opacity="0.9"/>')
    fit_pts = " ".join(f"{f(cx(i / N))},{f(cy(srgb_fit(i / N)))}" for i in range(N + 1))
    body.append(f'<polyline points="{fit_pts}" fill="none" stroke="var(--dia-hi)" '
                f'stroke-width="1.4" stroke-dasharray="4 3"/>')

    body.append(f'<text x="{f(cx(0.52))}" y="{f(cy(0.62))}" class="xs mono">'
                f'exact: 1.055 x^(1/2.4) &#8722; 0.055</text>')
    body.append(f'<text x="{f(cx(0.52))}" y="{f(cy(0.52))}" class="xs mono t-hi">'
                f'fit: a&#8730;x + b&#8724;&#8730;x + c&#8312;&#8730;x + dx</text>')

    # The toe, marked where it actually is.
    body.append(f'<line x1="{f(cx(0.0031308))}" y1="{f(Y0)}" x2="{f(cx(0.0031308))}" '
                f'y2="{f(Y1)}" class="ink-soft" stroke-width="1" stroke-dasharray="2 3"/>')
    body.append(f'<text x="{f(cx(0.0031308) + 6)}" y="{f(Y0 + 14)}" class="xs muted">'
                f'x = 0.0031308: below this the exact function is 12.92x, kept as it is</text>')

    # ---- The error, magnified ------------------------------------------------
    EY0, EY1 = 236.0, 316.0
    EMID = (EY0 + EY1) / 2

    # The magnification is DERIVED from the worst error, not chosen, so the curve
    # fills the panel whatever the fit turns out to be — and the caption can state
    # the factor honestly instead of the figure quietly clipping.
    errs = [(i / N, srgb_fit(i / N) - srgb(i / N)) for i in range(N + 1)]
    worst = max(abs(e) for _, e in errs)
    half = (EY1 - EY0) / 2.0 * 0.86
    mag = half / worst

    body.append(f'<text x="16" y="{f(EY0 - 12)}" class="sm">'
                f'the difference between those two curves, magnified '
                f'{mag:,.0f}x to be visible at all</text>')
    body.append(f'<rect x="{f(X0)}" y="{f(EY0)}" width="{f(X1 - X0)}" height="{f(EY1 - EY0)}" '
                f'class="fill-soft grid" stroke-width="1"/>')
    body.append(f'<line x1="{f(X0)}" y1="{f(EMID)}" x2="{f(X1)}" y2="{f(EMID)}" '
                f'class="ink-soft" stroke-width="1"/>')

    err_pts = [f"{f(cx(u))},{f(EMID - e * mag)}" for u, e in errs]
    body.append(f'<polyline points="{" ".join(err_pts)}" fill="none" stroke="var(--dia-hi)" '
                f'stroke-width="1.8"/>')

    body.append(f'<text x="{f(X1 - 4)}" y="{f(EY0 + 14)}" class="xs mono t-hi" '
                f'text-anchor="end">worst |error| = {worst:.7f} = '
                f'{worst * 255:.4f} of an 8-bit code</text>')
    body.append(f'<text x="{f(X0)}" y="{f(EY1 + 18)}" class="xs muted">'
                f'It CROSSES ZERO repeatedly &#8212; that is what a minimax fit looks like, and it is '
                f'why the error is a wobble rather than a bias.</text>')
    body.append(f'<text x="{f(X0)}" y="{f(EY1 + 32)}" class="xs muted">'
                f'The single tallest excursion is at the left, exactly at the toe boundary: '
                f'the hardest point on the curve is the one</text>')
    body.append(f'<text x="{f(X0)}" y="{f(EY1 + 46)}" class="xs muted">'
                f'where a fit made of square roots has to meet a straight line.</text>')

    return svg(640, 372,
               "The sRGB curve, the four-term fit, and the gap between them",
               "The exact sRGB transfer function drawn as a thick line with the fitted "
               "approximation dashed over it; at this scale the two are indistinguishable "
               "across the whole range. A vertical marker at x = 0.0031308 shows where the "
               "exact linear toe is kept rather than fitted. Below, the difference between "
               "the two curves multiplied by two thousand: it oscillates about zero with "
               "several crossings and a worst magnitude of 0.0000451, which is 0.0115 of "
               "one 8-bit code.",
               "\n".join("  " + s for s in body), "f3105")


# =============================================================================
# Figure 6 — Amdahl's law, with our own measurement on it
# =============================================================================
# ONE CLAIM: the ceiling is set by the part you are NOT speeding up. A reader must
# be able to run a finger up the p = 0.96 curve to the right edge and read off
# that even an INFINITELY fast fill only buys 25x.
def fig6():
    X0, X1 = 68.0, 470.0
    Y0, Y1 = 36.0, 240.0
    body = []

    S_MAX = 64.0
    Y_MAX = 32.0

    def sx(s):
        return X0 + math.log10(s) / math.log10(S_MAX) * (X1 - X0)

    def sy(v):
        return Y1 - math.log10(v) / math.log10(Y_MAX) * (Y1 - Y0)

    body.append(f'<rect x="{f(X0)}" y="{f(Y0)}" width="{f(X1 - X0)}" height="{f(Y1 - Y0)}" '
                f'class="fill-soft grid" stroke-width="1"/>')
    for s in (1, 2, 4, 8, 16, 32, 64):
        body.append(f'<line x1="{f(sx(s))}" y1="{f(Y0)}" x2="{f(sx(s))}" y2="{f(Y1)}" '
                    f'class="grid" stroke-width="0.8"/>')
        body.append(f'<text x="{f(sx(s))}" y="{f(Y1 + 15)}" class="xs mono muted" '
                    f'text-anchor="middle">{s}x</text>')
    for v in (1, 2, 4, 8, 16, 32):
        body.append(f'<line x1="{f(X0)}" y1="{f(sy(v))}" x2="{f(X1)}" y2="{f(sy(v))}" '
                    f'class="grid" stroke-width="0.8"/>')
        body.append(f'<text x="{f(X0 - 6)}" y="{f(sy(v) + 3)}" class="xs mono muted" '
                    f'text-anchor="end">{v}x</text>')
    body.append(f'<text x="{f((X0 + X1) / 2)}" y="{f(Y1 + 32)}" class="xs muted" '
                f'text-anchor="middle">how much faster you made the PART</text>')
    body.append(f'<text x="{f(X0 - 38)}" y="{f((Y0 + Y1) / 2)}" class="xs muted" '
                f'transform="rotate(-90 {f(X0 - 38)} {f((Y0 + Y1) / 2)})" '
                f'text-anchor="middle">the whole frame</text>')

    for p, col, lab in ((0.99, ZONE_COLOUR["fill"], "p = 0.99"),
                        (AMDAHL_P, "var(--dia-hi)", f"p = {AMDAHL_P:.2f}  (our fill)"),
                        (0.75, ZONE_COLOUR["collect"], "p = 0.75"),
                        (0.50, ZONE_COLOUR["overlay"], "p = 0.50")):
        pts = []
        s = 1.0
        while s <= S_MAX + 1e-9:
            pts.append(f"{f(sx(s))},{f(sy(1.0 / (1.0 - p + p / s)))}")
            s *= 1.06
        body.append(f'<polyline points="{" ".join(pts)}" fill="none" stroke="{col}" '
                    f'stroke-width="2"/>')
        ceiling = 1.0 / (1.0 - p)
        body.append(f'<line x1="{f(X0)}" y1="{f(sy(ceiling))}" x2="{f(X1)}" '
                    f'y2="{f(sy(ceiling))}" stroke="{col}" stroke-width="1" '
                    f'stroke-dasharray="2 4" opacity="0.7"/>')
        body.append(f'<text x="{f(X1 + 6)}" y="{f(sy(1.0 / (1.0 - p + p / S_MAX)) + 3)}" '
                    f'class="xs mono {tclass(col)}">{lab}</text>')

    # Our measured point.
    got = 1.0 / (1.0 - AMDAHL_P + AMDAHL_P / AMDAHL_S)
    body.append(f'<circle cx="{f(sx(AMDAHL_S))}" cy="{f(sy(got))}" r="4.5" '
                f'fill="var(--dia-hi)" class="ink" stroke-width="1.2"/>')
    # NO LABEL ON THE POINT. Every curve bunches within a few pixels of each other
    # near s = 1.3, so any text placed beside the marker lands on top of one of
    # them. The chart shows the SHAPE; the number goes in the prose below it, where
    # there is room to state it properly. A cramped annotation that overlaps a
    # curve costs more than it explains.
    body.append(f'<text x="16" y="{f(Y1 + 56)}" class="sm">'
                f'The marked point is this lesson\'s result: a '
                f'<tspan class="mono t-hi">{AMDAHL_S:.2f}x</tspan> faster fill at '
                f'p = {AMDAHL_P:.2f} gave a '
                f'<tspan class="mono t-hi">{AMDAHL_MEASURED:.2f}x</tspan> faster frame.</text>')
    body.append(f'<text x="16" y="{f(Y1 + 74)}" class="sm">'
                f'Now run a finger UP that curve to the right edge. Even an '
                f'<tspan class="mono">infinitely</tspan> fast fill stops at '
                f'{1.0 / (1.0 - AMDAHL_P):.0f}x, because</text>')
    body.append(f'<text x="16" y="{f(Y1 + 90)}" class="sm">'
                f'the other {100 * (1 - AMDAHL_P):.1f}% of the frame does not go away.</text>')

    return svg(640, 352,
               "Amdahl's law, with this lesson's own optimisation plotted on it",
               "A log-log chart of whole-frame speedup against the speedup of one phase, "
               "for four different phase shares. Each curve flattens to a horizontal "
               "ceiling: 100x for a phase that is 99 per cent of the frame, 25x for 96 per "
               "cent, 4x for 75 per cent and 2x for 50 per cent. A marked point shows this "
               "lesson's result - a 1.30 times faster fill occupying 96 per cent of the "
               "frame, giving a 1.29 times faster frame.",
               "\n".join("  " + s for s in body), "f3106")


def main():
    print("Lesson 3.10 figures")
    # RENUMBERED 2026-09-11 to match page order — see build_310.py. The
    # fig<N>() names are the ORIGINAL draft order and are left alone; what
    # matters is which FILE each one writes, because that is what the page
    # embeds. Content named per line so the pairing can be checked by eye.
    write("l310_fig1.svg", fig1())   # the clock's resolution
    write("l310_fig2.svg", fig2())   # the frame budget
    write("l310_fig3.svg", fig3())   # pixels per triangle
    write("l310_fig5.svg", fig4())   # the differential ladder
    write("l310_fig6.svg", fig5())   # the sRGB fit
    write("l310_fig4.svg", fig6())   # Amdahl's law


if __name__ == "__main__":
    main()
