#!/usr/bin/env python3
"""Compute every SVG figure for Lesson 4.4 from real data.

Figure 5 is drawn from `scratch/l44_edge.txt`, which `verify_44` writes: the
actual per-pixel comparison between the hardware's rasterizer and ours. Nothing
in it is drawn by hand, which is the only way a figure about a one-pixel
disagreement is worth printing.

Writes scratch/l44_fig{1..6}.svg.
"""

# ---- Measured inputs, from scratch/verify_44.log ----------------------------

SHADER_MS = 0.031          # SDL_CreateGPUShader — ~0.03 ms in every run, every config
PIPE_FIRST_MS = 31.6       # the FIRST pipeline in a process: one-time driver setup
PIPE_NEW_MS = 2.4          # each new state permutation after it: the compile
PIPE_CACHED_MS = 0.024     # a description the driver has compiled before, even in a past run
PIPE_FIRST_DEBUG_MS = 31.8 # the same first pipeline with validation on — barely different

COVERED = 20808            # px the GPU filled, 256x256 target
EXPECTED = 20972           # px the geometry says: 0.5 * 204.8 * 204.8
CPU_COVERED = 20910        # px our rasterizer filled
BOTH = 20808
GPU_ONLY = 0
CPU_ONLY = 102
INTERIOR_DISAGREEMENTS = 0

CENTROID = (85, 86, 85)

CULL_CCW = 20808
CULL_CW = 0
CULL_CW_NONE = 20808

PRIMITIVES = [("TRIANGLELIST", 20808), ("LINESTRIP", 410), ("POINTLIST", 0)]

COMPOSE = [("cleared, untouched", 22364), ("software picture", 22364), ("the triangle", 20808)]

# §B: (what, result)
REFUSALS = [
    ("correct", True),
    ("a colour format the target does not have", True),
    ("an attribute the shader never declared", True),
    ("no vertex layout, but the shader wants one", False),
]

