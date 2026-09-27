#!/usr/bin/env python3
"""Compute every SVG figure for Lesson 4.7 from real data.

Figure 6 is drawn from scratch/l47_{scene,nodepth}.ppm, which verify_47 §G renders
offscreen. The raster helpers are Lesson 4.5's, imported rather than copied.

Every number in every caption comes from scratch/verify_47.log.

Writes scratch/l47_fig{1..6}.svg.
"""
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_45 import (svg, write, f, panel, hexrgb, C_TINT,
                     C_POS, C_NRM, C_UV, C_INST, C_BAD, C_HUB)

# ---- Measured inputs, from scratch/verify_47.log ----------------------------

NEAR, FAR = 0.3, 100.0

# §C: distance -> (D16 measured mm, D16 predicted mm, D32F measured mm)
DEPTH_ROWS = [
    (1,   0.050,   0.051,  0.000),
    (5,   1.3,     1.268,  0.008),
    (10,  4.8,     5.071,  0.036),
    (25,  31.1,    31.694, 0.206),
    (50,  125.7,   126.777, 0.967),
    (90,  412.4,   410.758, 2.2),
]
# §D: reversed-Z, same distances
REVERSED = [
    (1,   0.050, 0.000),
    (5,   1.3,   0.000),
    (10,  5.1,   0.001),
    (25,  31.2,  0.004),
    (50,  125.2, 0.007),
    (90,  412.4, 0.012),
]

SUPPORTED = [("D16_UNORM", True, "the only one SDL guarantees"),
             ("D24_UNORM", False, "the classic desktop choice"),
             ("D32_FLOAT", True, "float depth, and what reversed-Z wants"),
             ("D24_UNORM_S8_UINT", False, "with a stencil byte"),
             ("D32_FLOAT_S8_UINT", True, "with a stencil byte")]

# §E: (corner, file rgb, unorm rgb, srgb rgb)
CORNERS = [
    ("top-left",     (222, 70, 60),  (222, 70, 60),  (186, 16, 12)),
    ("top-right",    (82, 190, 110), (82, 190, 110), (22, 131, 40)),
    ("bottom-left",  (80, 130, 230), (80, 130, 230), (20, 57, 202)),
    ("bottom-right", (238, 214, 150),(238, 214, 150),(218, 171, 78)),
]

NEAREST_COLOURS = 7
LINEAR_COLOURS = 13
DEPTH_CHANGED_PCT = 18.9


def z_ndc(d):
    return (FAR / (FAR - NEAR)) * (1.0 - NEAR / d)


