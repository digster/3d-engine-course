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

The code in `src/` is the state of the engine as of the most recently published lesson (see
[STATE.md](STATE.md)). Running it today gives you **Module 3's software rasterizer** — a solid,
correctly occluded icosahedron spinning above a ground grid, and a checkerboard running to the
horizon, every pixel of both rasterized by code in this repository:

```sh
./build/engine            # macOS / Linux
.\build\Debug\engine.exe  # Windows
```

Arrow keys orbit the camera, <kbd>-</kbd>/<kbd>=</kbd> dolly. The keys worth pressing first:

- <kbd>F</kbd> cycles **wireframe → painter's algorithm → z-buffer → depth view**. The HUD counts,
  live, how many pixels the two hidden-surface strategies disagree about.
- <kbd>C</kbd> cycles six scenes, three of which exist to break sorting: a cyclic overlap where
  A is in front of B in front of C in front of A, an intersecting pair where the right answer
  changes halfway across one triangle, and two panels a millimetre apart.
- <kbd>B</kbd> switches the depth format between `D32_FLOAT`, `D24_UNORM` and `D16_UNORM` — the
  last of which makes z-fighting appear on demand.

Why sorting cannot be repaired, why the buffer stores *device* depth rather than view-space `z`,
and how to predict z-fighting in metres before you see it, is
[Lesson 3.1](docs/lessons/03-01-z-buffer.html).

Press <kbd>C</kbd> once more for the **checkered floor**. <kbd>I</kbd> turns perspective-correct
interpolation off and the floor buckles into the warping every PlayStation game had; <kbd>T</kbd>
subdivides the floor and shows you what the era's alternative actually cost. The derivation — five
lines, and it covers every attribute a vertex will ever carry — is
[Lesson 3.2](docs/lessons/03-02-perspective-correct.html).

Back on the solids scene, press <kbd>U</kbd> to cycle back-face culling. Half the triangles stop
being drawn and the picture does not change — except that the painter-versus-z-buffer counter drops
from 29 pixels to **0**, settling a debt [Lesson 3.1](docs/lessons/03-01-z-buffer.html) measured and
could not explain. Keep pressing and the last setting is the classic bug: culling with
`dot(normal, camera_forward)`, which misjudges one triangle in six at this field of view. Why that
is a different question from the right one, and why the answer is a sign the rasterizer was already
computing, is [Lesson 3.4](docs/lessons/03-04-back-face-culling.html).

Press <kbd>L</kbd> and the scene fills with something nobody typed: a **2,304-triangle torus read
off a disk**, non-convex, occluding itself, with the HUD reading `1152+73 -> 1225 verts`. Those
three numbers are the lesson — a file gives every face corner three *independent* indices and a
vertex buffer has one, so 1,152 positions become 1,225 vertices, and a cube's 8 become 24. Keep
pressing: `cube.obj` shows that split at a size you can count by hand, and `twisted.obj` is the
same cube with one face listed backwards — press <kbd>U</kbd> there and 2,459 pixels of it
disappear, because back-face culling believes winding. The HUD says `WINDING!` in red before you
press anything, because the mesh was *validated* at load: Euler characteristic (0 for the torus,
not 2 — a torus is not a sphere), boundary edges, and signed volume by the divergence theorem. And
the reading `round trip: 0 px differ` is the loaded torus being compared, every frame, against the
same mesh generated in memory and written out by our own OBJ writer.
[Lesson 3.5](docs/lessons/03-05-obj-loader.html).

Everything above is **lit**. Press <kbd>G</kbd> to cycle the shading: the debug palette that
coloured this course for five lessons (five brightnesses indexed by *triangle number* — watch it
fail to change as the object spins), then Lambert with one normal per face, then one per vertex,
at which point 2,304 flat triangles become a smooth torus. Hold <kbd>A</kbd>/<kbd>D</kbd> to swing
the light and the terminator sweeps across the surface; orbit the camera instead and **nothing
changes at all**, because Lambert is view-independent.

Then press <kbd>J</kbd>. That transforms normals with the model matrix instead of its inverse
transpose — the mistake nearly everyone makes — and the HUD reports what it costs. The
uniformly-scaled icosahedron is pixel-identical either way; the squashed slab and plinth are lit
as though they were never squashed, with normals tilted by up to 68°. Why a normal is not a
direction you can transform like any other, derived from the one property that defines it, is
[Lesson 3.6](docs/lessons/03-06-normals-and-lambert.html).

Now press <kbd>H</kbd> and orbit again. A highlight slides across the surface — the first thing in
this course that changes when you move your head, which is what makes a surface read as smooth and
hard rather than as plaster. The icosahedron's highlight is **white** on an amber body and the
slab's is **teal** on a teal body: one is a plastic and one is a metal, and no geometry or texture
is doing that work. <kbd>H</kbd> also cycles to Phong's original model, and the HUD counts how
many pixels the two disagree about *at matched exponents* — because Blinn's must be about 4× Phong's
for the same tightness, a fact the lesson derives rather than quotes. The interesting part is
where they differ: Phong's highlight switches off entirely when the light and your eye are on the
same side of a surface's normal, which on a floor means the sun is behind you. Measured on a
plane at 35° sun elevation: Phong lights **0 of 30,806** pixels and Blinn lights all of them.

