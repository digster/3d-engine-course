#!/usr/bin/env python3
"""scratch/figs_59.py — Lesson 5.9's diagrams.

Same rules as 5.5's: computed coordinates, no fill="..." on any <text> (CSS wins
over presentation attributes on text), every line kept well inside the viewBox,
and no right-anchored label near x=0.

Filenames are numbered by PAGE ORDER, which is the trap 5.3 fell into.
Row labels start well right of any column, which is the trap 5.4 fell into.
"""
import os

OUT = "scratch"

AMBER = "#f0961e"
BLUE = "#5082e6"
GREEN = "#5ac878"
RED = "#e05c5c"
PURPLE = "#9b7ede"
GREY = "#8f918a"


def esc(t):
    return t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def markers(uid):
    out = ["  <defs>"]
    for suffix, var in (("i", "--dia-ink"), ("s", "--dia-ink-soft"), ("h", "--dia-hi")):
        out.append(
            f'    <marker id="e-{suffix}-{uid}" viewBox="0 0 10 10" refX="9" refY="5"'
            f' markerWidth="6" markerHeight="6" orient="auto-start-reverse">'
            f'<path d="M0,0 L10,5 L0,10 z" fill="var({var})"/></marker>')
    for suffix, colour in (("b", RED), ("g", GREEN)):
        out.append(
            f'    <marker id="e-{suffix}-{uid}" viewBox="0 0 10 10" refX="9" refY="5"'
            f' markerWidth="6" markerHeight="6" orient="auto-start-reverse">'
            f'<path d="M0,0 L10,5 L0,10 z" fill="{colour}"/></marker>')
    out.append("  </defs>")
    return "\n".join(out)


def svg(uid, w, h, title, desc, body):
    return (f'<svg viewBox="0 0 {w} {h}" role="img" aria-labelledby="{uid}-t {uid}-d">\n'
            f'  <title id="{uid}-t">{esc(title)}</title>\n'
            f'  <desc id="{uid}-d">{esc(desc)}</desc>\n'
            f'{markers(uid)}\n' + "\n".join(body) + "\n</svg>\n")


def box(x, y, w, h, colour=None, dash=None, rx=3, width=1.2, opacity=None):
    fill = 'class="fill-soft grid"' if colour is None else f'fill="{colour}" stroke="{colour}"'
    d = f' stroke-dasharray="{dash}"' if dash else ""
    o = f' fill-opacity="{opacity}"' if opacity is not None else ""
    return (f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" {fill}'
            f' stroke-width="{width}"{d}{o}/>')


def hollow(x, y, w, h, colour, dash=None, rx=3, width=1.3):
    d = f' stroke-dasharray="{dash}"' if dash else ""
    return (f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="none"'
            f' stroke="{colour}" stroke-width="{width}"{d}/>')


def label(x, y, text, cls="sm", anchor="middle"):
    return f'<text x="{x}" y="{y}" class="{cls}" text-anchor="{anchor}">{esc(text)}</text>'


def arrow(x1, y1, x2, y2, uid, kind="s", width=1.2, dash=None):
    d = f' stroke-dasharray="{dash}"' if dash else ""
    cls = {"s": "ink-soft", "i": "ink", "h": "hi", "b": "ink", "g": "ink"}[kind]
    stroke = ""
    if kind == "b":
        cls = ""
        stroke = f' stroke="{RED}"'
    if kind == "g":
        cls = ""
        stroke = f' stroke="{GREEN}"'
    c = f' class="{cls}"' if cls else ""
    return (f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}"{c}{stroke}'
            f' stroke-width="{width}"{d} marker-end="url(#e-{kind}-{uid})"/>')


def rule(x1, y1, x2, y2, cls="grid", width=1.0, dash=None):
    d = f' stroke-dasharray="{dash}"' if dash else ""
    return (f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" class="{cls}"'
            f' stroke-width="{width}"{d}/>')






