#!/usr/bin/env python3
"""Compute every SVG figure for Lesson 4.6 from real data.

Figure 6 is drawn from scratch/l46_scene.ppm, which verify_46 §F renders offscreen
and downloads. The raster helpers are Lesson 4.5's, imported rather than copied —
they are an authoring tool, not lesson content.

Every number in every caption comes from scratch/verify_46.log.

Writes scratch/l46_fig{1..6}.svg.
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_45 import (svg, write, f, panel, hexrgb, C_TINT,
                     C_POS, C_NRM, C_UV, C_INST, C_BAD, C_HUB)

# ---- Measured inputs, from scratch/verify_46.log ----------------------------

TORUS_VERTS = 1225
INSTANCES = 7

CAMERA_BYTES = 64
LIGHT_BYTES = 32
INSTANCE_BYTES = 196
PER_FRAME_TOTAL = 292

# §B — the offsets each side chose, for the probe block
FIELDS = [
    # (name, components, cpp_offset, hlsl_offset)
    ("float4x4 m", 16,  0,  0),
    ("float  a",    1, 64, 64),
    ("float3 b",    3, 68, 68),
    ("float  c",    1, 80, 80),
    ("float2 d",    2, 84, 84),
    ("float3 e",    3, 92, 96),
]
NAIVE_E = (242, 243, 0)
FIXED_E = (241, 242, 243)

# §E — best of seven runs of 256 pushes each
PUSH_COST = [
    ("108 bytes", 108, 0.015, 7.1),
    ("4 KB", 4096, 0.058, 70.3),
    ("16 KB", 16384, 0.259, 63.4),
]

SCENE_COVERED = 28687
SCENE_TOTAL = 147456


# =============================================================================
# Figure 1 — three rates of change
# =============================================================================
# ONE CLAIM: the camera is not per-vertex and not per-instance, so neither buffer
# can hold it without writing it thousands of times.
def fig1():
    W, H = 720, 366
    body = []

    body.append('<text x="24" y="22" class="sm">'
                'the same frame, three kinds of data, three rates of change</text>')

    rows = [
        ("per VERTEX", "slot 0", C_POS, "position, normal, uv",
         f"{TORUS_VERTS:,} &#215; 7 = 8,575 times a frame", 1.0),
        ("per INSTANCE", "slot 1", C_INST, "offset, scale, spin, tint",
         f"{INSTANCES} times a frame", 7 / 8575.0),
        ("per FRAME", "a push", C_UV, "the camera, the lamp",
         "<tspan class=\"t-hi\">once</tspan>", 1 / 8575.0),
    ]

    X0 = 168.0
    BAR = 342.0
    import math
    for i, (name, where, col, what, howoften, frac) in enumerate(rows):
        y = 56 + i * 78
        body.append(f'<text x="24" y="{y+16}" class="sm t-hi">{name}</text>')
        body.append(f'<text x="24" y="{y+30}" class="xs mono muted">{where}</text>')
        body.append(f'<text x="24" y="{y+44}" class="xs muted">{what}</text>')

        # A log-scaled bar, because the three counts span four orders of magnitude
        # and a linear bar would make two of them invisible.
        wpx = BAR * (1.0 + math.log10(max(frac, 1e-5))) / 5.0 * 5.0 / 5.0
        wpx = BAR * (math.log10(max(frac, 1e-5)) + 5.0) / 5.0
        body.append(f'<rect x="{f(X0)}" y="{y}" width="{f(max(wpx, 3.0))}" height="26" '
                    f'fill="{col}" fill-opacity="0.78" stroke="none"/>')
        body.append(f'<text x="{f(X0 + max(wpx, 3.0) + 10)}" y="{y+18}" class="xs mono">'
                    f'{howoften}</text>')

    body.append(f'<text x="{f(X0)}" y="{56+3*78+2}" class="xs muted">'
                'written per frame &#8212; logarithmic, because the three span four orders '
                'of magnitude</text>')

    ny = 296
    body.append(f'<rect x="24" y="{ny}" width="{W-48}" height="56" class="fill-soft ink" '
                'stroke-width="0.75" rx="3"/>')
    body.append(f'<text x="40" y="{ny+22}" class="sm">'
                'a camera in the vertex buffer would be the same sixteen floats, '
                f'<tspan class="t-hi">{TORUS_VERTS:,} times per instance</tspan></text>')
    body.append(f'<text x="40" y="{ny+42}" class="xs muted">'
                'and in the instance buffer, seven times &#8212; still wrong, because the '
                'camera is not a property of an instance</text>')

    return svg(W, H, "Three rates of change in one frame",
               "Vertex data is written 8,575 times a frame, instance data seven times, and "
               "the camera once. The vertex buffer solved the first two rates in Lesson 4.5. "
               "The third needs a different mechanism, because putting a view-projection "
               "matrix in either buffer would mean writing the same sixteen floats thousands "
               "of times.",
               "\n".join("  " + b for b in body), "f46a")


# =============================================================================
# Figure 2 — what a push actually is
# =============================================================================
def fig2():
    W, H = 720, 340
    body = []

    body.append('<text x="24" y="22" class="sm">'
                'there is no uniform buffer object &#8212; the data lives in the '
                '<tspan class="t-hi">command stream</tspan></text>')

    # The command buffer as a tape of recorded calls.
    X0, Y = 40.0, 58.0
    CW, CH = 128.0, 34.0
    items = [
        ("push A=201", C_UV, True),
        ("draw", C_POS, False),
        ("push A=77", C_UV, True),
        ("draw", C_POS, False),
        ("submit", C_HUB, False),
    ]
    for i, (label, col, is_push) in enumerate(items):
        x = X0 + i * (CW + 6)
        body.append(f'<rect x="{f(x)}" y="{Y}" width="{f(CW)}" height="{f(CH)}" '
                    f'fill="{col}" fill-opacity="{0.78 if is_push else 0.30}" '
                    'stroke="var(--dia-bg)" stroke-width="1"/>')
        cls = "t-inv" if is_push else ""
        body.append(f'<text x="{f(x+CW/2)}" y="{f(Y+22)}" class="xs mono {cls}" '
                    f'text-anchor="middle">{label}</text>')

    body.append(f'<text x="{f(X0)}" y="{f(Y-10)}" class="xs muted">'
                'one SDL_GPUCommandBuffer, recorded left to right</text>')

    # What each draw saw.
    for i, (idx, val) in enumerate([(1, "201"), (3, "77")]):
        x = X0 + idx * (CW + 6) + CW / 2
        body.append(f'<line x1="{f(x)}" y1="{f(Y+CH)}" x2="{f(x)}" y2="{f(Y+CH+26)}" '
                    'class="hi" stroke-width="1" stroke-dasharray="3 2"/>')
        body.append(f'<text x="{f(x)}" y="{f(Y+CH+40)}" class="xs mono t-hi" '
                    f'text-anchor="middle">reads {val}</text>')

    body.append(f'<text x="{f(X0)}" y="{f(Y+CH+66)}" class="xs muted">'
                'measured: nothing was bound, rebound or released between the two draws'
                '</text>')

    # The contrast.
    cy = 198
    body.append(f'<rect x="24" y="{cy}" width="{W-48}" height="128" class="fill-soft ink" '
                'stroke-width="0.75" rx="3"/>')
    body.append(f'<text x="40" y="{cy+20}" class="sm">'
                'SDL_GPUBufferUsageFlags, in full</text>')

    flags = ["VERTEX", "INDEX", "INDIRECT", "GRAPHICS_STORAGE_READ",
             "COMPUTE_STORAGE_READ", "COMPUTE_STORAGE_WRITE"]
    for i, fl in enumerate(flags):
        x = 40 + (i % 3) * 218
        y = cy + 42 + (i // 3) * 18
        body.append(f'<text x="{x}" y="{y}" class="xs mono muted">{fl}</text>')

    body.append(f'<text x="40" y="{cy+96}" class="sm t-hi">'
                'no UNIFORM bit, because there is no uniform buffer to create</text>')
    body.append(f'<text x="40" y="{cy+114}" class="xs muted">'
                'for data too large to push, the answer is a STORAGE buffer &#8212; a '
                'different resource, bound rather than pushed (Module 6)</text>')

    return svg(W, H, "A push is command-buffer state, not a resource",
               "Pushes and draws recorded into one command buffer, left to right. A push sets "
               "the uniform data that every draw recorded after it will read, so the second "
               "draw sees 77 where the first saw 201 — with nothing bound, rebound or released "
               "in between. The list of buffer usage flags below has no UNIFORM entry, because "
               "SDL_GPU has no uniform buffer object at all.",
               "\n".join("  " + b for b in body), "f46b")


# =============================================================================
# Figure 3 — the packing rule, byte by byte
# =============================================================================
# ONE CLAIM: C++ and HLSL agree for five of six fields and diverge on the sixth,
# and the divergence is silent.
def fig3():
    W, H = 720, 412
    body = []
    BW = 5.0          # pixels per byte
    X0 = 96.0
    SPAN = 112        # bytes drawn: registers 4, 5 and 6

    body.append('<text x="24" y="22" class="sm">'
                'bytes 64 to 111 of one uniform block &#8212; the matrix occupies 0 to 63 '
                'and both sides agree about it</text>')

    def register_grid(y, label):
        out = [f'<text x="24" y="{y+17}" class="sm">{label}</text>']
        for reg in range(4, 7):
            x = X0 + (reg * 16 - 64) * BW
            out.append(f'<rect x="{f(x)}" y="{f(y-8)}" width="{f(16*BW)}" height="42" '
                       'class="grid" fill="none" stroke-dasharray="3 3"/>')
            out.append(f'<text x="{f(x+8*BW)}" y="{f(y-12)}" class="xs muted" '
                       f'text-anchor="middle">register {reg}</text>')
        return out

    def field_boxes(y, which):
        out = []
        for name, comps, cpp, hlsl in FIELDS:
            if name.startswith("float4x4"):
                continue
            off = cpp if which == "cpp" else hlsl
            width = comps * 4 * BW
            x = X0 + (off - 64) * BW
            bad = (which == "cpp" and cpp != hlsl)
            col = C_BAD if bad else (C_UV if comps == 3 else C_POS if comps == 1 else C_NRM)
            out.append(f'<rect x="{f(x)}" y="{f(y)}" width="{f(width)}" height="26" '
                       f'fill="{col}" fill-opacity="0.8" stroke="var(--dia-bg)" '
                       'stroke-width="0.75"/>')
            out.append(f'<text x="{f(x+width/2)}" y="{f(y+17)}" class="xs mono t-inv" '
                       f'text-anchor="middle">{name.split()[-1]}</text>')
            out.append(f'<text x="{f(x)}" y="{f(y+38)}" class="xs muted">{off}</text>')
        return out

    y1 = 62
    body += register_grid(y1, "C++")
    body += field_boxes(y1, "cpp")

    y2 = 152
    body += register_grid(y2, "the shader")
    body += field_boxes(y2, "hlsl")

    # The divergence, marked.
    x_bad = X0 + (92 - 64) * BW
    x_ok = X0 + (96 - 64) * BW
    body.append(f'<line x1="{f(x_bad)}" y1="{f(y1+30)}" x2="{f(x_ok)}" y2="{f(y2-10)}" '
                'class="hi" stroke-width="1.4" marker-end="url(#e-h)"/>')
    body.append(f'<text x="{f(x_ok+10)}" y="{f((y1+y2)/2+16)}" class="xs t-hi">'
                'C++ says 92, the shader reads 96 &#8212; because 92&#8230;103 would '
                'STRADDLE the boundary at 96</text>')

    # The result.
    ry = 246
    body.append(f'<rect x="24" y="{ry}" width="{W-48}" height="150" class="fill-soft ink" '
                'stroke-width="0.75" rx="3"/>')
    body.append(f'<text x="40" y="{ry+20}" class="sm">'
                'what arrives, measured &#8212; wrote e = (241, 242, 243)</text>')

    for i, (label, vals, cls) in enumerate([
            ("the naive struct", NAIVE_E, "t-bad"),
            ("padded to 96", FIXED_E, "t-ok")]):
        yy = ry + 46 + i * 30
        body.append(f'<text x="40" y="{yy}" class="xs">{label}</text>')
        for j, v in enumerate(vals):
            body.append(f'<rect x="{200 + j*72}" y="{yy-15}" width="64" height="21" '
                        f'class="fill-soft ink" stroke-width="0.75" rx="2"/>')
            body.append(f'<text x="{200 + j*72 + 32}" y="{yy}" class="xs mono {cls}" '
                        f'text-anchor="middle">{v}</text>')

    body.append(f'<text x="40" y="{ry+118}" class="xs t-hi">'
                'shifted by one float, with a zero on the end &#8212; and every field '
                'BEFORE it arrived correctly</text>')
    body.append(f'<text x="40" y="{ry+136}" class="xs muted">'
                'no error, no warning, no validation message: a uniform layout bug corrupts '
                'the TAIL of a block</text>')

    return svg(W, H, "Where C++ puts a field and where the shader reads it",
               "Bytes 64 to 111 of one uniform block, with the 16-byte register boundaries "
               "drawn. C++ and the shader agree about the first four fields and diverge on the "
               "last: C++ places the trailing float3 at byte 92, and the compiler moved it to "
               "96 because 92 to 103 would straddle the boundary. Measured, the naive struct's "
               "float3 arrives as (242, 243, 0) — shifted by one float, with a zero on the end "
               "— while every field before it arrives intact.",
               "\n".join("  " + b for b in body), "f46c")


# =============================================================================
# Figure 4 — sixteen floats through the boundary
# =============================================================================
def fig4():
    W, H = 720, 408
    body = []

    body.append('<text x="24" y="22" class="sm">'
                'a claim this course has carried since Lesson 2.6, finally checked</text>')

    # Left: our storage order.
    body.append('<text x="24" y="52" class="xs muted">'
                'engine::mat4 in memory &#8212; four columns, contiguous</text>')
    CW = 26.0
    for i in range(16):
        x = 24 + i * CW
        col = i // 4
        body.append(f'<rect x="{f(x)}" y="60" width="{f(CW)}" height="24" '
                    f'fill="{[C_POS, C_NRM, C_UV, C_INST][col]}" fill-opacity="0.65" '
                    'stroke="var(--dia-bg)" stroke-width="0.75"/>')
    for col in range(4):
        body.append(f'<text x="{f(24 + col*4*CW + 2*CW)}" y="98" class="xs mono muted" '
                    f'text-anchor="middle">c{col}</text>')
    body.append(f'<text x="24" y="118" class="xs t-hi">'
                'memcpy &#8212; and that is the whole conversion</text>')

    # Right: the readback table.
    TX, TY = 452.0, 52.0
    body.append(f'<text x="{f(TX)}" y="{f(TY)}" class="xs muted">'
                'what the shader reports for m[row][col]</text>')
    CS = 44.0
    for col in range(4):
        body.append(f'<text x="{f(TX + 34 + col*CS + CS/2)}" y="{f(TY+18)}" '
                    f'class="xs muted" text-anchor="middle">{col}</text>')
    for row in range(4):
        yy = TY + 26 + row * 28
        body.append(f'<text x="{f(TX)}" y="{f(yy+16)}" class="xs muted">row {row}</text>')
        for col in range(4):
            v = 16 * row + col + 1
            x = TX + 34 + col * CS
            body.append(f'<rect x="{f(x)}" y="{f(yy)}" width="{f(CS-3)}" height="24" '
                        'class="fill-soft ink" stroke-width="0.75" rx="2"/>')
            body.append(f'<text x="{f(x+(CS-3)/2)}" y="{f(yy+17)}" class="xs mono t-ok" '
                        f'text-anchor="middle">{v}</text>')
    body.append(f'<text x="{f(TX)}" y="{f(TY+150)}" class="xs">'
                'we wrote 16&#215;row + col + 1</text>')

    # The verdict, and the trap.
    vy = 206
    body.append(f'<rect x="24" y="{vy}" width="{W-48}" height="60" class="fill-soft ink" '
                'stroke-width="0.75" rx="3"/>')
    body.append(f'<text x="40" y="{vy+24}" class="sm t-ok">'
                'every element landed where our storage put it. No transpose, anywhere.'
                '</text>')
    body.append(f'<text x="40" y="{vy+44}" class="xs muted">'
                'so the column-major choice Lesson 2.6 made for its own reasons is also the '
                'one this boundary wanted</text>')

    ty = 284
    body.append(f'<rect x="24" y="{ty}" width="{W-48}" height="108" class="fill-soft ink" '
                'stroke-width="0.75" rx="3"/>')
    body.append(f'<text x="40" y="{ty+22}" class="sm t-bad">'
                '&#9888; and do not believe the intermediate</text>')
    body.append(f'<text x="40" y="{ty+44}" class="xs mono">'
                'spirv-dis build/shaders/mesh.vert.spv | grep RowMajor</text>')
    body.append(f'<text x="40" y="{ty+62}" class="xs mono muted">'
                'OpMemberDecorate %Camera 0 RowMajor</text>')
    body.append(f'<text x="40" y="{ty+86}" class="xs">'
                'which looks exactly like the transpose the table above proves is not '
                'happening. It is an artefact of how DXC names things on the way to SPIR-V.'
                '</text>')

    return svg(W, H, "Sixteen floats through the uniform boundary",
               "Our mat4 stores four columns contiguously. Pushing a matrix whose element at "
               "written (row, col) is 16·row + col + 1 and asking the shader to report "
               "m[row][col] returns exactly that table: every element landed where our storage "
               "put it, with no transpose. The compiled SPIR-V decorates the member RowMajor, "
               "which looks like the opposite and is an artefact of the toolchain's naming "
               "rather than a statement about the bytes.",
               "\n".join("  " + b for b in body), "f46d")


# =============================================================================
# Figure 5 — the register space, and which tool sees a mistake
# =============================================================================
def fig5():
    W, H = 720, 356
    body = []

    body.append('<text x="24" y="22" class="sm">'
                'the register space is fixed by SDL, per stage &#8212; and it is '
                '<tspan class="t-hi">not a choice</tspan></text>')

    # The two stages and their four spaces.
    rows = [
        ("vertex", "space0", "textures, samplers, storage buffers", C_POS,
         "space1", "uniform buffers", C_UV),
        ("fragment", "space2", "textures, samplers, storage buffers", C_POS,
         "space3", "uniform buffers", C_UV),
    ]
    for i, (stage, sa, wa, ca, sb, wb, cb_) in enumerate(rows):
        y = 54 + i * 62
        body.append(f'<text x="24" y="{y+24}" class="sm">{stage}</text>')
        for j, (sp, what, col) in enumerate([(sa, wa, ca), (sb, wb, cb_)]):
            x = 120 + j * 300
            body.append(f'<rect x="{x}" y="{y}" width="286" height="40" fill="{col}" '
                        'fill-opacity="0.16" stroke="none" rx="3"/>')
            body.append(f'<text x="{x+12}" y="{y+18}" class="xs mono t-hi">{sp}</text>')
            body.append(f'<text x="{x+12}" y="{y+33}" class="xs muted">{what}</text>')

    body.append(f'<text x="120" y="192" class="xs muted">'
                'our mesh.vert declares b0 in space1; mesh.frag declares b0 in space3. '
                'verify_46 &#167;D reads both back out of the compiled SPIR-V.</text>')

    # Who catches a wrong one.
    cy = 212
    body.append(f'<rect x="24" y="{cy}" width="{W-48}" height="132" class="fill-soft ink" '
                'stroke-width="0.75" rx="3"/>')
    body.append(f'<text x="40" y="{cy+20}" class="sm">'
                'put the cbuffer in the wrong space and ask each tool in turn</text>')

    checks = [
        ("glslc: HLSL &#8594; SPIR-V", "accepted", False),
        ("the JSON reflection", "BYTE-IDENTICAL to the correct shader", False),
        ("spirv-dis: DescriptorSet", "0 instead of 3 &#8212; visible", True),
        ("shadercross: SPIR-V &#8594; MSL", "REFUSED, with an exact message", True),
    ]
    for i, (who, what, good) in enumerate(checks):
        yy = cy + 46 + i * 20
        body.append(f'<text x="40" y="{yy}" class="xs mono">{who}</text>')
        body.append(f'<text x="300" y="{yy}" class="xs {"t-ok" if good else "t-bad"}">'
                    f'{what}</text>')

    body.append(f'<text x="40" y="{cy+124}" class="xs t-hi">'
                'so the BUILD catches it on the way to Metal and D3D &#8212; and the '
                'reflection, which saved Lesson 4.5, never can</text>')

    return svg(W, H, "Register spaces, and which tool notices a wrong one",
               "SDL fixes the register space per stage: vertex textures in space0 and vertex "
               "uniform buffers in space1; fragment textures in space2 and fragment uniform "
               "buffers in space3. Putting a cbuffer in the wrong space compiles to SPIR-V "
               "without complaint and produces a byte-identical JSON reflection, so the "
               "cross-check that saved Lesson 4.5 cannot see it. The disassembler shows it, and "
               "the translation to MSL refuses it outright.",
               "\n".join("  " + b for b in body), "f46e")


# =============================================================================
# Figure 6 — the scene, from the actual pixels
# =============================================================================
def fig6():
    W, H = 720, 516
    body = []
    CROP = (72, 66, 440, 278)     # 368 x 212 — the scene's bounding box plus a margin
    CELL = 3
    PX = 5

    body.append('<text x="24" y="22" class="sm">'
                'the same seven tori as Lesson 4.5, seen from a camera that can '
                '<tspan class="t-hi">move</tspan></text>')

    gw, gh, rects = panel("scratch/l46_scene.ppm", CROP, CELL, 30, 38, PX,
                          [hexrgb(c) for c in C_TINT], 4)
    body += rects
    body.append(f'<rect x="30" y="38" width="{gw*PX}" height="{gh*PX}" class="ink" '
                'fill="none" stroke-width="0.75"/>')

    y = 38 + gh * PX + 20
    body.append(f'<text x="30" y="{y}" class="xs muted">'
                'rendered offscreen at 512&#215;288 and downloaded, then quantised to four '
                'shades of each tint</text>')

    y += 26
    facts = [
        ("camera", f"{CAMERA_BYTES} bytes, pushed to vertex slot 0"),
        ("lighting", f"{LIGHT_BYTES} bytes, pushed to fragment slot 0"),
        ("instances", f"{INSTANCE_BYTES} bytes, a vertex buffer at slot 1"),
        ("geometry", "53,024 bytes, uploaded once in Lesson 4.5"),
    ]
    for i, (k, v) in enumerate(facts):
        yy = y + i * 17
        body.append(f'<text x="30" y="{yy}" class="xs mono t-hi">{k}</text>')
        body.append(f'<text x="150" y="{yy}" class="xs mono">{v}</text>')

    body.append(f'<text x="30" y="{y + 4*17 + 12}" class="xs muted">'
                f'{PER_FRAME_TOTAL} bytes move per frame; the ambient term is tinted by a '
                'sky colour, which is why the shadowed sides are blue rather than grey'
                '</text>')

    return svg(W, H, "The scene, with the camera in a uniform buffer",
               "The frame rendered offscreen and downloaded so it can be counted. The geometry "
               "has not changed since Lesson 4.5 and has not moved on the device; what changed "
               "is that 64 bytes of camera and 32 of lighting now arrive each frame, so the "
               "view is something the program decides rather than something the shader was "
               "compiled with. The shadowed sides are blue because the ambient term is tinted "
               "by a sky colour, which is one multiply and the seed of Module 6's image-based "
               "lighting.",
               "\n".join("  " + b for b in body), "f46f")


if __name__ == "__main__":
    write("l46_fig1.svg", fig1())
    write("l46_fig2.svg", fig2())
    write("l46_fig3.svg", fig3())
    write("l46_fig4.svg", fig4())
    write("l46_fig5.svg", fig5())
    write("l46_fig6.svg", fig6())
