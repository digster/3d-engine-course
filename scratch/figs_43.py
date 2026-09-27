#!/usr/bin/env python3
"""Compute every SVG figure for Lesson 4.3 from real data.

Same discipline as 3.7 through 4.2: nothing is placed by eye, and every number
drawn here is one `scratch/verify_43.log` printed or one measured while writing
the lesson. Each figure's docstring states the ONE claim a reader must be able to
extract by MEASURING the picture.

Marker ids are namespaced per figure, and <text> never carries an inline `fill`
(course.css beats an SVG presentation attribute — apply-shared.py lints for it).

Writes scratch/l43_fig{1..6}.svg.
"""

# ---- Measured inputs --------------------------------------------------------

# Build-time cost of each hop, minimum of 15 runs, milliseconds.
HOP_GLSLC = 12.3          # HLSL -> SPIR-V
HOP_MSL = 1.5             # SPIR-V -> MSL
HOP_JSON = 1.5            # SPIR-V -> JSON
HOP_ALL_ONE = 15.3        # all three, one shader
HOP_ALL_FOUR = 61.2       # all three, four shaders

# verify_43 §A: (shader, hlsl, spv, msl, json) in bytes
SIZES = [
    ("triangle.vert", 1556, 756, 493, 307),
    ("triangle.frag", 895, 464, 342, 237),
    ("textured.vert", 1435, 1100, 585, 299),
    ("textured.frag", 1405, 1032, 524, 233),
]

# verify_43 §H: first (cold) creation, milliseconds
CREATE_COLD = 0.028       # all four
CREATE_EACH = 0.008       # one, typical

# verify_43 §E: what SDL does with wrong resource counts
COUNT_TRIALS = [
    ("correct", "smp 1, uni 1", True),
    ("too low", "smp 0, uni 0", True),
    ("too high", "smp 4, uni 4", True),
    ("absurd", "smp 99, uni 99", True),
]

# verify_43 §D
ENTRY_TRIALS = [("main0", True), ("main", False), ("not_a_function", False)]

# Verified with spirv-dis on our own compiled output.
SPACES = [
    ("vertex", "sampled textures, storage textures, storage buffers", "space0", "set 0"),
    ("vertex", "uniform buffers", "space1", "set 1"),
    ("fragment", "sampled textures, storage textures, storage buffers", "space2", "set 2"),
    ("fragment", "uniform buffers", "space3", "set 3"),
]

