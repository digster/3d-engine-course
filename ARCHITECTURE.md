# Architecture

This document describes the **big picture**: how the repository is shaped, why it changes shape
at Module 5, how the engine's subsystems relate, and the structural decisions that are expensive
to reverse. It is the map you want before reading multiple files at once.

For per-lesson detail, read the lessons. For verified SDL facts, read [LEARNINGS.md](LEARNINGS.md).
For the binding spec, read [CLAUDE.md](CLAUDE.md).

---

## 1. Two products in one repository

This repo contains **a course** and **an engine**, and they evolve together:

| Stream | Lives in | Build step? |
|---|---|---|
| Course — self-contained HTML lessons | `docs/` | **None, ever.** Open the file. |
| Engine — an always-compiling C++20 codebase | `src/` → later `engine/`, `demos/`, `tools/` | CMake ≥ 3.24 |

The no-build-step rule is a hard constraint on the *tutorial content only*. A lesson page must
render from a bare filesystem with no network — a CDN is allowed for math typesetting, but every
crucial equation stays legible from the prose if the CDN is unreachable.

**The codebase is a single evolving tree, not per-lesson snapshots.** `HEAD` is always the state
at the end of the most recently published lesson, and it always compiles. Each lesson's own
*Code Listings* section carries the full text of every file it touched, so the lesson is the
historical record; git history is the other half. This is why continuity errors are treated as
correctness bugs — there is no snapshot to fall back on.

---

## 2. Repository layout

### 2.1 Now (Modules 0–4): one library-shaped executable

Deliberately **not** a framework yet. The internal structure is already drawn along the seams
the Module 5 refactor will cut, so that refactor feels like *revealing* a boundary rather than
inventing one — but without paying framework ceremony before it buys anything.

```
3d-engine-course/
├── CLAUDE.md               # master prompt — the binding spec
├── ARCHITECTURE.md         # this file
├── README.md
├── LEARNINGS.md            # verified SDL facts, gotchas
├── PROMPT.md               # prompt log
├── LICENSE                 # MIT, digster
├── CMakeLists.txt          # root build — written by the student in Module 0
├── cmake/                  # helper modules (warnings, shader compilation)
├── memory/                 # dated session summaries
├── docs/                   # THE COURSE (see §3)
├── src/                    # the engine-to-be, single executable  [EXISTS from 0.5]
│   ├── main.cpp            # entry point + frame loop            [EXISTS]
│   ├── core/               # input, logging, assertions, time, error handling
│   │   ├── input.hpp       # frame-coherent keyboard/mouse snapshot  [EXISTS from 1.2]
│   │   └── input.cpp
│   ├── math/               # vec2/3/4, mat3/4, quaternion — hand-rolled, no GLM
│   ├── gfx/                # framebuffer, software rasterizer → later SDL_GPU renderer
│   └── platform/           # window + event pumping (Module 5; see the note below)
├── shaders/                # HLSL sources (Module 4+). Compiled output is gitignored.
├── assets/                 # meshes, textures, fonts
├── tests/                  # unit tests (math first — it is the most testable layer)
└── third_party/            # stb, ImGui, cgltf. SDL3 arrives via FetchContent.
```

> **Directories appear only when a lesson needs them.** The tree above is the *target*; entries
> marked `[EXISTS]` are the only ones on disk. Module 0 has the student write the first
> `CMakeLists.txt` themselves, so pre-creating any of this would spoil a lesson.

**Include root.** `target_include_directories(engine PRIVATE src)` makes `src/` the root for our
own headers, so every file spells an include the same way — `#include "core/input.hpp"` — no
matter where it sits. Relative paths (`../core/input.hpp`) break when a file moves; a stable
include root does not. Sources are **listed explicitly** in `add_executable`, never `file(GLOB)`:
a glob is evaluated at configure time, so a newly added file is silently absent from the build
until someone reconfigures, and the symptom is a link error naming a function you are looking
straight at.