# ===========================================================================
# Figure 1 — the composition rule, and the order it demands  (§1)
# ===========================================================================
def fig_compose():
    uid = "l59f1"
    W, H = 720, 420
    b = [label(20, 20, "One rule, and the constraint it puts on the ORDER you may compute it in.",
               "sm", "start")]

    # ---- the chain ----
    b.append(label(24, 48, "THE RULE — world(child) = world(parent) x local(child)",
                   "xs t-hi", "start"))
    nodes = [("sun", AMBER, 90), ("planet", BLUE, 300), ("moon", GREY, 510)]
    for name, colour, x in nodes:
        b.append(box(x, 62, 120, 22, colour=colour, opacity=0.24))
        b.append(label(x + 60, 77, name, "xs mono"))
    b.append(arrow(212, 73, 296, 73, uid, "h", 1.4))
    b.append(arrow(422, 73, 506, 73, uid, "h", 1.4))
    b.append(label(254, 60, "parent of", "xs muted"))
    b.append(label(464, 60, "parent of", "xs muted"))

    rows = [("local", ["(0, 0, 0)", "(10, 0, 0)", "(0, 2, 0)"], AMBER),
            ("world", ["(0, 0, 0)", "(10, 0, 0)", "(10, 2, 0)"], GREEN)]
    for r, (rowname, vals, colour) in enumerate(rows):
        y = 96 + r * 26
        b.append(label(82, y + 15, rowname, "xs mono muted", "end"))
        for (name, _c, x), v in zip(nodes, vals):
            b.append(box(x, y, 120, 20, colour=colour, opacity=0.13))
            b.append(label(x + 60, y + 14, v, "xs mono"))
    b.append(label(24, 168, "The moon's (10, 2, 0) was computed by NOBODY. Move the sun and every "
                            "number in the lower row", "xs muted", "start"))
    b.append(label(24, 182, "changes, with no code anywhere that says so.", "xs muted", "start"))

    b.append(rule(24, 198, 696, 198, "grid", 1.2))

    # ---- the tension ----
    b.append(label(24, 220, "THE CONSTRAINT — a parent must be finished before its children start.",
                   "xs t-hi", "start"))
    b.append(label(24, 240, "…and a component pool's dense order is INSERTION order, disturbed "
                            "by every swap-and-pop (5.7).", "xs muted", "start"))

    order = [("moon", GREY, False), ("sun", AMBER, True), ("planet", BLUE, True),
             ("moon", GREY, False), ("planet", BLUE, True), ("sun", AMBER, True),
             ("moon", GREY, False), ("planet", BLUE, True)]
    b.append(label(112, 275, "pool<transform>", "xs mono muted", "end"))
    for i, (name, colour, ok) in enumerate(order):
        x = 120 + i * 72
        b.append(box(x, 262, 66, 22, colour=colour, opacity=0.20))
        b.append(label(x + 33, 277, name, "xs mono"))
        b.append(label(x + 33, 296, f"i={i}", "xs mono muted"))
    b.append(hollow(117, 259, 72, 28, RED, dash="4 3", width=1.4))
    b.append(label(153, 316, "a child at i=0", "xs t-bad"))
    b.append(label(153, 328, "before its parent at i=1", "xs t-bad"))

    b.append(label(24, 356, "VERIFIED, not asserted: verify_59 §C builds a 4-level tree, churns it, "
                            "and finds 12 of 48 entities sitting", "xs muted", "start"))
    b.append(label(24, 370, "ahead of their own parent. A walk in dense order composes those "
                            "against a matrix from LAST frame —", "xs muted", "start"))
    b.append(label(24, 384, "a picture that is wrong in a way nothing reports.",
                   "xs muted", "start"))
    b.append(label(24, 406, "So the lesson is not the maths. It is what a correct order costs.",
                   "xs t-hi", "start"))

    return svg(uid, W, H, "The composition rule and the ordering constraint it implies",
               "Above, a sun, planet and moon with their local and composed world positions. "
               "Below, a component pool whose dense order puts a child before its parent, which "
               "a naive walk would compose against a stale matrix.", b)


