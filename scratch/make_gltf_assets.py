#!/usr/bin/env python3
"""scratch/make_gltf_assets.py — write the two glTF assets Lesson 6.6 ships.

Run from the repository root:

    python3 scratch/make_gltf_assets.py

WHY A GENERATOR AND NOT A DOWNLOADED MODEL. Lesson 3.5 made the same call for
`assets/torus.obj` and stated the reason there: the course ships no third-party
geometry, so every number quoted in a lesson is reproducible from the repository
alone. It also buys the thing that makes verify_66 a real test rather than a
tautology — the geometry written here is BYTE-FOR-BYTE the same cube
`cube_mesh()` returns, so loading it and comparing against the engine's own
built-in cube is a genuine end-to-end check of the accessor path, written by a
different implementation in a different language.

Two files, chosen to cover the parts of the format that behave differently:

  assets/cube.gltf   JSON + an EXTERNAL .bin + an EXTERNAL image URI. This is
                     what an exporter writes by default, and it is the case that
                     exercises relative-path resolution — the one that breaks
                     when somebody copies the .gltf and forgets the .bin.
  assets/shapes.glb  The BINARY container: one file, JSON chunk + BIN chunk, no
                     external anything. Three primitives under a node hierarchy
                     with three materials, including a fully metallic one, so
                     that `f0_of`'s metal branch has an asset to run on.
"""
import json
import os
import struct

OUT_DIR = "assets"

# ---------------------------------------------------------------------------
#  glTF's magic numbers, which are OpenGL's
# ---------------------------------------------------------------------------
#
# The component and target enumerants in a glTF file are literal GLenum values.
# That is history showing through the format, and it is worth writing them out
# with their GL names rather than as bare integers, because that is how they
# read in every other tool you will open the file in.
FLOAT = 5126           # GL_FLOAT
UNSIGNED_SHORT = 5123  # GL_UNSIGNED_SHORT
ARRAY_BUFFER = 34962         # GL_ARRAY_BUFFER
ELEMENT_ARRAY_BUFFER = 34963  # GL_ELEMENT_ARRAY_BUFFER
NEAREST = 9728
LINEAR = 9729
REPEAT = 10497
CLAMP_TO_EDGE = 33071
TRIANGLES = 4


# ---------------------------------------------------------------------------
#  Geometry — the engine's own, transcribed
# ---------------------------------------------------------------------------

# engine/include/engine/gfx/mesh.hpp, k_cube_vertices. Eight corners, centred on
# the origin.
CUBE_VERTICES = [
    (-0.5, -0.5, -0.5), (+0.5, -0.5, -0.5), (+0.5, +0.5, -0.5), (-0.5, +0.5, -0.5),
    (-0.5, -0.5, +0.5), (+0.5, -0.5, +0.5), (+0.5, +0.5, +0.5), (-0.5, +0.5, +0.5),
]

# k_cube_indices. Counter-clockwise seen from outside — which is the course's
# front-face convention AND glTF's ("the winding order determines front- and
# back-facing sides ... counter-clockwise"), so no reversal is needed here
# either. Two conventions agreeing is the third one this lesson gets for free.
CUBE_INDICES = [
    0, 3, 2, 0, 2, 1,
    4, 5, 6, 4, 6, 7,
    0, 4, 7, 0, 7, 3,
    1, 2, 6, 1, 6, 5,
    3, 7, 6, 3, 6, 2,
    0, 1, 5, 0, 5, 4,
]

# Per-CORNER texture coordinates cannot exist on an 8-vertex cube — that is
# Lesson 3.5's index problem, and it is exactly why a textured cube needs 24
# vertices. This asset carries a uv per CORNER of the cube's bounding box
# instead, which is a projection rather than an unwrap: it puts the uv grid on
# the box without splitting a single vertex, so the loaded mesh can be compared
# index-for-index against `cube_mesh()`. A real exporter would split.
CUBE_UVS = [
    (0.0, 1.0), (1.0, 1.0), (1.0, 0.0), (0.0, 0.0),
    (0.0, 1.0), (1.0, 1.0), (1.0, 0.0), (0.0, 0.0),
]

