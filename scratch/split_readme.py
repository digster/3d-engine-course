#!/usr/bin/env python3
"""Move README.md's per-lesson notes, verbatim, into CHANGELOG.md.

One-off, kept for provenance (2026-09-27), the third of the process-doc splits
(see split_state.py and split_learnings.py). README.md had grown to 130 KB, most
of it two things a student opening the repository does not need first: a
"newest lesson" narrative that had been prepended at every lesson (one paragraph
of 46 KB, still announcing 8.3 as the newest when 8.13 was), and a tour of every
demo key and flag, lesson by lesson, inside "Building the code".

Both move to CHANGELOG.md unchanged. README.md keeps its introduction, the
spine, the audience, how to read, how to build (now with the per-platform
prerequisites beside the commands), a table of the 23 demos, what gets built,
the layout and the documents - with a short, current status line and an
up-to-date layout written fresh.

    python3 scratch/split_readme.py            # dry run
    python3 scratch/split_readme.py --apply

The source is pinned to the last commit before the split.
"""
from __future__ import annotations

import collections
import os
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REPO)
SOURCE_COMMIT = "0663233"
SRC = subprocess.run(["git", "show", f"{SOURCE_COMMIT}:README.md"], capture_output=True,
                     text=True, check=True).stdout.split("\n")


def find(prefix: str, start: int = 0) -> int:
    """0-based index of the first line at or after `start` that begins with `prefix`."""
    return next(i for i in range(start, len(SRC)) if SRC[i].startswith(prefix))


# Landmarks, found by content rather than line number so a mismatch fails loudly.
STATUS = find("**Status:**")
SPINE_RULE = find("## The two-stage spine") - 2          # the `---` above it
NOTES = find("The code is the state of the engine as of")
PREREQ = find("### Prerequisites")
WHAT = find("## What gets built") - 2
LAYOUT = find("## Repository layout")
DOCS = find("## Project documents")
LICENSE = find("## License")

MOVED = [(STATUS, SPINE_RULE), (NOTES, PREREQ)]

STATUS_NEW = """\
**Status:** 96 of 107 lessons published — **Modules 0–8 are complete**: orientation and
toolchain, the loop and the pixel, two modules of software rasterizer, SDL_GPU, the refactor into
an engine with an ECS, advanced rendering, rotation and animation and audio, and physics. **Module
9 — Professional Polish & Capstone** is next. The [course index](docs/index.html) has the module
map, every lesson and its hours; what each lesson added to the engine, and the keys and flags
that show it, is in [CHANGELOG.md](CHANGELOG.md).
"""