# ===========================================================================
# Figure 2 — three ways to visit the same tree  (§3)
# ===========================================================================
def fig_orders():
    uid = "l59f2"
    W, H = 720, 400
    b = [label(20, 20, "The same tree, three visit orders. Every one computes the same matrices.",
               "sm", "start")]

    # a small tree: 0 -> {1,2}, 1 -> {3,4}, 2 -> {5}
    coords = {0: (360, 60), 1: (250, 108), 2: (470, 108),
              3: (190, 156), 4: (310, 156), 5: (470, 156)}
    edges = [(0, 1), (0, 2), (1, 3), (1, 4), (2, 5)]
    depth_colour = {0: AMBER, 1: BLUE, 2: GREEN}
    node_depth = {0: 0, 1: 1, 2: 1, 3: 2, 4: 2, 5: 2}
    for a, c in edges:
        b.append(rule(coords[a][0], coords[a][1] + 11, coords[c][0], coords[c][1] - 11,
                      "ink-soft", 1.0))
    for n, (x, y) in coords.items():
        b.append(box(x - 22, y - 11, 44, 22, colour=depth_colour[node_depth[n]], opacity=0.26))
        b.append(label(x, y + 4, str(n), "xs mono"))
    b.append(label(24, 46, "the tree", "xs t-hi", "start"))
    b.append(label(120, 66, "depth 0", "xs muted", "start"))
    b.append(label(120, 114, "depth 1", "xs muted", "start"))
    b.append(label(120, 162, "depth 2", "xs muted", "start"))

    b.append(rule(24, 188, 696, 188, "grid", 1.2))

    arms = [
        ("A  recurse from roots", [0, 1, 3, 4, 2, 5],
         "depth first. The parent was written one call ago, so it is certainly in L1 —",
         "the best temporal locality any arm can have. Pays in stack traffic and a jumping write."),
        ("B  level order, via an index", [0, 1, 2, 3, 4, 5],
         "breadth first. One flat loop per level, and everything a level reads was written",
         "by an EARLIER level — so within a level nothing depends on anything else."),
        ("C  level order, rows packed", [0, 1, 2, 3, 4, 5],
         "the same order, with the rows physically moved into it. Reads and writes go",
         "straight down the arrays; only the parent lookup still jumps."),
    ]
    for i, (title, seq, note1, note2) in enumerate(arms):
        y = 212 + i * 62
        b.append(label(24, y, title, "xs t-hi", "start"))
        for k, n in enumerate(seq):
            x = 250 + k * 46
            b.append(box(x, y - 12, 38, 18, colour=depth_colour[node_depth[n]], opacity=0.24))
            b.append(label(x + 19, y + 1, str(n), "xs mono"))
            if k + 1 < len(seq):
                b.append(rule(x + 39, y - 3, x + 45, y - 3, "ink-soft", 1.0))
        b.append(label(24, y + 18, note1, "xs muted", "start"))
        b.append(label(24, y + 30, note2, "xs muted", "start"))

    b.append(label(24, 392, "B and C visit in the same ORDER and differ only in where the rows "
                            "LIVE. That is the whole of §5.", "xs t-hi", "start"))

    return svg(uid, W, H, "Three visit orders over one small tree",
               "A six-node tree, and the sequences produced by depth-first recursion, by level "
               "order through an index, and by level order with the rows packed.", b)


