#!/usr/bin/env python3
"""Compute every SVG figure for Lesson 4.2 from real data.

Same discipline as 3.7 through 4.1: nothing is placed by eye, and every number
drawn here is a number `scratch/verify_42.log` or `scratch/run_r*_v*.log` printed.
Each figure's docstring states the ONE claim a reader must be able to extract by
MEASURING the picture.

Marker ids are namespaced per figure, and <text> never carries an inline `fill`
(course.css beats an SVG presentation attribute — apply-shared.py lints for it).

Writes scratch/l42_fig{1..6}.svg.
"""
import math

# ---- Measured inputs --------------------------------------------------------

# verify_42 §C, the 2048x2048 noise workload.
REC_ACQUIRE = 0.0009      # ms, SDL_AcquireGPUCommandBuffer
REC_BLITS = 0.1846        # ms, 48 x SDL_BlitGPUTexture
REC_SUBMIT = 0.0024       # ms, SDL_SubmitGPUCommandBufferAndAcquireFence
REC_CPU = 0.1879          # ms, all of the above
REC_WAIT = 4.7815         # ms, SDL_WaitForGPUFences
REC_RATIO = 25.45
BLIT_COUNT = 48
BLIT_SIDE = 2048

# verify_42 §C, the size sweep: (side, MB per texture, flat GB/s, noise GB/s)
BANDWIDTH = [
    (512, 1.00, 129.6, 124.9),
    (1024, 4.00, 466.0, 399.9),
    (2048, 16.00, 686.3, 309.1),
    (4096, 64.00, 797.9, 277.6),
]
BUS_GBS = 273.0           # Apple M4 Pro, published memory bandwidth

# verify_42 §B: (float, UNORM byte, UNORM_SRGB byte)
CLEAR_BYTES = [
    (0.00, 0, 0),
    (0.25, 64, 137),
    (0.50, 128, 188),
    (0.75, 191, 225),
    (1.00, 255, 255),
]

# verify_42 §F, the engine's own 320x180 framebuffer.
FB_W, FB_H = 320, 180
FB_BYTES = FB_W * FB_H * 4
FB_MEMCPY_MS = 0.0028
FB_ROUNDTRIP_MS = 0.2084

# The probe, run controlled and alternating (scratch/run_r*_v*.log), 60 Hz display.
# (label, draw, record, acquire, fence, frame)
FRAME_OFF = ("[4] off", 0.301, 0.226, 16.002, 0.000, 16.667)
FRAME_ON = ("[4] on", 0.284, 0.223, 15.272, 0.761, 16.667)
FRAME_IMMEDIATE = ("IMMEDIATE", 0.329, 0.165, 4.238, 0.000, 4.816)