C_GPU = "#5082e6"
C_CPU = "#5ac878"
C_BOTH = "#78b4eb"
C_BAD = "#eb786e"
C_META = "#f0961e"
C_HUB = "#5acdd7"


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
# Figure 1 — where the compile happens
# =============================================================================
# ONE CLAIM: the compile is at pipeline creation (15x a shader), and the 42 ms a
# debug build reports is mostly the validation layer, not the compile.
def fig1():
    W, H = 720, 372
    X0, XW = 232.0, 356.0
    body = []

    body.append('<text x="24" y="26" class="sm">'
                'creating a shader, and creating pipelines &#8212; four measurements of one call</text>')

    rows = [
        ("SDL_CreateGPUShader", SHADER_MS, C_HUB,
         "~0.03 ms in every run and configuration"),
        ("first pipeline in a process", PIPE_FIRST_MS, C_BAD,
         "one-time driver and compiler setup"),
        ("each new state permutation", PIPE_NEW_MS, C_GPU,
         "THE COMPILE &#8212; ~80&#215; a shader"),
        ("one compiled before", PIPE_CACHED_MS, C_CPU,
         "cached ON DISK, so it survives a restart"),
    ]

    import math
    lo, hi = 0.01, 50.0

    def px(v):
        return X0 + XW * (math.log10(max(v, lo)) - math.log10(lo)) / (math.log10(hi) - math.log10(lo))

    for i, (label, value, colour, note) in enumerate(rows):
        y = 74.0 + i * 58.0
        body.append(f'<text x="{f(X0 - 12)}" y="{f(y + 17)}" class="xs mono" '
                    f'text-anchor="end">{label}</text>')
        body.append(f'<rect x="{f(X0)}" y="{f(y)}" width="{f(px(value) - X0)}" height="26" '
                    f'rx="3" fill="{colour}" opacity="0.85"/>')
        body.append(f'<text x="{f(px(value) + 8)}" y="{f(y + 18)}" class="xs mono">'
                    f'{value:.3f} ms</text>')
        body.append(f'<text x="{f(X0 + 4)}" y="{f(y + 41)}" class="xs muted">{note}</text>')

    ay = 74.0 + len(rows) * 58.0
    body.append(f'<line x1="{f(X0)}" y1="{f(ay)}" x2="{f(X0 + XW)}" y2="{f(ay)}" '
                f'class="ink-soft" stroke-width="1.2"/>')
    for v in (0.01, 0.1, 1.0, 10.0):
        body.append(f'<line x1="{f(px(v))}" y1="{f(ay)}" x2="{f(px(v))}" y2="{f(ay + 4)}" '
                    f'class="ink-soft" stroke-width="1"/>')
        body.append(f'<text x="{f(px(v))}" y="{f(ay + 17)}" class="xs mono muted" '
                    f'text-anchor="middle">{v:g}</text>')
    body.append(f'<text x="{f(X0)}" y="{f(ay + 32)}" class="xs muted">'
                f'milliseconds (log scale)</text>')

    body.append(f'<text x="24" y="{H - 14}" class="xs">'
                f'Validation changes none of this materially: the first pipeline measures '
                f'{PIPE_FIRST_DEBUG_MS:.1f} ms with it on against {PIPE_FIRST_MS:.1f} with it off.</text>')

    return svg(W, H, "What creating a shader costs, and what creating a pipeline costs",
               f"Four horizontal bars on a logarithmic millisecond scale. Creating a shader object "
               f"takes {SHADER_MS:.3f} ms and does so in every run and every configuration. "
               f"Creating the first pipeline in a process takes {PIPE_FIRST_MS:.1f} ms, which is "
               f"one-time driver and compiler setup. Each pipeline after it whose state "
               f"permutation the driver has not seen takes about {PIPE_NEW_MS:.1f} ms — that is "
               f"the compile, roughly eighty times the cost of creating a shader. A description "
               f"the driver has compiled before takes {PIPE_CACHED_MS:.3f} ms, and because the "
               f"cache is on disk that remains true across restarts. Enabling the validation "
               f"layer changes none of these materially.",
               "\n".join(body), "l44f1")


