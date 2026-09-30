"""scratch/make_mannequin.py — author Lesson 7.7b's character in Blender, and export it.

Run from the repository root, with Blender 4.2 or later (written and run on 5.1):

    blender --background --factory-startup --python scratch/make_mannequin.py

    (on macOS the binary is /Applications/Blender.app/Contents/MacOS/Blender)

It writes three files:

    assets/mannequin.glb               the character the `mannequin` demo plays,
                                       exported with Blender's DEFAULT animation
                                       settings — every frame sampled, LINEAR
    scratch/mannequin_curves.glb       the same scene exported with sampling OFF,
                                       so the authored Bezier curves arrive as
                                       CUBICSPLINE samplers (verify_77b)
    scratch/mannequin_unfixed.glb      the FIRST export, before step 3b below
                                       existed: kept as the control that proves
                                       the importer's report can see the defect

WHY BLENDER, WHEN LESSON 6.6 WROTE ITS glTF BY HAND. 6.6's generator was right for
6.6: a cube whose every byte is known makes the accessor path testable to the bit.
It is wrong for this lesson, and the reason is Module 8's hardest-won learning —
hand-picked test data agrees with the code by construction. A file written by the
same person who writes the importer encodes that person's reading of the
specification twice, and the importer passes. So the file below is written by
Blender's own glTF exporter, which shares nothing with this engine: its joint
order, its inverse bind matrices, its quaternion signs, its key times and its
weight quantisation are all Blender's decisions, and every one of them is
something the importer has to cope with rather than something it was told.

What IS ours is the geometry and the motion, authored by script so that anyone
with Blender can rebuild the file and get the same one. The course ships no
third-party geometry (Lesson 3.5); this is not third-party geometry, it is ours,
exported by a third-party tool — and the tool's behaviour is the part under test.

THE BUILD, IN THE ORDER A RIGGER WOULD DO IT BY HAND:

  1. A stick figure as a graph of vertices and edges, and a SKIN modifier that
     grows a tube of a chosen radius along every edge. With a subdivision
     surface on top, that is one continuous, watertight body in a few lines —
     the continuity matters, because Lesson 7.6's point was that skinning is
     for surfaces no rigid part can bend.
  2. An armature with nineteen named bones laid along the same graph.
  3. Parent the body to the armature with AUTOMATIC WEIGHTS — Blender's bone
     heat diffusion, not a formula of ours. The weights are therefore the
     tool's, and the exporter's four-influence limit and quantisation are
     applied to them exactly as they would be to any artist's rig.
  3b. Give every vertex bone heat left unweighted to its nearest bone. THE
     FIRST EXPORT DID NOT HAVE THIS STEP, and the importer's joint table found
     the result: bone heat failed on the nine vertices at the tip of the nose,
     and Blender's exporter — rather than refusing — gave them full weight on a
     twentieth joint it invented, `neutral_bone`, parented to the armature and
     animated by nothing (io_scene_gltf2/blender/exp/primitive_extract.py,
     `__get_bone_data`: "Is not assign to any bone"). The file was valid, every
     weight summed to one, and the nose tip would have stayed behind whenever the
     head turned. Painting the missed vertices onto a bone is exactly what a
     rigger does by hand.
  4. Two actions, a walk and a wave, keyed with Blender's default Bezier
     interpolation, each pushed to its own NLA track so the exporter writes one
     glTF animation per action.

Blender's axes are Z-up with the character facing -Y; the exporter's default
"+Y Up" conversion turns that into glTF's Y-up, +Z-forward (spec §3.4). No
conversion is done here, on purpose: the exporter's is the one under test.
"""
import math
import os
import sys

import bpy
from mathutils import Euler, Quaternion, Vector

REPO = os.getcwd()
OUT_MAIN = os.path.join(REPO, "assets", "mannequin.glb")
OUT_CURVES = os.path.join(REPO, "scratch", "mannequin_curves.glb")

FPS = 30

