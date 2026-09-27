#!/usr/bin/env python3
"""Compute every SVG figure for Lesson 4.5 from real data.

Figures 3 and 6 are drawn from `scratch/l45_*.ppm`, which `verify_45` renders
offscreen and downloads: actual pixels from actual draws, quantised and
run-length encoded so that a picture of 147,456 pixels costs a few hundred
rectangles instead of a few hundred thousand.

Every number in every caption comes from `scratch/verify_45.log`.

Writes scratch/l45_fig{1..6}.svg.
"""

# ---- Measured inputs, from scratch/verify_45.log ----------------------------

TORUS_VERTS = 1225          # unified vertices after the loader's split
TORUS_TRIS = 2304
TORUS_INDICES = 6912
OBJ_POSITIONS = 1152        # `v` lines in assets/torus.obj
OBJ_UVS = 1225              # `vt` lines — and note that this is the unified count

IDX_VBYTES = 39200
IDX_IBYTES = 13824
IDX_TOTAL = 53024
EXP_VBYTES = 221184
EXP_TOTAL = 221184
BYTE_RATIO = 4.17

INVOKE_BEST = 1225
INVOKE_WORST = 6912
INVOKE_EXPANDED = 6912
INVOKE_GAIN = 5.64

LINES_INTER_PER_VERTEX = 1.00
LINES_INTER_WORST = 1
LINES_SEP_WORST = 5
LINES_POS_INTER = 613
LINES_POS_SEP = 230
LINES_POS_GAIN = 2.7

COVER_RIGHT = 3696
COVER_SHORT = 5076
COVER_LONG = 5027

INSTANCE_BYTES = 28
INSTANCES = 7
PER_FRAME_BYTES = 196
PER_FRAME_PERCENT = 0.370

VTX_F4 = 28
VTX_U4 = 16
TRI_DIFFERING = 22494
TRI_COVERED = 47124
TRI_MAX_DELTA = 1

# What each of the six broken layouts produced: (what, our checker, SDL)
LAYOUT_TRIALS = [
    ("correct",                                 True,  True),
    ("pitch four bytes SHORT",                  False, True),
    ("pitch four bytes LONG",                   True,  True),
    ("position/normal offsets SWAPPED",         True,  True),
    ("the uv attribute MISSING",                False, False),
    ("an attribute the shader never declares",  False, True),
]

C_POS = "#5082e6"     # position — blue
C_NRM = "#5ac878"     # normal   — green
C_UV = "#f0961e"      # uv       — amber
C_INST = "#a97ae0"    # per-instance
C_BAD = "#eb786e"
C_HUB = "#5acdd7"
C_TINT = ["#ecd6a8", "#e87666", "#ecb056", "#9ad07a", "#60c6be", "#709ce8", "#ba86e2"]


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
    print(f"  wrote scratch/{name}  ({len(text):,} bytes)")


# =============================================================================
# Turning a real render into a small number of rectangles
# =============================================================================

def read_ppm(path):
    with open(path, "rb") as fh:
        assert fh.readline().strip() == b"P6"
        w, h = map(int, fh.readline().split())
        assert fh.readline().strip() == b"255"
        return w, h, fh.read()


