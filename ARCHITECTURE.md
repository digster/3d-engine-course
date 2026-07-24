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
│   ├── math/               # vec2/3/4, mat2/3/4, transform, quaternion — hand-rolled, no GLM
│   │   ├── vec2.hpp        # header-only; dot, normalise, reflect     [EXISTS from 1.7]
│   │   ├── vec3/4.hpp, mat2/3/4.hpp  # header-only 3-D maths          [EXISTS from 2.5–2.6]
│   │   └── transform.hpp   # position/rotation/scale → model matrix   [EXISTS from 2.8]
│   ├── gfx/                # framebuffer, software rasterizer → later SDL_GPU renderer
│   │   ├── colour.hpp      # pack/unpack, sRGB transfer functions   [EXISTS from 1.6]
│   │   ├── colour.cpp
│   │   ├── framebuffer.hpp # CPU pixel buffer, ARGB8888, row-major   [EXISTS from 1.5]
│   │   ├── framebuffer.cpp
│   │   ├── raster.hpp      # which pixels a SHAPE is made of         [EXISTS from 2.1]
│   │   ├── raster.cpp      # lines (2.1) + triangles (2.2) + shading (2.4)
│   │   └── viewport.hpp    # NDC -> pixels + the y-flip           [EXISTS from 2.11]
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

**The maths library, as of Lesson 2.5.** `src/math/` is header-only and stays that way: these
types are small, hot and stable, which is exactly the case where inlining at every call site beats
the tidiness of a `.cpp`. `vec2` (1.7) and `mat2` (2.5) are both plain aggregates with default
member initialisers and **no constructors** — deliberately, because that combination keeps brace
initialisation, `constexpr` evaluation, and a memory layout guaranteed to match what the
declaration looks like. The last of those stops being a nicety in Module 4, when a matrix is
uploaded to the GPU as raw bytes.

`mat2` stores **two `vec2` columns** rather than a `float[4]`, and that is a design decision with
teeth. The columns of a matrix are the images of the basis vectors — that is what a matrix *is* —
so a type whose members are those two vectors makes the idea structural rather than commentary.
Column-major storage then falls out for free: two adjacent `vec2`s are four adjacent floats in
column order, which is what SDL_GPU and HLSL expect, so there is no transpose at the API boundary
in Module 4. We did not choose the convention; we named the right things and the convention
followed.

Two consequences worth stating because they will govern `mat3` and `mat4` in Lesson 2.6:

- **Multiplication is written as its derivation.** `operator*(mat2, vec2)` is `c0*x + c1*y`, and
  `operator*(mat2, mat2)` is `{a * b.c0, a * b.c1}`. Both are one line, both read as the sentence
  that defines them, and neither contains an index that could be transposed by accident. The
  row-times-column form compiles to the same code and cannot be checked by reading it.
- **`at(row, col)` exists to bridge notation and storage.** Matrices are written in rows and stored
  in columns; the accessor takes the row first so that code can be read against a written
  derivation without transposing in your head. It is unchecked, which is a different trade from
  `framebuffer::put_pixel` — the rule is *check where the input can actually be wrong*, and a 2×2's
  indices are literals at every call site.

**Homogeneous coordinates, as of Lesson 2.7.** The fourth component of a `vec4` is not padding: it
records **what kind of thing the vector is**. `w = 1` marks a position, so a transform's translation
column is added in full; `w = 0` marks a direction, so it is skipped. That is one number doing the
work a type system would otherwise need two types for, and the engine makes the choice visible at
every call site with `point()` and `direction()` rather than a bare `to_vec4(v, 1.0f)`.

Two consequences run through everything built after this point:

- **Every transform before the projection is affine** — a bottom row of `(0, 0, 0, 1)`. That is what
  makes `w_out = w_in`, exactly, and the property is closed under composition, so a chain of any
  depth still returns positions as positions. `affine()` and `translation()` exist so matrices are
  built rather than filled in, which is what keeps the invariant true.
- **`xyz()` drops `w`; `perspective_divide()` divides by it — two separate named functions.** As of
  Lesson 2.10 the divide has its own name (`perspective_divide`, in `vec4.hpp`) rather than hiding
  inside `xyz()` or an implicit conversion. Keeping "drop `w`" and "divide by `w`" as distinct
  functions means a call site states which it meant, and a bug caused by a silent divide — far
  harder to find than a missing, named step — cannot happen.

