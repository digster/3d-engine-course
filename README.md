# Build a Professional 3D Game Engine

A complete course that takes you from "I can program a little" to "I can design, build, and ship
a 3D game engine" — by actually building one, in **C++20** on **SDL3**, lesson by lesson.

There is no engine to download here and no framework doing the interesting parts for you. You
write the math library, the rasterizer, the ECS, the renderer, the physics, and the editor. By
the end you have a real engine and a game built on its public API.

**Status:** 96 of 113 lessons published. **Modules 0–5 are complete** — orientation and toolchain,
the loop and the pixel, two modules of software rasterizer, SDL_GPU, and the refactor into an
engine with an ECS — and Modules 6–8 (advanced rendering; rotation, animation and audio; physics)
are complete except for four lessons inserted after the post-Module 8 review: local lights (6.17b),
compute shaders (6.18b), glTF skins and clips (7.7b) and scene queries (8.14). Those come next,
then **Module 9 — Professional Polish & Capstone**. The [course index](docs/index.html) has the module
map, every lesson and its hours; what each lesson added to the engine, and the keys and flags
that show it, is in [CHANGELOG.md](CHANGELOG.md).

---

## The two-stage spine

The course's central pedagogical bet is that you learn the graphics pipeline twice:

1. **Modules 1–3 — a CPU software rasterizer.** You own every pixel. Triangles, the z-buffer,
   perspective-correct interpolation, clipping, and texturing are all code *you* wrote, so the
   pipeline becomes intuitive instead of incantational.
2. **Module 4 onward — SDL_GPU.** SDL3's modern cross-platform GPU API (Vulkan / D3D12 / Metal).
   Every concept maps back to the software counterpart you already built.

The software rasterizer deliberately targets **the same NDC as SDL_GPU** (+Y up, depth 0..1), so
moving to the GPU is an *API change, not a math change*.

---

## Who this is for

- You can program in **some** language. C++ specifics (RAII, ownership, move semantics,
  templates, `const`-correctness, translation units) are taught in place, the first time each
  appears.
- **No assumed background** in graphics, linear algebra, or calculus. All math is built from
  zero, geometrically — intuition first, then derivation, then formula, then code.
- Budget roughly **700–850 hours** across ~113 lessons. The figures are measured from each lesson's
  reading, the code its commit adds, and its exercises — [`docs/_template/estimate-hours.py`](docs/_template/estimate-hours.py) says how.

**Exit profile:** implement techniques straight from papers, debug GPU work in RenderDoc, reason
about frame budgets and cache behaviour, design and defend engine architecture, and read real
engine codebases without drowning.

---

## Reading the course

The lessons are **plain HTML files** — no build step, no server, no npm. Open the index and
navigate:

```sh
open docs/index.html          # macOS
xdg-open docs/index.html      # Linux
start docs\index.html         # Windows
```

Styling and page behaviour come from two shared files, `docs/shared/course.css` and
`docs/shared/course.js`, which every page links. They resolve straight off the filesystem, so
this works offline with no server — but it does mean **a lesson file is only readable inside the
`docs/` tree**. Copy one out on its own and it renders unstyled; keep the folder together, or
just clone the repository.

Three living pages sit alongside the lessons and are updated at every module boundary:

| Page | What it is |
|---|---|
| [`docs/index.html`](docs/index.html) | Course home — module map, every lesson, progress |
| [`docs/conventions.html`](docs/conventions.html) | Handedness, matrices, NDC/depth, winding. **Read before Module 2.** |
| [`docs/math-toolbox.html`](docs/math-toolbox.html) | Cumulative math appendix, grows as you go |
| [`docs/cpp-style.html`](docs/cpp-style.html) | The C++ style guide the codebase obeys |

> The "no build step" rule applies to the **tutorial HTML only**. The C++ obviously builds with
> CMake — see below.

---

## Building the code

The engine itself is a normal CMake project. You write the first `CMakeLists.txt` yourself in
**Module 0**, because CMake is taught from zero rather than handed over as a magic file — so if
you are following along from the start, ignore the one in this repository until Lesson 0.4 asks
you to write it.

Once you are past Module 0, the build is the standard incantation everywhere:

```sh
cmake -S . -B build
cmake --build build
```

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

Every program lands in `build/demos/` (with MSVC, `build\demos\Debug\`). All of them except
`pong` take `--shot FILE`, which renders one deterministic frame to a PPM with no window and no
display; that is how the course's characterization tests run them.

```sh
./build/demos/sandbox                  # macOS / Linux
.\build\demos\Debug\sandbox.exe        # Windows
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

---

---

## What gets built

By the final module the engine has: a documented public C++ API; an SDL_GPU forward **PBR**
renderer with shadow-mapped directional, point and spot lights, HDR, tonemapping and a
post-processing stack; skybox and image-based lighting; a compute-shader path (GPU particles); an
asset pipeline (images, OBJ, glTF — including skinned, animated characters); a handle-based
resource system; a from-scratch **ECS** with transform hierarchy; skeletal animation; a
**rigid-body physics engine** (angular dynamics, GJK/EPA, persistent manifolds, a warm-started
sequential-impulse solver, joints and ragdolls, scene queries and a static mesh collider) and a
**character controller**; 3D audio; symplectic integration; input mapping; an **ImGui editor**
with hierarchy, inspector and gizmos; profiling hooks; serialization and a scene format; hot
reload; a job system; and a **capstone game built solely against the public API**.

Hand-rolled on purpose: the math library (no GLM), rasterizer, OBJ parser, ECS, renderer, asset
system, allocators, and the whole physics engine — collision, the solver, joints, ragdolls
and the character controller (Module 8: no Bullet, PhysX or Jolt).

Third-party, each with an explicit "why we don't hand-roll this" justification: `stb_image`,
`stb_truetype` (6.18 — the decode and the outline rasterization; the atlas packer, the layout
and the compositing are ours, because those three *are* the subject), Dear ImGui (tooling only
— never gameplay UI), `cgltf`, SDL_shadercross. One committed asset that is not generated:
`assets/fonts/Karla-Regular.ttf`, 16.8 kB, SIL Open Font License 1.1, provenance and licence in
`assets/fonts/OFL.txt`.

---

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

`engine/include/engine/ecs/pool.hpp` declares `engine::ecs::pool<T>`, which is **not**
`engine::pool<T>` from `core/pool.hpp`. Same three arrays, opposite jobs: the core one mints its
own keys, the ECS one is keyed by an id it did not mint. The namespace keeps them apart, and the
header comments say so at both ends.

`engine/include/engine/platform/main.hpp` is the one public header **not** reachable through the
`engine.hpp` umbrella, and the omission is deliberate: including it defines a program's entry
point, so it belongs in exactly one `.cpp` per program. An umbrella whose promise is "include
everything, it is harmless" must not be a way to acquire a `main()` by accident.

**The boundary is law, and it is enforced by the include path rather than by discipline.**
`target_include_directories(engine PUBLIC include PRIVATE src)` means a demo writing
`#include "gfx/raster.hpp"` — the spelling every file in this repository used through Module 4 —
does not compile. Modules 0–4 built a single, library-shaped executable; the shape was already
right, which is why 57 files moved without one of them changing.

---

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

## License

MIT — see [LICENSE](LICENSE). Copyright (c) 2026 digster.