BUILD_NEW = """\
The code is the engine as of the most recently published lesson ([STATE.md](STATE.md) says
which). Since **Lesson 5.1** it is split in two: `engine/` is a static library whose public
headers live under `engine/include/engine/`, and `demos/` holds the programs built on it.

### Prerequisites

| Platform | Needs |
|---|---|
| **Windows** | Visual Studio 2022 (MSVC v143) with the C++ workload, CMake ≥ 3.24 |
| **Linux** | GCC ≥ 12 or Clang ≥ 15, CMake ≥ 3.24, plus SDL3's build deps (X11/Wayland dev packages) |
| **macOS** | Xcode Command Line Tools (Clang ≥ 15), CMake ≥ 3.24 |

**SDL3 is fetched automatically** by CMake via `FetchContent` at a pinned tag — you do not
install it yourself, and a fresh clone builds with no extra setup. The first configure takes a
few minutes while SDL3 compiles; after that it is cached.

Shaders are authored in **HLSL** and cross-compiled to SPIR-V / DXIL / MSL with
**SDL_shadercross** (introduced in Module 4, Lesson 4.3). Without it the C++ still builds —
`cmake/Shaders.cmake` warns and skips the shaders — but the GPU paths have nothing to load.

Every push is built by CI on four toolchains — Ubuntu with GCC and with Clang, Windows with
MSVC, macOS with Apple Clang — see [`.github/workflows/ci.yml`](.github/workflows/ci.yml).

### Running the demos

Every program lands in `build/demos/` (with MSVC, `build\\demos\\Debug\\`). All of them except
`pong` take `--shot FILE`, which renders one deterministic frame to a PPM with no window and no
display; that is how the course's characterization tests run them.

```sh
./build/demos/sandbox                  # macOS / Linux
.\\build\\demos\\Debug\\sandbox.exe        # Windows
```

| Program | Lessons | What it shows |
|---|---|---|
| `sandbox` | 2.1–4.9 | Every early demo on <kbd>Tab</kbd>; Module 3's scene on the GPU, `--software` for the CPU renderer beside it, `--probe` for Module 4's instrument |
| `pong` | 1.8, 5.2 | Module 1's game, as an `engine::app` with no `main()` |
| `hello_cube` | 5.1, 6.18 | The public-API acceptance test: a lit cube drawn from outside the library, with a text overlay |
| `ecs_swarm` | 5.8–5.11 | The ECS, the transform hierarchy, input actions, debug drawing and ImGui panels |
| `collector` | 5.12 | The checkpoint game, written only against the public headers |
| `gltf_view` | 6.6–6.15 | Module 6's renderer: glTF, normal maps, shadows, mipmaps, transparency, HDR, bloom, AA, IBL — one flag each |
| `gimbal` | 7.1–7.5 | Euler angles, axis-angle, quaternions and slerp on one aircraft |
| `plane` | 7.3 | Complex numbers rotating the plane |
| `rig` | 7.6–7.7 | A skinned tube: skinning, then clip sampling and blending |
| `audio` | 7.8 | A listener, three emitters and a map: mixing and 3D spatialization |
| `integrate` | 8.1 | Three integrators on one spring, in phase space |
| `bodies` | 8.2 | Forces, gravity and linear rigid bodies |
| `spin` | 8.3 | A torque-free box flipping about its middle axis |
| `collide` | 8.4 | The Separating Axis Theorem's fifteen axes, live |
| `gjk` | 8.5 | GJK searching the Minkowski difference |
| `epa` | 8.6 | EPA expanding a polytope to the penetration depth |
| `manifold` | 8.7 | Contact manifolds and their persistence |
| `broadphase` | 8.8 | A uniform grid finding candidate pairs |
| `impulse` | 8.9 | One contact, resolved with restitution and friction |
| `stack` | 8.10 | A crate stack: warm starting, islands and sleeping |
| `joints` | 8.11 | Rods, ropes, sockets and hinges in the contact loop |
| `ragdoll` | 8.12 | A skeleton handed to the solver and back |
| `character` | 8.13 | A capsule character controller: grounding, slopes, steps |

Each lesson's page has the exact commands and what you should see; [CHANGELOG.md](CHANGELOG.md)
collects every key and flag in one place.
"""

LAYOUT_NEW = """\
## Repository layout

See [ARCHITECTURE.md](ARCHITECTURE.md) for the full tree and the reasoning behind it. The short
version, as of **Lesson 8.13**:

```
engine/include/engine/   the public API — 105 headers, and the only path a demo can name
  core/                  clock, input, fixed_step, profile, log, assert, handle, pool, bench, actions
  math/                  vec2–4, mat2–4, transform, euler, axis_angle, complex, quat, rotation, bounds
  gfx/                   both renderers: the software rasterizer and the SDL_GPU scene, materials,
                         shadows, mipmaps, HDR, bloom, AA, IBL, culling, the frame graph, text
  asset/                 search_path, asset_store — names, roots, lifetimes (5.5)
  ecs/                   entity, pool, registry, view, hierarchy, camera (5.8–5.9)
  anim/                  skeleton, skin, clip (7.6–7.7)
  audio/                 sound, mixer, spatial (7.8)
  phys/                  integrate through character: bodies, collision, GJK/EPA, manifolds,
                         broadphase, the solver, joints, ragdolls, casts (Module 8)
  platform/              how a program starts: platform.hpp, app.hpp, main.hpp
  ui/                    debug_ui — the Dear ImGui lifecycle. TOOLING ONLY (5.11)
engine/src/              private implementation; stb_image, stb_truetype, cgltf
                         and <imgui.h> stop here — one translation unit each
shaders/                 HLSL, compiled by SDL_shadercross at build time
demos/                   23 programs on the public API (table above); common/ is shared content
docs/                    the course: index.html, lessons/, conventions, math toolbox, style guide
docs/_template/          the lesson template, authoring guide and the checkers
scratch/                 the pages' authoring sources: builders, fragments, figures, pins
tools/                   not yet: the editor and asset cooker arrive in Module 9
```
"""