The design decision worth recording is what we did *not* do. Distinct `position` and `direction`
types would make the "transform a normal as a position" bug a compile error, and some engines take
that route. We use named constructors instead: the type-safe version roughly doubles the maths
library's surface (every operation must state which combinations it accepts), and the distinction
has to collapse at the GPU boundary regardless, where a shader receives four floats and no types.
This is a genuine trade rather than an obvious call, and Lesson 2.7's Exercise 2.7.5 argues the
other side.

**The camera and the view matrix, as of Lesson 2.9.** A camera needs no new type: it is an object
with a placement, and the view matrix is the *inverse* of that placement — `view_from_world =
inverse(world_from_camera)`. That single sentence is why moving the camera one way moves the world
the other, and it is the second link of the space chain. `look_at(eye, target, up_hint)` in
`mat4.hpp` builds it, and two design points govern the directory going forward:

- **No general 4×4 inverse — and there still is not one.** A camera has no scale, so its placement
  is *rigid* (rotation + translation), and the inverse of a rigid transform is `transpose(R)` with
  `−transpose(R)·eye` for the translation, because an orthonormal rotation's inverse is its
  transpose. `look_at` writes that closed form directly. When the general inverse finally ships it
  will be because something genuinely needs it, not because a view matrix nudged it in early —
  exactly the discipline the missing cross product followed until 2.9.
- **The cross product entered here.** Building the camera's `right` axis from a look direction and
  an up hint is the first place the engine needs a vector perpendicular to two others, so `vec3`
  gained `cross` in 2.9 rather than 3.4. The course's spiral then deepens it in 3.4 (a triangle
  normal for back-face culling, and its tie to signed area). The old "deferred to 3.4" comment in
  `vec3.hpp` was revised accordingly.

The demo change is total but small: every object now draws through `view_from_model =
view_from_world · world_from_model`, and `to_screen3` became a plain orthographic projection of
*view* space — Lesson 2.8's oblique-projection hack is gone, because a real movable camera now
supplies the third dimension. Dollying the camera is deliberately a no-op under orthographic (the
HUD says so); making distance matter is the perspective divide, Lesson 2.10.

**The viewport, as of Lesson 2.11 — the chain completed.** `src/gfx/viewport.hpp` holds the final
transform: NDC to framebuffer pixels. It is three independent affine maps (a scale and an offset per
axis), and architecturally it earns its own type for two reasons.

- **It is the y-flip's one home.** NDC has `+y` up; a framebuffer counts rows downward from the top.
  Exactly one axis reverses, and that reversal — the `(1 - t)` in `to_screen` — had previously drifted
  through `to_screen` in the 2.5 basis demo, through every 3-D demo since, and through `project` in
  2.10, each time as a bare minus sign with a comment. Collecting it into one function means the
  convention cannot be gotten subtly wrong in the eleventh place that needs it, and it makes the
  upside-down-render bug a one-line diagnosis. This is the same "one home for a convention" instinct
  as `a_from_b` naming (2.8) — a fact scattered across a dozen call sites is a defect waiting to happen.
- **It mirrors `SDL_GPUViewport` field-for-field** (`x, y, w, h, min_depth, max_depth`, verified
  against `SDL3/SDL_gpu.h`), so Module 4 fills the GPU's struct by copying ours rather than translating.
  Same instinct as `mat4` already being column-major like HLSL constant buffers (2.6): match the
  destination early, port cheaply later. The `min_depth`/`max_depth` pair is not decoration — narrowing
  it pins a HUD or gizmo in front of the world without an extra pass, and the depth output is computed
  now so Lesson 3.1's z-buffer has something to store.

The refactor moved no pixels: a harness sweeps an NDC grid through both the old ad-hoc constants and
the new viewport and finds a worst difference of `0.000e+00`. With this, the geometry pipeline is
**complete** — `model → world → view → clip → NDC → screen` — and Module 2's remaining lesson spends
itself consolidating rather than adding.

**Perspective, as of Lesson 2.10 — the keystone.** `perspective(fovy, aspect, near, far)` in
`mat4.hpp` is the third link of the chain and the one that finally makes the scene look 3-D. The
architecture worth recording is the *shape of the trick*, because it explains several things that
otherwise look like arbitrary jargon:

