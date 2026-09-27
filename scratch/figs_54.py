#!/usr/bin/env python3
"""scratch/figs_54.py — Lesson 5.4's diagrams.

Same rules as 5.3's: computed coordinates, no fill="..." on any <text> (CSS wins
over presentation attributes on text), every line kept well inside the viewBox,
and no right-anchored label near x=0.

Filenames are numbered by PAGE ORDER, which is the trap 5.3 fell into.
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
# Figure 1 — the question a pointer cannot answer
# ===========================================================================
def fig_question():
    uid = "l54f1"
    W, H = 720, 330
    b = [label(20, 20, "One question, asked of the two things a program can hold.",
               "sm", "start")]

    # ---- left: the pointer ------------------------------------------------
    b.append(label(24, 48, "A POINTER  (mesh, std::span, T*)", "xs t-hi", "start"))
    b.append(box(24, 58, 310, 30))
    b.append(label(179, 77, "0x7ffd_9c40_0018", "sm mono"))

    b.append(label(24, 108, "before the free", "xs muted", "start"))
    b.append(box(24, 116, 310, 26, colour=GREEN, opacity=0.16))
    b.append(label(179, 133, "vertices — 8 positions, 36 indices", "xs"))

    b.append(label(24, 168, "after the free", "xs muted", "start"))
    b.append(box(24, 176, 310, 26, colour=RED, opacity=0.16))
    b.append(label(179, 193, "whatever moved in. Or nothing. Or the same bytes.", "xs"))

    b.append(rule(24, 216, 334, 216, "grid", 1.0, "3 3"))
    b.append(label(24, 236, "is this still valid?", "sm mono t-bad", "start"))
    b.append(label(24, 254, "THE POINTER HOLDS THE SAME BITS EITHER WAY.", "xs t-bad", "start"))
    b.append(label(24, 270, "There is no test, no assertion, no flag. From the", "xs muted", "start"))
    b.append(label(24, 284, "pointer's side, nothing happened.", "xs muted", "start"))

    # ---- right: the handle ------------------------------------------------
    b.append(label(386, 48, "A HANDLE  (index + generation)", "xs t-hi", "start"))
    b.append(box(386, 58, 310, 30))
    b.append(rule(560, 58, 560, 88, "grid", 1.2))
    b.append(label(473, 77, "generation 3", "sm mono"))
    b.append(label(628, 77, "index 17", "sm mono"))

    b.append(label(386, 108, "slot 17 holds", "xs muted", "start"))
    b.append(box(386, 116, 310, 26, colour=GREEN, opacity=0.16))
    b.append(label(541, 133, "generation 3  →  the mesh you were given", "xs"))

    b.append(label(386, 168, "after a free and a refill", "xs muted", "start"))
    b.append(box(386, 176, 310, 26, colour=BLUE, opacity=0.16))
    b.append(label(541, 193, "generation 4  →  somebody else's mesh", "xs"))

    b.append(rule(386, 216, 696, 216, "grid", 1.0, "3 3"))
    b.append(label(386, 236, "3 != 4  →  nullptr", "sm mono t-ok", "start"))
    b.append(label(386, 254, "THE HANDLE CARRIES ITS OWN ANSWER.", "xs t-ok", "start"))
    b.append(label(386, 270, "Two loads, two compares, no dereference — and the", "xs muted", "start"))
    b.append(label(386, 284, "failure happens at the lookup, not three frames later.", "xs muted", "start"))

    b.append(rule(360, 40, 360, 296, "grid", 1.0))
    return svg(uid, W, H, "A pointer versus a handle, asked whether it is still valid",
               "Left: a pointer's bit pattern is identical before and after the "
               "memory it names is freed, so the program cannot tell. Right: a "
               "handle carries a generation which no longer matches the slot's, so "
               "the lookup returns nullptr.", b)


# ===========================================================================
# Figure 2 — three failures, three designs
# ===========================================================================
def fig_failures():
    uid = "l54f2"
    W, H = 720, 322
    b = [label(20, 20, "Three ways a reference goes wrong, and what each design "
                       "survives.", "sm", "start")]

    # Columns start well right of the row labels, which is where the first
    # version of this figure collided.
    cols = [("RAW POINTER", 241), ("BARE INDEX", 423), ("INDEX + GENERATION", 605)]
    half = 86
    rows = [
        ("DANGLING", "the object was freed", 118),
        ("ALIASING", "the slot was refilled", 176),
        ("RELOCATION", "the storage moved", 234),
    ]

    verdicts = [
        [("undefined", RED, "reads freed memory"),
         ("detected", GREEN, "the index is out of range"),
         ("detected", GREEN, "the generation differs")],
        [("undefined", RED, "reads the new object"),
         ("WORSE", RED, "silently valid"),
         ("detected", GREEN, "the generation differs")],
        [("undefined", RED, "the address is stale"),
         ("survives", GREEN, "an index does not move"),
         ("survives", GREEN, "an index does not move")],
    ]

    for name, cx in cols:
        b.append(label(cx, 60, name, "xs t-hi"))
    b.append(rule(24, 70, 696, 70, "grid", 1.0))

    for r, (title, sub, y) in enumerate(rows):
        b.append(label(24, y, title, "sm", "start"))
        b.append(label(24, y + 16, sub, "xs muted", "start"))
        for c, (name, cx) in enumerate(cols):
            word, colour, note = verdicts[r][c]
            b.append(box(cx - half, y - 12, half * 2, 24, colour=colour, opacity=0.15))
            cls = "xs t-bad" if colour == RED else "xs t-ok"
            b.append(label(cx, y + 4, word, cls))
            b.append(label(cx, y + 26, note, "xs muted"))
        if r < 2:
            b.append(rule(24, y + 36, 696, y + 36, "grid", 1.0, "2 3"))

    b.append(rule(24, 268, 696, 268, "grid", 1.2))
    b.append(label(24, 288, "READ THE MIDDLE CELL.", "xs t-bad", "start"))
    b.append(label(150, 288, "An index alone fixes relocation and makes aliasing "
                             "WORSE — a stale", "xs", "start"))
    b.append(label(24, 306, "index is in range, points at a live object, and is "
                            "wrong. The generation is what closes it.", "xs", "start"))
    return svg(uid, W, H, "Three failure modes against three designs",
               "A 3x3 table. Rows: dangling, aliasing, relocation. Columns: raw "
               "pointer, bare index, index plus generation. The raw pointer is "
               "undefined in all three; the bare index detects dangling and "
               "survives relocation but makes aliasing silently wrong; index plus "
               "generation detects or survives all three.", b)


# ===========================================================================
# Figure 3 — the bit budget
# ===========================================================================
def fig_bits():
    uid = "l54f3"
    W, H = 720, 340
    b = [label(20, 20, "One 32-bit word, split 20 / 12 — and the arithmetic that "
                       "chose the split.", "sm", "start")]

    # The word.
    x0, y0, w, h = 40, 52, 640, 40
    gen_w = w * 12 / 32.0
    b.append(box(x0, y0, gen_w, h, colour=PURPLE, opacity=0.18))
    b.append(box(x0 + gen_w, y0, w - gen_w, h, colour=BLUE, opacity=0.18))
    b.append(label(x0 + gen_w / 2, y0 + 25, "generation (12 bits)", "sm"))
    b.append(label(x0 + gen_w + (w - gen_w) / 2, y0 + 25, "index (20 bits)", "sm"))
    b.append(label(x0, y0 - 6, "bit 31", "xs muted", "start"))
    b.append(label(x0 + gen_w, y0 - 6, "bit 19", "xs muted"))
    b.append(label(x0 + w, y0 - 6, "bit 0", "xs muted", "end"))

    b.append(label(x0 + gen_w / 2, y0 + h + 18, "which occupant", "xs t-hi"))
    b.append(label(x0 + gen_w + (w - gen_w) / 2, y0 + h + 18, "which slot", "xs t-hi"))

    # The two budgets.
    b.append(rule(40, 128, 680, 128, "grid", 1.0))
    b.append(label(40, 150, "20 index bits", "sm mono", "start"))
    b.append(label(200, 150, "= 1,048,576 slots", "sm", "start"))
    b.append(label(400, 150, "a million meshes; something else breaks first", "xs muted", "start"))

    b.append(label(40, 176, "12 generation bits", "sm mono", "start"))
    b.append(label(200, 176, "= 4,095 usable", "sm", "start"))
    b.append(label(400, 176, "generation 0 is reserved, so the null handle is free",
                   "xs muted", "start"))

    # Wrap arithmetic.
    b.append(rule(40, 194, 680, 194, "grid", 1.0, "2 3"))
    b.append(label(40, 216, "TIME UNTIL ONE SLOT'S GENERATION REPEATS, at a given "
                            "recycle rate for that slot:", "xs t-hi", "start"))
    rates = [("once a frame, 60 Hz", "4,095 / 60", "68 seconds", RED),
             ("100 times a second", "4,095 / 100", "41 seconds", RED),
             ("once a second", "4,095 / 1", "68 minutes", AMBER),
             ("once a level load", "4,095 / 0.01", "4.7 days", GREEN)]
    for i, (what, sums, ans, colour) in enumerate(rates):
        y = 240 + i * 20
        b.append(label(56, y, what, "xs", "start"))
        b.append(label(240, y, sums, "xs mono muted", "start"))
        b.append(box(340, y - 11, 92, 16, colour=colour, opacity=0.18))
        b.append(label(386, y, ans, "xs"))
    b.append(label(452, 240, "So for ASSETS — loaded at level", "xs", "start"))
    b.append(label(452, 256, "boundaries, counted in hundreds —", "xs", "start"))
    b.append(label(452, 272, "12 bits is enormous. For ENTITIES", "xs", "start"))
    b.append(label(452, 288, "it is worth measuring, and pool", "xs", "start"))
    b.append(label(452, 304, "counts its wraps for exactly that.", "xs", "start"))
    b.append(label(40, 326, "The alternative — 64 bits split 32/32 — makes wrap "
                            "unreachable and doubles every handle in every component.",
                   "xs muted", "start"))
    return svg(uid, W, H, "The 32-bit handle layout and the wrap-time arithmetic",
               "A 32-bit word divided into a 12-bit generation field and a 20-bit "
               "index field, with the slot count, the usable generation count, and "
               "a table of times until one slot's generation repeats at four "
               "different recycle rates.", b)


# ===========================================================================
# Figure 4 — the anatomy of the pool
# ===========================================================================
def fig_anatomy():
    uid = "l54f4"
    W, H = 720, 376
    b = [label(20, 20, "Resolving handle 2:3 — two arrays and one indirection.",
               "sm", "start")]

    # ---- the handle, sitting above the slot it names ----------------------
    hx = 296
    b.append(box(hx, 38, 170, 30, colour=AMBER, opacity=0.18))
    b.append(label(hx + 85, 57, "gen 3  |  index 2", "sm mono"))
    b.append(label(hx, 84, "the handle", "xs muted", "start"))

    b.append(label(24, 48, "index 2 picks the slot;", "xs t-hi", "start"))
    b.append(label(24, 64, "generation 3 == generation 3,", "xs t-hi", "start"))
    b.append(label(24, 80, "so it is live, and it is YOURS.", "xs t-hi", "start"))

    # ---- slots_ : sparse, stable ------------------------------------------
    sy = 116
    b.append(label(24, 104, "slots_   sparse, STABLE", "xs t-hi", "start"))
    slots = [("gen 2", "dense 1"), ("gen 1", "dense —"), ("gen 3", "dense 0"),
             ("gen 1", "dense 2"), ("gen 5", "dense —")]
    for i, (g, d) in enumerate(slots):
        x = 24 + i * 136
        hit = (i == 2)
        if hit:
            b.append(box(x, sy, 128, 44, colour=AMBER, opacity=0.20))
        else:
            b.append(box(x, sy, 128, 44))
        b.append(label(x + 64, sy + 17, f"slot {i}", "xs muted"))
        b.append(label(x + 64, sy + 33, f"{g}  ·  {d}", "xs mono"))

    b.append(arrow(hx + 85, 70, 360, sy - 4, uid, "h", 1.4))

    # ---- the elbow down into items_ ---------------------------------------
    b.append(f'<path d="M360,{sy + 46} L360,186 L88,186 L88,214" fill="none"'
             f' class="hi" stroke-width="1.4" marker-end="url(#e-h-{uid})"/>')

    # ---- items_ : dense, mobile -------------------------------------------
    iy = 218
    b.append(label(210, 204, "items_   dense, MOBILE — packed, reallocates freely",
                   "xs t-hi", "start"))
    items = [("torus", True), ("cube", False), ("floor", False)]
    for i, (name, hit) in enumerate(items):
        x = 24 + i * 136
        if hit:
            b.append(box(x, iy, 128, 40, colour=GREEN, opacity=0.20))
        else:
            b.append(box(x, iy, 128, 40))
        b.append(label(x + 64, iy + 15, f"dense {i}", "xs muted"))
        b.append(label(x + 64, iy + 32, name, "sm mono"))
    b.append(box(24 + 3 * 136, iy, 128, 40, dash="3 3"))
    b.append(label(24 + 3 * 136 + 64, iy + 25, "(spare capacity)", "xs muted"))

    # ---- owners_ ----------------------------------------------------------
    oy = 282
    b.append(label(24, oy + 15, "owners_", "xs t-hi", "start"))
    for i, owner in enumerate(["2", "0", "3"]):
        x = 110 + i * 136
        b.append(box(x, oy, 128, 22))
        b.append(label(x + 64, oy + 15, f"slot {owner}", "xs mono"))

    b.append(label(24, 330, "owners_ is the way back, and the only reason removal "
                            "is O(1): when the last item is moved down into a hole, "
                            "its slot has to be told.", "xs muted", "start"))
    b.append(label(24, 346, "slot 1 and slot 4 are FREE, and “dense —” is the whole "
                            "occupancy flag — no extra byte, nothing to keep in sync.",
                   "xs muted", "start"))
    b.append(label(24, 366, "free_slots_ = [ 4, 1 ]   — LIFO, so the warmest slot is "
                            "reused first.", "xs mono muted", "start"))
    return svg(uid, W, H, "The pool's three arrays and how a handle resolves",
               "A handle carrying generation 3 and index 2 selects entry 2 of the "
               "sparse slots array, whose generation matches and whose dense field "
               "is 0, which selects entry 0 of the dense items array. A third "
               "array, owners, maps each dense entry back to its slot.", b)


# ===========================================================================
# Figure 5 — swap and patch
# ===========================================================================
def fig_remove():
    uid = "l54f5"
    W, H = 720, 350
    b = [label(20, 20, "remove(handle 1:1) — the two steps, and why the SURVIVOR "
                       "moves.", "sm", "start")]

    names = ["torus", "cube", "floor", "quad"]
    owners = [0, 1, 2, 3]

    def strip(y, items, owner_of, title, note, dead=None, moved=None):
        out = [label(24, y - 12, title, "xs t-hi", "start")]
        for i, name in enumerate(items):
            x = 24 + i * 130
            colour = None
            op = None
            if dead is not None and i == dead:
                colour, op = RED, 0.20
            elif moved is not None and i == moved:
                colour, op = GREEN, 0.20
            out.append(box(x, y, 122, 34, colour=colour, opacity=op))
            if colour is None:
                out.append(box(x, y, 122, 34))
            out.append(label(x + 61, y + 22, name, "sm mono"))
            out.append(label(x + 61, y - 2, f"dense {i}", "xs muted"))
            out.append(label(x + 61, y + 48, f"slot {owner_of[i]}", "xs mono muted"))
        out.append(label(24, y + 68, note, "xs muted", "start"))
        return out

    b += strip(56, names, owners, "BEFORE   items_, four live meshes",
               "handle 1:1 names slot 1, whose dense field is 1 — the cube.", dead=1)

    b.append(arrow(360, 136, 360, 168, uid, "h", 1.4))
    b.append(label(376, 158, "1. move the LAST item into the hole and patch ITS "
                             "slot (3 → dense 1)", "xs t-hi", "start"))

    b += strip(190, ["torus", "quad", "floor"], [0, 3, 2],
               "AFTER    items_, three live meshes, still packed",
               "", moved=1)

    b.append(rule(24, 276, 696, 276, "grid", 1.0, "2 3"))
    b.append(label(24, 296, "2. bump slot 1's generation 1 → 2 and push it on the "
                            "free list.", "xs t-hi", "start"))
    b.append(label(24, 314, "The quad's ADDRESS changed and nobody outside the pool "
                            "cares — its handle still says slot 3, and slot 3 now "
                            "says dense 1.", "xs", "start"))
    b.append(label(24, 332, "That is the whole proof that a handle is not a pointer "
                            "with extra steps.", "xs t-ok", "start"))
    return svg(uid, W, H, "Removal by swap and patch",
               "Before: four items packed in the dense array, the second of which "
               "is being removed. Step one moves the last item into the hole and "
               "patches its slot to point at the new dense index. Step two bumps "
               "the removed slot's generation and pushes it on the free list.", b)


# ===========================================================================
# Figure 6 — what it cost, what it bought
# ===========================================================================
def fig_cost():
    uid = "l54f6"
    W, H = 720, 336
    b = [label(20, 20, "Measured, not asserted.", "sm", "start")]

    # Left: the cache key.
    b.append(label(24, 48, "THE CACHE KEY IN sandbox", "xs t-hi", "start"))
    before = ["key == m.vertices.data()",
              "&& style == style",
              "&& cpu.vertices.size() >= m.vertices.size()",
              "&& source_vertices == m.vertices.size()",
              "&& source_indices == m.indices.size()"]
    b.append(box(24, 58, 372, 92, colour=RED, opacity=0.12))
    for i, line in enumerate(before):
        b.append(label(34, 74 + i * 16, line, "xs mono", "start"))
    b.append(label(24, 166, "five tests, four of them present only because an "
                            "address can be reused", "xs muted", "start"))

    b.append(arrow(210, 176, 210, 200, uid, "h", 1.4))

    b.append(box(24, 208, 372, 30, colour=GREEN, opacity=0.14))
    b.append(label(34, 227, "source == handle && style == style", "xs mono", "start"))
    b.append(label(24, 254, "two. A handle IS an identity; an address only "
                            "resembles one.", "xs muted", "start"))

    # Stops above the notes: an earlier version ran to y=300 and the divider
    # struck through the first line of prose beneath the table.
    b.append(rule(420, 40, 420, 272, "grid", 1.0))

    # Right: the numbers.
    rows = [
        ("mesh view → handle", "64 B", "4 B", "16x smaller"),
        ("scene_object", "160 B", "96 B", "40% smaller"),
        ("cache lines / object", "2.50", "1.50", "5.6 needs this"),
        ("resolve vs dereference", "0.85 ns", "0.98 ns", "+0.13 ns"),
        ("golden shot", "905BF27E", "905BF27E", "0 bytes differ"),
    ]
    b.append(label(444, 48, "what", "xs muted", "start"))
    b.append(label(612, 48, "before", "xs muted", "end"))
    b.append(label(676, 48, "after", "xs muted", "end"))
    b.append(rule(444, 56, 696, 56, "grid", 1.0))
    for i, (what, a, c, note) in enumerate(rows):
        y = 78 + i * 40
        b.append(label(444, y, what, "xs", "start"))
        b.append(label(612, y, a, "xs mono muted", "end"))
        b.append(label(676, y, c, "xs mono t-hi", "end"))
        b.append(label(444, y + 15, note, "xs muted", "start"))
        if i < len(rows) - 1:
            b.append(rule(444, y + 24, 696, y + 24, "grid", 1.0, "2 3"))

    b.append(label(24, 292, "The resolution is 0.13 ns dearer than a dereference — "
                            "well under one cycle, because both extra loads hit L1.",
                   "xs", "start"))
    b.append(label(24, 310, "At one resolution per OBJECT per frame, a 4,096-object "
                            "scene pays 0.48 us: 0.003% of a 60 Hz budget.",
                   "xs", "start"))
    b.append(label(24, 328, "At one per VERTEX it would be a different conversation, "
                            "which is why the renderer resolves at the top of the "
                            "loop.", "xs t-hi", "start"))
    return svg(uid, W, H, "The measured cost and benefit of the conversion",
               "Left: the sandbox mesh cache key shrinking from five tests to two. "
               "Right: a table of before and after numbers for the mesh view size, "
               "scene_object size, cache lines per object, resolution cost, and the "
               "golden shot hash.", b)


FIGS = {
    "l54_fig1.svg": fig_question,
    "l54_fig2.svg": fig_failures,
    "l54_fig3.svg": fig_bits,
    "l54_fig4.svg": fig_anatomy,
    "l54_fig5.svg": fig_remove,
    "l54_fig6.svg": fig_cost,
}


def main():
    for name, fn in FIGS.items():
        with open(os.path.join(OUT, name), "w") as fh:
            fh.write(fn())
        print(f"wrote {os.path.join(OUT, name)}")


if __name__ == "__main__":
    main()