# ---------------------------------------------------------------------------
#  The figure: joint positions in Blender space (metres, Z up, facing -Y).
#  The character's LEFT is +X — facing -Y with Z up, right = forward x up = -X.
# ---------------------------------------------------------------------------
P = {
    "pelvis":   (0.00,  0.00, 0.95),
    "spine":    (0.00,  0.00, 1.12),
    "chest":    (0.00,  0.00, 1.32),
    "neck":     (0.00,  0.00, 1.50),
    "head":     (0.00,  0.00, 1.62),
    "headtop":  (0.00,  0.00, 1.80),
    "nose":     (0.00, -0.11, 1.66),     # the only part of the figure that is not
                                         # symmetric front-to-back: it says which
                                         # way the character faces, in every render
}
for side, sx in (("L", 1.0), ("R", -1.0)):
    P[f"shoulder.{side}"] = (0.19 * sx, 0.00, 1.44)
    P[f"elbow.{side}"] = (0.37 * sx, 0.03, 1.22)
    P[f"wrist.{side}"] = (0.52 * sx, 0.00, 1.03)
    P[f"handtip.{side}"] = (0.58 * sx, -0.01, 0.94)
    P[f"hip.{side}"] = (0.10 * sx, 0.00, 0.90)
    P[f"knee.{side}"] = (0.11 * sx, -0.03, 0.50)
    P[f"ankle.{side}"] = (0.11 * sx, 0.02, 0.09)
    P[f"toe.{side}"] = (0.11 * sx, -0.14, 0.03)

# The skin modifier's graph: every edge becomes a tube.
EDGES = [("pelvis", "spine"), ("spine", "chest"), ("chest", "neck"), ("neck", "head"),
         ("head", "headtop"), ("head", "nose")]
for s in ("L", "R"):
    EDGES += [("chest", f"shoulder.{s}"), (f"shoulder.{s}", f"elbow.{s}"),
              (f"elbow.{s}", f"wrist.{s}"), (f"wrist.{s}", f"handtip.{s}"),
              ("pelvis", f"hip.{s}"), (f"hip.{s}", f"knee.{s}"),
              (f"knee.{s}", f"ankle.{s}"), (f"ankle.{s}", f"toe.{s}")]

# Tube radius at each vertex (metres): the figure's proportions.
RADIUS = {"pelvis": 0.14, "spine": 0.12, "chest": 0.16, "neck": 0.055, "head": 0.105,
          "headtop": 0.075, "nose": 0.035}
for s in ("L", "R"):
    RADIUS.update({f"shoulder.{s}": 0.065, f"elbow.{s}": 0.048, f"wrist.{s}": 0.036,
                   f"handtip.{s}": 0.03, f"hip.{s}": 0.085, f"knee.{s}": 0.062,
                   f"ankle.{s}": 0.046, f"toe.{s}": 0.042})

# Bones: name -> (head, tail, parent). Parent-before-child in THIS list, which
# is no promise about the order the exporter writes them in.
BONES = [
    ("hips", "pelvis", "spine", None),
    ("spine", "spine", "chest", "hips"),
    ("chest", "chest", "neck", "spine"),
    ("neck", "neck", "head", "chest"),
    ("head", "head", "headtop", "neck"),
]
for s in ("L", "R"):
    BONES += [
        (f"shoulder.{s}", "chest", f"shoulder.{s}", "chest"),
        (f"upper_arm.{s}", f"shoulder.{s}", f"elbow.{s}", f"shoulder.{s}"),
        (f"forearm.{s}", f"elbow.{s}", f"wrist.{s}", f"upper_arm.{s}"),
        (f"hand.{s}", f"wrist.{s}", f"handtip.{s}", f"forearm.{s}"),
        (f"thigh.{s}", f"hip.{s}", f"knee.{s}", "hips"),
        (f"shin.{s}", f"knee.{s}", f"ankle.{s}", f"thigh.{s}"),
        (f"foot.{s}", f"ankle.{s}", f"toe.{s}", f"shin.{s}"),
    ]


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.fps = FPS
    scene.render.fps_base = 1.0
    return scene