# =============================================================================
# Figure 2 — the create-info, and what it refuses
# =============================================================================
# ONE CLAIM: nine fields, and the ones that matter most are the ones with no
# sensible zero — while most wrong descriptions are accepted without complaint.
def fig2():
    W, H = 720, 496
    body = []

    body.append('<text x="24" y="26" class="sm">'
                'SDL_GPUGraphicsPipelineCreateInfo &#8212; nine fields, and what each decides</text>')

    fields = [
        ("vertex_shader", "the program", "ours", True),
        ("fragment_shader", "the program", "ours", True),
        ("vertex_input_state", "how to read the buffer", "ours", True),
        ("primitive_type", "what three vertices MEAN", "list", True),
        ("rasterizer_state", "cull, winding, fill", "CCW / back", True),
        ("multisample_state", "samples per pixel", "1", False),
        ("depth_stencil_state", "Lesson 3.1, as a struct", "off until 4.7", False),
        ("target_info", "the format, which must MATCH", "swapchain's", True),
        ("props", "extensions", "0", False),
    ]

    X0, ROW = 30.0, 30.0
    Y0 = 66.0
    W_NAME, W_WHAT, W_OURS = 190.0, 250.0, 190.0

    for i, (name, what, ours, we_set) in enumerate(fields):
        y = Y0 + i * ROW
        colour = C_GPU if we_set else "#7a7a72"
        body.append(f'<rect x="{f(X0)}" y="{f(y)}" width="{f(W_NAME)}" height="24" rx="3" '
                    f'fill="{colour}" opacity="{0.18 if we_set else 0.10}" '
                    f'stroke="{colour}" stroke-width="1.2"/>')
        body.append(f'<text x="{f(X0 + 8)}" y="{f(y + 16)}" class="xs mono">{name}</text>')
        body.append(f'<text x="{f(X0 + W_NAME + 12)}" y="{f(y + 16)}" class="xs">{what}</text>')
        body.append(f'<text x="{f(X0 + W_NAME + W_WHAT + 12)}" y="{f(y + 16)}" '
                    f'class="xs mono muted">{ours}</text>')

    y = Y0 + len(fields) * ROW + 20
    body.append(f'<text x="{f(X0)}" y="{f(y)}" class="xs">'
                f'Nine at the top; <tspan class="t-hi">fifty-three</tspan> once the nested state '
                f'structs are expanded &#8212; the object Lesson 4.1 predicted from '
                f'<tspan class="mono">fill_style</tspan>.</text>')

    # What creation refuses.
    y += 30
    body.append(f'<text x="{f(X0)}" y="{f(y)}" class="sm">and what it refuses</text>')
    for i, (what, created) in enumerate(REFUSALS):
        ry = y + 24 + i * 24
        colour = C_BAD if created and i > 0 else C_CPU
        body.append(f'<rect x="{f(X0)}" y="{f(ry - 11)}" width="12" height="12" rx="3" '
                    f'fill="{colour}" opacity="0.85"/>')
        body.append(f'<text x="{f(X0 + 22)}" y="{f(ry)}" class="xs">{what}</text>')
        body.append(f'<text x="{f(X0 + 400)}" y="{f(ry)}" class="xs mono'
                    f'{"" if not created else " t-hi"}">'
                    f'{"created" if created else "REFUSED"}</text>')

    return svg(W, H, "The pipeline create-info, field by field, and what it checks",
               "A table of the nine top-level fields of SDL_GPUGraphicsPipelineCreateInfo with "
               "what each decides and the value this course gives it, followed by four creation "
               "trials. Only one of the four wrong descriptions is refused — the one with no "
               "vertex layout at all, where the driver reports that the vertex function has "
               "input attributes but no vertex descriptor was set. A mismatched colour target "
               "format and an attribute the shader never declared are both accepted.",
               "\n".join(body), "l44f2")


