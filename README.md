# Build a Professional 3D Game Engine

A complete course that takes you from "I can program a little" to "I can design, build, and ship
a 3D game engine" — by actually building one, in **C++20** on **SDL3**, lesson by lesson.

There is no engine to download here and no framework doing the interesting parts for you. You
write the math library, the rasterizer, the ECS, the renderer, the physics, and the editor. By
the end you have a real engine and a game built on its public API.

**Status:** curriculum and conventions published; lessons in progress. Start at
[`docs/index.html`](docs/index.html).

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
- Budget roughly **300–500 hours** across ~91 lessons.

**Exit profile:** implement techniques straight from papers, debug GPU work in RenderDoc, reason
about frame budgets and cache behaviour, design and defend engine architecture, and read real
engine codebases without drowning.

---

## Reading the course

The lessons are **self-contained HTML files** — no build step, no server, no npm.

```sh
open docs/index.html          # macOS
xdg-open docs/index.html      # Linux
start docs\index.html         # Windows
```

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

The code in `src/` is the state of the engine as of the most recently published lesson (see
[STATE.md](STATE.md)). Running it today gives you the **Module 1 checkpoint: a complete, playable
game of Pong**, drawn entirely into a CPU framebuffer we own pixel by pixel:

```sh
./build/engine            # macOS / Linux
.\build\Debug\engine.exe  # Windows
```

Left paddle <kbd>W</kbd>/<kbd>S</kbd>, right paddle <kbd>↑</kbd>/<kbd>↓</kbd> (<kbd>C</kbd> swaps
the AI for a second player). Press <kbd>K</kbd> then <kbd>1</kbd> to switch to a naive collision
test at 10 Hz and watch the ball tunnel straight through a paddle — that failure, and why a
60 Hz machine can never show it to you, is what
[Lesson 1.8](docs/lessons/01-08-pong.html) is about.

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
**SDL_shadercross** (introduced in Module 4).

---

## What gets built

By the final module the engine has: a documented public C++ API; an SDL_GPU forward **PBR**
renderer with shadow-mapped lights, HDR, tonemapping and a post-processing stack; skybox and
image-based lighting; an asset pipeline (images, OBJ, glTF); a handle-based resource system; a
from-scratch **ECS** with transform hierarchy; skeletal animation; rigid-body collision and
response; 3D audio; input mapping; an **ImGui editor** with hierarchy, inspector and gizmos;
profiling hooks; serialization and a scene format; hot reload; a job system; and a **capstone
game built solely against the public API**.

Hand-rolled on purpose: the math library (no GLM), rasterizer, OBJ parser, ECS, renderer, asset
system, allocators, and collision/rigid-body basics.

Third-party, each with an explicit "why we don't hand-roll this" justification: `stb_image`,
`stb_truetype`, Dear ImGui (tooling only — never gameplay UI), `cgltf`, SDL_shadercross.

---

## Repository layout

See [ARCHITECTURE.md](ARCHITECTURE.md) for the full tree and the reasoning behind it. The short
version: Modules 0–4 build a single, library-shaped executable; **Module 5 opens with a refactor
arc** that splits the tree into `engine/` (static library, public headers under
`engine/include/engine/`), `demos/`, and later `tools/`. From that point the boundary is law —
demos and the capstone may only use the public API.

---

## Project documents

| File | Purpose |
|---|---|
| [CLAUDE.md](CLAUDE.md) | The master prompt — the binding specification for this course |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Repository layout, engine architecture, and the *why* |
| [LEARNINGS.md](LEARNINGS.md) | Verified SDL3/SDL_GPU facts and hard-won gotchas |
| [PROMPT.md](PROMPT.md) | Prompt log |
| `memory/` | Dated session summaries |

## License

MIT — see [LICENSE](LICENSE). Copyright (c) 2026 digster.
