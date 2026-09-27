#!/usr/bin/env python3
"""scratch/gen_l812_body_e.py — writes scratch/l812_body_e.html, §14 "Writing it".

A ONE-OFF GENERATOR, run once while the lesson was written, so that every code
snippet in §14 is cut from the source it describes rather than retyped into
HTML. Its output is a static fragment like the other five; build_812.py reads
the fragment, not this script, so a later edit to the engine does not move the
page (the discipline LISTING_SOURCE enforces for the full listings).

Snippets are whole functions, cut from their signature to the closing brace at
column 0, or a marked range; `strip` removes // comment lines for the one
snippet long enough to need it, and the caption says so.
"""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def esc(text):
    return (text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
                .replace('"', "&quot;").replace("'", "&#x27;"))


def read(path):
    with open(os.path.join(ROOT, path)) as fh:
        return fh.read().split("\n")


def function(path, signature):
    """From the line starting with `signature` to the first '}' at column 0."""
    lines = read(path)
    start = next(i for i, ln in enumerate(lines) if ln.startswith(signature))
    end = next(i for i in range(start, len(lines)) if lines[i] == "}")
    return "\n".join(lines[start:end + 1])


def between(path, first, last, dedent=0, strip=False):
    """From the line containing `first` to the line containing `last`, inclusive."""
    lines = read(path)
    start = next(i for i, ln in enumerate(lines) if first in ln)
    end = next(i for i in range(start, len(lines)) if last in lines[i])
    out = []
    for ln in lines[start:end + 1]:
        if strip and ln.strip().startswith("//"):
            continue
        out.append(ln[dedent:] if ln[:dedent].strip() == "" else ln)
    return "\n".join(out)


def snippet(caption, code, lang="cpp", label="C++"):
    return ('  <div class="listing">\n'
            f'    <figcaption><span class="path">{caption}</span>\n'
            f'      <span class="lang" data-lang="{lang}">{label}</span></figcaption>\n'
            f'<pre><code class="lang-{lang}">{esc(code)}</code></pre>\n'
            '  </div>\n')


C = "engine/src/phys/constraint.cpp"
R = "engine/src/phys/ragdoll.cpp"
S = "engine/src/phys/solver.cpp"

parts = []
parts.append('''  <h2 id="implementation">14. Writing it</h2>

  <p>
    Three files change and one is new. <code>constraint.hpp</code> grows the swing–twist split, a cone
    on the ball-socket and the position-pass fix; <code>solver.hpp</code> grows one stage; and
    <code>phys/ragdoll.hpp</code> is the seam between the animation system and the solver. It is the
    first file in <code>phys/</code> that knows what a skeleton is, and the arrow points that way on
    purpose: <code>anim/skeleton.hpp</code> includes nothing but maths, a game with no physics must still
    be able to animate, and a ragdoll is a physics object built <em>from</em> a skeleton — 5.12's rule,
    that the arrow points at the more general.
  </p>

  <h3>Step 1: the split</h3>

  <p>
    §3's projection, four lines and a guard. The guard is the one singularity: a swing of exactly
    180° leaves nothing to project, every twist is as good as any other, and the identity is the one
    that does not invent a turn:
  </p>
''')
parts.append(snippet("engine/src/phys/constraint.cpp — split_swing_twist", function(C, "swing_twist split_swing_twist")))
parts.append('''
  <h3>Step 2: authoring a cone that is not centred on the bone</h3>

  <p>
    A cone-twist joint stores the cone's axis in the parent's frame and the twist axis — the bone — in
    the child's, and they need not agree when the joint is made: a shoulder authored in a T-pose wants
    its cone between "out" and "down", or an arm hanging at rest sits on the edge of it. The one subtle
    line is <code>rest</code>. 8.11's hinge stored <code>conj(q_a)·q_b</code>, which makes the relative
    rotation the identity at the authored pose; here it is pre-multiplied by the conjugate of the swing
    from the cone's axis to the bone, so that the relative rotation <em>is</em> that swing at the
    authored pose — twist zero, swing the tilt — and its swing–twist split about the cone's axis is the
    joint's swing and twist at every pose after. <code>hinge_angle</code> then measures the twist
    unchanged:
  </p>
''')
parts.append(snippet("engine/src/phys/constraint.cpp — make_cone_twist", function(C, "joint make_cone_twist")))
parts.append('''
  <h3>Step 3: the rows</h3>

  <p>
    In <code>prepare_joint</code>, at the head of the ball-socket case and guarded on the kind — the
    hinge falls through into this case for its pin, and a hinge's <code>limit</code> is its own angle.
    §4's swing row and §5's twist rows, through 8.11's <code>one_sided</code> lambda, which is what makes
    speculation, warm starting and the position pass apply to them without a line of solver code:
  </p>
''')
parts.append(snippet("engine/src/phys/constraint.cpp — prepare_joint, the ball-socket's limits (comments removed; the listing has them)",
                     between(C, "if (j.kind == joint_kind::ball_socket && (j.cone.enabled", "        batch.point_error = pb - pa;",
                             dedent=8, strip=True).rsplit("\n", 1)[0].rstrip()))
