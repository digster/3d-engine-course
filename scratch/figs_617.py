#!/usr/bin/env python3
"""scratch/figs_617.py — Lesson 6.17's diagrams.

Same rules as 6.1-6.16's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - ~5.2 units per character for `xs`, ~6.0 for `sm`
  - filenames numbered by PAGE ORDER
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - `rule()` takes a CSS CLASS; `cline()` takes a COLOUR (6.10's trap)
  - LEGENDS AND ANNOTATION BOXES GO OUTSIDE THE PLOT (6.11/6.13/6.15's trap)
  - a SHAPE can leave the viewBox where a label cannot

Every number comes from verify_617's output.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)
from figs_610 import cline                                          # noqa: E402

OUT = "scratch"

# ---- measured (verify_617) -------------------------------------------------
CHECKS = 40

# §A — the inventory
FRAME_PASSES = 17
BLOOM_PASSES = 11
CASCADES = 4

# the harness's frame
LIVE = 14
CULLED = 11

# §G — memory, 256x256
NAIVE = 1223296
PEAK = 1048576
POOLED = 1223296
PRIZE = NAIVE - PEAK
PRIZE_PC = 100.0 * PRIZE / NAIVE

# §G — 1920x1080 with a 2048 shadow map (MiB)
BIG_NAIVE = 45.00
BIG_PEAK = 39.73
BIG_PRIZE = 5.27

# §G — the lifetime table (name, bytes, first, last)
LIFE = [
    ("shadow",     262144, 0,  1),
    ("hdr",        524288, 1, 13),
    ("depth",      262144, 1,  1),
    ("bloom 0",    131072, 2, 13),
    ("bloom 1",     32768, 3, 12),
    ("bloom 2",      8192, 4, 11),
    ("bloom 3",      2048, 5, 10),
    ("bloom 4",       512, 6,  9),
    ("bloom 5",       128, 7,  8),
]

# §H — the compile
COMPILE_US = 8.62
COMPILE_FIRST_US = 15.83
ALLOCS = 0
ACCESSES = 29

# §I — the golden
CHANNELS = 262144
CONTROL_DIFF = 196608


# ===========================================================================
# Figure 1 — the frame today: seventeen passes, four facts kept by hand  (§1)
# ===========================================================================
def fig_today():
    uid = "l617f1"
    W, H = 760, 430
    b = [label(20, 22, "One frame, seventeen render passes, and nothing that knows the order.",
               "sm", "start")]

    # The frame as a strip of passes, grouped by who begins them.
    groups = [
        ("gpu_shadow_map::render", CASCADES, AMBER, "begins its own"),
        ("gpu_scene_renderer::render", 1, BLUE, "handed one"),
        ("gpu_bloom::render", BLOOM_PASSES, PURPLE, "begins eleven"),
        ("gpu_post_stack::resolve_into", 1, GREEN, "handed one"),
    ]

    x0, y0 = 34, 74
    total = sum(g[1] for g in groups)
    slot = (W - 2 * x0) / total
    x = x0
    for name, n, colour, who in groups:
        w = slot * n
        b.append(box(x, y0, w - 4, 34, colour=colour, opacity=0.16))
        b.append(hollow(x, y0, w - 4, 34, colour, width=1.1))
        cx = x + (w - 4) / 2
        b.append(label(cx, y0 - 10, f"{n}", "xs t-hi"))
        # Long names wrap badly inside a narrow box; put them under the strip.
        x += w

    # Owner labels below, staggered so the two narrow ones do not collide.
    #
    # CLAMPED, because `gpu_post_stack::resolve_into` is 28 characters centred
    # over a slot one seventeenth of the strip wide, and centring it there put
    # 21 px of it outside the viewBox. check-page.js caught that; the fix is to
    # keep the label inside and let the leader line do the pointing.
    x = x0
    rows = [0, 1, 0, 1]
    for (name, n, colour, who), row in zip(groups, rows):
        w = slot * n
        cx = x + (w - 4) / 2
        yy = y0 + 52 + row * 30
        half = max(len(name), len(who)) * 5.2 / 2.0
        tx = min(max(cx, x0 + half), W - x0 - half)
        b.append(rule(cx, y0 + 36, cx, yy - 18, "grid", 1.0))
        b.append(rule(cx, yy - 18, tx, yy - 12, "grid", 1.0))
        b.append(label(tx, yy, name, "xs mono"))
        b.append(label(tx, yy + 12, who, "xs muted"))
        x += w

    b.append(label(x0, y0 + 128,
                   "FOUR FACTS, EACH WRITTEN DOWN BY A HUMAN AND CHECKED BY NOTHING",
                   "xs t-bad", "start"))

    facts = [
        ("the ORDER",
         "lives in whichever function assembles the frame, plus prose in three headers"),
        ("every LOAD OP",
         "DONT_CARE / CLEAR / LOAD, chosen per attachment; 6.13 called one 'load-bearing'"),
        ("every STORE OP",
         "4.7 chose DONT_CARE for the scene depth; 6.8 had to choose STORE for the map"),
        ("which passes RUN",
         "gpu_post.cpp:562 — `if (!s.enabled) { return; }`, a second copy of a dependency"),
    ]
    for i, (title, note) in enumerate(facts):
        yy = y0 + 150 + i * 34
        b.append(box(x0, yy - 13, 150, 20, colour=RED, opacity=0.10, rx=3))
        b.append(label(x0 + 75, yy, title, "xs"))
        b.append(label(x0 + 162, yy, note, "xs muted", "start"))

    b.append(rule(x0, y0 + 292, W - x0, y0 + 292, "grid", 1.2))
    b.append(label(x0, y0 + 312,
                   "All four are consequences of ONE thing each pass could say instead: "
                   "what it reads, and what it writes.",
                   "xs t-ok", "start"))

    return svg(uid, W, H,
               "The engine's frame today: seventeen render passes across four owners",
               "A strip of seventeen render passes grouped by the type that records them, "
               "above a list of the four facts a human currently maintains by hand: the "
               "pass order, every load op, every store op, and which passes run at all.",
               b)


# ===========================================================================
# Figure 2 — a resource is a sequence of values  (§3)
# ===========================================================================
def fig_versions():
    uid = "l617f2"
    W, H = 760, 380
    b = [label(20, 22, "A resource is not a texture. It is a variable, and a variable has "
                       "values over time.", "sm", "start")]

    # Top: the wrong mental model.
    b.append(label(34, 54, "THE TEXTURE VIEW — one object, mutated in place", "xs t-bad",
                   "start"))
    b.append(box(180, 66, 400, 26, colour=GREY, opacity=0.14))
    b.append(label(380, 83, "SDL_GPUTexture* hdr", "xs mono"))
    for i, who in enumerate(("scene writes it", "skybox writes it", "resolve reads it")):
        xx = 240 + i * 160
        b.append(arrow(xx, 104, xx, 94, uid, "s", 1.1))
        b.append(label(xx, 118, who, "xs muted"))
    b.append(label(380, 140,
                   "Nothing here distinguishes 'the image before the skybox' from "
                   "'the image after it'.", "xs muted"))

    b.append(rule(34, 160, W - 34, 160, "grid", 1.2))

    # Bottom: versions.
    b.append(label(34, 184, "THE VERSION VIEW — three values, and each has one producer",
                   "xs t-ok", "start"))

    stops = [
        ("hdr@0", "undefined", GREY, None),
        ("hdr@1", "what the scene pass produced", BLUE, "scene"),
        ("hdr@2", "what the skybox produced from hdr@1", AMBER, "skybox"),
    ]
    y = 216
    for i, (name, note, colour, producer) in enumerate(stops):
        xx = 90 + i * 230
        b.append(box(xx - 44, y, 88, 26, colour=colour, opacity=0.18))
        b.append(hollow(xx - 44, y, 88, 26, colour, width=1.1))
        b.append(label(xx, y + 17, name, "xs mono"))
        b.append(label(xx, y + 44, note, "xs muted"))
        if i < 2:
            b.append(arrow(xx + 50, y + 13, xx + 178, y + 13, uid, "i", 1.3))
            b.append(label(xx + 114, y + 8, stops[i + 1][3], "xs t-hi"))

    b.append(label(34, 310,
                   "Now 'the bloom samples hdr@1' is a statement a compiler can act on. "
                   "It fixes the ORDER (whoever", "xs", "start"))
    b.append(label(34, 326,
                   "produced hdr@1 runs first), the STORE OP (hdr@1 has a reader, so it "
                   "must be written out), and the", "xs", "start"))
    b.append(label(34, 342,
                   "LIFETIME (hdr is live from the scene pass to the bloom). One sentence, "
                   "three deductions.", "xs", "start"))

    return svg(uid, W, H,
               "A resource as a sequence of versions rather than a mutable texture",
               "Above, a single texture pointer written by three passes with no way to name "
               "the intermediate states. Below, the same resource as three versions - hdr@0 "
               "undefined, hdr@1 from the scene pass, hdr@2 from the skybox - each with "
               "exactly one producer.",
               b)


# ===========================================================================
# Figure 3 — one statement, four derived facts  (§4)
# ===========================================================================
def fig_deduction():
    uid = "l617f3"
    W, H = 760, 400
    b = [label(20, 22, "What follows from `keep`, and why it is not four decisions.",
               "sm", "start")]

    # The statement, centre-left.
    sx, sy = 40, 150
    b.append(box(sx, sy, 250, 62, colour=AMBER, opacity=0.16))
    b.append(hollow(sx, sy, 250, 62, AMBER, width=1.3))
    b.append(label(sx + 125, sy + 24, "level[i-1] = fg.keep(up, level[i-1])", "xs mono"))
    b.append(label(sx + 125, sy + 44, '"my output depends on what', "xs muted"))
    b.append(label(sx + 125, sy + 56, 'was already in this target"', "xs muted"))

    facts = [
        ("ORDERING", GREEN,
         "whoever produced level[i-1]'s current",
         "version must run before this pass"),
        ("LOAD OP", BLUE,
         "SDL_GPU_LOADOP_LOAD, because the old",
         "contents are an operand of the blend"),
        ("STORE OP", PURPLE,
         "the producer's store becomes STORE:",
         "this pass is a reader of its version"),
        ("LIFETIME", RED,
         "level[i-1] is live from its producer",
         "through this pass, so the pool cannot reuse it"),
    ]
    for i, (title, colour, l1, l2) in enumerate(facts):
        yy = 52 + i * 82
        bx = 380
        b.append(box(bx, yy, 340, 62, colour=colour, opacity=0.12))
        b.append(hollow(bx, yy, 340, 62, colour, width=1.1))
        b.append(label(bx + 12, yy + 20, title, "xs", "start"))
        b.append(label(bx + 12, yy + 38, l1, "xs muted", "start"))
        b.append(label(bx + 12, yy + 52, l2, "xs muted", "start"))
        b.append(arrow(sx + 256, sy + 31, bx - 6, yy + 31, uid, "s", 1.1))

    return svg(uid, W, H,
               "Four derived facts from one dataflow statement",
               "The declaration `keep` - this pass's output depends on what was already in "
               "the target - fanning out into four consequences the compiler derives: the "
               "ordering edge, the LOAD op, the producer's STORE op, and the resource's "
               "lifetime.",
               b)


# ===========================================================================
# Figure 4 — the worked example: cull, then order  (§5, §6)
# ===========================================================================
def fig_worked():
    uid = "l617f4"
    W, H = 760, 404
    b = [label(20, 22, "Three passes, two resources, and every answer checkable by reading.",
               "sm", "start")]

    # Declaration, left.
    b.append(label(34, 52, "DECLARED", "xs", "start"))
    decls = [
        ("blur",    "writes  tmp@0 -> tmp@1", BLUE),
        ("combine", "samples tmp@1;  writes out@0 -> out@1", GREEN),
        ("unused",  "writes  tmp@1 -> tmp@2", RED),
    ]
    for i, (name, what, colour) in enumerate(decls):
        yy = 68 + i * 30
        b.append(box(34, yy, 76, 22, colour=colour, opacity=0.16))
        b.append(label(72, yy + 15, name, "xs mono"))
        b.append(label(122, yy + 15, what, "xs mono", "start"))

    b.append(label(34, 176, "`out` is IMPORTED — somebody else owns it, so its value is the "
                            "only thing", "xs muted", "start"))
    b.append(label(34, 190, "observable after the frame ends. That makes it the cull root.",
                   "xs muted", "start"))

    b.append(rule(34, 210, W - 34, 210, "grid", 1.2))

    # The graph, right-ish.
    b.append(label(34, 234, "REACHABILITY, BACKWARDS FROM THE IMPORT", "xs", "start"))

    nodes = [("blur", 130, 282, BLUE, True),
             ("combine", 340, 282, GREEN, True),
             ("unused", 560, 282, RED, False)]
    for name, cx, cy, colour, alive in nodes:
        b.append(box(cx - 52, cy - 18, 104, 36,
                     colour=colour, opacity=0.18 if alive else 0.06,
                     dash=None if alive else "4 3"))
        b.append(hollow(cx - 52, cy - 18, 104, 36, colour, width=1.2,
                        dash=None if alive else "4 3"))
        b.append(label(cx, cy + 4, name, "xs mono"))
        b.append(label(cx, cy + 32, "LIVE" if alive else "CULLED",
                       "xs " + ("t-ok" if alive else "t-bad")))

    b.append(arrow(182, 282, 286, 282, uid, "i", 1.3))
    b.append(label(234, 274, "tmp@1", "xs mono"))
    b.append(arrow(392, 274, 452, 256, uid, "g", 1.3))
    b.append(label(448, 250, "out@1  (imported)", "xs mono", "start"))

    # unused's dependence on blur, drawn faintly - it exists but leads nowhere.
    # BELOW the LIVE/CULLED row, not level with it: at y=332 the dashed line and
    # the word LIVE landed on each other and check-page.js said so.
    b.append(cline(182, 336, 508, 336, GREY, 1.0, dash="4 3"))
    b.append(label(345, 354, "tmp@1 — a real edge, and it leads nowhere", "xs muted"))

    b.append(label(34, 384,
                   "`unused` writes tmp@2, and nothing reads tmp@2. It is not scheduled, "
                   "so nothing it reads is either.", "xs", "start"))

    return svg(uid, W, H,
               "Culling and ordering a three-pass graph",
               "Three declared passes - blur, combine and unused - with the imported "
               "resource `out` as the cull root. Backward reachability keeps blur and "
               "combine and drops unused, whose only output nobody reads.",
               b)


# ===========================================================================
# Figure 5 — lifetimes, and the three memory numbers  (§7)
# ===========================================================================
def fig_memory():
    uid = "l617f5"
    W, H = 760, 470
    b = [label(20, 22, "Every transient's lifetime, and the saving that is not there.",
               "sm", "start")]

    # Gantt.
    x0, x1 = 130, 520
    y0 = 56
    row = 26
    n = LIVE
    step = (x1 - x0) / n

    for i in range(n + 1):
        xx = x0 + i * step
        b.append(rule(xx, y0 - 6, xx, y0 + row * len(LIFE) - 8, "grid", 0.8))
    for i in range(n):
        b.append(label(x0 + (i + 0.5) * step, y0 - 12, str(i), "xs muted"))

    for i, (name, byts, f, l) in enumerate(LIFE):
        yy = y0 + i * row
        b.append(label(x0 - 10, yy + 10, name, "xs mono", "end"))
        bx = x0 + f * step
        bw = (l - f + 1) * step
        colour = AMBER if name == "shadow" else (BLUE if name in ("hdr", "depth") else PURPLE)
        b.append(box(bx + 1, yy, bw - 2, 15, colour=colour, opacity=0.55, rx=2))
        b.append(label(x1 + 10, yy + 11, f"{byts:,} B", "xs muted", "start"))

    b.append(label(x0 + (x1 - x0) / 2, y0 + row * len(LIFE) + 8,
                   "position in the compiled schedule", "xs muted"))

    # The three numbers, as bars, BELOW the plot (annotation boxes go outside).
    ty = y0 + row * len(LIFE) + 40
    b.append(rule(34, ty, W - 34, ty, "grid", 1.2))
    b.append(label(34, ty + 22, "THE THREE NUMBERS", "xs", "start"))

    bars = [("no sharing at all", NAIVE, GREY, "100.0%"),
            ("true memory aliasing", PEAK, GREEN, f"{100.0 * PEAK / NAIVE:.1f}%"),
            ("what SDL_GPU can express", POOLED, RED, "100.0%")]
    bx0 = 230
    bw_max = 300
    for i, (name, val, colour, pc) in enumerate(bars):
        yy = ty + 40 + i * 26
        b.append(label(bx0 - 12, yy + 11, name, "xs", "end"))
        w = bw_max * val / NAIVE
        b.append(box(bx0, yy, w, 15, colour=colour, opacity=0.55, rx=2))
        b.append(label(bx0 + w + 10, yy + 11, f"{val:,} B   {pc}", "xs muted", "start"))

    b.append(label(34, ty + 132,
                   f"The prize is {PRIZE:,} B ({PRIZE_PC:.1f}%) and this engine collects "
                   f"none of it — {BIG_PRIZE:.2f} MB at 1920x1080.", "xs t-bad", "start"))

    return svg(uid, W, H,
               "Transient lifetimes and the three memory totals",
               "A Gantt chart of nine transient resources across the fourteen scheduled "
               "passes, with their byte sizes, above three bars comparing no sharing "
               "(1,223,296 B), true memory aliasing (1,048,576 B) and what SDL_GPU can "
               "actually express (1,223,296 B - no saving at all).",
               b)


# ===========================================================================
# Figure 6 — a chain has nothing to alias  (§7)
# ===========================================================================
def fig_shape():
    uid = "l617f6"
    W, H = 760, 378
    b = [label(20, 22, "Why the saving is not there: a chain, against a tree.", "sm", "start")]

    # Left: the chain.
    b.append(label(40, 56, "OUR FRAME — a CHAIN", "xs t-bad", "start"))
    chain = ["scene", "bright", "down x5", "up x5", "resolve"]
    for i, name in enumerate(chain):
        yy = 74 + i * 42
        b.append(box(48, yy, 120, 28, colour=BLUE, opacity=0.16))
        b.append(hollow(48, yy, 120, 28, BLUE, width=1.1))
        b.append(label(108, yy + 18, name, "xs mono"))
        if i < len(chain) - 1:
            b.append(arrow(108, yy + 30, 108, yy + 40, uid, "s", 1.1))
    b.append(label(198, 150, "every stage's output is", "xs muted", "start"))
    b.append(label(198, 164, "the next stage's input,", "xs muted", "start"))
    b.append(label(198, 178, "and the pyramid's six", "xs muted", "start"))
    b.append(label(198, 192, "levels are ALL live at", "xs muted", "start"))
    b.append(label(198, 206, "the turn. Nothing is", "xs muted", "start"))
    b.append(label(198, 220, "dead while anything", "xs muted", "start"))
    b.append(label(198, 234, "else is alive.", "xs muted", "start"))

    b.append(rule(390, 46, 390, H - 30, "grid", 1.2))

    # Right: the tree, drawn as the harness actually declares it — the source is
    # a trunk that persists, and each effect hangs a SCRATCH LEAF off it that
    # dies before the next one is born. Drawing A and B side by side would say
    # "parallel branches", which is not what makes the pool work: it is that the
    # schedule puts them one after the other.
    b.append(label(420, 56, "A FRAME THAT WOULD PAY — a TREE", "xs t-ok", "start"))

    trunk = [("fill source", 74, GREEN),
             ("effect A", 128, AMBER),
             ("A -> source", 182, GREEN),
             ("effect B", 236, AMBER),
             ("composite", 290, GREEN)]
    tx = 470
    for i, (name, yy, colour) in enumerate(trunk):
        b.append(box(tx - 60, yy, 120, 26, colour=colour, opacity=0.16))
        b.append(hollow(tx - 60, yy, 120, 26, colour, width=1.1))
        b.append(label(tx, yy + 17, name, "xs mono"))
        if i < len(trunk) - 1:
            b.append(arrow(tx, yy + 28, tx, trunk[i + 1][1] - 4, uid, "s", 1.1))

    # The two leaves, and the one texture they share.
    for name, yy, live in (("scratch A", 128, "live [1, 2]"), ("scratch B", 236, "live [3, 4]")):
        b.append(box(600, yy, 116, 26, colour=PURPLE, opacity=0.20))
        b.append(hollow(600, yy, 116, 26, PURPLE, width=1.1))
        b.append(label(658, yy + 17, name, "xs mono"))
        b.append(arrow(tx + 62, yy + 13, 596, yy + 13, uid, "s", 1.1))
        b.append(label(658, yy + 40, live, "xs muted"))

    b.append(label(420, 336, "A dies before B is born, so BOTH land", "xs muted", "start"))
    b.append(label(420, 350, "on pool slot 1 and 131,072 bytes come", "xs muted", "start"))
    b.append(label(420, 364, "back (§7.3).", "xs muted", "start"))

    return svg(uid, W, H,
               "A chain has nothing to alias; a tree does",
               "On the left, this engine's frame drawn as a linear chain in which every "
               "intermediate is alive while the next is produced. On the right, a branching "
               "frame in which two half-resolution scratch targets are used one after the "
               "other and can therefore share one texture.",
               b)


# ===========================================================================
# Figure 7 — the instrument  (§11)
# ===========================================================================
def fig_golden():
    uid = "l617f7"
    W, H = 760, 350
    b = [label(20, 22, "The only proof that matters: the same frame, assembled two ways.",
               "sm", "start")]

    paths = [
        ("ASSEMBLED BY HAND", AMBER, 150,
         ["shadow.render(cb, ...)",
          "BeginGPURenderPass(hdr, depth)",
          "scene.render(cb, pass, ...)",
          "EndGPURenderPass",
          "bloom.render(cb, hdr, ...)",
          "BeginGPURenderPass(ldr)",
          "tonemap.render(cb, pass, ...)",
          "EndGPURenderPass"]),
        ("COMPILED FROM A DECLARATION", GREEN, 460,
         ["fg.reset()",
          "declare_frame(fg, ...)",
          "fg.compile(dev)",
          "fg.execute(cb)",
          "",
          "— no begin, no end,",
          "   no load op, no store op,",
          "   no order"]),
    ]
    for title, colour, cx, lines in paths:
        b.append(label(cx, 52, title, "xs", "middle"))
        b.append(box(cx - 140, 62, 280, 150, colour=colour, opacity=0.10))
        b.append(hollow(cx - 140, 62, 280, 150, colour, width=1.1))
        for i, ln in enumerate(lines):
            if not ln:
                continue
            b.append(label(cx - 128, 82 + i * 17, ln, "xs mono", "start"))
        b.append(arrow(cx, 216, cx, 244, uid, "s", 1.2))

    b.append(box(240, 248, 280, 34, colour=BLUE, opacity=0.16))
    b.append(hollow(240, 248, 280, 34, BLUE, width=1.2))
    b.append(label(380, 269, f"{CHANNELS:,} channels, 0 differing", "xs mono"))

    b.append(label(40, 312,
                   f"AND THE INSTRUMENT CAN FAIL: the same comparison against a bloom-less "
                   f"frame reports {CONTROL_DIFF:,} differing", "xs t-hi", "start"))
    b.append(label(40, 328,
                   "channels — the control 6.16 added after finding a golden that passed "
                   "when both its file reads failed.", "xs muted", "start"))

    return svg(uid, W, H,
               "The hand-written frame and the compiled frame compared",
               "Two columns of recording code - eight hand-written calls including four "
               "explicit render-pass boundaries, against four calls that declare and "
               "execute - converging on one result: 262,144 channels compared, zero "
               "differing, with a control proving the comparison can report a difference.",
               b)


def main():
    figs = [("l617_fig1.svg", fig_today()),
            ("l617_fig2.svg", fig_versions()),
            ("l617_fig3.svg", fig_deduction()),
            ("l617_fig4.svg", fig_worked()),
            ("l617_fig5.svg", fig_memory()),
            ("l617_fig6.svg", fig_shape()),
            ("l617_fig7.svg", fig_golden())]
    for name, body in figs:
        with open(os.path.join(OUT, name), "w") as fh:
            fh.write(body)
        print(f"wrote {OUT}/{name}  ({len(body):,} bytes)")


if __name__ == "__main__":
    main()
