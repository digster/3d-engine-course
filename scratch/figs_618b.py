#!/usr/bin/env python3
"""scratch/figs_618b.py — Lesson 6.18b's diagrams.

Same rules as 6.1-6.18's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - ~5.2 units per character for `xs`, ~6.0 for `sm`
  - filenames numbered by PAGE ORDER
  - `rule()` takes a CSS CLASS; `cline()` takes a COLOUR
  - legends and annotation boxes go OUTSIDE the plot
  - viewBoxes at most 928 wide, labels no smaller than `xs`

Every number comes from verify_618b's output — scratch/_618b/run_O0.txt (the
default build, every exact section) and run_rel.txt (the Release library, §L's
timings) — and the plots from the data files it writes, copied to
scratch/l618b_*.csv and l618b_demo.ppm so the figures regenerate from tracked
files. Colours: CPU = BLUE, GPU = AMBER, right = GREEN, wrong = RED.
"""
import csv
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)
from figs_610 import cline                                          # noqa: E402
from figs_45 import read_ppm, box_sample, rle_rects, hexrgb         # noqa: E402
from figs_71 import cmarker, carrow, poly, frame                    # noqa: E402

OUT = "scratch"

# ---- measured (verify_618b, default build unless marked) --------------------
CHECKS = 69

# §A — the record
NAIVE_GPU = (16, 28, 32)          # velocity, age, stride as the GPU laid them out
NAIVE_CPP = (12, 24, 28)
NAIVE_READ = ("0", "4", "5", "6")  # velocity x y z and age, read through C++'s struct

# §B
TAIL_IDLE = 24

# §C
PAIRS = {"pcg": (-0.0019, 4096, 65411), "lcg": (0.9977, 128, 65536),
         "sin_small": (0.0014, 4096, 4178), "sin_big": (0.4963, 4096, 4179)}
SINE_GPU_SAME = 568408
MILLION = 1048576

# §D
BORN_MINUTE = 1200000
TRUNC_MINUTE = 1198800
POOL_CUT = (152704, 200000, 0.685)   # pool 32,768: cut short, births, mean life lost
WRAP_HOURS = 59.7

# §E
LUMPY = {"narrow_spread": 0.213, "narrow_shells": 0.912, "wide_spread": 0.221,
         "wide_shells": 0.232}

# §F
DRAG_EXPLICIT = 57.67
DRAG_IMPLICIT = 3.63e-06
PEAKS = (0.1848, 0.0318, 0.0033)
PEAKS_FINE = (0.1970, 0.0386, 0.0070)
CLOSED_FORM_ERR = 0.0817

# §G
AFTER_10S = {"alive": 44915, "identical": 5264, "within_1um": 44889, "worst_um": 1.38}
DEBUG_RELEASE = (897, 44915, 0.596)

# §H
RACY = (296, 509)          # fifteen trials across five runs of the harness
ATOMIC = 1048576
ALIVE_COUNT = 44915
ORDER_MOVED = (44814, 44915)   # four runs

# §I — the four arms, alive after 120 steps (CPU 38,284)
CYCLE_ARMS = [("cycle = false, back to back", 38284, True),
              ("cycle = true, back to back", 668, False),
              ("cycle = true, fence wait each step", 334, False),
              ("cycle = true, SDL_WaitForGPUIdle", 38284, True)]
CYCLE_CPU = 38284

# §J
GRAPH_CHANNELS = 172800

# §K
K_LIT = 10329
K_DIFF = (7681, 8075)
K_WORST_ULP = 11
WALL = (2.89, 0, 6.00, 9401)


def fmt_int(n):
    return f"{n:,}"


def read_csv(name):
    with open(os.path.join(OUT, name), newline="") as fh:
        return list(csv.DictReader(fh))


def write(name, text):
    with open(os.path.join(OUT, name), "w") as fh:
        fh.write(text)


def dot(x, y, r, colour, opacity=1.0):
    o = f' fill-opacity="{opacity}"' if opacity < 1.0 else ""
    return f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r}" fill="{colour}" stroke="none"{o}/>'


def dots(points, colour, size=1.8, opacity=0.8):
    """Many dots as ONE path: a zero-length segment per point, drawn with a round
    cap. A <circle> per dot costs ~70 bytes and a scatter of 4,000 is then a
    quarter of a megabyte; this costs ~12 a dot and renders identically."""
    d = "".join(f"M{x:.1f},{y:.1f}h0" for x, y in points)
    return (f'<path d="{d}" fill="none" stroke="{colour}" stroke-width="{size}" '
            f'stroke-linecap="round" stroke-opacity="{opacity}"/>')


def pcg(v):
    v &= 0xFFFFFFFF
    state = (v * 747796405 + 2891336453) & 0xFFFFFFFF
    word = (((state >> ((state >> 28) + 4)) ^ state) * 277803737) & 0xFFFFFFFF
    return ((word >> 22) ^ word) & 0xFFFFFFFF


def unit(bits):
    return (bits >> 8) * (1.0 / 16777216.0)


# ===========================================================================
# Figure 1 — the bus  (§1)
# ===========================================================================
def fig_bus():
    uid = "l618bf1"
    W, H = 880, 372
    b = [cmarker(uid, "cpu", BLUE), cmarker(uid, "gpu", AMBER)]
    b.append(label(20, 22, "Who owns the particles decides what crosses the bus every frame.",
                   "sm", "start"))
    rows = read_csv("l618b_budget.csv")
    r65 = next(r for r in rows if r["capacity"] == "65536")
    r1m = next(r for r in rows if r["capacity"] == "1048576")

    def lane(y, title, cls, cpu_text, bus_text, gpu_text, colour, name):
        b.append(label(20, y - 12, title, f"xs {cls}", "start"))
        b.append(box(20, y, 230, 58, colour=BLUE, opacity=0.16))
        b.append(label(135, y + 18, "CPU", "sm"))
        for i, t in enumerate(cpu_text):
            b.append(label(135, y + 34 + 12 * i, t, "xs"))
        b.append(box(610, y, 250, 58, colour=AMBER, opacity=0.16))
        b.append(label(735, y + 18, "GPU", "sm"))
        for i, t in enumerate(gpu_text):
            b.append(label(735, y + 34 + 12 * i, t, "xs"))
        b.append(carrow(254, y + 29, 604, y + 29, uid, name, colour, 2.4 if name == "cpu" else 1.2))
        for i, t in enumerate(bus_text):
            b.append(label(429, y + 21 - 12 * (len(bus_text) - 1 - i), t, "xs"))

    lane(62, "BEFORE — the CPU simulates, and the pool is shipped", "t-bad",
         ["step 65,536 slots", f"{float(r65['cpu_ms']):.3f} ms (Release)"],
         ["3.00 MB, every frame", f"{float(r65['upload_ms']):.3f} ms incl. submit"],
         ["count + draw", "(it could not simulate)"], BLUE, "cpu")
    lane(170, "AFTER — the GPU simulates, and nothing is shipped but the step", "t-ok",
         ["advance the emission clock", "fill one uniform block"],
         ["144 B per fixed step"],
         ["step, count, draw", f"{float(r65['gpu_ms']):.3f} ms per step"], AMBER, "gpu")

    # the scaling table, below both lanes
    y0 = 268
    b.append(rule(20, y0 - 12, 860, y0 - 12, "grid", 1.0))
    cols = [(20, "start", "slots"), (170, "end", "CPU step"), (300, "end", "upload"),
            (430, "end", "CPU total"), (560, "end", "GPU step"), (690, "end", "of 16.7 ms"),
            (860, "end", "ratio")]
    for x, a, t in cols:
        b.append(label(x, y0 + 4, t, "xs muted", a))
    for k, r in enumerate(rows):
        y = y0 + 22 + 16 * k
        cpu = float(r["cpu_ms"])
        up = float(r["upload_ms"])
        gpu = float(r["gpu_ms"])
        vals = [fmt_int(int(r["capacity"])), f"{cpu:.3f} ms", f"{up:.3f} ms",
                f"{cpu + up:.3f} ms", f"{gpu:.3f} ms", f"{100 * (cpu + up) / 16.667:.1f}%",
                f"{(cpu + up) / gpu:.0f}x"]
        for (x, a, _), v in zip(cols, vals):
            b.append(label(x, y, v, "xs mono", a))
    b.append(label(20, H - 8, "Release library, this course's M-series machine (unified memory: "
                   "the upload is a copy within one RAM; a discrete GPU pays PCIe as well).",
                   "xs muted", "start"))
    desc = ("Two lanes. Before: the CPU steps 65,536 particles and ships 3 MB to the GPU every "
            "frame. After: the CPU sends 144 bytes of uniforms per step and the GPU steps, counts "
            "and draws. A table gives the costs at four pool sizes; at 1,048,576 the upload alone "
            f"({float(r1m['upload_ms']):.2f} ms) costs more than the CPU step.")
    write("l618b_fig1.svg", svg(uid, W, H, "What crosses the bus", desc, b))