# SMOOTH normals — the outward diagonal at each corner — and the choice matters
# for what the asset can test. A cube with FLAT normals cannot have eight
# vertices at all: a corner where three faces meet needs three different normals
# and a vertex carries exactly one, so flat shading forces the 8 -> 24 split
# (Lesson 3.5's index problem). Smooth normals are legal glTF, keep the vertex
# count at eight, and let verify_66 §A compare the loaded mesh against
# `cube_mesh()` INDEX FOR INDEX.
#
# They also make a second point that only an asset can make: a file's normals
# are AUTHORSHIP, and `with_normals` returns geometry that already has them
# unchanged whatever style is asked for. shapes.glb ships no normals at all, so
# the generation path — and the split it causes — is tested there instead.
_INV_SQRT3 = 3.0 ** -0.5
CUBE_NORMALS = [tuple(c * 2.0 * _INV_SQRT3 for c in v) for v in CUBE_VERTICES]


def octahedron():
    """Six vertices, eight faces, wound outward. Small, closed, and NOT a cube —
    so the multi-primitive file is not three copies of one test."""
    v = [(0.0, 0.6, 0.0), (0.6, 0.0, 0.0), (0.0, 0.0, 0.6),
         (-0.6, 0.0, 0.0), (0.0, 0.0, -0.6), (0.0, -0.6, 0.0)]
    i = [0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 1,
         5, 2, 1, 5, 3, 2, 5, 4, 3, 5, 1, 4]
    return v, i


def tetrahedron():
    v = [(0.0, 0.5, 0.0), (0.5, -0.35, 0.35),
         (-0.5, -0.35, 0.35), (0.0, -0.35, -0.6)]
    i = [0, 1, 2, 0, 2, 3, 0, 3, 1, 1, 3, 2]
    return v, i


# ---------------------------------------------------------------------------
#  Buffer assembly
# ---------------------------------------------------------------------------

class BufferBuilder:
    """Accumulates binary data and the accessors/views that describe it.

    Every offset in glTF is measured from the start of the BUFFER, and views
    must respect their component's alignment — a `float` accessor whose view
    starts at byte 2 is illegal and, worse, is the kind of illegal that appears
    to work in a loader that memcpys. `_align` is the four lines that prevent it.
    """

    def __init__(self):
        self.data = bytearray()
        self.views = []
        self.accessors = []

    def _align(self, n=4):
        while len(self.data) % n != 0:
            self.data.append(0)

    def add_vec3(self, values, target=ARRAY_BUFFER):
        self._align()
        offset = len(self.data)
        for x, y, z in values:
            self.data += struct.pack("<fff", x, y, z)
        self.views.append({"buffer": 0, "byteOffset": offset,
                           "byteLength": len(self.data) - offset, "target": target})
        # min/max are REQUIRED on a POSITION accessor — the spec says so because
        # a viewer needs the bounding box to frame the asset without decoding
        # every vertex, and cgltf_validate rejects a file without them.
        xs = [v[0] for v in values]
        ys = [v[1] for v in values]
        zs = [v[2] for v in values]
        self.accessors.append({
            "bufferView": len(self.views) - 1, "componentType": FLOAT,
            "count": len(values), "type": "VEC3",
            "min": [min(xs), min(ys), min(zs)],
            "max": [max(xs), max(ys), max(zs)],
        })
        return len(self.accessors) - 1

    def add_vec2(self, values):
        self._align()
        offset = len(self.data)
        for u, v in values:
            self.data += struct.pack("<ff", u, v)
        self.views.append({"buffer": 0, "byteOffset": offset,
                           "byteLength": len(self.data) - offset, "target": ARRAY_BUFFER})
        self.accessors.append({
            "bufferView": len(self.views) - 1, "componentType": FLOAT,
            "count": len(values), "type": "VEC2",
        })
        return len(self.accessors) - 1

    def add_indices(self, values):
        self._align()
        offset = len(self.data)
        for i in values:
            self.data += struct.pack("<H", i)
        self.views.append({"buffer": 0, "byteOffset": offset,
                           "byteLength": len(self.data) - offset,
                           "target": ELEMENT_ARRAY_BUFFER})
        self.accessors.append({
            "bufferView": len(self.views) - 1, "componentType": UNSIGNED_SHORT,
            "count": len(values), "type": "SCALAR",
        })
        return len(self.accessors) - 1


# ---------------------------------------------------------------------------
#  assets/cube.gltf — text JSON, external buffer, external image
# ---------------------------------------------------------------------------

