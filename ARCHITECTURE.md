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
│   ├── core/               # input, time, logging, assertions, error handling
│   │   ├── input.hpp       # frame-coherent keyboard/mouse snapshot  [EXISTS from 1.2]
│   │   ├── input.cpp
│   │   ├── clock.hpp       # monotonic frame timing, clamped dt      [EXISTS from 1.3]
│   │   ├── clock.cpp
│   │   ├── fixed_step.hpp  # simulation accumulator + alpha          [EXISTS from 1.4]
│   │   └── fixed_step.cpp
│   ├── math/               # vec2/3/4, mat3/4, quaternion — hand-rolled, no GLM
│   │   └── vec2.hpp        # header-only; dot, normalise, reflect     [EXISTS from 1.7]
│   ├── gfx/                # framebuffer, software rasterizer → later SDL_GPU renderer
│   │   ├── colour.hpp      # pack/unpack, sRGB transfer functions   [EXISTS from 1.6]
│   │   ├── colour.cpp
│   │   ├── framebuffer.hpp # CPU pixel buffer, ARGB8888, row-major   [EXISTS from 1.5]
│   │   ├── framebuffer.cpp
│   │   ├── raster.hpp      # which pixels a SHAPE is made of         [EXISTS from 2.1]
│   │   └── raster.cpp      # lines (2.1) + triangles (2.2)
│   ├── game/               # NOT engine — game code, see §2.1.1        [EXISTS from 1.8]
│   │   ├── pong.hpp        # the Module 1 checkpoint game
│   │   └── pong.cpp
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

**The frame's shape, and why the order is load-bearing.** As of Lesson 1.4 the loop in
`src/main.cpp` is settled, and its shape does not change again for the rest of the course:

```
drain events -> clk.tick() -> in.update()
             -> stepper.begin_frame(clk.dt())
             -> while (stepper.next_step()) { previous = current; simulate(current, h); }
             -> alpha = stepper.alpha()
             -> render(lerp(previous, current, alpha))
```

The drain must come first because `SDL_PollEvent` *pumps*, and pumping is what refreshes the
internal keyboard and mouse state `input` samples — sampling first costs a full frame of latency
and re-opens the stuck-key-after-alt-tab bug. The clock and input are each ticked **exactly once**
per frame, before anything reads them, so every system in the frame sees one coherent snapshot:
two systems integrating with two different ideas of how long the frame was is a class of bug with
no good symptom. `dt` is captured into a local `const float` at the top of the frame to make that
guarantee visible at the call site.

**Simulation and rendering are decoupled.** The simulation advances in fixed steps of `h`
(1/60 s by default) — `simulate()` receives `h` and has no access to the frame duration, which is
the separation expressed as a function signature. Rendering happens once per frame at whatever rate
the machine manages, drawing `lerp(previous, current, alpha)` where `alpha` is the accumulator's
leftover as a fraction of a step.

Three consequences worth knowing before touching this loop:

- **`previous = current` belongs inside the step loop.** A frame may run several steps; `previous`
  must hold the second-newest state. Hoisting it out is invisible above the simulation rate and
  rubber-bands below it.
- **Interpolation costs a duplicate of all render-visible state.** That is why the demo's state
  lives in a `sim_state` struct rather than loose locals, and it is the same pressure that produces
  the ECS in Module 5.
- **Two guards bound the spiral of death:** `clock`'s 0.25 s `dt` clamp (capping one frame at 15
  steps at 60 Hz) and `fixed_step`'s per-frame step cap, which discards the excess and *reports*
  it. Past the cap the simulation permanently falls behind the wall clock — a deliberate loss, not
  a repair.

Determinism from this is same-binary, same-machine only. Cross-platform lockstep needs far more
(see LEARNINGS.md).

**Rendering, as of Lesson 1.5.** The engine draws into a `gfx::framebuffer` — a `320×180`
row-major buffer of `Uint32` ARGB8888 pixels that it owns — and presents it once per frame through
an `SDL_TEXTUREACCESS_STREAMING` texture:

```
draw into framebuffer -> SDL_LockTexture -> copy row by row -> SDL_UnlockTexture
                      -> SDL_RenderTexture(NULL dst)  [scales to the window]
```

Four structural points:

- **`index = y * width + x`.** The only addressing convention in the engine. An x past `width` is
  not an error — it silently lands on the next row, which is why `put_pixel` is bounds-checked and
  `fill_rect` clips before writing rather than testing per pixel.
