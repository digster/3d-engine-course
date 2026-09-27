#!/usr/bin/env python3
"""scratch/gen_l813_body_e.py — cut §13's snippets out of the sources, ONCE.

8.12's pattern: the prose of "Writing it" quotes functions line for line, and a
snippet typed by hand drifts from the listing it quotes. So each snippet is cut
from the source file here, escaped exactly as the builder escapes a listing, and
written into the STATIC fragment scratch/l813_body_e.html. build_813.py reads the
fragment and never this script, so the page does not depend on live sources.

Run it again only if a quoted function changes before the lesson ships, and then
re-pin the listings too (they are the same code).
"""
import re

OUT = "scratch/l813_body_e.html"


def esc(text):
    return (text.replace("&", "&amp;")
                .replace("<", "&lt;")
                .replace(">", "&gt;")
                .replace('"', "&quot;")
                .replace("'", "&#x27;"))


def cut(path, start, indent=""):
    """From the first line that starts with `start` to the matching closing
    brace at the same indentation, inclusive. Leading doc-comment lines directly
    above the signature are NOT included: the prose says what they say."""
    lines = open(path).read().split("\n")
    for i, ln in enumerate(lines):
        if ln.startswith(start):
            break
    else:
        raise SystemExit(f"{path}: no line starts with {start!r}")
    out = []
    for ln in lines[i:]:
        out.append(ln[len(indent):] if ln.startswith(indent) else ln)
        if ln == indent + "}" or ln == indent + "};":
            break
    else:
        raise SystemExit(f"{path}: no closing brace after {start!r}")
    return "\n".join(out)


def snippet(path, label, code):
    return (
        '  <div class="listing">\n'
        f'    <figcaption><span class="path">{path} — {label}</span>\n'
        '      <span class="lang" data-lang="cpp">C++</span></figcaption>\n'
        f'<pre><code class="lang-cpp">{esc(code)}</code></pre>\n'
        '  </div>\n'
    )


CAST = "engine/src/phys/cast.cpp"
CHAR = "engine/src/phys/character.cpp"
CONVEX = "engine/include/engine/phys/convex.hpp"
DEMO = "demos/character/main.cpp"