DOCS_NEW = """\
## Project documents

| File | Purpose |
|---|---|
| [CLAUDE.md](CLAUDE.md) | The master prompt — the binding specification for this course |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Repository layout, engine architecture, and the *why* |
| [CHANGELOG.md](CHANGELOG.md) | What each lesson added, and every demo key and flag |
| [STATE.md](STATE.md) | The resume key: conventions, completed lessons, capabilities, `next` — full text in [`state/`](state/) |
| [LEARNINGS.md](LEARNINGS.md) | The hazards that recur, and an index of every verified fact and gotcha in [`learnings/`](learnings/) |
| [docs/_template/README.md](docs/_template/README.md) | How a lesson page is authored, built and checked |
| [PROMPT.md](PROMPT.md) | Prompt log |
| `memory/` | Dated session summaries |
"""

CHANGELOG_HEAD = """\
# Changelog — the engine, lesson by lesson

Moved verbatim from README.md on 2026-09-27, when the README became a short front door. Two
parts, in the order they stood there:

1. **The newest-lesson notes** — the README's opening, as it stood: a status line that stopped
   being updated at 8.3, the notes for 8.3 and 8.1, and one line that grew by a paragraph per
   lesson from 8.4 to 8.13. The demo tour covers 1.8 to 6.8 and touches 7.1 and 7.4;
   the other lessons of 6.9–7.8 never had README notes — their pages are the record.
2. **The demo tour** — every program, key and flag, and what each one shows, as the lessons
   added them.

New lessons append their entry to the relevant part; the README keeps only the current status.

---

## Newest-lesson notes
"""


def compose() -> tuple[list[str], list[str]]:
    readme = (SRC[:STATUS] + STATUS_NEW.split("\n") + SRC[SPINE_RULE:NOTES]
              + BUILD_NEW.split("\n") + ["---", ""] + SRC[WHAT:LAYOUT]
              + LAYOUT_NEW.split("\n") + SRC[find("`engine/include/engine/ecs/pool.hpp` declares", LAYOUT):DOCS]
              + DOCS_NEW.split("\n") + SRC[LICENSE:])
    changelog = (CHANGELOG_HEAD.split("\n") + SRC[STATUS:SPINE_RULE]
                 + ["", "---", "", "## The demo tour", ""] + SRC[NOTES:PREREQ])
    while changelog[-1] == "":
        changelog.pop()
    return readme, changelog + [""]


def main() -> int:
    readme, changelog = compose()
    # Coverage: every non-blank line of the two moved ranges is in CHANGELOG.md, and every
    # other original line is still in README.md, except the four blocks rewritten above
    # (status, build prose, layout tree, documents table), which are reported for review.
    moved = {i for a, b in MOVED for i in range(a, b)}
    have_c = collections.Counter(changelog)
    have_r = collections.Counter(readme)
    lost_moved = [i for i in moved if SRC[i].strip() and have_c[SRC[i]] == 0]
    dropped = [i for i in range(len(SRC)) if i not in moved and SRC[i].strip()
               and have_r[SRC[i]] == 0]
    size = lambda L: sum(len(l) + 1 for l in L) / 1024
    print(f"README.md: {size(SRC):.0f} KB -> {size(readme):.1f} KB; "
          f"CHANGELOG.md {size(changelog):.1f} KB; moved lines lost: {len(lost_moved)}")
    print(f"  kept-range lines replaced by rewritten blocks: {len(dropped)}")
    for i in dropped:
        print(f"    {i + 1:5d}  {SRC[i][:96]}")
    if "--apply" in sys.argv and not lost_moved:
        open("README.md", "w", encoding="utf-8").write("\n".join(readme))
        open("CHANGELOG.md", "w", encoding="utf-8").write("\n".join(changelog))
        print("written")
    return 1 if lost_moved else 0


if __name__ == "__main__":
    sys.exit(main())