# =============================================================================
# Figure 1 — where the depth range actually goes
# =============================================================================
def fig1():
    W, H = 720, 414
    body = []

    body.append('<text x="24" y="22" class="sm">'
                'z in the depth buffer against distance from the eye &#8212; near 0.3, '
                'far 100</text>')

    X0, Y0, PW, PH = 76.0, 46.0, 420.0, 240.0
    body.append(f'<rect x="{f(X0)}" y="{f(Y0)}" width="{f(PW)}" height="{f(PH)}" '
                'class="fill-soft ink" stroke-width="0.75"/>')

    def px(d):   return X0 + PW * (d / FAR)
    def py(z):   return Y0 + PH * (1.0 - z)

    # gridlines
    for z in (0.25, 0.5, 0.75):
        body.append(f'<line x1="{f(X0)}" y1="{f(py(z))}" x2="{f(X0+PW)}" y2="{f(py(z))}" '
                    'class="grid" stroke-width="0.75"/>')
        body.append(f'<text x="{f(X0-8)}" y="{f(py(z)+4)}" class="xs muted" '
                    f'text-anchor="end">{z}</text>')
    for z, lab in ((0.0, "0  near"), (1.0, "1  far")):
        body.append(f'<text x="{f(X0-8)}" y="{f(py(z)+4)}" class="xs muted" '
                    f'text-anchor="end">{lab}</text>')

    pts = []
    d = NEAR
    while d <= FAR:
        pts.append(f"{f(px(d))},{f(py(z_ndc(d)))}")
        d *= 1.03
    body.append(f'<polyline points="{" ".join(pts)}" class="hi" fill="none" stroke-width="1.8"/>')

    # the headline: 70% of the range in the first metre
    z1 = z_ndc(1.0)
    body.append(f'<rect x="{f(X0)}" y="{f(py(z1))}" width="{f(px(1.0)-X0)}" '
                f'height="{f(py(0)-py(z1))}" fill="{C_BAD}" fill-opacity="0.18" stroke="none"/>')
    body.append(f'<line x1="{f(px(1.0))}" y1="{f(Y0)}" x2="{f(px(1.0))}" y2="{f(Y0+PH)}" '
                'class="hi" stroke-width="1" stroke-dasharray="3 2"/>')
    body.append(f'<text x="{f(px(1.0)+8)}" y="{f(py(z1)-8)}" class="xs t-bad">'
                f'{100*z1:.0f}% of the whole range is spent in the first metre</text>')

    for d in (1, 10, 50, 100):
        body.append(f'<text x="{f(px(d))}" y="{f(Y0+PH+14)}" class="xs muted" '
                    f'text-anchor="middle">{d}</text>')
    body.append(f'<text x="{f(X0+PW/2)}" y="{f(Y0+PH+28)}" class="xs muted" '
                'text-anchor="middle">distance from the eye, metres</text>')

    # the derivation, to the right
    body.append(f'<text x="{f(X0+PW+22)}" y="{f(Y0+30)}" class="sm">why</text>')
    lines = [
        ("z = (f/(f&#8722;n)) &#215; (1 &#8722; n/d)", "mono"),
        ("dz/dd = (f/(f&#8722;n)) &#215; n/d&#178;", "mono t-hi"),
        ("", ""),
        ("resolution falls off as", ""),
        ("the SQUARE of distance", "t-hi"),
    ]
    for i, (t, cls) in enumerate(lines):
        body.append(f'<text x="{f(X0+PW+22)}" y="{f(Y0+56+i*18)}" class="xs {cls}">{t}</text>')

    ny = 324
    body.append(f'<rect x="24" y="{ny}" width="{W-48}" height="76" class="fill-soft ink" '
                'stroke-width="0.75" rx="3"/>')
    body.append(f'<text x="40" y="{ny+22}" class="sm">'
                'so the smallest gap a buffer of N evenly spaced codes can resolve is</text>')
    body.append(f'<text x="40" y="{ny+46}" class="sm mono t-hi">'
                '&#916;d = (f &#8722; n) &#215; d&#178; / (f &#215; n &#215; N)</text>')
    body.append(f'<text x="40" y="{ny+66}" class="xs muted">'
                'which Figure 2 measures against, and finds agreeing to within a few per '
                'cent across two orders of magnitude</text>')

    return svg(W, H, "Where the depth range goes",
               "Depth-buffer z against distance from the eye, for a frustum with near 0.3 and "
               "far 100. The curve is not a line: 70% of the entire representable range is spent "
               "in the first metre, because the perspective divide puts 1/d into the depth value. "
               "Differentiating gives dz/dd proportional to n/d squared, so depth resolution "
               "falls off as the square of distance — which is why a buffer that separates "
               "millimetres up close cannot separate half a metre at ninety.",
               "\n".join("  " + b for b in body), "f47a")


