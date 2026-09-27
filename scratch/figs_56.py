#!/usr/bin/env python3
"""scratch/figs_56.py — Lesson 5.6's diagrams.

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
# Figure 1 — what the scene weighs, and where ours sits
# ===========================================================================
def fig_scale():
    uid = "l56f1"
    W, H = 720, 316
    b = [label(20, 20, "Before any argument about layout: how much data is there, "
                       "and where does it fit?", "sm", "start")]

    b.append(label(24, 48, "ONE scene_object = 96 bytes = 1.5 cache lines", "xs t-hi", "start"))
    # a 96-byte object drawn against 64-byte lines
    x0, y0 = 24, 60
    for i in range(2):
        b.append(box(x0 + i * 200, y0, 200, 22, dash="3 3"))
        b.append(label(x0 + i * 200 + 100, y0 + 15, f"cache line {i}  (64 B)", "xs muted"))
    b.append(box(x0, y0 + 28, 300, 24, colour=AMBER, opacity=0.22))
    b.append(label(x0 + 94, y0 + 44, "transform  60 B", "xs"))
    b.append(box(x0 + 187, y0 + 28, 113, 24, colour=GREY, opacity=0.22))
    b.append(label(x0 + 243, y0 + 44, "the rest 36 B", "xs"))
    b.append(label(336, y0 + 44, "the full-transform loop reads the amber part;", "xs muted", "start"))
    b.append(label(336, y0 + 58, "the cull loop reads 12 bytes of it.", "xs muted", "start"))

    # working sets
    b.append(rule(24, 142, 696, 142, "grid", 1.0))
    b.append(label(24, 162, "WORKING SET OF A FLAT ARRAY, against this machine's caches",
                   "xs t-hi", "start"))
    rows = [("4", "0.4 KB", "L1", GREEN, "<- OUR SCENE. k_max_objects = 4."),
            ("100", "9.4 KB", "L1", GREEN, ""),
            ("1,000", "93.8 KB", "L2", BLUE, ""),
            ("10,000", "937.5 KB", "L2 / L3", AMBER, "<- the knee is in here"),
            ("100,000", "9,375 KB", "L3 / DRAM", RED, "")]
    for i, (n, kb, where, colour, note) in enumerate(rows):
        y = 184 + i * 24
        b.append(label(96, y, n + " objects", "xs mono", "end"))
        b.append(label(180, y, kb, "xs mono muted", "end"))
        b.append(box(196, y - 11, 92, 16, colour=colour, opacity=0.20))
        b.append(label(242, y, where, "xs"))
        if note:
            b.append(label(300, y, note, "xs t-hi", "start"))

    b.append(rule(24, 292, 696, 292, "grid", 1.0, "2 3"))
    b.append(label(24, 308, "Every claim in this lesson is a claim about WHICH ROW "
                            "you are on. A rule with no N in it is not a rule.",
                   "xs t-hi", "start"))
    return svg(uid, W, H, "The size of a scene object and the working sets it implies",
               "A 96-byte scene_object drawn against two 64-byte cache lines, of "
               "which 60 bytes are the transform, and a table of working-set sizes "
               "for 4 to 100,000 objects against L1, L2, L3 and DRAM.", b)


# ===========================================================================
# Figure 2 — six layouts, as memory
# ===========================================================================
def fig_layouts():
    uid = "l56f2"
    W, H = 720, 330
    b = [label(20, 20, "The same scene, six ways. Shading marks the bytes the "
                       "full-transform loop actually reads.", "sm", "start")]

    def strip(y, title, cells, note):
        out = [label(24, y - 6, title, "xs t-hi", "start")]
        for cx, cw, colour, op, txt in cells:
            out.append(box(cx, y, cw, 22, colour=colour, opacity=op))
            if txt:
                out.append(label(cx + cw / 2, y + 15, txt, "xs"))
        out.append(label(24, y + 38, note, "xs muted", "start"))
        return out

    # FLAT
    cells = []
    for i in range(6):
        cells.append((24 + i * 74, 46, AMBER, 0.22, "T"))
        cells.append((70 + i * 74, 28, GREY, 0.18, ""))
    b += strip(46, "FLAT — an array of scene_object. What the engine has.", cells,
               "One stride, one direction, hardware prefetch works. 96 B apart; "
               "36 of every 96 are read for nothing.")

    # POINTERS
    cells = []
    for i in range(6):
        cells.append((24 + i * 74, 30, BLUE, 0.20, "p"))
    b += strip(122, "POINTERS — an array of scene_object*, objects on the heap.",
               cells, "")
    # The arrows fan into the left two thirds only, and the note sits clear to
    # their right — the first version put the note at y+38 straight through them.
    for i in range(6):
        b.append(arrow(39 + i * 74, 144, 40 + ((i * 61) % 380), 172, uid, "s", 0.9))
    for i in range(6):
        b.append(box(24 + ((i * 61) % 380), 174, 34, 14, colour=GREY, opacity=0.16))
    b.append(label(440, 152, "Eight bytes each, then a load to", "xs muted", "start"))
    b.append(label(440, 166, "somewhere else. Free while the", "xs muted", "start"))
    b.append(label(440, 180, "objects stay in cache; not free", "xs muted", "start"))
    b.append(label(440, 194, "afterwards.", "xs muted", "start"))

    # SOA
    b += strip(214, "SoA — three parallel arrays: position, rotation, scale.",
               [(24, 200, AMBER, 0.22, "position[]"),
                (228, 260, AMBER, 0.22, "rotation[]"),
                (492, 180, AMBER, 0.22, "scale[]")],
               "Every byte fetched is a byte wanted. A loop that needs only one "
               "array touches only one array.")

    # TREE
    b += strip(280, "TREE — nodes linked by pointers, walked depth-first.",
               [(24, 60, PURPLE, 0.20, "root")],
               "")
    for i in range(5):
        x = 100 + i * 76
        b.append(box(x, 280, 60, 22, colour=PURPLE, opacity=0.20))
        b.append(arrow(x - 14, 291, x - 2, 291, uid, "s", 1.0))
    b.append(label(24, 318, "The address of the next node is INSIDE the current "
                            "one, so the CPU cannot start fetching it until this "
                            "one arrives. Figure 3 is that sentence.",
                   "xs muted", "start"))
    return svg(uid, W, H, "Four scene layouts drawn as memory",
               "A flat array of 96-byte objects; an array of pointers into a "
               "scattered heap; three parallel arrays; and a chain of linked "
               "nodes.", b)


# ===========================================================================
# Figure 3 — the knee
# ===========================================================================
def fig_knee():
    uid = "l56f4"
    W, H = 720, 350

    series = [
        ("tree_shuffled", [1.06, 1.04, 1.03, 4.86, 6.47], RED),
        ("ptr_shuffled", [0.99, 1.00, 1.13, 1.24, 2.38], AMBER),
        ("ptr_ordered", [1.00, 1.01, 1.00, 1.01, 1.98], BLUE),
        ("tree_fresh", [1.06, 1.04, 1.04, 1.28, 1.68], PURPLE),
        ("virtual", [1.55, 1.55, 1.55, 1.57, 1.70], GREY),
        ("soa", [1.00, 0.68, 0.64, 0.66, 0.68], GREEN),
    ]
    xs_lab = ["4", "100", "1,000", "10,000", "100,000"]

    b = [label(20, 20, "Cost relative to the flat array, against scene size. "
                       "1.00x means the layout made no difference.", "sm", "start")]

    px0, py0, pw, ph = 92, 52, 470, 210
    top = 7.0
    b.append(box(px0, py0, pw, ph, dash="2 4"))
    for v in (1, 2, 3, 4, 5, 6, 7):
        y = py0 + ph - (v / top) * ph
        b.append(rule(px0, y, px0 + pw, y, "grid", 1.0, "2 4"))
        b.append(label(px0 - 8, y + 4, f"{v}x", "xs muted", "end"))
    # the 1x line, emphasised
    y1 = py0 + ph - (1.0 / top) * ph
    b.append(rule(px0, y1, px0 + pw, y1, "ink", 1.4))

    for i, lab in enumerate(xs_lab):
        x = px0 + (i / 4.0) * pw
        b.append(label(x, py0 + ph + 16, lab, "xs muted"))
    b.append(label(px0 + pw / 2, py0 + ph + 32, "objects in the scene", "xs muted"))

    # Draw the curves, then place the right-hand labels with a de-overlap pass:
    # ptr_ordered (1.98), tree_fresh (1.68) and virtual (1.70) end within 0.3x of
    # each other, so at this scale their labels land on top of one another. Sort by
    # y and push each one at least 13 units below the last.
    label_slots = []
    for name, vals, colour in series:
        pts = []
        for i, v in enumerate(vals):
            x = px0 + (i / 4.0) * pw
            y = py0 + ph - (min(v, top) / top) * ph
            pts.append(f"{x:.1f},{y:.1f}")
            b.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="2.6" fill="{colour}"/>')
        b.append(f'<polyline points="{" ".join(pts)}" fill="none" stroke="{colour}"'
                 f' stroke-width="1.6"/>')
        label_slots.append([py0 + ph - (min(vals[-1], top) / top) * ph, name, colour,
                            pts[-1]])

    label_slots.sort(key=lambda r: r[0])
    for i in range(1, len(label_slots)):
        if label_slots[i][0] - label_slots[i - 1][0] < 13.0:
            label_slots[i][0] = label_slots[i - 1][0] + 13.0

    # `style="fill:…"`, NOT `fill="…"`. `course.css` has a bare
    # `figure.dia svg text { fill: var(--dia-ink) }`, and a CSS declaration beats an
    # SVG PRESENTATION ATTRIBUTE — so `fill="#e05c5c"` on a <text> is silently
    # ignored and every one of these six labels rendered in the theme ink. Measured:
    # getComputedStyle() said rgb(51,51,46) for all of them, and the screenshot did
    # not make it obvious because each label sits beside its own coloured leader.
    # An inline STYLE attribute outranks any author selector, so it wins.
    # (`docs/_template/apply-shared.py` lints for the attribute form and is what
    # caught this.)
    for ly, name, colour, last in label_slots:
        lx, lyy = (float(v) for v in last.split(","))
        b.append(f'<line x1="{lx + 3:.1f}" y1="{lyy:.1f}" x2="{px0 + pw + 5:.1f}"'
                 f' y2="{ly:.1f}" stroke="{colour}" stroke-width="0.8"'
                 f' stroke-dasharray="2 2"/>')
        b.append(f'<text x="{px0 + pw + 9}" y="{ly + 4:.1f}" class="xs mono"'
                 f' style="fill:{colour}" text-anchor="start">{esc(name)}</text>')

    b.append(rule(24, 288, 696, 288, "grid", 1.2))
    b.append(label(24, 306, "READ THE LEFT HALF FIRST.", "xs t-ok", "start"))
    b.append(label(178, 306, "Up to a thousand objects every layout is within 13% "
                             "of every other, except", "xs", "start"))
    b.append(label(24, 322, "the virtual call. THE KNEE IS BETWEEN 1,000 AND 10,000 "
                            "— which is where 96 bytes an object stops fitting in L2.",
                   "xs", "start"))
    b.append(label(24, 340, "Our scene is FOUR objects. On this data, nothing in "
                            "this lesson justifies changing it.", "xs t-hi", "start"))
    return svg(uid, W, H, "Cost of each layout relative to a flat array, versus scene size",
               "A line chart with scene size on the x axis from 4 to 100,000 and "
               "cost relative to a flat array on the y axis. All layouts sit near "
               "1x until 1,000 objects; beyond that the shuffled tree rises to "
               "6.5x, shuffled pointers to 2.4x, and SoA stays below 1x.", b)


# ===========================================================================
# Figure 4 — two SoA wins, two different causes
# ===========================================================================
def fig_two_soa():
    uid = "l56f5"
    W, H = 720, 320
    b = [label(20, 20, "SoA wins twice, for two completely different reasons — and "
                       "one experiment tells them apart.", "sm", "start")]

    xs_lab = ["4", "100", "1,000", "10,000", "100,000"]
    rows = [
        ("FULL TRANSFORM", "reads 60 of 96 bytes", 52,
         [1.00, 0.68, 0.64, 0.66, 0.68], [1.00, 1.00, 0.99, 1.00, 1.00]),
        ("CULL", "reads 12 of 96 bytes", 168,
         [0.82, 0.58, 0.52, 0.19, 0.19], [1.00, 0.97, 0.98, 0.37, 0.36]),
    ]

    for title, sub, y0, vec, scalar in rows:
        b.append(label(24, y0, title, "xs t-hi", "start"))
        b.append(label(24, y0 + 14, sub, "xs muted", "start"))
        b.append(label(200, y0 - 12, "-O2", "xs muted"))
        b.append(label(200, y0 + 2, "(vectorised)", "xs muted"))
        b.append(label(200, y0 + 46, "-fno-vectorize", "xs muted"))
        for i, lab in enumerate(xs_lab):
            x = 288 + i * 82
            b.append(label(x, y0 - 12, lab, "xs muted"))
            for j, (vals, yy) in enumerate(((vec, y0 + 2), (scalar, y0 + 46))):
                v = vals[i]
                colour = GREEN if v < 0.9 else GREY
                b.append(box(x - 34, yy - 12, 68, 18, colour=colour, opacity=0.20))
                b.append(label(x, yy + 1, f"{v:.2f}x", "xs mono"))
        b.append(rule(24, y0 + 64, 696, y0 + 64, "grid", 1.0, "2 3"))

    b.append(label(24, 122, "The win is the SAME at every size — 384 bytes and 9.6 "
                            "MB alike — and it VANISHES with the vectoriser off.",
                   "xs", "start"))
    b.append(label(24, 136, "It is not a cache effect at all. It is that a contiguous "
                            "array vectorises and a 96-byte stride does not.",
                   "xs t-hi", "start"))

    b.append(label(24, 238, "The win APPEARS at 10,000 and not before, and survives "
                            "the vectoriser being switched off.", "xs", "start"))
    b.append(label(24, 252, "This one is the cache: 12 useful bytes per 96 fetched, "
                            "which stops being free when the array leaves L2.",
                   "xs t-hi", "start"))

    b.append(rule(24, 268, 696, 268, "grid", 1.2))
    b.append(label(24, 286, "THE RULE THIS PRODUCES is not “use SoA”. It is: "
                            "SPLIT THE DATA A LOOP DOES NOT READ AWAY FROM THE DATA "
                            "IT DOES,", "xs t-hi", "start"))
    b.append(label(24, 302, "and the size of the prize is the fraction you leave "
                            "behind. 60 of 96 buys nothing; 12 of 96 buys 5x.",
                   "xs", "start"))
    return svg(uid, W, H, "SoA measured with and without vectorisation, on two workloads",
               "Two blocks of five ratios each. On the full-transform workload SoA "
               "wins about 0.66x at every size with vectorisation and exactly 1.00x "
               "without it. On the cull workload it wins only above ten thousand "
               "objects, and the win survives with vectorisation disabled.", b)


# ===========================================================================
# Figure 5 — what a virtual call actually costs
# ===========================================================================
def fig_virtual():
    uid = "l56f6"
    W, H = 720, 300
    b = [label(20, 20, "The cost of `virtual` — and the thing it is NOT.",
               "sm", "start")]

    xs_lab = ["4", "100", "1,000", "10,000", "100,000"]
    mono = [1.55, 1.55, 1.55, 1.57, 1.70]
    poly = [1.49, 1.56, 1.53, 1.57, 1.68]

    b.append(label(24, 56, "one implementation", "xs", "start"))
    b.append(label(24, 70, "(perfectly predictable)", "xs muted", "start"))
    b.append(label(24, 106, "two implementations", "xs", "start"))
    b.append(label(24, 120, "(interleaved, unpredictable)", "xs muted", "start"))

    for i, lab in enumerate(xs_lab):
        x = 250 + i * 88
        b.append(label(x, 38, lab, "xs muted"))
        for vals, y in ((mono, 60), (poly, 110)):
            b.append(box(x - 36, y - 13, 72, 20, colour=GREY, opacity=0.20))
            b.append(label(x, y + 2, f"{vals[i]:.2f}x", "xs mono"))

    b.append(rule(24, 140, 696, 140, "grid", 1.2))
    b.append(label(24, 160, "THEY ARE THE SAME.", "xs t-bad", "start"))
    b.append(label(150, 160, "Within 4% at every size. Whatever `virtual` costs "
                             "here, it is not branch misprediction —", "xs", "start"))
    b.append(label(24, 176, "the CPU predicts an unpredictable indirect call about "
                            "as well as a trivial one, because it is not the "
                            "prediction that hurts.", "xs", "start"))

    b.append(label(24, 206, "AND IT IS FLAT ACROSS N.", "xs t-hi", "start"))
    b.append(label(178, 206, "1.55x at four objects, in L1, where there is no memory "
                             "effect to have. So it is not", "xs", "start"))
    b.append(label(24, 222, "the vtable load either. What is left is INLINING: the "
                            "compiler cannot see through the call, so it cannot "
                            "fuse,", "xs", "start"))
    b.append(label(24, 238, "reorder or vectorise the sixteen stores of a matrix "
                            "build across iterations.", "xs", "start"))

    b.append(rule(24, 254, 696, 254, "grid", 1.0, "2 3"))
    b.append(label(24, 272, "This is the number that most surprised the author, and "
                            "it took removing a confound to get: an earlier version "
                            "read only five", "xs muted", "start"))
    b.append(label(24, 288, "of the sixteen matrix entries, which let every INLINED "
                            "arm skip work the virtual one had to do. It read 3.4x. "
                            "It was wrong.", "xs muted", "start"))
    return svg(uid, W, H, "The measured cost of a virtual call, monomorphic versus polymorphic",
               "Two rows of five ratios: a monomorphic virtual call and a "
               "polymorphic one, at scene sizes from 4 to 100,000. They agree "
               "within 4% everywhere and are roughly constant at 1.5 to 1.7 times "
               "the cost of a direct call.", b)


# ===========================================================================
# Figure 6 — an array of pointers is not a linked list
# ===========================================================================
def fig_chain():
    uid = "l56f3"
    W, H = 720, 322
    b = [label(20, 20, "Both are “pointer chasing”, and they are not the same thing "
                       "at all: 2.4x against 6.5x.", "sm", "start")]

    # array of pointers
    b.append(label(24, 52, "ARRAY OF POINTERS — the addresses are known up front",
                   "xs t-hi", "start"))
    for i in range(5):
        b.append(box(24 + i * 56, 62, 48, 20, colour=BLUE, opacity=0.20))
        b.append(label(48 + i * 56, 76, f"p{i}", "xs mono"))
    for i in range(5):
        b.append(arrow(48 + i * 56, 84, 30 + ((i * 61) % 240), 112, uid, "s", 0.9))
    for i in range(5):
        b.append(box(12 + ((i * 61) % 240), 114, 44, 18, colour=GREY, opacity=0.18))

    # Clear of the fan, which spans x = 12..300.
    b.append(label(330, 60, "The CPU can read p0..p4 in one go and issue FIVE",
                   "xs", "start"))
    b.append(label(330, 76, "loads at once. They miss in parallel; the misses",
                   "xs", "start"))
    b.append(label(330, 92, "OVERLAP. That is memory-level parallelism, and it",
                   "xs", "start"))
    b.append(label(330, 108, "is why scattering objects costs 2.4x and not 20x.",
                   "xs", "start"))
    b.append(label(330, 130, "A dozen misses in flight is a factor of a dozen.",
                   "xs t-ok", "start"))

    b.append(rule(24, 158, 696, 158, "grid", 1.0))

    # linked list
    b.append(label(24, 180, "LINKED LIST — each address is INSIDE the previous node",
                   "xs t-hi", "start"))
    for i in range(5):
        x = 24 + i * 120
        b.append(box(x, 192, 96, 26, colour=RED, opacity=0.16))
        b.append(label(x + 48, 209, f"node {i}", "xs mono"))
        if i < 4:
            b.append(arrow(x + 96, 205, x + 118, 205, uid, "b", 1.4))
    b.append(label(24, 240, "Load node 0 (miss, ~80 ns). Only then do you learn "
                            "node 1's address. Load it (miss). Only then…",
                   "xs", "start"))
    b.append(label(24, 256, "The misses are SERIAL — a dependency chain — so they "
                            "add up instead of overlapping.", "xs t-bad", "start"))

    b.append(rule(24, 274, 696, 274, "grid", 1.2))
    b.append(label(24, 292, "A scene TREE is the second picture. That is what "
                            "creaks — not the hierarchy, not the OOP, not the "
                            "virtual calls:", "xs t-hi", "start"))
    b.append(label(24, 308, "the fact that finding the next thing to work on "
                            "requires having finished fetching the last one.",
                   "xs t-hi", "start"))
    return svg(uid, W, H, "Memory-level parallelism versus a serial dependency chain",
               "Top: an array of pointers, whose five addresses can be read at "
               "once so five cache misses overlap. Bottom: a linked list, where "
               "each node's address is stored inside the previous node, so the "
               "misses must happen one after another.", b)


# NUMBERED BY PAGE ORDER, not by the order they were written — 5.3's trap, and
# the first build of 5.6 fell into it too. `fig_chain` is authored last and appears
# third (in §3.3); `fig_knee` is authored third and appears fourth (in §5.1). The
# filename is what keeps the generator and the builder honest about it.
FIGS = {
    "l56_fig1.svg": fig_scale,       # §2.1
    "l56_fig2.svg": fig_layouts,     # §2.2
    "l56_fig3.svg": fig_chain,       # §3.3
    "l56_fig4.svg": fig_knee,        # §5.1
    "l56_fig5.svg": fig_two_soa,     # §5.2
    "l56_fig6.svg": fig_virtual,     # §5.3
}


def main():
    for name, fn in FIGS.items():
        with open(os.path.join(OUT, name), "w") as fh:
            fh.write(fn())
        print(f"wrote {os.path.join(OUT, name)}")


if __name__ == "__main__":
    main()
