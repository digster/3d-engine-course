#!/usr/bin/env python3
"""scratch/figs_55.py — Lesson 5.5's diagrams.

Same rules as 5.4's: computed coordinates, no fill="..." on any <text> (CSS wins
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
# Figure 1 — three questions a pool does not answer
# ===========================================================================
def fig_questions():
    uid = "l55f1"
    W, H = 720, 300
    b = [label(20, 20, "Lesson 5.4 built the mechanism. These are the three "
                       "questions it does not answer.", "sm", "start")]

    rows = [
        ("LOADING", "where does a mesh come from,", "and who decides?",
         "40 lines of demo code opened a file, timed it, parsed it,",
         "flipped its uvs and logged about it. Four of the six steps",
         "were an engine's job."),
        ("FINDING", "ask for “torus.obj” twice —", "get the same handle?",
         "There has never been a name → handle map in this engine,",
         "so every request was a fresh file read. 200 KB, 3.7 ms,",
         "every time."),
        ("UNLOADING", "the word this module has", "been building towards.",
         "Nothing in this engine has ever freed anything. Every",
         "“is this still valid?” so far has been hypothetical,",
         "because the answer was always yes."),
    ]

    for i, (title, sub1, sub2, n1, n2, n3) in enumerate(rows):
        y = 58 + i * 78
        b.append(box(24, y - 16, 172, 56, colour=AMBER, opacity=0.14))
        b.append(label(110, y + 2, title, "sm t-hi"))
        b.append(label(110, y + 18, sub1, "xs muted"))
        b.append(label(110, y + 31, sub2, "xs muted"))
        b.append(label(216, y + 2, n1, "xs", "start"))
        b.append(label(216, y + 17, n2, "xs", "start"))
        b.append(label(216, y + 32, n3, "xs", "start"))
        if i < 2:
            b.append(rule(24, y + 50, 696, y + 50, "grid", 1.0, "2 3"))

    b.append(rule(24, 282, 696, 282, "grid", 1.2))
    b.append(label(24, 296, "All three were unanswerable before 5.4, for one "
                            "reason: you cannot free a thing safely until you can "
                            "say what happens to the references.",
                   "xs t-hi", "start"))
    return svg(uid, W, H, "The three questions an asset system answers",
               "Three rows — loading, finding, unloading — each with the question "
               "it asks and a note on what the engine did instead before this "
               "lesson.", b)


# ===========================================================================
# Figure 2 — a name is not a path
# ===========================================================================
def fig_search_path():
    uid = "l55f2"
    W, H = 720, 336
    b = [label(20, 20, "A NAME is not a PATH, and keeping them different types in "
                       "your head is what makes the rest possible.", "sm", "start")]

    b.append(box(24, 44, 200, 30, colour=BLUE, opacity=0.16))
    b.append(label(124, 63, '"torus.obj"', "sm mono"))
    b.append(label(24, 90, "a NAME: stable, recorded in a scene", "xs muted", "start"))
    b.append(label(24, 104, "file, and the key the store caches on", "xs muted", "start"))

    b.append(arrow(228, 59, 292, 59, uid, "h", 1.4))
    b.append(label(300, 50, "search_path::resolve()", "xs mono t-hi", "start"))
    b.append(label(300, 64, "first root that answers wins", "xs muted", "start"))

    # the roots
    ry = 140
    b.append(label(24, ry - 12, "ROOTS, searched in order", "xs t-hi", "start"))
    roots = [
        ("mods/dragon_pack/", "a mod, a localisation, a test fixture", GREEN, True),
        ("<exe>/assets/", "what the build copied beside the binary", None, False),
        ("<exe>/dlc_02/", "additional content, searched last", None, False),
    ]
    for i, (root, why, colour, hit) in enumerate(roots):
        y = ry + i * 34
        b.append(box(24, y, 250, 26, colour=colour, opacity=(0.18 if colour else None)))
        if colour is None:
            b.append(box(24, y, 250, 26))
        b.append(label(36, y + 17, f"{i}  {root}", "xs mono", "start"))
        b.append(label(288, y + 17, why, "xs muted", "start"))
        if hit:
            b.append(label(660, y + 17, "HIT", "xs t-ok", "end"))

    b.append(rule(24, 254, 696, 254, "grid", 1.0))
    b.append(box(24, 264, 480, 26, colour=GREEN, opacity=0.14))
    b.append(label(36, 281, "/Users/…/mods/dragon_pack/torus.obj", "xs mono", "start"))
    b.append(label(520, 281, "a PATH: where that name", "xs muted", "start"))
    b.append(label(520, 295, "resolved TODAY, on this machine", "xs muted", "start"))

    b.append(label(24, 318, "ORDER IS THE WHOLE FEATURE: put a directory in front "
                            "and everything in it shadows the shipped asset of the "
                            "same name —", "xs", "start"))
    b.append(label(24, 332, "without moving, renaming or deleting anything. Store "
                            "the name; compute the path.", "xs", "start"))
    return svg(uid, W, H, "A name resolving through an ordered list of roots",
               "The name torus.obj is passed to search_path::resolve, which tries "
               "three roots in order; the first, a mod directory, answers, and the "
               "result is an absolute path.", b)


# ===========================================================================
# Figure 3 — why there is no reference count
# ===========================================================================
def fig_no_refcount():
    uid = "l55f3"
    W, H = 720, 330
    b = [label(20, 20, "The obvious answer to “when may I free this?”, and "
                       "why it is the wrong one HERE.", "sm", "start")]

    b.append(label(24, 52, "IF A HANDLE WERE REFCOUNTED, it would need…",
                   "xs t-hi", "start"))
    costs = [
        ("a copy constructor", "so copying has a side effect"),
        ("a destructor", "so going out of scope has one too"),
        ("a pointer to its store", "so it knows whose count to touch"),
    ]
    for i, (what, why) in enumerate(costs):
        y = 74 + i * 24
        b.append(box(36, y - 12, 190, 20, colour=RED, opacity=0.13))
        b.append(label(131, y + 2, what, "xs"))
        b.append(label(240, y + 2, why, "xs muted", "start"))

    b.append(rule(24, 154, 440, 154, "grid", 1.0, "2 3"))
    b.append(label(24, 174, "…AND IT WOULD LOSE EVERY PROPERTY 5.4 BOUGHT:",
                   "xs t-bad", "start"))
    lost = ["4 bytes", "trivially copyable", "memcpy-able into a component",
            "serializable with no fixups"]
    for i, what in enumerate(lost):
        b.append(label(36, 194 + i * 16, "✗  " + what, "xs t-bad", "start"))

    b.append(rule(452, 40, 452, 300, "grid", 1.0))

    b.append(label(472, 52, "SO: EXPLICIT UNLOAD", "xs t-ok", "start"))
    b.append(label(472, 70, "and it is safe to get wrong precisely", "xs", "start"))
    b.append(label(472, 84, "because 5.4 made staleness", "xs", "start"))
    b.append(label(472, 98, "detectable.", "xs", "start"))

    b.append(box(472, 116, 216, 30, colour=AMBER, opacity=0.15))
    b.append(label(580, 129, "forget to unload  →  a leak", "xs"))
    b.append(label(580, 141, "which a total finds", "xs muted"))

    b.append(box(472, 154, 216, 30, colour=AMBER, opacity=0.15))
    b.append(label(580, 167, "unload too early  →  a null", "xs"))
    b.append(label(580, 179, "and a counter", "xs muted"))

    b.append(label(472, 204, "Neither is a crash. That is the trade.", "xs t-ok", "start"))

    b.append(rule(472, 220, 696, 220, "grid", 1.0, "2 3"))
    b.append(label(472, 240, "ONE PLACE NEEDS A RULE:", "xs t-hi", "start"))
    b.append(label(472, 256, "DERIVED ASSETS. The dependency is", "xs", "start"))
    b.append(label(472, 270, "internal — nobody outside asked for", "xs", "start"))
    b.append(label(472, 284, "it by name — so it can be tracked", "xs", "start"))
    b.append(label(472, 298, "without exposing anything.", "xs", "start"))
    return svg(uid, W, H, "Why handles are not reference counted",
               "Left: the three things a refcounted handle would need and the four "
               "properties it would lose. Right: explicit unload, whose two failure "
               "modes are a leak and a null rather than undefined behaviour, plus "
               "the one place a lifetime rule is unavoidable.", b)


# ===========================================================================
# Figure 4 — the cascade
# ===========================================================================
def fig_cascade():
    uid = "l55f4"
    W, H = 720, 320
    b = [label(20, 20, "unload_mesh(torus) — and everything that only existed "
                       "because it did.", "sm", "start")]

    # the tree
    b.append(box(40, 52, 180, 40, colour=BLUE, opacity=0.18))
    b.append(label(130, 70, "torus.obj", "sm mono"))
    b.append(label(130, 84, "LOADED — has a name", "xs muted"))

    b.append(box(40, 132, 180, 40, colour=PURPLE, opacity=0.18))
    b.append(label(130, 150, "with_normals(flat)", "xs mono"))
    b.append(label(130, 164, "DERIVED — no name", "xs muted"))

    b.append(box(40, 212, 180, 40, colour=PURPLE, opacity=0.18))
    b.append(label(130, 230, "the GPU upload's copy", "xs mono"))
    b.append(label(130, 244, "DERIVED from the derived", "xs muted"))

    b.append(arrow(130, 94, 130, 128, uid, "s", 1.2))
    b.append(arrow(130, 174, 130, 208, uid, "s", 1.2))

    b.append(label(240, 74, "A derived asset has NO NAME, because", "xs", "start"))
    b.append(label(240, 88, "nobody asked for it by one: it exists only", "xs", "start"))
    b.append(label(240, 102, "as a consequence of its source existing,", "xs", "start"))
    b.append(label(240, 116, "and it should stop existing for the same", "xs", "start"))
    b.append(label(240, 130, "reason.", "xs", "start"))

    b.append(box(240, 150, 300, 26, colour=RED, opacity=0.14))
    b.append(label(390, 167, "unload_mesh(torus)  →  3 assets released", "xs mono"))

    b.append(label(240, 194, "TRANSITIVELY. A cascade that stops after", "xs", "start"))
    b.append(label(240, 208, "one hop is a leak, and the chain above is", "xs", "start"))
    b.append(label(240, 222, "what a real pipeline produces.", "xs", "start"))

    b.append(label(240, 248, "THE HOLE 5.4 LEFT, CLOSED. Its mesh cache", "xs t-hi", "start"))
    b.append(label(240, 262, "held a second pool with no rule connecting", "xs t-hi", "start"))
    b.append(label(240, 276, "it: free the source and the derived copy", "xs t-hi", "start"))
    b.append(label(240, 290, "lived on, keyed by a handle that no longer", "xs t-hi", "start"))
    b.append(label(240, 304, "resolved — a leak with a clean bill of health.", "xs t-hi", "start"))
    return svg(uid, W, H, "Unloading a source cascades to everything derived from it",
               "A chain of three assets: a loaded mesh, the with_normals output "
               "derived from it, and the GPU upload's copy derived from that. "
               "Unloading the source releases all three.", b)


# ===========================================================================
# Figure 5 — what an acquire costs
# ===========================================================================
def fig_cost():
    uid = "l55f5"
    W, H = 720, 330
    b = [label(20, 20, "What an acquire costs, to scale. torus.obj — 200 KB, 1,225 "
                       "vertices, 2,304 triangles.", "sm", "start")]

    # Cold acquire: the stages, to scale, out of 3.693 ms.
    stages = [("resolve", 0.0022, GREY), ("read", 0.0148, BLUE),
              ("parse", 3.6922, RED), ("import", 0.0034, GREY)]
    cold = 3.693

    x0, y0, w, h = 150, 58, 520, 32
    x = float(x0)
    for name, val, colour in stages:
        seg = max(w * val / cold, 1.2)
        b.append(box(x, y0, seg, h, colour=colour, opacity=0.30))
        if seg > 60:
            b.append(label(x + seg / 2, y0 + 15, name, "xs"))
            b.append(label(x + seg / 2, y0 + 28, f"{val:.3f} ms  ·  99.9%", "xs mono muted"))
        x += seg
    b.append(label(140, y0 + 20, "COLD", "xs t-hi", "end"))
    b.append(label(x0, y0 - 8, "0", "xs muted"))
    b.append(label(x0 + w, y0 - 8, f"{cold:.3f} ms", "xs muted", "end"))

    b.append(label(150, 110, "resolve 0.0022 · read 200 KB 0.0148 · uv flip 0.0034 · "
                             "store into the pool ~0", "xs mono muted", "start"))
    b.append(label(150, 126, "Together 0.55% — and they are the four stages people "
                             "assume the cost is in.", "xs", "start"))
    b.append(label(150, 142, "THE ACQUIRE IS THE PARSE.", "xs t-bad", "start"))

    # The hit.
    b.append(box(x0, 164, 2.5, 32, colour=GREEN, opacity=0.95))
    b.append(label(140, 184, "HIT", "xs t-ok", "end"))
    b.append(label(x0 + 12, 178, "0.00025 ms — a hash of a short string, a map probe, "
                                 "one pool lookup", "xs", "start"))
    b.append(label(x0 + 12, 192, "14,771x. The bar is two pixels wide because that is "
                                 "what to scale looks like.", "xs muted", "start"))

    # Validate, which is the demo's.
    b.append(box(x0, 216, w * 2.4525 / cold, 26, colour=AMBER, opacity=0.24))
    b.append(label(140, 233, "…and", "xs muted", "end"))
    b.append(label(x0 + (w * 2.4525 / cold) / 2, 233, "validate  2.453 ms", "xs"))
    b.append(label(150, 258, "The demo validates every mesh it loads (Lesson 3.5) — "
                             "66% of the acquire again, for a check", "xs", "start"))
    b.append(label(150, 272, "the STORE does not perform and a shipping build would "
                             "not. Cost you can see is cost you can choose.",
                   "xs", "start"))

    b.append(rule(24, 288, 696, 288, "grid", 1.0))
    b.append(label(24, 306, "IT IS NOT “FAST LOADING”. It is NOT LOADING — a parser "
                            "twice as fast would have moved 3.69 ms to 1.85; the "
                            "cache moved it to 0.00025.", "xs t-hi", "start"))
    b.append(label(24, 324, "A SESSION — five models cycled twice, as [L] does: eight "
                            "acquires, FOUR file reads. The second lap is free.",
                   "xs", "start"))
    return svg(uid, W, H, "The cost of a cold acquire, a cache hit, and the demo's validation",
               "A bar showing a cold acquire of 3.693 ms in which parsing is 99.9% "
               "and resolve, read, import and store together are 0.55%; a two-pixel "
               "sliver for the 0.00025 ms cache hit; and a separate bar for the "
               "demo's 2.453 ms validation pass.", b)


# ===========================================================================
# Figure 6 — unloading under a live scene
# ===========================================================================
def fig_unload_live():
    uid = "l55f6"
    W, H = 720, 300
    b = [label(20, 20, "Freeing a mesh a scene is still holding — and the only "
                       "thing that happens.", "sm", "start")]

    objs = [("icosahedron", 20, True), ("slab (cube)", 12, False),
            ("plinth (cube)", 12, False)]

    def panel(x, title, freed, sub):
        out = [label(x + 150, 52, title, "xs t-hi")]
        out.append(rule(x, 60, x + 300, 60, "grid", 1.0))
        for i, (name, tris, is_target) in enumerate(objs):
            y = 74 + i * 40
            dead = freed and is_target
            colour = RED if dead else GREEN
            out.append(box(x, y, 300, 32, colour=colour, opacity=0.14))
            out.append(label(x + 12, y + 14, name, "xs mono", "start"))
            out.append(label(x + 12, y + 26,
                             "handle stale → SKIPPED" if dead
                             else f"{tris} triangles", "xs muted", "start"))
            out.append(label(x + 288, y + 20, "—" if dead else str(tris),
                             "sm mono " + ("t-bad" if dead else "t-ok"), "end"))
        out.append(label(x, 206, sub, "xs", "start"))
        return out

    b += panel(24, "BEFORE  —  unresolved = 0", False,
               "44 triangles collected from 3 objects.")
    b += panel(396, "AFTER  unload_mesh(icosahedron)", True,
               "24 triangles from 2. unresolved = 1.")

    b.append(rule(360, 44, 360, 220, "grid", 1.0))

    b.append(rule(24, 232, 696, 232, "grid", 1.2))
    b.append(label(24, 252, "THE SCENE STILL HOLDS THE HANDLE.", "xs t-bad", "start"))
    # x=228, not 210: at 210 it butted straight against the label before it and
    # the two ran together as one word. The visual pass is the only thing that
    # catches a missing space between two separately-positioned text runs.
    b.append(label(228, 252, "It is rebuilt from the same array every frame and "
                             "nothing nulled it.", "xs", "start"))
    b.append(label(24, 270, "Before 5.4 this was a dangling span and a picture "
                            "nobody could trust; before 5.5 it could not be "
                            "performed at all. Now it costs", "xs", "start"))
    b.append(label(24, 288, "one object, and the renderer says how many.",
                   "xs t-ok", "start"))
    return svg(uid, W, H, "A scene rendered before and after its mesh is unloaded",
               "Two panels of the same three-object scene. In the first all three "
               "resolve and 44 triangles are collected. In the second the "
               "icosahedron's mesh has been unloaded, so it is skipped and counted, "
               "and the other two objects draw 24 triangles between them.", b)


FIGS = {
    "l55_fig1.svg": fig_questions,
    "l55_fig2.svg": fig_search_path,
    "l55_fig3.svg": fig_no_refcount,
    "l55_fig4.svg": fig_cascade,
    "l55_fig5.svg": fig_cost,
    "l55_fig6.svg": fig_unload_live,
}


def main():
    for name, fn in FIGS.items():
        with open(os.path.join(OUT, name), "w") as fh:
            fh.write(fn())
        print(f"wrote {os.path.join(OUT, name)}")


if __name__ == "__main__":
    main()