INK = "var(--dia-ink)"
C_DRAW = "#5ac878"        # software rasterizer, on the CPU
C_RECORD = "#5acdd7"      # building the command buffer
C_ACQUIRE = "#5082e6"     # blocked waiting for a swapchain image
C_FENCE = "#f0961e"       # blocked waiting on a fence
C_OTHER = "#7a7a72"       # the remainder, always displayed (Lesson 3.10's rule)
C_OLD = "#78b4eb"         # a thing you already built
C_NEW = "#eb786e"         # a thing that is genuinely new


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
# Figure 1 — the object model, mapped
# =============================================================================
# ONE CLAIM: of the nine objects in SDL_GPU, FIVE are renames of things already
# built and FOUR are genuinely new — and every one of the four exists because a
# second processor is involved. A reader must be able to count 5 blue and 4 red.
def fig1():
    W, H = 720, 492
    ROW_H = 40.0
    Y0 = 76.0
    LX, LW = 24.0, 268.0        # left column: what you built
    RX, RW = 400.0, 296.0       # right column: what SDL_GPU calls it

    rows = [
        ("engine::framebuffer", "1.5", "swapchain texture", False),
        ("engine::texture + sampler", "3.9", "SDL_GPUTexture + SDL_GPUSampler", False),
        ("engine::depth_buffer", "3.1", "depth-stencil target", False),
        ("engine::fill_style", "3.2-4.1", "SDL_GPUGraphicsPipeline", False),
        ("the loop in fill_triangle", "2.2", "render pass + draw call", False),
        ("&#8212;", "", "SDL_GPUDevice", True),
        ("&#8212;", "", "SDL_GPUCommandBuffer", True),
        ("&#8212;", "", "SDL_GPUTransferBuffer", True),
        ("&#8212;", "", "SDL_GPUFence", True),
    ]

    body = []
    body.append('<text x="24" y="26" class="sm">what you have already built</text>')
    body.append(f'<text x="{f(RX)}" y="26" class="sm">what SDL_GPU calls it</text>')
    body.append('<text x="24" y="46" class="xs muted">Modules 1-3, on the CPU</text>')
    body.append(f'<text x="{f(RX)}" y="46" class="xs muted">'
                'the objects of Lessons 4.2 to 4.8</text>')

    old = new = 0
    for i, (left, lesson, right, is_new) in enumerate(rows):
        y = Y0 + i * ROW_H
        col = C_NEW if is_new else C_OLD
        if is_new:
            new += 1
        else:
            old += 1

        # Left box: solid for a rename, dashed-empty for something with no
        # counterpart. The emptiness is the information.
        dash = ' stroke-dasharray="4 4"' if is_new else ""
        body.append(f'<rect x="{f(LX)}" y="{f(y)}" width="{f(LW)}" height="28" rx="4" '
                    f'class="fill-soft ink-soft"{dash} stroke-width="1"/>')
        body.append(f'<text x="{f(LX + 10)}" y="{f(y + 18)}" class="xs mono'
                    f'{" muted" if is_new else ""}">{left}</text>')
        if lesson:
            body.append(f'<text x="{f(LX + LW - 10)}" y="{f(y + 18)}" '
                        f'class="xs muted" text-anchor="end">{lesson}</text>')

        # The arrow between them.
        body.append(f'<line x1="{f(LX + LW + 12)}" y1="{f(y + 14)}" '
                    f'x2="{f(RX - 12)}" y2="{f(y + 14)}" '
                    f'class="{"ink-soft" if not is_new else "hi"}" stroke-width="1.4" '
                    f'marker-end="url(#{"e-s" if not is_new else "e-h"})"/>')

        body.append(f'<rect x="{f(RX)}" y="{f(y)}" width="{f(RW)}" height="28" rx="4" '
                    f'fill="{col}" opacity="0.18" stroke="{col}" stroke-width="1.4"/>')
        body.append(f'<text x="{f(RX + 10)}" y="{f(y + 18)}" class="xs mono">{right}</text>')

    # The count, which is the measurable claim.
    y = Y0 + len(rows) * ROW_H + 26
    body.append(f'<rect x="{f(LX)}" y="{f(y - 11)}" width="12" height="12" '
                f'fill="{C_OLD}" opacity="0.6"/>')
    body.append(f'<text x="{f(LX + 18)}" y="{f(y)}" class="xs">'
                f'{old} renames &#8212; you know these already</text>')
    body.append(f'<rect x="{f(LX + 240)}" y="{f(y - 11)}" width="12" height="12" '
                f'fill="{C_NEW}" opacity="0.6"/>')
    body.append(f'<text x="{f(LX + 258)}" y="{f(y)}" class="xs">'
                f'{new} new &#8212; and all four exist because there are two processors</text>')

    return svg(W, H, "The SDL_GPU object model against what you already built",
               f"Two columns. On the left, {old} things built in Modules 1 to 3; on the right, "
               f"the SDL_GPU object each becomes: framebuffer to swapchain texture, texture and "
               f"sampler to SDL_GPUTexture and SDL_GPUSampler, depth buffer to depth-stencil "
               f"target, fill_style to graphics pipeline, the fill loop to a render pass and draw "
               f"call. Below them {new} SDL_GPU objects with an empty box opposite: the device, "
               f"the command buffer, the transfer buffer and the fence. Those four have no "
               f"counterpart because they exist to manage a second processor, which a software "
               f"rasterizer does not have.",
               "\n".join(body), "l42f1")