INK = "var(--dia-ink)"
C_SRC = "#5ac878"        # what you write
C_HUB = "#5acdd7"        # SPIR-V, the hub
C_OUT = "#5082e6"        # a backend's format
C_META = "#f0961e"       # the reflection
C_GONE = "#eb786e"       # not available here
C_OLD = "#78b4eb"


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
# Figure 1 — the toolchain
# =============================================================================
# ONE CLAIM: SPIR-V is the hub. One source language enters, everything else is
# translated FROM SPIR-V — so losing the front end costs you the whole toolchain
# and not merely one backend.
def fig1():
    W, H = 760, 400
    body = []

    body.append('<text x="24" y="26" class="sm">one source language, four outputs, '
                'and one hub in the middle</text>')
    body.append('<text x="24" y="46" class="xs muted">'
                'times are this machine&#8217;s, minimum of 15 runs, for one shader</text>')

    BW, BH = 128.0, 58.0
    src_x, src_y = 30.0, 168.0
    hub_x, hub_y = 292.0, 168.0
    out_x = 566.0

    # The source.
    body.append(f'<rect x="{f(src_x)}" y="{f(src_y)}" width="{f(BW)}" height="{f(BH)}" rx="5" '
                f'fill="{C_SRC}" opacity="0.18" stroke="{C_SRC}" stroke-width="1.6"/>')
    body.append(f'<text x="{f(src_x + BW / 2)}" y="{f(src_y + 26)}" class="xs mono" '
                f'text-anchor="middle">.hlsl</text>')
    body.append(f'<text x="{f(src_x + BW / 2)}" y="{f(src_y + 42)}" class="xs muted" '
                f'text-anchor="middle">what you write</text>')

    # The front end — two ways to get to SPIR-V, one of which is missing here.
    # Wide enough for the longest label inside them ("the fallback, used here"),
    # which at 102 px overflowed its own box. They sit above and below the .hlsl
    # box rather than beside it, so there is room to take.
    fe_x = src_x + 46
    fe_w = hub_x - fe_x - 14
    body.append(f'<rect x="{f(fe_x)}" y="{f(src_y - 42)}" width="{f(fe_w)}" height="34" rx="4" '
                f'class="fill-soft ink-soft" stroke-dasharray="4 4" stroke-width="1"/>')
    body.append(f'<text x="{f(fe_x + fe_w / 2)}" y="{f(src_y - 27)}" class="xs mono muted" '
                f'text-anchor="middle">shadercross + DXC</text>')
    body.append(f'<text x="{f(fe_x + fe_w / 2)}" y="{f(src_y - 14)}" class="xs" '
                f'text-anchor="middle">the sanctioned path</text>')

    body.append(f'<rect x="{f(fe_x)}" y="{f(src_y + 66)}" width="{f(fe_w)}" height="34" rx="4" '
                f'fill="{C_SRC}" opacity="0.14" stroke="{C_SRC}" stroke-width="1.4"/>')
    body.append(f'<text x="{f(fe_x + fe_w / 2)}" y="{f(src_y + 81)}" class="xs mono" '
                f'text-anchor="middle">glslc -x hlsl</text>')
    body.append(f'<text x="{f(fe_x + fe_w / 2)}" y="{f(src_y + 94)}" class="xs" '
                f'text-anchor="middle">the fallback, used here</text>')

    body.append(f'<line x1="{f(src_x + BW + 6)}" y1="{f(src_y + BH / 2)}" '
                f'x2="{f(hub_x - 8)}" y2="{f(hub_y + BH / 2)}" class="hi" stroke-width="2" '
                f'marker-end="url(#e-h)"/>')
    body.append(f'<text x="{f((src_x + BW + hub_x) / 2)}" y="{f(src_y + BH / 2 - 8)}" '
                f'class="xs mono t-hi" text-anchor="middle">{HOP_GLSLC:.1f} ms</text>')

    # The hub.
    body.append(f'<rect x="{f(hub_x)}" y="{f(hub_y)}" width="{f(BW)}" height="{f(BH)}" rx="5" '
                f'fill="{C_HUB}" opacity="0.20" stroke="{C_HUB}" stroke-width="2"/>')
    body.append(f'<text x="{f(hub_x + BW / 2)}" y="{f(hub_y + 26)}" class="xs mono" '
                f'text-anchor="middle">.spv</text>')
    body.append(f'<text x="{f(hub_x + BW / 2)}" y="{f(hub_y + 42)}" class="xs muted" '
                f'text-anchor="middle">SPIR-V &#8212; the hub</text>')

    # The outputs.
    outs = [
        (".msl", "Metal", C_OUT, f"{HOP_MSL:.1f} ms", True),
        (".dxil", "D3D12", C_GONE, "needs DXC", False),
        (".json", "the counts SDL wants", C_META, f"{HOP_JSON:.1f} ms", True),
    ]
    for i, (name, who, colour, cost, have) in enumerate(outs):
        y = 96.0 + i * 88.0
        body.append(f'<rect x="{f(out_x)}" y="{f(y)}" width="{f(BW + 34)}" height="{f(BH)}" rx="5" '
                    f'fill="{colour}" opacity="{0.18 if have else 0.10}" stroke="{colour}" '
                    f'stroke-width="1.6"{"" if have else " stroke-dasharray=\'4 4\'"}/>')
        body.append(f'<text x="{f(out_x + (BW + 34) / 2)}" y="{f(y + 25)}" class="xs mono'
                    f'{"" if have else " muted"}" text-anchor="middle">{name}</text>')
        body.append(f'<text x="{f(out_x + (BW + 34) / 2)}" y="{f(y + 41)}" class="xs muted" '
                    f'text-anchor="middle">{who}</text>')

        body.append(f'<line x1="{f(hub_x + BW + 6)}" y1="{f(hub_y + BH / 2)}" '
                    f'x2="{f(out_x - 8)}" y2="{f(y + BH / 2)}" '
                    f'class="{"ink-soft" if have else "ink-soft"}" stroke-width="1.5" '
                    f'{"" if have else "stroke-dasharray=\'3 3\' "}marker-end="url(#e-s)"/>')
        # 14 px of clearance, not 6: the middle arrow is nearly horizontal, so a
        # label at its midpoint lands ON the line. check-page.js caught it.
        body.append(f'<text x="{f((hub_x + BW + out_x) / 2)}" y="{f((hub_y + BH / 2 + y + BH / 2) / 2 - 14)}" '
                    f'class="xs mono muted" text-anchor="middle">{cost}</text>')

    body.append(f'<text x="24" y="{H - 16}" class="xs">'
                f'Everything on the right is translated <tspan class="t-hi">from SPIR-V</tspan>, '
                f'so a missing front end costs the whole toolchain &#8212; not one backend.</text>')

    return svg(W, H, "The shader toolchain, and why SPIR-V is in the middle",
               f"A flow diagram. On the left, a .hlsl file — what you write. Two possible front "
               f"ends carry it to SPIR-V: shadercross built with DirectXShaderCompiler, the "
               f"sanctioned path, drawn dashed because this machine does not have it; and glslc "
               f"with the -x hlsl flag, the fallback, which took {HOP_GLSLC:.1f} milliseconds. In "
               f"the middle sits the .spv file, the hub. From it three arrows lead right: .msl for "
               f"Metal at {HOP_MSL:.1f} ms, .dxil for D3D12 drawn dashed because it needs DXC, and "
               f".json at {HOP_JSON:.1f} ms carrying the resource counts SDL requires. Everything "
               f"on the right is translated from SPIR-V, which is why losing the front end costs "
               f"the whole toolchain rather than one backend.",
               "\n".join(body), "l43f1")


