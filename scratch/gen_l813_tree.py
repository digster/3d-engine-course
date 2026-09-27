#!/usr/bin/env python3
"""scratch/gen_l813_tree.py — Module 8's annotated project tree, generated ONCE.

CLAUDE.md §8: every module ends with a full annotated project tree. A tree typed
by hand is a list of what the author remembers, so this one is built from the
repository's own file list (`git ls-files`, plus this lesson's new files, which
are not committed yet) and written as a STATIC, escaped block into
scratch/l813_tree.html. build_813.py never runs this script.

Every file is named. Outside `phys/` and `demos/` the files of a directory are
listed compactly, several to a line, with the lesson that opened the directory;
`phys/` and `demos/` — Module 8's work — get a line and a lesson each.
"""
import subprocess

OUT = "scratch/l813_tree.html"

NEW = ["engine/include/engine/phys/cast.hpp", "engine/include/engine/phys/character.hpp",
       "engine/src/phys/cast.cpp", "engine/src/phys/character.cpp", "demos/character/main.cpp"]

DIR_NOTE = {
    "engine/include/engine/anim": "7.5–7.7: skeletons, clips, sampling and blending",
    "engine/include/engine/asset": "5.5: the asset system — images, OBJ, glTF, handles",
    "engine/include/engine/audio": "7.8: devices, sounds, the mixer",
    "engine/include/engine/core": "Modules 1 and 5: clock, input, log, assert, pools, actions",
    "engine/include/engine/ecs": "5.6–5.9: the archetype ECS and the transform hierarchy",
    "engine/include/engine/gfx": "Modules 2–6: the rasterizer, then SDL_GPU and the renderer",
    "engine/include/engine/math": "Modules 1–7: vectors, matrices, bounds, rotations — header-only",
    "engine/include/engine/phys": "Module 8: physics",
    "engine/include/engine/platform": "5.2: the app, the platform, ENGINE_MAIN",
    "engine/include/engine/ui": "5.11: the ImGui debug UI and debug draw",
}

PHYS = {
    "integrate.hpp": "8.1   semi-implicit Euler, and why explicit Euler explodes",
    "rigid_body.hpp": "8.2   bodies, forces, the body_world; 8.10 split the step in two",
    "inertia.hpp": "8.3   the inertia tensor, the parallel-axis theorem",
    "shape.hpp": "8.4   sphere, box, capsule, hull; support and support_face",
    "collide.hpp": "8.4   the Separating Axis Theorem; 8.5 and 8.6 generalised it",
    "convex.hpp": "8.5   a shape as its support function; 8.13 placed_shape",
    "gjk.hpp": "8.5   distance, from a support function",
    "epa.hpp": "8.6   penetration depth",
    "manifold.hpp": "8.7   contact manifolds, feature ids, the persistence cache",
    "broadphase.hpp": "8.8   the uniform grid",
    "solver.hpp": "8.9   impulses; 8.10 sequential impulses, islands, sleep",
    "constraint.hpp": "8.11  Jacobian rows and joints; 8.12 swing-twist",
    "ragdoll.hpp": "8.12  a skeleton handed to the solver and back",
    "cast.hpp": "8.13  a shape cast: conservative advancement",
    "character.hpp": "8.13  the character controller",
}

DEMO = {
    "common": "4.6   shared content (demo_scene, pong)", "pong": "1.8   the Module 1 checkpoint",
    "sandbox": "2.1   every rasterizer demo, Modules 2–4", "hello_cube": "5.1   the smallest program past the boundary",
    "ecs_swarm": "5.8   a world built of components", "collector": "5.12  the Module 5 checkpoint game",
    "gltf_view": "6.6   the asset system's acceptance test", "gimbal": "7.1   three rings and a number",
    "plane": "7.3   the complex plane", "rig": "7.6   one surface, six joints", "audio": "7.8   a listener and three emitters",
    "integrate": "8.1   three rules on one spring", "bodies": "8.2   three masses, three ways to fall",
    "spin": "8.3   a box that flips with nothing touching it", "collide": "8.4   fifteen candidate axes",
    "gjk": "8.5   the Minkowski difference, searched", "epa": "8.6   the balloon in the dark room",
    "manifold": "8.7   one contact is not a contact", "broadphase": "8.8   the cheap question",
    "impulse": "8.9   one contact, resolved", "stack": "8.10  a pile of crates",
    "joints": "8.11  rods, ropes, sockets, hinges", "ragdoll": "8.12  a skeleton handed to the solver",
    "character": "8.13  a capsule that obeys the design",
}