# ===========================================================================
# Figure 2 — the grid  (§3)
# ===========================================================================
def fig_grid():
    uid = "l618bf2"
    W, H = 880, 300
    b = []
    b.append(label(20, 22, "1,000 particles, 64 threads a group: the grid is sized in whole "
                           "groups, so the last one is partly empty.", "sm", "start"))
    gx, gy, gw, gh, gap = 20, 44, 49, 30, 4
    for g in range(16):
        x = gx + g * (gw + gap)
        if g < 15:
            b.append(box(x, gy, gw, gh, colour=AMBER, opacity=0.30))
        else:
            active = 40 / 64 * gw
            b.append(box(x, gy, active, gh, colour=AMBER, opacity=0.30, rx=0))
            b.append(box(x + active, gy, gw - active, gh, colour=RED, opacity=0.22, rx=0, dash="3 2"))
        b.append(label(x + gw / 2, gy + 19, f"g{g}", "xs mono"))
    b.append(label(gx + 15 * (gw + gap) + gw / 2, gy + gh + 14, "40 + 24", "xs t-bad"))
    b.append(label(gx, gy + gh + 30, "groups_for(1000, 64) = 16  ->  1,024 threads; threads "
                   "1,000 to 1,023 have no particle", "xs", "start"))

    # zoom on group 3
    zx, zy = 20, 138
    b.append(label(zx, zy - 6, "inside group 3 — the three IDs a thread is handed", "xs muted", "start"))
    cells = 16
    cw = 26
    for t in range(cells):
        x = zx + t * cw
        b.append(box(x, zy, cw - 2, 26, colour=AMBER, opacity=0.18, rx=2))
        lab = str(t) if t < 6 else ("..." if t == 6 else "")
        if t >= 13:
            lab = str(61 + (t - 13))
        b.append(label(x + (cw - 2) / 2, zy + 17, lab, "xs mono"))
    # One <text> per column: SVG collapses runs of spaces, so padding a single
    # string into columns comes out ragged (8.11's figure 2 learned it).
    rows3 = [("SV_GroupThreadID.x", "= t, 0 .. 63", "the lane's place in its group"),
             ("SV_GroupID.x", "= 3", "which group"),
             ("SV_DispatchThreadID.x", "= 3 * 64 + t", "192 .. 255: the particle's slot")]
    for i, (a, v, note) in enumerate(rows3):
        yy = zy + 48 + 16 * i
        b.append(label(zx, yy, a, "xs mono", "start"))
        b.append(label(zx + 138, yy, v, "xs mono", "start"))
        b.append(label(zx + 232, yy, note, "xs muted", "start"))

    # the ceiling
    x2 = 470
    b.append(hollow(x2, zy - 2, 390, 118, GREY))
    b.append(label(x2 + 12, zy + 16, "the textbook ceiling, and its edge", "xs muted", "start"))
    b.append(label(x2 + 12, zy + 36, "(n + 63) / 64  at n = 2^32 - 1   ->   0 groups", "xs mono t-bad", "start"))
    b.append(label(x2 + 12, zy + 54, "n / 64 + (n % 64 != 0)            ->   67,108,864", "xs mono t-ok", "start"))
    b.append(label(x2 + 12, zy + 76, "No pool reaches 2^32. The helper closes the edge", "xs", "start"))
    b.append(label(x2 + 12, zy + 90, "instead of documenting it, and every kernel still", "xs", "start"))
    b.append(label(x2 + 12, zy + 104, "checks  if (slot >= capacity) return;", "xs", "start"))

    b.append(label(20, H - 14, f"verify_618b §H: the probe kernel with its bounds check removed "
                   f"writes {TAIL_IDLE} elements past the end of a 1,000-element array — "
                   "one per idle thread.", "xs muted", "start"))
    desc = ("Sixteen groups of 64 threads cover 1,000 particles; the last group has 40 threads "
            "with work and 24 without. A zoom on group 3 shows how SV_DispatchThreadID is the "
            "group index times 64 plus the thread's place in the group. A panel shows the "
            "textbook ceiling division wrapping to zero groups at 2^32 - 1.")
    write("l618b_fig2.svg", svg(uid, W, H, "Threads, groups and the tail", desc, b))


# ===========================================================================
# Figure 3 — the record's layout  (§4)
# ===========================================================================
def fig_layout():
    uid = "l618bf3"
    W, H = 880, 356
    b = []
    b.append(label(20, 22, "Three 16-byte rows are laid out the same by every rule; two float3s "
                           "in a row are not.", "sm", "start"))
    x0 = 190
    px = 13.0          # units per byte
    b.append(label(x0 - 10, 50, "byte", "xs muted", "end"))
    for byte in range(0, 49, 4):
        x = x0 + byte * px
        b.append(rule(x, 54, x, 318, "grid", 0.6, "2 3"))
        b.append(label(x, 50, str(byte), "xs mono muted"))

    def row(y, name, fields, note=None, bad=False):
        b.append(label(x0 - 10, y + 16, name, "xs", "end"))
        for start, size, text, colour in fields:
            b.append(box(x0 + start * px, y, size * px - 2, 24, colour=colour,
                         opacity=0.30 if colour != GREY else 0.14))
            b.append(label(x0 + start * px + (size * px - 2) / 2, y + 16, text, "xs mono"))
        if note:
            b.append(label(x0 + 48 * px + 8, y + 16, note, "xs " + ("t-bad" if bad else "t-ok"),
                           "start"))

    row(64, "engine::particle", [(0, 12, "position", BLUE), (12, 4, "age", BLUE),
                                 (16, 12, "velocity", BLUE), (28, 4, "life", BLUE),
                                 (32, 12, "previous", BLUE), (44, 4, "serial", BLUE)])
    row(96, "Particle on the GPU", [(0, 12, "position", AMBER), (12, 4, "age", AMBER),
                                    (16, 12, "velocity", AMBER), (28, 4, "life", AMBER),
                                    (32, 12, "previous", AMBER), (44, 4, "serial", AMBER)],
        "identical")
    b.append(label(x0, 142, "the tempting struct: { float3 position; float3 velocity; float age; }",
                   "xs mono", "start"))
    row(154, "C++ (vec3, vec3, float)", [(0, 12, "position", BLUE), (12, 12, "velocity", BLUE),
                                         (24, 4, "age", BLUE)], "stride 28")
    row(186, "GPU, glslang -> SPIR-V", [(0, 12, "position", AMBER), (12, 4, "pad", GREY),
                                        (16, 12, "velocity", AMBER), (28, 4, "age", AMBER)],
        "stride 32", bad=True)
    row(218, "HLSL rules (DXC -> DXIL)", [(0, 12, "position", PURPLE), (12, 12, "velocity", PURPLE),
                                          (24, 4, "age", PURPLE)], "stride 28")
    b.append(label(x0, 268, "read the GPU's bytes through C++'s struct  ->  velocity = "
                   f"({NAIVE_READ[0]}, {NAIVE_READ[1]}, {NAIVE_READ[2]}),  age = {NAIVE_READ[3]}",
                   "xs mono t-bad", "start"))
    b.append(label(x0, 284, "the shader wrote velocity = (4, 5, 6) and age = 7", "xs mono muted",
                   "start"))
    b.append(label(20, 312, "SDL_gpu.h, on storage buffers: “vec3 and vec4 fields are "
                   "16-byte aligned.”", "xs", "start"))
    b.append(label(20, 330, "The same HLSL is 28 bytes a record under one compiler and 32 under "
                   "another; a row of float3 + one scalar is 16 under both.", "xs muted", "start"))
    desc = ("A byte ruler from 0 to 48. The engine's particle and the shader's Particle occupy "
            "identical offsets in three 16-byte rows. The tempting struct of two float3s and a "
            "float is 28 bytes with velocity at 12 in C++ and under HLSL rules, but 32 bytes with "
            "velocity at 16 under the SPIR-V rules this toolchain applies; reading the GPU's "
            "bytes through the C++ struct gives velocity (0, 4, 5) and age 6.")
    write("l618b_fig3.svg", svg(uid, W, H, "The record, laid out", desc, b))


