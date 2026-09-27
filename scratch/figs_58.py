#!/usr/bin/env python3
"""scratch/figs_58.py — Lesson 5.8's diagrams.

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
# Figure 1 — one wide struct vs six components  (§1)
# ===========================================================================
def fig_compose():
    uid = "l58f1"
    W, H = 720, 400
    b = [label(20, 20, "The same world, described two ways. Read the ROWS: what does each "
                       "kind of thing actually mean?", "sm", "start")]

    # ---- scene_object ----
    b.append(label(24, 46, "TODAY — one struct, six fields, and every object carries all six.",
                   "xs t-hi", "start"))
    fields = [("xform", AMBER), ("geometry", BLUE), ("name", GREY),
              ("tint", PURPLE), ("closed", GREEN), ("surface", RED)]
    x = 96
    for name, colour in fields:
        b.append(box(x, 58, 96, 18, colour=colour, opacity=0.22))
        b.append(label(x + 48, 71, name, "xs mono"))
        x += 100
    kinds = [("a cube", [1, 1, 1, 1, 1, 1]),
             ("the floor", [1, 1, 1, 1, 0, 0]),
             ("a mover", [1, 0, 1, 0, 0, 0])]
    for r, (kname, used) in enumerate(kinds):
        y = 82 + r * 20
        b.append(label(88, y + 13, kname, "xs mono muted", "end"))
        for c, on in enumerate(used):
            cx = 96 + c * 100
            if on:
                b.append(box(cx, y, 96, 17, colour=fields[c][1], opacity=0.14))
            else:
                b.append(hollow(cx, y, 96, 17, GREY, dash="3 3", width=0.9))
                b.append(label(cx + 48, y + 12, "unused", "xs muted"))
    b.append(label(24, 166, "The dashed cells are paid for anyway: they are in the struct, in "
                            "the array, and in the cache line.", "xs muted", "start"))
    b.append(label(24, 180, "A “mover” with no geometry is a null handle plus a branch "
                            "in the renderer.", "xs muted", "start"))

    b.append(rule(24, 196, 696, 196, "grid", 1.2))

    # ---- components ----
    b.append(label(24, 218, "LESSON 5.8 — six components, and an entity is WHICH ONES IT HAS.",
                   "xs t-hi", "start"))
    comps = [("placement", AMBER), ("geometry", BLUE), ("material", PURPLE),
             ("orbit", GREEN), ("spin", GREY), ("lifetime", RED)]
    x = 118
    for name, colour in comps:
        b.append(box(x, 230, 88, 18, colour=colour, opacity=0.22))
        b.append(label(x + 44, 243, name, "xs mono"))
        x += 95
    # 0 = absent, 1 = present, 2 = present on SOME of them (the ring's spinners)
    ents = [("the sun", [1, 1, 1, 0, 1, 0]),
            ("a ring member", [1, 1, 1, 1, 2, 0]),
            ("a waypoint", [1, 0, 0, 1, 0, 0]),
            ("a spark", [1, 1, 1, 1, 0, 1])]
    for r, (ename, used) in enumerate(ents):
        y = 254 + r * 20
        b.append(label(110, y + 13, ename, "xs mono muted", "end"))
        for c, on in enumerate(used):
            cx = 118 + c * 95
            if on == 1:
                b.append(box(cx, y, 88, 17, colour=comps[c][1], opacity=0.14))
            elif on == 2:
                b.append(hollow(cx, y, 88, 17, comps[c][1], dash="3 3", width=0.9))
                b.append(label(cx + 44, y + 12, "1 in 3", "xs muted"))
            else:
                b.append(rule(cx + 32, y + 8, cx + 56, y + 8, "grid", 1.0))
    b.append(label(24, 352, "There is no cell to leave blank — an absent component is an "
                            "absent ROW, in a pool that was never asked about.", "xs muted",
                   "start"))
    b.append(label(24, 366, "The waypoint is invisible because the render query names "
                            "`geometry` and it has none. Nobody wrote an `if`.", "xs muted",
                   "start"))
    b.append(label(24, 384, "Same information. The difference is which absences cost "
                            "something.", "xs t-hi", "start"))

    return svg(uid, W, H, "One wide struct compared with six components",
               "Above, a six-field scene_object where a floor and a mover leave fields unused "
               "but still pay for them. Below, six component types where each kind of entity "
               "simply has a different subset, and an absent component is an absent row.", b)


# ===========================================================================
# Figure 2 — one id space  (§3.1)
# ===========================================================================
def fig_id_space():
    uid = "l58f2"
    W, H = 720, 360
    b = [label(20, 20, "The one thing Lesson 5.4’s pool does not have, drawn twice.",
               "sm", "start")]

    # ---- today ----
    b.append(label(24, 46, "engine::pool<T> — EVERY POOL MINTS ITS OWN KEYS.",
                   "xs t-hi", "start"))
    for i, (nm, colour) in enumerate((("mesh_pool", AMBER), ("texture_pool", BLUE))):
        y = 58 + i * 58
        b.append(box(150, y, 130, 18, colour=colour, opacity=0.22))
        b.append(label(215, y + 13, nm, "xs mono"))
        b.append(label(142, y + 13, "its own free list", "xs muted", "end"))
        for s in range(4):
            b.append(box(292 + s * 62, y, 56, 18, colour=colour, opacity=0.12))
            b.append(label(320 + s * 62, y + 13, f"slot {s}", "xs mono"))
        b.append(label(560, y + 13, f"handle<{'mesh_data' if i == 0 else 'texture'}>{{7,1}}",
                       "xs mono muted", "start"))
    b.append(arrow(360, 84, 360, 108, uid, "b", 1.4))
    b.append(label(372, 100, "these two 7s have NOTHING to do with each other", "xs t-bad",
                   "start"))
    b.append(label(24, 152, "“Does the thing in mesh slot 7 also have a texture?” is "
                            "not a slow question here. It is not a QUESTION — the two "
                            "sevens", "xs muted", "start"))
    b.append(label(24, 166, "were minted by different free lists and mean different things.",
                   "xs muted", "start"))

    b.append(rule(24, 182, 696, 182, "grid", 1.2))

    # ---- the ECS ----
    b.append(label(24, 204, "engine::ecs — ONE ALLOCATOR MINTS THE ID; EVERY POOL IS "
                            "KEYED BY IT.", "xs t-hi", "start"))
    b.append(box(255, 216, 200, 22, colour=GREY, opacity=0.24))
    b.append(label(355, 231, "entity_allocator", "xs mono"))
    b.append(label(355, 252, "entity e = {index 7, generation 1}", "xs mono t-hi"))

    pools = [("pool<placement>", AMBER), ("pool<geometry>", BLUE), ("pool<material>", PURPLE)]
    for i, (nm, colour) in enumerate(pools):
        x = 84 + i * 194
        b.append(arrow(355, 258, x + 84, 282, uid, "h", 1.2))
        b.append(box(x, 286, 168, 18, colour=colour, opacity=0.22))
        b.append(label(x + 84, 299, nm, "xs mono"))
        b.append(box(x, 306, 168, 16, colour=colour, opacity=0.10))
        b.append(label(x + 84, 318, "sparse[7] → its row, or none", "xs mono"))

    b.append(label(24, 344, "Three answers to one question, and the question is now well "
                            "formed. That sentence is the whole of Lesson 5.7’s rule 2.",
                   "xs t-hi", "start"))

    return svg(uid, W, H, "Per-pool key spaces compared with one shared entity id",
               "Above, two engine::pool containers each minting their own handle 7, which "
               "cannot be related. Below, one entity allocator minting id 7, which every "
               "component pool is keyed by.", b)


# ===========================================================================
# Figure 3 — the slot table, a generation bump, and LIFO reuse  (§4.1)
# ===========================================================================
def fig_slots():
    uid = "l58f3"
    W, H = 720, 390
    b = [label(20, 20, "One slot, three moments. The generation is what turns a reused index "
                       "into a detectable mistake.", "sm", "start")]

    cols = ["slot 0", "slot 1", "slot 2"]
    # The free-list contents are spelled out per panel rather than derived from
    # `hot`: after create() pops slot 1 the list is EMPTY again, and a diagram
    # that shows it still holding 1 teaches the wrong thing about reuse.
    panels = [
        ("1.  three ids minted", [(1, True), (1, True), (1, True)], None, "[]",
         "b = {index 1, generation 1}"),
        ("2.  destroy(b) — the generation is bumped ON REMOVAL", [(1, True), (2, False),
                                                                       (1, True)], 1, "[1]",
         "b is now stale: slot 1 carries generation 2, and b carries 1"),
        ("3.  create() — the free list hands slot 1 back", [(1, True), (2, True),
                                                                 (1, True)], 1, "[]",
         "b2 = {index 1, generation 2}.  SAME INDEX, different occupant."),
    ]
    for p, (title, slots, hot, freelist, note) in enumerate(panels):
        y = 46 + p * 110
        b.append(label(24, y, title, "xs t-hi", "start"))
        for i, (gen, live) in enumerate(slots):
            x = 150 + i * 150
            colour = AMBER if live else GREY
            b.append(box(x, y + 12, 130, 20, colour=colour, opacity=0.24 if live else 0.10))
            b.append(label(x + 65, y + 26, cols[i], "xs mono"))
            b.append(box(x, y + 34, 130, 30, colour=colour, opacity=0.10))
            b.append(label(x + 65, y + 48, f"generation {gen}", "xs mono"))
            b.append(label(x + 65, y + 60, "live" if live else "FREE",
                           "xs mono" + ("" if live else " muted")))
            if hot == i:
                b.append(hollow(x - 4, y + 8, 138, 60, RED if not live else GREEN,
                                dash="4 3", width=1.4))
        b.append(label(142, y + 26, "free list:", "xs muted", "end"))
        b.append(label(142, y + 48, freelist, "xs mono muted", "end"))
        b.append(label(24, y + 82, note, "xs muted", "start"))

    b.append(rule(24, 366, 696, 366, "grid", 1.0, "3 3"))
    b.append(label(24, 384, "alive(b) tests THREE things: index in range, slot live, "
                            "generation matches. Only the third separates 2 from 3.",
                   "xs t-hi", "start"))

    return svg(uid, W, H, "A slot through mint, retire and reuse",
               "Three panels showing slot 1 holding generation 1 while live, generation 2 "
               "while free, and generation 2 while live again after reuse, so the original "
               "id no longer matches.", b)


# ===========================================================================
# Figure 4 — swap-and-pop and the sparse patch  (§4.2)
# ===========================================================================
def fig_erase():
    uid = "l58f4"
    W, H = 720, 420
    b = [label(20, 20, "erase(e=4) from a pool of five. Four arrays change; ONE of the changes "
                       "is the one people forget.", "sm", "start")]

    ids = [7, 4, 91, 30, 12]

    def draw_pool(y, ids_row, sparse_pairs, gone=None, moved=None):
        # sparse
        b.append(label(112, y + 14, "sparse[entity]", "xs mono muted", "end"))
        for k, (e, at) in enumerate(sparse_pairs):
            x = 120 + k * 74
            hot = (moved is not None and e == moved)
            colour = GREEN if hot else RED
            b.append(box(x, y, 66, 20, colour=colour, opacity=0.22 if hot else 0.12))
            b.append(label(x + 33, y + 14, f"{e}→{at}", "xs mono"))
            if hot:
                b.append(hollow(x - 3, y - 3, 72, 26, GREEN, dash="4 3", width=1.4))
        # dense entity
        b.append(label(112, y + 44, "dense[i]", "xs mono muted", "end"))
        for k, e in enumerate(ids_row):
            x = 120 + k * 74
            colour = GREY if (gone is not None and k == gone) else BLUE
            b.append(box(x, y + 30, 66, 20, colour=colour, opacity=0.20))
            b.append(label(x + 33, y + 44, f"e={e}", "xs mono"))
        # data
        b.append(label(112, y + 74, "data[i]", "xs mono muted", "end"))
        for k, e in enumerate(ids_row):
            x = 120 + k * 74
            colour = GREY if (gone is not None and k == gone) else AMBER
            b.append(box(x, y + 60, 66, 20, colour=colour, opacity=0.20))
            b.append(label(x + 33, y + 74, f"T({e})", "xs mono"))
        for k in range(len(ids_row)):
            b.append(label(120 + k * 74 + 33, y + 96, f"i={k}", "xs mono muted"))

    b.append(label(24, 46, "BEFORE", "xs t-hi", "start"))
    draw_pool(58, ids, [(7, 0), (4, 1), (91, 2), (30, 3), (12, 4)])

    b.append(arrow(370, 164, 370, 190, uid, "h", 1.6))
    b.append(label(384, 182, "sparse[4] says the row is i=1. The LAST row (i=4) moves into it.",
                   "xs muted", "start"))

    b.append(label(24, 214, "AFTER", "xs t-hi", "start"))
    draw_pool(226, [7, 12, 91, 30], [(7, 0), (91, 2), (30, 3), (12, 1)], moved=12)
    b.append(label(120 + 4 * 74 + 33, 270, "popped", "xs muted"))

    b.append(rule(24, 340, 696, 340, "grid", 1.0, "3 3"))
    b.append(label(24, 358, "THE PATCH: entity 12 did not ask to move, and its sparse entry "
                            "still said 4. One line fixes it — and without that line the "
                            "pool is", "xs t-hi", "start"))
    b.append(label(24, 372, "silently corrupt in a way that surfaces frames later, in a "
                            "different system, as one entity reading another’s data.",
                   "xs muted", "start"))
    b.append(label(24, 394, "EDGE CASE: erase the LAST row and the entity that “moves” "
                            "IS the one being erased. The patch writes its entry back to where "
                            "it already", "xs muted", "start"))
    b.append(label(24, 408, "was, and the next line marks it absent. Harmless — because "
                            "of the ORDER, not because it was skipped.", "xs muted", "start"))

    return svg(uid, W, H, "Erasing a component with swap and pop",
               "Before and after erasing entity 4's component from a five-element pool. The "
               "last element moves into the hole and the sparse entry of the entity that "
               "moved is patched to its new position.", b)


# ===========================================================================
# Figure 5 — type erasure and what destroy() reaches  (§4.3)
# ===========================================================================
def fig_erasure():
    uid = "l58f6"
    W, H = 720, 340
    b = [label(20, 20, "How destroy(e) reaches every pool without knowing what any of them "
                       "stores.", "sm", "start")]

    b.append(label(24, 48, "component_id_of<T>() — a counter behind a function-local "
                           "static, assigned on first use.", "xs t-hi", "start"))
    types = [("placement", 0, AMBER), ("geometry", 1, BLUE), ("material", 2, PURPLE),
             ("orbit", 3, GREEN), ("lifetime", 4, RED)]
    for nm, i, colour in types:
        x = 60 + i * 124
        b.append(box(x, 62, 108, 20, colour=colour, opacity=0.22))
        b.append(label(x + 54, 76, nm, "xs mono"))
        # The label sits BESIDE the arrow, not on it. A label centred on its own
        # connector reads as struck through, and check-page.js's svgTextOnShape
        # test exists because that has shipped twice before.
        b.append(arrow(x + 40, 86, x + 40, 106, uid, "s", 1.0))
        b.append(label(x + 50, 102, f"id {i}", "xs mono t-hi", "start"))

    b.append(label(52, 126, "pools_", "xs mono muted", "end"))
    for nm, i, colour in types:
        x = 60 + i * 124
        # Two lines, because `unique_ptr<pool_base>` at 9.5px is about 120px wide
        # and the box is 108. A label that overflows its own box is the same
        # failure as a label outside its viewBox; check-page.js cannot see it.
        b.append(box(x, 110, 108, 30, colour=colour, opacity=0.14))
        b.append(label(x + 54, 123, "unique_ptr<", "xs mono"))
        b.append(label(x + 54, 135, "pool_base>", "xs mono"))

    b.append(label(24, 162, "A vector indexed by the id. The pointer is to the BASE, so this "
                            "vector has one type and holds five.", "xs muted", "start"))

    b.append(rule(24, 176, 696, 176, "grid", 1.2))

    b.append(label(24, 198, "destroy(e): walk the vector, call the one virtual function that "
                            "needs no type.", "xs t-hi", "start"))
    for nm, i, colour in types:
        x = 60 + i * 124
        has = i in (0, 1, 3)
        b.append(box(x, 210, 108, 22, colour=colour if has else GREY,
                     opacity=0.20 if has else 0.08))
        b.append(label(x + 54, 225, "erase(e)", "xs mono"))
        b.append(label(x + 54, 248, "row removed" if has else "not there",
                       "xs" + ("" if has else " muted")))
    b.append(label(24, 274, "Five virtual calls to destroy an entity that had three "
                            "components. Two of them return after ONE array read — that "
                            "is the price of", "xs muted", "start"))
    b.append(label(24, 288, "not keeping a per-entity component mask, and at five types it is "
                            "nothing.", "xs muted", "start"))
    b.append(label(24, 312, "COLD BY CONSTRUCTION: this is the only place a virtual call "
                            "happens. A view holds pool<T>* and calls nothing through the base "
                            "—", "xs t-hi", "start"))
    b.append(label(24, 326, "which is Lesson 5.6’s 1.5–1.7× measurement being "
                            "obeyed rather than quoted.", "xs muted", "start"))

    return svg(uid, W, H, "Type erasure by component id",
               "Five component types each assigned a small integer id, indexing a vector of "
               "base-class pointers; destroy walks the vector calling the one virtual function "
               "that needs no type information.", b)


# ===========================================================================
# Figure 6 — the view, and which pool leads  (§4.4)
# ===========================================================================
def fig_lead():
    uid = "l58f5"
    W, H = 720, 400
    b = [label(20, 20, "view<position, velocity, label> over the same 60 entities, walked two "
                       "ways. Both answers are correct.", "sm", "start")]

    def strip(y, n, hits, colour, lbl, note):
        b.append(label(112, y + 15, lbl, "xs mono muted", "end"))
        cell = 560.0 / n
        for k in range(n):
            x = 120 + k * cell
            hot = k in hits
            b.append(box(round(x, 1), y, round(cell - 1.5, 1), 22,
                         colour=colour if hot else GREY, opacity=0.55 if hot else 0.10,
                         rx=1, width=0.6))
        b.append(label(24, y + 40, note, "xs muted", "start"))

    b.append(label(24, 48, "LEADING WITH THE BIGGEST POOL — walk every position, test the "
                           "rest.", "xs t-bad", "start"))
    strip(60, 60, {0, 10, 20, 30, 40, 50}, GREEN, "position (60)",
          "60 candidates. 54 of them cost a sparse read into the velocity pool and a rejection. "
          "6 survive.")

    b.append(rule(24, 122, 696, 122, "grid", 1.0, "3 3"))

    b.append(label(24, 146, "LEADING WITH THE SMALLEST POOL — rule 3, and it is one "
                            "comparison in the constructor.", "xs t-ok", "start"))
    strip(158, 12, {0, 2, 4, 6, 8, 10}, GREEN, "label (12)",
          "12 candidates. 6 rejections instead of 54, for the same six answers — a fifth "
          "of the walk.")

    b.append(rule(24, 220, 696, 220, "grid", 1.2))

    b.append(label(24, 242, "THE LOOP, and every part of it is a consequence of what a sparse "
                            "set is:", "xs t-hi", "start"))
    steps = [("walk the LEAD pool’s dense entity array",
              "sequential; this is the array itself"),
             ("for each other pool: sparse[e] != none  and  dense[at] == e",
              "one array read, then one compare"),
             ("fetch: lead is data[i]; the rest are data[sparse[e]]",
              "the lead needs NO redirect — i is the loop counter")]
    for i, (what, why) in enumerate(steps):
        y = 262 + i * 42
        b.append(box(60, y, 380, 22, colour=BLUE, opacity=0.16))
        b.append(label(70, y + 15, what, "xs mono", "start"))
        b.append(arrow(444, y + 11, 468, y + 11, uid, "s", 1.0))
        b.append(label(474, y + 15, why, "xs muted", "start"))

    b.append(label(24, 392, "Measured in Lesson 5.7 at one-in-four selectivity and 100,000 "
                            "entities: 1.73× leading big, 1.46× leading small.",
                   "xs t-hi", "start"))

    return svg(uid, W, H, "A view leading with the biggest pool compared with the smallest",
               "Two strips of candidate entities: sixty when the walk leads with the position "
               "pool, twelve when it leads with the label pool, with the same six matches in "
               "each.", b)


FIGS = {
    "l58_fig1.svg": fig_compose,     # §1
    "l58_fig2.svg": fig_id_space,    # §3.1
    "l58_fig3.svg": fig_slots,       # §4.1
    "l58_fig4.svg": fig_erase,       # §4.2
    "l58_fig5.svg": fig_lead,        # §3.4 — PAGE order, not authoring order (5.3's trap)
    "l58_fig6.svg": fig_erasure,     # §3.5
}


def main():
    for name, fn in FIGS.items():
        with open(os.path.join(OUT, name), "w") as fh:
            fh.write(fn())
        print(f"wrote {os.path.join(OUT, name)}")


if __name__ == "__main__":
    main()