- **The copy is row by row, using the pitch `SDL_LockTexture` returns.** Drivers pad rows for
  alignment, so the texture's pitch may exceed `width * 4`. A single whole-buffer `memcpy` shears
  the image on exactly the machines where it does.
- **Pixels are only ever touched as `Uint32`,** built with shifts, so we use SDL's packed-integer
  format name (`ARGB8888`) and endianness never enters the engine. It becomes a real concern when an
  image loader starts reading bytes (Module 6).
- **Render resolution is independent of window size.** A `NULL` destination rect stretches the
  framebuffer over the whole target, so window resizing needs no code and render scale is a
  parameter — the mechanism behind dynamic resolution.

The API is deliberately two-tier: `put_pixel` is safe and checked, `row(y)` is the documented fast
path for inner loops that have already established their bounds (measured at 5–15× depending on
optimisation level). That split is the shape the drawing API keeps as the rasterizer grows.

**Colour, as of Lesson 1.6.** Stored channel values are **sRGB-encoded**, not measures of light:
the value 128 emits 21.6% of white's light, and half the light is stored as 188. `gfx/colour`
carries the exact piecewise transform in both directions, with a 256-entry decode LUT (the encode
direction cannot be tabulated — its input is a continuous float).

Three rules follow, and they are load-bearing for everything after Module 3:

- **Arithmetic on stored values is wrong.** Copying, comparing and picking colours are safe;
  mixing, fading, averaging, downscaling, anti-aliasing and adding light are not. The fix is always
  decode → compute → re-encode.
- **Alpha is exempt.** It is a coverage fraction, already linear, and must never go through the
  transfer function. `mix_linear` converts three channels and not the fourth, deliberately.
- **The dependency runs framebuffer → colour**, never the reverse. A framebuffer is a container of
  colours and may know what one is; a colour has no business knowing where it is stored.

We convert **per operation**, which is both slower and lossier than a real pipeline — each round
trip requantises to 256 steps. A properly linear renderer decodes once on the way in and encodes
once on the way out, which needs a float or half-float framebuffer, headroom above 1.0, and a
tonemapping step. That is Module 6's HDR work, and 1.6 states the debt explicitly rather than
implying the problem is solved.

**Maths, as of Lesson 1.7.** `src/math/vec2.hpp` is **header-only**, and that is a deliberate
exception to the `.hpp`/`.cpp` split every other subsystem follows. These functions are two or three
lines and are called thousands of times per frame; a definition in another translation unit
generally cannot be inlined, so `a + b` would become a real function call. The trade — changing one
line recompiles every dependant — is acceptable for code that is **small, hot and stable**, and a
vector type is all three. It is not a general licence, and Module 5's public-API boundary turns the
judgement into a rule.

Two conventions that propagate from here:

- **Pass small value types by value.** `sizeof(vec2) == 8`, which fits in a register; a
  `const vec2&` would hand over an address to dereference. Reach for a reference when the object is
  large or must be mutated in place.
- **`length_squared` is the default; `length` is the exception.** Square root is monotonic, so every
  comparison of distances gives the same answer without it. Square the constant, do not root the
  variable.

`normalised()` returns `(0,0)` for a zero-length input rather than dividing by zero — a `NaN` there
would spread silently through the frame (Lesson 1.3 §3.5). `normalised_or()` takes an explicit
fallback where some direction must exist.

Note that a header-only addition needs **no `CMakeLists.txt` change at all**, which is the build
system agreeing with the design.

**The rasterizer, as of Lesson 2.1.** `framebuffer` knows how to set *one pixel*; `raster` knows
which pixels a *shape* is made of. Everything added to `gfx/` for the rest of Modules 2 and 3
answers that same question about a more interesting shape, so the split is worth stating: a
routine belongs in `raster` if it decides *which* pixels, and in `framebuffer` if it decides
*how* to write them.

`raster.hpp` forward-declares `engine::framebuffer` rather than including it — every function
takes it by reference and none needs its layout (see §2.1.1 for why this matters).

**Triangles, as of Lesson 2.2.** `fill_triangle` is the shape of every rasterizer that follows,
so its structure is worth stating: measure the signed area once (which also detects degeneracy),
orient the winding, clip a bounding box, fold the fill rule into the loop's starting values, then
step three affine edge functions with three adds per pixel. Measured **75× faster** than
evaluating the edge functions directly over the whole buffer.

Two decisions there are load-bearing rather than incidental:

- **It does not cull by winding.** Module 2 rasterises in framebuffer space, where the viewport
  y-flip reverses the sign relative to the `CCW = front` convention. A fill that silently dropped
  "backwards" triangles would be indistinguishable from a bug, so it accepts either and orients
  its test to match. Culling is Lesson 3.4's, made in NDC where the convention means something.
- **The zero-area check is not housekeeping.** The fill rule's correctness proof requires a
  non-degenerate edge, so rejecting collinear triangles up front is what makes exactly-once
  coverage true rather than usually-true.

`edge_function` is `constexpr` in the header for the same reason `vec2` is header-only: three
arithmetic operations that every caller wants inlined. Its documented limit — coordinates within
about ±16000 before the products overflow `int32` — is a real constraint, not a formality, because
signed overflow is UB and Module 3's clipping is what keeps us inside it.

**Barycentric coordinates, as of Lesson 2.3.** `barycentric_at` adds no new computation — it is
the three edge functions `fill_triangle` already evaluates, divided by the total area. The type is
a named three-float struct rather than an array because `b.w0` at the call site documents the
vertex pairing that `b[0]` invites you to get wrong.

The pairing is the load-bearing detail: `w0` uses the edge **opposite** `v0`. A rotated pairing
still produces weights in `[0,1]` that sum to 1, so the only check with diagnostic power is
**reconstruction** — `w0·v0 + w1·v1 + w2·v2` must return the point. Verified in the harness across
3,721 points spread well beyond the triangle.

Two constraints propagate from here into Module 3:

- **Interpolation must use *unbiased* edge values.** The top-left rule's `−1` bias decides
  coverage and is not part of the geometry; using biased values shifts attributes by a fraction of
  a pixel. Lesson 2.4 carries both sets through one loop.
- **The weights are affine in *screen* space**, which stops being surface-correct once perspective
  arrives. That is not a defect to fix here — without projection, screen-space is exactly right —
  but it is why Lesson 3.2 exists.

Three line routines exist and only one is meant to be called. `draw_line_naive` and
`draw_line_dda` are kept because Lesson 2.1 is an *argument*, and the demo switches between them
at run time so the failure mode can be reproduced rather than described — the same reasoning that
keeps Pong's naive collision test in the shipped code.

**The choice of Bresenham is deliberate and is not about speed.** Measured on an M4 Pro at `-O2`,
DDA is ~2.2× *faster* than the compact all-octant Bresenham; the cost is Bresenham's two
data-dependent branches, not its arithmetic. We ship the slower one for exactness — integer
decisions are bit-identical across compilers and architectures, which floating-point ones are not
— and because its error term is the direct ancestor of Lesson 2.2's edge function. The numbers are
in LEARNINGS.md so the decision can be revisited with evidence rather than deference.

#### 2.1.1 `src/game/` — the boundary, three modules early

Introduced in **Lesson 1.8**, and the first directory in the tree that is emphatically *not*
engine. The test that decides where a file goes is one question:

> Could a completely different game use this unchanged?

`core`, `gfx` and `math` all answer yes — none of them knows that a paddle exists. `game/` answers
no, and is the only place that may know. The rule that follows is one-way and absolute:
**game code may depend on engine code; engine code may never depend on game code.**

Today this is enforced by a directory name and discipline, because everything still links into one
executable. That is the point of doing it now: the Module 5 refactor's difficulty is decided
entirely by how many wrong-way dependencies have accumulated before it starts. Zero of them makes
it a rename.

Two habits established here that carry into the refactor:

- **`pong.hpp` forward-declares `engine::framebuffer` rather than including it.** The header names
  the type in a signature but never dereferences one, so the compiler needs nothing more. Include
  what you use; forward declare what you merely mention. This is *physical design*, and it is a
  large part of why big C++ builds are fast or slow.
- **The simulation is a pure function of `(state, intent, h)`.** No globals, no clock reads, no
  hidden RNG — the PRNG seed lives inside the state struct. `state` is a plain copyable aggregate
  with no pointers, which is what makes `previous = current` cheap enough to run every step, and
  what will let Module 8 serialise it in one call. A single global read would silently destroy
  replayability; see [conventions.html §9](docs/conventions.html).

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

The same rule already applies in today's single-executable layout, with `src/game/` standing in
for `demos/` (§2.1.1): `src/game/` may include from `core`, `gfx` and `math`; none of those three
may ever include from `src/game/`.

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