# ===========================================================================
# Figure 4 — pairs of random numbers  (§5)
# ===========================================================================
def fig_pairs():
    uid = "l618bf4"
    W, H = 880, 372
    b = []
    b.append(label(20, 22, "Each dot is a spark's first two draws, (u(2i), u(2i+1)), for 2,048 "
                           "consecutive particles.", "sm", "start"))
    rows = read_csv("l618b_c_pairs.csv")
    panels = [("one LCG step, seeded by i", "lcg", RED, PAIRS["lcg"], "t-bad"),
              ("fract(sin(i) × 43758.5453), i ≥ 2^24", "sin", RED, PAIRS["sin_big"], "t-bad"),
              ("pcg_hash(i)", "pcg", GREEN, PAIRS["pcg"], "t-ok")]
    size = 240
    for k, (title, key, colour, stats, cls) in enumerate(panels):
        x = 30 + k * 285
        y = 50
        b.append(frame(x, y, size, size))
        b.append(label(x + size / 2, y - 8, title, f"xs {cls}"))
        b.append(dots([(x + float(r[f"{key}_a"]) * size, y + size - float(r[f"{key}_b"]) * size)
                       for r in rows], colour, 1.8, 0.8))
        corr, cells, distinct = stats
        b.append(label(x, y + size + 18, f"corr(i, i+1) {corr:+.4f}", "xs mono", "start"))
        b.append(label(x, y + size + 32, f"cells reached {cells:,}/4,096", "xs mono", "start"))
        b.append(label(x, y + size + 46, f"distinct {distinct:,}/65,536", "xs mono", "start"))
    b.append(label(20, H - 6, f"And the sine hash is not one function: evaluated by the GPU, "
                   f"{fmt_int(MILLION - SINE_GPU_SAME)} of {fmt_int(MILLION)} outputs differ from "
                   "C++'s. PCG: all 1,048,576 identical.", "xs muted", "start"))
    desc = ("Three scatter plots of consecutive random pairs. One LCG step seeded by the index "
            "puts every pair on a few straight lines and reaches 128 of 4,096 cells. The sine "
            "hash past 2^24 repeats inputs, correlating neighbours at 0.50, and has only 4,179 "
            "distinct values from 65,536. PCG fills the square uniformly with correlation "
            "-0.0019.")
    write("l618b_fig4.svg", svg(uid, W, H, "A hash, not a sequence", desc, b))


# ===========================================================================
# Figure 5 — a direction uniform over the cap  (§5)
# ===========================================================================
def fig_cap():
    uid = "l618bf5"
    W, H = 880, 356
    b = []
    b.append(label(20, 22, "The cone seen down its axis. Equal steps in angle are rings of "
                           "unequal area.", "sm", "start"))
    theta_max = math.radians(40.0)     # drawn wide, for legibility
    R = 120.0
    for k, (title, mode, cls) in enumerate([("uniform in θ: crowded at the axis", "theta", "t-bad"),
                                            ("uniform in cos θ: equal density", "cos", "t-ok")]):
        cx = 150 + k * 300
        cy = 180
        b.append(hollow(cx - R, cy - R, 2 * R, 2 * R, GREY, rx=R))
        for ring in (1, 2, 3):
            rr = R * math.sin(theta_max * ring / 4) / math.sin(theta_max)
            b.append(hollow(cx - rr, cy - rr, 2 * rr, 2 * rr, GREY, dash="2 3", rx=rr, width=0.8))
        b.append(label(cx, cy - R - 12, title, f"xs {cls}"))
        omc = 1.0 - math.cos(theta_max)
        pts = []
        for i in range(700):
            h1 = pcg(i * 2 + 1)
            h2 = pcg(h1)
            u1, u2 = unit(h1), unit(h2)
            if mode == "theta":
                th = u1 * theta_max
            else:
                th = math.acos(1.0 - u1 * omc)
            ph = 2 * math.pi * u2
            rr = R * math.sin(th) / math.sin(theta_max)
            pts.append((cx + rr * math.cos(ph), cy + rr * math.sin(ph)))
        b.append(dots(pts, RED if mode == "theta" else GREEN, 2.2, 0.75))
    # the numeric example, beside
    x = 620
    lines = ["the demo's cone: θmax = 0.35 rad (20°)",
             "cap height 1 - cos 0.35 = 0.0606",
             "",
             "the median sample, u = 0.5:",
             "  h = 0.5 × 0.0606 = 0.0303",
             "  cos θ = 1 - h = 0.9697",
             "  θ = 0.2469 rad  (14.1°)",
             "  uniform in θ would say 0.175 rad",
             "",
             "sin θ = sqrt(h (2 - h)) = 0.2443",
             "never sqrt(1 - cos²θ): no digit",
             "is lost to a subtraction near 1"]
    for i, t in enumerate(lines):
        b.append(label(x, 70 + 18 * i, t, "xs mono" if t.startswith("  ") else "xs", "start"))
    b.append(label(20, H - 12, "Drawn at θmax = 40° so the rings can be seen; "
                   "700 samples each, from pcg_hash.", "xs muted", "start"))
    desc = ("Two top-down views of a cone's cap with 700 sample directions each. Drawing the "
            "angle uniformly crowds samples near the axis; drawing its cosine uniformly spreads "
            "them evenly. A worked example gives the median direction of the demo's 0.35 radian "
            "cone as 0.2469 radians, not 0.175.")
    write("l618b_fig5.svg", svg(uid, W, H, "Uniform over the cap", desc, b))