**Why input lives in `core/` and not `platform/`.** Input is the first subsystem the course
extracts (Lesson 1.2), and at that point there is no platform layer to put it in — `main.cpp`
still calls SDL directly, and the platform/application abstraction is not taught until Module 5.
Creating `src/platform/` for a single class, before the concept that justifies it, is exactly the
premature ceremony §2.1 exists to avoid. `core::input` is also not a device driver: it is a
frame-coherent *state cache* (poll levels, derive edges) that gameplay queries, which is
engine-fundamental rather than OS-specific. When Module 5 introduces the platform layer, the
raw device/event plumbing may move to `platform/` while the cached snapshot stays in `core/` —
and that split will be a taught decision, not an accident.

### 2.2 After the Module 5 refactor: engine / demos / tools

Module 5 opens with a dedicated refactor arc, taught as a **first-class architecture lesson**
(what makes a good public API, physical design, dependency direction) — not rushed through as a
chore.

```
├── engine/
│   ├── include/engine/     # THE PUBLIC API. Everything else is private.
│   │   ├── core/
│   │   ├── math/
│   │   ├── gfx/
│   │   ├── scene/
│   │   └── engine.hpp      # umbrella header
│   ├── src/                # private implementation + private headers
│   └── CMakeLists.txt      # produces the `engine` static library
├── demos/                  # executables; link engine, include ONLY public headers
│   ├── 01-triangle/
│   ├── 02-scene/
│   └── capstone/           # the proof: a full game on the public API alone
└── tools/                  # editor, asset cooker
```

**From that point the boundary is law.** Demo and capstone code may only `#include <engine/...>`.
The capstone being buildable against nothing but the public API is what makes the claim
"professional grade" falsifiable rather than decorative — if the public API is missing something
the game needs, that is a bug in the engine, and we fix it in the engine.

### 2.3 Dependency direction

Strictly one-way. Arrows point at what a layer is allowed to know about:

```
demos/ ──► engine public API ──► engine private impl ──► platform (SDL3) ──► OS
tools/ ──►
```

`math` depends on nothing but the standard library — which is exactly why it is the first thing
under test. `core` may not include `gfx`. Nothing in `engine/` may include from `demos/`. A
cycle here is a design error, not an inconvenience to work around.

---

## 3. The course tree (`docs/`)

```
docs/
├── index.html              # course home: module map, all lessons, progress
├── conventions.html        # handedness, matrices, NDC/depth, winding — READ BEFORE MODULE 2
├── math-toolbox.html       # cumulative math appendix; grows each module
├── cpp-style.html          # the style guide the codebase obeys
├── lessons/
│   ├── 00-01-what-is-an-engine.html
│   └── ...                 # NN-MM-slug.html, zero-padded so they sort correctly
└── _template/
    ├── lesson-template.html   # canonical lesson skeleton + shared CSS + shared script
    ├── apply-shared.py        # authoring-time: stamps both shared regions into every page
    └── README.md              # authoring & visual style guide
```

`index.html`, `conventions.html` and `math-toolbox.html` are **living pages**, reissued updated
at every module boundary.

**The shared CSS and the shared page script are duplicated into every lesson file, by design.**
This is the one place we knowingly trade DRY for the self-containment guarantee: a lesson must
render from a bare filesystem, so it can reference neither a shared stylesheet nor a shared
script. `_template/lesson-template.html` is the source of truth for both. This trade is worth
naming out loud because it looks like a mistake until you know the constraint.

Duplication that nothing propagates *will* drift, and it drifted here before it was mechanised:
by Lesson 1.2 the trailing `<script>` existed in six mutually inconsistent versions — three
different C++ keyword lists, a CMake highlighter present in exactly one lesson, and a
Windows-batch comment rule present in two. Each was a silent mis-render, never a crash, which is
what made it survive review.

`apply-shared.py` closes that hole. Each page opts a region in with a marker pair, and the tool
rewrites only what lies between the markers:

```
<!-- SHARED-CSS:BEGIN -->    …    <!-- SHARED-CSS:END -->
<!-- SHARED-SCRIPT:BEGIN --> …    <!-- SHARED-SCRIPT:END -->
```

