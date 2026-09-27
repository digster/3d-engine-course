#!/usr/bin/env python3
"""scratch/figs_57.py — Lesson 5.7's diagrams.

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
# Figure 1 — the two shapes, as memory  (§2.2)
# ===========================================================================
def fig_shapes():
    uid = "l57f1"
    W, H = 720, 420
    b = [label(20, 20, "The same four components on the same entities, stored two ways. "
                       "A query reads left to right.", "sm", "start")]

    # ---- ARCHETYPE ----
    b.append(label(24, 46, "ARCHETYPE — entities with the SAME COMPONENT SET share a chunk.",
                   "xs t-hi", "start"))
    cols = [("transform[]", 60, AMBER), ("velocity[]", 16, BLUE),
            ("bounds[]", 16, GREEN), ("material[]", 16, PURPLE)]
    x = 60
    for name, _w, colour in cols:
        b.append(box(x, 58, 140, 20, colour=colour, opacity=0.22))
        b.append(label(x + 70, 72, name, "xs mono"))
        for r in range(4):
            b.append(box(x, 82 + r * 15, 140, 13, colour=colour, opacity=0.10))
        x += 152
    for r in range(4):
        b.append(label(52, 92 + r * 15, f"row {r}", "xs mono muted", "end"))
        b.append(rule(56, 88 + r * 15, 664, 88 + r * 15, "grid", 0.8, "2 3"))
    b.append(label(24, 158, "Row i is one entity, in every column. The index is SHARED — "
                            "no lookup, no test, nothing to", "xs muted", "start"))
    b.append(label(24, 172, "check. Any entity that does not have all four is not in this "
                            "chunk at all.", "xs muted", "start"))

    b.append(rule(24, 186, 696, 186, "grid", 1.2))

    # ---- SPARSE SET ----
    b.append(label(24, 208, "SPARSE SET — one dense array per component TYPE, keyed by "
                            "entity through a redirect.", "xs t-hi", "start"))

    # lead pool, walked densely
    b.append(box(60, 220, 150, 18, colour=AMBER, opacity=0.22))
    b.append(label(135, 233, "transform: dense", "xs mono"))
    for r in range(4):
        b.append(box(60, 242 + r * 15, 150, 13, colour=AMBER, opacity=0.10))
        b.append(label(52, 252 + r * 15, f"i={r}", "xs mono muted", "end"))
    b.append(label(135, 320, "walked in order", "xs muted"))

    # entity ids
    b.append(box(222, 220, 62, 18, colour=GREY, opacity=0.22))
    b.append(label(253, 233, "entity", "xs mono"))
    ids = ["e=17", "e=4", "e=91", "e=30"]
    for r in range(4):
        b.append(box(222, 242 + r * 15, 62, 13, colour=GREY, opacity=0.12))
        b.append(label(253, 252 + r * 15, ids[r], "xs mono"))

    # sparse map
    b.append(box(330, 220, 110, 18, colour=RED, opacity=0.20))
    b.append(label(385, 233, "sparse[e]", "xs mono"))
    b.append(box(330, 242, 110, 58, colour=RED, opacity=0.10))
    b.append(label(385, 262, "one slot per", "xs muted"))
    b.append(label(385, 276, "ENTITY IN THE", "xs muted"))
    b.append(label(385, 290, "WORLD", "xs muted"))
    b.append(label(385, 320, "4 B each, present or not", "xs muted"))

    # data pools
    for j, (name, colour) in enumerate((("velocity[]", BLUE), ("bounds[]", GREEN),
                                        ("material[]", PURPLE))):
        x = 470 + j * 78
        b.append(box(x, 220, 70, 18, colour=colour, opacity=0.22))
        b.append(label(x + 35, 233, name, "xs mono"))
        for r in range(4):
            b.append(box(x, 242 + r * 15, 70, 13, colour=colour, opacity=0.10))

    # the two loads
    b.append(arrow(286, 249, 328, 254, uid, "h", 1.3))
    b.append(arrow(442, 262, 468, 276, uid, "h", 1.3))
    b.append(label(300, 240, "load 1", "xs t-hi", "start"))
    b.append(label(452, 306, "load 2", "xs t-hi", "start"))

    b.append(rule(24, 344, 696, 344, "grid", 1.2, "2 3"))
    b.append(label(24, 364, "THE WHOLE DIFFERENCE IS THE MIDDLE COLUMN.", "xs t-hi", "start"))
    b.append(label(292, 364, "Both designs walk dense arrays. The sparse set spends "
                             "two dependent", "xs", "start"))
    b.append(label(24, 380, "loads per EXTRA component to find where in its array the "
                            "entity's data sits; the archetype already knows, because "
                            "it put", "xs", "start"))
    b.append(label(24, 396, "the entity in a chunk where that is true by construction. "
                            "Everything else in this lesson is the price of that.",
                   "xs", "start"))
    return svg(uid, W, H, "Archetype storage compared with sparse-set storage",
               "Top: an archetype chunk drawn as four parallel columns whose rows "
               "line up, so one shared index reaches every component. Bottom: a "
               "sparse set drawn as a dense transform array, an entity id array, a "
               "sparse redirect table with one slot per entity in the world, and "
               "three dense component arrays reached through two dependent loads.", b)


# ===========================================================================
# Figure 2 — the redirect, and why it is not a linked list  (§2.3)
# ===========================================================================
def fig_redirect():
    uid = "l57f2"
    W, H = 720, 330
    b = [label(20, 20, "Two loads in a row is a dependency chain. Whether that costs "
                       "anything depends on what else is in flight.", "sm", "start")]

    # top: the chain for one entity
    b.append(label(24, 48, "ONE ENTITY, ONE EXTRA COMPONENT", "xs t-hi", "start"))
    steps = [("dense[i]", "sequential", GREEN), ("sparse[e]", "depends on e", AMBER),
             ("data[idx]", "depends on idx", RED)]
    for i, (nm, note, colour) in enumerate(steps):
        x = 40 + i * 200
        b.append(box(x, 60, 150, 34, colour=colour, opacity=0.18))
        b.append(label(x + 75, 76, nm, "xs mono"))
        b.append(label(x + 75, 89, note, "xs muted"))
        if i < 2:
            b.append(arrow(x + 152, 77, x + 196, 77, uid, "h", 1.4))
    b.append(label(640, 79, "depth 2", "xs t-hi", "start"))
    b.append(label(24, 112, "The address of the second load is not known until the first "
                            "returns. That is the SAME SHAPE as a linked list — and it "
                            "is", "xs muted", "start"))
    b.append(label(24, 126, "not the same cost, for the reason Lesson 5.6 measured.",
                   "xs muted", "start"))

    b.append(rule(24, 142, 696, 142, "grid", 1.2))

    # bottom: many chains in flight
    b.append(label(24, 164, "…BUT THE LOOP HAS MANY ENTITIES, AND THEIR CHAINS ARE "
                            "INDEPENDENT", "xs t-hi", "start"))
    for r in range(5):
        y = 178 + r * 20
        b.append(label(52, y + 10, f"i={r}", "xs mono muted", "end"))
        for i in range(3):
            x = 60 + i * 96
            colour = [GREEN, AMBER, RED][i]
            b.append(box(x, y, 84, 14, colour=colour, opacity=0.16))
            if i < 2:
                b.append(arrow(x + 86, y + 7, x + 94, y + 7, uid, "s", 0.9))
    b.append(f'<rect x="56" y="174" width="296" height="106" rx="4" fill="none"'
             f' stroke="{GREEN}" stroke-width="1.4" stroke-dasharray="4 3"/>')
    b.append(label(364, 200, "Five chains, started five cycles apart, all", "xs", "start"))
    b.append(label(364, 216, "outstanding at once. Their misses OVERLAP.", "xs", "start"))
    b.append(label(364, 238, "A linked list cannot do this: node 2's address", "xs muted", "start"))
    b.append(label(364, 252, "is inside node 1, so nothing can start until", "xs muted", "start"))
    b.append(label(364, 266, "node 1 has arrived. 5.6 measured 2.4x vs 6.5x.", "xs muted", "start"))

    b.append(rule(24, 296, 696, 296, "grid", 1.2, "2 3"))
    b.append(label(24, 316, "So the redirect is LATENCY, not bandwidth — and latency is "
                            "the kind of cost a machine can hide. Whether it does is "
                            "§5.2.", "xs t-hi", "start"))
    return svg(uid, W, H, "The sparse-set redirect as a two-deep dependency chain",
               "Top: three boxes in a row showing dense[i] feeding sparse[e] feeding "
               "data[idx], a chain of depth two. Bottom: five such chains drawn "
               "one above the other and marked as independent, so their cache "
               "misses overlap the way an array of pointers does and a linked list "
               "cannot.", b)


# ===========================================================================
# Figure 3 — what a structural change moves  (§3.2)
# ===========================================================================
def fig_move():
    uid = "l57f3"
    W, H = 720, 400
    b = [label(20, 20, "Adding one component to one entity, in each design.", "sm", "start")]

    # --- archetype ---
    b.append(label(24, 46, "ARCHETYPE — the entity CHANGES ARCHETYPE, so every column "
                           "travels.", "xs t-hi", "start"))
    src_cols = [("xform", AMBER, 60), ("vel", BLUE, 16), ("bnd", GREEN, 16)]
    x = 60
    b.append(label(52, 74, "from", "xs muted", "end"))
    for nm, colour, _n in src_cols:
        b.append(box(x, 62, 92, 18, colour=colour, opacity=0.22))
        b.append(label(x + 46, 75, nm, "xs mono"))
        x += 100
    b.append(label(x + 6, 75, "{xform, vel, bnd}", "xs mono muted", "start"))

    x = 60
    b.append(label(52, 134, "to", "xs muted", "end"))
    for nm, colour, _n in src_cols + [("material", PURPLE, 16)]:
        b.append(box(x, 122, 92, 18, colour=colour, opacity=0.22))
        b.append(label(x + 46, 135, nm, "xs mono"))
        x += 100
    b.append(label(x + 6, 135, "{xform, vel, bnd, material}", "xs mono muted", "start"))

    for i in range(3):
        b.append(arrow(106 + i * 100, 84, 106 + i * 100, 118, uid, "h", 1.4))
    b.append(label(24, 160, "92 bytes copied into the destination, plus 92 more moved "
                            "inside the source by swap-and-pop, plus two", "xs muted", "start"))
    b.append(label(24, 174, "index writes — one for this entity and one for whichever "
                            "entity the re-pack displaced.", "xs muted", "start"))
    b.append(label(24, 194, "AND THE COST IS PER COLUMN.", "xs t-bad", "start"))
    b.append(label(192, 194, "Twelve components instead of four is eight more arrays to "
                             "touch, in eight", "xs", "start"))
    b.append(label(24, 208, "different places. Measured: 13.1 ns narrow, 58.5 ns wide.",
                   "xs", "start"))

    b.append(rule(24, 226, 696, 226, "grid", 1.2))

    # --- sparse ---
    b.append(label(24, 248, "SPARSE SET — one pool grows by one. Nothing else is touched.",
                   "xs t-hi", "start"))
    b.append(box(60, 262, 240, 20, colour=PURPLE, opacity=0.22))
    b.append(label(180, 276, "material: dense + data", "xs mono"))
    b.append(box(60, 286, 200, 14, colour=PURPLE, opacity=0.10))
    b.append(box(262, 286, 38, 14, colour=PURPLE, opacity=0.30))
    b.append(label(281, 296, "new", "xs"))
    b.append(box(330, 262, 130, 20, colour=RED, opacity=0.20))
    b.append(label(395, 276, "material.sparse[e]", "xs mono"))
    b.append(box(330, 286, 130, 14, colour=RED, opacity=0.10))
    b.append(arrow(302, 293, 328, 293, uid, "h", 1.3))
    b.append(label(480, 280, "push_back, push_back, one write.", "xs", "start"))
    b.append(label(480, 296, "Measured: 4.2 ns, at every width.", "xs t-ok", "start"))

    b.append(label(24, 328, "The transform, velocity and bounds pools do not appear in "
                            "this picture, and that is the finding. They are not read, "
                            "not", "xs muted", "start"))
    b.append(label(24, 342, "written, and not even known about. An entity's OTHER "
                            "components are irrelevant to changing one of them.",
                   "xs muted", "start"))

    b.append(rule(24, 358, 696, 358, "grid", 1.2, "2 3"))
    b.append(label(24, 378, "One design's structural change scales with how wide the "
                            "entity is. The other one does not scale with anything.",
                   "xs t-hi", "start"))
    return svg(uid, W, H, "The cost of adding one component, in each design",
               "Top: an archetype move, drawn as three columns of data being copied "
               "from a chunk holding transform, velocity and bounds into a chunk "
               "that also holds material. Bottom: a sparse-set insert, drawn as one "
               "dense array growing by one element and one sparse slot being "
               "written, with the other pools untouched.", b)


# ===========================================================================
# Figure 4 — the query result  (§5.1)
# ===========================================================================
def fig_query():
    uid = "l57f4"
    W, H = 720, 372

    series = [
        ("cheap, scrambled", [0.84, 0.98, 1.16, 1.92, 2.40], RED),
        ("cheap, aligned", [0.81, 0.91, 1.06, 1.26, 1.31], AMBER),
        ("real, scrambled", [0.98, 1.05, 1.05, 1.17, 1.34], BLUE),
        ("real, aligned", [0.97, 1.05, 1.05, 1.02, 0.96], GREEN),
    ]
    xs_lab = ["4", "100", "1,000", "10,000", "100,000"]

    b = [label(20, 20, "Sparse-set query cost relative to the archetype, K = 4 "
                       "components. 1.00x means the design made no difference.",
               "sm", "start")]

    px0, py0, pw, ph = 92, 52, 452, 200
    top, bot = 2.6, 0.6
    b.append(box(px0, py0, pw, ph, dash="2 4"))
    for v in (0.8, 1.0, 1.4, 1.8, 2.2, 2.6):
        y = py0 + ph - ((v - bot) / (top - bot)) * ph
        b.append(rule(px0, y, px0 + pw, y, "grid", 1.0, "2 4"))
        b.append(label(px0 - 8, y + 4, f"{v:.1f}x", "xs muted", "end"))
    y1 = py0 + ph - ((1.0 - bot) / (top - bot)) * ph
    b.append(rule(px0, y1, px0 + pw, y1, "ink", 1.4))

    for i, lab in enumerate(xs_lab):
        x = px0 + (i / 4.0) * pw
        b.append(label(x, py0 + ph + 16, lab, "xs muted"))
    b.append(label(px0 + pw / 2, py0 + ph + 32, "entities in the world", "xs muted"))

    slots = []
    for name, vals, colour in series:
        pts = []
        for i, v in enumerate(vals):
            x = px0 + (i / 4.0) * pw
            y = py0 + ph - ((min(v, top) - bot) / (top - bot)) * ph
            pts.append(f"{x:.1f},{y:.1f}")
            b.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="2.6" fill="{colour}"/>')
        b.append(f'<polyline points="{" ".join(pts)}" fill="none" stroke="{colour}"'
                 f' stroke-width="1.6"/>')
        slots.append([py0 + ph - ((min(vals[-1], top) - bot) / (top - bot)) * ph,
                      name, colour, pts[-1]])

    slots.sort(key=lambda r: r[0])
    for i in range(1, len(slots)):
        if slots[i][0] - slots[i - 1][0] < 14.0:
            slots[i][0] = slots[i - 1][0] + 14.0
    for ly, name, colour, last in slots:
        lx, lyy = (float(v) for v in last.split(","))
        b.append(f'<line x1="{lx + 3:.1f}" y1="{lyy:.1f}" x2="{px0 + pw + 5:.1f}"'
                 f' y2="{ly:.1f}" stroke="{colour}" stroke-width="0.8"'
                 f' stroke-dasharray="2 2"/>')
        b.append(f'<text x="{px0 + pw + 9}" y="{ly + 4:.1f}" class="xs mono"'
                 f' style="fill:{colour}" text-anchor="start">{esc(name)}</text>')

    b.append(rule(24, 300, 696, 300, "grid", 1.2))
    b.append(label(24, 320, "THE CONTROL:", "xs t-ok", "start"))
    b.append(label(104, 320, "at K = 1 both designs perform literally the same walk, and "
                             "the measured ratio is 1.00x at every", "xs", "start"))
    b.append(label(24, 336, "size — so the harness is measuring the redirect and not "
                            "itself. THE SPREAD: the same redirect costs 2.4x on a body "
                            "with", "xs", "start"))
    b.append(label(24, 352, "nothing to hide behind and 1.34x on one that builds a model "
                            "matrix. Alignment is worth as much as the body is.",
                   "xs t-hi", "start"))
    return svg(uid, W, H, "Sparse-set query cost relative to the archetype, versus world size",
               "A line chart with world size from 4 to 100,000 entities on the x "
               "axis and cost relative to an archetype on the y axis. All four "
               "series sit near 1x up to a thousand entities. Beyond that the cheap "
               "scrambled series rises to 2.40x, cheap aligned to 1.31x, real "
               "scrambled to 1.34x, and real aligned stays at 0.96x.", b)


# ===========================================================================
# Figure 5 — the structural-change result  (§5.3)
# ===========================================================================
def fig_churn():
    uid = "l57f5"
    W, H = 720, 340
    b = [label(20, 20, "Nanoseconds per structural change, at 100,000 entities. "
                       "The axis that matters is entity WIDTH.", "sm", "start")]

    px0, py0, pw, ph = 150, 52, 430, 168
    top = 64.0
    b.append(box(px0, py0, pw, ph, dash="2 4"))
    for v in (0, 16, 32, 48, 64):
        y = py0 + ph - (v / top) * ph
        b.append(rule(px0, y, px0 + pw, y, "grid", 1.0, "2 4"))
        b.append(label(px0 - 8, y + 4, f"{v}", "xs muted", "end"))
    b.append(label(px0 - 8, py0 - 6, "ns / op", "xs muted", "end"))

    bars = [("archetype, 4 components", 13.10, AMBER, 0),
            ("archetype, 12 components", 58.48, RED, 1),
            ("sparse set, 4 components", 4.25, GREEN, 2),
            ("sparse set, 12 components", 4.23, GREEN, 3)]
    bw = 56
    for name, v, colour, i in bars:
        x = px0 + 34 + i * 100
        h = (v / top) * ph
        b.append(box(x, py0 + ph - h, bw, h, colour=colour, opacity=0.30))
        b.append(label(x + bw / 2, py0 + ph - h - 6, f"{v:.1f}", "xs t-hi"))
        b.append(label(x + bw / 2, py0 + ph + 16, name.split(", ")[1], "xs muted"))
    b.append(label(px0 + 84, py0 + ph + 32, "archetype", "xs"))
    b.append(label(px0 + 284, py0 + ph + 32, "sparse set", "xs"))

    # The 4.5x annotation belongs BETWEEN the two archetype bars, and its text has
    # to clear both — the first version drew the label straight across the 58.5 bar.
    # Start clear of the "13.1" value label, which sits at exactly the bar's top.
    b.append(arrow(px0 + 98, py0 + ph - (13.10 / top) * ph + 2,
                   px0 + 152, py0 + ph - (58.48 / top) * ph - 5, uid, "b", 1.6))
    b.append(f'<text x="{px0 + 204}" y="{py0 + 30}" class="xs" style="fill:{RED}"'
             f' text-anchor="start">4.5&#215;, for eight components</text>')
    b.append(f'<text x="{px0 + 204}" y="{py0 + 44}" class="xs" style="fill:{RED}"'
             f' text-anchor="start">no query ever reads</text>')

    # …and "flat" has to CONNECT the two sparse bars, or it looks like a label on
    # one of them rather than a statement about the pair.
    y_flat = py0 + ph - (4.25 / top) * ph - 16
    b.append(f'<line x1="{px0 + 62 + 200}" y1="{y_flat}" x2="{px0 + 62 + 300}"'
             f' y2="{y_flat}" stroke="{GREEN}" stroke-width="1.2"'
             f' stroke-dasharray="3 3"/>')
    b.append(f'<text x="{px0 + 62 + 250}" y="{y_flat - 5}" class="xs" style="fill:{GREEN}"'
             f' text-anchor="middle">did not move</text>')

    b.append(rule(24, 268, 696, 268, "grid", 1.2))
    b.append(label(24, 288, "The archetype's cost is the entity's TOTAL WIDTH; the sparse "
                            "set's is one pool, whatever else the entity has.",
                   "xs t-hi", "start"))
    b.append(label(24, 306, "A real entity carries ten to thirty components. Four is the "
                            "number that flatters the archetype, and this course will "
                            "not", "xs", "start"))
    b.append(label(24, 322, "stay at four past Module 8.", "xs", "start"))
    return svg(uid, W, H, "Cost of one structural change in each design, at two entity widths",
               "A bar chart of nanoseconds per add or remove at 100,000 entities. "
               "The archetype costs 13.1 ns for a four-component entity and 58.5 ns "
               "for a twelve-component one. The sparse set costs 4.25 and 4.23 ns "
               "respectively — flat in entity width.", b)


# ===========================================================================
# Figure 6 — selectivity, and the group  (§5.5)
# ===========================================================================
def fig_group():
    uid = "l57f6"
    W, H = 720, 400
    b = [label(20, 20, "A query matching one entity in four — the archetype's best case "
                       "— and the move that answers it.", "sm", "start")]

    # top-left: archetype
    b.append(label(24, 48, "ARCHETYPE: the matching entities are a chunk of their own.",
                   "xs t-hi", "start"))
    for i in range(8):
        b.append(box(60 + i * 34, 58, 30, 16, colour=BLUE, opacity=0.26))
    b.append(label(346, 70, "8 rows, contiguous, all wanted", "xs muted", "start"))

    b.append(label(24, 100, "SPARSE SET: the velocity pool is dense, but the transforms "
                            "it reaches are one in four.", "xs t-hi", "start"))
    for i in range(8):
        b.append(box(60 + i * 34, 110, 30, 16, colour=BLUE, opacity=0.26))
    b.append(label(346, 122, "vel.dense — dense, all wanted", "xs muted", "start"))
    for i in range(32):
        wanted = i % 4 == 0
        colour = AMBER if wanted else GREY
        b.append(f'<rect x="{60 + i * 8.5}" y="138" width="7" height="16" rx="2"'
                 f' fill="{colour}" fill-opacity="{0.45 if wanted else 0.06}"'
                 f' stroke="{colour}" stroke-opacity="{1.0 if wanted else 0.35}"'
                 f' stroke-width="1.2"/>')
    b.append(label(346, 150, "xf.data — three quarters fetched for nothing", "xs muted", "start"))
    for i in range(8):
        b.append(arrow(75 + i * 34, 128, 63.5 + i * 34, 136, uid, "s", 0.8))
    b.append(label(24, 176, "Every cache line the transform pool hands over contains one "
                            "wanted transform and two unwanted ones. Measured:",
                   "xs", "start"))
    b.append(f'<text x="24" y="192" class="xs" style="fill:{RED}" text-anchor="start">'
             f'1.46x on the real body at 100,000 entities, and 1.94x once the pools’ '
             f'orders have diverged.</text>')

    b.append(rule(24, 208, 696, 208, "grid", 1.2))

    # the group
    b.append(label(24, 230, "THE GROUP — sort both pools so the members come first, in "
                            "the same order.", "xs t-ok", "start"))
    for i in range(8):
        b.append(box(60 + i * 34, 242, 30, 16, colour=BLUE, opacity=0.26))
    b.append(label(346, 254, "vel.dense", "xs mono muted", "start"))
    for i in range(8):
        b.append(box(60 + i * 34, 268, 30, 16, colour=AMBER, opacity=0.26))
    for i in range(8, 32):
        b.append(f'<rect x="{60 + (i - 8) * 11.4}" y="292" width="10" height="10" rx="2"'
                 f' fill="{GREY}" fill-opacity="0.06" stroke="{GREY}"'
                 f' stroke-opacity="0.35" stroke-width="1.2"/>')
    b.append(label(346, 280, "xf.dense — members moved to the front", "xs mono muted", "start"))
    b.append(label(346, 301, "non-members, still there, behind them", "xs muted", "start"))
    for i in range(8):
        b.append(rule(75 + i * 34, 260, 75 + i * 34, 266, "hi", 1.2))
    b.append(label(24, 322, "Index i of one array and index i of the other are now the "
                            "same entity — which is exactly the guarantee an archetype "
                            "chunk", "xs", "start"))
    b.append(label(24, 336, "gives. No sparse read at all. Measured: 0.99x to 1.01x of "
                            "the archetype, at every size.", "xs t-ok", "start"))

    b.append(rule(24, 352, 696, 352, "grid", 1.2, "2 3"))
    b.append(label(24, 372, "THE MIGRATION ONLY RUNS ONE WAY.", "xs t-hi", "start"))
    b.append(label(232, 372, "A sparse set can be given an archetype's query later, one "
                             "group at a time.", "xs", "start"))
    b.append(label(24, 388, "An archetype cannot be given a sparse set's O(1) structural "
                            "change at all. That asymmetry is the decision.",
                   "xs", "start"))
    return svg(uid, W, H, "A selective query, and the group that answers it",
               "Top: an archetype holding the matching entities as a contiguous "
               "chunk, compared with a sparse set whose dense velocity pool reaches "
               "into a transform pool where only one element in four is wanted. "
               "Bottom: the same sparse set with both pools sorted so the members "
               "occupy the front of each in the same order, which removes the "
               "redirect entirely.", b)


FIGS = {
    "l57_fig1.svg": fig_shapes,      # §2.2
    "l57_fig2.svg": fig_redirect,    # §2.3
    "l57_fig3.svg": fig_move,        # §3.2
    "l57_fig4.svg": fig_query,       # §5.1
    "l57_fig5.svg": fig_churn,       # §5.3
    "l57_fig6.svg": fig_group,       # §5.5
}


def main():
    for name, fn in FIGS.items():
        with open(os.path.join(OUT, name), "w") as fh:
            fh.write(fn())
        print(f"wrote {os.path.join(OUT, name)}")


if __name__ == "__main__":
    main()