# =============================================================================
# Figure 2 — recording is not executing
# =============================================================================
# ONE CLAIM: the CPU's whole contribution to a submission is 0.19 ms and the work
# it describes takes 4.78 ms — so at one scale, the recording block is 25 times
# narrower than the execution block, and the CPU is free during all of it.
def fig2():
    W, H = 720, 376
    X0, XW = 104.0, 556.0
    total_ms = 11.0                       # the window of time drawn
    ppms = XW / total_ms

    CPU_Y, GPU_Y = 118.0, 232.0
    LANE_H = 44.0

    body = []
    body.append('<text x="24" y="26" class="sm">one command buffer, drawn to scale in time</text>')
    body.append(f'<text x="24" y="46" class="xs muted">'
                f'measured: {REC_CPU:.4f} ms of CPU calls describing '
                f'{REC_WAIT:.4f} ms of GPU work &#8212; {REC_RATIO:.1f}&#215;</text>')

    # Lane backgrounds and labels.
    for y, label in ((CPU_Y, "CPU"), (GPU_Y, "GPU")):
        body.append(f'<rect x="{f(X0)}" y="{f(y)}" width="{f(XW)}" height="{f(LANE_H)}" '
                    f'rx="3" class="fill-soft"/>')
        body.append(f'<text x="{f(X0 - 12)}" y="{f(y + 27)}" class="sm mono" '
                    f'text-anchor="end">{label}</text>')

    def block(x_ms, w_ms, y, colour, label=None):
        x = X0 + x_ms * ppms
        w = max(w_ms * ppms, 1.2)
        body.append(f'<rect x="{f(x)}" y="{f(y + 6)}" width="{f(w)}" height="{f(LANE_H - 12)}" '
                    f'rx="2" fill="{colour}" opacity="0.85"/>')
        if label is not None:
            body.append(f'<text x="{f(x + w / 2)}" y="{f(y + 27)}" class="xs t-inv" '
                        f'text-anchor="middle">{label}</text>')
        return x, w

    # Frame N: a sliver of recording on the CPU, then the work on the GPU. The
    # recording block is ~10 px wide at this scale, so its label goes ABOVE it,
    # anchored at its left edge, where nothing else is drawn.
    rx, rw = block(0.0, REC_CPU, CPU_Y, C_RECORD)
    body.append(f'<text x="{f(rx)}" y="{f(CPU_Y - 22)}" class="xs">record</text>')
    body.append(f'<text x="{f(rx)}" y="{f(CPU_Y - 8)}" class="xs mono t-hi">'
                f'{REC_CPU:.4f} ms</text>')

    # The CPU is not idle afterwards — it is free, which is the whole point.
    # Left-anchored and short, because a centred label wider than its own block
    # runs onto the lane behind it and across the fence arrow at the block's end.
    # check-page.js does not catch a label overflowing a filled rect; eyes do.
    fx0, _ = block(REC_CPU + 0.06, REC_WAIT - 0.06, CPU_Y, C_DRAW)
    body.append(f'<text x="{f(fx0 + 10)}" y="{f(CPU_Y + 27)}" class="xs t-inv">'
                f'free &#8212; the next frame&#8217;s work</text>')

    gx, gw = block(REC_CPU, REC_WAIT, GPU_Y, C_ACQUIRE, "execute")
    body.append(f'<text x="{f(gx + gw / 2)}" y="{f(GPU_Y + LANE_H + 16)}" '
                f'class="xs mono muted" text-anchor="middle">{REC_WAIT:.4f} ms</text>')

    # The submit handoff.
    sx = X0 + REC_CPU * ppms
    body.append(f'<line x1="{f(sx)}" y1="{f(CPU_Y + LANE_H)}" x2="{f(sx)}" '
                f'y2="{f(GPU_Y - 4)}" class="hi" stroke-width="2" marker-end="url(#e-h)"/>')
    body.append(f'<text x="{f(sx + 8)}" y="{f(GPU_Y - 12)}" class="xs mono t-hi">'
                f'SDL_SubmitGPUCommandBuffer</text>')

    # The fence: an arrow back, and what it means.
    fx = X0 + (REC_CPU + REC_WAIT) * ppms
    body.append(f'<line x1="{f(fx)}" y1="{f(GPU_Y - 4)}" x2="{f(fx)}" '
                f'y2="{f(CPU_Y + LANE_H + 4)}" class="ink" stroke-width="1.6" '
                f'stroke-dasharray="4 3" marker-end="url(#e-i)"/>')
    body.append(f'<text x="{f(fx + 8)}" y="{f(CPU_Y + LANE_H + 22)}" class="xs mono">'
                f'the fence signals here &#8212; and only here</text>')
    body.append(f'<text x="{f(fx + 8)}" y="{f(CPU_Y + LANE_H + 36)}" class="xs mono">'
                f'is the result readable</text>')

    # The time axis. The unit goes UNDER the tick row, not beside it, because the
    # rightmost tick label and a right-anchored unit label share the same x.
    ay = GPU_Y + LANE_H + 52
    body.append(f'<line x1="{f(X0)}" y1="{f(ay)}" x2="{f(X0 + XW)}" y2="{f(ay)}" '
                f'class="ink-soft" stroke-width="1.2" marker-end="url(#e-s)"/>')
    for ms in range(0, int(total_ms) + 1, 2):
        x = X0 + ms * ppms
        body.append(f'<line x1="{f(x)}" y1="{f(ay - 4)}" x2="{f(x)}" y2="{f(ay + 4)}" '
                    f'class="ink-soft" stroke-width="1"/>')
        body.append(f'<text x="{f(x)}" y="{f(ay + 18)}" class="xs mono muted" '
                    f'text-anchor="middle">{ms}</text>')
    body.append(f'<text x="{f(X0)}" y="{f(ay + 32)}" class="xs muted">milliseconds</text>')

    return svg(W, H, "Recording against executing, drawn to scale",
               f"Two horizontal lanes on a shared time axis. The CPU lane shows a very narrow "
               f"block labelled record, {REC_CPU:.4f} milliseconds wide, which is every SDL call "
               f"needed to describe the work, followed by a wide block showing the CPU free for "
               f"the rest of the time. An arrow marked SDL_SubmitGPUCommandBuffer drops from the "
               f"record block to the GPU lane, where a block {REC_WAIT:.4f} milliseconds wide, "
               f"{REC_RATIO:.0f} times wider, shows the work actually happening. A dashed arrow "
               f"returns from the end of the GPU block to the CPU lane: the fence, the only "
               f"moment at which the result may be read.",
               "\n".join(body), "l42f2")


