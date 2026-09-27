#!/usr/bin/env python3
"""Compute every SVG figure for Lesson 4.9 from real data.

Every number in every caption comes from scratch/verify_49.log, and the event
tree in Figure 3 is the actual output of `./build/engine --trace`.

Writes scratch/l49_fig{1..6}.svg.
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_45 import (svg, write, f, C_POS, C_NRM, C_UV, C_INST, C_BAD, C_HUB)

# ---- Measured inputs, from scratch/verify_49.log ----------------------------

NAME_PLAIN = 0.0011      # ms, median of 200, unnamed buffer creation
NAME_NAMED = 0.0018      # ms, named at creation

VALIDATION_ON = 0.0267   # ms to RECORD one 3-draw frame, debug mode on
VALIDATION_OFF = 0.0228  # …off
VALIDATION_RATIO = 1.17

NOISE = 0.00246          # ms, spread of five identical runs
GROUP_ROWS = [           # (count, ms, delta, per-pair ns or None)
    (0,   0.02400, None,     None),
    (1,   0.02342, -0.00058, None),
    (8,   0.02492,  0.00092, None),
    (32,  0.02987,  0.00587, 183.6),
    (128, 0.04729,  0.02329, 182.0),
]
GROUPS_PER_FRAME = 4
BUDGET_MS = 16.7

CACHE = [                # (size, invocations, hit rate %)
    (0,  6912, 0.0),
    (4,  4608, 33.3),
    (6,  2400, 65.3),
    (8,  2400, 65.3),
    (16, 2400, 65.3),
    (32, 2400, 65.3),
    (48, 2306, 66.6),
    (50, 1226, 82.3),
    (52, 1225, 82.3),
    (64, 1225, 82.3),
]
CACHE_BEST = 1225
CACHE_WORST = 6912
SHUFFLED_32 = 6784
ORDERED_32 = 2400
SHUFFLE_RATIO = 2.83

FRAME_EVENTS = 33
FRAME_DRAWS = 3
FRAME_UNIFORM_BYTES = 560


# =============================================================================
# Figure 1 — why the debugger you already have cannot help
# =============================================================================
def fig1():
    W, H = 720, 392
    body = []

    body.append('<text x="24" y="22" class="sm">'
                'the call that was wrong, and the moment the picture is wrong &#8212; '
                '<tspan class="t-hi">separated by a submit</tspan></text>')

    # One row per lane, and everything in a row sits inside that row: the label
    # on its own line above the bar, the bar below it, the note to the right.
    # The first draft right-aligned the labels into the bars' own column and the
    # two drew on top of each other.
    LANES = [
        ("your code, on the CPU", C_POS, 40, 250, "record",
         "every call returns success"),
        ("the command buffer", C_HUB, 40, 250, "a list of work, not work",
         "nothing has executed yet"),
        ("the GPU", C_UV, 330, 210, "execute",
         "…here is where it goes wrong"),
        ("the display", C_INST, 560, 130, "scan out",
         "and here is where you see it"),
    ]

    Y0 = 52
    RH = 70
    for i, (label, colour, x, w, text, note) in enumerate(LANES):
        y = Y0 + i * RH
        body.append(f'<text x="24" y="{y}" class="xs muted">{label}</text>')
        body.append(f'<line x1="24" y1="{y+34}" x2="{W-24}" y2="{y+34}" '
                    'class="grid" stroke-width="0.75"/>')
        body.append(f'<rect x="{x}" y="{y+10}" width="{w}" height="24" '
                    f'fill="{colour}" fill-opacity="0.28" stroke="{colour}" '
                    'stroke-width="1"/>')
        body.append(f'<text x="{x+w/2}" y="{y+26}" class="xs mono" '
                    f'text-anchor="middle">{text}</text>')
        body.append(f'<text x="{W-24}" y="{y+50}" class="xs muted" '
                    f'text-anchor="end">{note}</text>')

    # The submit, as the wall between the two halves.
    yend = Y0 + len(LANES) * RH
    body.append(f'<line x1="315" y1="{Y0+4}" x2="315" y2="{yend-24}" class="hi" '
                'stroke-width="1.6" stroke-dasharray="4 3"/>')
    body.append(f'<text x="322" y="{yend-10}" class="xs t-hi">'
                'SDL_SubmitGPUCommandBuffer</text>')

    body.append(f'<text x="24" y="{H-30}" class="xs">'
                'A breakpoint lives entirely in the top lane, where every call has already '
                'returned success.</text>')
    body.append(f'<text x="24" y="{H-12}" class="xs muted">'
                'By the time a pixel is wrong, the stack that produced it has been gone for '
                'two lanes.</text>')

    return svg(W, H, "Why a CPU debugger cannot debug a frame",
               "Four lanes on one timeline: your code recording calls, the command buffer "
               "holding them, the GPU executing them later, and the display showing the result. "
               "A breakpoint can only stop in the first lane, where every call has already "
               "returned success. The submit is the wall between the code and the consequence.",
               "\n".join("  " + b for b in body), "f49a")


# =============================================================================
# Figure 2 — three tools, and which one is yours
# =============================================================================
def fig2():
    W, H = 720, 330
    body = []

    body.append('<text x="24" y="22" class="sm">'
                'the tool depends on the backend, and '
                '<tspan class="t-bad">the backend depends on your platform</tspan></text>')

    TOOLS = [
        ("RenderDoc", C_NRM,
         ["Vulkan, D3D11, D3D12,", "OpenGL, OpenGL ES"],
         ["Windows, Linux,", "Android, Switch"],
         "free, open source, the one everybody means"),
        ("Xcode Metal Debugger", C_UV,
         ["Metal"],
         ["macOS, iOS"],
         "what SDL_gpu.h tells Mac readers to use"),
        ("PIX", C_POS,
         ["D3D12"],
         ["Windows"],
         "Microsoft's; strongest on GPU timing"),
    ]

    CW = 224
    for i, (name, colour, apis, plats, note) in enumerate(TOOLS):
        x = 24 + i * (CW + 8)
        body.append(f'<rect x="{x}" y="42" width="{CW}" height="180" '
                    'class="fill-soft ink" stroke-width="0.75"/>')
        body.append(f'<rect x="{x}" y="42" width="{CW}" height="5" fill="{colour}"/>')
        body.append(f'<text x="{x+14}" y="{70}" class="sm mono">{name}</text>')

        body.append(f'<text x="{x+14}" y="{96}" class="xs muted">captures</text>')
        for j, a in enumerate(apis):
            body.append(f'<text x="{x+14}" y="{112+j*15}" class="xs mono">{a}</text>')

        body.append(f'<text x="{x+14}" y="{152}" class="xs muted">on</text>')
        for j, pl in enumerate(plats):
            body.append(f'<text x="{x+14}" y="{168+j*15}" class="xs mono">{pl}</text>')

        body.append(f'<text x="{x+14}" y="{210}" class="xs muted">{note}</text>')

    body.append('<text x="24" y="252" class="xs">'
                '<tspan class="t-bad">RenderDoc does not support Metal</tspan>, and it is not '
                'an oversight to wait out &#8212; the list above is quoted from its own front '
                'page. If you are on a Mac,</text>')
    body.append('<text x="24" y="268" class="xs">'
                'SDL_GPU gives you Metal and the tool is Xcode. '
                '<tspan class="t-hi">Every concept in this lesson is identical</tspan>; the screenshots '
                'are not.</text>')
    body.append('<text x="24" y="294" class="xs muted">'
                'SDL_gpu.h has a "Debugging" section that says exactly this, including the '
                'menu path for the Xcode capture. It is the first place to look, and it is</text>')
    body.append('<text x="24" y="310" class="xs muted">'
                'already on your disk.</text>')

    return svg(W, H, "Three frame debuggers and what each one captures",
               "RenderDoc captures Vulkan, D3D11, D3D12, OpenGL and OpenGL ES on Windows, "
               "Linux, Android and Switch. Xcode's Metal Debugger captures Metal on macOS and "
               "iOS. PIX captures D3D12 on Windows. RenderDoc has no Metal support, so macOS "
               "readers use Xcode — which is what SDL_gpu.h's own debugging section says.",
               "\n".join("  " + b for b in body), "f49b")


# =============================================================================
# Figure 3 — the frame, as an event tree
# =============================================================================
def fig3():
    W, H = 720, 470
    body = []

    body.append('<text x="24" y="22" class="sm">'
                'one frame of <tspan class="mono">engine --trace</tspan>, and what a capture '
                'calls each part</text>')

    # (depth, kind, name, detail, class)
    ROWS = [
        (0, "group", "upload", "", "hi"),
        (1, "copy", "framebuffer -> texture", "230400 bytes", ""),
        (0, "group", "clear", "", "hi"),
        (1, "pass", "clear (no draws)", "", ""),
        (0, "group", "blit (software picture)", "", "hi"),
        (1, "blit", "framebuffer -> swapchain", "921600 bytes", ""),
        (0, "group", "scene", "", "hi"),
        (1, "pass", "colour + depth", "", ""),
        (1, "uniform", "camera (vertex)", "slot 0, 64 bytes", "muted"),
        (1, "uniform", "lighting (fragment)", "slot 0, 64 bytes", "muted"),
        (1, "pipeline", "solid (cull back)", "", ""),
        (1, "sampler", "white 1x1", "slot 0", "muted"),
        (1, "uniform", "object matrices", "slot 1, 112 bytes", "muted"),
        (1, "uniform", "material", "slot 1, 32 bytes", "muted"),
        (1, "vertex buf", "mesh vertices", "slot 0", "muted"),
        (1, "index buf", "mesh indices", "slot 0", "muted"),
        (1, "DRAW", "scene object", "60 indices, 20 triangles", "bad"),
        (1, "…", "(the same nine events, twice more)", "", "muted"),
        (1, "DRAW", "scene object", "36 indices, 12 triangles", "bad"),
        (1, "DRAW", "scene object", "36 indices, 12 triangles", "bad"),
    ]

    Y0 = 48
    RH = 17
    for i, (d, kind, name, detail, cls) in enumerate(ROWS):
        y = Y0 + i * RH
        if cls == "hi":
            body.append(f'<rect x="24" y="{y-12}" width="400" height="{RH}" '
                        f'fill="{C_HUB}" fill-opacity="0.14" stroke="none"/>')
        elif cls == "bad":
            body.append(f'<rect x="24" y="{y-12}" width="400" height="{RH}" '
                        f'fill="{C_BAD}" fill-opacity="0.14" stroke="none"/>')
        tc = "xs mono muted" if cls == "muted" else "xs mono"
        body.append(f'<text x="{30 + d*16}" y="{y}" class="{tc}">{kind}</text>')
        body.append(f'<text x="{118 + d*16}" y="{y}" class="{tc}">{name}</text>')
        if detail:
            body.append(f'<text x="418" y="{y}" class="xs mono muted" '
                        f'text-anchor="end">{detail}</text>')

    # Annotations on the right. Each block is (top y, class, heading, lines) and
    # every line gets its own y — the first draft emitted headings at y and y+16
    # while the following line also claimed y+16, and the two drew on top of each
    # other. A layout table with one row per line cannot do that.
    RX = 448
    NOTES = [
        (56, "t-hi", "DEBUG GROUPS",
         ["collapsible headings in a capture;",
          "four per frame, and the reason the",
          "tree reads instead of scrolls"]),
        (136, "", "RENDER PASSES",
         ["a capture's outline is passes;",
          "everything hangs off one"]),
        (200, "", "BINDS AND PUSHES",
         ["RenderDoc's Pipeline State panel;",
          "the bytes of every push are",
          "inspectable"]),
        (280, "t-bad", "DRAW CALLS",
         ["the events you actually stop at.",
          "Select one and the tool replays the",
          "frame UP TO IT, so every buffer and",
          "attachment is shown as it was then."]),
    ]
    for y, cls, head, lines in NOTES:
        body.append(f'<text x="{RX}" y="{y}" class="xs {cls}">{head}</text>')
        for j, line in enumerate(lines):
            body.append(f'<text x="{RX}" y="{y + 18 + j * 15}" class="xs muted">{line}</text>')

    body.append(f'<text x="24" y="{H-34}" class="xs">'
                f'<tspan class="t-hi">{FRAME_EVENTS} events, {FRAME_DRAWS} draws, '
                f'{FRAME_UNIFORM_BYTES} uniform bytes</tspan> &#8212; and every one of those '
                'numbers was already in Lesson 4.8&#8217;s log before any tool was opened.</text>')
    body.append(f'<text x="24" y="{H-16}" class="xs muted">'
                'Which is the point of the ordering: you are checking a tool against a frame '
                'you already understand, not the other way round.</text>')

    return svg(W, H, "One frame, as an event tree",
               "The output of engine --trace: four debug groups (upload, clear, blit, scene), "
               "a render pass, the per-frame and per-draw uniform pushes, the binds, and three "
               "draw calls. The annotations name what each part is called in a capture. 33 "
               "events, 3 draws, 560 uniform bytes — all numbers Lesson 4.8 already reported.",
               "\n".join("  " + b for b in body), "f49c")


# =============================================================================
# Figure 4 — what instrumentation costs, measured properly
# =============================================================================
def fig4():
    W, H = 720, 400
    body = []

    body.append('<text x="24" y="22" class="sm">'
                'the cost of being able to debug &#8212; '
                '<tspan class="t-hi">and the noise floor it has to clear first</tspan></text>')

    X0, Y0 = 40.0, 60.0
    PW, PH = 380.0, 200.0
    body.append(f'<rect x="{f(X0)}" y="{f(Y0)}" width="{f(PW)}" height="{f(PH)}" '
                'class="fill-soft ink" stroke-width="0.75"/>')

    lo, hi = 0.020, 0.050
    def py(v): return Y0 + PH * (1.0 - (v - lo) / (hi - lo))
    def px(i): return X0 + 40.0 + i * 76.0

    for v in (0.02, 0.03, 0.04, 0.05):
        body.append(f'<line x1="{f(X0)}" y1="{f(py(v))}" x2="{f(X0+PW)}" y2="{f(py(v))}" '
                    'class="grid" stroke-width="0.75"/>')
        body.append(f'<text x="{f(X0-8)}" y="{f(py(v)+4)}" class="xs muted" '
                    f'text-anchor="end">{v:.2f}</text>')

    base = GROUP_ROWS[0][1]
    # The noise band around the baseline: anything inside it is not a result.
    body.append(f'<rect x="{f(X0)}" y="{f(py(base + NOISE))}" width="{f(PW)}" '
                f'height="{f(py(base - NOISE) - py(base + NOISE))}" '
                f'fill="{C_BAD}" fill-opacity="0.13" stroke="none"/>')
    body.append(f'<text x="{f(X0+PW-8)}" y="{f(py(base + NOISE)-6)}" class="xs t-bad" '
                f'text-anchor="end">&#177; the noise floor ({NOISE*1000:.2f} &#181;s)</text>')

    pts = []
    for i, (n, msv, delta, per) in enumerate(GROUP_ROWS):
        x, y = px(i), py(msv)
        pts.append(f"{f(x)},{f(y)}")
        colour = C_HUB if per is None else C_UV
        body.append(f'<circle cx="{f(x)}" cy="{f(y)}" r="4" fill="{colour}"/>')
        body.append(f'<text x="{f(x)}" y="{f(Y0+PH+14)}" class="xs mono muted" '
                    f'text-anchor="middle">{n}</text>')
        if per is not None:
            # Left of the point, not above it: the curve rises steeply to the
            # right here and a centred label lands on top of it.
            body.append(f'<text x="{f(x-10)}" y="{f(y-6)}" class="xs t-hi" '
                        f'text-anchor="end">{per:.0f} ns</text>')
    body.append(f'<polyline points="{" ".join(pts)}" class="hi" fill="none" stroke-width="1.5"/>')

    body.append(f'<text x="{f(X0+PW/2)}" y="{f(Y0+PH+30)}" class="xs muted" '
                'text-anchor="middle">push + label + pop, per frame</text>')
    body.append(f'<text x="{f(X0-30)}" y="{f(Y0-10)}" class="xs muted">ms to record</text>')

    # The other two measurements, as a small table.
    RX = 452
    body.append(f'<text x="{RX}" y="62" class="xs muted">the validation layer</text>')
    body.append(f'<text x="{RX}" y="82" class="xs mono">debug on   {VALIDATION_ON:.4f} ms</text>')
    body.append(f'<text x="{RX}" y="98" class="xs mono">debug off  {VALIDATION_OFF:.4f} ms</text>')
    body.append(f'<text x="{RX}" y="116" class="xs t-hi">{VALIDATION_RATIO:.2f}x, on a '
                '3-draw frame</text>')
    body.append(f'<text x="{RX}" y="136" class="xs muted">per API CALL, so it scales</text>')
    body.append(f'<text x="{RX}" y="152" class="xs muted">with how chatty the frame is</text>')

    body.append(f'<text x="{RX}" y="188" class="xs muted">naming a buffer</text>')
    body.append(f'<text x="{RX}" y="208" class="xs mono">unnamed  {NAME_PLAIN:.4f} ms</text>')
    body.append(f'<text x="{RX}" y="224" class="xs mono">named    {NAME_NAMED:.4f} ms</text>')
    body.append(f'<text x="{RX}" y="242" class="xs t-ok">paid once, at load</text>')

    body.append(f'<text x="24" y="{H-56}" class="xs">'
                f'At one group per frame the effect is <tspan class="t-hi">below the noise floor</tspan>, '
                'and the first version of this measurement reported a '
                '<tspan class="t-bad">negative cost for adding work</tspan> &#8212;</text>')
    body.append(f'<text x="24" y="{H-40}" class="xs">'
                'which is the tell. Scale the count until the effect clears the floor and a '
                f'real per-call figure appears: <tspan class="t-hi">~{GROUP_ROWS[-1][3]:.0f} ns'
                '</tspan>, converging.</text>')
    body.append(f'<text x="24" y="{H-16}" class="xs muted">'
                f'The engine pushes {GROUPS_PER_FRAME} a frame. That is '
                f'{GROUPS_PER_FRAME*GROUP_ROWS[-1][3]/1000.0:.2f} &#181;s against a '
                f'{BUDGET_MS} ms budget &#8212; '
                f'{100.0*GROUPS_PER_FRAME*GROUP_ROWS[-1][3]/1e6/BUDGET_MS:.3f}% of a frame. '
                'They ship on.</text>')

    return svg(W, H, "What instrumentation costs, measured against its noise floor",
               "Recording time against the number of debug groups per frame. At one and eight "
               "groups the difference is inside the noise band and is not a result; at 32 and "
               "128 it clears the floor and converges on about 183 nanoseconds per "
               "push-label-pop. The validation layer costs 1.17x on a three-draw frame, and "
               "naming a buffer costs 0.7 microseconds once, at load.",
               "\n".join("  " + b for b in body), "f49d")


# =============================================================================
# Figure 5 — vertex reuse, predicted rather than measured
# =============================================================================
def fig5():
    W, H = 720, 424
    body = []

    body.append('<text x="24" y="22" class="sm">'
                'vertex reuse against cache size &#8212; '
                '<tspan class="t-hi">a staircase, not a slope</tspan></text>')

    X0, Y0 = 56.0, 50.0
    PW, PH = 400.0, 210.0
    body.append(f'<rect x="{f(X0)}" y="{f(Y0)}" width="{f(PW)}" height="{f(PH)}" '
                'class="fill-soft ink" stroke-width="0.75"/>')

    def py(v): return Y0 + PH * (1.0 - (v - 1000.0) / (7200.0 - 1000.0))
    def px(i): return X0 + 22.0 + i * ((PW - 44.0) / (len(CACHE) - 1))

    for v in (1225, 2400, 4608, 6912):
        body.append(f'<line x1="{f(X0)}" y1="{f(py(v))}" x2="{f(X0+PW)}" y2="{f(py(v))}" '
                    'class="grid" stroke-width="0.75"/>')
        body.append(f'<text x="{f(X0-8)}" y="{f(py(v)+4)}" class="xs muted" '
                    f'text-anchor="end">{v}</text>')

    body.append(f'<line x1="{f(X0)}" y1="{f(py(CACHE_BEST))}" x2="{f(X0+PW)}" '
                f'y2="{f(py(CACHE_BEST))}" stroke="{C_NRM}" stroke-width="1.2" '
                'stroke-dasharray="4 3"/>')
    body.append(f'<text x="{f(X0+6)}" y="{f(py(CACHE_BEST)-6)}" class="xs t-ok">'
                'best case: one invocation per vertex</text>')

    pts = []
    for i, (size, inv, hit) in enumerate(CACHE):
        x, y = px(i), py(inv)
        pts.append(f"{f(x)},{f(y)}")
        body.append(f'<circle cx="{f(x)}" cy="{f(y)}" r="3" class="hi-fill"/>')
        body.append(f'<text x="{f(x)}" y="{f(Y0+PH+14)}" class="xs mono muted" '
                    f'text-anchor="middle">{size}</text>')
    body.append(f'<polyline points="{" ".join(pts)}" class="hi" fill="none" stroke-width="1.6"/>')

    body.append(f'<text x="{f(X0+PW/2)}" y="{f(Y0+PH+30)}" class="xs muted" '
                'text-anchor="middle">post-transform cache size, vertices</text>')
    body.append(f'<text x="{f(X0-34)}" y="{f(Y0-10)}" class="xs muted">'
                'vertex-shader invocations</text>')

    # The two risers, annotated.
    body.append(f'<text x="{f(px(4))}" y="{f(py(2400)+18)}" class="xs t-hi" '
                'text-anchor="middle">reuse from the neighbouring quad</text>')
    body.append(f'<text x="{f(X0+PW-6)}" y="{f(py(4000))}" class="xs t-hi" '
                'text-anchor="end">and only here, the quad one ring away</text>')

    body.append(f'<text x="24" y="{H-100}" class="xs">'
                'A cache of 8 and a cache of 32 catch <tspan class="t-hi">exactly the same '
                'reuse</tspan>, because nothing in this mesh is reused at a distance between '
                'them.</text>')
    body.append(f'<text x="24" y="{H-76}" class="xs">'
                '<tspan class="t-bad">THE CONTROL:</tspan> the same 2,304 triangles with the '
                f'ORDER shuffled &#8212; {ORDERED_32:,} invocations becomes {SHUFFLED_32:,}, '
                f'<tspan class="t-bad">{SHUFFLE_RATIO}&#215; worse</tspan>.</text>')
    body.append(f'<text x="24" y="{H-56}" class="xs muted">'
                'Nothing about the geometry changed. Reuse is a property of the index ORDER, '
                'which is why real pipelines run an index optimiser as a build step.</text>')
    body.append(f'<text x="24" y="{H-24}" class="xs muted">'
                'And this whole figure is a MODEL, not a measurement &#8212; the true number '
                'needs a tool this machine cannot run. &#167;3.6 says exactly where to read '
                'it.</text>')

    return svg(W, H, "Vertex reuse against cache size, simulated",
               "A simulated FIFO post-transform vertex cache over torus.obj's own index order. "
               "The curve is a staircase: caches from 6 to 32 all give 2,400 invocations, and "
               "only at about 50 does the second kind of reuse start landing. Shuffling the "
               "triangle order while keeping the geometry identical costs 2.83x. This is a "
               "model with stated assumptions, not a measurement.",
               "\n".join("  " + b for b in body), "f49e")


# =============================================================================
# Figure 6 — what each instrument can and cannot tell you
# =============================================================================
def fig6():
    W, H = 720, 356
    body = []

    body.append('<text x="24" y="22" class="sm">'
                'two instruments, and the questions each one cannot answer</text>')

    ROWS = [
        ("the event tree: what happened, in order",        True,  True),
        ("resource NAMES, so the tree is readable",        True,  True),
        ("how many bytes each uniform push moved",         True,  True),
        ("a cross-check against a second counter",         True,  False),
        ("runs in CI, with no GUI and no driver hooks",    True,  False),
        ("runs on every backend, including Metal",         True,  False),
        ("the CONTENTS of a buffer at a given event",      False, True),
        ("the framebuffer replayed up to one draw",        False, True),
        ("which pipeline state was live at a draw",        False, True),
        ("the compiled shader, disassembled",              False, True),
        ("per-draw GPU timing",                            False, True),
        ("step a single fragment through the shader",      False, True),
    ]

    Y0 = 62
    RH = 21
    body.append(f'<text x="30" y="{Y0-12}" class="xs muted">question</text>')
    body.append(f'<text x="470" y="{Y0-12}" class="xs muted" text-anchor="middle">'
                'our frame log</text>')
    body.append(f'<text x="600" y="{Y0-12}" class="xs muted" text-anchor="middle">'
                'a real capture</text>')

    for i, (q, log_ok, cap_ok) in enumerate(ROWS):
        y = Y0 + i * RH
        if i % 2 == 0:
            body.append(f'<rect x="24" y="{y-14}" width="{W-48}" height="{RH}" '
                        'class="fill-soft" stroke="none"/>')
        body.append(f'<text x="30" y="{y}" class="xs">{q}</text>')
        for x, ok in ((470, log_ok), (600, cap_ok)):
            colour = C_NRM if ok else C_BAD
            mark = "yes" if ok else "no"
            body.append(f'<rect x="{x-16}" y="{y-11}" width="32" height="14" '
                        f'fill="{colour}" fill-opacity="{0.75 if ok else 0.55}"/>')
            body.append(f'<text x="{x}" y="{y}" class="xs t-inv" '
                        f'text-anchor="middle">{mark}</text>')

    yend = Y0 + len(ROWS) * RH
    body.append(f'<text x="24" y="{yend+16}" class="xs">'
                'The top six are why a hundred and fifty lines of frame log were worth writing. '
                '<tspan class="t-hi">The bottom six are why the tools exist</tspan>, and</text>')
    body.append(f'<text x="24" y="{yend+32}" class="xs">'
                'no amount of logging gets you them: they need the frame '
                '<tspan class="t-hi">replayed</tspan>, which needs the driver.</text>')

    return svg(W, H, "Our frame log against a real capture",
               "A twelve-row comparison. The log gives the event tree, resource names, uniform "
               "byte counts, a cross-check against a second counter, CI operation and support "
               "for every backend. A capture gives resource contents at an event, replay up to "
               "a draw, live pipeline state, shader disassembly, per-draw GPU timing and "
               "per-fragment shader stepping. The last six need the frame replayed.",
               "\n".join("  " + b for b in body), "f49f")


if __name__ == "__main__":
    write("l49_fig1.svg", fig1())
    write("l49_fig2.svg", fig2())
    write("l49_fig3.svg", fig3())
    write("l49_fig4.svg", fig4())
    write("l49_fig5.svg", fig5())
    write("l49_fig6.svg", fig6())