# =============================================================================
# Figure 2 — the register spaces
# =============================================================================
# ONE CLAIM: the space number is not free. Each stage-and-kind pair has exactly
# one legal space, it becomes exactly one SPIR-V descriptor set, and a reader can
# check their own shader with one command.
def fig2():
    W, H = 720, 372
    body = []

    body.append('<text x="24" y="26" class="sm">'
                'where a resource must be declared, per stage</text>')
    body.append('<text x="24" y="46" class="xs muted">'
                'fixed by SDL_gpu.h; the right-hand column is what our own compiled output '
                'actually contains</text>')

    X0, ROW_H = 28.0, 56.0
    Y0 = 96.0
    W_STAGE, W_KIND, W_SPACE, W_SET = 92.0, 300.0, 108.0, 108.0

    hdrs = [("stage", X0, W_STAGE), ("what you declare", X0 + W_STAGE + 8, W_KIND),
            ("HLSL register", X0 + W_STAGE + W_KIND + 16, W_SPACE),
            ("SPIR-V says", X0 + W_STAGE + W_KIND + W_SPACE + 24, W_SET)]
    for label, x, w in hdrs:
        body.append(f'<text x="{f(x + w / 2)}" y="{f(Y0 - 12)}" class="xs muted" '
                    f'text-anchor="middle">{label}</text>')

    for i, (stage, kind, space, dset) in enumerate(SPACES):
        y = Y0 + i * ROW_H
        colour = C_SRC if stage == "vertex" else C_OUT

        body.append(f'<rect x="{f(X0)}" y="{f(y)}" width="{f(W_STAGE)}" height="40" rx="4" '
                    f'fill="{colour}" opacity="0.16" stroke="{colour}" stroke-width="1.3"/>')
        body.append(f'<text x="{f(X0 + W_STAGE / 2)}" y="{f(y + 25)}" class="xs mono" '
                    f'text-anchor="middle">{stage}</text>')

        kx = X0 + W_STAGE + 8
        body.append(f'<rect x="{f(kx)}" y="{f(y)}" width="{f(W_KIND)}" height="40" rx="4" '
                    f'class="fill-soft ink-soft" stroke-width="1"/>')
        body.append(f'<text x="{f(kx + 10)}" y="{f(y + 25)}" class="xs">{kind}</text>')

        sx = kx + W_KIND + 8
        body.append(f'<rect x="{f(sx)}" y="{f(y)}" width="{f(W_SPACE)}" height="40" rx="4" '
                    f'fill="{C_META}" opacity="0.16" stroke="{C_META}" stroke-width="1.3"/>')
        body.append(f'<text x="{f(sx + W_SPACE / 2)}" y="{f(y + 25)}" class="xs mono" '
                    f'text-anchor="middle">{space}</text>')

        body.append(f'<line x1="{f(sx + W_SPACE + 4)}" y1="{f(y + 20)}" '
                    f'x2="{f(sx + W_SPACE + 14)}" y2="{f(y + 20)}" class="ink-soft" '
                    f'stroke-width="1.2" marker-end="url(#e-s)"/>')

        dx = sx + W_SPACE + 18
        body.append(f'<text x="{f(dx + W_SET / 2 - 10)}" y="{f(y + 25)}" class="xs mono" '
                    f'text-anchor="middle">{dset}</text>')

    y = Y0 + len(SPACES) * ROW_H + 6
    body.append(f'<text x="{f(X0)}" y="{f(y)}" class="xs">'
                f'Check your own shader rather than trusting the table:</text>')
    body.append(f'<text x="{f(X0)}" y="{f(y + 20)}" class="xs mono t-hi">'
                f'spirv-dis textured.frag.spv | grep DescriptorSet</text>')
    body.append(f'<text x="{f(X0)}" y="{f(y + 40)}" class="xs muted">'
                f'A wrong space compiles, links and runs &#8212; and reads whatever happens to be '
                f'bound at the slot you named.</text>')

    return svg(W, H, "Which register space each resource must use",
               "A four-row table. For vertex shaders, sampled textures, storage textures and "
               "storage buffers are declared in space0 and become SPIR-V descriptor set 0, while "
               "uniform buffers go in space1 and become set 1. For fragment shaders the same two "
               "kinds use space2 and space3, becoming sets 2 and 3. The mapping was verified by "
               "disassembling this course's own compiled shaders. A wrong space is not a compile "
               "error: the shader builds and runs and reads the wrong slot.",
               "\n".join(body), "l43f2")