# =============================================================================
# Figure 3 — the three hops of a pixel
# =============================================================================
# ONE CLAIM: a pixel takes three copies to reach the screen, they happen in TWO
# different time frames (one now, two later), and only the first is paid by the
# CPU. A reader must be able to read which arrow is "now" and which is "later".
def fig3():
    W, H = 760, 340
    BOX_W, BOX_H = 120.0, 74.0
    Y = 112.0
    GAP = 76.0

    stages = [
        ("engine::\nframebuffer", "CPU memory", C_DRAW),
        ("transfer\nbuffer", "both can see", C_RECORD),
        ("SDL_GPU\nTexture", "device memory", C_ACQUIRE),
        ("swapchain\ntexture", "the display's", C_FENCE),
    ]
    arrows = [
        ("std::memcpy", "NOW, on the CPU", f"{FB_MEMCPY_MS:.4f} ms", True),
        ("SDL_UploadToGPUTexture", "LATER, on the GPU", "recorded", False),
        ("SDL_BlitGPUTexture", "LATER, on the GPU", "recorded", False),
    ]

    total_w = len(stages) * BOX_W + (len(stages) - 1) * GAP
    X0 = (W - total_w) / 2

    body = []
    body.append(f'<text x="24" y="26" class="sm">'
                f'one pixel of a {FB_W}&#215;{FB_H} framebuffer, on its way to the screen</text>')
    body.append(f'<text x="24" y="46" class="xs muted">'
                f'{FB_BYTES:,} bytes per frame, three copies, two time frames</text>')

    for i, (name, where, colour) in enumerate(stages):
        x = X0 + i * (BOX_W + GAP)
        body.append(f'<rect x="{f(x)}" y="{f(Y)}" width="{f(BOX_W)}" height="{f(BOX_H)}" '
                    f'rx="5" fill="{colour}" opacity="0.16" stroke="{colour}" stroke-width="1.6"/>')
        for j, line in enumerate(name.split("\n")):
            body.append(f'<text x="{f(x + BOX_W / 2)}" y="{f(Y + 28 + j * 15)}" '
                        f'class="xs mono" text-anchor="middle">{line}</text>')
        body.append(f'<text x="{f(x + BOX_W / 2)}" y="{f(Y + BOX_H - 10)}" '
                    f'class="xs muted" text-anchor="middle">{where}</text>')

        if i < len(arrows):
            label, when, cost, is_now = arrows[i]
            ax0 = x + BOX_W + 8
            ax1 = x + BOX_W + GAP - 8
            body.append(f'<line x1="{f(ax0)}" y1="{f(Y + BOX_H / 2)}" x2="{f(ax1)}" '
                        f'y2="{f(Y + BOX_H / 2)}" class="{"hi" if is_now else "ink-soft"}" '
                        f'stroke-width="1.8" marker-end="url(#{"e-h" if is_now else "e-s"})"/>')
            mid = (ax0 + ax1) / 2
            body.append(f'<text x="{f(mid)}" y="{f(Y - 30)}" class="xs mono" '
                        f'text-anchor="middle">{label}</text>')
            body.append(f'<text x="{f(mid)}" y="{f(Y - 16)}" '
                        f'class="xs {"t-hi" if is_now else "muted"}" '
                        f'text-anchor="middle">{when}</text>')
            body.append(f'<text x="{f(mid)}" y="{f(Y + BOX_H + 18)}" class="xs mono muted" '
                        f'text-anchor="middle">{cost}</text>')

    # The line that divides the two time frames.
    split_x = X0 + 2 * BOX_W + GAP + 10
    body.append(f'<line x1="{f(split_x)}" y1="{f(Y - 6)}" x2="{f(split_x)}" '
                f'y2="{f(Y + BOX_H + 40)}" class="ink" stroke-width="1.2" '
                f'stroke-dasharray="3 4"/>')
    body.append(f'<text x="{f(split_x - 8)}" y="{f(Y + BOX_H + 58)}" class="xs" '
                f'text-anchor="end">happens when you call it</text>')
    body.append(f'<text x="{f(split_x + 8)}" y="{f(Y + BOX_H + 58)}" class="xs">'
                f'happens when the GPU gets to it</text>')

    body.append(f'<text x="{f(W / 2)}" y="{f(Y + BOX_H + 86)}" class="xs mono muted" '
                f'text-anchor="middle">whole round trip, measured with a fence: '
                f'{FB_ROUNDTRIP_MS:.4f} ms</text>')

    return svg(W, H, "The three copies between a CPU pixel and the display",
               f"Four boxes left to right: the engine framebuffer in CPU memory, the transfer "
               f"buffer in memory both processors can see, an SDL_GPUTexture in device memory, and "
               f"the swapchain texture owned by the display. Three arrows join them. The first, "
               f"a memcpy, is marked NOW on the CPU and measured at {FB_MEMCPY_MS:.4f} "
               f"milliseconds. The second and third, SDL_UploadToGPUTexture and "
               f"SDL_BlitGPUTexture, are marked LATER on the GPU and are only recorded when "
               f"called. A dashed vertical line separates the two time frames.",
               "\n".join(body), "l42f3")


