#!/usr/bin/env python3
"""scratch/figs_511.py — Lesson 5.11's diagrams.

Same rules as 5.10's, which are the accumulated traps of nine lessons:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - every element kept well inside the viewBox (a label CAN spill; SVG just draws it)
  - no right-anchored label near x = 0, and none on the side the data occupies
  - two adjacent <text> runs must be separated by real space, checked by measuring
  - filenames numbered by PAGE ORDER

Figure 7 embeds a REAL render: build/swarm511.ppm, box-sampled and run-length
encoded. Palette snapping is hue-then-brightness (5.7's trap) or the runs do not
form and the file quadruples.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)
from figs_45 import read_ppm, rle_rects                             # noqa: E402


def peak_sample(data, w, crop, cell):
    """Downsample by taking each block's BRIGHTEST pixel, not its average.

    figs_45.box_sample averages, which is right for a shaded surface and
    catastrophic for a line drawing: a one-pixel debug line inside a 3x3 block
    contributes one ninth of its brightness, so 152 crisp lines average into a
    uniform haze and the figure shows scattered dots instead of a hierarchy.
    (This was not a guess — the averaged version was rendered, looked at, and
    thrown away.)

    Taking the maximum keeps thin bright features on a dark ground at full
    strength, and it keeps the background exactly at the background colour, so
    quantise()'s `< 14` test still returns None for it and the panel stays clean.
    """
    x0, y0, x1, y1 = crop
    gw = (x1 - x0) // cell
    gh = (y1 - y0) // cell
    out = []
    for gy in range(gh):
        row = []
        for gx in range(gw):
            best = (0, 0, 0)
            best_lum = -1
            for dy in range(cell):
                base = ((y0 + gy * cell + dy) * w + (x0 + gx * cell)) * 3
                for dx in range(cell):
                    i = base + dx * 3
                    r, g, b = data[i], data[i + 1], data[i + 2]
                    lum = r * 299 + g * 587 + b * 114
                    if lum > best_lum:
                        best_lum = lum
                        best = (r, g, b)
            # AND A FLOOR, because figs_45.quantise() calls a cell background
            # only when all three channels are under 14 — and this demo's
            # background is (12, 14, 20), whose BLUE channel is 20. Without this
            # every empty cell snapped to the nearest tint at its dimmest step
            # and the panel came out a solid slate blue with the lines invisible
            # inside it. Handing back a true black is how a cell says "nothing
            # here" in the vocabulary the quantiser already speaks.
            row.append(best if best_lum >= 30_000 else (0, 0, 0))
        out.append(row)
    return gw, gh, out

OUT = "scratch"

# ---- Measured inputs, every one of them from a run ---------------------------
QUEUED, DRAWN, DROPPED = 152, 152, 0
ENTITIES, ROOTS, LEVELS = 154, 2, 3
RING, MOONS, WAYPOINTS = 96, 32, 24
CHECKS = 60
IMGUI_VERSION = "1.92.9b"
HUD_LINES_BEFORE = 5
PUBLIC_HEADERS_BEFORE, PUBLIC_HEADERS_AFTER = 54, 57


# ===========================================================================
# Figure 1 — immediate mode vs a queue  (§2)
# ===========================================================================
def fig_rework():
    uid = "l511f1"
    W, H = 720, 430
    b = [label(20, 20, "The same debug line, drawn two ways. Only one of them can be said "
                       "by code that is not the renderer.", "sm", "start")]

    # ---- LEFT: immediate
    b.append(label(24, 52, "BEFORE — line3(fb, a, b, colour, pr)", "xs t-bad", "start"))
    callers = [("collision system", False), ("ECS pass", False),
               ("mesh loader", False), ("the renderer", True)]
    for i, (who, ok) in enumerate(callers):
        y = 70 + i * 34
        colour = GREEN if ok else RED
        b.append(box(24, y, 132, 24, colour=colour, opacity=0.14))
        b.append(label(90, y + 16, who, "xs"))
        if ok:
            b.append(arrow(158, y + 12, 236, y + 12, uid, "g", 1.3))
            b.append(label(197, y + 6, "has both", "xs muted"))
        else:
            b.append(rule(158, y + 12, 214, y + 12, "ink-soft", 1.0, dash="3 3"))
            b.append(label(226, y + 16, "✗", "sm t-bad"))
            b.append(label(238, y + 16, "no framebuffer", "xs muted", "start"))

    b.append(hollow(24, 214, 316, 46, RED, dash="4 3"))
    b.append(label(182, 232, "framebuffer& + const projector&", "xs mono"))
    b.append(label(182, 250, "required AT EVERY CALL SITE", "xs t-bad"))

    for i, t in enumerate(["only the renderer can call it",
                           "framebuffer only — no GPU surface",
                           "a line lives exactly one frame"]):
        b.append(label(24, 284 + i * 20, "✗  " + t, "xs t-bad", "start"))

    b.append(rule(360, 44, 360, 400, "grid", 1.2))

    # ---- RIGHT: the queue
    b.append(label(380, 52, "AFTER — two files, one direction", "xs t-ok", "start"))
    for i, (who, _) in enumerate(callers):
        y = 70 + i * 34
        b.append(box(380, y, 132, 24, colour=GREEN, opacity=0.14))
        b.append(label(446, y + 16, who, "xs"))
        b.append(arrow(514, y + 12, 556, y + 12, uid, "g", 1.2))

    b.append(box(560, 70, 136, 126, colour=AMBER, opacity=0.20))
    b.append(label(628, 96, "debug_lines", "xs mono"))
    b.append(label(628, 114, "world-space", "xs muted"))
    b.append(label(628, 130, "segments +", "xs muted"))
    b.append(label(628, 146, "a lifetime", "xs muted"))
    b.append(label(628, 172, "bounded, counted", "xs muted"))

    b.append(arrow(628, 200, 628, 220, uid, "h", 1.3))
    b.append(label(628, 238, "one flush, per camera", "xs muted"))

    # The two diverging arrows start BELOW the caption above them. They used to
    # start at y = 232 and ran straight through it — invisible to a spill check,
    # obvious to check-page.js's on-shape test, and ugly to a reader.
    b.append(box(392, 262, 140, 26, colour=BLUE, opacity=0.18))
    b.append(label(462, 279, "framebuffer", "xs mono"))
    b.append(box(556, 262, 140, 26, colour=GREY, opacity=0.14))
    b.append(label(626, 279, "SDL_GPU (M6)", "xs mono"))
    b.append(arrow(596, 246, 500, 258, uid, "s", 1.1))
    b.append(arrow(660, 246, 648, 258, uid, "s", 1.1))

    for i, t in enumerate(["anybody queues; the renderer draws",
                           "one queue, any number of backends",
                           "a line can outlive its frame"]):
        b.append(label(380, 318 + i * 20, "✓  " + t, "xs t-ok", "start"))

    b.append(label(24, 396, "The queue is not an optimisation and makes nothing faster. It "
                            "removes an ARGUMENT from every call site —", "xs t-hi", "start"))
    b.append(label(24, 414, "and that argument was the reason the caller had to be the "
                            "renderer.", "xs t-hi", "start"))

    return svg(uid, W, H, "Immediate-mode debug drawing compared with a queue-then-flush design",
               "On the left, line3 needs a framebuffer and a projector at every call, so only "
               "the renderer can call it, it works on one surface, and a line lives one frame. "
               "On the right, four different systems queue world-space segments into "
               "debug_lines, and one flush per camera hands them to a backend.", b)


# ===========================================================================
# Figure 2 — the include lists ARE the design  (§3)
# ===========================================================================
def fig_includes():
    uid = "l511f2"
    W, H = 720, 400
    b = [label(20, 20, "Why this is two headers and not one. A header is a contract about "
                       "what you are forced to compile against.", "sm", "start")]

    left = ["engine/gfx/colour.hpp", "engine/math/mat4.hpp", "engine/math/vec3.hpp",
            "<span, vector, cstdint>"]
    right = ["debug_lines.hpp", "gfx/depth_buffer.hpp", "gfx/framebuffer.hpp",
             "gfx/mesh.hpp", "gfx/projector.hpp", "gfx/viewport.hpp"]

    b.append(box(30, 48, 300, 26, colour=AMBER, opacity=0.24))
    b.append(label(180, 66, "gfx/debug_lines.hpp   —   SAYS", "xs mono"))
    for i, inc in enumerate(left):
        b.append(box(46, 84 + i * 26, 268, 20, colour=AMBER, opacity=0.09))
        b.append(label(180, 98 + i * 26, inc, "xs mono"))
    b.append(label(180, 208, "maths, a colour, the standard library", "xs muted"))
    b.append(label(180, 226, "— and nothing that can draw", "xs t-ok"))

    b.append(box(390, 48, 300, 26, colour=BLUE, opacity=0.24))
    b.append(label(540, 66, "gfx/debug_draw.hpp   —   DOES", "xs mono"))
    for i, inc in enumerate(right):
        opacity = 0.20 if i == 0 else 0.09
        b.append(box(406, 84 + i * 26, 268, 20, colour=BLUE, opacity=opacity))
        b.append(label(540, 98 + i * 26, inc, "xs mono"))
    b.append(label(540, 250, "everything the renderer already has", "xs muted"))

    b.append(arrow(330, 96, 400, 96, uid, "h", 1.4))
    b.append(label(365, 84, "includes", "xs muted"))

    b.append(rule(30, 276, 690, 276, "grid", 1.2))

    b.append(label(30, 300, "THE DIRECTION IS THE WHOLE DESIGN, AND IT ONLY GOES ONE WAY.",
                   "xs t-hi", "start"))
    b.append(label(30, 320, "Module 8's collision code includes the left-hand file and "
                            "compiles against four headers. Put ONE renderer type in it —",
                   "xs muted", "start"))
    b.append(label(30, 336, "a framebuffer, a projector, even `mesh`, which drags in the "
                            "handle system — and every physics translation unit", "xs muted",
                   "start"))
    b.append(label(30, 352, "compiles the renderer in order to draw a box.", "xs muted",
                   "start"))
    b.append(label(30, 378, "This is why wire_mesh() takes two spans instead of the `mesh` "
                            "that owns them. Not fussiness: the only way to", "xs t-hi",
                   "start"))
    b.append(label(30, 394, "keep the promise above.", "xs t-hi", "start"))

    return svg(uid, W, H, "The include lists of the two debug headers, side by side",
               "debug_lines.hpp includes only colour, mat4, vec3 and three standard headers. "
               "debug_draw.hpp includes debug_lines.hpp plus the depth buffer, framebuffer, "
               "mesh, projector and viewport. The dependency runs one way only.", b)


# ===========================================================================
# Figure 3 — lifetimes and the order of the frame  (§4)
# ===========================================================================
def fig_lifetime():
    uid = "l511f3"
    W, H = 720, 426
    b = [label(20, 20, "Three lines with three lifetimes, across five frames — and the "
                       "ordering that deletes all of them.", "sm", "start")]

    FX0, FW = 150, 104
    frames = 5
    for i in range(frames):
        x = FX0 + i * FW
        b.append(rule(x, 44, x, 300, "grid", 1.0, dash="2 4"))
        b.append(label(x + FW / 2, 58, f"frame {i}", "xs muted"))
    b.append(rule(FX0 + frames * FW, 44, FX0 + frames * FW, 300, "grid", 1.0, dash="2 4"))

    rows = [
        # The in-bar text must FIT THE BAR, which no geometry check can see: a
        # label that overflows its own box is still inside the viewBox, on top of
        # nothing, overlapping nothing. Span 1 is 92 units — about 17 characters.
        ("seconds = 0", AMBER, 1, "drawn once"),
        ("seconds = 0.05", BLUE, 4, "3 frames of time + 1"),
        ("seconds = 2.0", GREEN, 5, "still there when you look"),
    ]
    for r, (name, colour, span, note) in enumerate(rows):
        y = 78 + r * 54
        b.append(label(140, y + 16, name, "xs mono", "end"))
        b.append(box(FX0 + 6, y, span * FW - 12, 22, colour=colour, opacity=0.26))
        b.append(label(FX0 + (span * FW) / 2, y + 16, note, "xs"))
        if span < frames:
            b.append(label(FX0 + span * FW + 8, y + 16, "expired", "xs muted", "start"))

    b.append(rule(30, 250, 690, 250, "grid", 1.2))
    b.append(label(30, 272, "THE EXPIRY RULE, AND WHY THE TEST COMES FIRST", "xs t-hi", "start"))

    b.append(box(40, 286, 300, 44, colour=RED, opacity=0.10, dash="3 3"))
    b.append(label(190, 304, "remaining -= dt;  if (r <= 0) drop;", "xs mono"))
    b.append(label(190, 322, "dt == 0  →  a one-frame line NEVER dies", "xs t-bad"))

    b.append(box(380, 286, 300, 44, colour=GREEN, opacity=0.10))
    b.append(label(530, 304, "if (r <= 0) drop;  remaining -= dt;", "xs mono"))
    b.append(label(530, 322, "dt == 0  →  drops anyway. Correct.", "xs t-ok"))

    b.append(label(30, 356, "dt is zero more often than you think: a paused clock, a "
                            "single-frame --shot, a breakpoint. Those are exactly the",
                   "xs muted", "start"))
    b.append(label(30, 372, "moments you are looking hardest, and the wrong rule leaks "
                            "without limit in precisely them.", "xs muted", "start"))
    b.append(label(30, 400, "AND THE ORDER: queue → flush → advance. Age first and every "
                            "default-lifetime line dies before it is drawn.",
                   "xs t-hi", "start"))

    return svg(uid, W, H, "Debug line lifetimes across five frames, and the expiry rule",
               "A zero-second line spans one frame, a 0.05-second line spans four, and a "
               "two-second line spans all five. Below, the obvious expiry rule leaks when dt "
               "is zero and the shipped rule does not.", b)


# ===========================================================================
# Figure 4 — where ImGui sits in the frame  (§5)
# ===========================================================================
def fig_uiframe():
    uid = "l511f4"
    W, H = 720, 446
    b = [label(20, 20, "Where each ImGui call goes — and the one-frame bug that comes of "
                       "moving one of them.", "sm", "start")]

    steps = [
        ("SDL_AppEvent × N", "handle_event(e)", GREY, "ImGui learns what happened"),
        ("begin_frame()", "platform: clock + input", GREY, ""),
        ("on_input()", "ui.begin_frame()", AMBER, "NewFrame → the capture flags exist"),
        ("", "gate.update(in(), …)", AMBER, "and are read HERE, one line later"),
        ("", "actions.update(gate)", AMBER, ""),
        ("on_fixed_step() × 0..N", "the simulation", GREY, ""),
        ("on_frame(alpha)", "the scene + the flush", GREY, ""),
        ("blit_framebuffer()", "our pixels reach the window", GREY, ""),
        ("on_overlay()", "ImGui::Begin … End", BLUE, "the panels"),
        ("", "ui.render()", BLUE, "over the frame, under the vsync wait"),
        ("present()", "", GREY, ""),
    ]
    y = 50
    for hook, what, colour, note in steps:
        if hook:
            b.append(label(140, y + 15, hook, "xs mono", "end"))
        if what:
            b.append(box(152, y, 210, 22, colour=colour,
                         opacity=0.22 if colour is not AMBER else 0.26))
            b.append(label(257, y + 15, what, "xs mono"))
        if note:
            b.append(label(376, y + 15, note, "xs muted", "start"))
        y += 30

    b.append(hollow(148, 108, 218, 92, AMBER, dash="4 3"))

    b.append(rule(30, 380, 690, 380, "grid", 1.2))
    b.append(label(30, 402, "MOVE ui.begin_frame() DOWN INTO on_overlay(), next to the "
                            "panels, and every read of wants_keyboard()", "xs t-bad", "start"))
    b.append(label(30, 418, "answers about the PREVIOUS frame: the first keystroke after "
                            "you click into a text field also reaches the", "xs muted",
                   "start"))
    b.append(label(30, 434, "game. One frame, every time, and invisible unless you look for "
                            "it.", "xs muted", "start"))

    return svg(uid, W, H, "The ImGui calls placed in the engine's frame",
               "handle_event runs per event; ui.begin_frame is the first thing in on_input, "
               "before the input gate and the action map read the capture flags; the panels "
               "and ui.render run in on_overlay, after the framebuffer blit.", b)


# ===========================================================================
# Figure 5 — two consumers, one keyboard  (§6)
# ===========================================================================
def fig_seam():
    uid = "l511f5"
    W, H = 720, 400
    b = [label(20, 20, "Two consumers of one keyboard, and the layer at which they are "
                       "separated.", "sm", "start")]

    b.append(box(280, 46, 160, 26, colour=GREY, opacity=0.20))
    b.append(label(360, 64, "SDL_Event", "xs mono"))

    b.append(arrow(330, 74, 250, 100, uid, "h", 1.3))
    b.append(arrow(392, 74, 470, 100, uid, "h", 1.3))

    b.append(box(150, 104, 200, 26, colour=BLUE, opacity=0.22))
    b.append(label(250, 122, "engine::input", "xs mono"))
    b.append(box(390, 104, 200, 26, colour=PURPLE, opacity=0.22))
    b.append(label(490, 122, "ImGui (via handle_event)", "xs mono"))

    b.append(label(360, 148, "BOTH SEE EVERY EVENT. Nothing is routed away.", "xs t-ok"))

    b.append(arrow(250, 158, 250, 196, uid, "s", 1.2))
    b.append(arrow(490, 158, 300, 196, uid, "s", 1.2))
    # Right-anchored, clear of the diagonal it labels: that arrow only exists for
    # x <= 490, so anything starting right of it cannot sit on it.
    b.append(label(692, 178, "wants_keyboard() / wants_mouse()", "xs muted", "end"))

    b.append(box(150, 200, 300, 30, colour=AMBER, opacity=0.26))
    b.append(label(300, 220, "masked_input<input>", "xs mono"))
    b.append(label(300, 246, "a DIFFERENT TYPE that satisfies input_snapshot", "xs muted"))

    b.append(arrow(300, 258, 300, 288, uid, "h", 1.3))
    b.append(box(150, 292, 300, 26, colour=GREEN, opacity=0.22))
    b.append(label(300, 310, "action_map::update(Source)", "xs mono"))
    b.append(label(300, 334, "unchanged, to the character, from Lesson 5.10", "xs t-ok"))

    b.append(hollow(470, 196, 220, 96, RED, dash="4 3"))
    b.append(label(580, 216, "THE TEMPTING WRONG FIX", "xs t-bad"))
    b.append(label(580, 236, "hand the event to the UI and", "xs muted"))
    b.append(label(580, 252, "stop if it takes one. Steal a", "xs muted"))
    b.append(label(580, 268, "key-UP and `input` believes", "xs muted"))
    b.append(label(580, 284, "the key is held. Forever.", "xs muted"))

    b.append(label(30, 366, "ARBITRATE ON LEVELS, NOT ON EVENTS. A level is corrected only "
                            "by the event that contradicts it,", "xs t-hi", "start"))
    b.append(label(30, 384, "so an event withheld is a level stuck.", "xs t-hi", "start"))

    return svg(uid, W, H, "One event stream feeding both engine input and ImGui, arbitrated by a mask",
               "Every SDL event reaches both engine::input and ImGui. The capture flags feed "
               "masked_input, which wraps engine::input and satisfies the same concept, so "
               "action_map::update is unchanged.", b)


# ===========================================================================
# Figure 6 — the virtual cursor  (§6)
# ===========================================================================
def fig_cursor():
    uid = "l511f6"
    W, H = 720, 430
    b = [label(20, 20, "You cannot mask a DELTA by masking one endpoint of it. The numbers "
                       "are the ones verify_511 §E asserts.", "sm", "start")]

    real = [100, 120, 145, 160, 170, 175]
    blocked = [False, True, True, True, False, False]

    CX0, CW = 190, 88
    for i, x in enumerate(real):
        cx = CX0 + i * CW
        b.append(label(cx, 58, f"f{i}", "xs muted"))
        b.append(label(cx, 78, str(x), "xs mono"))
        if blocked[i]:
            b.append(box(cx - 34, 86, 68, 12, colour=PURPLE, opacity=0.30))
    b.append(label(178, 78, "real cursor x", "xs muted", "end"))
    b.append(label(CX0 + 2 * CW, 108, "UI owns the mouse", "xs muted"))

    rows = [
        ("report 0 while blocked", RED,
         [None, 0, 0, 0, 170, 5], "the whole screen, in one frame"),
        ("freeze the last position", RED,
         [None, 0, 0, 0, 70, 5], "the whole excursion — this is the one that ships"),
        ("virtual cursor (ours)", GREEN,
         [None, 0, 0, 0, 10, 5], "one frame of real movement. Correct at both boundaries."),
    ]
    y = 128
    for name, colour, deltas, note in rows:
        b.append(label(178, y + 16, name, "xs mono", "end"))
        for i, d in enumerate(deltas):
            if d is None:
                continue
            cx = CX0 + i * CW
            hot = (i == 4)
            b.append(box(cx - 30, y, 60, 22, colour=colour,
                         opacity=0.30 if hot else 0.10))
            b.append(label(cx, y + 16, f"{d:+d}", "xs mono"))
        b.append(label(190 - 12 + 5 * CW + 40, y + 16, "", "xs muted", "start"))
        b.append(label(30, y + 42, "   " + note, "xs muted", "start"))
        y += 74

    b.append(rule(30, 336, 690, 336, "grid", 1.2))
    b.append(label(30, 358, "reported = real − offset,   and offset += (real − "
                            "real_previous) on every blocked frame.", "xs mono", "start"))
    b.append(label(30, 380, "While blocked the reported position stops dead, so the delta "
                            "is exactly zero. The offset stops GROWING the", "xs muted",
                   "start"))
    b.append(label(30, 396, "instant the block lifts, so the next difference is one frame's "
                            "real movement, not the whole excursion.", "xs muted", "start"))
    b.append(label(30, 412, "The cursor stays 60 px behind forever, and is permanently right "
                            "about how far it moved.", "xs muted", "start"))

    return svg(uid, W, H, "Three ways to withhold the mouse, compared on the frame the block lifts",
               "The cursor travels from 100 to 160 over three blocked frames and on to 170. "
               "Reporting zero gives a delta of 170 on the release frame; freezing the position "
               "gives 70; the virtual cursor gives 10, which is one frame of real movement.", b)


# ===========================================================================
# Figure 7 — the result  (§9)
# ===========================================================================
def fig_result():
    uid = "l511f7"
    W, H = 720, 438
    b = [label(20, 20, "The claim, made checkable — every line is one entity's parent "
                       "link.", "sm", "start")]

    # A real render, box-sampled and run-length encoded. Palette: the link blue,
    # the sun's amber, and the pale moons — hue first, then brightness (5.7).
    # 2-pixel cells at 2 units each: the same 360-unit panel as a 3/3 pair, at
    # twice the resolution. A one-pixel line survives either way now that the
    # sampling takes peaks, but at CELL = 3 the radial lines merge into wedges
    # near the sun, and the thing the figure is FOR is that they are separate.
    CELL = 2
    PX = 2
    PANEL_X, PANEL_Y = 24, 44
    w, h, data = read_ppm("build/swarm511.ppm")
    gw, gh, grid = peak_sample(data, w, (60, 6, 420, 264), CELL)
    # Five tints and two brightness steps. More tints would keep the objects'
    # identities and destroy the run lengths; fewer would turn the sun the same
    # colour as the links, which is the one distinction this figure is for.
    rects = rle_rects(grid, gw, gh, PANEL_X, PANEL_Y, PX,
                      [(120, 190, 255),    # the parent links
                       (232, 184, 76),     # the sun
                       (224, 122, 60),     # warm ring members
                       (140, 200, 230),    # cool ring members
                       (230, 232, 240)],   # the moons
                      levels=2)
    # THE PANEL IS DARK, EXPLICITLY. rle_rects() fills it with `fill-soft`, which
    # is a theme colour — mid-slate here — and the link blue vanished into it
    # completely. This is a screenshot of a dark program, so the ground is the
    # program's own background (12, 14, 20) in both themes, and the thin bright
    # lines have something to be bright against.
    rects[0] = (f'<rect x="{PANEL_X}" y="{PANEL_Y}" width="{gw*PX}" '
                f'height="{gh*PX}" fill="#0c0e14" stroke="none"/>')
    b += [r.replace(f'height="{PX}"', f'height="{PX + 0.6}"') for r in rects]
    b.append(f'<rect x="{PANEL_X}" y="{PANEL_Y}" width="{gw*PX}" height="{gh*PX}" '
             'fill="none" class="grid" stroke-width="1"/>')
    b.append(label(PANEL_X, PANEL_Y + gh * PX + 18,
                   f"{gw}×{gh} cells of the 480×270 framebuffer, run-length encoded",
                   "xs muted", "start"))

    tx = PANEL_X + gw * PX + 30
    notes = [
        (f"{QUEUED} lines queued", f"{RING} ring + {MOONS} moons + {WAYPOINTS} waypoints"),
        (f"{DRAWN} drawn, {DROPPED} dropped", "printed by --shot, so this figure is checkable"),
        (f"{ENTITIES} entities, {ROOTS} roots", "the sun and the camera have no parent"),
        ("0 framebuffers", "in hierarchy_debug_system's signature"),
    ]
    y = 76
    for head, note in notes:
        b.append(label(tx, y, head, "sm t-hi", "start"))
        b.append(label(tx, y + 17, note, "xs muted", "start"))
        y += 48

    b.append(label(tx, y + 6, "152 = 96 + 32 + 24, and the arithmetic", "xs muted", "start"))
    b.append(label(tx, y + 22, "is the point: a debug view whose count", "xs muted", "start"))
    b.append(label(tx, y + 38, "you cannot derive is a debug view you", "xs muted", "start"))
    b.append(label(tx, y + 54, "cannot trust.", "xs muted", "start"))

    b.append(rule(24, 366, 696, 366, "grid", 1.2))
    b.append(label(24, 390, "Every line converging on the middle is a ring member or a "
                            "waypoint placed relative to the sun. The short ones",
                   "xs muted", "start"))
    b.append(label(24, 406, "further out are moons, two composes deep. Press [H] and half "
                            "of them vanish — which is what “detached” has", "xs muted",
                   "start"))
    b.append(label(24, 422, "meant since Lesson 5.9, finally visible rather than asserted.",
                   "xs muted", "start"))

    return svg(uid, W, H, "The ecs_swarm hierarchy drawn as debug lines",
               "A run-length-encoded render of the demo's headless shot: 152 debug lines run "
               "from every parented entity to its parent, converging on the sun at the centre, "
               "with short links from each moon to its planet.", b)


FIGS = {
    "l511_fig1.svg": fig_rework,     # §2
    "l511_fig2.svg": fig_includes,   # §3
    "l511_fig3.svg": fig_lifetime,   # §4
    "l511_fig4.svg": fig_uiframe,    # §5
    "l511_fig5.svg": fig_seam,       # §6
    "l511_fig6.svg": fig_cursor,     # §6
    "l511_fig7.svg": fig_result,     # §9
}


def main():
    for name, fn in FIGS.items():
        with open(os.path.join(OUT, name), "w") as fh:
            fh.write(fn())
        size = os.path.getsize(os.path.join(OUT, name))
        print(f"wrote {os.path.join(OUT, name)}  ({size:,} bytes)")


if __name__ == "__main__":
    main()