def write_cube_gltf():
    buf = BufferBuilder()
    pos = buf.add_vec3(CUBE_VERTICES)
    nrm = buf.add_vec3(CUBE_NORMALS)
    uv = buf.add_vec2(CUBE_UVS)
    idx = buf.add_indices(CUBE_INDICES)

    gltf = {
        "asset": {"version": "2.0",
                  "generator": "3d-engine-course scratch/make_gltf_assets.py"},
        "scene": 0,
        "scenes": [{"name": "cube scene", "nodes": [0]}],
        "nodes": [{"name": "cube", "mesh": 0}],
        "meshes": [{
            "name": "unit cube",
            "primitives": [{
                "attributes": {"POSITION": pos, "NORMAL": nrm, "TEXCOORD_0": uv},
                "indices": idx,
                "material": 0,
                "mode": TRIANGLES,
            }],
        }],
        "materials": [{
            "name": "grid",
            "pbrMetallicRoughness": {
                # WHITE, deliberately: the factor multiplies the texture in glTF
                # and replaces it in this engine (3.9), and white is the value
                # at which those two rules agree exactly. verify_66 §F asserts
                # the import is lossless here, and shapes.glb carries the
                # non-white case so the divergence has an asset too.
                "baseColorFactor": [1.0, 1.0, 1.0, 1.0],
                "baseColorTexture": {"index": 0},
                "metallicFactor": 0.0,
                "roughnessFactor": 0.4,
            },
            "doubleSided": False,
        }],
        "textures": [{"source": 0, "sampler": 0}],
        # An EXTERNAL image, and one that already ships: assets/uv_grid.png has
        # been in this repository since Lesson 3.9, so the file resolves against
        # the same search path everything else uses.
        "images": [{"uri": "uv_grid.png"}],
        "samplers": [{"magFilter": LINEAR, "minFilter": LINEAR,
                      "wrapS": CLAMP_TO_EDGE, "wrapT": CLAMP_TO_EDGE}],
        "buffers": [{"uri": "cube.bin", "byteLength": len(buf.data)}],
        "bufferViews": buf.views,
        "accessors": buf.accessors,
    }

    with open(os.path.join(OUT_DIR, "cube.bin"), "wb") as fh:
        fh.write(bytes(buf.data))
    with open(os.path.join(OUT_DIR, "cube.gltf"), "w") as fh:
        json.dump(gltf, fh, indent=2)
        fh.write("\n")
    return len(buf.data)


# ---------------------------------------------------------------------------
#  assets/shapes.glb — the binary container
# ---------------------------------------------------------------------------