- **A matrix can't divide, so the projection defers.** Perspective is `x' = f·x/(−z)` — divide by
  depth — but a linear map cannot divide one coordinate by another. So the matrix copies `−z` into
  `w` (its bottom row is `(0,0,−1,0)`), and a *separate* step, `perspective_divide`, divides
  everything by `w` afterwards. This is why **`w` stops being 1 here** (the third case flagged back
  in Lesson 2.7, finally cashed), and why the pipeline has both a "clip space" (the matrix's output,
  pre-divide) and an "NDC" stage (post-divide): they are the two sides of the one divide the matrix
  could not do. The rendering path in the demo is literally `clip = proj · view · model`, then
  `perspective_divide`, then the viewport — three stages in one `project()` helper.
- **The projection carries every convention that matters.** It targets SDL_GPU's clip space exactly
  — depth in `[0,1]` (not OpenGL's `[−1,1]`), `+y` up — and it is the single place the right-handed
  → left-handed handedness flip happens (Conventions §5). Getting this matrix right is what makes the
  Module 4 GPU port a change of API and not of maths (the NDC-parity decision). It is marked with a
  `⚠ VERIFY` in the lesson against the SDL wiki for exactly this reason.
- **Depth is non-linear, by construction.** Because the matrix divides by `−z`, depth resolution is
  concentrated near the camera; the near plane is the dominant control on precision, and this is where
  z-fighting (Lesson 3.1) is born. The engine does not hide this — the lesson makes it a number
  (`z = −2` already at `z_ndc = 0.5` for a 1..100 frustum).
- **Orthographic stays demo-local for now.** The `[P]` toggle's orthographic matrix lives in
  `main.cpp`, not the engine, because nothing beyond the comparison needs it yet and Lesson 2.11 owns
  the viewport/ortho machinery. Both projections run the *same* `perspective_divide` (ortho keeps
  `w = 1`, so it divides by one) — which is what makes the on-screen comparison honest: the only thing
  that differs is whether the matrix put depth into `w`.

**The transform, and the first scene, as of Lesson 2.8.** `src/math/transform.hpp` adds the first
type in the library that knows what a *scene* is: a `transform` holds a `position`, a `rotation`
(`mat3`) and a `scale`, and `parent_from_local(t)` turns those three authored quantities into the
one `mat4` that carries a mesh from its own space into the shared world. This is the first link of
the `model → world → view → clip → NDC → screen` chain the rest of Module 2 completes.

Three architectural commitments are made here, each of which the later modules lean on:

- **The model matrix is `T · R · S`, and the order is derived rather than conventional.** Scale acts
  along the object's own axes, so it must run while the coordinates are still the object's; rotation
  is about the object's own origin, so it must run before the object is displaced; translation is a
  statement about the world, so it runs last. `parent_from_local()` is the *only* place that
  encodes this — it builds the matrix as three rotation columns each scaled by one size component
  (nine multiplies, and it states the "columns are the object's frame" property in code rather than
  leaving it to be re-derived). Both wrong orders are reproducible in the demo on `[O]`: `T·S·R`
  shears a non-uniformly-scaled object as it turns, `R·T·S` orbits it about the world origin. Both
  are *identical* to the correct order when the scale is uniform or the rotation is identity, which
  is exactly why a wrong order hides in most of a scene — the failure profile worth internalising
  now because it recurs (normal transforms, shadow bias) for the rest of the course.
- **Spaces are distinguished by naming, not by types.** A `vec3` is the same bytes in model space
  and world space, and `w` cannot help because space does not change how a vector multiplies, only
  what the answer means. The engine's defence is the `a_from_b` convention (`world_from_model`,
  `view_from_world`): a product's adjacent labels must match, so a wrong-ordered composition is a
  spelling mistake visible before the program runs. Exercise 2.8.5 argues the type-tagged
  alternative (`vec3<World>`); we decline it for the same reason we declined position/direction
  types — the tag multiplies every signature and evaporates at the GPU boundary — while noting it is
  the strongest for *matrices* specifically, where a mistyped product is the most damaging error.
- **`transform` stores inputs, never a running matrix.** The demo rebuilds every object's matrix
  from one authoritative scalar angle each frame rather than multiplying last frame's matrix by a
  small delta, because the latter drifts out of being a rotation — the same shear as a wrong `T·S·R`
  order, arriving by accumulated rounding. This is the pattern Module 5's transform *component*
  keeps, and the reason `rotation` is a `mat3` only *for now*: Lesson 7.1 replaces it with a
  quaternion, which touches one line of `parent_from_local()` and nothing downstream, because
  everything downstream asks the transform for a matrix rather than reaching into it.

The demo's `to_screen3` also gained an *oblique* z term (a cabinet projection) so the new ground
plane does not collapse onto a line — an honest stopgap with an expiry date, since Lesson 2.10
replaces it with real perspective derived from similar triangles.

**The maths library in three dimensions, as of Lesson 2.6.** `vec3`, `vec4`, `mat3` and `mat4`
join the header-only `src/math/`, and the notable thing is how little had to be decided. Lesson
2.5's derivations never counted the axes, so every rule carried over and the new types are the
same shape as the old ones: N columns of `vecN`, column-major, arithmetic written as its own
derivation.

Three decisions are worth recording because they will govern the directory as it grows:

- **`identity()` is a static member on every matrix type.** Adding `mat3` made a free `identity()`
  impossible — it takes no arguments, so the `mat2` and `mat3` versions could differ only by return
  type, which C++ cannot overload on. Everything else survived (`transpose`, `inverse` and
  `determinant` overload on the parameter; `rotation` versus `rotation_x/y/z` differ by name;
  `scale` differs by arity), which localises the general rule: **a zero-argument function cannot be
  overloaded at all**, so it needs a distinct name or a type scope from the start.
- **Nothing speculative ships.** `vec3` gained a cross product only in Lesson 2.9, when the camera
  first needed "a vector perpendicular to two others" (Lesson 3.4 revisits it for a triangle
  normal); there is no `perpendicular` (in 3-D there is a plane of them, so the 2-D function has no
  honest generalisation); and there is still no *general* 4×4 determinant or inverse. The view
  matrix that Lesson 2.9 needs is the inverse of a *rigid* transform, whose closed form
  (`transpose(R)`, `−transpose(R)·eye`) is far cheaper than the general formula, so `look_at`
  writes it directly rather than inverting a matrix. Every function in `src/math/` exists because a
  lesson needed it, which is why every one has a derivation to point at.
- **Conversions between vector widths are explicit.** `to_vec4(v, w)` is a named function rather
  than an implicit conversion, so a 3-D vector can never silently acquire a fourth component nobody
  chose — and `xyz(v)` *drops* the fourth while `perspective_divide(v)` *divides* by it (Lesson
  2.10), two separate names so a call site can never confuse the two.

`mat4` is deliberately not yet more capable than `mat3`: with `w = 0` its fourth column is
multiplied by zero and contributes nothing, which Lesson 2.6 demonstrates rather than papers over.
Its layout is already the one SDL_GPU and HLSL constant buffers expect, so Module 4 uploads one
with a plain `memcpy`.

**Attributes, as of Lesson 2.4.** The rasterizer now carries values from the corners into the
interior, and three structural decisions came out of it.

- **`struct vertex` bundles a position with what that corner carries** — position *and* colour in
  one object. This is a correctness decision, not a tidiness one. `fill_triangle` reorients a
  backwards triangle by swapping two vertices; swapping loose coordinates while leaving loose
  attributes behind yields a triangle of exactly the right shape, in the right place, shaded one
  corner out of step. It fires for one winding only, so a spinning triangle looks correct half the
  time and a static scene may never show it. One `std::swap` on a struct makes it unwritable, and
  every attribute Module 3 adds inherits the fix for free.
- **`fill_setup` / `prepare_fill` hold the shared preparation** — clipped bounding box, three
  fill-rule biases, six per-pixel steps, three starting values — because there are now two fills
  and Module 3 brings more. The rule being followed is not "never repeat yourself" but *never
  repeat something subtle*: a bias wrong in one fill and right in another produces a crack visible
  only where those two kinds of triangle meet. **Orientation is deliberately excluded** from the
  helper and left to the caller, because orienting moves vertices and only the caller knows what a
  vertex carries.
- **Coverage and interpolation read the same accumulators differently.** The accumulator holds
  `E(x,y) + bias`; the coverage test wants the bias (that *is* the top-left rule) and interpolation
  must subtract it back out. Left in, it translates the whole attribute field by `1/‖e‖` pixels
  perpendicular to the opposite edge — invisible on a smooth attribute, whole wrong pixels on a
  quantised one. `is_top_left` moved to the header for this reason: once anything has to *undo* the
  bias, the rule producing it is part of how the rasterizer's numbers are to be read, and a rule
  you cannot inspect is a rule you cannot check.

Colour interpolation happens in **linear light** via `colour.hpp`'s `linear_rgb`. That type exists
so the distinction lives in a function signature rather than a comment; `raster.cpp` keeps a
private `rgb3` for the "whatever space we are averaging in" case, because a type named
`linear_rgb` holding encoded 0–255 values would be a lie that compiles.

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