# =============================================================================
# Figure 3 — the triangle, in numbers
# =============================================================================
# ONE CLAIM: the hardware filled the area the geometry predicts, and interpolated
# the corners exactly as Lesson 2.4 derived by hand.
def fig3():
    W, H = 720, 400
    body = []

    body.append('<text x="24" y="26" class="sm">'
                'what came back from the device, checked against the geometry</text>')
    body.append('<text x="24" y="46" class="xs muted">'
                'drawn into a 256&#215;256 offscreen target and downloaded &#8212; '
                'no screenshot, no eyes</text>')

    # The clip-space square and the triangle inside it.
    S = 250.0
    X0, Y0 = 60.0, 84.0

    def cx(v):
        return X0 + (v * 0.5 + 0.5) * S

    def cy(v):
        return Y0 + (1.0 - (v * 0.5 + 0.5)) * S

    body.append(f'<rect x="{f(X0)}" y="{f(Y0)}" width="{f(S)}" height="{f(S)}" '
                f'class="fill-soft ink-soft" stroke-width="1.2"/>')
    for t in (-1.0, -0.5, 0.0, 0.5, 1.0):
        body.append(f'<line x1="{f(cx(t))}" y1="{f(Y0)}" x2="{f(cx(t))}" y2="{f(Y0 + S)}" '
                    f'class="grid" stroke-width="0.8"/>')
        body.append(f'<line x1="{f(X0)}" y1="{f(cy(t))}" x2="{f(X0 + S)}" y2="{f(cy(t))}" '
                    f'class="grid" stroke-width="0.8"/>')

    verts = [(-0.8, -0.8, "#e63946", "red"), (0.8, -0.8, "#2a9d5c", "green"),
             (0.0, 0.8, "#3d6fe6", "blue")]
    pts = " ".join(f"{f(cx(x))},{f(cy(y))}" for x, y, _c, _n in verts)
    body.append(f'<polygon points="{pts}" fill="{C_GPU}" opacity="0.20" '
                f'stroke="{C_GPU}" stroke-width="2"/>')

    for x, y, colour, name in verts:
        body.append(f'<circle cx="{f(cx(x))}" cy="{f(cy(y))}" r="5" fill="{colour}"/>')
        dy = 16 if y < 0 else -10
        body.append(f'<text x="{f(cx(x))}" y="{f(cy(y) + dy)}" class="xs mono" '
                    f'text-anchor="middle">({x:+.1f}, {y:+.1f})</text>')

    # The centroid.
    gx, gy = 0.0, -0.8 / 3.0
    body.append(f'<circle cx="{f(cx(gx))}" cy="{f(cy(gy))}" r="4" class="hi" fill="none" '
                f'stroke-width="2"/>')
    body.append(f'<text x="{f(cx(gx) + 10)}" y="{f(cy(gy) + 4)}" class="xs mono t-hi">'
                f'centroid</text>')

    body.append(f'<text x="{f(X0 + S / 2)}" y="{f(Y0 + S + 20)}" class="xs muted" '
                f'text-anchor="middle">clip space, +Y up (conventions &#167;4)</text>')

    # The numbers.
    tx = X0 + S + 40
    lines = [
        ("pixels covered", f"{COVERED:,}"),
        ("area from the geometry", f"{EXPECTED:,}"),
        ("ratio", f"{COVERED / EXPECTED:.4f}"),
        ("", ""),
        ("at the centroid", f"{CENTROID[0]} {CENTROID[1]} {CENTROID[2]}"),
        ("one third of 255", "85"),
    ]
    for i, (label, value) in enumerate(lines):
        y = Y0 + 24 + i * 26
        if not label:
            continue
        body.append(f'<text x="{f(tx)}" y="{f(y)}" class="xs">{label}</text>')
        body.append(f'<text x="{f(tx + 190)}" y="{f(y)}" class="xs mono'
                    f'{" t-hi" if i in (2, 4) else ""}" text-anchor="end">{value}</text>')

    body.append(f'<text x="{f(tx)}" y="{f(Y0 + 210)}" class="xs">'
                f'The centroid is an equal mix of all</text>')
    body.append(f'<text x="{f(tx)}" y="{f(Y0 + 226)}" class="xs">'
                f'three corners &#8212; which is Lesson 2.4&#8217;s</text>')
    body.append(f'<text x="{f(tx)}" y="{f(Y0 + 242)}" class="xs">'
                f'barycentric interpolation, in silicon,</text>')
    body.append(f'<text x="{f(tx)}" y="{f(Y0 + 258)}" class="xs">'
                f'giving the same number you derived.</text>')

    return svg(W, H, "The first GPU triangle, measured rather than admired",
               f"The triangle in clip space, with vertices at minus zero point eight, minus zero "
               f"point eight in red; zero point eight, minus zero point eight in green; and zero, "
               f"zero point eight in blue. Beside it the measurements: {COVERED:,} pixels "
               f"covered against {EXPECTED:,} predicted by the geometry, a ratio of "
               f"{COVERED / EXPECTED:.4f}; and at the centroid the three channels read "
               f"{CENTROID[0]}, {CENTROID[1]} and {CENTROID[2]}, where one third of 255 is 85 — "
               f"barycentric interpolation performed by hardware, agreeing with the arithmetic "
               f"Lesson 2.4 derived by hand.",
               "\n".join(body), "l44f3")