# ===========================================================================
# Figure 3 — depth is the axis  (§4)
# ===========================================================================
def fig_depth():
    uid = "l59f3"
    W, H = 720, 400
    b = [label(20, 20, "100,000 entities in every column. The only thing that moves is DEPTH.",
               "sm", "start")]

    depths = [1, 2, 4, 8, 16, 32]
    rec = [3.34, 6.55, 10.08, 11.81, 13.29, 13.55]
    lvl = [3.39, 4.42, 4.83, 4.78, 5.04, 4.94]

    x0, y0, w, h = 92, 60, 560, 236
    top = 15.0
    b.append(rule(x0, y0 + h, x0 + w, y0 + h, "grid", 1.2))
    b.append(rule(x0, y0, x0, y0 + h, "grid", 1.2))
    for t in range(0, 16, 3):
        y = y0 + h - (t / top) * h
        b.append(rule(x0, y, x0 + w, y, "grid", 0.7, "2 4"))
        b.append(label(x0 - 8, y + 4, str(t), "xs mono muted", "end"))
    b.append(label(x0 - 8, y0 - 6, "ns/entity", "xs muted", "end"))

    step = w / len(depths)
    for i, d in enumerate(depths):
        cx = x0 + step * (i + 0.5)
        b.append(label(cx, y0 + h + 18, f"depth {d}", "xs mono muted"))
        for series, vals, colour, dx in (("A", rec, RED, -13), ("C", lvl, GREEN, 13)):
            v = vals[i]
            bh = (v / top) * h
            b.append(box(cx + dx - 11, y0 + h - bh, 22, bh, colour=colour, opacity=0.45,
                         rx=2, width=0.8))
            b.append(label(cx + dx, y0 + h - bh - 5, f"{v:.1f}", "xs mono"))

    b.append(box(x0 + 6, y0 + 6, 12, 12, colour=RED, opacity=0.45, rx=2, width=0.8))
    b.append(label(x0 + 24, y0 + 16, "A  recurse from roots", "xs muted", "start"))
    b.append(box(x0 + 170, y0 + 6, 12, 12, colour=GREEN, opacity=0.45, rx=2, width=0.8))
    b.append(label(x0 + 188, y0 + 16, "C  level order", "xs muted", "start"))

    b.append(rule(24, 322, 696, 322, "grid", 1.0, "3 3"))
    b.append(label(24, 340, "RECURSION IS DEPTH-DEPENDENT AND LEVEL ORDER IS NOT: 3.3 → 13.6 ns "
                            "against 3.4 → 4.9.", "xs t-hi", "start"))
    b.append(label(24, 356, "READ THE FIRST COLUMN FIRST. At depth 1 there is no hierarchy, both "
                            "arms do the same work, and the ratio is 1.01× — the control that "
                            "says the", "xs muted", "start"))
    b.append(label(24, 370, "harness is measuring the tree rather than itself. Everything after "
                            "it is the tree.", "xs muted", "start"))
    b.append(label(24, 392, "And it SATURATES around depth 8: a chain twice as long costs a "
                            "recursive walk almost nothing more.", "xs muted", "start"))

    return svg(uid, W, H, "Resolve cost against tree depth at a fixed entity count",
               "A bar chart at depths 1 to 32 with 100,000 entities throughout. Recursion rises "
               "from 3.3 to 13.6 nanoseconds per entity; level order stays between 3.4 and 5.0.",
               b)


# ===========================================================================
# Figure 4 — the level order, and why it parallelises  (§5)
# ===========================================================================
def fig_levels():
    uid = "l59f4"
    W, H = 720, 396
    b = [label(20, 20, "What rebuild() produces, and the property that fell out of it for free.",
               "sm", "start")]

    b.append(label(24, 48, "order_  — every entity, parents strictly before children",
                   "xs t-hi", "start"))
    levels = [(0, 3, AMBER), (1, 5, BLUE), (2, 4, GREEN)]
    x = 96
    starts = []
    for lvl, count, colour in levels:
        starts.append(x)
        for k in range(count):
            b.append(box(x, 62, 44, 22, colour=colour, opacity=0.26))
            b.append(label(x + 22, 77, "e", "xs mono"))
            x += 48
        x += 14
    for (lvl, count, colour), sx in zip(levels, starts):
        b.append(rule(sx - 3, 90, sx + count * 48 - 7, 90, "grid", 1.0))
        b.append(label(sx + (count * 48 - 10) / 2, 104, f"level {lvl}", "xs mono muted"))
    b.append(label(88, 77, "order_", "xs mono muted", "end"))

    b.append(label(24, 128, "level_start_ = [0, 3, 8, 12]  — four numbers, and they are the "
                            "whole index.", "xs muted", "start"))

    b.append(rule(24, 148, 696, 148, "grid", 1.2))

    b.append(label(24, 170, "THE LOOP — and note what is NOT in it", "xs t-hi", "start"))
    # Explicit x offsets, not leading spaces: SVG collapses whitespace in <text>,
    # so an indented string renders flush left and the nesting — which is the
    # whole point of showing the loop — disappears.
    lines = [(0, "for each level:"),
             (18, "for each entity e in that level:"),
             (36, "out[e] = world(parent(e)) * parent_from_local(local[e])")]
    for i, (indent, ln) in enumerate(lines):
        b.append(label(60 + indent, 192 + i * 16, ln, "xs mono", "start"))
    # One column, wide enough for the longest line. The two-column version put a
    # 34-character label in a 132-unit box and it rendered straight through both
    # edges — a defect check-page.js cannot see, because the text is inside the
    # viewBox and on top of nothing.
    absent = ["no recursion", "no stack", "no visited set",
              "no “has my parent been resolved yet?” test"]
    for i, a in enumerate(absent):
        b.append(box(420, 178 + i * 20, 262, 18, colour=GREY, opacity=0.10, dash="3 3"))
        b.append(label(551, 191 + i * 20, a, "xs muted"))

    # BELOW the boxes, not beside them: at y=256 this line ran straight through
    # the fourth dashed box, which the viewBox and overlap checks cannot see
    # because a full-width prose line legitimately crosses the whole figure.
    b.append(label(24, 274, "The ORDER already guarantees the thing those tests would check. "
                            "Everything a level reads was", "xs muted", "start"))
    b.append(label(24, 288, "written by an earlier one.", "xs muted", "start"))

    b.append(rule(24, 304, 696, 304, "grid", 1.0, "3 3"))
    b.append(label(24, 324, "AND THEREFORE: within a level, every iteration is independent of "
                            "every other.", "xs t-ok", "start"))
    for i, (lvl, count, colour) in enumerate(levels):
        x = 120 + i * 200
        b.append(box(x, 338, 170, 22, colour=colour, opacity=0.20))
        b.append(label(x + 85, 353, f"level {lvl}: parallel_for", "xs mono"))
        if i < 2:
            b.append(arrow(x + 172, 349, x + 196, 349, uid, "h", 1.2))
    b.append(label(24, 382, "Module 9 does not have to design that. It arrived with the choice "
                            "of visit order.", "xs muted", "start"))

    return svg(uid, W, H, "The level order and the parallelism it implies",
               "The order array split into three levels by a start index, the flat loop that "
               "walks it, and the observation that each level can be a parallel_for that depends "
               "only on the level before it.", b)


