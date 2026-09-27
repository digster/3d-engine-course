#!/usr/bin/env python3
"""scratch/figs_78.py — Lesson 7.8's diagrams.

Same rules as 5.1-7.7's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - filenames numbered by PAGE ORDER
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - `rule()` takes a CSS CLASS; `cline()` takes a COLOUR
  - LEGENDS AND ANNOTATION BOXES GO OUTSIDE THE PLOT
  - a SHAPE can leave the viewBox where a label cannot
  - panel heights are COMPUTED, never guessed
  - the figure palette must contain the DEMO's own colours (rle_rects SNAPS)

Every number below is transcribed from scratch/l78_verify_out.txt or from
scratch/l78_devcheck_out.txt. Nothing here is estimated, and the section of the
harness each block came from is named above it.

ONE NEW RULE, AND THIS LESSON IS WHERE IT WAS NEEDED. Several of these figures
draw WAVEFORMS, which are the first figures in this course whose vertical axis is
signed and centred on zero. A waveform plotted with the usual bottom-up axis
looks fine and is unreadable: the eye cannot find the zero line, which is the one
value that matters (a discontinuity is measured FROM it). Every waveform panel
below draws its zero line first, in `grid`, and every amplitude is measured
against it.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)
from figs_610 import cline                                          # noqa: E402
from figs_45 import read_ppm, rle_rects, hexrgb, box_sample         # noqa: E402
from figs_511 import peak_sample                                    # noqa: E402
from figs_71 import cmarker, carrow, poly, frame, D                 # noqa: E402
from figs_76 import table, f, fmt_e                                 # noqa: E402

OUT = "scratch"

# The demo's own palette, so that `rle_rects`' nearest-colour snap has the right
# targets. Lesson 4.5's finding, reconfirmed every time: quantising per channel
# instead of snapping to a palette produces no runs at all and a file four times
# the size. These are demos/audio/main.cpp's constants, transcribed.
A_SIREN = "#ffbe5a"
A_FLYBY = "#78dca0"
A_MUSIC = "#aa96ff"
A_LISTENER = "#ebeef5"
A_FORWARD = "#6eaaff"
A_RIGHT = "#eb6060"
A_GRID = "#262a34"
AUDIO_PALETTE = [hexrgb(c) for c in (A_SIREN, A_FLYBY, A_MUSIC, A_LISTENER,
                                     A_FORWARD, A_RIGHT, A_GRID)]


def render_panel(ppm, crop, x, y, px, cell=4, levels=3, peak=True):
    """A real render, downsampled and run-length encoded to rectangles."""
    _w, _h, data = read_ppm(os.path.join(OUT, ppm))
    sampler = peak_sample if peak else box_sample
    grid_w, grid_h, grid = sampler(data, _w, crop, cell)
    body = rle_rects(grid, grid_w, grid_h, x, y, px, AUDIO_PALETTE, levels=levels,
                     bg_class="fill-shot")
    return (['<g shape-rendering="crispEdges">'] + body + ['</g>'],
            grid_w * px, grid_h * px)


# ===========================================================================
# Measured — every figure's data, with the section of the harness it came from
# ===========================================================================

# §C — uncorrelated. (N, peak, rms, peak/a, rms/a)
HEADROOM = [(1, 0.0500, 0.0289, 1.00, 0.58), (2, 0.0999, 0.0410, 2.00, 0.82),
            (4, 0.1773, 0.0580, 3.55, 1.16), (8, 0.3006, 0.0822, 6.01, 1.64),
            (16, 0.3838, 0.1158, 7.68, 2.32), (32, 0.6311, 0.1622, 12.62, 3.24),
            (64, 0.8449, 0.2313, 16.90, 4.63)]
# §C — CONTROL, correlated. (N, peak/a)
CORRELATED = [(1, 1.00), (2, 2.00), (4, 4.00), (8, 8.00), (16, 16.00)]
# §C — 16 voices, peak against how long you listen. (seconds, peak/a)
WINDOW = [(0.01, 6.98), (0.10, 8.45), (1.00, 10.09), (8.00, 10.46)]
CLIP32 = (33, 40960, 1.2675)

# §D — clipping. (master gain, peak, clipped, H3 dB, H5 dB, H7 dB, THD %)
CLIPPING = [(1.00, 0.500, 0, -166.8, -160.3, -159.7, 0.00),
            (1.50, 0.750, 0, -159.5, -152.7, -155.5, 0.00),
            (2.00, 1.000, 0, -166.8, -160.3, -159.7, 0.00),
            (3.00, 1.000, 27731, -16.5, -35.7, -32.5, 15.22),
            (6.00, 1.000, 40532, -11.0, -18.5, -27.5, 31.00)]

# §E — the pan laws. (pan, linear L, linear R, lin power dB, cp L, cp R)
PANS = [(-1.00, 1.0000, 0.0000, 0.00, 1.0000, 0.0000),
        (-0.50, 0.7500, 0.2500, -2.04, 0.9239, 0.3827),
        (0.00, 0.5000, 0.5000, -3.01, 0.7071, 0.7071),
        (0.50, 0.2500, 0.7500, -2.04, 0.3827, 0.9239),
        (1.00, 0.0000, 1.0000, 0.00, 0.0000, 1.0000)]
LIN_WORST, CP_WORST = 3.010, 0.000

# §E.4 — an emitter orbiting a listener. (degrees, pan, L, R)
ORBIT = [(0, 0.00, 0.707, 0.707), (45, 0.71, 0.228, 0.974),
         (90, 1.00, 0.000, 1.000), (135, 0.71, 0.228, 0.974),
         (180, 0.00, 0.707, 0.707), (225, -0.71, 0.974, 0.228),
         (270, -1.00, 1.000, 0.000), (315, -0.71, 0.974, 0.228)]

# §F — the curves. (d, inverse, linear, ranged, one-over-d-squared) as gains
FALLOFF = [(0.5, 1.0000, 1.0000, 1.0000, 1.0000), (1.0, 1.0000, 1.0000, 1.0000, 1.0000),
           (2.0, 0.5000, 0.9796, 0.4898, 0.2500), (4.0, 0.2500, 0.9388, 0.2347, 0.0625),
           (8.0, 0.1250, 0.8571, 0.1071, 0.0156), (16.0, 0.0625, 0.6939, 0.0434, 0.0039),
           (32.0, 0.0312, 0.3673, 0.0115, 0.0010), (49.0, 0.0204, 0.0204, 0.000416, 0.0004),
           (50.0, 0.0200, 0.0000, 0.0000, 0.0004), (60.0, 0.0200, 0.0000, 0.0000, 0.0003)]
PER_DOUBLING_INV, PER_DOUBLING_SQ = -6.02, -12.04
CLIFF_GAIN, CLIFF_DB = 0.0200, -33.98
# §F.5 — the worked example
WORKED = dict(pos=(3.0, 0.0, -4.0), gain=0.8, dist=5.0000, atten=0.1837,
              pan=0.6000, left=0.0454, right=0.1397, power_db=-16.66)

# §G — the fly-by, near the pass. (t, x, dist, L, R, dL)
FLYBY = [(1.967, -0.67, 1.202, 0.7790, 0.2841, 0.0954),
         (1.983, -0.33, 1.054, 0.8144, 0.4849, 0.0354),
         (2.000, 0.00, 1.000, 0.7071, 0.7071, 0.1073),
         (2.017, 0.33, 1.054, 0.4849, 0.8144, 0.2222),
         (2.033, 0.67, 1.202, 0.2841, 0.7790, 0.2008),
         (2.050, 1.00, 1.414, 0.1601, 0.6836, 0.1240)]
WORST_DL = 0.2222
OWN_SLOPE, STEP_JUMP, RAMP_JUMP = 0.013088, 0.100745, 0.013072
STEP_RATIO, RAMP_RATIO = 7.70, 1.00
ERR_RMS, ERR_AT_TONE, ERR_TONE_PCT = 2.608e-02, 2.847e-03, 1.2
CONST_CONTROL = 8.941e-09
FADE = dict(first=0.2500000, hard=0.2500000, faded=0.0065443, own=0.0065450,
            hard_x=38.2, faded_x=1.00, samples=240)

# §H — resampling. (label, SNR dB)
RESAMPLE = [("ours (linear, 2 taps)", 62.40), ("SDL_ConvertAudioSamples", 85.05),
            ("no resample at all", 91.67)]
# §H.3 — ours, by frequency. (Hz, SNR dB)
BY_FREQ = [(200, 89.00), (1000, 62.40), (4000, 38.01), (10000, 20.43)]

# §I — the budget. (voices, us/buffer, % of budget, ns/voice/frame)
BUDGET = [(0, 0.619, 0.01, 0.00), (1, 1.032, 0.01, 2.01), (4, 2.304, 0.02, 1.13),
          (16, 7.957, 0.07, 0.97), (32, 15.056, 0.14, 0.92), (64, 29.761, 0.28, 0.91)]
BUDGET_US = 10666.7
SET_GAIN_NS = 9.0
LOG_FMT_NS, LOG_IO_NS = 82.0, 1021.6
# §I.5 — (frames, latency ms, callbacks/s)
LATENCY = [(128, 2.67, 375.0), (256, 5.33, 187.5), (512, 10.67, 93.8),
           (1024, 21.33, 46.9), (2048, 42.67, 23.4)]

# devcheck_78, on a real device
DEV = dict(freq=44100, frames=512, budget_us=11609.98, buffers=58, late=0,
           queue_empty=28, clipped=0, peak=0.0141, worst_us=11.29, load_pct=0.097)


# ===========================================================================
# Figure 1 — a sample is a measurement, and the device wants one every 20.8 us
# ===========================================================================
def fig1():
    uid = "l78f1"
    W, H = 900, 380
    b = [cmarker(uid, "amber", AMBER), cmarker(uid, "blue", BLUE),
         cmarker(uid, "grey", GREY)]

    # ---- left: the pressure wave and its samples -------------------------
    px, py, pw, ph = 46, 54, 400, 200
    zero = py + ph / 2
    b.append(frame(px, py, pw, ph))
    b.append(rule(px, zero, px + pw, zero, cls="grid"))
    b.append(label(px, py - 14, "a pressure wave, and 24 measurements of it",
                   cls="xs muted", anchor="start"))
    b.append(label(px - 8, zero + 4, "0", cls="xs muted", anchor="end"))
    b.append(label(px - 8, py + 10, "+1", cls="xs muted", anchor="end"))
    b.append(label(px - 8, py + ph, "-1", cls="xs muted", anchor="end"))

    def wave(t):
        return 0.62 * math.sin(2 * math.pi * 1.6 * t) + 0.22 * math.sin(2 * math.pi * 4.1 * t)

    pts = []
    for i in range(361):
        t = i / 360.0
        pts.append((px + t * pw, zero - wave(t) * (ph / 2 - 6)))
    b.append(poly(pts, GREY, width=1.2, close=False))

    n = 24
    for i in range(n + 1):
        t = i / float(n)
        x = px + t * pw
        y = zero - wave(t) * (ph / 2 - 6)
        b.append(cline(x, zero, x, y, AMBER, width=1.0))
        b.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="2.6" fill="{AMBER}"/>')

    # the interval, called out between two samples
    x0 = px + (8 / float(n)) * pw
    x1 = px + (9 / float(n)) * pw
    b.append(carrow(x0, py + ph + 18, x1, py + ph + 18, uid, "amber", AMBER, width=1.2))
    b.append(carrow(x1, py + ph + 18, x0, py + ph + 18, uid, "amber", AMBER, width=1.2))
    b.append(label((x0 + x1) / 2, py + ph + 34, "1 / 48000 s = 20.83 us",
                   cls="xs mono"))
    b.append(label(px + pw / 2, py + ph + 54,
                   "the samples ARE the sound. There is nothing else in the file.",
                   cls="xs muted"))

    # ---- right: what one second costs ------------------------------------
    tx = 516
    rows = [("1 frame", "1 sample per channel"),
            ("48,000", "frames per second"),
            ("2", "channels, interleaved: L R L R"),
            ("4 bytes", "one float sample"),
            ("384 KB", "one second of stereo"),
            ("22 MB", "one minute of music")]
    el, hgt = table(tx, 44, ["", ""], rows, [90, 268],
                    title="the arithmetic, once")
    b += el

    b.append(rule(tx, 44 + hgt + 10, tx + 358, 44 + hgt + 10, cls="grid"))
    b.append(label(tx, 44 + hgt + 32,
                   "A device consumes frames on a clock it owns.", cls="xs", anchor="start"))
    b.append(label(tx, 44 + hgt + 50,
                   "Nothing you do changes the rate; you can only", cls="xs muted",
                   anchor="start"))
    b.append(label(tx, 44 + hgt + 66,
                   "be ready or be late.", cls="xs muted", anchor="start"))

    return svg(uid, W, H,
               "A pressure wave sampled 24 times, and the arithmetic of one second of audio",
               "Left: a continuous pressure wave with 25 sample points marked, one every "
               "20.83 microseconds at 48 kHz. Right: a table of the sizes involved, from one "
               "frame to 22 MB for a minute of stereo music.", b)


# ===========================================================================
# Figure 2 — the two clocks
# ===========================================================================
def fig2():
    uid = "l78f2"
    W, H = 900, 400
    b = [cmarker(uid, "amber", AMBER), cmarker(uid, "blue", BLUE),
         cmarker(uid, "red", RED)]

    left, right = 60, 840
    span = right - left

    # ---- the frame clock: variable ---------------------------------------
    fy = 76
    b.append(label(left, fy - 42, "the simulation clock: 60 Hz nominal, and it slips",
                   cls="xs muted", anchor="start"))
    b.append(rule(left, fy, right, fy, cls="grid"))
    # deliberately uneven frames, one of them long
    frames = [0.0, 0.098, 0.196, 0.294, 0.392, 0.62, 0.72, 0.82, 0.92, 1.0]
    for i, t in enumerate(frames):
        x = left + t * span
        b.append(cline(x, fy - 12, x, fy + 12, BLUE, width=1.6))
        if i < len(frames) - 1:
            mid = left + (t + frames[i + 1]) / 2 * span
            wide = frames[i + 1] - t > 0.15
            b.append(label(mid, fy - 18, "16" if not wide else "38",
                           cls="xs mono t-hi" if wide else "xs mono"))
    b.append(label(right + 4, fy + 4, "ms", cls="xs muted", anchor="start"))
    hitch_x0 = left + 0.392 * span
    hitch_x1 = left + 0.62 * span
    b.append(box(hitch_x0, fy + 16, hitch_x1 - hitch_x0, 20, RED, opacity=0.14, rx=2))
    b.append(label((hitch_x0 + hitch_x1) / 2, fy + 31,
                   "a hitch. Nobody dies.", cls="xs"))

    # ---- the device clock: fixed -----------------------------------------
    dy = 208
    b.append(label(left, dy - 26, "the device clock: 512 frames every 10.67 ms, forever",
                   cls="xs muted", anchor="start"))
    b.append(rule(left, dy, right, dy, cls="grid"))
    n = 16
    for i in range(n + 1):
        x = left + (i / float(n)) * span
        b.append(cline(x, dy - 10, x, dy + 10, AMBER, width=1.6))
    for i in range(n):
        x0 = left + (i / float(n)) * span
        x1 = left + ((i + 1) / float(n)) * span
        ok = not (6 <= i <= 7)
        b.append(box(x0 + 2, dy - 8, x1 - x0 - 4, 16,
                     AMBER if ok else RED, opacity=0.20 if ok else 0.35, rx=2))
    gap0 = left + (6 / float(n)) * span
    gap1 = left + (8 / float(n)) * span
    b.append(carrow((gap0 + gap1) / 2, dy + 64, (gap0 + gap1) / 2, dy + 22, uid, "red", RED))
    b.append(label((gap0 + gap1) / 2, dy + 82,
                   "two buffers the mixer did not deliver", cls="xs"))
    b.append(label((gap0 + gap1) / 2, dy + 98,
                   "21 ms of silence with a step at each end: a CLICK", cls="xs t-hi"))

    # ---- the slack -------------------------------------------------------
    sy = 330
    b.append(rule(left, sy, right, sy, cls="grid"))
    b.append(label(left, sy - 12, "the queue is the whole of your margin",
                   cls="xs muted", anchor="start"))
    for i, (frames_n, ms, _cb) in enumerate(LATENCY):
        x = left + (i + 0.5) * span / len(LATENCY)
        w = ms / LATENCY[-1][1] * (span / len(LATENCY)) * 0.8
        b.append(box(x - w / 2, sy + 8, w, 14, BLUE, opacity=0.35, rx=2))
        b.append(label(x, sy + 38, f"{frames_n}", cls="xs mono"))
        b.append(label(x, sy + 54, f"{ms:.2f} ms", cls="xs muted"))

    return svg(uid, W, H,
               "Two clocks: a slipping 60 Hz simulation above, a fixed device clock below",
               "The simulation's frames vary in length and a long one is merely a hitch. The "
               "device consumes a fixed buffer every 10.67 ms and two undelivered buffers "
               "become 21 ms of silence with a discontinuity at each end. A row of bars shows "
               "buffer size against latency.", b)


# ===========================================================================
# Figure 3 — headroom: N voices are sqrt(N) loud
# ===========================================================================
def fig3():
    uid = "l78f3"
    W, H = 900, 430
    b = [cmarker(uid, "amber", AMBER), cmarker(uid, "red", RED)]

    px, py, pw, ph = 62, 56, 420, 280
    b.append(frame(px, py, pw, ph))
    b.append(label(px, py - 16, "growth against voice count, both axes log2",
                   cls="xs muted", anchor="start"))

    def gx(n):
        return px + math.log2(n) / 6.0 * pw

    def gy(v):
        # 1 at the bottom, 64 at the top — the SAME span as the x axis, so the
        # `N` reference is a true diagonal and "below the diagonal" means
        # "sublinear" by eye rather than by arithmetic.
        return py + ph - math.log2(max(v, 1.0)) / 6.0 * ph

    for n in (1, 2, 4, 8, 16, 32, 64):
        b.append(rule(gx(n), py, gx(n), py + ph, cls="grid"))
        b.append(label(gx(n), py + ph + 16, str(n), cls="xs mono"))
    for v in (1, 2, 4, 8, 16, 32, 64):
        b.append(rule(px, gy(v), px + pw, gy(v), cls="grid"))
        b.append(label(px - 8, gy(v) + 4, f"{v}x", cls="xs muted", anchor="end"))
    b.append(label(px + pw / 2, py + ph + 36, "voices mixed", cls="xs muted"))

    # reference lines: N and sqrt(N)
    # HEAVY, and drawn first: the measured curves land ON these, so a hairline
    # reference disappears under the line that is supposed to be confirming it.
    b.append(poly([(gx(n), gy(n)) for n in (1, 64)], GREY, width=4.0,
                  dash="8 5", close=False))
    b.append(poly([(gx(n), gy(math.sqrt(n))) for n in (1, 64)], GREY, width=4.0,
                  dash="8 5", close=False))
    b.append(label(gx(22), gy(40), "N", cls="xs mono muted"))
    b.append(label(gx(30), gy(3.1), "sqrt(N)", cls="xs mono muted"))

    # measured
    b.append(poly([(gx(n), gy(pk)) for n, _p, _r, pk, _rr in HEADROOM], AMBER,
                  width=1.8, close=False))
    b.append(poly([(gx(n), gy(rr / 0.5774)) for n, _p, _r, _pk, rr in HEADROOM], BLUE,
                  width=1.8, close=False))
    for n, _p, _r, pk, rr in HEADROOM:
        b.append(f'<circle cx="{gx(n):.1f}" cy="{gy(pk):.1f}" r="3" fill="{AMBER}"/>')
        b.append(f'<circle cx="{gx(n):.1f}" cy="{gy(rr / 0.5774):.1f}" r="3" fill="{BLUE}"/>')
    # the correlated control sits exactly on N
    b.append(poly([(gx(n), gy(pk)) for n, pk in CORRELATED], RED, width=1.6,
                  dash="6 3", close=False))
    for n, pk in CORRELATED:
        b.append(f'<circle cx="{gx(n):.1f}" cy="{gy(pk):.1f}" r="2.6" fill="{RED}"/>')

    # legend, OUTSIDE the plot
    ly = py + ph + 54
    for i, (col, text) in enumerate([
            (BLUE, "rms of N uncorrelated voices - lands on sqrt(N) exactly"),
            (AMBER, "peak of the same mix - above sqrt(N), well below N"),
            (RED, "CONTROL: the SAME voice N times - lands on N, exactly")]):
        b.append(cline(px, ly + i * 18 - 4, px + 22, ly + i * 18 - 4, col, width=2.4))
        b.append(label(px + 30, ly + i * 18, text, cls="xs", anchor="start"))

    # ---- right: the peak is a bet --------------------------------------
    tx = 528
    rows = [(f"{s:g} s", f"{v:.2f}x") for s, v in WINDOW]
    el, hgt = table(tx, 56, ["listened for", "peak / a"], rows, [150, 110],
                    title="16 voices, and the peak grows with the window")
    b += el
    y = 56 + hgt + 24
    b.append(label(tx, y, "The rms is a property of the signals.", cls="xs", anchor="start"))
    b.append(label(tx, y + 17, "The peak is the largest coincidence that", cls="xs muted",
                   anchor="start"))
    b.append(label(tx, y + 33, "happened to occur, and a longer window", cls="xs muted",
                   anchor="start"))
    b.append(label(tx, y + 49, "holds more chances. Headroom is a BET.", cls="xs t-hi",
                   anchor="start"))

    rows2 = [("1 / N", "0.0625", "-24.08 dB"),
             ("1 / sqrt(N)", "0.2500", "-12.04 dB"),
             ("none", "1.0000", f"{CLIP32[0]} clipped")]
    el2, _h2 = table(tx, y + 74, ["headroom for 16", "gain", ""], rows2, [150, 100, 110])
    b += el2

    return svg(uid, W, H,
               "Peak and RMS against voice count, with N and sqrt(N) reference lines",
               "On log-log axes the RMS of N uncorrelated voices lies exactly on sqrt(N) while "
               "the peak lies above it and well below N. A control mixing the same voice N "
               "times lands exactly on N. A table shows the peak of 16 voices rising from "
               "6.98x to 10.46x as the observation window grows from 10 ms to 8 s.", b)


# ===========================================================================
# Figure 4 — what clipping does
# ===========================================================================
def fig4():
    uid = "l78f4"
    W, H = 900, 380
    b = [cmarker(uid, "red", RED), cmarker(uid, "amber", AMBER)]

    px, py, pw, ph = 52, 58, 380, 190
    zero = py + ph / 2
    b.append(frame(px, py, pw, ph))
    b.append(rule(px, zero, px + pw, zero, cls="grid"))
    b.append(label(px, py - 16, "a 0.5 sine at master gain 3.0, before and after the clamp",
                   cls="xs muted", anchor="start"))

    amp = ph / 2 - 10
    for sign, name in ((1, "+1"), (-1, "-1")):
        y = zero - sign * amp * (1.0 / 1.5)
        b.append(rule(px, y, px + pw, y, cls="grid"))
        b.append(label(px - 8, y + 4, name, cls="xs muted", anchor="end"))

    unclipped = []
    clipped = []
    for i in range(401):
        t = i / 400.0
        v = 1.5 * math.sin(2 * math.pi * 2.0 * t)   # 0.5 x 3.0 = 1.5
        unclipped.append((px + t * pw, zero - v / 1.5 * amp))
        c = max(-1.0, min(1.0, v))
        clipped.append((px + t * pw, zero - c / 1.5 * amp))
    b.append(poly(unclipped, GREY, width=1.2, dash="4 3", close=False))
    b.append(poly(clipped, RED, width=2.0, close=False))
    b.append(label(px + pw / 2, py + ph + 20,
                   "the flat tops are the distortion. They are straight lines,",
                   cls="xs muted"))
    b.append(label(px + pw / 2, py + ph + 36,
                   "and a straight line is not a sine: it is a sine plus harmonics.",
                   cls="xs muted"))

    # ---- right: the harmonics, measured ---------------------------------
    hx, hy, hw, hh = 512, 58, 330, 190
    b.append(frame(hx, hy, hw, hh))
    b.append(label(hx, hy - 16, "harmonics relative to the fundamental, measured",
                   cls="xs muted", anchor="start"))
    # dB axis from -60 (bottom) to 0 (top)
    for db in (0, -20, -40, -60):
        y = hy + (-db / 60.0) * hh
        b.append(rule(hx, y, hx + hw, y, cls="grid"))
        b.append(label(hx - 8, y + 4, f"{db}", cls="xs muted", anchor="end"))
    b.append(label(hx - 30, hy + hh / 2, "dB", cls="xs muted"))

    groups = [(3.00, AMBER), (6.00, RED)]
    slot = hw / 8.0
    for gi, (gain, col) in enumerate(groups):
        row = next(r for r in CLIPPING if abs(r[0] - gain) < 1e-6)
        for hi, db in enumerate((row[3], row[4], row[5])):
            x = hx + (hi * 2 + 1) * slot + (gi - 0.5) * slot * 0.7
            top = hy + (min(0.0, -db) / 60.0) * hh
            top = hy + (-db / 60.0) * hh
            top = max(hy + 2, top)
            b.append(box(x - slot * 0.30, top, slot * 0.60, hy + hh - top, col,
                         opacity=0.55, rx=1))
    for hi, name in enumerate(("3rd", "5th", "7th")):
        b.append(label(hx + (hi * 2 + 1) * slot, hy + hh + 16, name, cls="xs muted"))

    ly = hy + hh + 40
    for i, (col, text) in enumerate([(AMBER, "gain 3.0 - 15.22% THD, 27,731 of 102,400 samples clipped"),
                                     (RED, "gain 6.0 - 31.00% THD")]):
        b.append(cline(hx, ly + i * 18 - 4, hx + 20, ly + i * 18 - 4, col, width=5))
        b.append(label(hx + 28, ly + i * 18, text, cls="xs", anchor="start"))
    b.append(label(hx, ly + 2 * 18 + 20,
                   "At gain 2.0 the peak is exactly 1.000 and nothing clips:",
                   cls="xs muted", anchor="start"))
    b.append(label(hx, ly + 2 * 18 + 36,
                   "the harmonics there are the measurement's own floor.",
                   cls="xs muted", anchor="start"))

    return svg(uid, W, H,
               "A clipped sine and the harmonics the clipping adds",
               "Left: a sine at three times full scale, drawn dashed, and the same sine after "
               "the clamp, with flat tops. Right: bars showing the third, fifth and seventh "
               "harmonics at master gains of 3 and 6, reaching -16.5 dB and -11.0 dB relative "
               "to the fundamental.", b)


# ===========================================================================
# Figure 5 — the two pan laws, and the geometry behind the parameter
# ===========================================================================
def fig5():
    uid = "l78f5"
    W, H = 900, 474
    b = [cmarker(uid, "amber", AMBER), cmarker(uid, "red", RED),
         cmarker(uid, "blue", BLUE), cmarker(uid, "grey", GREY)]

    # ---- left: the gains, and the power ---------------------------------
    px, py, pw, ph = 56, 56, 380, 170
    b.append(frame(px, py, pw, ph))
    b.append(label(px, py - 16, "channel gains as the sound sweeps left to right",
                   cls="xs muted", anchor="start"))

    def gx(pan):
        return px + (pan + 1.0) / 2.0 * pw

    def gy(g):
        return py + ph - g * ph

    for g in (0.0, 0.5, 0.7071, 1.0):
        b.append(rule(px, gy(g), px + pw, gy(g), cls="grid"))
        b.append(label(px - 8, gy(g) + 4, f"{g:.3f}".rstrip("0").rstrip("."),
                       cls="xs muted", anchor="end"))
    for pan in (-1.0, 0.0, 1.0):
        b.append(rule(gx(pan), py, gx(pan), py + ph, cls="grid"))
    b.append(label(gx(-1.0), py + ph + 16, "hard left", cls="xs muted"))
    b.append(label(gx(0.0), py + ph + 16, "centre", cls="xs muted"))
    b.append(label(gx(1.0), py + ph + 16, "hard right", cls="xs muted"))

    n = 200
    lin_l = [(gx(-1 + 2 * i / n), gy(1 - (i / n))) for i in range(n + 1)]
    lin_r = [(gx(-1 + 2 * i / n), gy(i / n)) for i in range(n + 1)]
    cp_l = [(gx(-1 + 2 * i / n), gy(math.cos(i / n * math.pi / 2))) for i in range(n + 1)]
    cp_r = [(gx(-1 + 2 * i / n), gy(math.sin(i / n * math.pi / 2))) for i in range(n + 1)]
    b.append(poly(lin_l, GREY, width=1.4, dash="5 4", close=False))
    b.append(poly(lin_r, GREY, width=1.4, dash="5 4", close=False))
    b.append(poly(cp_l, BLUE, width=2.0, close=False))
    b.append(poly(cp_r, AMBER, width=2.0, close=False))
    b.append(f'<circle cx="{gx(0):.1f}" cy="{gy(0.7071):.1f}" r="3.4" fill="{AMBER}"/>')
    b.append(f'<circle cx="{gx(0):.1f}" cy="{gy(0.5):.1f}" r="3.4" fill="{GREY}"/>')
    # The two crossing values live in the legend rather than beside the dots.
    # In the plot they sat ON the curves they were annotating, which check-page.js
    # flags and which is right to flag: a number touching a line reads as a label
    # for that line at that point rather than as a value at the centre.

    # ---- left lower: the power -------------------------------------------
    qy = py + ph + 60
    qh = 92
    b.append(frame(px, qy, pw, qh))
    b.append(label(px, qy - 12, "and the POWER the pair delivers: L^2 + R^2",
                   cls="xs muted", anchor="start"))

    def qyv(p):
        # 0.4 at the bottom, 1.1 at the top
        return qy + qh - (p - 0.4) / 0.7 * qh

    for p in (0.5, 1.0):
        b.append(rule(px, qyv(p), px + pw, qyv(p), cls="grid"))
        b.append(label(px - 8, qyv(p) + 4, f"{p:.1f}", cls="xs muted", anchor="end"))
    lin_p = []
    for i in range(n + 1):
        u = i / n
        lin_p.append((gx(-1 + 2 * u), qyv((1 - u) ** 2 + u ** 2)))
    b.append(poly(lin_p, RED, width=2.0, close=False))
    b.append(poly([(px, qyv(1.0)), (px + pw, qyv(1.0))], GREEN, width=2.4, close=False))
    b.append(carrow(gx(0), qyv(1.0) - 4, gx(0), qyv(0.5) - 2, uid, "red", RED, width=1.4))
    b.append(label(gx(0) + 8, qyv(0.72) + 4, "-3.01 dB", cls="xs mono", anchor="start"))

    # legend, OUTSIDE
    ly = qy + qh + 26
    for i, (col, dash, text) in enumerate([
            (GREY, True, "the linear law: L + R = 1 - both gains are 0.5000 at centre,"),
            (GREY, True, "      and the power the pair delivers there is 0.5, or -3.01 dB."),
            (AMBER, False, "constant power: L = cos, R = sin - both are 0.7071 at centre,"),
            (AMBER, False, "      and the power is exactly 1 for every pan position.")]):
        if not text.startswith(" "):
            b.append(cline(px, ly + i * 17 - 4, px + 22, ly + i * 17 - 4, col, width=2.4,
                           dash="5 4" if dash else None))
        b.append(label(px + 30, ly + i * 17, text.strip() if text.startswith(" ") else text,
                       cls="xs" if not text.startswith(" ") else "xs muted", anchor="start"))

    # ---- right: the geometry --------------------------------------------
    cx, cy, R = 640, 196, 100
    b.append(label(478, 40, "where pan comes from: one dot product",
                   cls="xs muted", anchor="start"))
    b.append(f'<circle cx="{cx}" cy="{cy}" r="{R}" fill="none" stroke="{GREY}" '
             f'stroke-width="1" stroke-dasharray="3 4"/>')
    b.append(carrow(cx, cy, cx, cy - R - 18, uid, "blue", A_FORWARD, width=1.8))
    b.append(label(cx, cy - R - 26, "forward (-z)", cls="xs mono"))
    b.append(carrow(cx, cy, cx + R + 18, cy, uid, "red", A_RIGHT, width=1.8))
    b.append(label(cx + R + 24, cy + 4, "right (+x)", cls="xs mono", anchor="start"))
    b.append(f'<circle cx="{cx}" cy="{cy}" r="4" fill="{A_LISTENER}"/>')

    for deg, pan, l_g, r_g in ORBIT:
        a = deg * D
        ex = cx + R * math.sin(a)
        ey = cy - R * math.cos(a)
        b.append(f'<circle cx="{ex:.1f}" cy="{ey:.1f}" r="3.4" fill="{A_SIREN}"/>')
        tx = cx + (R + 24) * math.sin(a)
        ty = cy - (R + 24) * math.cos(a)
        if deg in (45, 135, 225, 315):
            b.append(label(tx, ty + 4, f"{pan:+.2f}", cls="xs mono"))
    b.append(label(cx, cy + R + 44,
                   "pan = dot(direction, right) — the sine of the angle",
                   cls="xs muted"))
    b.append(label(cx, cy + R + 60,
                   "off the median plane. Front and back give the same",
                   cls="xs muted"))
    b.append(label(cx, cy + R + 76,
                   "answer, because two speakers cannot say which.",
                   cls="xs muted"))

    return svg(uid, W, H,
               "Two pan laws plotted against pan position, and the geometry of the pan value",
               "Upper left: the linear law's two gains cross at 0.5 while the constant-power "
               "law's cross at 0.7071. Lower left: the power the pair delivers, flat at 1.0 for "
               "the constant-power law and dipping to 0.5 — 3.01 dB — at centre for the linear "
               "one. Right: a listener facing -z with eight emitter positions around a circle, "
               "showing pan as the dot product with the right axis.", b)


# ===========================================================================
# Figure 6 — distance
# ===========================================================================
def fig6():
    uid = "l78f6"
    W, H = 900, 440
    b = [cmarker(uid, "red", RED), cmarker(uid, "amber", AMBER)]

    px, py, pw, ph = 62, 56, 430, 250
    b.append(frame(px, py, pw, ph))
    b.append(label(px, py - 16, "gain in decibels against distance, log distance axis",
                   cls="xs muted", anchor="start"))

    def gx(d):
        return px + (math.log2(max(d, 0.5)) - math.log2(0.5)) / (math.log2(64) - math.log2(0.5)) * pw

    def gy(db):
        # 6 dB at the top, -66 at the bottom
        return py + (6.0 - db) / 72.0 * ph

    for d in (1, 2, 4, 8, 16, 32, 64):
        b.append(rule(gx(d), py, gx(d), py + ph, cls="grid"))
        b.append(label(gx(d), py + ph + 16, str(d), cls="xs mono"))
    for db in (0, -12, -24, -36, -48, -60):
        b.append(rule(px, gy(db), px + pw, gy(db), cls="grid"))
        b.append(label(px - 8, gy(db) + 4, str(db), cls="xs muted", anchor="end"))
    b.append(label(px + pw / 2, py + ph + 36, "distance from the listener, metres",
                   cls="xs muted"))
    b.append(label(px - 46, py - 4, "dB", cls="xs muted"))

    def db(g):
        return max(-66.0, 20.0 * math.log10(g)) if g > 1e-7 else -66.0

    series = [(1, AMBER, "inverse: 1/d"), (2, GREEN, "linear"),
              (3, BLUE, "inverse_ranged (ours)"), (4, RED, "the mistake: 1/d^2")]
    for idx, col, _name in series:
        pts = [(gx(row[0]), gy(db(row[idx]))) for row in FALLOFF if row[0] <= 64]
        b.append(poly(pts, col, width=2.0, dash="6 4" if idx == 4 else None, close=False))

    # the cliff: the bare inverse law is still audible when it is cut off
    b.append(cline(gx(50), gy(CLIFF_DB), gx(50), gy(-66), RED, width=2.0, dash="3 3"))
    b.append(f'<circle cx="{gx(50):.1f}" cy="{gy(CLIFF_DB):.1f}" r="4" fill="{RED}"/>')
    b.append(carrow(gx(12), gy(-52), gx(48), gy(CLIFF_DB) + 6, uid, "red", RED, width=1.2))
    b.append(label(gx(11), gy(-52) + 4, "-33.98 dB, then cut to nothing",
                   cls="xs mono", anchor="end"))

    ly = py + ph + 54
    for i, (_idx, col, name) in enumerate(series):
        b.append(cline(px, ly + i * 17 - 4, px + 20, ly + i * 17 - 4, col, width=2.4))
        b.append(label(px + 28, ly + i * 17, name, cls="xs", anchor="start"))

    # ---- right: the per-doubling table and the worked example ------------
    tx = 540
    rows = [("1/d  (pressure)", f"{PER_DOUBLING_INV:.2f} dB"),
            ("1/d^2  (intensity)", f"{PER_DOUBLING_SQ:.2f} dB")]
    el, hgt = table(tx, 56, ["per doubling of distance", ""], rows, [200, 110])
    b += el
    y = 56 + hgt + 18
    b.append(label(tx, y, "Energy spreads over a sphere, so INTENSITY", cls="xs", anchor="start"))
    b.append(label(tx, y + 16, "falls as 1/d^2. A sample is a PRESSURE, and", cls="xs muted",
                   anchor="start"))
    b.append(label(tx, y + 32, "pressure is the square root of intensity.", cls="xs muted",
                   anchor="start"))

    rows2 = [("distance", f"{WORKED['dist']:.4f} m"),
             ("attenuation", f"{WORKED['atten']:.4f}"),
             ("pan", f"{WORKED['pan']:.4f}"),
             ("left gain", f"{WORKED['left']:.4f}"),
             ("right gain", f"{WORKED['right']:.4f}")]
    el2, _h2 = table(tx, y + 58, ["emitter (3, 0, -4), gain 0.8", ""], rows2, [200, 110])
    b += el2

    return svg(uid, W, H,
               "Four distance-attenuation curves in decibels against a log distance axis",
               "The inverse law falls 6.02 dB per doubling, the mistaken inverse-square law "
               "falls 12.04 dB, the linear law holds up and then collapses, and our "
               "inverse_ranged curve follows the inverse law close in and reaches silence at "
               "50 m. The bare inverse law is marked at -33.98 dB where it is cut off.", b)


# ===========================================================================
# Figure 7 — the gain step against the gain ramp
# ===========================================================================
def fig7():
    uid = "l78f7"
    W, H = 900, 400
    b = [cmarker(uid, "red", RED), cmarker(uid, "amber", AMBER)]

    # The worst boundary in the fly-by: L goes 0.7071 -> 0.4849, a change of
    # 0.2222, and the measured worst step of 0.100745 says the signal was at
    # 0.4533 when it happened. The phase below is chosen to put the boundary
    # there, because that is the sample max_step() actually found.
    g0, g1 = FLYBY[2][3], FLYBY[3][3]
    amp = 0.5
    boundary = 40
    total = 100
    phase = math.asin(0.4533 / amp)
    w = 2 * math.pi * 200.0 / 48000.0

    px, py, pw, ph = 56, 62, 470, 230
    zero = py + ph * 0.62
    b.append(frame(px, py, pw, ph))
    b.append(rule(px, zero, px + pw, zero, cls="grid"))
    b.append(label(px, py - 16,
                   "100 samples across one buffer boundary, 2 ms of a 200 Hz tone",
                   cls="xs muted", anchor="start"))
    b.append(label(px - 8, zero + 4, "0", cls="xs muted", anchor="end"))

    def sx(i):
        return px + i / float(total) * pw

    def sy(v):
        return zero - v / 0.45 * (ph * 0.36)

    raw = [amp * math.sin(phase + w * (i - boundary)) for i in range(total)]
    stepped = []
    ramped = []
    for i, v in enumerate(raw):
        g_step = g0 if i < boundary else g1
        # the ramp spends the whole of the next buffer getting there; over the
        # 90 samples drawn it has covered 90/800 of the change
        t = 0.0 if i < boundary else min(1.0, (i - boundary) / 800.0)
        g_ramp = g0 + (g1 - g0) * t
        stepped.append((sx(i), sy(v * g_step)))
        ramped.append((sx(i), sy(v * g_ramp)))

    bx = sx(boundary)
    b.append(cline(bx, py + 4, bx, py + ph - 4, GREY, width=1.2, dash="4 4"))
    b.append(label(bx, py + ph + 16, "buffer boundary", cls="xs muted"))

    b.append(poly(ramped, BLUE, width=2.0, close=False))
    b.append(poly(stepped, RED, width=2.0, close=False))

    # the discontinuity, called out
    jump_top = sy(raw[boundary - 1] * g0)
    jump_bot = sy(raw[boundary] * g1)
    # ARROWS ONLY. The first draft put the numbers here too, and they sat on top
    # of the very curves they were pointing at — which check-page.js flags, and
    # is right to: a number touching a line reads as that line's label. The words
    # are in the legend, four lines down.
    b.append(carrow(bx + 120, jump_top, bx + 4, jump_top, uid, "red", RED, width=1.1))
    b.append(carrow(bx + 120, jump_bot, bx + 4, jump_bot, uid, "red", RED, width=1.1))

    ly = py + ph + 42
    for i, (col, text) in enumerate([
            (RED, f"a STEP at the boundary: {STEP_JUMP:.6f} in ONE sample"),
            (BLUE, "RAMPED across the buffer, which takes 800 — the shipping path")]):
        b.append(cline(px, ly + i * 18 - 4, px + 22, ly + i * 18 - 4, col, width=2.4))
        b.append(label(px + 30, ly + i * 18, text, cls="xs", anchor="start"))

    # ---- right: the numbers ----------------------------------------------
    tx = 574
    rows = [("the signal's own slope", f"{OWN_SLOPE:.6f}", "1.00x"),
            ("gain as a step", f"{STEP_JUMP:.6f}", f"{STEP_RATIO:.2f}x"),
            ("gain as a ramp", f"{RAMP_JUMP:.6f}", f"{RAMP_RATIO:.2f}x")]
    el, hgt = table(tx, 62, ["largest jump between samples", "", ""], rows,
                    [156, 96, 60])
    b += el

    y = 62 + hgt + 22
    b.append(label(tx, y, "step minus ramp, over one second:", cls="xs muted", anchor="start"))
    b.append(label(tx, y + 20, f"rms {ERR_RMS:.3e}, of which only {ERR_TONE_PCT}%",
                   cls="xs mono", anchor="start"))
    b.append(label(tx, y + 36, "sits at the tone's own 200 Hz.", cls="xs mono", anchor="start"))
    b.append(label(tx, y + 58, "The other 98.8% is spread across every",
                   cls="xs", anchor="start"))
    b.append(label(tx, y + 74, "frequency there is. That is what a click",
                   cls="xs", anchor="start"))
    b.append(label(tx, y + 90, "IS, and it is why this is audible at all.",
                   cls="xs t-hi", anchor="start"))
    b.append(label(tx, y + 116, f"CONTROL: constant gain, the two agree to",
                   cls="xs muted", anchor="start"))
    b.append(label(tx, y + 132, f"{CONST_CONTROL:.3e}.", cls="xs mono muted", anchor="start"))

    return svg(uid, W, H,
               "A waveform across a buffer boundary with the gain stepped and ramped",
               "The stepped render jumps 0.100745 in one sample at the boundary, 7.7 times the "
               "largest step the waveform itself ever takes, while the ramped render is "
               "indistinguishable from the waveform's own motion. A table gives the measured "
               "jumps and notes that 98.8 percent of the difference between them is broadband.",
               b)


# ===========================================================================
# Figure 8 — resampling
# ===========================================================================
def fig8():
    uid = "l78f8"
    W, H = 900, 400
    b = [cmarker(uid, "amber", AMBER), cmarker(uid, "blue", BLUE)]

    # ---- left: what linear interpolation does ---------------------------
    px, py, pw, ph = 56, 60, 380, 170
    zero = py + ph / 2
    b.append(frame(px, py, pw, ph))
    b.append(rule(px, zero, px + pw, zero, cls="grid"))
    b.append(label(px, py - 16, "the source's samples, and where the new rate lands",
                   cls="xs muted", anchor="start"))

    n_src = 8
    ratio = 44100.0 / 48000.0
    amp = ph / 2 - 16

    def wave(u):
        return math.sin(2 * math.pi * 1.45 * u)

    # the true continuous signal
    b.append(poly([(px + i / 400.0 * pw, zero - wave(i / 400.0) * amp)
                   for i in range(401)], GREY, width=1.6, close=False))
    src_pts = []
    for i in range(n_src + 1):
        u = i / float(n_src)
        x = px + u * pw
        y = zero - wave(u) * amp
        src_pts.append((x, y))
        b.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3.6" fill="{AMBER}"/>')
    b.append(poly(src_pts, AMBER, width=1.4, close=False))

    n_dst = int(n_src / ratio)
    for i in range(n_dst + 1):
        pos = i * ratio
        if pos > n_src:
            break
        i0 = int(pos)
        i1 = min(i0 + 1, n_src)
        t = pos - i0
        u0, u1 = i0 / float(n_src), i1 / float(n_src)
        y = (zero - wave(u0) * amp) * (1 - t) + (zero - wave(u1) * amp) * t
        x = px + (pos / n_src) * pw
        b.append(cline(x, zero, x, y, BLUE, width=1.0))
        b.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="2.6" fill="{BLUE}"/>')

    b.append(label(px + pw / 2, py + ph + 20,
                   "every new sample is a weighted average of two old ones,",
                   cls="xs muted"))
    b.append(label(px + pw / 2, py + ph + 36,
                   "so it lands on the CHORD and never on the curve.",
                   cls="xs muted"))
    ly = py + ph + 58
    for i, (col, text) in enumerate([(GREY, "the sound that was recorded"),
                                     (AMBER, "the file's samples, at 44,100 Hz"),
                                     (BLUE, "the device's samples, at 48,000 Hz")]):
        b.append(cline(px, ly + i * 18 - 4, px + 20, ly + i * 18 - 4, col, width=2.4))
        b.append(label(px + 28, ly + i * 18, text, cls="xs", anchor="start"))

    # ---- right: the SNR, measured ---------------------------------------
    hx, hy, hw, hh = 508, 60, 330, 170
    b.append(frame(hx, hy, hw, hh))
    b.append(label(hx, hy - 16, "signal to noise after the conversion, measured",
                   cls="xs muted", anchor="start"))

    def by(db):
        # The axis runs to 110 rather than 100 so that the 91.67 bar's own label
        # has somewhere to sit that is not on the frame's top edge.
        return hy + hh - db / 110.0 * hh

    for db in (0, 25, 50, 75, 100):
        b.append(rule(hx, by(db), hx + hw, by(db), cls="grid"))
        b.append(label(hx - 8, by(db) + 4, str(db), cls="xs muted", anchor="end"))
    b.append(label(hx - 34, hy - 2, "dB", cls="xs muted"))

    cols = [AMBER, BLUE, GREEN]
    for i, ((name, db), col) in enumerate(zip(RESAMPLE, cols)):
        x = hx + (i + 0.5) * hw / 3.0
        b.append(box(x - 26, by(db), 52, hy + hh - by(db), col, opacity=0.5, rx=2))
        b.append(label(x, by(db) - 8, f"{db:.2f}", cls="xs mono"))
    b.append(label(hx + hw / 6.0, hy + hh + 16, "ours", cls="xs muted"))
    b.append(label(hx + hw / 2.0, hy + hh + 16, "SDL", cls="xs muted"))
    b.append(label(hx + 5 * hw / 6.0, hy + hh + 16, "no resample", cls="xs muted"))

    rows = [(f"{hz:,} Hz", f"{db:.2f} dB") for hz, db in BY_FREQ]
    el, _h = table(hx, hy + hh + 34, ["ours, by tone frequency", ""], rows, [200, 110],
                   row_h=16.0, head_h=18.0)
    b += el

    return svg(uid, W, H,
               "Linear-interpolation resampling and the signal-to-noise it achieves",
               "Left: ten source samples at 44.1 kHz with the 48 kHz output samples landing on "
               "the chords between them rather than on the curve. Right: bars showing 62.40 dB "
               "for our linear resampler, 85.05 dB for SDL's and 91.67 dB for not resampling, "
               "and a table showing ours falling from 89 dB at 200 Hz to 20.43 dB at 10 kHz.",
               b)


# ===========================================================================
# Figure 9 — the demo, which is a picture of something you cannot see
# ===========================================================================
def fig9():
    uid = "l78f9"
    CROP = (240, 140, 816, 452)
    CELL, PX = 3, 3
    b = [cmarker(uid, "amber", AMBER), cmarker(uid, "grey", GREY)]

    X0, Y0 = 20, 54
    panel, PW, PH = render_panel("l78_demo.ppm", CROP, X0, Y0, PX, cell=CELL, peak=True)
    W, H = 900, Y0 + PH + 92
    b += panel
    b.append(hollow(X0, Y0, PW, PH, GREY, width=1.0))
    b.append(label(X0, Y0 - 16, "demos/audio, t = 3.2 s, headless and therefore silent",
                   cls="xs muted", anchor="start"))

    # The crop maps demo pixels to page units: page = X0 + (demo - crop0) * PX / CELL
    def dx(x):
        return X0 + (x - CROP[0]) * PX / CELL

    def dy(y):
        return Y0 + (y - CROP[1]) * PX / CELL

    # The three emitters, at their t = 3.2 positions, and the listener at the
    # origin. Demo pixels: to_screen() is 480 + 12x, 270 + 12z.
    def demo_px(wx, wz):
        return 480 + wx * 12.0, 270 + wz * 12.0

    sx, sy = demo_px(6.0 * math.sin(3.2 * 0.6), -6.0 * math.cos(3.2 * 0.6))
    fx, fy = demo_px(math.fmod(3.2 * 6.0 + 30.0, 60.0) - 30.0, -2.0)
    mx, my = demo_px(-9.0, 7.0)
    lx, ly = demo_px(0.0, 0.0)

    # ANNOTATIONS GO OUTSIDE THE PANEL. The first draft put four labels on the
    # map itself with leader arrows, and they were invisible: `label()` renders
    # in the page's ink colour, which is dark, and the panel is dark. CSS wins
    # over any fill you write on a <text>, so the fix is not a colour — it is to
    # put the words somewhere the page's own colours are correct for.
    keys = [(A_SIREN, "siren, orbiting at 6 m", f"({sx:.0f}, {sy:.0f})"),
            (A_FLYBY, "fly-by, crossing at 2 m", f"({fx:.0f}, {fy:.0f})"),
            (A_MUSIC, "music, falloff::none", f"({mx:.0f}, {my:.0f})"),
            (A_LISTENER, "the listener, at the origin", f"({lx:.0f}, {ly:.0f})"),
            (A_FORWARD, "its forward axis (-z)", ""),
            (A_RIGHT, "its right axis (+x)", "")]
    ky = Y0 + PH + 22
    for i, (col, text, _pos) in enumerate(keys):
        row = i // 2
        col_x = X0 + (i % 2) * 290
        b.append(cline(col_x, ky + row * 18 - 4, col_x + 20, ky + row * 18 - 4,
                       col, width=4.0))
        b.append(label(col_x + 28, ky + row * 18, text, cls="xs", anchor="start"))

    tx = X0 + PW + 28
    b.append(label(tx, Y0 - 16, "what the picture is for", cls="xs muted", anchor="start"))
    lines = [
        ("Every other demo in this repository", "xs"),
        ("can be judged by looking at it.", "xs"),
        ("", "xs"),
        ("This one cannot. So the map is not", "xs"),
        ("the result — it is the EXPLANATION", "xs t-hi"),
        ("of the result: where each source is,", "xs"),
        ("and the two gains that fell out of", "xs"),
        ("that, drawn as bars in decibels.", "xs"),
        ("", "xs"),
        ("The faint rings are 2, 4, 8, 16 and", "xs muted"),
        ("32 m from the LISTENER, so each one", "xs muted"),
        ("is a halving: -6 dB per ring.", "xs muted"),
    ]
    for i, (text, cls) in enumerate(lines):
        if text:
            b.append(label(tx, Y0 + 8 + i * 17, text, cls=cls, anchor="start"))

    return svg(uid, W, H,
               "The audio demo's top-down map of a listener and three emitters",
               "A dark map with a grid, faint distance rings around the listener at 2, 4, 8, 16 "
               "and 32 metres, and three coloured emitters joined to the listener by lines. Each "
               "emitter carries two short horizontal bars showing its left and right gain in "
               "decibels. The listener's forward axis is drawn in blue and its right axis in red.",
               b)


# ===========================================================================
# Figure 10 — the budget
# ===========================================================================
def fig10():
    uid = "l78f10"
    W, H = 900, 400
    b = [cmarker(uid, "red", RED), cmarker(uid, "amber", AMBER)]

    px, py, pw, ph = 130, 60, 400, 230
    b.append(frame(px, py, pw, ph))
    b.append(label(px - 76, py - 34,
                   "microseconds, log scale: what a buffer costs against what it has",
                   cls="xs muted", anchor="start"))

    lo, hi = 0.005, 100000.0

    def gx(us):
        return px + (math.log10(max(us, lo)) - math.log10(lo)) / \
            (math.log10(hi) - math.log10(lo)) * pw

    for us, name in ((0.01, "0.01"), (0.1, "0.1"), (1, "1"), (10, "10"), (100, "100"),
                     (1000, "1 ms"), (10000, "10 ms"), (100000, "100 ms")):
        b.append(rule(gx(us), py, gx(us), py + ph, cls="grid"))
        b.append(label(gx(us), py + ph + 16, name, cls="xs mono muted"))

    bars = [(f"{n} voices", us, AMBER) for n, us, _pct, _ns in BUDGET if n > 0]
    bars.append(("one log line", LOG_IO_NS / 1000.0, RED))
    bars.append(("set_gain", SET_GAIN_NS / 1000.0, BLUE))
    row_h = ph / (len(bars) + 1.0)
    for i, (name, us, col) in enumerate(bars):
        y = py + (i + 0.7) * row_h
        b.append(box(px, y - 6, gx(us) - px, 12, col, opacity=0.5, rx=2))
        b.append(label(px - 8, y + 4, name, cls="xs", anchor="end"))
        b.append(label(gx(us) + 6, y + 4, f"{us:.2f}", cls="xs mono", anchor="start"))

    b.append(cline(gx(BUDGET_US), py - 6, gx(BUDGET_US), py + ph + 4, GREEN, width=2.0))
    b.append(label(gx(BUDGET_US), py - 12, "the deadline", cls="xs mono"))

    b.append(label(px - 76, py + ph + 44,
                   "Sixty-four voices use 0.28% of the budget. One log line — which is a",
                   cls="xs", anchor="start"))
    b.append(label(px - 76, py + ph + 60,
                   "SYSCALL, and therefore unbounded — costs more than mixing four of them.",
                   cls="xs t-hi", anchor="start"))

    tx = 596
    rows = [(f"{n}", f"{ms:.2f} ms", f"{cb:.0f}") for n, ms, cb in LATENCY]
    el, hgt = table(tx, 60, ["frames", "latency", "calls/s"], rows, [92, 100, 90],
                    title="the other half of the trade")
    b += el

    y = 60 + hgt + 22
    rows2 = [("device", f"{DEV['freq']} Hz"),
             ("buffers", f"{DEV['buffers']}"),
             ("late", f"{DEV['late']}"),
             ("queue_empty", f"{DEV['queue_empty']}"),
             ("worst mix", f"{DEV['worst_us']:.2f} us"),
             ("load", f"{DEV['load_pct']:.3f}%")]
    el2, _h2 = table(tx, y, ["on a real device", ""], rows2, [150, 132],
                     row_h=16.0, head_h=18.0)
    b += el2

    return svg(uid, W, H,
               "Mixing cost against the audio deadline, on a logarithmic microsecond axis",
               "Horizontal bars show 1 to 64 voices costing 1.03 to 29.76 microseconds, one log "
               "line costing 1.02 microseconds and a set_gain call 0.009, against a deadline "
               "marked at 10,666 microseconds. Tables give buffer size against latency and the "
               "counters from a run on a real device.", b)