def main():
    parts = []
    P = parts.append

    P('''  <h2 id="implementation">13. Writing it</h2>

  <p>
    Two new pairs of files and one small change. <code>phys/cast.hpp</code> is the query. Like
    <code>gjk.cpp</code> it knows nothing about bodies or shapes, only about <code>convex</code>, because a
    projectile, a camera boom or a line of sight needs a cast as much as a character does.
    <code>phys/character.hpp</code> is the policy. It reaches down to <code>rigid_body.hpp</code> for where
    the obstacles are and how a platform moved, and to <code>collide.hpp</code> once, for 8.6's EPA. It does
    <em>not</em> include <code>solver.hpp</code>: the controller is outside the simulation on purpose, and
    the only thing it hands the solver is a proxy's velocity. <code>convex.hpp</code> gains a small
    struct, and that is a debt being paid, not a feature.
  </p>

  <h3>Step 1: the cast</h3>

  <p>
    §3's loop, and every line is one of four questions: how far apart are the two shapes now (GJK), how
    fast is the motion closing that gap (a dot product), how far can the mover go before the gap is the
    skin (a division), and has it arrived (a comparison). The mover is moved by changing one vector,
    <code>origin</code>, because 8.5 made every support function answer <em>relative</em> to it. The line
    that matters most is the one that takes the gap from <code>certify(...).lower</code> and not from
    <code>g.distance</code>:
  </p>

''')
    P(snippet(CAST, "cast", cut(CAST, "cast_result cast(")))
    P('''
  <p>
    Note the two ways out besides a hit. If the motion is not closing the gap (<code>approach</code> at or
    below <code>min_approach·|d|</code>), the slab will never narrow and the path is clear, which is a proof
    and not a guess. If GJK finds the shapes intersecting after the first step, the step landed on the
    contact, and the cast returns the last iterate it could still measure. §3's skin measurement is the
    case for never getting there.
  </p>

  <h3>Step 2: a placed shape, by value</h3>

  <p>
    Every scene since 8.10 has needed to turn "body <code>i</code>'s shape at body <code>i</code>'s pose" into a
    <code>convex</code>. <code>as_convex</code> needs an <em>lvalue</em> of the placed primitive to point at,
    so each of them wrote the same small struct: a sphere, a box and a capsule held by value, and a switch
    to fill the right one. That makes five copies, in three demos and two harnesses, each named as a debt
    by the lesson that wrote it. The controller is the first code <em>inside</em> the engine that needs it,
    and a sixth copy there would be the first a student could not see was a copy. So it moves into
    <code>convex.hpp</code>, with the deleted rvalue overload that makes <code>place(...).view()</code>, a
    view of a temporary, a compile error:
  </p>

''')
    P(snippet(CONVEX, "placed_shape", cut(CONVEX, "struct placed_shape")))
    P('''
  <h3>Step 3: the one query</h3>

  <p>
    A sweep is §3's rule made into a function. Test the move's swept bounds against every body, cast
    against each survivor <em>separately</em>, and keep the smallest <code>t</code>. The <code>solid</code>
    filter leaves out pushable bodies, which is how the sideways pass walks into a crate and lets the proxy
    move it. An obstacle the capsule already overlaps is skipped, because overlap is <code>recover</code>'s
    job, and a sweep that refused to move because of it would pin the character inside whatever pushed
    into it:
  </p>

''')
    P(snippet(CHAR, "sweep", cut(CHAR, "character_hit sweep(")))
    P('''
  <h3>Step 4: what the rest of a move is clipped from</h3>

  <p>
    Quake's rule is <code>clip_to_planes</code>: one plane at a time, then each pair's crease, then stop.
    Its one subtle line is the slack. A vector clipped exactly onto plane <code>i</code> reads a hair either
    side of it after rounding, and without the slack every corner would be a stop. <code>next_move</code> is
    §4's finding, with the other two rules kept so that §C can measure them. The default clips the
    <em>original</em> motion, scaled to the time left:
  </p>

''')
    P(snippet(CHAR, "next_move", cut(CHAR, "[[nodiscard]] vec3 next_move(")))
    P(snippet(CHAR, "clip_to_planes", cut(CHAR, "vec3 clip_to_planes(")))
    P('''
  <h3>Step 5: the sideways pass</h3>

  <p>
    The pass carries three things through its loop: the <code>intent</code> (the whole step's motion, laid
    along whatever ground it is on), the <code>time_left</code>, and the list of walls. A walkable hit
    re-lays the intent along the new ground and carries on. A wall gets one step-up attempt, then a plane
    in the list with its normal flattened. The clipped result is laid back along the ground, because the
    clip turned it:
  </p>

''')
    P(snippet(CHAR, "sideways", cut(CHAR, "void sideways(")))
    P('''
  <h3>Step 6: the step-up</h3>

  <p>
    Three sweeps and four ways to refuse: no headroom, still a wall at the top, no floor to land on, or a
    landing that is not higher than the start. The across move is the larger of what was left and §8's
    <code>step_forward_min</code>, called with the radius the casts see:
  </p>

''')
    P(snippet(CHAR, "try_step_up", cut(CHAR, "bool try_step_up(")))
    P('''
  <h3>Step 7: the vertical pass, and the snap</h3>

  <p>
    Rising, one sweep; the first thing overhead is a ceiling, and the game zeroes its upward velocity when
    <code>hit_ceiling</code> says so. Falling, land on the first walkable surface, or slide along a steep
    one. <code>stop_on_ground</code> is §5's creep, kept as a knob. The snap is §7: roll off an edge the
    character is resting on, by exactly what clears it, then cast straight down and keep the result only
    if it is floor:
  </p>

''')
    P(snippet(CHAR, "vertical", cut(CHAR, "void vertical(")))
    P(snippet(CHAR, "snap_down", cut(CHAR, "void snap_down(")))
    P('''
  <h3>Step 8: recovering from what moved in</h3>

  <p>
    Overlap arrives from outside the controller: a door closes on the character, or a platform rises
    faster than the carry. It is 8.6's question, not a cast's. <code>recover</code> pushes out of anything
    fixed or kinematic that overlaps, by EPA's minimum translation plus the skin, and back to a full skin
    from anything nearer than half of one. The <code>dynamic</code> line is §11's refusal, one line long:
  </p>

''')
    P(snippet(CHAR, "recover", cut(CHAR, "int recover(")))
    P('''
  <h3>Step 9: what a platform just did</h3>

  <p>
    <code>advance_orientation(q)</code> is <code>Δ·q</code> for both spin rules. For the linearised one that is
    because normalising <code>(1&nbsp;+&nbsp;½hω)·q</code> is normalising <code>(1&nbsp;+&nbsp;½hω)</code> when
    <code>q</code> is unit. So the step's rotation is what the step does to the identity: one line, and
    exactly the integrator's:
  </p>

''')
    P(snippet(CHAR, "step_motion", cut(CHAR, "body_step step_motion(")))
    P(snippet(CHAR, "carry_by_transform", cut(CHAR, "vec3 carry_by_transform(")))
    P('''
  <h3>Step 10: the move, in order</h3>

  <p>
    Five stages, each there because a section needed it: carry (§10), recover (§11), sideways (§4, §5,
    §8), vertical (§5), and then the ground, probed and snapped (§6, §7). Two details are worth reading
    slowly. The sideways pass is only <em>grounded</em> if the character was grounded and is not rising,
    so a jump does not follow the floor. And <code>c.velocity</code> is <code>(end&nbsp;−&nbsp;start)/h</code>,
    what the move <em>achieved</em>, never what was asked for. It is the velocity to show a camera, an
    animation blend, or the proxy:
  </p>

''')
    P(snippet(CHAR, "move_character", cut(CHAR, "move_report move_character(")))
    P('''
  <h3>Step 11: the frame, from outside</h3>

  <p>
    The demo's fixed step is §2's three parts in order: steer the proxies (or teleport them, on
    <kbd>P</kbd>), run 8.10's physics step unchanged, then move each character against the world the step
    left. The game's own rules are the few lines around <code>move_character</code>. A grounded character
    does not fall, a jump is <code>jump_speed(g,&nbsp;H)</code>, and a landing or a ceiling zeroes the
    vertical velocity:
  </p>

''')
    P(snippet(DEMO, "advance", cut(DEMO, "    void advance(float h)", indent="    ")))
    P('''
  <p>
    Nothing in that function knows about casts, slopes or steps, and nothing in <code>character.cpp</code>
    knows about sticks, jumps or the solver. That is the boundary §2 drew, and it is where Module 9's editor
    will put a gizmo and its scene format will put a save file.
  </p>
''')
    with open(OUT, "w") as fh:
        fh.write("".join(parts))
    print(f"wrote {OUT}")


if __name__ == "__main__":
    main()