# =============================================================================
# Figure 3 — what the HLSL became
# =============================================================================
# ONE CLAIM: compiled shader code is TINY — hundreds of bytes — and the reflection
# file that tells SDL how to bind it is the same order of size as the code.
def fig3():
    W, H = 720, 356
    X0, Y0 = 118.0, 74.0
    PW, PH = 500.0, 200.0
    top = 1700.0

    def px(v):
        return X0 + (v / top) * PW

    body = []
    body.append('<text x="24" y="26" class="sm">what each file weighs, in bytes</text>')
    body.append('<text x="24" y="46" class="xs muted">'
                'the .hlsl bar is mostly the comments this course writes &#8212; '
                'the code itself is smaller than its description</text>')

    series = [("hlsl", C_SRC, 1), ("spv", C_HUB, 2), ("msl", C_OUT, 3), ("json", C_META, 4)]
    row_h = PH / len(SIZES)
    bar_h = 9.0

    for i, row in enumerate(SIZES):
        name = row[0]
        y0 = Y0 + i * row_h
        body.append(f'<text x="{f(X0 - 10)}" y="{f(y0 + 26)}" class="xs mono" '
                    f'text-anchor="end">{name}</text>')
        for j, (label, colour, idx) in enumerate(series):
            v = row[idx]
            y = y0 + 6 + j * (bar_h + 2)
            body.append(f'<rect x="{f(X0)}" y="{f(y)}" width="{f(px(v) - X0)}" '
                        f'height="{f(bar_h)}" fill="{colour}" opacity="0.85"/>')
            body.append(f'<text x="{f(px(v) + 6)}" y="{f(y + 8)}" class="xs mono muted">{v}</text>')

    # Axis.
    ay = Y0 + PH + 6
    body.append(f'<line x1="{f(X0)}" y1="{f(ay)}" x2="{f(X0 + PW)}" y2="{f(ay)}" '
                f'class="ink-soft" stroke-width="1.2"/>')
    for v in (0, 500, 1000, 1500):
        body.append(f'<line x1="{f(px(v))}" y1="{f(ay)}" x2="{f(px(v))}" y2="{f(ay + 4)}" '
                    f'class="ink-soft" stroke-width="1"/>')
        body.append(f'<text x="{f(px(v))}" y="{f(ay + 18)}" class="xs mono muted" '
                    f'text-anchor="middle">{v}</text>')
    body.append(f'<text x="{f(X0)}" y="{f(ay + 34)}" class="xs muted">bytes</text>')

    lx = X0 + 200
    ly = ay + 34
    for label, colour, _idx in series:
        body.append(f'<rect x="{f(lx)}" y="{f(ly - 9)}" width="10" height="10" '
                    f'fill="{colour}" opacity="0.85"/>')
        body.append(f'<text x="{f(lx + 15)}" y="{f(ly)}" class="xs mono">.{label}</text>')
        lx += 74

    return svg(W, H, "The size of every file in the toolchain",
               "A grouped bar chart, four shaders by four file types. Compiled shader code is "
               "tiny: the largest SPIR-V here is 1,100 bytes and the largest MSL is 585. The "
               "reflection JSON, at roughly 230 to 310 bytes, is the same order of size as the "
               "code it describes. The HLSL bars are the longest only because this course's "
               "shaders carry more comment than code.",
               "\n".join(body), "l43f3")