# =============================================================================
# Figure 4 — winding
# =============================================================================
# ONE CLAIM: swapping two vertices does not flip the triangle, it DELETES it —
# and the geometry is provably unchanged, because turning culling off brings back
# exactly the same pixel count.
def fig4():
    W, H = 720, 320
    body = []

    body.append('<text x="24" y="26" class="sm">'
                'the same three points, in two orders, with cull-back on</text>')

    S = 150.0
    cases = [
        ("counter-clockwise", [(-0.8, -0.8), (0.8, -0.8), (0.0, 0.8)], CULL_CCW, True),
        ("clockwise", [(0.8, -0.8), (-0.8, -0.8), (0.0, 0.8)], CULL_CW, False),
        ("clockwise, cull NONE", [(0.8, -0.8), (-0.8, -0.8), (0.0, 0.8)], CULL_CW_NONE, True),
    ]

    for i, (label, verts, covered, drawn) in enumerate(cases):
        X0 = 46.0 + i * 224.0
        Y0 = 72.0

        def cx(v, X0=X0):
            return X0 + (v * 0.5 + 0.5) * S

        def cy(v, Y0=Y0):
            return Y0 + (1.0 - (v * 0.5 + 0.5)) * S

        body.append(f'<rect x="{f(X0)}" y="{f(Y0)}" width="{f(S)}" height="{f(S)}" '
                    f'class="fill-soft ink-soft" stroke-width="1"/>')

        pts = " ".join(f"{f(cx(x))},{f(cy(y))}" for x, y in verts)
        if drawn:
            body.append(f'<polygon points="{pts}" fill="{C_GPU}" opacity="0.22" '
                        f'stroke="{C_GPU}" stroke-width="1.8"/>')
        else:
            body.append(f'<polygon points="{pts}" fill="none" stroke="{C_BAD}" '
                        f'stroke-width="1.4" stroke-dasharray="4 4"/>')

        # The order, drawn as 1-2-3 so the winding is countable.
        for n, (x, y) in enumerate(verts):
            body.append(f'<circle cx="{f(cx(x))}" cy="{f(cy(y))}" r="9" '
                        f'fill="{C_GPU if drawn else C_BAD}" opacity="0.9"/>')
            body.append(f'<text x="{f(cx(x))}" y="{f(cy(y) + 4)}" class="xs t-inv" '
                        f'text-anchor="middle">{n + 1}</text>')

        body.append(f'<text x="{f(X0 + S / 2)}" y="{f(Y0 - 12)}" class="xs" '
                    f'text-anchor="middle">{label}</text>')
        body.append(f'<text x="{f(X0 + S / 2)}" y="{f(Y0 + S + 22)}" class="xs mono'
                    f'{"" if drawn else " t-hi"}" text-anchor="middle">'
                    f'{covered:,} px</text>')

    body.append(f'<text x="24" y="{H - 34}" class="xs">'
                f'The middle one is not flipped, or dark, or behind anything. It is '
                f'<tspan class="t-hi">not drawn at all</tspan> &#8212; and the third panel proves '
                f'the geometry never changed,</text>')
    body.append(f'<text x="24" y="{H - 16}" class="xs">'
                f'because turning culling off brings back exactly the same '
                f'{CULL_CW_NONE:,} pixels as the first panel.</text>')

    return svg(W, H, "Winding, and what culling does about it",
               f"Three panels. The first shows the vertices in counter-clockwise order, filled, "
               f"covering {CULL_CCW:,} pixels. The second shows the same three points with the "
               f"first two swapped, so the order is clockwise: it is drawn only as a dashed "
               f"outline because the measurement is {CULL_CW} pixels — the triangle is culled "
               f"entirely. The third shows the clockwise order again with culling disabled, "
               f"covering {CULL_CW_NONE:,} pixels, exactly the first panel's count, which proves "
               f"the geometry was never the problem.",
               "\n".join(body), "l44f4")