```sh
python3 docs/_template/apply-shared.py           # stamp
python3 docs/_template/apply-shared.py --check   # verify only; exit 1 on drift
```

Each region's canonical copy is read from between that same marker pair **in the template
itself**, so the template carries every marker a page does — which is what makes starting a
lesson by copying the template produce a page that is already opted into both regions. A new
lesson that drops the markers silently stops receiving updates, so the authoring guide calls
them out as must-keep.

Two further properties matter architecturally. **It is not a build step** — readers never run it
and the published files are complete static HTML, so the "no build tooling" guarantee in §1
holds; it is an authoring-time formatter. And **it is region-scoped, not file-scoped**, so a page
keeps its own page-specific JavaScript (Lesson 1.2's key-state widget, and any future
interactive diagram) outside the markers where the stamp cannot reach it.

---

## 4. The two-stage rendering spine

This is the course's central structural bet, and the reason the module order is what it is.

| Stage | Modules | Renderer | Student owns |
|---|---|---|---|
| **A** | 1–3 | CPU software rasterizer → SDL streaming texture | every pixel |
| **B** | 4+ | SDL_GPU (Vulkan / D3D12 / Metal) | every pipeline object |

Stage A exists so that Stage B is *recognition rather than incantation*. When SDL_GPU asks for a
depth-stencil state, the student has already written the depth test by hand and knows precisely
what they are configuring.

**The hinge that makes this work:** the software rasterizer targets **SDL_GPU's exact NDC** —
+Y up, depth `z ∈ [0,1]` with 0 at the near plane. Consequently the projection matrix, viewport
transform, and depth test all carry over to Module 4 **unchanged**, and the port is an API change
rather than a math change. Had Stage A used OpenGL-style `z ∈ [-1,1]`, every one of those would
need re-deriving exactly when the student is already loaded down with a new API.

The cost, stated honestly in the lessons: our projection matrix is the D3D-style `[0,1]`-depth
one, so it differs in the third row from the matrix in most tutorials the student will find
online. Paid gladly.

See [LEARNINGS.md](LEARNINGS.md) for the verified SDL_GPU convention table.

---

## 5. Engine subsystems and how they relate

Built roughly in dependency order — each module's milestone is the next module's foundation.

```
                    ┌──────────────────────────────┐
                    │   demos / capstone game      │
                    └──────────────┬───────────────┘
                                   │  public API only
   ┌───────────────────────────────▼────────────────────────────────┐
   │  scene: ECS ── transform hierarchy ── camera ── serialization   │
   └───┬─────────────────┬──────────────────┬───────────────┬───────┘
       │                 │                  │               │
  ┌────▼─────┐   ┌───────▼──────┐   ┌───────▼──────┐  ┌─────▼──────┐
  │ renderer │   │   physics    │   │  animation   │  │   audio    │
  │ PBR/     │   │ integrators  │   │  skeletal    │  │  streams   │
  │ shadows/ │   │ SAT/broad-   │   │  skinning    │  │  3D spatial│
  │ post     │   │ phase/impulse│   │  blending    │  │            │
  └────┬─────┘   └───────┬──────┘   └───────┬──────┘  └─────┬──────┘
       └─────────────────┴──────────┬───────┴───────────────┘
                                    │
              ┌─────────────────────▼──────────────────────┐
              │ core: log, assert, error, handles, alloc,  │
              │       jobs, time  │  math  │  assets       │
              └─────────────────────┬──────────────────────┘
                                    │
              ┌─────────────────────▼──────────────────────┐
              │        platform: window, events, input      │
              └─────────────────────┬──────────────────────┘
                                    │
                                 SDL3
```

**Load-bearing structural decisions:**

- **Handles, not pointers** (Module 5). Resources are addressed by *generational indices* — an
  index plus a generation counter — rather than raw pointers. Stale references are detectable
  (generation mismatch) instead of undefined behaviour, storage can be relocated and compacted,
  and serialization becomes trivial because a handle is just a number. This is why engines look
  the way they do, and it is taught as such.
- **ECS, not a scene tree** (Module 5). Data-oriented storage chosen after demonstrating —
  with cache-line reasoning and measurements — why OOP scene graphs creak at scale. Archetype
  vs sparse-set is a justified choice made in the lesson, not a coin flip.