# =============================================================================
# Figure 2 — what each format can actually separate
# =============================================================================
def fig2():
    W, H = 720, 446
    body = []

    body.append('<text x="24" y="22" class="sm">'
                'the smallest separation each format can resolve &#8212; '
                '<tspan class="t-hi">measured</tspan>, and predicted by the formula</text>')

    # Which formats this device even has.
    sy = 40
    body.append(f'<text x="24" y="{sy}" class="xs muted">'
                'asked with SDL_GPUTextureSupportsFormat, not assumed:</text>')
    for i, (name, ok, why) in enumerate(SUPPORTED):
        y = sy + 18 + i * 15
        body.append(f'<text x="40" y="{y}" class="xs mono">{name}</text>')
        body.append(f'<text x="220" y="{y}" class="xs {"t-ok" if ok else "t-bad"}">'
                    f'{"yes" if ok else "NOT SUPPORTED"}</text>')
        body.append(f'<text x="330" y="{y}" class="xs muted">{why}</text>')
    body.append(f'<text x="40" y="{sy+18+5*15+4}" class="xs t-hi">'
                'D24_UNORM &#8212; the format a desktop renderer would have hard-coded &#8212; '
                'is not available on this device</text>')

    # The log-scale bars.
    BY = 206.0
    X0, BW_ = 96.0, 470.0
    lo, hi = 0.0005, 1000.0     # millimetres

    def bx(mm):
        mm = max(mm, lo)
        return X0 + BW_ * (math.log10(mm) - math.log10(lo)) / (math.log10(hi) - math.log10(lo))

    body.append(f'<text x="{f(X0)}" y="{f(BY-26)}" class="xs muted">'
                'smallest resolvable separation, millimetres, logarithmic</text>')
    for mm, lab in ((0.001, "1 &#181;m"), (0.1, "0.1 mm"), (10, "10 mm"), (1000, "1 m")):
        body.append(f'<line x1="{f(bx(mm))}" y1="{f(BY-8)}" x2="{f(bx(mm))}" '
                    f'y2="{f(BY + len(DEPTH_ROWS)*26)}" class="grid" stroke-width="0.75"/>')
        body.append(f'<text x="{f(bx(mm))}" y="{f(BY-12)}" class="xs muted" '
                    f'text-anchor="middle">{lab}</text>')

    for i, (d, d16, d16p, d32) in enumerate(DEPTH_ROWS):
        y = BY + i * 26
        body.append(f'<text x="24" y="{f(y+14)}" class="xs">{d} m</text>')

        for mm, col, h in ((d32, C_POS, 9), (d16, C_BAD, 9)):
            body.append(f'<rect x="{f(X0)}" y="{f(y + (0 if col==C_POS else 10))}" '
                        f'width="{f(max(bx(mm)-X0, 1.0))}" height="{h}" fill="{col}" '
                        'fill-opacity="0.8" stroke="none"/>')
        # the prediction, as a tick on the D16 bar
        body.append(f'<line x1="{f(bx(d16p))}" y1="{f(y+8)}" x2="{f(bx(d16p))}" '
                    f'y2="{f(y+22)}" class="ink" stroke-width="1.4"/>')

    body.append(f'<rect x="{f(X0)}" y="{f(BY + len(DEPTH_ROWS)*26 + 10)}" width="9" height="9" '
                f'fill="{C_POS}" fill-opacity="0.8"/>')
    body.append(f'<text x="{f(X0+16)}" y="{f(BY + len(DEPTH_ROWS)*26 + 19)}" class="xs">'
                'D32_FLOAT</text>')
    body.append(f'<rect x="{f(X0+110)}" y="{f(BY + len(DEPTH_ROWS)*26 + 10)}" width="9" height="9" '
                f'fill="{C_BAD}" fill-opacity="0.8"/>')
    body.append(f'<text x="{f(X0+126)}" y="{f(BY + len(DEPTH_ROWS)*26 + 19)}" class="xs">'
                'D16_UNORM</text>')
    body.append(f'<line x1="{f(X0+232)}" y1="{f(BY + len(DEPTH_ROWS)*26 + 8)}" '
                f'x2="{f(X0+232)}" y2="{f(BY + len(DEPTH_ROWS)*26 + 22)}" class="ink" '
                'stroke-width="1.4"/>')
    body.append(f'<text x="{f(X0+240)}" y="{f(BY + len(DEPTH_ROWS)*26 + 19)}" class="xs">'
                'what the formula predicts for D16</text>')

    return svg(W, H, "What each depth format can separate, measured",
               "Top: which depth formats this device accepts — asked rather than assumed, and "
               "D24_UNORM, the format a desktop renderer would have hard-coded, is not among "
               "them. Below, the smallest separation each format can actually resolve, found by "
               "drawing a far surface and then a near one and bisecting on the gap. The tick "
               "marks the formula's prediction for D16, which the measurement tracks to within a "
               "few per cent from fifty microns at one metre to forty centimetres at ninety.",
               "\n".join("  " + b for b in body), "f47b")