def build_body(scene):
    names = list(P)
    mesh = bpy.data.meshes.new("mannequin_graph")
    mesh.from_pydata([P[n] for n in names],
                     [(names.index(a), names.index(b)) for a, b in EDGES], [])
    body = bpy.data.objects.new("body", mesh)
    scene.collection.objects.link(body)

    skin = body.modifiers.new("skin", "SKIN")
    skin.use_smooth_shade = True
    for i, n in enumerate(names):
        sv = mesh.skin_vertices[0].data[i]
        sv.radius = (RADIUS[n], RADIUS[n])
        sv.use_root = (n == "pelvis")
    sub = body.modifiers.new("smooth", "SUBSURF")
    sub.levels = 2
    sub.render_levels = 2

    bpy.context.view_layer.objects.active = body
    body.select_set(True)
    bpy.ops.object.modifier_apply(modifier="skin")
    bpy.ops.object.modifier_apply(modifier="smooth")
    bpy.ops.object.shade_smooth()

    # Two materials, so the character is two glTF primitives sharing ONE skin —
    # the multi-material case every real character is.
    clay = bpy.data.materials.new("clay")
    clay.use_nodes = True
    clay.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.80, 0.52, 0.36, 1.0)
    clay.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.6
    visor = bpy.data.materials.new("visor")
    visor.use_nodes = True
    visor.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.05, 0.16, 0.40, 1.0)
    visor.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.3
    body.data.materials.append(clay)
    body.data.materials.append(visor)
    for poly in body.data.polygons:
        c = poly.center
        # the face plate: the front of the head, around the nose
        poly.material_index = 1 if (c.z > 1.56 and c.y < -0.05) else 0
    return body


def build_armature(scene):
    arm_data = bpy.data.armatures.new("rig")
    arm = bpy.data.objects.new("Armature", arm_data)
    scene.collection.objects.link(arm)
    bpy.context.view_layer.objects.active = arm
    arm.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    made = {}
    for name, h, t, parent in BONES:
        b = arm_data.edit_bones.new(name)
        b.head = Vector(P[h])
        b.tail = Vector(P[t])
        # Roll: keep each bone's local X pointing along world X (or as near as
        # the bone direction allows), so a positive rotation about local X bends
        # every limb the same way — the convention the keys below are written in.
        b.align_roll(Vector((0.0, 0.0, 1.0)) if abs(b.vector.normalized().z) < 0.9
                     else Vector((0.0, -1.0, 0.0)))
        if parent is not None:
            b.parent = made[parent]
            b.use_connect = (arm_data.edit_bones[parent].tail - b.head).length < 1e-6
        made[name] = b
    bpy.ops.object.mode_set(mode="OBJECT")
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"
    return arm


def bind(body, arm):
    bpy.ops.object.select_all(action="DESELECT")
    body.select_set(True)
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.parent_set(type="ARMATURE_AUTO")


def weight_the_missed(body, arm):
    """Step 3b: every vertex with no bone influence goes to its nearest bone.

    "Nearest" is distance to the bone's SEGMENT in the rest pose, which is the
    choice a rigger's brush makes; the exporter's own threshold for "no
    influence" is 0.0001, so the same one is used here."""
    groups = {g.index: g.name for g in body.vertex_groups}
    segments = [(b.name, arm.matrix_world @ b.head_local, arm.matrix_world @ b.tail_local)
                for b in arm.data.bones]
    fixed = 0
    for v in body.data.vertices:
        if any(g.weight > 0.0001 and g.group in groups for g in v.groups):
            continue
        p = body.matrix_world @ v.co
        best, best_d = None, None
        for name, a, b in segments:
            ab = b - a
            t = max(0.0, min(1.0, (p - a).dot(ab) / ab.length_squared))
            d = (a + ab * t - p).length
            if best_d is None or d < best_d:
                best, best_d = name, d
        body.vertex_groups[best].add([v.index], 1.0, "REPLACE")
        fixed += 1
    print(f"make_mannequin: {fixed} vertices had no weight and were given to their nearest bone")
    return fixed


def key(arm, frame, rotations, hips_z=None):
    """Key every bone's rotation at `frame`. `rotations` maps bone -> (x, y, z)
    Euler degrees in the bone's own frame; bones not named are keyed at rest, so
    every action keys every bone and no bone inherits another action's pose."""
    for pb in arm.pose.bones:
        e = rotations.get(pb.name, (0.0, 0.0, 0.0))
        pb.rotation_quaternion = Euler([math.radians(a) for a in e], "XYZ").to_quaternion()
        pb.keyframe_insert("rotation_quaternion", frame=frame)
    if hips_z is not None:
        hips = arm.pose.bones["hips"]
        hips.location = (0.0, hips_z, 0.0)   # bone-local Y is the bone's own axis: up
        hips.keyframe_insert("location", frame=frame)


