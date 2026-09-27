#!/usr/bin/env python3
"""Compute every SVG figure for Lesson 4.8 from real data.

Figures 4 and 6 are drawn from scratch/l48_*.ppm, which verify_48 renders
offscreen and downloads. The raster helpers are Lesson 4.5's, imported rather
than copied.

Every number in every caption comes from scratch/verify_48.log.

Writes scratch/l48_fig{1..6}.svg.
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_45 import (svg, write, f, panel, hexrgb, read_ppm, box_sample, rle_rects,
                     C_TINT, C_POS, C_NRM, C_UV, C_INST, C_BAD, C_HUB)


def panel_bg(path, crop, cell, x, y, px, palette, levels, bg, tol=6,
             bg_class="fill-soft"):
    """`panel`, with the scene's background knocked out first.

    `quantise` calls a cell background only when every channel is under 14, and
    this lesson's scenes clear to (18, 20, 28) — three codes over the line. Left
    alone, every background cell is snapped to the nearest TINT instead, and the
    result is a panel striped with faint horizontal bands where adjacent rows
    round differently. Zeroing the background before the run-length encoder sees
    it costs one comparison per cell and removes several thousand rectangles.
    """
    w, h, data = read_ppm(path)
    gw, gh, grid = box_sample(data, w, crop, cell)
    for row in grid:
        for i, (r, g, b) in enumerate(row):
            if (abs(r - bg[0]) <= tol and abs(g - bg[1]) <= tol
                    and abs(b - bg[2]) <= tol):
                row[i] = (0, 0, 0)
    return gw, gh, rle_rects(grid, gw, gh, x, y, px, palette, levels, bg_class)


SKY = (18, 20, 28)

# ---- Measured inputs, from scratch/verify_48.log ----------------------------

# §D, the solids scene at 320x180
SOLIDS = dict(both=2729, cpu_only=42, gpu_only=60,
              hist=[("exact", 2374, 86.99), ("1 code", 215, 7.88),
                    ("2 codes", 64, 2.35), ("5-16", 9, 0.33), ("&gt;16", 67, 2.46)],
              mean=2.1796, worst=134)

# §D, the textured floor, and its untextured control
FLOOR = dict(both=39202, gpu_only=235, mean=6.1370, worst=54, exact_pct=66.76,
             gt16_pct=14.38)
FLOOR_PLAIN = dict(both=39202, mean=1.0, worst=1)

# The sRGB encode sweep: (linear, exact*255, ours, hardware)
ENCODE = [
    (0.0000000, 0.000, 0, 0),
    (0.0010000, 3.295, 3, 3),
    (0.0031308, 10.315, 10, 10),
    (0.0044000, 14.023, 14, 14),
    (0.0045148, 14.325, 14, 14),
    (0.0045186, 14.335, 14, 15),
    (0.0045800, 14.495, 14, 15),
    (0.0046000, 14.547, 15, 15),
    (0.0047000, 14.804, 15, 15),
    (0.0050000, 15.557, 16, 15),
    (0.0060000, 17.892, 18, 18),
    (0.0100000, 25.462, 25, 25),
    (0.0451480, 59.974, 60, 60),
    (0.2000000, 123.555, 124, 124),
    (0.5000000, 187.516, 188, 188),
    (1.0000000, 255.000, 255, 255),
]

# §B, the normal counts
NORMALS = [("cube", 8, 36, 8), ("quad", 4, 6, 4), ("icosahedron", 12, 60, 12)]

# §E
DRAWS = 3
PIPELINE_BINDS_UNSORTED = 3
PIPELINE_BINDS_SORTED = 2
UNIFORM_PER_FRAME = 128
UNIFORM_PER_DRAW = 144
UNIFORM_TOTAL = 560

# The [J] experiment
NAIVE_BOXES_PX = 0
NAIVE_SQUASHED_PX = 2160
NAIVE_SQUASHED_MAX = 143

# §F
SHADE_WORST = 1.192e-7


# =============================================================================
# Figure 1 — the two pipelines, stage for stage
# =============================================================================
def fig1():
    W, H = 720, 532
    body = []

    body.append('<text x="24" y="22" class="sm">'
                'Module 3&#8217;s pipeline and Module 4&#8217;s, stage for stage &#8212; '
                '<tspan class="t-hi">and what each move actually cost</tspan></text>')

    ROWS = [
        # (stage, cpu side, gpu side, verdict key)
        ("model &#8594; world", "parent_from_local(t)", "mul(world_from_model, p)", "same"),
        ("the normal matrix", "normal_matrix(M)", "c0*n.x + c1*n.y + c2*n.z", "same"),
        ("world &#8594; view", "view_from_world * p", "folded into", "fold"),
        ("view &#8594; clip", "proj * point(v)", "clip_from_world", "fold"),
        ("the divide", "xyz(clip) / clip.w", "fixed function", "hw"),
        ("near clipping", "clip_polygon_near()", "fixed function", "hw"),
        ("the viewport", "viewport::to_screen", "SDL_SetGPUViewport", "hw"),
        ("back-face culling", "is_front_facing()", "a PIPELINE object", "gone"),
        ("coverage + interpolation", "fill_triangle()", "fixed function", "hw"),
        ("perspective correction", "interpolate a/w, 1/w", "fixed function", "hw"),
        ("the depth test", "depth_buffer", "an ATTACHMENT", "hw"),
        ("the texture sample", "texture::sample()", "albedo.Sample()", "hw"),
        ("the shading equation", "engine::shade()", "scene.frag.hlsl", "port"),
        ("the material", "on a raster_triangle", "a per-DRAW push", "gone"),
        ("the sRGB encode", "to_encoded()", "the _SRGB target", "hw"),
    ]

    # (label, swatch colour, TEXT CLASS). The class, not a fill attribute:
    # `figure.dia svg text { fill: … }` in course.css is CSS and CSS always beats
    # an SVG presentation attribute, so an inline fill on a <text> is silently
    # ignored. Only four text colours exist, so the five-way distinction is
    # carried by the SWATCH — which is a <rect>, and unaffected by that rule.
    VERDICT = {
        "same": ("the same function", C_NRM, "t-ok"),
        "fold": ("folded, on the CPU", C_HUB, "muted"),
        "hw":   ("now the hardware&#8217;s", C_POS, "muted"),
        "port": ("translated to HLSL", C_UV, "t-hi"),
        "gone": ("could not survive", C_BAD, "t-bad"),
    }

    Y0 = 60
    RH = 25
    body.append(f'<text x="30" y="{Y0-10}" class="xs muted">stage</text>')
    body.append(f'<text x="222" y="{Y0-10}" class="xs muted">Modules 2&#8211;3, on the CPU</text>')
    body.append(f'<text x="424" y="{Y0-10}" class="xs muted">Module 4, on the GPU</text>')
    body.append(f'<text x="600" y="{Y0-10}" class="xs muted">what moved</text>')

    for i, (stage, cpu, gpu, key) in enumerate(ROWS):
        y = Y0 + i * RH
        label, colour, cls = VERDICT[key]
        if i % 2 == 0:
            body.append(f'<rect x="24" y="{y}" width="{W-48}" height="{RH}" '
                        'class="fill-soft" stroke="none"/>')
        body.append(f'<text x="30" y="{y+17}" class="xs">{stage}</text>')
        body.append(f'<text x="222" y="{y+17}" class="xs mono muted">{cpu}</text>')
        body.append(f'<text x="424" y="{y+17}" class="xs mono muted">{gpu}</text>')
        body.append(f'<rect x="592" y="{y+5}" width="8" height="{RH-10}" '
                    f'fill="{colour}"/>')
        body.append(f'<text x="606" y="{y+17}" class="xs {cls}">{label}</text>')

    yend = Y0 + len(ROWS) * RH
    body.append(f'<text x="24" y="{yend+26}" class="xs">'
                'Fifteen stages. <tspan class="t-ok">Two are literally the same C++ function, '
                'called from both sides</tspan>; eight became the hardware&#8217;s job;</text>')
    body.append(f'<text x="24" y="{yend+44}" class="xs">'
                'one was translated line for line; two were folded into a single matrix '
                '&#8212; and <tspan class="t-bad">two could not cross at all</tspan>.</text>')
    body.append(f'<text x="24" y="{yend+68}" class="xs muted">'
                'The last two are the lesson. A cull mode and a material are not values a '
                'triangle can carry on a GPU,</text>')
    body.append(f'<text x="24" y="{yend+84}" class="xs muted">'
                'and that is what turns one loop into N draw calls.</text>')

    return svg(W, H, "The port, stage by stage",
               "A fifteen-row table comparing each stage of the software rasterizer with its "
               "GPU counterpart. Two stages call the same C++ function from both sides; eight "
               "became fixed-function hardware; the shading equation was translated to HLSL; "
               "the view and projection matrices were folded into one; and back-face culling "
               "and the material could not cross at all, because both are pipeline state.",
               "\n".join("  " + b for b in body), "f48a")


# =============================================================================
# Figure 2 — three rates of change
# =============================================================================
def fig2():
    W, H = 720, 402
    body = []

    body.append('<text x="24" y="22" class="sm">'
                'four rates of change, and where each one physically lives</text>')

    # (name, bar colour, TEXT CLASS, …) — see fig1 on why the class is needed.
    LANES = [
        ("per FRAME", C_HUB, "muted", "camera_uniforms (64 B)\nscene_light_uniforms (64 B)",
         "SDL_PushGPUVertexUniformData(cb, 0, ...)",
         "pushed once, before the pass; every draw after it sees them",
         "128 B / frame"),
        ("per DRAW", C_UV, "t-hi", "object_uniforms (112 B)\nmaterial_uniforms (32 B)",
         "SDL_PushGPU*UniformData(cb, 1, ...)",
         "pushed between draws &#8212; the rate Lesson 4.8 adds",
         f"{UNIFORM_PER_DRAW} B &#215; {DRAWS} draws"),
        ("per INSTANCE", C_INST, "muted", "gpu_instance (28 B)",
         "a vertex buffer at slot 1, INSTANCE rate",
         "unused by a scene of distinct meshes; back for foliage and crates",
         "0 B here"),
        ("per VERTEX", C_POS, "muted", "gpu_vertex_pnu (32 B)",
         "a vertex buffer at slot 0, VERTEX rate",
         "uploaded ONCE at import and never touched again",
         "0 B / frame"),
    ]

    Y0 = 46
    LH = 78
    for i, (name, colour, cls, blocks, how, why, cost) in enumerate(LANES):
        y = Y0 + i * LH
        body.append(f'<rect x="24" y="{y}" width="{W-48}" height="{LH-10}" '
                    'class="fill-soft ink" stroke-width="0.75"/>')
        body.append(f'<rect x="24" y="{y}" width="6" height="{LH-10}" fill="{colour}"/>')
        body.append(f'<text x="42" y="{y+20}" class="sm mono {cls}">{name}</text>')
        for j, ln in enumerate(blocks.split("\n")):
            body.append(f'<text x="42" y="{y+38+j*14}" class="xs mono muted">{ln}</text>')
        body.append(f'<text x="300" y="{y+20}" class="xs mono">{how}</text>')
        body.append(f'<text x="300" y="{y+38}" class="xs muted">{why}</text>')
        body.append(f'<text x="{W-40}" y="{y+20}" class="xs mono {cls}" '
                    f'text-anchor="end">{cost}</text>')

    y = Y0 + len(LANES) * LH
    body.append(f'<text x="24" y="{y+8}" class="xs">'
                f'This frame moved <tspan class="t-hi">{UNIFORM_TOTAL} bytes</tspan> of uniform '
                'data for a scene of 44 triangles &#8212; and would move the same 560 bytes if '
                'each object had a million.</text>')
    body.append(f'<text x="24" y="{y+26}" class="xs muted">'
                'That is the whole argument for grouping data by how often it changes rather '
                'than by what it is about.</text>')

    return svg(W, H, "Four rates of change",
               "Per-frame data (the camera and the lamp, 128 bytes) is pushed once before the "
               "render pass. Per-draw data (this object's matrices and material, 144 bytes) is "
               "pushed between draws — the rate this lesson adds. Per-instance data is unused by "
               "a scene of distinct meshes. Per-vertex data was uploaded once at import. The "
               "whole frame moves 560 bytes of uniform traffic.",
               "\n".join("  " + b for b in body), "f48b")


# =============================================================================
# Figure 3 — the normals a vertex shader cannot invent
# =============================================================================
def fig3():
    W, H = 720, 356
    body = []

    body.append('<text x="24" y="22" class="sm">'
                'a corner where three faces meet &#8212; '
                '<tspan class="t-hi">one position, three normals</tspan></text>')

    # An exploded corner: three quads meeting at a point, each with its own normal.
    CX, CY = 168.0, 168.0
    S = 62.0
    # Three rhombi around the corner, in an isometric-ish projection.
    # These three faces ARE the model's axes, so they take the course's axis
    # colours (conventions §8: x/y/z = red/green/blue) rather than arbitrary
    # ones — and the labels take .lbl-x/.lbl-y/.lbl-z, which is the only way to
    # colour SVG text here (see fig1).
    AX_X, AX_Y, AX_Z = "#d1344a", "#2f9e4f", "#2b6fd4"
    faces = [
        # (points, shape colour, normal arrow dx dy, label, label class)
        ([(0, 0), (-S, -S * 0.5), (-S, -S * 0.5 - S * 0.72), (0, -S * 0.72)],
         AX_X, (-0.86, -0.5), "&#8722;x", "lbl-x"),
        ([(0, 0), (S, -S * 0.5), (S, -S * 0.5 - S * 0.72), (0, -S * 0.72)],
         AX_Z, (0.86, -0.5), "+z", "lbl-z"),
        ([(0, 0), (-S, -S * 0.5), (0, -S), (S, -S * 0.5)],
         AX_Y, (0, -1), "+y", "lbl-y"),
    ]
    for pts, colour, (nx, ny), lab, lab_cls in faces:
        d = " ".join(f"{f(CX+px)},{f(CY+py)}" for px, py in pts)
        body.append(f'<polygon points="{d}" fill="{colour}" fill-opacity="0.30" '
                    f'stroke="{colour}" stroke-width="1"/>')
        # the face normal, from the centroid
        cx = CX + sum(p[0] for p in pts) / len(pts)
        cy = CY + sum(p[1] for p in pts) / len(pts)
        body.append(f'<line x1="{f(cx)}" y1="{f(cy)}" x2="{f(cx+nx*34)}" y2="{f(cy+ny*34)}" '
                    f'stroke="{colour}" stroke-width="1.6" marker-end="url(#e-i)"/>')
        body.append(f'<text x="{f(cx+nx*44)}" y="{f(cy+ny*44+4)}" class="xs mono {lab_cls}" '
                    f'text-anchor="middle">{lab}</text>')

    body.append(f'<circle cx="{f(CX)}" cy="{f(CY)}" r="4" class="hi-fill"/>')
    body.append(f'<text x="{f(CX)}" y="{f(CY+22)}" class="xs" text-anchor="middle">'
                'one position</text>')
    body.append(f'<text x="30" y="{H-104}" class="xs muted">'
                'A vertex carries exactly one normal.</text>')
    body.append(f'<text x="30" y="{H-88}" class="xs muted">'
                'Three faces meeting here want three, so</text>')
    body.append(f'<text x="30" y="{H-72}" class="xs muted">'
                'FLAT has to write the position three times</text>')
    body.append(f'<text x="30" y="{H-56}" class="xs muted">'
                '&#8212; and then the sharing is gone.</text>')

    # The counts, as a small table.
    X0 = 376
    body.append(f'<text x="{X0}" y="52" class="xs muted">mesh</text>')
    body.append(f'<text x="{X0+150}" y="52" class="xs muted" text-anchor="end">positions</text>')
    body.append(f'<text x="{X0+230}" y="52" class="xs muted" text-anchor="end">flat</text>')
    body.append(f'<text x="{X0+310}" y="52" class="xs muted" text-anchor="end">smooth</text>')
    for i, (name, pos, flat, smooth) in enumerate(NORMALS):
        y = 74 + i * 26
        body.append(f'<text x="{X0}" y="{y}" class="xs mono">{name}</text>')
        body.append(f'<text x="{X0+150}" y="{y}" class="xs mono muted" text-anchor="end">'
                    f'{pos}</text>')
        body.append(f'<text x="{X0+230}" y="{y}" class="xs mono t-bad" '
                    'text-anchor="end">' + str(flat) + '</text>')
        body.append(f'<text x="{X0+310}" y="{y}" class="xs mono t-ok" '
                    'text-anchor="end">' + str(smooth) + '</text>')

    body.append(f'<line x1="{X0}" y1="158" x2="{X0+310}" y2="158" class="grid" '
                'stroke-width="0.75"/>')
    body.append(f'<text x="{X0}" y="180" class="xs">'
                'None of the three carries an authored normal.</text>')
    body.append(f'<text x="{X0}" y="200" class="xs muted">'
                'The software rasterizer computed a face normal</text>')
    body.append(f'<text x="{X0}" y="216" class="xs muted">'
                'inside its per-triangle loop, from the three corners.</text>')
    body.append(f'<text x="{X0}" y="240" class="xs t-bad">'
                'A vertex shader sees ONE vertex.</text>')
    body.append(f'<text x="{X0}" y="260" class="xs muted">'
                'So the fallback stops being the renderer&#8217;s job and</text>')
    body.append(f'<text x="{X0}" y="276" class="xs muted">'
                'becomes the importer&#8217;s &#8212; which is where every real</text>')
    body.append(f'<text x="{X0}" y="292" class="xs muted">'
                'engine already puts it.</text>')
    body.append(f'<text x="{X0}" y="318" class="xs">'
                'And a runtime toggle became a <tspan class="t-hi">build-time</tspan></text>')
    body.append(f'<text x="{X0}" y="334" class="xs">'
                'decision: flat and smooth no longer share a buffer.</text>')

    return svg(W, H, "Why the geometry gained normals",
               "At a cube's corner three faces meet at one position, and each wants a different "
               "normal. A vertex holds one, so flat shading must write the position three times: "
               "a cube's 8 positions become 36 vertices, an icosahedron's 12 become 60. None of "
               "the built-in meshes carries an authored normal, and a vertex shader — which sees "
               "one vertex — cannot compute a face normal the way the software rasterizer's "
               "per-triangle loop did.",
               "\n".join("  " + b for b in body), "f48c")


# =============================================================================
# Figure 4 — the same scene, both rasterizers
# =============================================================================
def fig4():
    W, H = 720, 396
    body = []

    body.append('<text x="24" y="22" class="sm">'
                'the same scene, the same geometry, the same light &#8212; '
                '<tspan class="t-hi">two rasterizers</tspan></text>')

    CROP = (60, 20, 260, 160)     # 200 x 140
    CELL = 2
    PX = 2.2

    pal = [hexrgb(c) for c in ("#e0a83c", "#3cb8a8", "#a070d8")] + \
          [(232, 90, 80), (60, 130, 230), (255, 255, 255), (24, 28, 38)]

    panels = [("scratch/l48_solids_cpu.ppm", "software rasterizer", "muted"),
              ("scratch/l48_solids_gpu.ppm", "SDL_GPU", "muted"),
              ("scratch/l48_solids_diff.ppm", "where they differ", "t-hi")]

    for i, (path, label, cls) in enumerate(panels):
        x = 24 + i * 232
        gw, gh, rects = (panel_bg(path, CROP, CELL, x, 40, PX, pal, 5, SKY)
                         if "diff" not in path
                         else panel_bg(path, CROP, CELL, x, 40, PX, pal, 6, (255, 255, 255)))
        body += rects
        body.append(f'<rect x="{x}" y="40" width="{f(gw*PX)}" height="{f(gh*PX)}" class="ink" '
                    'fill="none" stroke-width="0.75"/>')
        body.append(f'<text x="{f(x+gw*PX/2)}" y="{f(40+gh*PX+16)}" class="sm mono {cls}" '
                    f'text-anchor="middle">{label}</text>')

    # The histogram, as a stacked bar.
    Y = 262
    BAR_X, BAR_W = 24, 420
    body.append(f'<text x="{BAR_X}" y="{Y-8}" class="xs muted">'
                f'the {SOLIDS["both"]:,} pixels both renderers drew:</text>')
    cursor = BAR_X
    COLOURS = [C_NRM, C_HUB, C_HUB, C_UV, C_BAD]
    for (name, count, pct), colour in zip(SOLIDS["hist"], COLOURS):
        w = BAR_W * pct / 100.0
        body.append(f'<rect x="{f(cursor)}" y="{Y}" width="{f(w)}" height="22" '
                    f'fill="{colour}" fill-opacity="0.75"/>')
        cursor += w
    body.append(f'<rect x="{BAR_X}" y="{Y}" width="{BAR_W}" height="22" class="ink" '
                'fill="none" stroke-width="0.75"/>')

    body.append(f'<text x="{BAR_X}" y="{Y+38}" class="xs">'
                f'<tspan class="t-ok">{SOLIDS["hist"][0][2]}% identical</tspan>, '
                f'{SOLIDS["hist"][1][2]}% one code, {SOLIDS["hist"][2][2]}% two &#8212; '
                f'and <tspan class="t-bad">{SOLIDS["hist"][4][2]}% badly wrong</tspan>.</text>')
    body.append(f'<text x="{BAR_X}" y="{Y+56}" class="xs muted">'
                'Every one of those 67 pixels is on a silhouette.</text>')
    body.append(f'<text x="{BAR_X}" y="{Y+80}" class="xs">'
                f'Coverage: <tspan class="t-hi">{SOLIDS["cpu_only"]} px only the CPU drew, '
                f'{SOLIDS["gpu_only"]} px only the GPU did</tspan> &#8212; a sliver one pixel '
                'wide around each object.</text>')
    body.append(f'<text x="{BAR_X}" y="{Y+98}" class="xs muted">'
                'The GPU snaps corners to a fixed sub-pixel grid; our fill rounds them to whole '
                'pixels. Same rule, different resolution.</text>')

    body.append(f'<text x="512" y="{Y-8}" class="xs muted">difference image:</text>')
    body.append(f'<rect x="512" y="{Y}" width="12" height="10" fill="{C_BAD}"/>')
    body.append(f'<text x="530" y="{Y+9}" class="xs">hotter = further apart</text>')
    body.append(f'<rect x="512" y="{Y+18}" width="12" height="10" fill="#2878dc"/>')
    body.append(f'<text x="530" y="{Y+27}" class="xs">one of them drew nothing</text>')

    return svg(W, H, "Both rasterizers on the same scene",
               "The software rasterizer, SDL_GPU, and a difference image, from a 320x180 render "
               "of the same scene through the same camera. 87% of the pixels both drew are "
               "byte-identical; 8% differ by one code; 2.5% differ badly, and all of those are "
               "on a silhouette. About a hundred pixels are covered by one renderer and not the "
               "other, forming a one-pixel sliver around each object.",
               "\n".join("  " + b for b in body), "f48d")


# =============================================================================
# Figure 5 — the one-code floor
# =============================================================================
def fig5():
    W, H = 720, 400
    body = []

    body.append('<text x="24" y="22" class="sm">'
                'the last step, measured: linear float &#8594; eight-bit sRGB, '
                '<tspan class="t-hi">two implementations</tspan></text>')

    X0, Y0 = 40, 52
    CW = [116, 96, 70, 84]
    heads = ["linear value", "exact &#215;255", "ours", "hardware"]
    x = X0
    for i, hd in enumerate(heads):
        body.append(f'<text x="{x+CW[i]-8}" y="{Y0}" class="xs muted" '
                    f'text-anchor="end">{hd}</text>')
        x += CW[i]

    RH = 19
    for i, (lin, exact, ours, hw) in enumerate(ENCODE):
        y = Y0 + 16 + i * RH
        disagree = ours != hw
        if disagree:
            body.append(f'<rect x="{X0-6}" y="{y-13}" width="{sum(CW)+12}" height="{RH}" '
                        f'fill="{C_BAD}" fill-opacity="0.16" stroke="none"/>')
        elif i % 2 == 0:
            body.append(f'<rect x="{X0-6}" y="{y-13}" width="{sum(CW)+12}" height="{RH}" '
                        'class="fill-soft" stroke="none"/>')
        x = X0
        vals = [f"{lin:.7f}", f"{exact:.3f}", str(ours), str(hw)]
        for j, v in enumerate(vals):
            cls = "xs mono"
            if disagree and j >= 2:
                cls = "xs mono t-bad"
            elif j < 2:
                cls = "xs mono muted"
            body.append(f'<text x="{x+CW[j]-8}" y="{y}" class="{cls}" '
                        f'text-anchor="end">{v}</text>')
            x += CW[j]

    # The argument, to the right.
    RX = 424
    body.append(f'<text x="{RX}" y="{Y0+16}" class="xs">'
                'Everything above linear 0.006 agrees</text>')
    body.append(f'<text x="{RX}" y="{Y0+32}" class="xs">'
                '<tspan class="t-ok">exactly</tspan>. Every disagreement is in</text>')
    body.append(f'<text x="{RX}" y="{Y0+48}" class="xs">the <tspan class="t-hi">darks</tspan>.</text>')
    body.append(f'<text x="{RX}" y="{Y0+76}" class="xs muted">'
                'The sRGB curve&#8217;s slope near zero is</text>')
    body.append(f'<text x="{RX}" y="{Y0+92}" class="xs muted">'
                '12.92, so a linear value crosses a code</text>')
    body.append(f'<text x="{RX}" y="{Y0+108}" class="xs muted">'
                'more than twelve times faster there than</text>')
    body.append(f'<text x="{RX}" y="{Y0+124}" class="xs muted">'
                'it does near white. Both sides approximate</text>')
    body.append(f'<text x="{RX}" y="{Y0+140}" class="xs muted">'
                'the same curve; neither is wrong.</text>')
    body.append(f'<text x="{RX}" y="{Y0+172}" class="xs">'
                'Which explains the untextured floor:</text>')
    body.append(f'<text x="{RX}" y="{Y0+192}" class="xs mono muted">'
                'one flat quad, one colour,</text>')
    body.append(f'<text x="{RX}" y="{Y0+208}" class="xs mono muted">'
                'blue = 0.0045186</text>')
    body.append(f'<text x="{RX}" y="{Y0+232}" class="xs">'
                f'&#8212; and so <tspan class="t-bad">{FLOOR_PLAIN["both"]:,} pixels</tspan>, '
                '100% of</text>')
    body.append(f'<text x="{RX}" y="{Y0+248}" class="xs">'
                'them, differ by exactly one code.</text>')
    body.append(f'<text x="{RX}" y="{Y0+276}" class="xs muted">'
                '&#8220;100% of pixels differ&#8221; turns out to</text>')
    body.append(f'<text x="{RX}" y="{Y0+292}" class="xs muted">'
                'mean nothing at all.</text>')

    body.append(f'<rect x="{RX}" y="{Y0+306}" width="14" height="10" '
                f'fill="{C_BAD}" fill-opacity="0.20"/>')
    body.append(f'<text x="{RX+20}" y="{Y0+315}" class="xs muted">'
                'rows where the two disagree</text>')


    body.append(f'<text x="24" y="{H-14}" class="xs">'
                'So a per-pixel comparison across this boundary has a '
                '<tspan class="t-hi">floor of one code</tspan>, and the floor is worst in the '
                'shadows. Plan your assertions accordingly.</text>')

    return svg(W, H, "Two implementations of the sRGB encode",
               "A sweep of linear values through engine::to_encoded and through the hardware's "
               "_SRGB render target. Everything above 0.006 agrees exactly; three probes in the "
               "darks differ by one code, because the sRGB curve's slope near zero is 12.92 and "
               "both sides are approximations of the same curve. This explains why every pixel "
               "of a flat untextured floor differs by one code in blue.",
               "\n".join("  " + b for b in body), "f48e")


# =============================================================================
# Figure 6 — the normal matrix, and the geometry that hides it
# =============================================================================
def fig6():
    W, H = 720, 348
    body = []

    body.append('<text x="24" y="22" class="sm">'
                'the naive normal matrix &#8212; '
                '<tspan class="t-hi">invisible on a box, unmissable on anything else</tspan>'
                '</text>')

    CROP = (60, 20, 260, 160)
    CELL = 2
    PX = 2.3

    pal = [hexrgb(c) for c in ("#e0a83c", "#3cb8a8", "#a070d8")] + \
          [(255, 250, 235), (24, 28, 38)]

    for i, (path, label, cls) in enumerate([
            ("scratch/l48_squashed_good.ppm", "inverse transpose (correct)", "t-ok"),
            ("scratch/l48_squashed_naive.ppm", "the model matrix (naive)", "t-bad")]):
        x = 30 + i * 348
        gw, gh, rects = panel_bg(path, CROP, CELL, x, 40, PX, pal, 7, SKY)
        body += rects
        body.append(f'<rect x="{x}" y="40" width="{f(gw*PX)}" height="{f(gh*PX)}" class="ink" '
                    'fill="none" stroke-width="0.75"/>')
        body.append(f'<text x="{f(x+gw*PX/2)}" y="{f(40+gh*PX+16)}" class="sm mono {cls}" '
                    f'text-anchor="middle">{label}</text>')

    y = 262
    body.append(f'<text x="24" y="{y}" class="xs">'
                'The two boxes in this scene are non-uniformly scaled and '
                f'<tspan class="t-hi">{NAIVE_BOXES_PX} pixels of them change</tspan>. '
                'A box&#8217;s model-space normals ARE its own axes, and a diagonal</text>')
    body.append(f'<text x="24" y="{y+16}" class="xs">'
                'scale sends an axis to a multiple of itself &#8212; so the two matrices differ '
                'only in the normal&#8217;s <tspan class="t-hi">length</tspan>, which the '
                'fragment&#8217;s <tspan class="mono">normalize</tspan> then throws away.</text>')
    body.append(f'<text x="24" y="{y+38}" class="xs">'
                'Squash the icosahedron instead, whose twenty face normals are not axes, and '
                f'<tspan class="t-bad">{NAIVE_SQUASHED_PX:,} pixels change by up to '
                f'{NAIVE_SQUASHED_MAX} codes</tspan>.</text>')
    body.append(f'<text x="24" y="{y+60}" class="xs muted">'
                'Lesson 3.6 said two thirds of a typical scene looks perfect with this bug. '
                'The sharper statement is that on boxes it is</text>')
    body.append(f'<text x="24" y="{y+76}" class="xs muted">'
                'invisible BY CONSTRUCTION &#8212; so a test scene made of crates proves '
                'nothing. Choose test geometry that can fail.</text>')

    return svg(W, H, "The normal matrix, and the geometry that hides it",
               "The same scene rendered with the inverse transpose and with the model matrix "
               "used as a normal matrix. On the two non-uniformly scaled boxes, zero pixels "
               "change: a box's normals are its own axes, and a diagonal scale only changes "
               "their length, which normalize undoes. On a squashed icosahedron, 2,160 pixels "
               "change by up to 143 codes.",
               "\n".join("  " + b for b in body), "f48f")


if __name__ == "__main__":
    write("l48_fig1.svg", fig1())
    write("l48_fig2.svg", fig2())
    write("l48_fig3.svg", fig3())
    write("l48_fig4.svg", fig4())
    write("l48_fig5.svg", fig5())
    write("l48_fig6.svg", fig6())