# =============================================================================
# Figure 5 — the edge, pixel by pixel
# =============================================================================
# ONE CLAIM: our rasterizer and the hardware agree about every interior pixel and
# differ only along one edge, one pixel at a time — a fill rule, not a bug.
def fig5():
    with open("scratch/l44_edge.txt") as fh:
        lines = [ln.rstrip("\n") for ln in fh if not ln.startswith("#")]

    rows = len(lines)
    cols = max(len(ln) for ln in lines)

    CELL = 13.0
    X0, Y0 = 60.0, 92.0
    W = int(X0 + cols * CELL + 250)
    H = int(Y0 + rows * CELL + 60)

    body = []
    body.append('<text x="24" y="26" class="sm">'
                'the boundary, one square per pixel &#8212; drawn from the actual comparison</text>')
    body.append('<text x="24" y="46" class="xs muted">'
                'a 34&#215;20 patch at the triangle&#8217;s apex, where the two rasterizers '
                'first disagree</text>')

    for r, line in enumerate(lines):
        for c, ch in enumerate(line):
            x = X0 + c * CELL
            y = Y0 + r * CELL
            if ch == "#":
                fill, op = C_BOTH, 0.85
            elif ch == "c":
                fill, op = C_CPU, 1.0
            elif ch == "g":
                fill, op = C_GPU, 1.0
            else:
                continue
            body.append(f'<rect x="{f(x)}" y="{f(y)}" width="{f(CELL - 1)}" '
                        f'height="{f(CELL - 1)}" fill="{fill}" opacity="{op}"/>')

    # A faint grid so single pixels are countable.
    for c in range(cols + 1):
        body.append(f'<line x1="{f(X0 + c * CELL - 0.5)}" y1="{f(Y0)}" '
                    f'x2="{f(X0 + c * CELL - 0.5)}" y2="{f(Y0 + rows * CELL)}" '
                    f'class="grid" stroke-width="0.5"/>')
    for r in range(rows + 1):
        body.append(f'<line x1="{f(X0)}" y1="{f(Y0 + r * CELL - 0.5)}" '
                    f'x2="{f(X0 + cols * CELL)}" y2="{f(Y0 + r * CELL - 0.5)}" '
                    f'class="grid" stroke-width="0.5"/>')

    lx = X0 + cols * CELL + 24
    legend = [
        (C_BOTH, 0.85, "both agree", f"{BOTH:,} px"),
        (C_CPU, 1.0, "ours only", f"{CPU_ONLY} px"),
        (C_GPU, 1.0, "the GPU only", f"{GPU_ONLY} px"),
    ]
    for i, (colour, op, label, count) in enumerate(legend):
        y = Y0 + 16 + i * 30
        body.append(f'<rect x="{f(lx)}" y="{f(y - 10)}" width="12" height="12" '
                    f'fill="{colour}" opacity="{op}"/>')
        body.append(f'<text x="{f(lx + 18)}" y="{f(y)}" class="xs">{label}</text>')
        body.append(f'<text x="{f(lx + 18)}" y="{f(y + 13)}" class="xs mono muted">{count}</text>')

    y = Y0 + 130
    body.append(f'<text x="{f(lx)}" y="{f(y)}" class="xs">'
                f'disagreements in</text>')
    body.append(f'<text x="{f(lx)}" y="{f(y + 15)}" class="xs">'
                f'the interior:</text>')
    body.append(f'<text x="{f(lx)}" y="{f(y + 34)}" class="xs mono t-hi">'
                f'{INTERIOR_DISAGREEMENTS}</text>')

    return svg(W, H, "Where our rasterizer and the hardware disagree",
               f"A magnified patch of the triangle's apex, one square per pixel, taken from the "
               f"actual pixel-by-pixel comparison. Pale blue squares are pixels both rasterizers "
               f"covered. Green squares, appearing one at a time along the right-hand edge, are "
               f"pixels our software rasterizer covered and the hardware did not — {CPU_ONLY} of "
               f"them across the whole triangle. There are no squares of the third colour at all: "
               f"the hardware covered nothing we missed. And the count of disagreements strictly "
               f"inside the triangle is {INTERIOR_DISAGREEMENTS}, so the two agree completely "
               f"about the interior and differ only in where they consider a boundary pixel to "
               f"belong.",
               "\n".join(body), "l44f5")