parts.append('''
  <p>
    <code>k_max_joint_rows</code> did not have to grow: three point rows, one swing row and two twist
    rows is six, under the hinge's eight.
  </p>

  <h3>Step 4: the position pass, fixed</h3>

  <p>
    §7's bug, in the one line that changed. A satisfied one-sided row is given its speculative target
    rather than its zero bias, through a helper that takes the goal explicitly so that no row's state is
    borrowed and restored:
  </p>
''')
parts.append(snippet("engine/src/phys/constraint.cpp — solve_joint_positions, the scalar rows",
                     between(C, "        const bool one_sided = row.lower == 0.0f;", "solve_row_position_to(row, goal", dedent=8)))
parts.append('''
  <h3>Step 5: a moving kinematic body wakes what it touches</h3>

  <p>
    §12's bug, as a stage in front of 8.10's <code>wake_islands</code>. "Moving" is the sleep test's own
    definition turned round, so that a stopped lift wakes nothing:
  </p>
''')
parts.append(snippet("engine/src/phys/solver.cpp — wake_touched_by_kinematic", function(S, "int wake_touched_by_kinematic")))
parts.append('''
  <h3>Step 6: building the ragdoll</h3>

  <p>
    <code>build_ragdoll</code> is four passes. The first maps skeleton joints to parts; the second
    builds each part's capsule, mass, inertia and <code>joint_from_body</code>, places a throwaway body
    at the bind pose in model space, and finds the parent part by walking up the skeleton. The third
    authors the joints on those throwaway bodies — which is legitimate because a joint keeps only
    body-local anchors, axes and a relative rest orientation — and the fourth asks GJK, once per pair,
    which capsules overlap at rest:
  </p>
''')
parts.append(snippet("engine/src/phys/ragdoll.cpp — build_ragdoll, pass 3: the joints",
                     between(R, "    // ---- pass 3: the joints", "    // ---- pass 4", dedent=4).rsplit("\n", 1)[0].rstrip()))
parts.append('''
  <h3>Step 7: steering</h3>

  <p>
    §9's two inverses. The chord for position; for orientation, the Rodrigues vector under the
    linearised rule and the logarithm under the exponential one:
  </p>
''')
parts.append(snippet("engine/src/phys/ragdoll.cpp — steering_angular_velocity and steer",
                     function(R, "vec3 steering_angular_velocity") + "\n\n" + function(R, "void steer(")))
parts.append('''
  <h3>Step 8: the two handoffs</h3>

  <p>
    Which are, as §2 promised, almost nothing. <code>simulate</code> gives the bodies their mass back
    and keeps their velocities; <code>animate</code> takes the mass away and clears the joints' cached
    impulses:
  </p>
''')
parts.append(snippet("engine/src/phys/ragdoll.cpp — simulate", function(R, "void simulate(")))
parts.append('''
  <h3>Step 9: reading the pose back</h3>

  <p>
    A part's joint from its body, a passenger from its parent and its frozen local, and every local
    transform as the parent's inverse times the joint's own. The loop runs parent before child, which
    the skeleton guarantees (7.6), so a parent's placement is always ready when its child reads it:
  </p>
''')
parts.append(snippet("engine/src/phys/ragdoll.cpp — read_pose", function(R, "void read_pose")))
parts.append('''
  <h3>Step 10: the caller's frame</h3>

  <p>
    The ragdoll does not run a narrow phase or own a world — the caller's, as since 8.7 — so the last
    step is the loop a game writes. While the clip owns the character, aim and let the step carry the
    bodies; at the trip, <code>simulate</code>; each step while the solver owns it, add the ragdoll's
    joints between <code>begin</code> and <code>solve</code>; to get up, realign, read the pose back,
    <code>animate</code>, and steer toward a blend. The demo's <code>advance</code> is exactly this:
  </p>
''')
parts.append(snippet("the shape of a frame, from demos/ragdoll/main.cpp",
'''// animated: aim at where the clip will be at the END of this step
part_targets(rd, posed_from_clip(t + h), world_from_model, targets);
steer(rd, world.bodies(), targets, h);

// the trip: the velocities are already right
simulate(rd, world.bodies(), clip_local_pose(t));

// every step, while the solver owns it
solver.begin(world.bodies());
/* ...contacts... */
add_joints(rd, solver);
solver.solve(h, cfg, sleep);

// getting up: put the clip over the pelvis, start from where the bodies lie
world_from_model = realign_model(rd, world.bodies(), tripped_at, clip_hips_in_model_space);
read_pose(rd, skeleton, world.bodies(), world_from_model, start);
animate(rd, world.bodies());
// ...then each step: blend start -> clip with transform_blend_slerp, part_targets, steer'''))
parts.append('''
  <p>
    The narrow phase in the demo and the harness dispatches capsules for the first time — 8.10's and
    8.11's scenes knew spheres and boxes — through a <code>placed</code> struct that holds whichever
    primitive a body has by value, so that <code>as_convex</code>, which is a view and refuses a
    temporary, has an lvalue to look at. That is dispatch, not geometry: <code>collide_manifold</code>
    has taken any convex pair since 8.7. It has now been written in three demos and a harness, which is
    the argument for Module 9's scene layer owning it.
  </p>
''')

with open(os.path.join(ROOT, "scratch/l812_body_e.html"), "w") as fh:
    fh.write("\n".join(parts))
print("wrote scratch/l812_body_e.html")
