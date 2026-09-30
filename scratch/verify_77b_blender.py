"""scratch/verify_77b_blender.py — Lesson 7.7b's three Blender-side checks.

    blender --background --factory-startup --python scratch/verify_77b_blender.py

verify_77b §E.7 found the mannequin's two exports disagree by 0.218 degrees on the
frames, and that the files alone disagree by exactly that much. This asks Blender
WHY, in two steps, the first of which refused the explanation written for it:

  HANDLES  the first explanation was that Blender's automatic handles do not sit a
           third of the way along unequal intervals, so a slope-only conversion to
           Hermite tangents is inexact. Blender says they do: 1.333 of 4 frames,
           1.000 of 3. That explanation is dead.
  TAN      Blender's own curve against two Hermites: one with the fcurves' true
           tangents, one with the tangents the exporter writes — a control point one
           frame of slope past the key, passed through `transform_rotation`, whose
           first line is `rotation.normalize()`. The first reproduces Blender's curve
           exactly; the second is the 0.218 degrees.
  INFL     Exercise 4's premise: with the exporter's influence limit raised to 8, how
           many JOINTS_n sets does it write for this body? (One: no vertex has a fifth.)
"""
import bpy, math
from mathutils import Quaternion
src = open("scratch/make_mannequin.py").read().replace("\nmain()\n", "\n")
g = {"__name__": "mm"}
exec(compile(src, "make_mannequin.py", "exec"), g)
scene = g["reset"](); body = g["build_body"](scene); arm = g["build_armature"](scene)
g["bind"](body, arm); act = g["make_walk"](arm)
fcs = []
for layer in act.layers:
    for strip in layer.strips:
        for cb in strip.channelbags:
            fcs += list(cb.fcurves)

# ---- HANDLES --------------------------------------------------------------
for fc in fcs:
    if 'thigh.L' in fc.data_path and 'rotation_quaternion' in fc.data_path and fc.array_index == 1:
        kps = fc.keyframe_points
        for i, k in enumerate(kps):
            if i == 0 or i + 1 == len(kps):
                continue
            left = (k.co[0] - k.handle_left[0]) / (k.co[0] - kps[i - 1].co[0])
            right = (k.handle_right[0] - k.co[0]) / (kps[i + 1].co[0] - k.co[0])
            print(f"HANDLES frame {k.co[0]:4.0f}: left {left:.3f} of its interval, "
                  f"right {right:.3f}  ({k.handle_left_type})")

# ---- TAN ------------------------------------------------------------------
def angle(a, b):
    d = abs(sum(x * y for x, y in zip(a, b)))
    return 2 * math.degrees(math.acos(min(1.0, d)))
def unit(q):
    l = math.sqrt(sum(x * x for x in q)); return [x / l for x in q]
worst = {"linear": 0.0, "exporter": 0.0}
for bone in ("shin.R", "shin.L", "thigh.L", "upper_arm.L"):
    comp = [None] * 4
    for fc in fcs:
        if f'"{bone}"' in fc.data_path and fc.data_path.endswith("rotation_quaternion"):
            comp[fc.array_index] = fc
    keys = [k.co[0] for k in comp[0].keyframe_points]
    vals = [[comp[i].keyframe_points[k].co[1] for i in range(4)] for k in range(len(keys))]
    def slope(i, k, side):
        kp = comp[i].keyframe_points[k]; h = kp.handle_right if side == "r" else kp.handle_left
        return (h[1] - kp.co[1]) / (h[0] - kp.co[0])
    for f10 in range(0, 301):
        f = f10 / 10.0
        k = max(i for i in range(len(keys) - 1) if keys[i] <= f) if f < keys[-1] else len(keys) - 2
        td = keys[k + 1] - keys[k]; u = (f - keys[k]) / td
        h = [2*u**3-3*u**2+1, u**3-2*u**2+u, -2*u**3+3*u**2, u**3-u**2]
        blender = unit([comp[i].evaluate(f) for i in range(4)])
        lin = unit([h[0]*vals[k][i] + td*h[1]*slope(i, k, "r") + h[2]*vals[k+1][i] + td*h[3]*slope(i, k+1, "l") for i in range(4)])
        # the exporter: control = value + one frame of slope, NORMALISED, minus value
        cr = unit([vals[k][i] + slope(i, k, "r") for i in range(4)])
        cl = unit([vals[k+1][i] + slope(i, k+1, "l") for i in range(4)])
        vk = unit(vals[k]); vk1 = unit(vals[k+1])
        ex = unit([h[0]*vk[i] + td*h[1]*(cr[i]-vk[i]) + h[2]*vk1[i] + td*h[3]*(cl[i]-vk1[i]) for i in range(4)])
        a_lin, a_ex = angle(blender, lin), angle(blender, ex)
        worst["linear"] = max(worst["linear"], a_lin); worst["exporter"] = max(worst["exporter"], a_ex)
        if bone == "shin.R" and f in (1.0, 2.0, 3.0):
            print(f"TAN {bone} frame {f:.0f}: linear tangents {a_lin:.4f} deg, exporter's {a_ex:.4f} deg")
print(f"TAN worst over four bones, every tenth of a frame: linear {worst['linear']:.4f}, exporter {worst['exporter']:.4f}")

# ---- INFL -------------------------------------------------------------------
# Exercise 4's premise: does this body have any vertex with a fifth influence?
# Export the full, fixed character with the exporter's limit raised to 8 and count
# the JOINTS_n sets it writes. One set means no vertex needed a fifth.
import json, os, struct
scene = g["reset"](); body = g["build_body"](scene); arm = g["build_armature"](scene)
g["bind"](body, arm); g["weight_the_missed"](body, arm)
out = os.path.join(os.getcwd(), "scratch", "_mannequin_8infl.glb")
bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", export_yup=True,
                          export_skins=True, export_animations=False,
                          export_influence_nb=8, export_texcoords=False)
raw = open(out, "rb").read()
doc = json.loads(raw[20:20 + struct.unpack_from("<I", raw, 12)[0]])
sets = max(sum(1 for k in p["attributes"] if k.startswith("JOINTS_"))
           for m in doc["meshes"] for p in m["primitives"])
print(f"INFL sets written with the limit at 8: {sets}")
os.remove(out)