def esc(t):
    return t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def files():
    out = subprocess.run(["git", "ls-files"], capture_output=True, text=True, check=True).stdout.split()
    return sorted(set(out + NEW))


def compact(names, width=64, pad="          "):
    lines, cur = [], ""
    for n in names:
        if cur and len(cur) + 2 + len(n) > width:
            lines.append(cur)
            cur = n
        else:
            cur = n if not cur else cur + "  " + n
    if cur:
        lines.append(cur)
    return [pad + ln for ln in lines]


def main():
    fs = files()
    L = []
    L.append("3d-engine-course/")
    L.append("├── CMakeLists.txt              top level: engine, demos, shaders (0.4, 5.1)")
    L.append("├── cmake/                      " + "  ".join(f.split("/")[-1] for f in fs if f.startswith("cmake/")))
    L.append("├── shaders/                    HLSL, cross-compiled by SDL_shadercross (4.3) — "
             f"{sum(1 for f in fs if f.startswith('shaders/'))} files")
    L.append("├── assets/                     meshes, a texture and a font the demos load")
    L.extend(compact([f.split("/", 1)[1] for f in fs if f.startswith("assets/")], pad="│   "))
    L.append("├── engine/                     the static library (5.1)")
    L.append("│   ├── CMakeLists.txt          sources, warnings, the umbrella lint")
    L.append("│   ├── include/engine/         the public API — the only thing demos may include")
    inc = [f for f in fs if f.startswith("engine/include/engine/")]
    L.append("│   │   ├── engine.hpp          the umbrella: every public header")
    dirs = sorted({"/".join(f.split("/")[:4]) for f in inc if f.count("/") >= 4})
    for d in dirs:
        last = d == dirs[-1]
        head = "│   │   └── " if last else "│   │   ├── "
        name = d.split("/")[-1] + "/"
        members = sorted(f.split("/")[-1] for f in inc if f.startswith(d + "/"))
        L.append(f"{head}{name:<20}{DIR_NOTE.get(d, '')}  ({len(members)})")
        bar = "│   │   │   " if not last else "│   │       "
        if name == "phys/":
            order = list(PHYS)
            for m in order:
                L.append(f"{bar}{m:<19} {PHYS[m]}")
            missing = [m for m in members if m not in PHYS]
            if missing:
                raise SystemExit(f"phys/ header with no annotation: {missing}")
        else:
            L.extend(compact(members, pad=bar))
    L.append("│   └── src/                    the implementations, one directory per include/ one")
    src = [f for f in fs if f.startswith("engine/src/")]
    sdirs = sorted({"/".join(f.split("/")[:3]) for f in src if f.count("/") >= 3})
    for d in sdirs:
        last = d == sdirs[-1]
        head = "│       └── " if last else "│       ├── "
        members = sorted(f.split("/")[-1] for f in src if f.startswith(d + "/"))
        L.append(f"{head}{d.split('/')[-1] + '/':<20}({len(members)})")
        L.extend(compact(members, pad="│       │   " if not last else "│           "))
    L.append("├── demos/                      executables: the public API only (5.1)")
    L.append("│   ├── CMakeLists.txt")
    ddirs = sorted({f.split("/")[1] for f in fs if f.startswith("demos/") and f.count("/") >= 2})
    for i, d in enumerate(ddirs):
        head = "│   └── " if i == len(ddirs) - 1 else "│   ├── "
        L.append(f"{head}{d + '/':<24}{DEMO.get(d, '?')}")
    L.append("├── docs/                       this course: index, conventions, math toolbox, lessons (no build step)")
    L.append("├── scratch/                    gitignored: harnesses, builders, figures, pins")
    L.append("├── memory/                     a summary of each session's work")
    L.append("└── ARCHITECTURE.md  README.md  STATE.md  LEARNINGS.md  PROMPT.md  CLAUDE.md  LICENSE")
    text = "\n".join(L)
    html = ('  <div class="listing">\n'
            '    <figcaption><span class="path">the repository after Module 8</span>\n'
            '      <span class="lang" data-lang="text">tree</span></figcaption>\n'
            f'<pre><code class="lang-text">{esc(text)}</code></pre>\n'
            '  </div>\n')
    with open(OUT, "w") as fh:
        fh.write(html)
    print(f"wrote {OUT}: {len(L)} lines, {len(inc)} public headers, {len(src)} sources, {len(ddirs)} demo dirs")


if __name__ == "__main__":
    main()