# ===========================================================================
# Figure 6 — the ring  (§6)
# ===========================================================================
def fig_ring():
    uid = "l618bf6"
    W, H = 880, 356
    b = [cmarker(uid, "amb", AMBER)]
    b.append(label(20, 22, "A pool is a ring: serial s lives in slot s & mask, and a step's "
                           "births overwrite the oldest particles.", "sm", "start"))
    cx, cy, R = 170, 178, 108
    n = 16
    first, count = 13, 5
    for s in range(n):
        a = -math.pi / 2 + 2 * math.pi * s / n
        x = cx + R * math.cos(a)
        y = cy + R * math.sin(a)
        born = ((s - first) & 15) < count
        b.append(box(x - 14, y - 11, 28, 22, colour=AMBER if born else BLUE,
                     opacity=0.45 if born else 0.16))
        b.append(label(x, y + 4, str(s), "xs mono"))
    b.append(label(cx, cy - 6, "16 slots", "xs"))
    b.append(label(cx, cy + 10, "mask = 15", "xs mono"))
    b.append(label(cx, cy + 40, "this step: first = 13", "xs mono"))
    b.append(label(cx, cy + 54, "count = 5", "xs mono"))
    b.append(label(30, H - 18, "amber: respawned this step (13, 14, 15, 0, 1)", "xs", "start"))
    b.append(label(30, H - 4, "blue: stepped, if alive", "xs", "start"))

    # the wrap table
    x = 360
    b.append(label(x, 58, "the real pool, across the counter's wrap", "xs muted", "start"))
    b.append(label(x, 78, "serial", "xs muted", "start"))
    b.append(label(x + 150, 78, "s & 1023", "xs muted", "start"))
    b.append(label(x + 250, 78, "s % 1000", "xs muted", "start"))
    serials = [4294967293, 4294967294, 4294967295, 0, 1, 2]
    for i, s in enumerate(serials):
        y = 98 + 17 * i
        b.append(label(x, y, f"{s:>10}", "xs mono", "start"))
        b.append(label(x + 150, y, str(s & 1023), "xs mono t-ok", "start"))
        b.append(label(x + 250, y, str(s % 1000), "xs mono " + ("t-bad" if i == 3 else ""), "start"))
    b.append(label(x, 214, "A power of two divides 2^32, so the mask is continuous where the",
                   "xs", "start"))
    b.append(label(x, 228, "counter wraps; 1,000 jumps from slot 295 to 0 (after 59.7 hours).",
                   "xs", "start"))
    b.append(label(x, 256, "A slot comes round every capacity / rate seconds:", "xs", "start"))
    b.append(label(x, 272, "65,536 / 20,000 = 3.28 s > 3.0 s life   ->   0 cut short", "xs mono t-ok", "start"))
    b.append(label(x, 288, f"32,768 / 20,000 = 1.64 s   ->   {fmt_int(POOL_CUT[0])} of "
                   f"{fmt_int(POOL_CUT[1])} cut short", "xs mono t-bad", "start"))
    b.append(label(x, 304, f"(76.4%), each losing {POOL_CUT[2]:.3f} s of life on average",
                   "xs mono t-bad", "start"))
    desc = ("A ring of sixteen slots in which serial numbers 13 to 17 are born this step into "
            "slots 13, 14, 15, 0 and 1. A table shows real serials crossing 2^32 mapping to "
            "consecutive slots under a power-of-two mask but jumping under modulo 1,000. The "
            "pool must outlast the longest life: 65,536 slots cut nothing short at 20,000 a "
            "second; 32,768 cut 152,704 of 200,000.")
    write("l618b_fig6.svg", svg(uid, W, H, "Emission as a ring", desc, b))


# ===========================================================================
# Figure 7 — births through the step  (§6)
# ===========================================================================
def fig_shells():
    uid = "l618bf7"
    W, H = 880, 372
    b = []
    b.append(label(20, 22, "The first 0.4 s of flight, side on (x across, height up), for a "
                           "fountain whose speeds span 6.9 to 7 m/s.", "sm", "start"))
    pw, ph = 380, 270
    for k, (name, title, cls, lumpy) in enumerate([
            ("l618b_e_shells.csv", "every birth at the step's end", "t-bad", LUMPY["narrow_shells"]),
            ("l618b_e_spread.csv", "births spread through the step (birth_age)", "t-ok",
             LUMPY["narrow_spread"])]):
        x = 40 + k * 430
        y = 50
        b.append(frame(x, y, pw, ph))
        b.append(label(x + pw / 2, y - 8, title, f"xs {cls}"))
        rows = read_csv(name)
        # x in [-0.6, 0.6] m, y in [0, 2.4] m
        pts = []
        for r in rows:
            px = float(r["x"])
            py = float(r["y"])
            if -0.6 <= px <= 0.6 and 0.0 <= py <= 2.4:
                pts.append((x + (px + 0.6) / 1.2 * pw, y + ph - py / 2.4 * ph))
        b.append(dots(pts, RED if k == 0 else GREEN, 1.6, 0.7))
        b.append(label(x, y + ph + 16, f"bin-to-bin change / mean, 5 mm bins: {lumpy:.3f}",
                       "xs mono", "start"))
    b.append(label(40, H - 10, f"With speeds over 4 to 7 m/s the two measure "
                   f"{LUMPY['wide_shells']:.3f} and {LUMPY['wide_spread']:.3f}: a step's births "
                   "spread 3 m/s apart smear the shells by themselves.", "xs muted", "start"))
    desc = ("Two side-on scatter plots of young particles. When every birth of a step happens at "
            "the step's end the fountain comes out in horizontal shells about 11.7 cm apart; "
            "spreading births through the step removes them. Lumpiness 0.912 against 0.213.")
    write("l618b_fig7.svg", svg(uid, W, H, "Shells", desc, b))


# ===========================================================================
# Figure 8 — the bounce  (§7)
# ===========================================================================
def drop(h, e=0.45, g=9.81, t_end=1.6):
    """The harness's step, in double: semi-implicit Euler with the ground clamp."""
    y, v, t = 1.0, 0.0, 0.0
    pts = [(0.0, 1.0)]
    while t < t_end:
        v = v - g * h
        y = y + v * h
        if y < 0.0 and v < 0.0:
            y = 0.0
            v = -v * e
        t += h
        pts.append((t, y))
    return pts