Then watch the `peak` number while something spins. On the 1,225-vertex torus it sits at 255; on
the icosahedron it lurches, and on `cube.obj` the highlight is absent altogether in **157 of 180**
frames — because per-vertex shading samples a very sharp function at a few points and draws
straight lines between them. That is not a bug in the equation, it is a bug in where the equation
is evaluated, and it is left standing on purpose: it is Lesson 3.8's entire argument.
[Lesson 3.7](docs/lessons/03-07-specular-blinn-phong.html).

So press <kbd>G</kbd> once more. It now cycles four evaluation points — the debug palette, flat,
Gouraud, and **per-pixel** — while <kbd>Q</kbd> independently chooses whether the normal comes
from the face or the vertex. Two keys, because those are two questions, and separating them is
what Lesson 3.8 is about: where a normal comes from is a property of the *mesh*, and where the
shading equation is evaluated is a property of the *pipeline*. The HUD prints how many pixels the
current cell differs from per-pixel by, and with face normals selected and the highlight off it
reads **0** — all three evaluation points give the identical picture, exactly, until you press
<kbd>H</kbd> and the viewer starts to matter.

Per-pixel is the first change in eight lessons to touch the rasterizer's inner loop: `vertex`
grows two *varyings*, the clipper learns to carry them, and `fill_triangle` interpolates a normal
and a position and calls the shading equation itself. What it costs is on the HUD in microseconds
— and the answer contradicts the folklore. At 320×180 the torus covers 2.8 pixels per triangle,
which means more vertices than covered pixels, and per-pixel shading is measured at **0.91×** the
cost of Gouraud. It crosses over around three pixels per triangle and settles at 2.15× by 4K.
[Lesson 3.8](docs/lessons/03-08-shading-models.html).

Press <kbd>C</kbd> for the **FLOOR** and then <kbd>M</kbd>. The checkerboard that has run to the
horizon since Lesson 3.2 was always a *formula* — `checker_at(u, v)` — and <kbd>M</kbd> swaps it
for an actual image, one texel at a time. The picture barely moves, which is the point: the
default image was chosen to match the rule, so everything that *does* change from here is the
sampler's doing. <kbd>S</kbd> switches bilinear for nearest, <kbd>R</kbd> cycles repeat / mirrored
/ clamp (watch clamp turn the tiling into four stretched streaks — the edge texels smeared
outward forever, working exactly as designed), and <kbd>1</kbd> removes the half-texel offset.

That last one is the lesson. A texel is a **sample, not a square**, so its value lives at
`(i + 0.5)/N` — and forgetting the half shifts every bilinear sample by exactly half a texel,
which the HUD counts. Then press <kbd>S</kbd> to nearest and the count drops to **0**, and the HUD
says so in parentheses: nearest-neighbour sampling *cannot see this error*. Swept over 160,801
sample positions, removing the offset changes 0 of them under nearest and 160,632 under bilinear.
That is why the bug ships, and why the filter gets blamed for it.

Then press <kbd>G</kbd> to per-pixel and swing the light with <kbd>A</kbd>/<kbd>D</kbd>. **The
floor is lit** — for the first time, because a procedural rule computes a finished colour and has
no *albedo* for a light to multiply, while a texture does. Texture × light is not a convention
somebody chose: an albedo is a reflectance, a reflectance multiplies a quantity of light, and both
have to be linear, which is why the sampler decodes sRGB *before* it filters. Get that order wrong
and a black-and-white blend delivers 42.8% of the light it should.
[Lesson 3.9](docs/lessons/03-09-textures.html).

Then hold <kbd>=</kbd> on that floor and walk *into* it. <kbd>K</kbd> cycles what happens to a
triangle with a corner behind your eye: **clip** it (correct), **drop** it (the ground vanishes —
31,747 pixels of 57,600), or divide anyway with **no guard** at all (the floor folds inside out and
188 pixels survive). Why the perspective divide has a precondition rather than a guard, why the
near plane is `z_clip ≥ 0` and not `w ≥ 0`, and Sutherland–Hodgman implemented for polygons *and*
line segments, is [Lesson 3.3](docs/lessons/03-03-near-plane-clipping.html).

<kbd>Tab</kbd> cycles the earlier demos, which are kept rather than deleted: the basis-transform
visualiser (2.5), the triangle rasterizer's seven views (2.2–2.4), the line-algorithm fan (2.1),
and the **Module 1 checkpoint: a complete, playable game of Pong**. Left paddle
<kbd>W</kbd>/<kbd>S</kbd>, right paddle <kbd>↑</kbd>/<kbd>↓</kbd> (<kbd>C</kbd> swaps the AI for a
second player). Press <kbd>K</kbd> there to switch to a naive collision test and watch the ball
tunnel straight through a paddle — that failure, and why a 60 Hz machine can never show it to you,
is [Lesson 1.8](docs/lessons/01-08-pong.html).

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