# =============================================================================
# Figure 6 — the frame
# =============================================================================
# ONE CLAIM: one frame, three recorded operations, and every pixel of the result
# is accounted for by exactly one of them.
def fig6():
    W, H = 720, 330
    body = []

    body.append('<text x="24" y="26" class="sm">'
                'one frame of <tspan class="mono">engine --gpu</tspan>, and where every pixel '
                'came from</text>')

    steps = [
        ("render pass 1", "CLEAR the swapchain", "22,364 px survive", "#7a7a72"),
        ("blit", "the software framebuffer, on top", "22,364 px survive", C_CPU),
        ("render pass 2", "LOAD, then draw the triangle", "20,808 px", C_GPU),
    ]

    BW, BH = 196.0, 74.0
    for i, (name, what, result, colour) in enumerate(steps):
        x = 30.0 + i * (BW + 30.0)
        y = 76.0
        body.append(f'<rect x="{f(x)}" y="{f(y)}" width="{f(BW)}" height="{f(BH)}" rx="5" '
                    f'fill="{colour}" opacity="0.18" stroke="{colour}" stroke-width="1.6"/>')
        body.append(f'<text x="{f(x + BW / 2)}" y="{f(y + 22)}" class="xs mono" '
                    f'text-anchor="middle">{name}</text>')
        body.append(f'<text x="{f(x + BW / 2)}" y="{f(y + 40)}" class="xs" '
                    f'text-anchor="middle">{what}</text>')
        body.append(f'<text x="{f(x + BW / 2)}" y="{f(y + 60)}" class="xs mono muted" '
                    f'text-anchor="middle">{result}</text>')
        if i + 1 < len(steps):
            body.append(f'<line x1="{f(x + BW + 6)}" y1="{f(y + BH / 2)}" '
                        f'x2="{f(x + BW + 24)}" y2="{f(y + BH / 2)}" class="ink-soft" '
                        f'stroke-width="1.6" marker-end="url(#e-s)"/>')

    # The sum.
    total = sum(v for _n, v in COMPOSE)
    y = 200.0
    X0, XW = 30.0, 660.0
    x = X0
    for (label, value), colour in zip(COMPOSE, ("#7a7a72", C_CPU, C_GPU)):
        w = XW * value / total
        body.append(f'<rect x="{f(x)}" y="{f(y)}" width="{f(w)}" height="34" '
                    f'fill="{colour}" opacity="0.85"/>')
        body.append(f'<text x="{f(x + w / 2)}" y="{f(y + 22)}" class="xs t-inv" '
                    f'text-anchor="middle">{value:,}</text>')
        body.append(f'<text x="{f(x + w / 2)}" y="{f(y + 50)}" class="xs" '
                    f'text-anchor="middle">{label}</text>')
        x += w

    body.append(f'<text x="{f(X0)}" y="{f(y + 82)}" class="xs">'
                f'{total:,} pixels, which is 256&#178; exactly &#8212; '
                f'<tspan class="t-hi">every pixel accounted for by exactly one step</tspan>. '
                f'Checked offscreen,</text>')
    body.append(f'<text x="{f(X0)}" y="{f(y + 100)}" class="xs">'
                f'because a window cannot be asked what it contains and a screenshot proves less '
                f'than a count.</text>')

    return svg(W, H, "One frame, three operations, every pixel accounted for",
               f"Three boxes left to right: a render pass that clears, a blit that puts the "
               f"software rasterizer's picture on top, and a second render pass that loads what "
               f"is there and draws the triangle over it. Below, a stacked bar dividing the "
               f"finished image into {COMPOSE[0][1]:,} cleared pixels, {COMPOSE[1][1]:,} pixels "
               f"of the software picture, and {COMPOSE[2][1]:,} pixels of triangle — "
               f"{total:,} in total, which is 256 squared exactly.",
               "\n".join(body), "l44f6")


def main():
    print("Lesson 4.4 figures:")
    write("l44_fig1.svg", fig1())
    write("l44_fig2.svg", fig2())
    write("l44_fig3.svg", fig3())
    write("l44_fig4.svg", fig4())
    write("l44_fig5.svg", fig5())
    write("l44_fig6.svg", fig6())


if __name__ == "__main__":
    main()