def fig_bounce():
    uid = "l618bf8"
    W, H = 880, 330
    b = []
    b.append(label(20, 22, "A spark dropped from 1 m onto the floor, restitution e = 0.45: "
                           "each bounce should keep e² = 0.2025 of the height.", "sm", "start"))
    x0, y0, pw, ph = 60, 44, 560, 240
    b.append(frame(x0, y0, pw, ph))
    t_end = 1.6

    def to(pt):
        return (x0 + pt[0] / t_end * pw, y0 + ph - pt[1] * ph)

    for yy in (0.25, 0.5, 0.75, 1.0):
        b.append(rule(x0, y0 + ph - yy * ph, x0 + pw, y0 + ph - yy * ph, "grid", 0.6, "2 3"))
        b.append(label(x0 - 6, y0 + ph - yy * ph + 4, f"{yy:.2f}", "xs mono muted", "end"))
    for tt in (0.4, 0.8, 1.2, 1.6):
        b.append(label(x0 + tt / t_end * pw, y0 + ph + 14, f"{tt:.1f} s", "xs mono muted"))
    fine = [to(p) for p in drop(1.0 / 240.0) if p[0] <= t_end]
    coarse = [to(p) for p in drop(1.0 / 60.0) if p[0] <= t_end]
    b.append(poly(fine, GREEN, 1.4, close=False))
    b.append(poly(coarse, BLUE, 1.4, close=False))
    x = 650
    b.append(cline(x, 62, x + 24, 62, BLUE, 2))
    b.append(label(x + 30, 66, "h = 1/60", "xs", "start"))
    b.append(cline(x, 80, x + 24, 80, GREEN, 2))
    b.append(label(x + 30, 84, "h = 1/240", "xs", "start"))
    # The ratios as verify_618b §F printed them, from its unrounded peaks —
    # recomputing them here from the four-decimal peaks gave 0.104 for 0.103.
    lines = [("peaks at h = 1/60", ""), ("", f"{PEAKS[0]:.4f}  {PEAKS[1]:.4f}  {PEAKS[2]:.4f} m"),
             ("", "ratios 0.185, 0.172, 0.103"),
             ("peaks at h = 1/240", ""),
             ("", f"{PEAKS_FINE[0]:.4f}  {PEAKS_FINE[1]:.4f}  {PEAKS_FINE[2]:.4f} m"),
             ("", "ratios 0.197, 0.196, 0.181")]
    for i, (a, v) in enumerate(lines):
        b.append(label(x, 114 + 16 * i, a if a else v, "xs" if a else "xs mono", "start"))
    b.append(label(x, 226, "The third bounce is 3 mm high and", "xs", "start"))
    b.append(label(x, 240, "lasts three steps: a step that size", "xs", "start"))
    b.append(label(x, 254, "cannot resolve it. A quarter of the", "xs", "start"))
    b.append(label(x, 268, "step brings the ratios toward e².", "xs", "start"))
    desc = ("Height against time for a particle dropped from one metre, stepped at 1/60 s and "
            "at 1/240 s. The coarse step's bounces keep 0.185, 0.172 and 0.103 of the previous "
            "height; the fine step's keep 0.197, 0.196 and 0.181, approaching the continuous "
            "e squared of 0.2025.")
    write("l618b_fig8.svg", svg(uid, W, H, "The bounce, stepped", desc, b))


# ===========================================================================
# Figure 9 — a billboard from two IDs  (§8)
# ===========================================================================
def fig_billboard():
    uid = "l618bf9"
    W, H = 880, 330
    b = [cmarker(uid, "x", RED), cmarker(uid, "y", GREEN), cmarker(uid, "amb", AMBER)]
    b.append(label(20, 22, "No vertex buffer: the instance ID finds the particle, the vertex "
                           "ID finds the corner.", "sm", "start"))
    # the chain
    chain = [("SV_InstanceID", "e.g. i = 2"), ("alive[i]", "e.g. slot 40,961"),
             ("particles[slot]", "position, age, life"), ("SV_VertexID", "v = 0 .. 5")]
    for k, (a, c) in enumerate(chain[:3]):
        x = 20 + k * 150
        b.append(box(x, 44, 128, 44, colour=AMBER, opacity=0.18))
        b.append(label(x + 64, 62, a, "xs mono"))
        b.append(label(x + 64, 78, c, "xs muted"))
        if k < 2:
            b.append(carrow(x + 130, 66, x + 148, 66, uid, "amb", AMBER, 1.2))
    b.append(label(20, 112, "The list is the compaction's; the count of instances is in the "
                   "indirect arguments, never on the CPU.", "xs", "start"))

    # the quad
    cx, cy, s = 200, 222, 70
    corners = [(-1, -1), (1, -1), (1, 1), (-1, -1), (1, 1), (-1, 1)]
    b.append(hollow(cx - s, cy - s, 2 * s, 2 * s, GREY, dash="3 3", rx=0))
    tri1 = [(cx - s, cy + s), (cx + s, cy + s), (cx + s, cy - s)]
    tri2 = [(cx - s, cy + s), (cx + s, cy - s), (cx - s, cy - s)]
    b.append(poly(tri1, AMBER, 1.2))
    b.append(poly(tri2, AMBER, 1.2, dash="4 2"))
    # Two corners are shared by both triangles, so they carry two vertex IDs.
    corner_names = {(-1, -1): "v0, v3", (1, -1): "v1", (1, 1): "v2, v4", (-1, 1): "v5"}
    for (u, w), name in corner_names.items():
        px = cx + u * (s + 8)
        py = cy - w * s + (14 if w < 0 else -6)
        b.append(label(px, py, name, "xs mono", "start" if u > 0 else "end"))
    b.append(carrow(cx, cy, cx + s * 0.9, cy, uid, "x", RED, 1.6))
    b.append(carrow(cx, cy, cx, cy - s * 0.9, uid, "y", GREEN, 1.6))
    # Labels OUTSIDE the quad: inside, every position lies on a triangle's edge.
    b.append(label(cx + s + 8, cy + 4, "right", "xs lbl-x", "start"))
    b.append(label(cx, cy - s - 8, "up", "xs lbl-y"))
    b.append(label(cx, cy + s + 36, "two counter-clockwise triangles, facing the camera", "xs"))

    x = 440
    lines = ["corner = table[v]           (-1,-1) (1,-1) (1,1) (-1,-1) (1,1) (-1,1)",
             "right  = row 0 of view_from_world     (what the view maps to +x)",
             "up     = row 1 of view_from_world",
             "centre = lerp(previous, position, alpha)",
             "world  = centre + (right * c.x + up * c.y) * radius",
             "",
             "worked, the demo's shot: eye (3.7633, 2.3, 6.1382) looking at (0, 1.3, 0)",
             "  right = (0.8525, 0, -0.5227)    up = (-0.0719, 0.9905, -0.1173)",
             "  view * right = (1, 0, 0) and view * up = (0, 1, 0), as rows must",
             "  a spark at (0.10, 1.00, 0.20), radius 0.03, corner v2 = (1, 1):",
             "  (0.1234, 1.0297, 0.1808) — computed by the engine's own look_at"]
    for i, t in enumerate(lines):
        b.append(label(x, 150 + 16 * i, t, "xs mono", "start"))
    desc = ("A chain from SV_InstanceID through the alive list to a particle's slot, and a quad "
            "made of two counter-clockwise triangles whose six corners come from a table indexed "
            "by SV_VertexID and are placed along the camera's right and up vectors.")
    write("l618b_fig9.svg", svg(uid, W, H, "A billboard from two IDs", desc, b))


