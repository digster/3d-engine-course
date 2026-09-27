#!/usr/bin/env python3
"""scratch/figs_66.py — Lesson 6.6's diagrams.

Same rules as 6.1–6.5's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - every element well inside the viewBox
  - ~5.2 units per character for `xs`, ~6.0 for `sm`
  - filenames numbered by PAGE ORDER
  - a truncated bar axis lies; bars start at zero (6.4's lesson)
  - no HTML tags inside <text>; use <tspan class="t-hi">

Every number comes from verify_66's output, from assets/cube.gltf, or from the
peak-radiance measurement quoted in the lesson.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)

OUT = "scratch"

# ---- measured -------------------------------------------------------------
CHECKS = 58
LERP_ERROR = "1.19e-07"
LERP_POINTS = 4851
WORST_RATIO = 5.62
NORMAL_RATIO = 1.036
FURNACE_SPEC = 1.3395
FURNACE_OURS = 0.9255
CUBE_BIN_BYTES = 328
GLB_BYTES = 3052


def tspan_hi(text):
    return f'<tspan class="t-hi">{esc(text)}</tspan>'


# ===========================================================================
# Figure 1 — what each format DESCRIBES  (§1)
# ===========================================================================
def fig_formats():
    uid = "l66f1"
    W, H = 720, 400
    b = [box(0, 0, W, H)]
    b.append(label(W / 2, 26, "OBJ describes a shape. glTF describes a scene.", "lg"))

    # ---- left: OBJ --------------------------------------------------------
    lx, lw = 24, 300
    b.append(hollow(lx, 46, lw, 250, GREY))
    b.append(label(lx + lw / 2, 68, "torus.obj  —  Lesson 3.5", "sm"))
    b.append(rule(lx + 14, 78, lx + lw - 14, 78, "grid", 1.0))

    obj_rows = [
        "v  0.5 -0.5 0.5",
        "vt 0.25 0.75",
        "vn 0.0 1.0 0.0",
        "f  1/1/1 2/2/2 3/3/3",
        "usemtl  Metal",
    ]
    y = 100
    for i, r in enumerate(obj_rows):
        c = RED if i == 4 else None
        if c:
            b.append(box(lx + 16, y - 12, lw - 32, 18, c, opacity=0.18))
        b.append(label(lx + lw / 2, y, r, "xs"))
        y += 22

    b.append(rule(lx + 14, 220, lx + lw - 14, 220, "grid", 1.0))
    b.append(label(lx + lw / 2, 240, "ONE bag of triangles.", "sm"))
    b.append(label(lx + lw / 2, 260, "`usemtl` names a string the format", "xs"))
    b.append(label(lx + lw / 2, 276, "cannot itself define.", "xs"))

    # ---- right: glTF ------------------------------------------------------
    rx0, rw = 396, 300
    b.append(hollow(rx0, 46, rw, 250, BLUE))
    b.append(label(rx0 + rw / 2, 68, "shapes.glb  —  this lesson", "sm"))
    b.append(rule(rx0 + 14, 78, rx0 + rw - 14, 78, "grid", 1.0))

    tree = [
        (0, "scene", GREY),
        (1, "node  \"group\"   T = (1,0,0)", BLUE),
        (2, "node \"gold octahedron\"", BLUE),
        (3, "mesh -> primitive", GREEN),
        (4, "material  \"gold\"", AMBER),
        (4, "accessors: POSITION, indices", PURPLE),
    ]
    y = 100
    for depth, text, col in tree:
        x = rx0 + 20 + depth * 13
        b.append(box(x, y - 12, rw - 40 - depth * 13, 18, col, opacity=0.14))
        b.append(label(x + 8, y, text, "xs", anchor="start"))
        y += 22

    b.append(rule(rx0 + 14, 220, rx0 + rw - 14, 220, "grid", 1.0))
    b.append(label(rx0 + rw / 2, 240, "FOUR draws, four placements,", "sm"))
    b.append(label(rx0 + rw / 2, 260, "three materials the file DEFINES —", "xs"))
    b.append(label(rx0 + rw / 2, 276, "base colour, metallic, roughness.", "xs"))

    b.append(arrow(lx + lw + 12, 170, rx0 - 12, 170, uid, "s", 1.4))
    b.append(label(W / 2, 158, "the same", "xs"))
    b.append(label(W / 2, 190, "job?", "xs"))

    b.append(box(24, 312, W - 48, 72, None))
    b.append(label(W / 2, 336,
                   "A glTF mesh is a LIST of primitives and each one has its own material.", "sm"))
    b.append(label(W / 2, 358,
                   "Flatten them into one mesh_data and you get geometry that is correct", "xs"))
    b.append(label(W / 2, 374,
                   "and unpaintable — a car body and its windscreen must be two draws.", "xs"))

    return svg(uid, W, H,
               "OBJ against glTF: a bag of triangles against a scene graph",
               "On the left an OBJ file: v, vt, vn and f statements, plus a usemtl "
               "line highlighted because it names a material the format cannot "
               "define. On the right a glTF scene: a scene containing a group node "
               "with a translation, containing a node holding a mesh, whose "
               "primitive names both a material and a set of accessors.", b)


# ===========================================================================
# Figure 2 — the accessor chain, with real bytes  (§3)
# ===========================================================================
def fig_accessor():
    uid = "l66f2"
    W, H = 720, 392
    b = [box(0, 0, W, H)]
    b.append(label(W / 2, 26, "Why we do not hand-roll the parser", "lg"))
    b.append(label(W / 2, 46, "assets/cube.gltf, POSITION, as actually written", "xs"))

    # Chain of four boxes.
    stages = [
        ("accessor 0", ["type   VEC3", "compType FLOAT", "count  8", "byteOffset 0"], PURPLE),
        ("bufferView 0", ["buffer 0", "byteOffset 0", "byteLength 96", "target ARRAY"], BLUE),
        ("buffer 0", [f'uri "cube.bin"', f"byteLength {CUBE_BIN_BYTES}", "",
                      "an external file"], GREEN),
    ]
    bw, bh = 176, 116
    gap = (W - 48 - 3 * bw) / 2
    for i, (name, rows, col) in enumerate(stages):
        x = 24 + i * (bw + gap)
        b.append(box(x, 68, bw, bh, col, opacity=0.10))
        b.append(hollow(x, 68, bw, bh, col))
        b.append(label(x + bw / 2, 90, name, "sm"))
        b.append(rule(x + 12, 98, x + bw - 12, 98, "grid", 1.0))
        y = 118
        for r in rows:
            if r:
                b.append(label(x + bw / 2, y, r, "xs"))
            y += 18
        if i < 2:
            b.append(arrow(x + bw + 6, 126, x + bw + gap - 6, 126, uid, "i", 1.4))

    # The combinatorial matrix underneath.
    b.append(label(W / 2, 214, "…and every one of those fields has options, all legal:", "sm"))

    axes = [
        ("component type", "float32 · uint8 · uint16 · int8 · int16", 5),
        ("normalized", "yes · no", 2),
        ("packing", "tight · interleaved at a stride", 2),
        ("sparse", "dense · base array + patch list", 2),
    ]
    y = 240
    for name, options, n in axes:
        b.append(label(150, y, name, "xs", anchor="end"))
        b.append(box(164, y - 12, 400, 18, AMBER, opacity=0.12))
        b.append(label(364, y, options, "xs"))
        b.append(label(590, y, f"x {n}", "xs", anchor="start"))
        y += 24

    b.append(rule(164, y - 4, 620, y - 4, "grid", 1.0))
    b.append(label(590, y + 16, "= 40", "sm", anchor="start"))
    b.append(label(364, y + 16,
                   "forty combinations, none of them about graphics", "xs"))

    b.append(label(W / 2, H - 18,
                   "cgltf_accessor_read_float collapses all forty. That is the whole argument.", "sm"))

    return svg(uid, W, H,
               "The accessor chain and the combinatorial matrix behind it",
               "Three linked boxes: accessor 0 (VEC3 of FLOAT, count 8) points at "
               "bufferView 0 (offset 0, length 96 bytes), which points at buffer 0 "
               "(the external file cube.bin, 328 bytes). Below, four independent "
               "axes of variation — component type, normalized, packing and "
               "sparseness — multiplying out to forty legal encodings of the same "
               "eight positions.", b)


# ===========================================================================
# Figure 3 — four conventions, three of which agree  (§4)
# ===========================================================================
def fig_conventions():
    uid = "l66f3"
    # H is set by the arithmetic, not by eye: four rows from y = 92 at 68 apart
    # put the last row's note at 326, so the footer cannot start before 350.
    W, H = 720, 404
    b = [box(0, 0, W, H)]
    b.append(label(W / 2, 26, "Four conventions. Three agree; one is not a conversion.", "lg"))

    rows = [
        ("handedness", "right-handed, +Y up", "right-handed, Y-up, −Z fwd",
         "AGREES", GREEN, "no axis swap, no negated z"),
        ("triangle winding", "counter-clockwise = front", "counter-clockwise = front",
         "AGREES", GREEN, "no index reversal"),
        ("texture origin", "(0,0) is the UPPER left", "(0,0) is the upper left",
         "AGREES", GREEN, "and OBJ's is the LOWER left"),
        ("asset facing", "front faces +Z", "camera looks down −Z",
         "DIFFERS", AMBER, "an authoring fact, not a coordinate change"),
    ]

    b.append(label(250, 62, "glTF says", "xs"))
    b.append(label(410, 62, "this course says", "xs"))
    b.append(rule(24, 70, W - 24, 70, "grid", 1.0))

    y = 92
    for name, theirs, ours, verdict, col, note in rows:
        b.append(box(24, y - 14, W - 48, 60, col, opacity=0.07))
        b.append(label(30, y + 4, name, "sm", anchor="start"))
        b.append(label(250, y + 4, theirs, "xs"))
        b.append(label(410, y + 4, ours, "xs"))
        b.append(box(500, y - 8, 72, 20, col, opacity=0.36))
        b.append(label(536, y + 5, verdict, "xs"))
        b.append(label(W / 2, y + 30, note, "xs"))
        y += 68

    b.append(box(24, 354, W - 48, 34, None))
    b.append(label(W / 2, 376,
                   "The loader contains ZERO conversion code — and that is Module 2 having "
                   "argued rather than guessed.", "sm"))

    return svg(uid, W, H,
               "The four convention comparisons between glTF and this course",
               "A four-row table. Handedness, triangle winding and texture "
               "coordinate origin all agree between glTF and this course, marked "
               "AGREES in green. The fourth row, asset facing, differs — glTF says "
               "an asset's front faces plus Z and our camera looks down minus Z — "
               "and is marked DIFFERS in amber, with the note that it is an "
               "authoring fact rather than a coordinate change.", b)


# ===========================================================================
# Figure 4 — the spec's BRDF against ours, term by term  (§6)
# ===========================================================================
def fig_brdf():
    uid = "l66f4"
    W, H = 720, 430
    b = [box(0, 0, W, H)]
    b.append(label(W / 2, 26, "The ⚠ VERIFY, discharged: one factor differs", "lg"))

    # Two columns of terms.
    cols = [("Khronos glTF 2.0, Appendix B", 24, BLUE),
            ("this engine, Lessons 6.3 + 6.4", 372, GREEN)]
    cw = 324
    for name, x, col in cols:
        b.append(hollow(x, 46, cw, 250, col))
        b.append(label(x + cw / 2, 68, name, "sm"))
        b.append(rule(x + 12, 78, x + cw - 12, 78, "grid", 1.0))

    terms = [
        ("distribution", "GGX, alpha = roughness^2", "ndf(ggx), alpha_from_roughness", True),
        ("geometry", "Smith, height-correlated", "smith_g, height-correlated", True),
        ("Fresnel", "Schlick, f90 = 1", "fresnel_schlick", True),
        ("dielectric F0", "0.04, from ior 1.5", "k_dielectric_f0 = 0.04", True),
        ("metal blend", "mix of two BRDFs", "one BRDF, lerped F0", True),
        ("diffuse coupling", "1 − F(v·h)   [one crossing]",
         "(1−F(n·l))(1−F(n·v))   [two]", False),
    ]
    y = 100
    for name, theirs, ours, same in terms:
        col = GREEN if same else RED
        op = 0.10 if same else 0.26
        b.append(box(24 + 12, y - 13, cw - 24, 20, col, opacity=op))
        b.append(box(372 + 12, y - 13, cw - 24, 20, col, opacity=op))
        b.append(label(24 + cw / 2, y, theirs, "xs"))
        b.append(label(372 + cw / 2, y, ours, "xs"))
        b.append(label(W / 2, y, "=" if same else "≠", "sm"))
        y += 32

    y_label = 100
    for name, _, _, _ in terms:
        y_label += 32

    # The measured consequence.
    b.append(box(24, 306, W - 48, 108, None))
    b.append(label(W / 2, 328, "…and the difference is measured, not argued", "sm"))

    facts = [
        (f"metal blend, worst error over {LERP_POINTS} points", f"{LERP_ERROR}",
         "identical: Schlick is affine in f0", GREEN),
        ("diffuse coupling at normal incidence", f"{NORMAL_RATIO:.3f}x",
         "one crossing of a 4% interface", GREEN),
        ("diffuse coupling at 88 degrees", f"{WORST_RATIO:.2f}x",
         "which is why it hides at the centre and shows at the rim", AMBER),
        ("white furnace: spec's form / ours",
         f"{FURNACE_SPEC:.4f} / {FURNACE_OURS:.4f}",
         "the spec REQUIRES energy conservation; only one form has it", RED),
    ]
    y = 350
    for what, value, why, col in facts:
        b.append(label(298, y, what, "xs", anchor="end"))
        b.append(box(306, y - 11, 90, 16, col, opacity=0.30))
        b.append(label(351, y, value, "xs"))
        b.append(label(406, y, why, "xs", anchor="start"))
        y += 18

    return svg(uid, W, H,
               "The glTF reference BRDF compared term by term against this engine's",
               "Two columns of six terms. Distribution, geometry term, Fresnel "
               "approximation, dielectric F0 and the metallic blend are all marked "
               "identical in green. Only the diffuse coupling differs, marked in "
               "red: the spec applies one Fresnel crossing at the half vector, this "
               "engine applies two, at the light and eye angles. Below, four "
               "measured consequences.", b)


# ===========================================================================
# Figure 5 — the layers, and where a reference stops being a string  (§7)
# ===========================================================================
def fig_layers():
    uid = "l66f5"
    W, H = 720, 400
    b = [box(0, 0, W, H)]
    b.append(label(W / 2, 26, "Where a reference stops being a string", "lg"))

    layers = [
        ("the file", "assets/cube.gltf + cube.bin", GREY,
         ['"uri": "uv_grid.png"', '"material": 0'], "bytes"),
        ("parse_gltf / load_gltf", "gfx/gltf.cpp — the only file that sees cgltf", BLUE,
         ['base_colour_uri = "uv_grid.png"', "material = 0"], "descriptions"),
        ("asset_store::load_model", "asset/asset_store.cpp — search path, cache, pools", AMBER,
         ["albedo_map = handle 3:1", "material_handle 0:1"], "handles"),
        ("scene_object", "the renderer's own vocabulary", GREEN,
         ["mat.albedo_map", "bind_albedo() -> const texture*"], "pointers, once per draw"),
    ]
    lh = 68
    for i, (name, sub, col, rows, kind) in enumerate(layers):
        y = 48 + i * (lh + 16)
        b.append(box(24, y, W - 48, lh, col, opacity=0.09))
        b.append(hollow(24, y, W - 48, lh, col))
        b.append(label(36, y + 24, name, "sm", anchor="start"))
        b.append(label(36, y + 44, sub, "xs", anchor="start"))
        b.append(box(290, y + 10, 210, 20, col, opacity=0.26))
        b.append(label(395, y + 24, rows[0], "xs"))
        b.append(label(395, y + 48, rows[1], "xs"))
        b.append(box(520, y + 22, 160, 20, None))
        b.append(label(600, y + 36, kind, "xs"))
        if i < 3:
            b.append(arrow(W / 2, y + lh + 2, W / 2, y + lh + 13, uid, "i", 1.4))

    b.append(label(W / 2, H - 20,
                   "A parser that returned handles could never be tested from a string literal, "
                   "nor run offline in Module 9.", "xs"))

    return svg(uid, W, H,
               "The four layers a glTF texture reference passes through",
               "Four stacked layers. The file holds a URI string and a material "
               "index. parse_gltf turns them into descriptions, still strings and "
               "indices. asset_store::load_model resolves them into handles against "
               "its search path and pools. The renderer resolves a handle to a "
               "pointer once per draw through bind_albedo.", b)


# ===========================================================================
# Figure 6 — what you should see  (§9)
# ===========================================================================
def fig_result():
    uid = "l66f6"
    W, H = 720, 340
    b = [box(0, 0, W, H)]
    b.append(label(W / 2, 24, "gltf_view --model cube.gltf", "lg"))

    # A mock of the framebuffer.
    fx, fy, fw, fh = 24, 44, 400, 252
    b.append(box(fx, fy, fw, fh, "#0c0e14", opacity=1.0))
    b.append(hollow(fx, fy, fw, fh, GREY))

    # An isometric-ish cube with the uv grid on it. Computed, not hand-placed.
    cx, cy = fx + fw / 2, fy + fh / 2 + 8
    s = 66
    top = [(cx, cy - s * 1.15), (cx + s, cy - s * 0.58),
           (cx, cy), (cx - s, cy - s * 0.58)]
    left = [(cx - s, cy - s * 0.58), (cx, cy), (cx, cy + s * 0.95), (cx - s, cy + s * 0.38)]
    right = [(cx, cy), (cx + s, cy - s * 0.58), (cx + s, cy + s * 0.38), (cx, cy + s * 0.95)]

    def poly(pts, fill, op):
        d = " ".join(f"{x:.1f},{y:.1f}" for x, y in pts)
        return (f'<polygon points="{d}" fill="{fill}" fill-opacity="{op}" '
                f'stroke="{GREY}" stroke-opacity="0.5" stroke-width="0.8"/>')

    b.append(poly(top, AMBER, 0.80))
    b.append(poly(left, "#e8e8e8", 0.62))
    b.append(poly(right, "#e8e8e8", 0.30))

    # The checker on the left face, as alternating quads along its own axes.
    for i in range(4):
        for j in range(4):
            if (i + j) % 2:
                continue
            u0, u1 = i / 4.0, (i + 1) / 4.0
            v0, v1 = j / 4.0, (j + 1) / 4.0

            def at(u, v):
                ax = left[0][0] + (left[1][0] - left[0][0]) * u
                ay = left[0][1] + (left[1][1] - left[0][1]) * u
                bx = left[3][0] + (left[2][0] - left[3][0]) * u
                by = left[3][1] + (left[2][1] - left[3][1]) * u
                return (ax + (bx - ax) * v, ay + (by - ay) * v)

            b.append(poly([at(u0, v0), at(u1, v0), at(u1, v1), at(u0, v1)], "#141414", 0.85))

    # The orientation arrow, which is the whole point of the picture.
    b.append(arrow(cx - s * 0.5, cy + s * 0.52, cx - s * 0.5, cy - s * 0.26, uid, "h", 3.4))
    b.append(label(cx - s * 0.5, cy + s * 0.92, "+v", "sm"))

    # ---- the annotations --------------------------------------------------
    ax0 = 448
    notes = [
        ("the arrow points UP", GREEN,
         "so v was NOT flipped. OBJ needs the", "flip; glTF must not have it."),
        ("the image was found", GREEN,
         "\"uv_grid.png\" resolved against the", "store's search path, not the .gltf's."),
        ("roughness 0.4 came", AMBER,
         "from the FILE. Nothing in gltf_view", "types a colour or a shininess."),
    ]
    y = 66
    for title, col, l1, l2 in notes:
        b.append(box(ax0, y, W - ax0 - 24, 68, col, opacity=0.08))
        b.append(hollow(ax0, y, W - ax0 - 24, 68, col))
        b.append(label(ax0 + 12, y + 22, title, "sm", anchor="start"))
        b.append(label(ax0 + 12, y + 42, l1, "xs", anchor="start"))
        b.append(label(ax0 + 12, y + 58, l2, "xs", anchor="start"))
        y += 80

    b.append(label(W / 2, H - 22,
                   "480 x 270, one directional light, back-face culling gated on validate(). "
                   "An SVG mock; the real frame is a PPM.", "xs"))

    return svg(uid, W, H,
               "A mock of the expected result: the textured glTF cube",
               "A dark framebuffer containing a cube drawn in three-quarter view. "
               "Its left face carries a black and white checkerboard with an arrow "
               "pointing upward, its top face is amber. Three annotations to the "
               "right note that the upward arrow proves the v coordinate was not "
               "flipped, that the external image resolved against the asset "
               "store's search path, and that the roughness came from the file.", b)


# ===========================================================================
def main():
    figures = [
        ("l66_fig1.svg", fig_formats()),
        ("l66_fig2.svg", fig_accessor()),
        ("l66_fig3.svg", fig_conventions()),
        ("l66_fig4.svg", fig_brdf()),
        ("l66_fig5.svg", fig_layers()),
        ("l66_fig6.svg", fig_result()),
    ]
    for name, body in figures:
        path = os.path.join(OUT, name)
        with open(path, "w") as fh:
            fh.write(body)
        print(f"wrote {path}  ({len(body)} bytes)")


if __name__ == "__main__":
    main()