# =============================================================================
# Figure 4 — the four counts, and what SDL does about them
# =============================================================================
# ONE CLAIM: the reflection file supplies exactly the four numbers the create-info
# demands — and SDL accepts wrong ones without complaint, so the file is the only
# thing standing between you and a silent bug.
def fig4():
    W, H = 720, 420
    body = []

    body.append('<text x="24" y="26" class="sm">'
                'four numbers, and where they have to come from</text>')

    JX, JW = 30.0, 296.0
    SX, SW = 392.0, 300.0
    Y0 = 76.0
    ROW = 34.0

    # Name the SAME shader the trials below use. Labelling the panel with one
    # file and measuring another is the sort of small inconsistency that makes a
    # reader distrust the whole figure.
    body.append(f'<text x="{f(JX)}" y="{f(Y0 - 12)}" class="xs muted">'
                f'textured.frag.json &#8212; written by shadercross</text>')
    body.append(f'<text x="{f(SX)}" y="{f(Y0 - 12)}" class="xs muted">'
                f'SDL_GPUShaderCreateInfo</text>')

    fields = [
        ("\"samplers\"", "num_samplers"),
        ("\"storage_textures\"", "num_storage_textures"),
        ("\"storage_buffers\"", "num_storage_buffers"),
        ("\"uniform_buffers\"", "num_uniform_buffers"),
    ]

    body.append(f'<rect x="{f(JX)}" y="{f(Y0 - 4)}" width="{f(JW)}" height="{f(len(fields) * ROW + 12)}" '
                f'rx="5" fill="{C_META}" opacity="0.10" stroke="{C_META}" stroke-width="1.4"/>')
    body.append(f'<rect x="{f(SX)}" y="{f(Y0 - 4)}" width="{f(SW)}" height="{f(len(fields) * ROW + 12)}" '
                f'rx="5" class="fill-soft ink-soft" stroke-width="1.4"/>')

    for i, (jkey, sfield) in enumerate(fields):
        y = Y0 + 18 + i * ROW
        body.append(f'<text x="{f(JX + 14)}" y="{f(y)}" class="xs mono">{jkey}</text>')
        body.append(f'<line x1="{f(JX + JW + 8)}" y1="{f(y - 4)}" x2="{f(SX - 8)}" y2="{f(y - 4)}" '
                    f'class="hi" stroke-width="1.4" marker-end="url(#e-h)"/>')
        body.append(f'<text x="{f(SX + 14)}" y="{f(y)}" class="xs mono">{sfield}</text>')

    # What SDL does when they are wrong.
    ty = Y0 + len(fields) * ROW + 44
    body.append(f'<text x="{f(JX)}" y="{f(ty)}" class="sm">'
                f'and what SDL does when they are wrong</text>')
    body.append(f'<text x="{f(JX)}" y="{f(ty + 18)}" class="xs muted">'
                f'measured on textured.frag, which really has 1 sampler and 1 uniform buffer</text>')

    for i, (label, counts, created) in enumerate(COUNT_TRIALS):
        y = ty + 44 + i * 28
        colour = C_SRC if i == 0 else C_GONE
        body.append(f'<rect x="{f(JX)}" y="{f(y - 13)}" width="14" height="14" rx="3" '
                    f'fill="{colour}" opacity="0.8"/>')
        body.append(f'<text x="{f(JX + 24)}" y="{f(y)}" class="xs">{label}</text>')
        body.append(f'<text x="{f(JX + 110)}" y="{f(y)}" class="xs mono muted">{counts}</text>')
        body.append(f'<text x="{f(JX + 260)}" y="{f(y)}" class="xs mono">&#8594;</text>')
        body.append(f'<text x="{f(JX + 290)}" y="{f(y)}" class="xs mono'
                    f'{" t-hi" if i > 0 else ""}">{"created" if created else "REFUSED"}</text>')

    body.append(f'<text x="{f(JX)}" y="{f(H - 18)}" class="xs">'
                f'<tspan class="t-hi">Every one was accepted</tspan>, including 99 of each. '
                f'Nothing checks these but you &#8212; so read them out of the file.</text>')

    return svg(W, H, "The reflection file supplies exactly what the create-info demands",
               "On the left, the four integer keys in shadercross's JSON reflection output; on "
               "the right, the four fields of SDL_GPUShaderCreateInfo, joined one to one by "
               "arrows. Below, four trials on a fragment shader that really has one sampler and "
               "one uniform buffer: correct counts, counts that are too low, too high, and "
               "absurdly high at 99 each. All four were accepted by SDL_CreateGPUShader. Nothing "
               "validates these numbers at creation, which is why they must be read from the "
               "reflection file rather than remembered.",
               "\n".join(body), "l43f4")