# ===========================================================================
# Figure 5 — the dirty crossover  (§6)
# ===========================================================================
def fig_dirty():
    uid = "l59f5"
    W, H = 720, 400
    b = [label(20, 20, "Skipping work is only free if the work you skip is most of it.",
               "sm", "start")]

    moved = [0.1, 1.0, 5.0, 10.0, 25.0, 50.0, 100.0]
    reach = [0.5, 4.8, 20.7, 36.4, 66.3, 87.6, 100.0]
    ratio = [0.08, 0.15, 0.40, 0.65, 1.04, 1.19, 1.29]

    x0, y0, w, h = 92, 56, 560, 200
    b.append(rule(x0, y0 + h, x0 + w, y0 + h, "grid", 1.2))
    b.append(rule(x0, y0, x0, y0 + h, "grid", 1.2))
    top = 1.4
    for t in (0.0, 0.5, 1.0):
        y = y0 + h - (t / top) * h
        b.append(rule(x0, y, x0 + w, y, "grid", 0.7, "2 4"))
        b.append(label(x0 - 8, y + 4, f"{t:.1f}x", "xs mono muted", "end"))
    # the break-even line
    y_one = y0 + h - (1.0 / top) * h
    b.append(rule(x0, y_one, x0 + w, y_one, "hi", 1.4, "5 3"))
    # LEFT, not right: the right-hand bars are the tall ones, and a label anchored
    # there sits on top of the 1.29x bar it is meant to be explaining.
    b.append(label(x0 + 6, y_one - 6, "break even", "xs t-hi", "start"))

    step = w / len(moved)
    for i, m in enumerate(moved):
        cx = x0 + step * (i + 0.5)
        v = ratio[i]
        bh = (v / top) * h
        colour = GREEN if v < 1.0 else RED
        b.append(box(cx - 18, y0 + h - bh, 36, bh, colour=colour, opacity=0.45, rx=2, width=0.8))
        b.append(label(cx, y0 + h - bh - 5, f"{v:.2f}", "xs mono"))
        b.append(label(cx, y0 + h + 18, f"{m:g}%", "xs mono muted"))
        b.append(label(cx, y0 + h + 32, f"→{reach[i]:g}%", "xs muted"))
    b.append(label(x0 - 8, y0 + h + 18, "moved", "xs muted", "end"))
    b.append(label(x0 - 8, y0 + h + 32, "reached", "xs muted", "end"))

    b.append(rule(24, 306, 696, 306, "grid", 1.0, "3 3"))
    b.append(label(24, 326, "THE AMPLIFICATION IS THE PART NOBODY MENTIONS. Moving 10% of a "
                            "depth-8 tree dirties 36% of it,", "xs t-hi", "start"))
    b.append(label(24, 340, "because every descendant of a moved entity has to move too. "
                            "25% moved reaches two thirds of the world.", "xs muted", "start"))
    b.append(label(24, 362, "SO THE CROSSOVER IS AT ~25% MOVED, and past it the dirty pass is "
                            "SLOWER — 1.29× when everything moves,", "xs t-bad", "start"))
    b.append(label(24, 376, "which is exactly what an animated scene does. That is why this "
                            "engine does not ship one.", "xs muted", "start"))
    b.append(label(24, 396, "A measurement lesson has to be willing to conclude NO. This one "
                            "concludes no twice — see also §7.", "xs muted", "start"))

    return svg(uid, W, H, "Dirty-subtree resolution against the fraction of the world that moved",
               "A bar chart of the dirty pass's cost relative to a full pass, from 0.08 at one "
               "entity in a thousand moving to 1.29 when everything moves, crossing break-even "
               "at about a quarter.", b)