def walk_pose(phase):
    """A walk at `phase` in [0, 1): one full cycle, left foot forward at 0."""
    s = math.sin(2.0 * math.pi * phase)
    c = math.cos(2.0 * math.pi * phase)
    lift_l = max(0.0, -s)   # the left knee bends while the left leg swings forward
    lift_r = max(0.0, s)
    return {
        "thigh.L": (-28.0 * c, 0.0, 0.0),
        "thigh.R": (28.0 * c, 0.0, 0.0),
        "shin.L": (55.0 * lift_l + 4.0, 0.0, 0.0),
        "shin.R": (55.0 * lift_r + 4.0, 0.0, 0.0),
        "foot.L": (-12.0 * c, 0.0, 0.0),
        "foot.R": (12.0 * c, 0.0, 0.0),
        "upper_arm.L": (22.0 * c, 0.0, 0.0),
        "upper_arm.R": (-22.0 * c, 0.0, 0.0),
        "forearm.L": (-18.0 - 10.0 * max(0.0, c), 0.0, 0.0),
        "forearm.R": (-18.0 - 10.0 * max(0.0, -c), 0.0, 0.0),
        "spine": (0.0, 7.0 * c, 0.0),
        "chest": (4.0, -9.0 * c, 0.0),
        "head": (-4.0, 3.0 * c, 0.0),
    }


def make_walk(arm):
    """One second, keyed every quarter cycle plus the closing key: the last key
    repeats the first, so the loop is seamless and the clip's length is the last
    key's time — the case Lesson 7.7's `duration` comment argues for."""
    act = bpy.data.actions.new("walk")
    arm.animation_data_create()
    arm.animation_data.action = act
    for i in range(9):   # frames 0, 3.75, ... 30 -> rounded to 0, 4, 8, 11, 15, 19, 22, 26, 30
        phase = i / 8.0
        frame = round(phase * 30)
        bob = 0.03 * abs(math.cos(2.0 * math.pi * phase))   # low at contact, high between
        key(arm, frame, walk_pose(phase % 1.0), hips_z=-0.03 + bob)
    return act


def make_wave(arm):
    """Two seconds: the right arm comes up, waves three times, and goes down."""
    act = bpy.data.actions.new("wave")
    arm.animation_data.action = act
    up = {"shoulder.R": (0.0, 0.0, 12.0), "upper_arm.R": (0.0, 0.0, 128.0)}
    key(arm, 0, {})
    key(arm, 12, dict(up, **{"forearm.R": (0.0, 0.0, 15.0)}))
    for k, frame in enumerate(range(18, 48, 6)):
        swing = -12.0 if k % 2 == 0 else 30.0
        key(arm, frame, dict(up, **{"forearm.R": (0.0, 0.0, swing),
                                    "hand.R": (0.0, 0.0, 0.5 * swing),
                                    "head": (0.0, 8.0, 0.0)}))
    key(arm, 60, {})
    return act


def stash_on_nla(arm, actions):
    """One NLA track per action, so the exporter writes one glTF animation each."""
    arm.animation_data.action = None
    for act in actions:
        track = arm.animation_data.nla_tracks.new()
        track.name = act.name
        track.strips.new(act.name, int(act.frame_range[0]), act)
        track.mute = True


def export(path, sampled):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.export_scene.gltf(
        filepath=path,
        export_format="GLB",
        export_yup=True,
        export_skins=True,
        export_animations=True,
        export_animation_mode="ACTIONS",
        export_force_sampling=sampled,
        export_materials="EXPORT",
        export_normals=True,
        export_texcoords=False,
    )
    print(f"make_mannequin: wrote {path} ({os.path.getsize(path):,} bytes, "
          f"{'sampled' if sampled else 'curves'})")


def main():
    scene = reset()
    body = build_body(scene)
    arm = build_armature(scene)
    bind(body, arm)
    walk = make_walk(arm)
    wave = make_wave(arm)
    stash_on_nla(arm, [walk, wave])
    scene.frame_set(0)
    OUT_UNFIXED = os.path.join(REPO, "scratch", "mannequin_unfixed.glb")
    export(OUT_UNFIXED, sampled=True)      # before step 3b: the control
    weight_the_missed(body, arm)
    export(OUT_MAIN, sampled=True)
    export(OUT_CURVES, sampled=False)
    print(f"make_mannequin: {len(body.data.vertices)} vertices, "
          f"{len(body.data.polygons)} faces, {len(arm.data.bones)} bones")


main()