- **Fixed timestep + render interpolation** (Module 1). The accumulator loop, derived rather
  than pasted as folklore. Simulation determinism is a property you design in early or retrofit
  painfully; physics in Module 7 depends on it already being right.
- **No exceptions, no RTTI in engine core.** Explicit error handling instead. The tradeoff is
  taught honestly in its own section rather than asserted.
- **Public API surface is a deliberate artifact,** not whatever headers happen to be reachable.

---

## 6. Critical workflows

Commands that are not obvious from reading files.

### Build

```sh
cmake -S . -B build              # first run compiles SDL3 via FetchContent — minutes
cmake --build build
```

`FetchContent` pins SDL3 to a tag. A fresh clone needs no SDL install and no submodule ritual.
The tradeoff (a one-time source build) was accepted over vendored submodules specifically to
kill the "forgot `git submodule update --init`" failure mode.

### Warnings are errors in spirit

The build runs at `-Wall -Wextra` (`/W4`) and all warnings get fixed. A warning left standing is
a broken window.

### Authoring a lesson page

```sh
python3 docs/_template/apply-shared.py --check   # before committing docs/ changes
python3 docs/_template/apply-shared.py           # after editing lesson-template.html
cd docs && python3 -m http.server 8000           # then verify in a real browser
```

Edit the shared CSS or the shared page script **in `lesson-template.html` only**, then stamp
(§3). `--check` exits 1 on drift, so it works unchanged as a pre-commit hook or CI gate; it also
lints for inline `fill=` on SVG `<text>`, which the shared stylesheet silently overrides.

Verify pages in **Playwright/Chromium over HTTP, never an embedded preview pane** — the pane
misreports computed styles, so a dead highlighter or a broken theme toggle can look correct
there.

### Shaders (Module 4+)

HLSL under `shaders/` → SDL_shadercross → SPIR-V / DXIL / MSL, compiled **offline** as a CMake
build step. Compiled artifacts are gitignored: HLSL is source, everything else is derived. Never
commit bytecode — stale bytecode that silently disagrees with its source is a miserable bug.

### Debugging

- **CPU:** the debugger from day one (Module 0), not printf. Breakpoints, watch, stepping.
- **GPU:** RenderDoc, with a dedicated lesson in Module 4. Frame captures are gitignored.
- **Profiling:** measure before optimizing. Module 3 profiles the software rasterizer; Module 8
  does CPU/GPU profiling case studies *on our own engine*.

### Tests

Unit tests under `tests/`, starting with `math` — it is pure, dependency-free, and every later
subsystem's correctness rests on it. Module 8 covers a pragmatic testing strategy for the parts
of an engine that resist unit testing.

---

## 7. Conventions that must never drift

Full detail with diagrams in [`docs/conventions.html`](docs/conventions.html); the compact form:

| Thing | Choice |
|---|---|
| World space | Right-handed, **Y-up, −Z forward** |
| Clip space | Left-handed, +Y up, **z ∈ [0,1]** — fixed by SDL_GPU, not our choice |
| Handedness flip | Absorbed by the **projection matrix** |
| Matrices | Column vectors, `v' = M·v`, stored **column-major** (maps onto HLSL's default) |
| Winding | **CCW = front**, cull back — *our* choice, set explicitly on every pipeline |
| Units | 1 unit = 1 metre; **radians** internally, degrees only at UI edges |
| Axis colours | x/y/z = **red/green/blue**, course-wide, in every diagram |
| Angles | Radians. Always. |

Right-handed world space was chosen to match glTF 2.0 (Module 6 loads it with zero axis
conversion) and every reference the course cites. Matching the references matters more than
matching the clip space, because the projection matrix mediates between them anyway.

**Winding deserves a special note:** it is *per-pipeline state* in SDL_GPU, not a global rule, and
both relevant enums default to zero — meaning "CCW front, cull nothing." A forgotten `cull_mode`
therefore manifests as *no culling*, not as an error. We set it explicitly, every time.