# =============================================================================
# Figure 5 — where the time goes
# =============================================================================
# ONE CLAIM: the expensive part happens once, at build time, and what is left at
# program start is microseconds — because the real compile has not happened yet.
def fig5():
    W, H = 720, 320
    X0, XW = 150.0, 470.0
    body = []

    body.append('<text x="24" y="26" class="sm">'
                'when each part of the toolchain runs, and what it costs</text>')

    rows = [
        ("build time", f"{HOP_ALL_FOUR:.1f} ms", "four shaders, three hops each, once per edit",
         HOP_ALL_FOUR, C_SRC),
        ("program start", f"{CREATE_COLD:.3f} ms", "SDL_CreateGPUShader x4 — 2,200x cheaper",
         CREATE_COLD, C_OUT),
        ("first draw", "not yet measured", "the real compile — Lesson 4.4 finds it",
         0.0, C_GONE),
    ]

    scale = HOP_ALL_FOUR * 1.08
    for i, (when, cost, note, value, colour) in enumerate(rows):
        y = 82.0 + i * 66.0
        body.append(f'<text x="{f(X0 - 12)}" y="{f(y + 22)}" class="xs mono" '
                    f'text-anchor="end">{when}</text>')

        if value > 0.0:
            w = max(XW * value / scale, 2.0)
            body.append(f'<rect x="{f(X0)}" y="{f(y)}" width="{f(w)}" height="32" rx="3" '
                        f'fill="{colour}" opacity="0.85"/>')
            body.append(f'<text x="{f(X0 + w + 10)}" y="{f(y + 21)}" class="xs mono">{cost}</text>')
        else:
            body.append(f'<rect x="{f(X0)}" y="{f(y)}" width="{f(XW)}" height="32" rx="3" '
                        f'fill="{colour}" opacity="0.10" stroke="{colour}" stroke-width="1.4" '
                        f'stroke-dasharray="5 4"/>')
            body.append(f'<text x="{f(X0 + XW / 2)}" y="{f(y + 21)}" class="xs mono muted" '
                        f'text-anchor="middle">{cost}</text>')

        body.append(f'<text x="{f(X0)}" y="{f(y + 48)}" class="xs muted">{note}</text>')

    body.append(f'<text x="24" y="{H - 44}" class="xs">'
                f'Eight microseconds cannot be a compile. The MSL is handed over at start and '
                f'turned into machine code</text>')
    body.append(f'<text x="24" y="{H - 26}" class="xs">'
                f'later &#8212; when the driver finally knows the target formats and vertex '
                f'layout it must be specialised to.</text>')

    return svg(W, H, "When each part of the shader toolchain runs",
               f"Three horizontal bars on a shared scale. Build time is {HOP_ALL_FOUR:.1f} "
               f"milliseconds — four shaders, three compilation hops each, paid once per edit. "
               f"Program start is {CREATE_COLD:.3f} milliseconds for all four calls to "
               f"SDL_CreateGPUShader, roughly two thousand times cheaper and far too fast to "
               f"include compiling anything. The third bar, first draw, is drawn empty and dashed: "
               f"the real compile has not happened yet, and Lesson 4.4 is where it shows up.",
               "\n".join(body), "l43f5")