# ===========================================================================
# Figure 10 — the lost update  (§9)
# ===========================================================================
def fig_race():
    uid = "l618bf10"
    W, H = 880, 330
    b = []
    b.append(label(20, 22, "Two lanes increment one counter. A read, an add and a write are "
                           "three steps; an atomic is one.", "sm", "start"))

    def lane_row(y, name, steps, colour):
        b.append(label(20, y + 16, name, "xs", "start"))
        for k, t in enumerate(steps):
            if not t:
                continue
            x = 110 + k * 92
            b.append(box(x, y, 86, 24, colour=colour, opacity=0.2))
            b.append(label(x + 43, y + 16, t, "xs mono"))

    b.append(label(20, 50, "words[0] = words[0] + 1", "xs mono t-bad", "start"))
    lane_row(60, "lane A", ["read 7", "add -> 8", "write 8", ""], BLUE)
    lane_row(90, "lane B", ["read 7", "add -> 8", "", "write 8"], PURPLE)
    b.append(label(110, 134, "memory: 7 -> 8 -> 8   (two increments, one counted)",
                   "xs mono t-bad", "start"))

    b.append(label(20, 170, "InterlockedAdd(words[0], 1, before)", "xs mono t-ok", "start"))
    lane_row(180, "lane A", ["7 -> 8", "", "", ""], BLUE)
    lane_row(210, "lane B", ["", "8 -> 9", "", ""], PURPLE)
    b.append(label(110, 254, "memory: 7 -> 8 -> 9   (each lane gets what it replaced)",
                   "xs mono t-ok", "start"))

    x = 520
    b.append(hollow(x, 44, 340, 118, GREY))
    b.append(label(x + 12, 64, "measured, 1,048,576 threads, one counter", "xs muted", "start"))
    b.append(label(x + 12, 88, f"read-add-write      {RACY[0]:,} to {RACY[1]:,}", "xs mono t-bad", "start"))
    b.append(label(x + 12, 106, f"InterlockedAdd      {ATOMIC:>9,}", "xs mono t-ok", "start"))
    b.append(label(x + 12, 130, "Over 99.9% of the increments lost: the lanes of", "xs", "start"))
    b.append(label(x + 12, 144, "a SIMD group read in lock-step and write the same", "xs", "start"))
    b.append(label(x + 12, 158, "value, and groups overlap one another.", "xs", "start"))
    b.append(label(x, 200, "The value an atomic returns is the lane's", "xs", "start"))
    b.append(label(x, 214, "PLACE: no other lane receives it.", "xs", "start"))
    b.append(label(20, H - 30, f"The compaction does exactly this with the real pool: "
                   f"{fmt_int(ALIVE_COUNT)} living particles, {fmt_int(ALIVE_COUNT)} places taken, "
                   "and the same count the CPU gets.", "xs", "start"))
    b.append(label(20, H - 12, "Fifteen trials counted 296 to 509: which increments are lost is "
                   "a property of the schedule, and the schedule is not ours.", "xs muted", "start"))
    desc = ("Timelines of two lanes. With a plain read-add-write both read 7 and both write 8, "
            "so one increment is lost. With InterlockedAdd the lanes are serialised on the "
            "counter and each receives the value it replaced. Measured with a million threads: "
            "between 296 and 509 counted against 1,048,576.")
    write("l618b_fig10.svg", svg(uid, W, H, "The lost update", desc, b))


# ===========================================================================
# Figure 11 — counting and drawing without the CPU  (§9)
# ===========================================================================
def fig_count():
    uid = "l618bf11"
    W, H = 880, 360
    b = [cmarker(uid, "amb", AMBER)]
    b.append(label(20, 22, "The count of the living is produced, consumed and forgotten on the "
                           "GPU.", "sm", "start"))
    stages = [("clear", "args := {6, 0, 0, 0}", "compute, 1 thread"),
              ("compact", "if alive: place = add(args[1], 1)", "compute, 1 per slot"),
              ("", "alive[place] = slot", ""),
              ("draw", "DrawGPUPrimitivesIndirect", "render, reads args")]
    xs = [20, 230, 230, 560]
    ys = [52, 52, 52, 52]
    b.append(box(20, 44, 190, 64, colour=AMBER, opacity=0.18))
    b.append(label(115, 64, "clear", "sm"))
    b.append(label(115, 82, "args := {6, 0, 0, 0}", "xs mono"))
    b.append(label(115, 98, "one thread, its own pass", "xs muted"))
    b.append(carrow(212, 76, 238, 76, uid, "amb", AMBER, 1.3))
    b.append(box(240, 44, 290, 64, colour=AMBER, opacity=0.18))
    b.append(label(385, 64, "compact", "sm"))
    b.append(label(385, 82, "place = InterlockedAdd(args[1], 1)", "xs mono"))
    b.append(label(385, 98, "alive[place] = slot", "xs mono"))
    b.append(carrow(532, 76, 558, 76, uid, "amb", AMBER, 1.3))
    b.append(box(560, 44, 300, 64, colour=AMBER, opacity=0.18))
    b.append(label(710, 64, "draw", "sm"))
    b.append(label(710, 82, "SDL_DrawGPUPrimitivesIndirect", "xs mono"))
    b.append(label(710, 98, "6 vertices x args[1] instances", "xs muted"))
    b.append(label(20, 134, f"args after compaction: {{6, {ALIVE_COUNT:,}, 0, 0}} — the CPU's "
                   f"count, exactly; the list holds exactly the CPU's {ALIVE_COUNT:,} living "
                   "slots, once each.", "xs", "start"))
    b.append(label(20, 150, f"Compacted twice, {ORDER_MOVED[0]:,} to {ORDER_MOVED[1]:,} of "
                   f"{ALIVE_COUNT:,} places hold a different slot: the ORDER is the atomics', and it "
                   "changes every frame.", "xs", "start"))

    # histogram of ULP differences from k_order.csv
    rows = read_csv("l618b_k_order.csv")
    counts = {}
    for r in rows:
        u = int(r["ulp"])
        counts[u] = counts.get(u, 0) + 1
    x0, y0, pw, ph = 60, 186, 480, 130
    b.append(frame(x0, y0, pw, ph))
    mx = max(counts.values())
    top = max(counts)
    bw = pw / (top + 1)
    for u in range(1, top + 1):
        c = counts.get(u, 0)
        hgt = 0 if c == 0 else max(1.0, math.log10(c + 1) / math.log10(mx + 1) * (ph - 26))
        b.append(box(x0 + (u - 0.5) * bw + 2, y0 + ph - hgt, bw - 4, hgt, colour=PURPLE, opacity=0.5, rx=0))
        b.append(label(x0 + (u - 0.5) * bw + bw / 2 - 2, y0 + ph + 14, str(u), "xs mono muted"))
        if c:
            b.append(label(x0 + (u - 0.5) * bw + bw / 2 - 2, y0 + ph - hgt - 4, fmt_int(c), "xs mono"))
    b.append(label(x0 + pw / 2, y0 + ph + 30, "half-float ULPs between two draws of one state "
                   "(log scale)", "xs muted"))
    x = 580
    lines = ["the same pool, drawn after two compactions:",
             f"about 7,000 to 9,000 of {fmt_int(GRAPH_CHANNELS)} channels",
             f"differ, the worst by 8 to {K_WORST_ULP} half-float ULPs,",
             "and the total light agrees to 1 part in 10^4.",
             "",
             "Addition is commutative in the maths;",
             "sixteen-bit float addition rounds after",
             "every add, so the ORDER moves the last",
             "bits — and moves nothing else."]
    for i, t in enumerate(lines):
        b.append(label(x, 200 + 16 * i, t, "xs", "start"))
    desc = ("A clear pass sets the indirect arguments to six vertices and zero instances; the "
            "compaction adds one per living particle with an atomic and writes each slot into "
            "the list at the place it received; the draw reads the arguments. A histogram shows "
            "that two draws of one state differ by at most eleven half-float units in the last "
            "place.")
    write("l618b_fig11.svg", svg(uid, W, H, "Counting without the CPU", desc, b))