def box_sample(data, w, crop, cell):
    """Average `cell`x`cell` blocks of the crop into a small grid of RGB triples."""
    x0, y0, x1, y1 = crop
    gw = (x1 - x0) // cell
    gh = (y1 - y0) // cell
    out = []
    for gy in range(gh):
        row = []
        for gx in range(gw):
            r = g = b = 0
            for dy in range(cell):
                base = ((y0 + gy * cell + dy) * w + (x0 + gx * cell)) * 3
                for dx in range(cell):
                    i = base + dx * 3
                    r += data[i]
                    g += data[i + 1]
                    b += data[i + 2]
            n = cell * cell
            row.append((r // n, g // n, b // n))
        out.append(row)
    return gw, gh, out


def quantise(rgb, palette, levels=4):
    """Snap a sampled cell to one of `len(palette) * levels` fixed colours.

    Two stages, and the order matters. First the HUE is matched against the known
    tints, because the tints are what identify an instance. Then the BRIGHTNESS is
    quantised into `levels` steps of that tint.

    Quantising each channel independently — the obvious thing — produces hundreds
    of near-identical colours across a smoothly shaded surface, and run-length
    encoding then encodes nothing. Snapping to a fixed palette is what turns
    147,456 pixels into a few hundred rectangles.
    """
    r, g, b = rgb
    if r < 14 and g < 14 and b < 14:
        return None                     # background

    lum = (r * 299 + g * 587 + b * 114) / 1000.0
    if lum < 1.0:
        return None

    # Nearest tint by normalised chroma, so that shading does not change identity.
    best = None
    best_d = None
    for pr, pg, pb in palette:
        plum = max(1.0, (pr * 299 + pg * 587 + pb * 114) / 1000.0)
        d = ((r - pr * lum / plum) ** 2 + (g - pg * lum / plum) ** 2
             + (b - pb * lum / plum) ** 2)
        if best_d is None or d < best_d:
            best_d = d
            best = (pr, pg, pb)

    plum = max(1.0, (best[0] * 299 + best[1] * 587 + best[2] * 114) / 1000.0)
    step = max(1, int(round(levels * lum / 255.0)))
    step = min(levels, step)
    k = (step / levels) * (255.0 / plum)
    return (min(255, int(best[0] * k)), min(255, int(best[1] * k)),
            min(255, int(best[2] * k)))


def rle_rects(grid, gw, gh, x, y, px, palette, levels=4, bg_class="fill-soft"):
    """Run-length encode a grid of quantised colours into <rect> elements."""
    parts = []
    parts.append(f'<rect x="{x}" y="{y}" width="{gw*px}" height="{gh*px}" '
                 f'class="{bg_class}" stroke="none"/>')
    for gy in range(gh):
        run_start = None
        run_col = None
        for gx in range(gw + 1):
            col = quantise(grid[gy][gx], palette, levels) if gx < gw else "END"
            if col != run_col:
                if run_col is not None and run_start is not None:
                    fill = "#%02x%02x%02x" % run_col
                    parts.append(f'<rect x="{x+run_start*px}" y="{y+gy*px}" '
                                 f'width="{(gx-run_start)*px}" height="{px}" '
                                 f'fill="{fill}"/>')
                run_col = col if col != "END" else None
                run_start = gx
                if col is None:
                    run_col = None
        # a trailing run of background needs no rect: the panel is already filled
    return parts


def hexrgb(h):
    return (int(h[1:3], 16), int(h[3:5], 16), int(h[5:7], 16))


def panel(path, crop, cell, x, y, px, palette, levels=4):
    w, h, data = read_ppm(path)
    gw, gh, grid = box_sample(data, w, crop, cell)
    return gw, gh, rle_rects(grid, gw, gh, x, y, px, palette, levels)


# =============================================================================
# Figure 1 — the memory layout, interleaved against separate
# =============================================================================
# ONE CLAIM: interleaved puts a whole vertex in one cache line; separate puts one
# ATTRIBUTE in one cache line. Which is better depends on what the pass reads,
# and both halves of that are measured.
def fig1():
    W, H = 720, 450
    body = []
    BW = 4.4          # pixels per byte
    X0 = 128.0

    body.append('<text x="24" y="22" class="sm">'
                'one vertex is 32 bytes: FLOAT3 position (12) + FLOAT3 normal (12)'
                ' + FLOAT2 uv (8)</text>')

    def byte_row(y, groups, label, sub):
        """groups = [(nbytes, colour, text)]"""
        out = [f'<text x="24" y="{y+14}" class="sm">{label}</text>',
               f'<text x="24" y="{y+27}" class="xs muted">{sub}</text>']
        x = X0
        for n, col, txt in groups:
            wpx = n * BW
            out.append(f'<rect x="{f(x)}" y="{y}" width="{f(wpx)}" height="22" '
                       f'fill="{col}" fill-opacity="0.75" stroke="var(--dia-bg)" '
                       f'stroke-width="0.75"/>')
            if txt and wpx > 22:
                out.append(f'<text x="{f(x+wpx/2)}" y="{y+15}" class="xs mono t-inv" '
                           f'text-anchor="middle">{txt}</text>')
            x += wpx
        return out, x

    # ---- Interleaved -------------------------------------------------------
    y = 62
    body.append(f'<text x="24" y="{y-24}" class="sm t-hi">'
                'INTERLEAVED &#8212; one vertex is contiguous</text>')
    groups = []
    for i in range(4):
        groups += [(12, C_POS, "pos"), (12, C_NRM, "nrm"), (8, C_UV, "uv")]
    rows, xend = byte_row(y, groups, "vertex buffer", "slot 0, pitch 32")
    body += rows

    # the 64-byte cache line, drawn over it
    for k in range(2):
        x = X0 + k * 64 * BW
        body.append(f'<rect x="{f(x)}" y="{y-5}" width="{f(64*BW)}" height="32" '
                    f'class="hi" fill="none" stroke-dasharray="3 3"/>')
        body.append(f'<text x="{f(x+32*BW)}" y="{y-11}" class="xs t-hi" '
                    f'text-anchor="middle">64-byte cache line &#8212; two whole vertices</text>')
    body.append(f'<text x="{f(X0)}" y="{y+40}" class="xs muted">'
                'byte 0</text>')
    body.append(f'<text x="{f(X0+32*BW)}" y="{y+40}" class="xs muted">32</text>')
    body.append(f'<text x="{f(X0+64*BW)}" y="{y+40}" class="xs muted">64</text>')
    body.append(f'<text x="{f(X0+96*BW)}" y="{y+40}" class="xs muted">96</text>')

    # ---- Separate ----------------------------------------------------------
    y2 = 166
    body.append(f'<text x="24" y="{y2-8}" class="sm t-hi">'
                'SEPARATE — one attribute is contiguous</text>')
    r1, _ = byte_row(y2, [(12, C_POS, "pos")] * 8, "positions", "slot 0, pitch 12")
    r2, _ = byte_row(y2 + 46, [(12, C_NRM, "nrm")] * 8, "normals", "slot 1, pitch 12")
    r3, _ = byte_row(y2 + 92, [(8, C_UV, "uv")] * 12, "uvs", "slot 2, pitch 8")
    body += r1 + r2 + r3
    body.append(f'<text x="{f(X0)}" y="{y2+134}" class="xs muted">'
                'three buffers, three base addresses, three fetches per vertex</text>')

    # ---- The two measurements ---------------------------------------------
    y3 = 322
    body.append(f'<rect x="24" y="{y3}" width="{W-48}" height="106" class="fill-soft ink" '
                'stroke-width="0.75" rx="3"/>')
    body.append(f'<text x="40" y="{y3+20}" class="sm">'
                f'measured on our torus, {TORUS_VERTS} vertices, 64-byte lines</text>')

    cols = [(300, "interleaved"), (470, "separate"), (620, "winner")]
    for cx, name in cols:
        body.append(f'<text x="{cx}" y="{y3+40}" class="xs muted" '
                    f'text-anchor="middle">{name}</text>')

    rows = [
        ("lines to fetch ONE vertex",
         f"{LINES_INTER_WORST}", f"{LINES_SEP_WORST}", "interleaved"),
        ("lines for a positions-only pass",
         f"{LINES_POS_INTER}", f"{LINES_POS_SEP}", "separate"),
    ]
    for i, (what, a, b, win) in enumerate(rows):
        yy = y3 + 62 + i * 24
        body.append(f'<text x="40" y="{yy}" class="xs">{what}</text>')
        cls_a = "t-ok" if win == "interleaved" else "t-bad"
        cls_b = "t-ok" if win == "separate" else "t-bad"
        body.append(f'<text x="300" y="{yy}" class="xs mono {cls_a}" text-anchor="middle">{a}</text>')
        body.append(f'<text x="470" y="{yy}" class="xs mono {cls_b}" text-anchor="middle">{b}</text>')
        body.append(f'<text x="620" y="{yy}" class="xs t-hi" text-anchor="middle">{win}</text>')

    return svg(W, H, "Interleaved and separate vertex layouts, byte by byte",
               "Top: one vertex buffer of 32-byte records, position then normal then uv, "
               "with 64-byte cache lines drawn over it — each line holds exactly two whole "
               "vertices. Middle: the same data as three separate buffers, one per attribute. "
               "Bottom: two measurements. Fetching one vertex costs 1 cache line interleaved "
               "and up to 5 separate; sweeping every position costs 613 lines interleaved and "
               "230 separate.",
               "\n".join("  " + b for b in body), "f45a")


# =============================================================================
# Figure 2 — the vertex, declared in three places
# =============================================================================
# ONE CLAIM: the layout is a contract written in two languages and joined only by
# an integer, and of six ways to break it, SDL notices one.
def fig2():
    W, H = 720, 460
    body = []

    body.append('<text x="24" y="22" class="sm">'
                'the same six inputs, written three times &#8212; joined only by '
                '<tspan class="t-hi">location</tspan></text>')

    COLS = [(24, 226, "shaders/mesh.vert.hlsl", "you write this"),
            (286, 158, "mesh.vert.json", "shadercross writes this"),
            (466, 230, "src/gfx/gpu_mesh.cpp", "you write this too")]

    for x, w, title, sub in COLS:
        body.append(f'<rect x="{x}" y="36" width="{w}" height="216" class="fill-soft ink" '
                    'stroke-width="0.75" rx="3"/>')
        body.append(f'<text x="{x+8}" y="52" class="xs mono t-hi">{title}</text>')
        body.append(f'<text x="{x+8}" y="64" class="xs muted">{sub}</text>')

    rows = [
        (0, "position",  "float3", "float3", "FLOAT3 @ 0",  C_POS),
        (1, "normal",    "float3", "float3", "FLOAT3 @12",  C_NRM),
        (2, "uv",        "float2", "float2", "FLOAT2 @24",  C_UV),
        (3, "placement", "float4", "float4", "FLOAT4 @ 0",  C_INST),
        (4, "spin",      "float2", "float2", "FLOAT2 @16",  C_INST),
        (5, "tint",      "float4", "float4", "UBYTE4_NORM @24", C_INST),
    ]
    for i, (loc, name, hlsl, json_t, cpp, col) in enumerate(rows):
        y = 84 + i * 27
        body.append(f'<rect x="26" y="{y-11}" width="{W-52}" height="24" fill="{col}" '
                    'fill-opacity="0.09" stroke="none"/>')
        body.append(f'<text x="32" y="{y+4}" class="xs mono">{hlsl} {name} :</text>')
        body.append(f'<text x="242" y="{y+4}" class="xs mono t-hi" text-anchor="end">'
                    f'TEXCOORD{loc}</text>')
        body.append(f'<text x="294" y="{y+4}" class="xs mono">"{json_t}"</text>')
        body.append(f'<text x="436" y="{y+4}" class="xs mono t-hi" text-anchor="end">'
                    f'loc {loc}</text>')
        body.append(f'<text x="474" y="{y+4}" class="xs mono">{cpp}</text>')
        # the join
        body.append(f'<line x1="248" y1="{y}" x2="288" y2="{y}" class="ink-soft" '
                    'stroke-width="0.75" stroke-dasharray="2 2"/>')
        body.append(f'<line x1="442" y1="{y}" x2="470" y2="{y}" class="ink-soft" '
                    'stroke-width="0.75" stroke-dasharray="2 2"/>')

    body.append(f'<text x="{W//2}" y="268" class="xs muted" text-anchor="middle">'
                'slot 0 &#8212; per vertex, pitch 32 &#160;&#160;|&#160;&#160; '
                'slot 1 &#8212; per instance, pitch 28</text>')

    # ---- The trial table ---------------------------------------------------
    ty = 292
    body.append(f'<rect x="24" y="{ty}" width="{W-48}" height="152" class="fill-soft ink" '
                'stroke-width="0.75" rx="3"/>')
    body.append(f'<text x="40" y="{ty+18}" class="sm">'
                'six layouts, and which of the two possible checks notices</text>')
    body.append(f'<text x="470" y="{ty+36}" class="xs muted" text-anchor="middle">'
                'check_layout</text>')
    body.append(f'<text x="614" y="{ty+36}" class="xs muted" text-anchor="middle">'
                'pipeline creation</text>')

    for i, (what, ours_clean, sdl_ok) in enumerate(LAYOUT_TRIALS):
        yy = ty + 56 + i * 16
        body.append(f'<text x="40" y="{yy}" class="xs">{what}</text>')
        if i == 0:
            body.append(f'<text x="470" y="{yy}" class="xs muted" text-anchor="middle">&#8212;</text>')
            body.append(f'<text x="614" y="{yy}" class="xs muted" text-anchor="middle">&#8212;</text>')
            continue
        body.append(f'<text x="470" y="{yy}" class="xs {"t-bad" if ours_clean else "t-ok"}" '
                    f'text-anchor="middle">{"silent" if ours_clean else "CAUGHT"}</text>')
        body.append(f'<text x="614" y="{yy}" class="xs {"t-bad" if sdl_ok else "t-ok"}" '
                    f'text-anchor="middle">{"created" if sdl_ok else "REFUSED"}</text>')

    return svg(W, H, "The vertex layout, declared three times and joined by location",
               "The HLSL declaration, the JSON reflection derived from it, and the C++ "
               "pipeline description, side by side. Only the location number connects them. "
               "Below, six layouts: SDL refuses exactly one — the one whose shader input "
               "nothing supplies — and our own check_layout catches three of the five "
               "mistakes.",
               "\n".join("  " + b for b in body), "f45b")


# =============================================================================
# Figure 3 — what a wrong pitch actually does
# =============================================================================
# Real pixels. The crop is the union of the three bounding boxes verify_45 §C
# reported, so nothing is cropped away that the smear reached.
def fig3():
    W, H = 720, 372
    body = []
    CROP = (206, 58, 306, 154)      # 100 x 96 of the 512 x 288 target: the
                                    # union of the three bounding boxes
    CELL = 2
    PX = 4
    LEVELS = 4

    body.append('<text x="24" y="22" class="sm">'
                'the same torus, the same shader, the same buffer &#8212; '
                'three values of one integer</text>')

    panels = [
        ("scratch/l45_pitch32.ppm", "pitch 32", "correct", COVER_RIGHT, True),
        ("scratch/l45_pitch28.ppm", "pitch 28", "four bytes short", COVER_SHORT, False),
        ("scratch/l45_pitch36.ppm", "pitch 36", "four bytes long", COVER_LONG, False),
    ]
    for i, (path, name, sub, cover, good) in enumerate(panels):
        x = 40 + i * 216
        gw, gh, rects = panel(path, CROP, CELL, x, 40, PX,
                              [hexrgb(C_TINT[2])], LEVELS)
        body += rects
        body.append(f'<rect x="{x}" y="40" width="{gw*PX}" height="{gh*PX}" '
                    'class="ink" fill="none" stroke-width="0.75"/>')
        cls = "t-ok" if good else "t-bad"
        body.append(f'<text x="{x+gw*PX//2}" y="{40+gh*PX+16}" class="sm mono {cls}" '
                    f'text-anchor="middle">{name}</text>')
        body.append(f'<text x="{x+gw*PX//2}" y="{40+gh*PX+29}" class="xs muted" '
                    f'text-anchor="middle">{sub} &#183; {cover:,} px</text>')

    # ---- Why: the address arithmetic --------------------------------------
    ay = 286
    body.append(f'<rect x="24" y="{ay}" width="{W-48}" height="76" class="fill-soft ink" '
                'stroke-width="0.75" rx="3"/>')
    body.append(f'<text x="40" y="{ay+18}" class="sm">'
                'why it shatters rather than shifts: the error <tspan class="t-hi">'
                'accumulates</tspan></text>')
    body.append(f'<text x="40" y="{ay+38}" class="xs mono">'
                'address of vertex i = buffer + i &#215; pitch</text>')
    for j, (label, expr, cls) in enumerate([
            ("vertex 0", "+ 0", "muted"),
            ("vertex 1", "&#8722;4 bytes", "t-bad"),
            ("vertex 100", "&#8722;400 bytes", "t-bad"),
            ("vertex 1224", "&#8722;4,896 bytes", "t-bad")]):
        xx = 40 + j * 168
        body.append(f'<text x="{xx}" y="{ay+58}" class="xs">{label}</text>')
        body.append(f'<text x="{xx}" y="{ay+70}" class="xs mono {cls}">{expr}</text>')

    return svg(W, H, "A vertex pitch that is wrong by four bytes",
               "Three panels of actual downloaded pixels. With the correct pitch of 32 a "
               "torus appears, covering 3,696 pixels. With 28 or 36 the same buffer and the "
               "same shader produce a shattered cloud covering about 5,000. The reason is in "
               "the strip below: the address of vertex i is the buffer plus i times the pitch, "
               "so a four-byte error is four bytes at vertex 1 and nearly five kilobytes by "
               "the end of the mesh.",
               "\n".join("  " + b for b in body), "f45c")


# =============================================================================
# Figure 4 — what the index buffer buys, on our own mesh
# =============================================================================
# ONE CLAIM: 13,824 bytes of indices remove 181,984 bytes of duplicated vertices,
# and the vertex shader gets a range instead of a fixed cost.
def fig4():
    W, H = 720, 340
    body = []

    body.append('<text x="24" y="22" class="sm">'
                'assets/torus.obj &#8212; 2,304 triangles over 1,225 vertices, so the average '
                'vertex is named <tspan class="t-hi">5.6 times</tspan></text>')

    # ---- Left: the sharing, drawn ------------------------------------------
    cx, cy, r = 108.0, 100.0, 38.0
    pts = []
    for k in range(6):
        a = 6.28318531 * k / 6.0
        pts.append((cx + r * __import__("math").cos(a), cy + r * __import__("math").sin(a)))
    for k in range(6):
        p, q = pts[k], pts[(k + 1) % 6]
        body.append(f'<path d="M{f(cx)},{f(cy)} L{f(p[0])},{f(p[1])} L{f(q[0])},{f(q[1])} z" '
                    f'class="ink-soft" fill="{C_POS}" fill-opacity="0.13" stroke-width="0.75"/>')
    for p in pts:
        body.append(f'<circle cx="{f(p[0])}" cy="{f(p[1])}" r="2.6" class="ink-soft" '
                    'fill="var(--dia-bg)" stroke-width="0.75"/>')
    body.append(f'<circle cx="{f(cx)}" cy="{f(cy)}" r="4.6" fill="{C_UV}" class="ink" '
                'stroke-width="0.9"/>')
    body.append(f'<text x="{f(cx)}" y="{f(cy+r+20)}" class="xs t-hi" text-anchor="middle">'
                '1 vertex &#183; 6 indices</text>')
    body.append(f'<text x="{f(cx)}" y="{f(cy+r+34)}" class="xs muted" text-anchor="middle">'
                'six triangles meet here</text>')

    # ---- Middle: the byte bars ---------------------------------------------
    BX = 214.0
    BAR = 300.0
    scale = BAR / EXP_TOTAL

    body.append(f'<text x="{f(BX)}" y="52" class="xs muted">bytes on the device</text>')

    rows = [
        ("indexed", [(IDX_VBYTES, C_POS, "vertices"), (IDX_IBYTES, C_UV, "idx")], IDX_TOTAL),
        ("expanded", [(EXP_VBYTES, C_BAD, "vertices, every share duplicated")], EXP_TOTAL),
    ]
    for i, (name, segs, total) in enumerate(rows):
        y = 66 + i * 40
        body.append(f'<text x="{f(BX)}" y="{y+15}" class="xs">{name}</text>')
        x = BX + 62
        for n, col, txt in segs:
            wpx = n * scale
            body.append(f'<rect x="{f(x)}" y="{y}" width="{f(wpx)}" height="22" fill="{col}" '
                        'fill-opacity="0.8" stroke="none"/>')
            if wpx > 74:
                body.append(f'<text x="{f(x+wpx/2)}" y="{y+15}" class="xs mono t-inv" '
                            f'text-anchor="middle">{txt}</text>')
            x += wpx
        body.append(f'<text x="{f(x+8)}" y="{y+15}" class="xs mono">{total:,}</text>')

    body.append(f'<text x="{f(BX+62)}" y="152" class="xs t-hi">'
                f'{BYTE_RATIO}&#215; smaller &#8212; the saving exceeds the index buffer '
                'itself</text>')

    # ---- Bottom: invocations ------------------------------------------------
    iy = 186
    body.append(f'<rect x="24" y="{iy}" width="{W-48}" height="98" class="fill-soft ink" '
                'stroke-width="0.75" rx="3"/>')
    body.append(f'<text x="40" y="{iy+18}" class="sm">'
                'vertex-shader invocations for one un-instanced draw</text>')

    ax0, ax1 = 190.0, 620.0
    def at(v):
        return ax0 + (ax1 - ax0) * (v / INVOKE_EXPANDED)

    body.append(f'<text x="40" y="{iy+46}" class="xs">indexed</text>')
    body.append(f'<line x1="{f(ax0)}" y1="{iy+42}" x2="{f(ax1)}" y2="{iy+42}" '
                'class="ink-soft" stroke-width="0.75"/>')
    body.append(f'<rect x="{f(at(INVOKE_BEST))}" y="{iy+35}" '
                f'width="{f(at(INVOKE_WORST)-at(INVOKE_BEST))}" height="14" fill="{C_POS}" '
                'fill-opacity="0.30" stroke="none"/>')
    for v, lab, cls, anch in [(INVOKE_BEST, f"{INVOKE_BEST:,}", "t-ok", "middle"),
                              (INVOKE_WORST, f"{INVOKE_WORST:,}", "t-bad", "middle")]:
        body.append(f'<line x1="{f(at(v))}" y1="{iy+31}" x2="{f(at(v))}" y2="{iy+53}" '
                    'class="hi" stroke-width="1.2"/>')
        body.append(f'<text x="{f(at(v))}" y="{iy+28}" class="xs mono {cls}" '
                    f'text-anchor="{anch}">{lab}</text>')

    body.append(f'<text x="40" y="{iy+76}" class="xs">expanded</text>')
    body.append(f'<line x1="{f(ax0)}" y1="{iy+72}" x2="{f(ax1)}" y2="{iy+72}" '
                'class="ink-soft" stroke-width="0.75" stroke-dasharray="2 3"/>')
    body.append(f'<line x1="{f(at(INVOKE_EXPANDED))}" y1="{iy+62}" '
                f'x2="{f(at(INVOKE_EXPANDED))}" y2="{iy+82}" class="hi" stroke-width="1.2"/>')
    body.append(f'<text x="{f(at(INVOKE_EXPANDED)-6)}" y="{iy+68}" class="xs mono t-bad" '
                f'text-anchor="end">{INVOKE_EXPANDED:,} exactly</text>')
    body.append(f'<text x="{f(ax0+6)}" y="{iy+90}" class="xs muted">'
                'a RANGE, because the post-transform cache decides where in it you land'
                '</text>')

    # ---- The seam note -----------------------------------------------------
    body.append(f'<text x="24" y="{H-12}" class="xs muted">'
                f'the file holds {OBJ_POSITIONS:,} positions; the loader produces '
                f'<tspan class="t-hi">{TORUS_VERTS:,}</tspan> vertices &#8212; the uv seam '
                'splits every vertex where u wraps from 1 back to 0 (Lesson 3.5 &#167;3)</text>')

    return svg(W, H, "What the index buffer buys on our torus",
               "Left: a vertex where six triangles meet is stored once and named six times. "
               "Middle: the indexed form is 39,200 bytes of vertices plus 13,824 of indices, "
               "53,024 in total, against 221,184 for the expanded form — 4.17 times smaller, "
               "and the saving exceeds the cost of the index buffer. Bottom: the indexed draw "
               "invokes the vertex shader somewhere between 1,225 and 6,912 times depending "
               "on the post-transform cache; the expanded draw invokes it 6,912 times with no "
               "possibility of doing better.",
               "\n".join("  " + b for b in body), "f45d")


# =============================================================================
# Figure 5 — input_rate: two buffers, two cursors
# =============================================================================
# ONE CLAIM: instancing is one enum value. The shader cannot tell.
def fig5():
    W, H = 720, 380
    body = []

    body.append('<text x="24" y="22" class="sm">'
                'one draw call, seven instances &#8212; and the only thing that makes slot 1 '
                'different is <tspan class="t-hi">input_rate</tspan></text>')

    CW = 30.0
    X0 = 132.0

    def strip(y, n, colour, labels, slot, rate, note):
        out = [f'<text x="24" y="{y+13}" class="xs">slot {slot}</text>',
               f'<text x="24" y="{y+26}" class="xs mono t-hi">{rate}</text>',
               f'<text x="24" y="{y+40}" class="xs muted">{note}</text>']
        for k in range(n):
            x = X0 + k * CW
            out.append(f'<rect x="{f(x)}" y="{y}" width="{f(CW)}" height="26" fill="{colour}" '
                       'fill-opacity="0.7" stroke="var(--dia-bg)" stroke-width="0.75"/>')
            out.append(f'<text x="{f(x+CW/2)}" y="{y+17}" class="xs mono t-inv" '
                       f'text-anchor="middle">{labels[k]}</text>')
        return out

    Y_V, Y_I = 48, 122
    body += strip(Y_V, 12, C_POS, [f"v{k}" for k in range(12)], 0, "VERTEX",
                  "one element per vertex")
    body += strip(Y_I, 7, C_INST, [f"i{k}" for k in range(7)], 1, "INSTANCE",
                  "one element per instance")

    # ---- One fetch, traced through both strips ------------------------------
    vx = X0 + 9 * CW + CW / 2
    ix = X0 + 1 * CW + CW / 2
    JOIN = 196.0

    body.append(f'<rect x="{f(X0 + 9*CW)}" y="{Y_V}" width="{f(CW)}" height="26" '
                'class="hi" fill="none" stroke-width="1.4"/>')
    body.append(f'<rect x="{f(X0 + 1*CW)}" y="{Y_I}" width="{f(CW)}" height="26" '
                'class="hi" fill="none" stroke-width="1.4"/>')
    body.append(f'<line x1="{f(vx)}" y1="{Y_V+26}" x2="{f(vx)}" y2="{f(JOIN)}" '
                'class="hi" stroke-width="1" stroke-dasharray="3 2"/>')
    body.append(f'<line x1="{f(ix)}" y1="{Y_I+26}" x2="{f(ix)}" y2="{f(JOIN)}" '
                'class="hi" stroke-width="1" stroke-dasharray="3 2"/>')
    body.append(f'<line x1="{f(ix)}" y1="{f(JOIN)}" x2="{f(vx + 24)}" y2="{f(JOIN)}" '
                'class="hi" stroke-width="1" marker-end="url(#e-h)"/>')

    body.append(f'<rect x="{f(vx + 32)}" y="{f(JOIN-22)}" width="216" height="44" '
                'class="fill-soft ink" stroke-width="0.75" rx="3"/>')
    body.append(f'<text x="{f(vx + 44)}" y="{f(JOIN-6)}" class="xs">'
                'vertex 9 of instance 1 fetches</text>')
    body.append(f'<text x="{f(vx + 44)}" y="{f(JOIN+10)}" class="xs mono t-hi">'
                'v9 from slot 0, i1 from slot 1</text>')

    # ---- The bytes ---------------------------------------------------------
    by = 240
    body.append(f'<rect x="24" y="{by}" width="{W-48}" height="112" class="fill-soft ink" '
                'stroke-width="0.75" rx="3"/>')
    body.append(f'<text x="40" y="{by+20}" class="sm">what actually moves</text>')

    bars = [
        ("uploaded ONCE, at startup", IDX_TOTAL, C_POS, f"{IDX_TOTAL:,} bytes of torus"),
        ("rewritten EVERY frame", PER_FRAME_BYTES, C_INST,
         f"{PER_FRAME_BYTES} bytes = {INSTANCES} &#215; {INSTANCE_BYTES}"),
    ]
    sc = 300.0 / IDX_TOTAL
    for i, (name, n, col, txt) in enumerate(bars):
        y = by + 38 + i * 32
        body.append(f'<text x="40" y="{y+15}" class="xs">{name}</text>')
        wpx = max(2.0, n * sc)
        body.append(f'<rect x="228" y="{y}" width="{f(wpx)}" height="20" fill="{col}" '
                    'fill-opacity="0.8" stroke="none"/>')
        body.append(f'<text x="{f(228+wpx+8)}" y="{y+15}" class="xs mono">{txt}</text>')

    body.append(f'<text x="40" y="{by+102}" class="xs t-hi">'
                f'{PER_FRAME_PERCENT}% of the data on the device changes per frame &#8212; '
                'that is the argument for instancing, as a number</text>')

    return svg(W, H, "Vertex rate and instance rate, and what each costs",
               "Slot 0 advances one element per vertex; slot 1 advances one element per "
               "instance, so every vertex of instance 1 reads the same placement record. "
               "Nothing else about the two buffers differs — same buffer type, same bind "
               "call, same attribute list. Below: 53,024 bytes of torus are uploaded once and "
               "never touched again, while 196 bytes of placement are rewritten each frame, "
               "which is 0.37% of the data on the device.",
               "\n".join("  " + b for b in body), "f45e")


# =============================================================================
# Figure 6 — the scene, from the actual pixels
# =============================================================================
def fig6():
    W, H = 720, 490
    body = []
    CROP = (88, 34, 424, 220)     # 336 x 186 — the scene's bounding box, plus a margin
    CELL = 3
    PX = 5

    body.append('<text x="24" y="22" class="sm">'
                'one vertex buffer, one index buffer, one draw call &#8212; '
                '<tspan class="t-hi">seven tori</tspan></text>')

    gw, gh, rects = panel("scratch/l45_scene.ppm", CROP, CELL, 30, 38, PX,
                          [hexrgb(c) for c in C_TINT], 4)
    body += rects
    body.append(f'<rect x="30" y="38" width="{gw*PX}" height="{gh*PX}" class="ink" '
                'fill="none" stroke-width="0.75"/>')

    y = 38 + gh * PX + 20
    body.append(f'<text x="30" y="{y}" class="xs muted">'
                f'rendered offscreen at 512&#215;288 and downloaded, then quantised to four '
                'shades of each tint</text>')
    body.append(f'<text x="30" y="{y+13}" class="xs muted">'
                'so that this figure is a few hundred rectangles rather than 147,456</text>')

    y += 38
    facts = [
        ("SDL_DrawGPUIndexedPrimitives", f"({INDEX_ARG}, 7, 0, 0, 0)"),
        ("vertex buffers bound", "2 — slot 0 per vertex, slot 1 per instance"),
        ("pixels covered", "32,006 of 147,456 (21.7%)"),
        ("per-frame upload", f"{PER_FRAME_BYTES} bytes"),
    ]
    for i, (k, v) in enumerate(facts):
        yy = y + i * 17
        body.append(f'<text x="30" y="{yy}" class="xs mono t-hi">{k}</text>')
        body.append(f'<text x="290" y="{yy}" class="xs mono">{v}</text>')

    return svg(W, H, "Seven instances of one mesh, from one draw call",
               "The actual frame, rendered offscreen and downloaded so it can be counted "
               "rather than described. Seven tori, each at its own place and its own angle "
               "and its own tint, all drawn from a single vertex buffer and a single index "
               "buffer by one call to SDL_DrawGPUIndexedPrimitives with an instance count of "
               "seven. The uv grid on each surface is the third vertex attribute made "
               "visible.",
               "\n".join("  " + b for b in body), "f45f")


INDEX_ARG = f"{TORUS_INDICES:,}"


if __name__ == "__main__":
    write("l45_fig1.svg", fig1())
    write("l45_fig2.svg", fig2())
    write("l45_fig3.svg", fig3())
    write("l45_fig4.svg", fig4())
    write("l45_fig5.svg", fig5())
    write("l45_fig6.svg", fig6())