# =============================================================================
# Figure 4 — where a frame's 16.667 ms actually goes
# =============================================================================
# ONE CLAIM: turning on a full GPU sync every frame does NOT change the frame
# time. The orange fence band appears and the blue acquire band shrinks by very
# nearly the same amount, and both bars end at the same place.
def fig4():
    W, H = 720, 356
    X0, XW = 130.0, 500.0
    scale_ms = 17.4
    ppms = XW / scale_ms
    BAR_H = 46.0

    BAR_GAP = 34.0
    bars = [FRAME_OFF, FRAME_ON, FRAME_IMMEDIATE]
    body = []
    body.append('<text x="24" y="26" class="sm">'
                'where one frame of the GPU probe actually goes</text>')
    body.append('<text x="24" y="46" class="xs muted">'
                'measured on a 60 Hz display, averaged over 160 frames, '
                'variants run alternately in one session</text>')

    segs = [("draw", C_DRAW), ("record", C_RECORD),
            ("acquire", C_ACQUIRE), ("fence", C_FENCE), ("other", C_OTHER)]

    for i, (label, draw, record, acquire, fence, frame) in enumerate(bars):
        y = 92.0 + i * (BAR_H + BAR_GAP)
        other = max(frame - (draw + record + acquire + fence), 0.0)
        values = [draw, record, acquire, fence, other]

        body.append(f'<text x="{f(X0 - 12)}" y="{f(y + 28)}" class="xs mono" '
                    f'text-anchor="end">{label}</text>')

        x = X0
        for (name, colour), v in zip(segs, values):
            w = v * ppms
            if w <= 0.0:
                continue
            body.append(f'<rect x="{f(x)}" y="{f(y)}" width="{f(w)}" height="{f(BAR_H)}" '
                        f'fill="{colour}" opacity="0.88"/>')
            if w > 34:
                body.append(f'<text x="{f(x + w / 2)}" y="{f(y + 28)}" class="xs t-inv" '
                            f'text-anchor="middle">{v:.2f}</text>')
            x += w

        body.append(f'<text x="{f(x + 10)}" y="{f(y + 28)}" class="xs mono">'
                    f'{frame:.3f} ms</text>')

        # The narrow bands get a leader line rather than a label inside them.
        if fence > 0.0:
            fx = X0 + (draw + record + acquire) * ppms
            body.append(f'<line x1="{f(fx + fence * ppms / 2)}" y1="{f(y + BAR_H)}" '
                        f'x2="{f(fx + fence * ppms / 2)}" y2="{f(y + BAR_H + 14)}" '
                        f'class="hi" stroke-width="1.2"/>')
            body.append(f'<text x="{f(fx + fence * ppms / 2 - 6)}" y="{f(y + BAR_H + 26)}" '
                        f'class="xs mono t-hi" text-anchor="end">'
                        f'fence {fence:.3f}</text>')

    # The 60 Hz budget.
    bx = X0 + 16.667 * ppms
    body.append(f'<line x1="{f(bx)}" y1="82" x2="{f(bx)}" '
                f'y2="{f(92.0 + 2 * (BAR_H + BAR_GAP) + BAR_H + 8)}" '
                f'class="ink" stroke-width="1.4" stroke-dasharray="5 4"/>')
    body.append(f'<text x="{f(bx - 6)}" y="78" class="xs mono" text-anchor="end">'
                f'16.667 ms &#8212; one 60 Hz frame</text>')

    # Legend.
    ly = 92.0 + 3 * (BAR_H + BAR_GAP) + 2
    lx = X0
    for name, colour in segs:
        body.append(f'<rect x="{f(lx)}" y="{f(ly - 10)}" width="11" height="11" '
                    f'fill="{colour}" opacity="0.88"/>')
        body.append(f'<text x="{f(lx + 16)}" y="{f(ly)}" class="xs">{name}</text>')
        lx += 76

    return svg(W, H, "A frame's time, with and without a GPU sync",
               f"Three horizontal stacked bars on a shared millisecond scale. The first, with the "
               f"fence off, is draw {FRAME_OFF[1]:.2f}, record {FRAME_OFF[2]:.2f} and acquire "
               f"{FRAME_OFF[3]:.2f} milliseconds, totalling {FRAME_OFF[5]:.3f}. The second, with a "
               f"full GPU sync every frame, adds a fence band of {FRAME_ON[4]:.3f} milliseconds "
               f"while the acquire band shrinks from {FRAME_OFF[3]:.2f} to {FRAME_ON[3]:.2f} — and "
               f"the bar ends in exactly the same place, {FRAME_ON[5]:.3f} milliseconds. A dashed "
               f"line marks the 60 Hz budget of 16.667 milliseconds, which both bars reach. The "
               f"third bar, in IMMEDIATE present mode, is only {FRAME_IMMEDIATE[5]:.3f} "
               f"milliseconds, showing that almost all of the first two is waiting for the display.",
               "\n".join(body), "l42f4")