def write_shapes_glb():
    buf = BufferBuilder()

    # NO NORMAL attribute on any primitive here, deliberately. The spec says a
    # client SHOULD compute flat normals when a primitive has none, and doing so
    # SPLITS the vertices — 8 corners become 36 face-corners on the cube — which
    # is 3.5's index problem arriving in a second format. cube.gltf ships
    # normals so its round trip stays index-for-index; this file ships none so
    # the generation path has an asset.
    cube_pos = buf.add_vec3(CUBE_VERTICES)
    cube_idx = buf.add_indices(CUBE_INDICES)

    oct_v, oct_i = octahedron()
    oct_pos = buf.add_vec3(oct_v)
    oct_idx = buf.add_indices(oct_i)

    tet_v, tet_i = tetrahedron()
    tet_pos = buf.add_vec3(tet_v)
    tet_idx = buf.add_indices(tet_i)

    # The plinth reuses the cube's geometry, scaled flat by its node — which is
    # itself worth having in the asset, because it means two primitives share one
    # SHAPE and differ only in placement. A loader that baked node transforms
    # into vertices could not express that at all.
    plinth_pos = buf.add_vec3(CUBE_VERTICES)
    plinth_idx = buf.add_indices(CUBE_INDICES)

    gltf = {
        "asset": {"version": "2.0",
                  "generator": "3d-engine-course scratch/make_gltf_assets.py"},
        "scene": 0,
        # A NODE HIERARCHY, not three siblings — because the flattening in
        # walk_node has to be worth testing. "group" translates and its children
        # translate again, so the octahedron's world transform is a product of
        # two matrices and verify_66 §C can check the arithmetic by hand.
        "scenes": [{"name": "shapes", "nodes": [0]}],
        "nodes": [
            {"name": "group", "translation": [1.0, 0.0, 0.0], "children": [1, 2, 4]},
            {"name": "silver cube", "mesh": 0},
            {"name": "gold octahedron", "mesh": 1,
             "translation": [0.0, 2.0, 0.0], "children": [3]},
            {"name": "red tetrahedron", "mesh": 2, "translation": [0.0, 1.5, 0.0]},
            # NO MESH on this one, only a scale — a pure grouping/placement node,
            # which is legal and common (an exporter emits them for empties and
            # for pivot corrections). walk_node must visit it, count it, and
            # produce nothing.
            {"name": "plinth", "mesh": 3, "translation": [0.0, -1.2, 0.0],
             "scale": [2.4, 0.25, 2.4]},
        ],
        "meshes": [
            {"name": "cube", "primitives": [
                {"attributes": {"POSITION": cube_pos}, "indices": cube_idx,
                 "material": 0, "mode": TRIANGLES}]},
            {"name": "octahedron", "primitives": [
                {"attributes": {"POSITION": oct_pos}, "indices": oct_idx,
                 "material": 1, "mode": TRIANGLES}]},
            {"name": "tetrahedron", "primitives": [
                {"attributes": {"POSITION": tet_pos}, "indices": tet_idx,
                 "material": 2, "mode": TRIANGLES}]},
            # NO MATERIAL on the plinth, on purpose: the spec says such a
            # primitive uses the default material (white, metallic 1, rough 1),
            # and a file where every primitive is painted never exercises that.
            # It is also a deliberately UGLY default — a fully rough conductor is
            # dark and colourless (6.3 measured it losing 69% of its energy) —
            # so a primitive that lost its material looks like it did.
            {"name": "plinth", "primitives": [
                {"attributes": {"POSITION": plinth_pos}, "indices": plinth_idx,
                 "mode": TRIANGLES}]},
        ],
        "materials": [
            # BRUSHED, not polished, and the roughness is chosen from a
            # measurement rather than by eye. At roughness 0.25 a conductor's
            # mirror-direction radiance under this engine's reference irradiance
            # is 64x displayable white; at 0.40 it is 3.4x. Both clip — there is
            # no tonemapper until Lesson 6.10 — but one clips a facet and the
            # other clips the object. See the lesson's Expected Result.
            {"name": "silver",
             "pbrMetallicRoughness": {"baseColorFactor": [0.95, 0.93, 0.88, 1.0],
                                      "metallicFactor": 1.0,
                                      "roughnessFactor": 0.40}},
            # GOLD, with the measured F0 of a real conductor — (1.00, 0.71,
            # 0.29) is the value Lesson 6.4 quotes, and here it arrives as
            # baseColor because for a metal the base colour IS the F0.
            {"name": "gold",
             "pbrMetallicRoughness": {"baseColorFactor": [1.00, 0.71, 0.29, 1.0],
                                      "metallicFactor": 1.0,
                                      "roughnessFactor": 0.45}},
            # A dielectric with a NON-WHITE factor and no texture, so the
            # conformance warning in load_model has a case that does NOT fire —
            # the gap is the factor-and-texture combination, not the factor.
            {"name": "red plastic",
             "pbrMetallicRoughness": {"baseColorFactor": [0.8, 0.12, 0.1, 1.0],
                                      "metallicFactor": 0.0,
                                      "roughnessFactor": 0.55},
             "doubleSided": True},
        ],
        "buffers": [{"byteLength": len(buf.data)}],
        "bufferViews": buf.views,
        "accessors": buf.accessors,
    }

    # ---- The container -----------------------------------------------------
    #
    # A .glb is a 12-byte header followed by chunks. Both the JSON chunk and the
    # BIN chunk must be padded to a 4-byte boundary, and — this is the part
    # people get wrong — the padding BYTE differs: JSON pads with spaces (0x20)
    # so the chunk stays parseable text, BIN pads with zeros. A zero inside the
    # JSON chunk is a parse error in most readers.
    json_bytes = json.dumps(gltf, separators=(",", ":")).encode("utf-8")
    json_bytes += b" " * ((4 - len(json_bytes) % 4) % 4)

    bin_bytes = bytes(buf.data)
    bin_bytes += b"\x00" * ((4 - len(bin_bytes) % 4) % 4)

    total = 12 + 8 + len(json_bytes) + 8 + len(bin_bytes)

    out = bytearray()
    out += b"glTF"                            # magic — what cgltf sniffs for
    out += struct.pack("<I", 2)               # version
    out += struct.pack("<I", total)           # total length, header included
    out += struct.pack("<I", len(json_bytes))
    out += b"JSON"
    out += json_bytes
    out += struct.pack("<I", len(bin_bytes))
    out += b"BIN\x00"                         # note the trailing NUL: 4 bytes
    out += bin_bytes

    path = os.path.join(OUT_DIR, "shapes.glb")
    with open(path, "wb") as fh:
        fh.write(bytes(out))
    return len(out)


if __name__ == "__main__":
    os.makedirs(OUT_DIR, exist_ok=True)
    n = write_cube_gltf()
    m = write_shapes_glb()
    print(f"wrote {OUT_DIR}/cube.gltf + cube.bin ({n} bytes of buffer)")
    print(f"wrote {OUT_DIR}/shapes.glb ({m} bytes total)")