# ===========================================================================
# Figure 12 — cycle, set wrong  (§10)
# ===========================================================================
def fig_cycle():
    uid = "l618bf12"
    W, H = 880, 386
    b = []
    b.append(label(20, 22, "cycle = true on a buffer the next pass READS: each pass is handed an "
                           "internal buffer whose contents SDL calls undefined.", "sm", "start"))
    # verify_618b §I: a fresh pool, six steps, read back after each (alive, lowest serial)
    back = [(333, 0, 332), (333, 333, 665), (334, 666, 999), (666, 0, 1332), (666, 333, 1665),
            (668, 666, 1999)]
    fence = [(333, 0, 332), (333, 333, 665), (334, 666, 999), (333, 1000, 1332), (333, 1333, 1665),
             (334, 1666, 1999)]
    cpu = [333, 666, 1000, 1333, 1666, 2000]
    rows = [("submitted back to back", back, [AMBER, PURPLE, BLUE, AMBER, PURPLE, BLUE]),
            ("a fence wait after each step", fence, [GREY] * 6)]
    y = 50
    for name, data, cols in rows:
        b.append(label(20, y, name, "xs muted", "start"))
        for k, ((alive, lo, hi), col) in enumerate(zip(data, cols)):
            x = 20 + k * 140
            b.append(box(x, y + 8, 128, 40, colour=col, opacity=0.25))
            b.append(label(x + 64, y + 24, f"step {k + 1}: {alive:,} alive", "xs mono"))
            b.append(label(x + 64, y + 40, f"serials {lo:,}..{hi:,}", "xs mono muted"))
        y += 70
    b.append(label(20, y - 4, "the CPU after each step: " + ", ".join(f"{c:,}" for c in cpu)
                   + " alive", "xs mono t-ok", "start"))
    b.append(label(20, y + 14, "Back to back, the pool became THREE pools (same colour = same "
                   "internal buffer): step 4 continued step 1's, holding serials 0..332 and "
                   "1,000..1,332.", "xs", "start"))
    b.append(label(20, y + 30, "With a fence wait, every step began from a buffer holding no live "
                   "particle. Which buffer SDL picks is the backend's business; that it is not "
                   "this one is the contract.", "xs", "start"))

    y0 = y + 62
    b.append(label(20, y0, "120 steps, one command buffer each (the CPU: "
                   f"{fmt_int(CYCLE_CPU)} alive)", "xs muted", "start"))
    for i, (name, alive, ok) in enumerate(CYCLE_ARMS):
        yy = y0 + 20 + 20 * i
        frac = alive / CYCLE_CPU
        b.append(label(20, yy, name, "xs mono " + ("t-ok" if ok else "t-bad"), "start"))
        b.append(box(330, yy - 11, max(2.0, 360 * frac), 13, colour=GREEN if ok else RED,
                     opacity=0.45, rx=0))
        b.append(label(700, yy, f"{alive:,} alive", "xs mono", "start"))
    b.append(label(20, H - 10, "SDL takes a reference when a command buffer first uses a buffer, "
                   "and drops it only when that command buffer is cleaned up after completing.",
                   "xs muted", "start"))
    desc = ("Two rows of six steps each with the pool read back after every step. Submitted back "
            "to back with cycle true, steps 1 to 3 each start from an empty buffer and steps 4 to 6 "
            "continue the buffers of steps 1 to 3, so the pool becomes three pools. With a fence "
            "wait after each step every step starts from an empty buffer. Below, 120 steps: cycle "
            "false gives the CPU's 38,284 living particles, cycle true gives 668 or 334, and only "
            "waiting for the whole device to go idle restores 38,284.")
    write("l618b_fig12.svg", svg(uid, W, H, "cycle, set wrong", desc, b))


# ===========================================================================
# Figure 13 — the particle frame, declared  (§11)
# ===========================================================================
def fig_graph():
    uid = "l618bf13"
    W, H = 880, 380
    b = [cmarker(uid, "amb", AMBER), cmarker(uid, "blu", BLUE)]
    b.append(label(20, 22, "What the demo declares, and what the graph derives from it (in "
                           "amber).", "sm", "start"))
    passes = ["step 0", "step 1", "step 2", "clear args", "compact", "scene", "sparks"]
    res = ["particle state", "draw args", "alive list", "hdr", "depth"]
    x0, y0, cw, rh = 128, 60, 106, 48
    for j, p in enumerate(passes):
        x = x0 + j * cw
        kind = "compute" if j < 5 else "render"
        b.append(box(x + 6, y0 - 8, cw - 12, 22, colour=AMBER if j < 5 else BLUE, opacity=0.25))
        b.append(label(x + cw / 2, y0 + 7, p, "xs mono"))
        b.append(label(x + cw / 2, y0 + 28, kind, "xs muted"))
    for i, r in enumerate(res):
        y = y0 + 40 + i * rh
        b.append(label(x0 - 10, y + 20, r, "xs", "end"))
        b.append(rule(x0, y + 16, x0 + len(passes) * cw, y + 16, "grid", 0.6, "2 3"))
    cells = {
        (0, 0): ("keep  @0->1", "cycle no"), (0, 1): ("keep  @1->2", "cycle no"),
        (0, 2): ("keep  @2->3", "cycle no"), (0, 4): ("read  @3", ""), (0, 6): ("read  @3", ""),
        (1, 3): ("write @0->1", "cycle YES"), (1, 4): ("keep  @1->2", "cycle no"),
        (1, 6): ("read  @2", "(indirect)"),
        (2, 4): ("write @0->1", "cycle YES"), (2, 6): ("read  @1", ""),
        (3, 5): ("clear @0->1", "STORE"), (3, 6): ("keep  @1->2", "LOAD"),
        (4, 5): ("clear @0->1", "STORE"), (4, 6): ("keep  @1->2", "LOAD, DONT_CARE"),
    }
    for (i, j), (a, d) in cells.items():
        x = x0 + j * cw
        y = y0 + 40 + i * rh
        b.append(box(x + 6, y + 2, cw - 12, 30, colour=GREY, opacity=0.10))
        b.append(label(x + cw / 2, y + 14, a, "xs mono"))
        if d:
            b.append(label(x + cw / 2, y + 27, d, "xs t-hi"))
    b.append(label(20, H - 42, "Schedule, from the declarations alone: step 0, step 1, step 2, "
                   "clear args, compact, scene, sparks. (The harness's frame, without the scene, "
                   "is the same order.)", "xs", "start"))
    b.append(label(20, H - 26, "hdr is imported (the post stack reads it next), so its last "
                   "store is STORE; depth is the graph's own, so the sparks' pass ends DONT_CARE.",
                   "xs", "start"))
    b.append(label(20, H - 10, "Refused at compile: a render pass that writes a buffer, a compute "
                   "pass with an attachment, read-write slots with a gap or a repeat.", "xs muted",
                   "start"))
    desc = ("A grid of the demo's seven passes against its five resources. The three steps keep "
            "the particle state and are derived cycle no; the clear writes the arguments with "
            "cycle yes; the compaction keeps the arguments and writes the list; the sparks' "
            "render pass reads all three buffers and keeps the scene's colour and depth.")
    write("l618b_fig13.svg", svg(uid, W, H, "The particle frame, declared", desc, b))