# =============================================================================
# Figure 5 — a float clear becomes bytes
# =============================================================================
# ONE CLAIM: the same SDL_FColor value lands as a DIFFERENT byte depending only
# on the target's format. 0.5 becomes 128 in a _UNORM target and 188 in a
# _UNORM_SRGB one, and a reader must be able to read both off the axes.
def fig5():
    W, H = 720, 400
    X0, Y0 = 92.0, 60.0
    PW, PH = 420.0, 288.0

    def px(v):
        return X0 + v * PW

    def py(byte):
        return Y0 + PH - (byte / 255.0) * PH

    body = []
    body.append('<text x="24" y="26" class="sm">'
                'the same clear colour, into two formats</text>')
    body.append('<text x="24" y="44" class="xs muted">'
                'measured: clear a 1&#215;1 target, download it, read the byte</text>')

    # Grid and axes.
    for i in range(0, 6):
        v = i / 5.0
        body.append(f'<line x1="{f(px(v))}" y1="{f(Y0)}" x2="{f(px(v))}" y2="{f(Y0 + PH)}" '
                    f'class="grid" stroke-width="0.8"/>')
        body.append(f'<text x="{f(px(v))}" y="{f(Y0 + PH + 18)}" class="xs mono muted" '
                    f'text-anchor="middle">{v:.1f}</text>')
    for b in (0, 64, 128, 192, 255):
        body.append(f'<line x1="{f(X0)}" y1="{f(py(b))}" x2="{f(X0 + PW)}" y2="{f(py(b))}" '
                    f'class="grid" stroke-width="0.8"/>')
        body.append(f'<text x="{f(X0 - 10)}" y="{f(py(b) + 4)}" class="xs mono muted" '
                    f'text-anchor="end">{b}</text>')

    body.append(f'<line x1="{f(X0)}" y1="{f(Y0 + PH)}" x2="{f(X0 + PW)}" y2="{f(Y0 + PH)}" '
                f'class="ink" stroke-width="1.4"/>')
    body.append(f'<line x1="{f(X0)}" y1="{f(Y0)}" x2="{f(X0)}" y2="{f(Y0 + PH)}" '
                f'class="ink" stroke-width="1.4"/>')
    body.append(f'<text x="{f(X0 + PW / 2)}" y="{f(Y0 + PH + 36)}" class="xs" '
                f'text-anchor="middle">the float in SDL_FColor</text>')
    body.append(f'<text x="{f(X0 - 62)}" y="{f(Y0 + PH / 2)}" class="xs" '
                f'transform="rotate(-90 {f(X0 - 62)} {f(Y0 + PH / 2)})" '
                f'text-anchor="middle">the byte stored</text>')

    # The two curves, sampled densely and drawn continuous, with the five
    # MEASURED points marked on top of each.
    def srgb(v):
        return 12.92 * v if v <= 0.0031308 else 1.055 * (v ** (1.0 / 2.4)) - 0.055

    pts_unorm = " ".join(f"{f(px(i / 100.0))},{f(py(i / 100.0 * 255.0))}" for i in range(101))
    pts_srgb = " ".join(f"{f(px(i / 100.0))},{f(py(srgb(i / 100.0) * 255.0))}"
                        for i in range(101))
    body.append(f'<polyline points="{pts_unorm}" fill="none" stroke="{C_ACQUIRE}" '
                f'stroke-width="2.2"/>')
    body.append(f'<polyline points="{pts_srgb}" fill="none" stroke="{C_FENCE}" '
                f'stroke-width="2.2"/>')

    for v, unorm, srgb_byte in CLEAR_BYTES:
        body.append(f'<circle cx="{f(px(v))}" cy="{f(py(unorm))}" r="3.4" '
                    f'fill="{C_ACQUIRE}"/>')
        body.append(f'<circle cx="{f(px(v))}" cy="{f(py(srgb_byte))}" r="3.4" '
                    f'fill="{C_FENCE}"/>')

    # The 0.5 case, called out because it is the one everybody has an intuition
    # about and the intuition is wrong for one of the two formats.
    body.append(f'<line x1="{f(px(0.5))}" y1="{f(py(0))}" x2="{f(px(0.5))}" y2="{f(py(188))}" '
                f'class="hi" stroke-width="1.2" stroke-dasharray="3 3"/>')
    body.append(f'<line x1="{f(X0)}" y1="{f(py(128))}" x2="{f(px(0.5))}" y2="{f(py(128))}" '
                f'class="hi" stroke-width="1.2" stroke-dasharray="3 3"/>')
    body.append(f'<line x1="{f(X0)}" y1="{f(py(188))}" x2="{f(px(0.5))}" y2="{f(py(188))}" '
                f'class="hi" stroke-width="1.2" stroke-dasharray="3 3"/>')

    lx = X0 + PW + 24
    body.append(f'<rect x="{f(lx)}" y="{f(Y0 + 16)}" width="12" height="12" fill="{C_ACQUIRE}"/>')
    body.append(f'<text x="{f(lx + 18)}" y="{f(Y0 + 26)}" class="xs mono">_UNORM</text>')
    body.append(f'<text x="{f(lx)}" y="{f(Y0 + 46)}" class="xs muted">no transfer</text>')
    body.append(f'<text x="{f(lx)}" y="{f(Y0 + 60)}" class="xs muted">function at all</text>')
    body.append(f'<text x="{f(lx)}" y="{f(Y0 + 78)}" class="xs mono t-hi">0.5 &#8594; 128</text>')

    body.append(f'<rect x="{f(lx)}" y="{f(Y0 + 112)}" width="12" height="12" fill="{C_FENCE}"/>')
    body.append(f'<text x="{f(lx + 18)}" y="{f(Y0 + 122)}" class="xs mono">_UNORM_SRGB</text>')
    body.append(f'<text x="{f(lx)}" y="{f(Y0 + 142)}" class="xs muted">encodes on write,</text>')
    body.append(f'<text x="{f(lx)}" y="{f(Y0 + 156)}" class="xs muted">decodes on read</text>')
    body.append(f'<text x="{f(lx)}" y="{f(Y0 + 174)}" class="xs mono t-hi">0.5 &#8594; 188</text>')

    body.append(f'<text x="{f(lx)}" y="{f(Y0 + 216)}" class="xs">the clear colour</text>')
    body.append(f'<text x="{f(lx)}" y="{f(Y0 + 230)}" class="xs">is always LINEAR;</text>')
    body.append(f'<text x="{f(lx)}" y="{f(Y0 + 244)}" class="xs">the FORMAT decides</text>')
    body.append(f'<text x="{f(lx)}" y="{f(Y0 + 258)}" class="xs">what is stored</text>')

    return svg(W, H, "One float clear colour, two formats, two different bytes",
               "A graph of the byte stored against the float given in SDL_FColor. A straight blue "
               "line is the _UNORM format, where the byte is simply the float times 255: half "
               "becomes 128. A curved orange line is the _UNORM_SRGB format, which applies the "
               "sRGB encode on write, so half becomes 188. Five measured points are marked on each "
               "line and both pass through zero and 255. Dashed guides pick out the value one half "
               "and its two answers.",
               "\n".join(body), "l42f5")