# =============================================================================
# Figure 6 — the entry point
# =============================================================================
# ONE CLAIM: the function you wrote is not the function SDL must be told about,
# and passing the name you wrote fails.
def fig6():
    W, H = 720, 300
    body = []

    body.append('<text x="24" y="26" class="sm">'
                'the name of your entry point, through the toolchain</text>')

    BW, BH = 168.0, 56.0
    Y = 82.0
    stages = [
        (".hlsl", "main", C_SRC),
        (".spv", "main", C_HUB),
        (".msl", "main0", C_OUT),
    ]
    gap = (W - 60 - len(stages) * BW) / (len(stages) - 1)

    for i, (fmt, entry, colour) in enumerate(stages):
        x = 30.0 + i * (BW + gap)
        body.append(f'<rect x="{f(x)}" y="{f(Y)}" width="{f(BW)}" height="{f(BH)}" rx="5" '
                    f'fill="{colour}" opacity="0.18" stroke="{colour}" stroke-width="1.6"/>')
        body.append(f'<text x="{f(x + BW / 2)}" y="{f(Y + 24)}" class="xs mono" '
                    f'text-anchor="middle">{fmt}</text>')
        body.append(f'<text x="{f(x + BW / 2)}" y="{f(Y + 42)}" class="xs mono'
                    f'{" t-hi" if entry != "main" else ""}" '
                    f'text-anchor="middle">entry: {entry}</text>')

        if i + 1 < len(stages):
            body.append(f'<line x1="{f(x + BW + 8)}" y1="{f(Y + BH / 2)}" '
                        f'x2="{f(x + BW + gap - 8)}" y2="{f(Y + BH / 2)}" class="ink-soft" '
                        f'stroke-width="1.6" marker-end="url(#e-s)"/>')

    body.append(f'<text x="{f(30.0 + 2 * (BW + gap) + BW / 2)}" y="{f(Y + BH + 20)}" '
                f'class="xs t-hi" text-anchor="middle">'
                f'renamed &#8212; `main` is reserved in MSL</text>')

    ty = Y + BH + 62
    body.append(f'<text x="30" y="{f(ty)}" class="xs">'
                f'What SDL_CreateGPUShader does with each name, measured:</text>')
    for i, (name, ok) in enumerate(ENTRY_TRIALS):
        y = ty + 26 + i * 26
        colour = C_SRC if ok else C_GONE
        body.append(f'<rect x="36" y="{f(y - 12)}" width="12" height="12" rx="3" '
                    f'fill="{colour}" opacity="0.85"/>')
        body.append(f'<text x="58" y="{f(y)}" class="xs mono">"{name}"</text>')
        body.append(f'<text x="240" y="{f(y)}" class="xs mono">&#8594;</text>')
        body.append(f'<text x="272" y="{f(y)}" class="xs mono{"" if ok else " t-hi"}">'
                    f'{"created" if ok else "REFUSED"}</text>')

    body.append(f'<text x="360" y="{f(ty + 52)}" class="xs">'
                f'A good failure: it is caught at creation,</text>')
    body.append(f'<text x="360" y="{f(ty + 70)}" class="xs">'
                f'with a message, rather than at the first draw.</text>')

    return svg(W, H, "The entry point is renamed on the way to MSL",
               "Three boxes left to right. The HLSL declares a function called main; the SPIR-V "
               "still calls it main; the MSL calls it main0, because main is a reserved identifier "
               "in Metal Shading Language and SPIRV-Cross renames it. Below, three measured "
               "trials of SDL_CreateGPUShader: the name main0 created a shader, while main and a "
               "nonsense name were both refused. The mistake is caught at creation, with a "
               "message, rather than at the first draw.",
               "\n".join(body), "l43f6")


def main():
    print("Lesson 4.3 figures:")
    write("l43_fig1.svg", fig1())
    write("l43_fig2.svg", fig2())
    write("l43_fig3.svg", fig3())
    write("l43_fig4.svg", fig4())
    write("l43_fig5.svg", fig5())
    write("l43_fig6.svg", fig6())


if __name__ == "__main__":
    main()