# ===========================================================================
# Figure 14 — CPU and GPU, particle by particle  (§13)
# ===========================================================================
def fig_agree():
    uid = "l618bf14"
    W, H = 880, 340
    b = []
    b.append(label(20, 22, "The same 600 steps on both processors: which numbers agree exactly, "
                           "and how far the rest drift.", "sm", "start"))
    rows = read_csv("l618b_g_agreement.csv")
    x0, y0, pw, ph = 70, 50, 460, 220
    b.append(frame(x0, y0, pw, ph))
    tmin, tmax = math.log10(0.1), math.log10(10.0)

    def tx(t):
        return x0 + (math.log10(t) - tmin) / (tmax - tmin) * pw

    def uy(um):
        return y0 + ph - um / 2.0 * ph

    for um in (0.5, 1.0, 1.5, 2.0):
        b.append(rule(x0, uy(um), x0 + pw, uy(um), "grid", 0.6, "2 3"))
        b.append(label(x0 - 6, uy(um) + 4, f"{um:.1f}", "xs mono muted", "end"))
    for t in (0.1, 0.25, 0.5, 1, 2, 5, 10):
        b.append(label(tx(t), y0 + ph + 14, f"{t:g} s", "xs mono muted"))
    b.append(label(x0 - 44, y0 + ph / 2, "µm", "xs muted"))
    pts = [(tx(int(r["step"]) / 60.0), uy(float(r["worst_um"]))) for r in rows]
    b.append(poly(pts, AMBER, 1.6, close=False))
    for p in pts:
        b.append(dot(p[0], p[1], 2.2, AMBER))
    b.append(label(x0 + pw / 2, y0 + ph + 30, "simulated time (log); the worst live particle's "
                   "distance from its CPU twin", "xs muted"))
    x = 560
    last = rows[-1]
    lines = [("EXACT, every step, both processors", "t-ok"),
             ("  every serial in every slot", ""),
             ("  which particles are alive: 0 disagree", ""),
             (f"  the count: {int(last['alive_cpu']):,} = {int(last['alive_gpu']):,}", ""),
             ("  1,048,576 hashes, bit for bit", ""),
             ("WITHIN A BOUND", "t-hi"),
             (f"  worst {float(last['worst_um']):.2f} µm after 10 s", ""),
             (f"  {int(last['within_1um']):,} of {int(last['alive_cpu']):,} within 1 µm", ""),
             (f"  only {int(last['live_identical']):,} bit-identical", ""),
             ("AND THE CPU AGAINST ITSELF", "t-bad"),
             (f"  debug vs Release library: {DEBUG_RELEASE[0]:,} of", ""),
             (f"  {DEBUG_RELEASE[1]:,} differ, worst {DEBUG_RELEASE[2]:.2f} µm", "")]
    for i, (t, cls) in enumerate(lines):
        b.append(label(x, 60 + 17 * i, t, ("xs " + cls) if cls else "xs mono", "start"))
    desc = ("A plot of the worst distance between a live particle and its CPU twin, rising to "
            "1.67 micrometres by two seconds and staying there to ten. Beside it, the exact "
            "agreements (serials, alive flags, counts, hashes), the bounded ones, and the CPU "
            "reference's own disagreement between debug and Release builds.")
    write("l618b_fig14.svg", svg(uid, W, H, "CPU and GPU, particle by particle", desc, b))


# ===========================================================================
# Figure 15 — the budget  (§13)
# ===========================================================================
def fig_budget():
    uid = "l618bf15"
    W, H = 880, 340
    b = []
    b.append(label(20, 22, "Milliseconds per frame against pool size, Release library (log-log). "
                           "The frame is 16.7 ms.", "sm", "start"))
    rows = read_csv("l618b_budget.csv")
    x0, y0, pw, ph = 80, 50, 520, 230
    b.append(frame(x0, y0, pw, ph))
    cmin, cmax = math.log10(16384 / 1.3), math.log10(1048576 * 1.3)
    mmin, mmax = math.log10(0.01), math.log10(30.0)

    def cx(c):
        return x0 + (math.log10(c) - cmin) / (cmax - cmin) * pw

    def my(ms):
        return y0 + ph - (math.log10(ms) - mmin) / (mmax - mmin) * ph

    for ms in (0.01, 0.1, 1.0, 10.0):
        b.append(rule(x0, my(ms), x0 + pw, my(ms), "grid", 0.6, "2 3"))
        b.append(label(x0 - 6, my(ms) + 4, f"{ms:g}", "xs mono muted", "end"))
    b.append(cline(x0, my(16.667), x0 + pw, my(16.667), RED, 1.0, "5 3"))
    b.append(label(x0 + pw - 4, my(16.667) - 5, "16.7 ms", "xs t-bad", "end"))
    for r in rows:
        c = int(r["capacity"])
        b.append(label(cx(c), y0 + ph + 14, fmt_int(c), "xs mono muted"))
    series = [("cpu_ms", BLUE, "CPU step"), ("upload_ms", PURPLE, "upload (CPU path)"),
              ("gpu_ms", AMBER, "GPU step")]
    for key, colour, _ in series:
        pts = [(cx(int(r["capacity"])), my(float(r[key]))) for r in rows]
        b.append(poly(pts, colour, 1.8, close=False))
        for p in pts:
            b.append(dot(p[0], p[1], 2.6, colour))
    tot = [(cx(int(r["capacity"])), my(float(r["cpu_ms"]) + float(r["upload_ms"]))) for r in rows]
    b.append(poly(tot, GREY, 1.4, dash="4 3", close=False))
    b.append(label(x0 + pw / 2, y0 + ph + 30, "slots in the pool (about 69% alive)", "xs muted"))
    x = 630
    for i, (key, colour, name) in enumerate(series):
        b.append(cline(x, 62 + 18 * i, x + 22, 62 + 18 * i, colour, 2.2))
        b.append(label(x + 28, 66 + 18 * i, name, "xs", "start"))
    b.append(cline(x, 116, x + 22, 116, GREY, 1.6, "4 3"))
    b.append(label(x + 28, 120, "CPU step + upload", "xs", "start"))
    r1m = rows[-1]
    lines = ["at 1,048,576:",
             f"  CPU {float(r1m['cpu_ms']):.2f} + upload {float(r1m['upload_ms']):.2f} ms",
             f"  GPU {float(r1m['gpu_ms']):.3f} ms per step",
             "",
             "the upload outgrows the step:",
             "about 0.14 ms a MB plus 0.14 ms",
             "of submit, on unified memory.",
             "",
             "CPU: 3.3 to 3.6 ns a slot;",
             "GPU: 0.31 to 0.79 ns, launch",
             "included (60 steps, one buffer)."]
    for i, t in enumerate(lines):
        b.append(label(x, 150 + 15 * i, t, "xs mono" if t.startswith("  ") else "xs", "start"))
    desc = ("A log-log chart of milliseconds against pool size from 16,384 to 1,048,576. The CPU "
            "step and the upload both grow linearly; the upload overtakes the step and the two "
            "together reach 10.6 milliseconds at a million particles, while the GPU step stays "
            "under 0.4 milliseconds.")
    write("l618b_fig15.svg", svg(uid, W, H, "The budget", desc, b))


# ===========================================================================
# Figure 16 — the demo  (§14, expected result)
# ===========================================================================
def fig_demo():
    uid = "l618bf16"
    w, h, data = read_ppm(os.path.join(OUT, "l618b_demo.ppm"))
    cell = 2
    palette = [hexrgb("#fff2d8"), hexrgb("#ffc070"), hexrgb("#e07a30"), hexrgb("#9ea4b0"),
               hexrgb("#ffffff")]
    gw, gh, grid = box_sample(data, w, (0, 0, w, h), cell)
    px = 4
    body = ['<g shape-rendering="crispEdges">'] + rle_rects(grid, gw, gh, 40, 40, px, palette,
                                                            levels=8, bg_class="fill-shot") + ['</g>']
    W, H = 40 + gw * px + 40, 40 + gh * px + 30
    b = [label(40, 26, "particles --shot: 65,536 slots, 2.5 s in, simulated on the GPU "
                       "(the region around the fountain).", "sm", "start")]
    b += body
    b.append(label(40, H - 10, "Palette-snapped for the page: five tints, eight levels.", "xs muted",
                   "start"))
    desc = ("The demo's frame: a fountain of warm sparks rising from the floor, brightest at the "
            "core, spraying outward and falling onto a dark floor, with a metal torus standing "
            "on edge to the right that hides the sparks falling behind it.")
    write("l618b_fig16.svg", svg(uid, W, H, "The demo", desc, b))


FIGS = [fig_bus, fig_grid, fig_layout, fig_pairs, fig_cap, fig_ring, fig_shells, fig_bounce,
        fig_billboard, fig_race, fig_count, fig_cycle, fig_graph, fig_agree, fig_budget, fig_demo]


def main():
    for f in FIGS:
        f()
    print(f"wrote {len(FIGS)} figures")


if __name__ == "__main__":
    main()