# =============================================================================
# Figure 6 — the bandwidth that was impossible
# =============================================================================
# ONE CLAIM: measured on flat-coloured textures, the copy reports a bandwidth
# ABOVE this machine's memory bus, which cannot be true; on incompressible noise
# it lands on the bus. A reader must be able to see two bars crossing the line
# and the noise bars converging to it.
def fig6():
    W, H = 720, 360
    X0, Y0 = 78.0, 62.0
    PW, PH = 500.0, 232.0
    top_gbs = 900.0

    def py(v):
        return Y0 + PH - (v / top_gbs) * PH

    body = []
    body.append('<text x="24" y="26" class="sm">'
                'copy bandwidth, and the number that could not be true</text>')
    body.append('<text x="24" y="44" class="xs muted">'
                'the same ping-ponged blits, counting reads and writes, at four texture sizes</text>')

    for v in (0, 200, 400, 600, 800):
        body.append(f'<line x1="{f(X0)}" y1="{f(py(v))}" x2="{f(X0 + PW)}" y2="{f(py(v))}" '
                    f'class="grid" stroke-width="0.8"/>')
        body.append(f'<text x="{f(X0 - 10)}" y="{f(py(v) + 4)}" class="xs mono muted" '
                    f'text-anchor="end">{v}</text>')
    body.append(f'<line x1="{f(X0)}" y1="{f(Y0 + PH)}" x2="{f(X0 + PW)}" y2="{f(Y0 + PH)}" '
                f'class="ink" stroke-width="1.4"/>')
    body.append(f'<text x="{f(X0 - 56)}" y="{f(Y0 + PH / 2)}" class="xs" '
                f'transform="rotate(-90 {f(X0 - 56)} {f(Y0 + PH / 2)})" '
                f'text-anchor="middle">GB/s</text>')

    group_w = PW / len(BANDWIDTH)
    bar_w = 34.0
    for i, (side, mb, flat, noise) in enumerate(BANDWIDTH):
        cx = X0 + group_w * (i + 0.5)
        for j, (v, colour, _name) in enumerate(((flat, C_OLD, "flat"), (noise, C_NEW, "noise"))):
            x = cx - bar_w - 3 + j * (bar_w + 6)
            body.append(f'<rect x="{f(x)}" y="{f(py(v))}" width="{f(bar_w)}" '
                        f'height="{f(Y0 + PH - py(v))}" fill="{colour}" opacity="0.85"/>')
            body.append(f'<text x="{f(x + bar_w / 2)}" y="{f(py(v) - 6)}" class="xs mono" '
                        f'text-anchor="middle">{v:.0f}</text>')
        body.append(f'<text x="{f(cx)}" y="{f(Y0 + PH + 18)}" class="xs mono muted" '
                    f'text-anchor="middle">{side}&#178;</text>')
        body.append(f'<text x="{f(cx)}" y="{f(Y0 + PH + 32)}" class="xs muted" '
                    f'text-anchor="middle">{mb:.0f} MB</text>')

    # The bus, which is the whole argument.
    body.append(f'<line x1="{f(X0)}" y1="{f(py(BUS_GBS))}" x2="{f(X0 + PW)}" '
                f'y2="{f(py(BUS_GBS))}" class="hi" stroke-width="2" stroke-dasharray="6 4"/>')
    body.append(f'<text x="{f(X0 + PW + 8)}" y="{f(py(BUS_GBS) + 4)}" class="xs mono t-hi">'
                f'{BUS_GBS:.0f} GB/s</text>')
    body.append(f'<text x="{f(X0 + PW + 8)}" y="{f(py(BUS_GBS) + 18)}" class="xs muted">'
                f'this machine&#8217;s</text>')
    body.append(f'<text x="{f(X0 + PW + 8)}" y="{f(py(BUS_GBS) + 30)}" class="xs muted">'
                f'memory bus</text>')

    lx = X0
    ly = Y0 + PH + 58
    body.append(f'<rect x="{f(lx)}" y="{f(ly - 10)}" width="11" height="11" '
                f'fill="{C_OLD}" opacity="0.85"/>')
    body.append(f'<text x="{f(lx + 16)}" y="{f(ly)}" class="xs">'
                f'flat colour &#8212; compresses, so the bytes are never moved</text>')
    body.append(f'<rect x="{f(lx + 330)}" y="{f(ly - 10)}" width="11" height="11" '
                f'fill="{C_NEW}" opacity="0.85"/>')
    body.append(f'<text x="{f(lx + 346)}" y="{f(ly)}" class="xs">'
                f'noise &#8212; cannot compress</text>')

    return svg(W, H, "Copy bandwidth on compressible and incompressible content",
               f"A bar chart of copy bandwidth in gigabytes per second at four texture sizes, with "
               f"two bars per size. The first bar of each pair copies a flat-coloured texture and "
               f"reports {BANDWIDTH[2][2]:.0f} and {BANDWIDTH[3][2]:.0f} gigabytes per second at "
               f"2048 and 4096 squared — above the dashed line at {BUS_GBS:.0f}, this machine's "
               f"entire memory bandwidth, which is impossible. The second bar of each pair copies "
               f"incompressible noise and settles onto the line at {BANDWIDTH[3][3]:.0f}. The "
               f"difference is lossless render-target compression, which makes a flat texture "
               f"almost free to copy and makes the first measurement fiction.",
               "\n".join(body), "l42f6")


def main():
    print("Lesson 4.2 figures:")
    write("l42_fig1.svg", fig1())
    write("l42_fig2.svg", fig2())
    write("l42_fig3.svg", fig3())
    write("l42_fig4.svg", fig4())
    write("l42_fig5.svg", fig5())
    write("l42_fig6.svg", fig6())


if __name__ == "__main__":
    main()