# =============================================================================
# Figure 3 — reversed-Z
# =============================================================================
def fig3():
    W, H = 720, 414
    body = []

    body.append('<text x="24" y="22" class="sm">'
                'reversed-Z: map near to 1 and far to 0, and a float&#8217;s precision lands '
                'where it is needed</text>')

    # The two mappings, drawn.
    X0, Y0, PW, PH = 60.0, 48.0, 260.0, 130.0
    for k, (title, rev) in enumerate([("ordinary", False), ("reversed", True)]):
        x = X0 + k * (PW + 60)
        body.append(f'<rect x="{f(x)}" y="{f(Y0)}" width="{f(PW)}" height="{f(PH)}" '
                    'class="fill-soft ink" stroke-width="0.75"/>')
        body.append(f'<text x="{f(x)}" y="{f(Y0-8)}" class="xs t-hi">{title}</text>')

        pts = []
        d = NEAR
        while d <= FAR:
            z = z_ndc(d)
            if rev: z = 1.0 - z
            pts.append(f"{f(x + PW*(d/FAR))},{f(Y0 + PH*(1.0-z))}")
            d *= 1.05
        body.append(f'<polyline points="{" ".join(pts)}" class="hi" fill="none" '
                    'stroke-width="1.6"/>')

        # where a float has its precision: near zero
        body.append(f'<rect x="{f(x)}" y="{f(Y0+PH-18)}" width="{f(PW)}" height="18" '
                    f'fill="{C_POS}" fill-opacity="0.22" stroke="none"/>')
        body.append(f'<text x="{f(x+PW/2)}" y="{f(Y0+PH+16)}" class="xs muted" '
                    f'text-anchor="middle">'
                    f'{"far plane lands here &#8212; wasted" if not rev else "NEAR plane lands here"}'
                    '</text>')

    body.append(f'<rect x="{f(X0)}" y="{f(Y0+PH+26)}" width="12" height="9" '
                f'fill="{C_POS}" fill-opacity="0.22" stroke="none"/>')
    body.append(f'<text x="{f(X0+18)}" y="{f(Y0+PH+35)}" class="xs muted">'
                'shaded: the part of the range where a float stores its precision</text>')

    ty = 232
    body.append(f'<rect x="24" y="{ty}" width="{W-48}" height="166" class="fill-soft ink" '
                'stroke-width="0.75" rx="3"/>')
    body.append(f'<text x="40" y="{ty+20}" class="sm">'
                'the same measurement, both ways &#8212; smallest resolvable gap</text>')
    body.append(f'<text x="200" y="{ty+38}" class="xs muted" text-anchor="middle">'
                'D32_FLOAT ordinary</text>')
    body.append(f'<text x="380" y="{ty+38}" class="xs muted" text-anchor="middle">'
                'D32_FLOAT reversed</text>')
    body.append(f'<text x="560" y="{ty+38}" class="xs muted" text-anchor="middle">'
                'D16_UNORM, either</text>')

    for i, ((d, _, _, d32), (_, d16r, d32r)) in enumerate(zip(DEPTH_ROWS, REVERSED)):
        y = ty + 58 + i * 15
        body.append(f'<text x="40" y="{y}" class="xs">{d} m</text>')
        body.append(f'<text x="200" y="{y}" class="xs mono" text-anchor="middle">'
                    f'{d32:.3f} mm</text>')
        body.append(f'<text x="380" y="{y}" class="xs mono t-ok" text-anchor="middle">'
                    f'{d32r:.3f} mm</text>')
        body.append(f'<text x="560" y="{y}" class="xs mono muted" text-anchor="middle">'
                    f'{d16r:.1f} mm</text>')

    body.append(f'<text x="40" y="{ty+150}" class="xs t-hi">'
                'D32_FLOAT gains <tspan class="t-ok">180&#215;</tspan> at ninety metres; '
                'D16_UNORM gains nothing &#8212; its codes are evenly spaced, so which end is '
                'which does not matter.</text>')

    return svg(W, H, "Reversed-Z, measured",
               "A float stores its precision near zero, and the ordinary depth mapping puts the "
               "far plane there — where 1/d squared has already thrown the resolution away. "
               "Reversing the mapping puts the near plane at 1 and the far plane at 0, so the two "
               "effects very nearly cancel. Measured: D32_FLOAT improves from 2.2 mm to 0.012 mm "
               "at ninety metres, a factor of 180, while D16_UNORM is unchanged because its codes "
               "are evenly spaced and reversing them moves nothing.",
               "\n".join("  " + b for b in body), "f47c")


