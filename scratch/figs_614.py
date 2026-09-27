#!/usr/bin/env python3
"""scratch/figs_614.py — Lesson 6.14's diagrams.

Same rules as 6.1-6.13's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - ~5.2 units per character for `xs`, ~6.0 for `sm`
  - filenames numbered by PAGE ORDER
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - `rule()` takes a CSS CLASS; `cline()` takes a COLOUR (6.10's trap)
  - LEGENDS AND ANNOTATION BOXES GO OUTSIDE THE PLOT (6.11's trap, 6.13's again)
  - and a SHAPE can leave the viewBox where a label cannot: check-page.js's
    spill test is text-only (6.13 found that the hard way)

Every number comes from verify_614's output.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)
from figs_610 import cline                                          # noqa: E402

OUT = "scratch"

# ---- measured (verify_614) -------------------------------------------------
LOBE_COEFF = 0.643631
FOLKLORE_OVER = 55          # per cent
TURN_40PX = 0.039270        # rad per pixel, sphere 40 px in radius
CROSSOVER = 0.2468
LOBES = [(0.50, 0.166950, 4.251), (0.30, 0.058192, 1.482), (0.20, 0.025767, 0.656),
         (0.10, 0.006436, 0.164), (0.05, 0.001609, 0.041)]

CHECKER = [(1, 1, 0.5000, 0.5000), (2, 4, 0.1719, 0.5000),
           (4, 16, 0.0794, 0.1240), (8, 64, 0.0231, 0.0333)]

RESOLVE_LINEAR = 188
RESOLVE_ENCODED = 127
RESOLVE_DELIVERED = 21.2    # per cent of the light

SPEC = [(0.30, 0.3087, 2.8, 8.1, 1.1, 1.1),
        (0.20, 0.2254, 10.5, 28.5, 1.6, 1.3),
        (0.10, 0.1814, 81.3, 77.5, 24.0, 1.9),
        (0.05, 0.1773, 230.1, 94.7, 3996.5, 2.0)]
SWING_GAIN = 2034
RMS_GAIN = 2.4

MSAA_SSAA = [(0.50, 5.99, 5.97, 1.003), (0.20, 191.66, 181.98, 1.053),
             (0.10, 406.28, 1288.18, 0.315), (0.05, 52.02, 15587.65, 0.003)]

FIREFLY_RESOLVED = 13979
FIREFLY_OVER = 2192
MSAA_MB = 15.84
HDR_MB = 3.96
PIPELINES = 22
DEMO_CHANGED_PCT = 2.2
DEMO_LEVELS = (166, 184)
CHECKS = 27


# ===========================================================================
# Figure 1 — two problems that share a name                              (§1)
# ===========================================================================
def fig_two():
    W, H = 720, 400
    b = [label(W / 2, 22, "Two problems that share a name, and the popular cure fixes one", "sm")]

    # LEFT: geometric aliasing — a staircase.
    lx, ly, cell = 60, 60, 20
    b.append(label(lx + 5 * cell, ly - 14, "GEOMETRIC: the staircase", "xs"))
    for i in range(6):
        # a descending stair of filled cells
        for j in range(i + 1):
            b.append(box(lx + i * cell, ly + (5 - j) * cell, cell - 1, cell - 1,
                         BLUE, rx=1, opacity=0.55))
    # the true edge the stair approximates
    b.append(cline(lx, ly + 6 * cell, lx + 6 * cell, ly, RED, 2.0))
    b.append(label(lx + 5 * cell, ly + 6 * cell + 20, "coverage is a STEP function:", "xs"))
    b.append(label(lx + 5 * cell, ly + 6 * cell + 36, "infinite bandwidth, so NO", "xs"))
    b.append(label(lx + 5 * cell, ly + 6 * cell + 52, "sample rate is enough", "xs"))

    # RIGHT: shading aliasing — a narrow lobe falling between sample points.
    rx, ry, step = 400, 62, 24
    b.append(label(rx + 2.5 * step, ry - 14, "SHADING: the sparkle", "xs"))

    # The pixel grid, drawn faintly, with one sample point per pixel centre.
    for j in range(6):
        for i in range(6):
            b.append(box(rx + i * step, ry + j * step, step - 1, step - 1,
                         None, rx=1))
    for j in range(6):
        for i in range(6):
            b.append(f'<circle cx="{rx + i * step + step / 2:.1f}" '
                     f'cy="{ry + j * step + step / 2:.1f}" r="1.8" fill="{GREY}"/>')

    # The lobe: small, bright, and sitting on a CORNER between four samples.
    lcx = rx + 3 * step
    lcy = ry + 2 * step
    for r_, op in ((13.0, 0.18), (8.0, 0.34), (4.0, 0.85)):
        b.append(f'<circle cx="{lcx}" cy="{lcy}" r="{r_}" fill="{AMBER}" fill-opacity="{op}"/>')

    # Where it was one sub-pixel step ago — also between samples.
    for r_, op in ((11.0, 0.10), (6.5, 0.16)):
        b.append(f'<circle cx="{lcx - step * 0.55:.1f}" cy="{lcy + step * 0.45:.1f}" '
                 f'r="{r_}" fill="{AMBER}" fill-opacity="{op}"/>')

    b.append(label(rx + 3 * step, ry + 6 * step + 20,
                   "the lobe is NARROWER than the spacing", "xs"))
    b.append(label(rx + 3 * step, ry + 6 * step + 36,
                   "of the samples hunting for it, so it lands", "xs"))
    b.append(label(rx + 3 * step, ry + 6 * step + 52,
                   "BETWEEN them and the pixel reports nothing", "xs"))

    b.append(hollow(60, 300, 600, 54, AMBER, rx=4))
    b.append(label(360, 320,
                   "MSAA multisamples COVERAGE and shades ONCE per primitive per "
                   "pixel. That asymmetry is what makes it", "xs"))
    b.append(label(360, 338,
                   "cost ~1.3x instead of 4x — and it is exactly why it fixes the "
                   "left-hand problem and not the right-hand one.", "xs"))

    b.append(label(W / 2, H - 8,
                   "You cannot buy shading samples with a coverage feature.", "xs"))
    return svg("f614a", W, H, "Geometric aliasing against shading aliasing",
               "Left: a staircase of filled cells approximating a straight edge. "
               "Right: a shiny sphere with a narrow specular lobe falling between "
               "the pixel's sample points.", b)


# ===========================================================================
# Figure 2 — what supersampling does, and does not                      (§3.2)
# ===========================================================================
def fig_nyquist():
    W, H = 720, 400
    b = [label(W / 2, 22, "A checker finer than the pixel grid: the error falls, the detail does not return", "sm")]

    x0, x1 = 110, 470
    y0, y1 = 58, 250

    def px(f):
        return x0 + (x1 - x0) * (math.log2(f) / 3.0)

    def py(v):
        return y1 - (y1 - y0) * (v / 0.55)

    b.append(hollow(x0, y0, x1 - x0, y1 - y0, GREY, dash="3 3"))
    for f, _s, _r, _w in CHECKER:
        b.append(rule(px(f), y1, px(f), y1 + 5, "grid", 1.0))
        b.append(label(px(f), y1 + 20, f"{f}x", "xs"))
    b.append(label((x0 + x1) / 2, y1 + 42, "linear supersampling factor", "xs"))
    for v in (0.0, 0.25, 0.5):
        b.append(rule(x0 - 5, py(v), x0, py(v), "grid", 1.0))
        b.append(label(x0 - 10, py(v) + 4, f"{v:.2f}", "xs", "end"))

    rms = " ".join(f"{px(f):.1f},{py(r):.1f}" for f, _s, r, _w in CHECKER)
    wor = " ".join(f"{px(f):.1f},{py(w):.1f}" for f, _s, _r, w in CHECKER)
    b.append(f'<polyline points="{wor}" fill="none" stroke="{GREY}" stroke-width="2.0" '
             f'stroke-dasharray="5 3"/>')
    b.append(f'<polyline points="{rms}" fill="none" stroke="{AMBER}" stroke-width="2.4"/>')
    for f, _s, r, _w in CHECKER:
        b.append(f'<circle cx="{px(f):.1f}" cy="{py(r):.1f}" r="3.2" fill="{AMBER}"/>')

    lx = x1 + 26
    b.append(cline(lx, y0 + 14, lx + 22, y0 + 14, AMBER, 2.4))
    b.append(label(lx + 28, y0 + 18, "per-pixel RMS deviation", "xs", "start"))
    b.append(cline(lx, y0 + 44, lx + 22, y0 + 44, GREY, 2.0))
    b.append(label(lx + 28, y0 + 48, "worst pixel", "xs", "start"))

    b.append(hollow(lx - 6, y0 + 74, 180, 80, GREY, rx=4))
    b.append(label(lx + 84, y0 + 94, "THE STATISTIC MATTERS", "xs"))
    b.append(label(lx + 84, y0 + 110, "the MEAN over the image is", "xs"))
    b.append(label(lx + 84, y0 + 124, "already right at 1x, because", "xs"))
    b.append(label(lx + 84, y0 + 138, "opposite errors cancel", "xs"))

    b.append(label(W / 2, H - 62,
                   "A checker of period 0.37 px is far above Nyquist: its true local "
                   "mean is 0.5 and a point sample returns 0 or 1", "xs"))
    b.append(label(W / 2, H - 44,
                   "depending on where it lands. That is aliasing in one line. "
                   "Supersampling drives the per-pixel error from 0.5000", "xs"))
    b.append(label(W / 2, H - 26,
                   "to 0.0231 — but NOTHING recovers the checker, because it is not "
                   "representable on this grid at any sample count.", "xs"))
    b.append(label(W / 2, H - 8,
                   "The image stops LYING about being something else. That is all "
                   "antialiasing ever does.", "xs"))
    return svg("f614b", W, H, "Supersampling error against sample count",
               "A plot of per-pixel RMS deviation and worst-pixel deviation against "
               "supersampling factor for a checkerboard above the Nyquist limit.", b)


# ===========================================================================
# Figure 3 — the linear-light resolve                                   (§3.3)
# ===========================================================================
def fig_resolve():
    W, H = 700, 330
    b = [label(W / 2, 22, "Resolving a half-covered pixel: the fourth appearance of one mistake", "sm")]

    # The 2x2 block.
    bx, by, cell = 70, 66, 46
    b.append(label(bx + cell, by - 14, "4 samples", "xs"))
    for j in range(2):
        for i in range(2):
            white = (j == 0)
            b.append(box(bx + i * cell, by + j * cell, cell - 2, cell - 2,
                         GREY if white else BLUE, rx=2, opacity=0.85 if white else 0.25))
    b.append(label(bx + cell, by + 2 * cell + 20, "two white, two black", "xs"))

    b.append(arrow(bx + 2 * cell + 14, by + cell, bx + 2 * cell + 48, by + cell,
                   "f614c", "s", 1.3))

    # Two outcomes.
    ox = bx + 2 * cell + 66
    for k, (lbl, code, colour, note) in enumerate((
            ("average the LIGHT", RESOLVE_LINEAR, GREEN, "correct"),
            ("average the BYTES", RESOLVE_ENCODED, RED, "the bug"))):
        yy = by + k * 76
        b.append(hollow(ox, yy, 200, 60, colour, rx=4))
        b.append(label(ox + 100, yy + 22, lbl, "xs"))
        b.append(label(ox + 100, yy + 42, f"code {code}   ({note})", "xs"))

    b.append(hollow(ox + 218, by, 190, 136, GREY, rx=4))
    b.append(label(ox + 313, by + 24, "WHY 188", "xs"))
    b.append(label(ox + 313, by + 42, "half the light is 0.5", "xs"))
    b.append(label(ox + 313, by + 58, "sRGB(0.5) = 0.7354", "xs"))
    b.append(label(ox + 313, by + 74, "x 255 = 188", "xs"))
    b.append(label(ox + 313, by + 100, f"code {RESOLVE_ENCODED} delivers only", "xs"))
    b.append(label(ox + 313, by + 116, f"{RESOLVE_DELIVERED}% of the light", "xs"))

    b.append(label(W / 2, H - 60,
                   "The symptom is specific and usually misdiagnosed: antialiased "
                   "edges come out TOO DARK, so every silhouette", "xs"))
    b.append(label(W / 2, H - 42,
                   "grows a thin dark outline — and the usual guess is that edges "
                   "are being blended with the background twice.", "xs"))
    b.append(label(W / 2, H - 24,
                   "This is Lesson 6.1's rule arriving for the fourth time, after "
                   "6.10's mip chains and 6.11's compositing. It", "xs"))
    b.append(label(W / 2, H - 6,
                   "will keep arriving, because every new way of AVERAGING is a new "
                   "chance to average the wrong quantity.", "xs"))
    return svg("f614c", W, H, "Resolving a half-covered pixel in light and in bytes",
               "A two by two block half white and half black, resolved two ways: "
               "averaging linear light gives code 188, averaging stored bytes gives "
               "code 127.", b)


# ===========================================================================
# Figure 4 — the lobe, and where aliasing begins                        (§3.4)
# ===========================================================================
def fig_crossover():
    W, H = 720, 410
    b = [label(W / 2, 22, "Where shading aliasing begins: the lobe against the pixel", "sm")]

    x0, x1 = 110, 470
    y0, y1 = 58, 268

    def px(r):
        lo, hi = math.log10(0.04), math.log10(0.6)
        return x0 + (x1 - x0) * (math.log10(r) - lo) / (hi - lo)

    def py(v):
        lo, hi = math.log10(0.001), math.log10(0.3)
        return y1 - (y1 - y0) * (math.log10(max(v, 1e-4)) - lo) / (hi - lo)

    b.append(hollow(x0, y0, x1 - x0, y1 - y0, GREY, dash="3 3"))
    for r in (0.05, 0.10, 0.20, 0.50):
        b.append(rule(px(r), y1, px(r), y1 + 5, "grid", 1.0))
        b.append(label(px(r), y1 + 20, f"{r:.2f}", "xs"))
    b.append(label((x0 + x1) / 2, y1 + 42, "roughness", "xs"))
    for v, t in ((0.001, "0.001"), (0.01, "0.01"), (0.1, "0.1")):
        b.append(rule(x0 - 5, py(v), x0, py(v), "grid", 1.0))
        b.append(label(x0 - 10, py(v) + 4, t, "xs", "end"))
    b.append(label(x0 - 46, (y0 + y1) / 2, "radians", "xs"))

    # the lobe half-width
    pts = " ".join(f"{px(r):.1f},{py(w):.1f}" for r, w, _ in LOBES)
    b.append(f'<polyline points="{pts}" fill="none" stroke="{AMBER}" stroke-width="2.4"/>')
    for r, w, _ in LOBES:
        b.append(f'<circle cx="{px(r):.1f}" cy="{py(w):.1f}" r="3.2" fill="{AMBER}"/>')

    # the normal's turn per pixel — a horizontal line
    b.append(cline(x0, py(TURN_40PX), x1, py(TURN_40PX), BLUE, 2.0))
    b.append(label(x0 + 8, py(TURN_40PX) - 8,
                   f"the normal turns {TURN_40PX:.4f} rad per pixel", "xs", "start"))

    # the crossover
    b.append(rule(px(CROSSOVER), y0, px(CROSSOVER), y1, "grid", 1.2, dash="2 4"))
    b.append(label(px(CROSSOVER), y0 - 8, f"crossover {CROSSOVER}", "xs"))

    # region shading, OUTSIDE the curve labels
    b.append(box(x0, y1 - 16, px(CROSSOVER) - x0, 14, RED, rx=2, opacity=0.16))
    b.append(label((x0 + px(CROSSOVER)) / 2, y1 - 5, "ALIASES", "xs"))
    b.append(box(px(CROSSOVER), y1 - 16, x1 - px(CROSSOVER), 14, GREEN, rx=2, opacity=0.16))
    b.append(label((px(CROSSOVER) + x1) / 2, y1 - 5, "resolved", "xs"))

    lx = x1 + 26
    b.append(hollow(lx - 6, y0 + 10, 182, 96, GREY, rx=4))
    b.append(label(lx + 85, y0 + 30, "THE MODULE'S OWN", "xs"))
    b.append(label(lx + 85, y0 + 46, "ROUGHNESSES", "xs"))
    b.append(label(lx + 85, y0 + 64, "the demo: 0.49  resolved", "xs"))
    b.append(label(lx + 85, y0 + 80, "6.12's metals: 0.20, 0.10,", "xs"))
    b.append(label(lx + 85, y0 + 94, "0.05  all alias", "xs"))

    b.append(hollow(lx - 6, y0 + 122, 182, 78, AMBER, rx=4))
    b.append(label(lx + 85, y0 + 142, "THE LOBE, EXACTLY", "xs"))
    b.append(label(lx + 85, y0 + 158, "sin(t) = alpha x", "xs"))
    b.append(label(lx + 85, y0 + 172, "sqrt((sqrt2-1)/(1-alpha^2))", "xs"))
    b.append(label(lx + 85, y0 + 190, f"= {LOBE_COEFF:.4f} alpha, not alpha", "xs"))

    b.append(label(W / 2, H - 44,
                   "A highlight is safely sampled while the lobe is WIDER than the "
                   "normal's variation across a pixel. Below the crossover it", "xs"))
    b.append(label(W / 2, H - 26,
                   "can fall between sample points entirely — and no amount of "
                   "COVERAGE sampling reaches it, because MSAA shades once.", "xs"))
    b.append(label(W / 2, H - 8,
                   f"The folklore 'the lobe is about alpha wide' overstates it by "
                   f"{FOLKLORE_OVER}%.", "xs"))
    return svg("f614d", W, H, "Specular lobe width against normal variation per pixel",
               "A log-log plot of GGX lobe half-width against roughness, crossed by "
               "the constant normal variation per pixel, with the crossover marked.", b)


# ===========================================================================
# Figure 5 — MSAA's architecture                                        (§3.5)
# ===========================================================================
def fig_msaa():
    W, H = 720, 380
    b = [label(W / 2, 22, "MSAA and SSAA differ in exactly one thing: how often the shader runs", "sm")]

    def pixel(x, y, size, shades, title, sub, colour):
        out = [hollow(x, y, size, size, GREY, rx=3)]
        # four coverage sample points
        for j in range(2):
            for i in range(2):
                cx = x + size * (0.3 + 0.4 * i)
                cy = y + size * (0.3 + 0.4 * j)
                out.append(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="3.2" fill="{BLUE}"/>')
        # shading evaluations
        for k in range(shades):
            cx = x + size * (0.5 if shades == 1 else (0.3 + 0.4 * (k % 2)))
            cy = y + size * (0.5 if shades == 1 else (0.3 + 0.4 * (k // 2)))
            out.append(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="7.5" fill="none" '
                       f'stroke="{colour}" stroke-width="2.0"/>')
        out.append(label(x + size / 2, y + size + 20, title, "xs"))
        out.append(label(x + size / 2, y + size + 36, sub, "xs"))
        return out

    b.extend(pixel(130, 60, 96, 1, "MSAA", "4 coverage, 1 shade", AMBER))
    b.extend(pixel(330, 60, 96, 4, "SSAA", "4 coverage, 4 shades", GREEN))

    b.append(f'<circle cx="510" cy="76" r="3.2" fill="{BLUE}"/>')
    b.append(label(524, 80, "coverage + depth sample", "xs", "start"))
    b.append(f'<circle cx="510" cy="102" r="7.5" fill="none" stroke="{AMBER}" stroke-width="2.0"/>')
    b.append(label(524, 106, "shader evaluation", "xs", "start"))
    b.append(label(524, 132, "MSAA costs ~1.3x", "xs", "start"))
    b.append(label(524, 148, "SSAA costs 4x", "xs", "start"))

    # the measured gap
    ty = 210
    b.append(label(W / 2, ty, "on an INTERIOR pixel — one primitive, no silhouette:", "xs"))
    b.append(label(150, ty + 24, "roughness", "xs"))
    b.append(label(280, ty + 24, "MSAA", "xs"))
    b.append(label(400, ty + 24, "SSAA", "xs"))
    b.append(label(530, ty + 24, "ratio", "xs"))
    for k, (r, m, s, ratio) in enumerate(MSAA_SSAA):
        yy = ty + 44 + k * 18
        b.append(label(150, yy, f"{r:.2f}", "xs"))
        b.append(label(280, yy, f"{m:,.2f}", "xs"))
        b.append(label(400, yy, f"{s:,.2f}", "xs"))
        b.append(label(530, yy, f"{ratio:.3f}x", "xs"))

    b.append(label(W / 2, H - 44,
                   "THE GAP BETWEEN THE TWO COLUMNS IS THE SHADING ALIASING, and "
                   "MSAA cannot reach it by construction. Shading once", "xs"))
    b.append(label(W / 2, H - 26,
                   "per primitive per pixel is not a shortcut MSAA takes — it is "
                   "what MSAA IS, and what makes it affordable.", "xs"))
    b.append(label(W / 2, H - 8,
                   "At roughness 0.05 the two answers differ by a factor of 300.", "xs"))
    return svg("f614e", W, H, "MSAA against SSAA sampling patterns",
               "Two pixels showing four coverage samples each, one with a single "
               "shader evaluation and one with four, beside the measured difference "
               "in shaded value at four roughnesses.", b)


# ===========================================================================
# Figure 6 — accuracy against stability                                 (§4)
# ===========================================================================
def fig_stability():
    W, H = 760, 400
    b = [label(W / 2, 22, "What filtering the NDF actually buys, and what it does not", "sm")]

    # Two bar groups, log scale on the swing.
    gx, gy, gw = 96, 66, 190
    b.append(label(gx + gw / 2, gy - 14, "per-pixel ACCURACY (RMS error)", "xs"))
    b.append(label(gx + gw + 150 + gw / 2, gy - 14, "STABILITY (sub-pixel swing)", "xs"))

    maxrms = 240.0
    for k, (r, _fr, nr, fr_rms, ns, fs) in enumerate(SPEC):
        yy = gy + 16 + k * 42
        b.append(label(gx - 14, yy + 14, f"{r:.2f}", "xs", "end"))
        b.append(box(gx, yy, gw * nr / maxrms, 12, RED, rx=1, opacity=0.55))
        b.append(label(gx + gw * nr / maxrms + 6, yy + 10, f"{nr:.1f}%", "xs", "start"))
        b.append(box(gx, yy + 15, gw * fr_rms / maxrms, 12, GREEN, rx=1, opacity=0.55))
        b.append(label(gx + gw * fr_rms / maxrms + 6, yy + 25, f"{fr_rms:.1f}%", "xs", "start"))

        # swing, log scale
        sx = gx + gw + 150
        def lg(v):
            return gw * min(math.log10(max(v, 1.0)) / math.log10(4000.0), 1.0)
        b.append(box(sx, yy, lg(ns), 12, RED, rx=1, opacity=0.55))
        b.append(label(sx + lg(ns) + 6, yy + 10, f"{ns:,.1f}x", "xs", "start"))
        b.append(box(sx, yy + 15, lg(fs), 12, GREEN, rx=1, opacity=0.55))
        b.append(label(sx + lg(fs) + 6, yy + 25, f"{fs:.1f}x", "xs", "start"))

    b.append(label(gx + gw / 2, gy + 16 + 4 * 42 + 8, "linear scale", "xs"))
    b.append(label(gx + gw + 150 + gw / 2, gy + 16 + 4 * 42 + 8, "LOG scale", "xs"))

    # legend, outside
    b.append(box(gx, gy + 16 + 4 * 42 + 24, 14, 10, RED, rx=1, opacity=0.55))
    b.append(label(gx + 20, gy + 16 + 4 * 42 + 33, "unfiltered", "xs", "start"))
    b.append(box(gx + 100, gy + 16 + 4 * 42 + 24, 14, 10, GREEN, rx=1, opacity=0.55))
    b.append(label(gx + 120, gy + 16 + 4 * 42 + 33, "NDF filtered", "xs", "start"))

    b.append(hollow(80, H - 92, 600, 62, AMBER, rx=4))
    b.append(label(380, H - 72,
                   f"At roughness 0.05 the filter improves ACCURACY by {RMS_GAIN}x "
                   f"and STABILITY by {SWING_GAIN:,}x. At 0.30 and 0.20 it makes the", "xs"))
    b.append(label(380, H - 56,
                   "accuracy WORSE. That is not a defect — it is what antialiasing "
                   "is: the removal of frequencies the grid", "xs"))
    b.append(label(380, H - 40,
                   "cannot carry, paid for in detail. The artefact was never a "
                   "wrong pixel; it was a pixel that moved.", "xs"))

    b.append(label(W / 2, H - 8,
                   "Which is why the question to ask of an AA technique is not "
                   "\"how accurate is it\".", "xs"))
    return svg("f614f", W, H, "Accuracy against stability for NDF filtering",
               "Paired bars at four roughnesses comparing per-pixel RMS error and "
               "sub-pixel swing, unfiltered against NDF-filtered.", b)


# ===========================================================================
# Figure 7 — the MSAA frame                                             (§5)
# ===========================================================================
def fig_frame():
    W, H = 780, 360
    b = [label(W / 2, 22, "What MSAA costs, and the two targets an MSAA frame needs", "sm")]

    # THE ROW IS COMPUTED, NOT HAND-PLACED. The first version put the four
    # stages at hand-picked offsets and the last box ran off the right edge —
    # which check-page.js could not see until 6.14 taught it to test SHAPES as
    # well as text. Deriving the positions from the canvas width makes the
    # overflow impossible rather than merely detected.
    y = 70
    bh = 66
    stages = [("scene pass", "writes 4x target", f"{MSAA_MB} MB", RED),
              ("resolved target", "1x, sampleable", f"{HDR_MB} MB", GREEN),
              ("bloom (6.13)", "reads the RESOLVED", "one, never the 4x", BLUE),
              ("tonemap", "to the display", "", PURPLE)]
    gap = 46
    margin = 26
    bw = (W - 2 * margin - 3 * gap) / 4.0

    def stage(x, title, sub, sub2, colour):
        out = [hollow(x, y, bw, bh, colour, rx=4)]
        out.append(label(x + bw / 2, y + 22, title, "xs"))
        out.append(label(x + bw / 2, y + 40, sub, "xs"))
        if sub2:
            out.append(label(x + bw / 2, y + 56, sub2, "xs"))
        return out

    for k, (t, s1, s2, c) in enumerate(stages):
        x = margin + k * (bw + gap)
        b.extend(stage(x, t, s1, s2, c))
        if k > 0:
            b.append(arrow(x - gap + 6, y + bh / 2, x - 6, y + bh / 2,
                           "f614g", "i" if k == 1 else "s", 1.5 if k == 1 else 1.3))
    # the resolve is the one transition worth naming
    b.append(label(margin + bw + gap / 2, y + bh / 2 - 12, "RESOLVE", "xs"))

    b.append(hollow(48, 176, 340, 96, GREY, rx=4))
    b.append(label(218, 196, "THE THINGS THAT MUST MATCH", "xs"))
    b.append(label(218, 214, "colour target, depth target and EVERY", "xs"))
    b.append(label(218, 230, "pipeline share one sample count — a 4x", "xs"))
    b.append(label(218, 246, "colour target beside a 1x depth target", "xs"))
    b.append(label(218, 262, "is a pass that cannot be begun", "xs"))

    b.append(hollow(400, 176, 330, 96, AMBER, rx=4))
    b.append(label(565, 196, "THE COSTS", "xs"))
    b.append(label(565, 214, f"the 4x target is {MSAA_MB / HDR_MB:.0f}x the memory", "xs"))
    b.append(label(565, 230, f"the pipeline count goes 13 -> {PIPELINES}", "xs"))
    b.append(label(565, 246, "8x is UNSUPPORTED on this machine's", "xs"))
    b.append(label(565, 262, "float format — so the device is ASKED", "xs"))

    b.append(label(W / 2, H - 44,
                   "A multisample texture cannot be sampled AT ALL, which is why an "
                   "MSAA frame needs two colour targets where a", "xs"))
    b.append(label(W / 2, H - 26,
                   "plain one needs one — and why 6.13's ownership rule needed no "
                   "rewrite: the RESOLVED target is what crosses", "xs"))
    b.append(label(W / 2, H - 8,
                   "between stages, and the multisample one is an intermediate "
                   "nothing downstream ever sees.", "xs"))
    return svg("f614g", W, H, "The shape and cost of an MSAA frame",
               "A four-stage frame from the multisample scene pass through the "
               "resolve into the bloom and tonemap, with the matching requirements "
               "and costs listed below.", b)


def main():
    figs = [("l614_fig1.svg", fig_two()),
            ("l614_fig2.svg", fig_nyquist()),
            ("l614_fig3.svg", fig_resolve()),
            ("l614_fig4.svg", fig_crossover()),
            ("l614_fig5.svg", fig_msaa()),
            ("l614_fig6.svg", fig_stability()),
            ("l614_fig7.svg", fig_frame())]
    for name, body in figs:
        path = os.path.join(OUT, name)
        with open(path, "w") as fh:
            fh.write(body)
        print(f"wrote {path}  ({len(body):,} bytes)")


if __name__ == "__main__":
    main()