# ===========================================================================
# Figure 6 — the camera is an entity  (§7)
# ===========================================================================
def fig_camera():
    uid = "l59f6"
    W, H = 720, 380
    b = [label(20, 20, "A camera stops being a kind of thing and becomes a set of components.",
               "sm", "start")]

    b.append(label(24, 48, "BEFORE — demo::orbit_camera, a struct one program owned",
                   "xs t-bad", "start"))
    b.append(box(90, 60, 250, 60, colour=GREY, opacity=0.14))
    for i, f in enumerate(["target, radius, azimuth, elevation", "eye()  → vec3",
                           "view() → mat4"]):
        b.append(label(215, 76 + i * 15, f, "xs mono"))
    b.append(label(360, 78, "One camera. Cannot be parented,", "xs muted", "start"))
    b.append(label(360, 92, "cannot be swapped without a pointer,", "xs muted", "start"))
    b.append(label(360, 106, "cannot be found by a query.", "xs muted", "start"))

    b.append(rule(24, 136, 696, 136, "grid", 1.2))

    b.append(label(24, 158, "AFTER — an entity, and the components are the whole design",
                   "xs t-ok", "start"))
    comps = [("transform", AMBER, "where it is (local)"),
             ("world_transform", GREEN, "…resolved, so it can be PARENTED"),
             ("camera", BLUE, "fovy, near, far — NOT aspect"),
             ("active_camera", PURPLE, "a TAG: presence is the information")]
    for i, (name, colour, note) in enumerate(comps):
        y = 176 + i * 26
        b.append(box(120, y, 150, 22, colour=colour, opacity=0.24))
        b.append(label(195, y + 15, name, "xs mono"))
        b.append(label(282, y + 15, note, "xs muted", "start"))
    b.append(label(112, 190, "entity", "xs mono muted", "end"))

    b.append(rule(24, 292, 696, 292, "grid", 1.0, "3 3"))
    b.append(label(24, 310, "THE IDENTITY THAT SAYS IT IS THE SAME MATHS:", "xs t-hi", "start"))
    b.append(box(60, 320, 600, 22, colour=GREEN, opacity=0.12))
    b.append(label(360, 335, "rigid_inverse(parent_from_local(look_along(e, t, u)))  ==  "
                             "look_at(e, t, u)", "xs mono"))
    b.append(label(24, 360, "Bit for bit — verify_59 §D. Lesson 2.9 said the view matrix is the "
                            "inverse of the camera's placement;", "xs muted", "start"))
    b.append(label(24, 374, "now the camera HAS a placement, so the sentence became executable.",
                   "xs muted", "start"))

    return svg(uid, W, H, "A camera struct compared with a camera entity",
               "Above, a demo-owned orbit camera struct. Below, an entity carrying a transform, "
               "a world transform, a camera component and an active-camera tag, with the identity "
               "relating look_along and look_at.", b)


FIGS = {
    "l59_fig1.svg": fig_compose,   # §1 — PAGE order, not authoring order
    "l59_fig2.svg": fig_orders,    # §3
    "l59_fig3.svg": fig_depth,     # §4
    "l59_fig4.svg": fig_levels,    # §5
    "l59_fig5.svg": fig_dirty,     # §6
    "l59_fig6.svg": fig_camera,    # §7
}


def main():
    for name, fn in FIGS.items():
        with open(os.path.join(OUT, name), "w") as fh:
            fh.write(fn())
        print(f"wrote {os.path.join(OUT, name)}")


if __name__ == "__main__":
    main()