# =============================================================================
# Figure 4 — a texture and a sampler are two objects
# =============================================================================
def fig4():
    W, H = 720, 386
    body = []

    body.append('<text x="24" y="22" class="sm">'
                'Lesson 3.9 passed the filter as an <tspan class="t-hi">argument</tspan>; '
                'the GPU wants an <tspan class="t-hi">object</tspan></text>')

    # Before / after
    body.append('<text x="24" y="52" class="xs muted">Lesson 3.9, on the CPU</text>')
    body.append('<text x="24" y="72" class="xs mono">'
                'sample(image, uv, filter::linear, address_mode::repeat)</text>')

    body.append('<text x="24" y="106" class="xs muted">Lesson 4.7, on the GPU</text>')

    BX, BY, BW_, BH = 24.0, 118.0, 300.0, 54.0
    for k, (title, sub, col) in enumerate([
            ("SDL_GPUTexture", "the pixels &#8212; what is there", C_POS),
            ("SDL_GPUSampler", "the rules &#8212; how to read it", C_UV)]):
        y = BY + k * (BH + 10)
        body.append(f'<rect x="{f(BX)}" y="{f(y)}" width="{f(BW_)}" height="{f(BH)}" '
                    f'fill="{col}" fill-opacity="0.16" stroke="none" rx="3"/>')
        body.append(f'<text x="{f(BX+12)}" y="{f(y+22)}" class="xs mono t-hi">{title}</text>')
        body.append(f'<text x="{f(BX+12)}" y="{f(y+40)}" class="xs muted">{sub}</text>')

    body.append(f'<path d="M{f(BX+BW_+8)},{f(BY+BH/2)} L{f(BX+BW_+34)},{f(BY+BH+10+BH/2 - (BH+10)/2)} '
                f'L{f(BX+BW_+8)},{f(BY+BH+10+BH/2)}" class="ink-soft" fill="none" '
                'stroke-width="0.9"/>')
    body.append(f'<text x="{f(BX+BW_+42)}" y="{f(BY+BH+6)}" class="xs t-hi">'
                'bound as a PAIR</text>')
    body.append(f'<text x="{f(BX+BW_+42)}" y="{f(BY+BH+22)}" class="xs mono">'
                'SDL_GPUTextureSamplerBinding</text>')
    body.append(f'<text x="{f(BX+BW_+42)}" y="{f(BY+BH+38)}" class="xs muted">'
                't0 with s0, in space2</text>')

    # What the object has that the call did not
    ty = 240
    body.append(f'<rect x="24" y="{ty}" width="{W-48}" height="132" class="fill-soft ink" '
                'stroke-width="0.75" rx="3"/>')
    body.append(f'<text x="40" y="{ty+20}" class="sm">'
                'fields SDL_GPUSamplerCreateInfo has that Lesson 3.9&#8217;s call had no room for'
                '</text>')
    fields = [
        ("min_filter / mag_filter", "separately &#8212; the CPU path never minified"),
        ("mipmap_mode, min_lod, max_lod, mip_lod_bias", "Module 6"),
        ("address_mode_u / _v / _w", "three axes; ours wrapped both the same way"),
        ("enable_anisotropy, max_anisotropy", "sampling along the axis of stretch"),
        ("enable_compare, compare_op", "SHADOW sampling &#8212; the comparison happens "
                                       "IN the sampler"),
    ]
    for i, (k, v) in enumerate(fields):
        y = ty + 42 + i * 17
        body.append(f'<text x="40" y="{y}" class="xs mono t-hi">{k}</text>')
        body.append(f'<text x="330" y="{y}" class="xs muted">{v}</text>')

    return svg(W, H, "A texture and a sampler are two objects",
               "On the CPU in Lesson 3.9 the filter and the address mode were arguments to a "
               "function. On the GPU they are an object, created once and bound alongside the "
               "texture as a pair — which is Lesson 4.1's argument arriving for the fourth time, "
               "after pipelines, vertex layouts and uniform blocks. The separation is better than "
               "it looks: one image can be read three ways in one frame by binding it with three "
               "samplers, and one sampler serves every texture in a material system.",
               "\n".join("  " + b for b in body), "f47d")


