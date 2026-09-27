#!/usr/bin/env python3
"""scratch/figs_65.py — Lesson 6.5's diagrams.

Same rules as 6.1–6.4's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - every element well inside the viewBox
  - ~5.2 units per character for `xs`, ~6.0 for `sm`
  - filenames numbered by PAGE ORDER
  - a truncated bar axis lies; bars start at zero (6.4's lesson)

Every number comes from verify_65's output.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)

OUT = "scratch"

# ---- measured, from verify_65 ----------------------------------------------
SIZE_MATERIAL = 36
SIZE_HANDLE = 4
RING_ENTITIES = 96
RING_DISTINCT = 6
BYTES_BY_VALUE = 3456
BYTES_BY_HANDLE = 600
SCENE_OBJECT_BEFORE = 96
SCENE_OBJECT_AFTER = 112
CHECKS = 27


# ===========================================================================
# Figure 1 — three places grew the same hole  (§1)
# ===========================================================================
def fig_hole():
    uid = "l65f1"
    W, H = 720, 380
    b = [box(0, 0, W, H)]
    b.append(label(W / 2, 28, "Three places grew the same struct-shaped hole", "lg"))

    cols = [
        ("scene_object", "Lessons 3.4, 3.7", GREEN,
         ["xform", "geometry", "name", "tint", "closed", "surface"],
         [3, 5], "“…the second kind", "has a name: a material”"),
        ("fill_style", "Lesson 3.8", BLUE,
         ["interp", "shade", "cull", "lights", "surface", "albedo"],
         [3, 4, 5], "“two structs", "wearing one name”"),
        ("ecs_swarm", "a DEMO, Lesson 5.7", AMBER,
         ["struct material", "{", "  Uint32 tint;", "  microsurface", "  surface;", "}"],
         [0, 1, 2, 3, 4, 5], "it INVENTED the type", "the engine did not have"),
    ]
    w = 200
    gap = (W - 3 * w) / 4
    for i, (name, when, col, fields, hot, q1, q2) in enumerate(cols):
        x = gap + i * (w + gap)
        b.append(box(x, 60, w, 178, col, opacity=0.08))
        b.append(hollow(x, 60, w, 178, col))
        b.append(label(x + w / 2, 84, name, "sm"))
        b.append(label(x + w / 2, 102, when, "xs"))
        b.append(rule(x + 16, 112, x + w - 16, 112, "grid", 1.0))
        y = 132
        for j, f in enumerate(fields):
            if j in hot:
                b.append(box(x + 14, y - 11, w - 28, 16, col, opacity=0.42))
            b.append(label(x + w / 2, y, f, "xs"))
            y += 18
        b.append(label(x + w / 2, 258, q1, "xs"))
        b.append(label(x + w / 2, 274, q2, "xs"))

    b.append(box(gap, 296, W - 2 * gap, 62, None))
    b.append(label(W / 2, 320,
                   "Highlighted: the fields that describe the SURFACE rather than the object.", "xs"))
    b.append(label(W / 2, 342,
                   "And Lesson 6.4 priced the absence — 44 call sites to change one of them.", "sm"))

    return svg(uid, W, H,
               "Three structs that each accumulated surface description",
               "scene_object collected tint, closed and surface beside its transform "
               "and geometry; fill_style collected lights, surface and albedo beside "
               "the rasterizer's knobs, and its own comment calls itself two structs "
               "wearing one name; and the ecs_swarm demo, restricted to the public "
               "API, declared its own material struct because the engine offered "
               "none.", b)


# ===========================================================================
# Figure 2 — which half can be a number in a buffer  (§3)
# ===========================================================================
def fig_split():
    uid = "l65f2"
    W, H = 720, 400
    b = [box(0, 0, W, H)]
    b.append(label(W / 2, 28, "The question that draws the line", "lg"))
    b.append(label(W / 2, 52, "Can this be a number in a buffer?", "sm"))

    # ---- LEFT: yes -> material ---------------------------------------------
    lx, ly, lw = 46, 78, 300
    b.append(box(lx, ly, lw, 214, GREEN, opacity=0.10))
    b.append(hollow(lx, ly, lw, 214, GREEN))
    b.append(label(lx + lw / 2, ly + 26, "YES  →  it is the MATERIAL", "sm"))
    b.append(label(lx + lw / 2, ly + 46, "per-draw data, pushed as a uniform", "xs"))
    b.append(rule(lx + 20, ly + 58, lx + lw - 20, ly + 58, "grid", 1.0))
    rows = [("tint", "the albedo, sRGB"), ("albedo_map", "a texture HANDLE"),
            ("samp", "how to read it"), ("surface", "roughness / metallic / F0")]
    y = ly + 82
    for k, v in rows:
        b.append(label(lx + 24, y, k, "xs", "start"))
        b.append(label(lx + lw - 24, y, v, "xs", "end"))
        y += 26
    b.append(label(lx + lw / 2, ly + 196, "two of these differ → no state change", "xs"))

    # ---- RIGHT: no -> pipeline state ---------------------------------------
    rx = 374
    b.append(box(rx, ly, lw, 214, RED, opacity=0.10))
    b.append(hollow(rx, ly, lw, 214, RED))
    b.append(label(rx + lw / 2, ly + 26, "NO  →  it is PIPELINE STATE", "sm"))
    b.append(label(rx + lw / 2, ly + 46, "baked into a pipeline object", "xs"))
    b.append(rule(rx + 20, ly + 58, rx + lw - 20, ly + 58, "grid", 1.0))
    rows2 = [("cull mode", "which faces to drop"), ("fill mode", "solid or wireframe"),
             ("specular_model", "which BRDF runs"), ("the shader", "the program itself")]
    y = ly + 82
    for k, v in rows2:
        b.append(label(rx + 24, y, k, "xs", "start"))
        b.append(label(rx + lw - 24, y, v, "xs", "end"))
        y += 26
    b.append(label(rx + lw / 2, ly + 196, "two of these differ → TWO PIPELINES + a sort", "xs"))

    b.append(box(46, 312, W - 92, 62, None))
    b.append(label(W / 2, 336,
                   "Lesson 4.8 measured the right-hand side: three pipelines, and a sort", "xs"))
    b.append(label(W / 2, 356,
                   "to keep the bind count at three instead of one per draw.", "xs"))

    return svg(uid, W, H,
               "The rule that decides what belongs in a material",
               "Anything the fragment stage reads as a value can be pushed as a "
               "uniform and belongs in the material: the tint, the texture handle, "
               "the sampler and the microsurface. Anything that decides which "
               "pipeline runs cannot be a uniform and stays out: cull mode, fill "
               "mode, the choice of BRDF and the shader itself. Two objects "
               "differing only in the first group draw back to back; two differing "
               "in the second need two pipelines and a sort between them.", b)


# ===========================================================================
# Figure 3 — handle, resolve, pointer  (§4)
# ===========================================================================
def fig_handle():
    uid = "l65f3"
    W, H = 720, 340
    b = [box(0, 0, W, H)]
    b.append(label(W / 2, 28, "Store a handle. Use a pointer. Resolve once, between.", "lg"))

    stages = [
        (60, "HANDLE", "material::albedo_map", GREEN,
         ["durable", "checkable", "4 bytes", "survives a realloc"], "STORAGE"),
        (280, "resolve", "bind_albedo(m, pool)", AMBER,
         ["once per DRAW", "a bounds check", "+ a generation", "compare"], "THE STEP"),
        (500, "POINTER", "texture_binding::image", BLUE,
         ["one dereference", "no lookup", "no branch", "per PIXEL"], "THE HOT LOOP"),
    ]
    w = 160
    for x, title, code, col, notes, band in stages:
        b.append(box(x, 62, w, 190, col, opacity=0.10))
        b.append(hollow(x, 62, w, 190, col))
        b.append(label(x + w / 2, 86, title, "sm"))
        b.append(label(x + w / 2, 104, band, "xs"))
        b.append(rule(x + 14, 114, x + w - 14, 114, "grid", 1.0))
        y = 136
        for n in notes:
            b.append(label(x + w / 2, y, n, "xs")); y += 19
        b.append(label(x + w / 2, 240, code, "xs"))

    b.append(arrow(224, 157, 274, 157, uid, "i", 1.7))
    b.append(arrow(444, 157, 494, 157, uid, "i", 1.7))

    b.append(box(60, 268, W - 120, 58, None))
    b.append(label(W / 2, 290,
                   "Wrong in one direction: the scene dangles the first time a pool grows.", "xs"))
    b.append(label(W / 2, 312,
                   "Wrong in the other: a bounds check and a generation compare, per pixel.", "xs"))

    return svg(uid, W, H,
               "Where a handle belongs and where a pointer belongs",
               "A handle is how a material stores a reference to a texture: durable, "
               "four bytes, checkable, and unaffected when the pool reallocates. A "
               "pointer is how the fill loop reads it: one dereference with no "
               "lookup. bind_albedo is the named step between them, run once per "
               "draw. Storing pointers instead makes the scene dangle when a pool "
               "grows; resolving inside the loop adds a bounds check to every "
               "pixel.", b)


# ===========================================================================
# Figure 4 — sharing, measured  (§6)
# ===========================================================================
def fig_sharing():
    uid = "l65f4"
    W, H = 720, 382
    b = [box(0, 0, W, H)]
    b.append(label(W / 2, 28, "What the pool is actually for", "lg"))
    b.append(label(W / 2, 50,
                   f"{RING_ENTITIES} ring drones, {RING_DISTINCT} distinct materials", "sm"))

    # ZERO-BASED bars — 6.4's lesson about truncated axes.
    px, py, pw, ph = 150, 76, 420, 120
    top = BYTES_BY_VALUE
    for frac in (0.0, 0.25, 0.5, 0.75, 1.0):
        x = px + frac * pw
        b.append(rule(x, py - 6, x, py + ph, "grid", 0.8, "2 4"))
        b.append(label(x, py + ph + 16, f"{int(frac * top)}", "xs"))
    b.append(label(px + pw / 2, py + ph + 34, "bytes of material component", "xs"))

    rows = [("a material each", BYTES_BY_VALUE, RED,
             f"{RING_ENTITIES} x {SIZE_MATERIAL} B"),
            ("a handle each", BYTES_BY_HANDLE, GREEN,
             f"{RING_ENTITIES} x {SIZE_HANDLE} B  +  {RING_DISTINCT} x {SIZE_MATERIAL} B")]
    y = py + 12
    for name, val, col, detail in rows:
        b.append(label(px - 14, y + 16, name, "xs", "end"))
        bw = pw * val / top
        b.append(box(px, y, bw, 30, col, opacity=0.55))
        b.append(label(px + bw + 10, y + 20, f"{val} B", "sm", "start"))
        b.append(label(px + 8, y + 45, detail, "xs", "start"))
        y += 62

    b.append(box(60, 252, W - 120, 108, None))
    b.append(label(W / 2, 276,
                   f"{BYTES_BY_VALUE / BYTES_BY_HANDLE:.1f}x smaller — and that is the SIDE EFFECT.", "sm"))
    b.append(rule(120, 290, W - 120, 290, "grid", 1.0))
    b.append(label(W / 2, 312,
                   "With copies, “make the drones rougher” is a loop over the registry", "xs"))
    b.append(label(W / 2, 332, "that has to find every entity carrying that appearance.", "xs"))
    b.append(label(W / 2, 352, "With handles it is ONE WRITE.", "sm"))

    return svg(uid, W, H,
               "The cost of copying a material into every entity",
               "Ninety-six ring drones cycle through six tints, so there are six "
               "materials and ninety-six references to them. Storing a material in "
               "each entity costs 3456 bytes; storing a handle each and pooling the "
               "six costs 600, a 5.8 times reduction. The stronger argument is not "
               "the bytes: with copies, changing the drones' finish means finding "
               "every entity that carries it, and with handles it is a single "
               "write.", b)


# ===========================================================================
# Figure 5 — the fact and the decision  (§7)
# ===========================================================================
def fig_closed():
    uid = "l65f5"
    W, H = 720, 320
    b = [box(0, 0, W, H)]
    b.append(label(W / 2, 28, "A fact about the mesh is not a decision about the draw", "lg"))

    boxes = [
        (56, "THE MESH", "mesh_report::closed()", GREEN,
         ["counted from edges", "“no rim, no non-manifold", "junctions”",
          "a cube: TRUE", "a quad: FALSE"]),
        (398, "THE CALLER", "scene_object::closed", BLUE,
         ["the INTENT", "a closed mesh may still", "be drawn two-sided —", 
          "to look inside it, or", "to debug a winding"]),
    ]
    for x, title, code, col, notes in boxes:
        b.append(box(x, 62, 266, 158, col, opacity=0.10))
        b.append(hollow(x, 62, 266, 158, col))
        b.append(label(x + 133, 86, title, "sm"))
        b.append(label(x + 133, 104, code, "xs"))
        b.append(rule(x + 20, 114, x + 246, 114, "grid", 1.0))
        y = 136
        for n in notes:
            b.append(label(x + 133, y, n, "xs")); y += 18

    b.append(arrow(324, 141, 390, 141, uid, "s", 1.4))
    b.append(label(357, 130, "+", "sm"))

    b.append(box(190, 238, 340, 62, AMBER, opacity=0.12))
    b.append(hollow(190, 238, 340, 62, AMBER))
    b.append(label(360, 262, "cull_of(fact, intent)", "sm"))
    b.append(label(360, 284, "the one place the rule lives", "xs"))
    # Start BELOW the boxes (which end at y=220), not inside them: the first
    # draft began at y=205 and ran straight through each box's last line.
    b.append(arrow(189, 226, 300, 242, uid, "s", 1.3))
    b.append(arrow(531, 226, 420, 242, uid, "s", 1.3))

    return svg(uid, W, H,
               "Why closed did not move onto the material",
               "The mesh reports whether it is closed, counted from its own edges by "
               "validate. The caller supplies the intent, because a closed mesh may "
               "still be drawn two-sided deliberately. cull_of combines the two, and "
               "is the single place the rule that culling is only valid on closed "
               "geometry is written down.", b)


def main():
    figs = [("l65_fig1.svg", fig_hole()),
            ("l65_fig2.svg", fig_split()),
            ("l65_fig3.svg", fig_handle()),
            ("l65_fig4.svg", fig_sharing()),
            ("l65_fig5.svg", fig_closed())]
    for name, body in figs:
        with open(os.path.join(OUT, name), "w") as fh:
            fh.write(body)
        print(f"  wrote {OUT}/{name}  ({len(body):,} bytes)")


if __name__ == "__main__":
    main()