# =============================================================================
# Figure 5 — orientation, and what _SRGB did
# =============================================================================
def fig5():
    W, H = 720, 380
    body = []

    body.append('<text x="24" y="22" class="sm">'
                'the four corners of the test image, read back out of the GPU</text>')

    CW, CH = 150.0, 56.0
    X0, Y0 = 150.0, 54.0
    heads = ["in the file", "sampled, _UNORM", "sampled, _SRGB"]
    for i, hname in enumerate(heads):
        body.append(f'<text x="{f(X0 + i*CW + CW/2)}" y="{f(Y0-8)}" class="xs muted" '
                    f'text-anchor="middle">{hname}</text>')

    for r, (name, fv, uv, sv) in enumerate(CORNERS):
        y = Y0 + r * (CH + 4)
        body.append(f'<text x="24" y="{f(y+CH/2+4)}" class="xs">{name}</text>')
        for i, v in enumerate((fv, uv, sv)):
            x = X0 + i * CW
            col = "#%02x%02x%02x" % v
            body.append(f'<rect x="{f(x)}" y="{f(y)}" width="{f(CW-8)}" height="{f(CH)}" '
                        f'fill="{col}" stroke="var(--dia-bg)" stroke-width="1"/>')
            body.append(f'<text x="{f(x+(CW-8)/2)}" y="{f(y+CH/2+4)}" class="xs mono t-inv" '
                        f'text-anchor="middle">{v[0]},{v[1]},{v[2]}</text>')

    ny = 296
    body.append(f'<rect x="24" y="{ny}" width="{W-48}" height="72" class="fill-soft ink" '
                'stroke-width="0.75" rx="3"/>')
    body.append(f'<text x="40" y="{ny+20}" class="xs t-ok">'
                'the _UNORM column matches the file EXACTLY, corner for corner</text>')
    body.append(f'<text x="40" y="{ny+36}" class="xs muted">'
                'so nothing between the decoder and the readback reordered a row or a channel'
                '</text>')
    body.append(f'<text x="40" y="{ny+58}" class="xs t-hi">'
                'the _SRGB column is darker: the sampler decoded to linear on the way out, free, '
                'and BEFORE the filter</text>')

    return svg(W, H, "Which way up, and what _SRGB did",
               "The test image carries a differently-coloured block in each corner, so reading "
               "the four corners of the readback answers two questions numerically. Under a "
               "_UNORM format the sampled bytes match the file exactly, which means the decoder, "
               "the upload and the sampler all agreed about which way v runs. Under an _SRGB "
               "format the same texels come back darker, because the sampler performed the sRGB "
               "to linear decode that Lesson 3.9 did by hand per texel — for free, and before "
               "the filter rather than after.",
               "\n".join("  " + b for b in body), "f47e")


# =============================================================================
# Figure 6 — the scene, with and without the depth test
# =============================================================================
def fig6():
    W, H = 720, 316
    body = []
    CROP = (54, 60, 458, 244)     # 404 x 184
    CELL = 4
    PX = 3

    body.append('<text x="24" y="22" class="sm">'
                'the same geometry, the same camera, the same draw call &#8212; '
                '<tspan class="t-hi">three fields of state apart</tspan></text>')

    pal = [hexrgb(c) for c in C_TINT] + [(210, 214, 224), (34, 37, 46), (250, 190, 70)]

    for i, (path, label, cls) in enumerate([
            ("scratch/l47_flat_depth.ppm", "depth test ON", "t-ok"),
            ("scratch/l47_flat_nodepth.ppm", "depth test off", "t-bad")]):
        x = 30 + i * 348
        gw, gh, rects = panel(path, CROP, CELL, x, 40, PX, pal, 4)
        body += rects
        body.append(f'<rect x="{x}" y="40" width="{gw*PX}" height="{gh*PX}" class="ink" '
                    'fill="none" stroke-width="0.75"/>')
        body.append(f'<text x="{x+gw*PX//2}" y="{40+gh*PX+16}" class="sm mono {cls}" '
                    f'text-anchor="middle">{label}</text>')

    body.append(f'<text x="24" y="{H-32}" class="xs">'
                f'<tspan class="t-hi">{DEPTH_CHANGED_PCT}%</tspan> of the covered pixels differ '
                '&#8212; every one of them a surface that should have been hidden</text>')
    body.append(f'<text x="24" y="{H-14}" class="xs muted">'
                'Lesson 4.5 arranged the scene so the tori barely overlapped, which is how this '
                'went unnoticed for two lessons; a lower camera makes it unmissable</text>')

    return svg(W, H, "The depth test, on and off",
               "The same seven tori, the same camera and the same draw call, differing only in "
               "enable_depth_test, enable_depth_write and compare_op — plus one attachment on "
               "the render pass. Without it, whichever instance was drawn later wins, so "
               "background tori punch through the foreground one and the scene reads as broken. "
               "18.9% of the covered pixels differ. Lesson 4.5 arranged its scene so nothing "
               "overlapped, which is how the missing test went unnoticed for two lessons.",
               "\n".join("  " + b for b in body), "f47f")


if __name__ == "__main__":
    write("l47_fig1.svg", fig1())
    write("l47_fig2.svg", fig2())
    write("l47_fig3.svg", fig3())
    write("l47_fig4.svg", fig4())
    write("l47_fig5.svg", fig5())
    write("l47_fig6.svg", fig6())
