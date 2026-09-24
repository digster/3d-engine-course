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
| Course — plain HTML lessons + two shared assets | `docs/` | **None, ever.** Open the file. |
| Engine — an always-compiling C++20 codebase | `src/` → later `engine/`, `demos/`, `tools/` | CMake ≥ 3.24 |

The no-build-step rule is a hard constraint on the *tutorial content only*. A lesson page must
render from a bare filesystem with no network — a CDN is allowed for math typesetting, but every
crucial equation stays legible from the prose if the CDN is unreachable.

Note what that rule does **not** say. Pages are not individually self-contained: styling and page
behaviour live in `docs/shared/course.css` and `docs/shared/course.js`, which every page links.
That is still zero build steps — a relative `<link>` resolves straight off the filesystem — but
the unit of portability is the `docs/` **tree**, not the file. §3 covers why this changed and what
it costs.

**The codebase is a single evolving tree, not per-lesson snapshots.** `HEAD` is always the state
at the end of the most recently published lesson, and it always compiles. Each lesson's own
*Code Listings* section carries the full text of every file it touched, so the lesson is the
historical record; git history is the other half. This is why continuity errors are treated as
correctness bugs — there is no snapshot to fall back on.

---

## 2. Repository layout

> **The current layout is [§2.2](#22-now-lesson-51-onward-engine--demos--tools).** §2.1 records
> what the tree looked like through Module 4 and why, because the shape of the refactor is only
> legible next to the shape it replaced.

### 2.1 Then (Modules 0–4): one library-shaped executable — *retired by Lesson 5.1*

Deliberately **not** a framework yet. The internal structure was already drawn along the seams
the Module 5 refactor would cut, so that refactor felt like *revealing* a boundary rather than
inventing one — but without paying framework ceremony before it bought anything.

It worked: 57 files moved without a single one of them changing, because none of them had ever
been allowed to depend the wrong way. **The refactor's difficulty is decided entirely by how many
wrong-way dependencies accumulated before it started, and there were none.** What did have to be
untangled was the one file that had never been given a home: `src/main.cpp`, at 7,789 lines.

```
3d-engine-course/
├── CLAUDE.md               # master prompt — the binding spec
├── ARCHITECTURE.md         # this file
├── README.md
├── LEARNINGS.md            # verified SDL facts, gotchas
├── PROMPT.md               # prompt log
├── LICENSE                 # MIT, digster
├── CMakeLists.txt          # root build — written by the student in Module 0
├── cmake/                  # helper modules                            [EXISTS from 4.3]
│   └── Shaders.cmake       # HLSL -> spv/msl/dxil/json + a capability probe
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
│   │   ├── fixed_step.cpp
│   │   ├── profile.hpp     # zones, scope_timer, the frame budget  [EXISTS from 3.10]
│   │   └── profile.cpp     # tick conversion, the ring, medians
│   ├── math/               # vec2/3/4, mat2/3/4, transform, quaternion — hand-rolled, no GLM
│   │   ├── vec2.hpp        # header-only; dot, normalise, reflect     [EXISTS from 1.7]
│   │   ├── vec3/4.hpp, mat2/3/4.hpp  # header-only 3-D maths          [EXISTS from 2.5–2.6]
│   │   ├── transform.hpp   # position/rotation/scale → model matrix   [EXISTS from 2.8]
│   │   │                   #   `rotation` IS A quat SINCE 7.5 (was a
│   │   │                   #   mat3 for five modules). Also holds
│   │   │                   #   transform_from_affine, the decomposition
│   │   │                   #   THREE call sites were doing and TWO were
│   │   │                   #   doing wrong. INCLUDES quat.hpp, which
│   │   │                   #   makes it the only file in math/ that
│   │   │                   #   depends on the TOP of the rotation layer.
│   │   ├── rotation.hpp    # what is true of a rotation whatever you  [EXISTS from 7.2]
│   │   │                   #   store it in: the metric on rotations.
│   │   │                   #   SITS ABOVE the two below — both include
│   │   │                   #   it, neither includes the other.
│   │   ├── euler.hpp       # three angles ⇄ mat3, one convention of   [EXISTS from 7.1]
│   │   │                   #   twenty-four. An INTERFACE, not storage.
│   │   ├── axis_angle.hpp  # Rodrigues, the exp/log maps, and slerp   [EXISTS from 7.2]
│   │   ├── quat.hpp        # the TOP of the rotation layer: four      [EXISTS from 7.4]
│   │   │                   #   floats, the Hamilton product, the
│   │   │                   #   sandwich, and Shepperd extraction.
│   │   │                   #   7.5 ADDED THE INTERPOLATION BLOCK and
│   │   │                   #   rewrote `angle_between` with atan2.
│   │   │                   #   Includes axis_angle.hpp (it REUSES
│   │   │                   #   `axis_angle_extraction` rather than
│   │   │                   #   growing a twin), euler.hpp, mat3, vec3.
│   │   │                   #   NOT complex.hpp — same shape, no code.
│   │   └── complex.hpp     # plane rotations as a NUMBER: the product [EXISTS from 7.3]
│   │                       #   the rotor (half-angle), both blends.
│   │                       #   Includes mat2/vec2 only — it is one
│   │                       #   dimension down and shares no code with
│   │                       #   the three above. The TEMPLATE for 7.4's
│   │                       #   quat.hpp, function for function.
│   ├── gfx/                # framebuffer, software rasterizer → later SDL_GPU renderer
│   │   ├── clip.hpp        # near-plane clipping, in CLIP space     [EXISTS from 3.3]
│   │   ├── clip.cpp        # Sutherland–Hodgman; segments and polygons
│   │   ├── colour.hpp      # pack/unpack, sRGB transfer functions   [EXISTS from 1.6]
│   │   ├── colour.cpp      # + the fitted fast encode + encode_mode        (3.10)
│   │   ├── framebuffer.hpp # CPU pixel buffer, ARGB8888, row-major   [EXISTS from 1.5]
│   │   ├── framebuffer.cpp
│   │   ├── light.hpp       # directional light, Lambert + specular    [EXISTS from 3.6]
│   │   │                   #   NOTE: raster.hpp includes this as of 3.8 — see §5
│   │   ├── depth_buffer.hpp# CPU depth attachment, [0,1], 0 = near  [EXISTS from 3.1]
│   │   ├── depth_buffer.cpp
│   │   ├── texture.hpp     # image + sampler; SDL_GPUSampler-shaped   [EXISTS from 3.9]
│   │   │                   #   NOTE: raster.hpp includes this too — see §5
│   │   ├── texture.cpp     # wrap_texel, nearest + bilinear sampling
│   │   ├── gpu_device.hpp  # the SDL_GPU device + the window claim   [EXISTS from 4.2]
│   │   ├── gpu_device.cpp  #   gpu_report: every fact ASKED, none assumed
│   │   ├── gpu_present.hpp # a framebuffer's device-side mirror       [EXISTS from 4.2]
│   │   ├── gpu_present.cpp #   memcpy -> transfer buffer -> texture -> blit
│   │   ├── gpu_buffer.hpp  # geometry on the device                  [EXISTS from 4.4]
│   │   ├── gpu_buffer.cpp  #   one-shot upload + gpu_stream_buffer (4.5): the
│   │   │                   #   difference is HOW OFTEN, not what it holds
│   │   ├── gpu_mesh.hpp    # a mesh in the shape hardware wants      [EXISTS from 4.5]
│   │   ├── gpu_mesh.cpp    #   interleave/expand, 16-bit indices, and describe() —
│   │   │                   #   the ONE place the vertex layout is named
│   │   ├── gpu_uniform.hpp # per-frame data, and where each byte goes [EXISTS from 4.6]
│   │   │                   #   header-only: there is no object to own
│   │   ├── gpu_texture.hpp # an image on the device, and the sampler   [EXISTS from 4.7]
│   │   ├── gpu_texture.cpp #   + the depth target: same SDL type, no upload
│   │   ├── gpu_scene.hpp   # a SCENE, rather than a thing              [EXISTS from 4.8]
│   │   ├── gpu_scene.cpp   #   surface_style -> 3 pipelines; a list of gpu_draw_item
│   │   │                   #   -> N draws; draw_stats counts what the order cost
│   │   ├── gpu_debug.hpp   # making a frame legible to a debugger     [EXISTS from 4.9]
│   │   ├── gpu_debug.cpp   #   names at creation, debug_group (an RAII scope that
│   │   │                   #   is deliberately NOT movable), and frame_log
│   │   ├── image.hpp       # decoded pixels, always RGBA8              [EXISTS from 4.7]
│   │   ├── image.cpp       #   the ONE unit that contains stb_image
│   │   ├── gpu_pipeline.hpp# ALL render state, in one object        [EXISTS from 4.4]
│   │   ├── gpu_pipeline.cpp#   pipeline_desc owns the arrays the create-info points at
│   │   │                   # + instance_buffer + check_layout (4.5)
│   │   ├── gpu_shader.hpp  # a compiled shader + its reflection      [EXISTS from 4.3]
│   │   ├── gpu_shader.cpp  #   format choice, the JSON counts, CreateGPUShader
│   │   ├── raster.hpp      # which pixels a SHAPE is made of         [EXISTS from 2.1]
│   │   ├── raster.cpp      # lines (2.1) + triangles (2.2) + shading (2.4)
│   │   │                   # + depth (3.1) + perspective correction (3.2)
│   │   │                   # + back-face culling (3.4) + texturing (3.9)
│   │   │                   # + fill_style::encode (3.10)
│   │   │                   # + 2x2 quad traversal + quad_stats (4.1)
│   │   ├── viewport.hpp    # NDC -> pixels + the y-flip           [EXISTS from 2.11]
│   │   ├── mesh.hpp        # indexed geometry: verts + tri indices [EXISTS from 2.12]
│   │   │                   # + normals, owning mesh_data, mesh_report (3.5)
│   │   ├── mesh.cpp        # validate() + make_torus() + flip_uv_v   [EXISTS from 3.5]
│   │   ├── obj.hpp         # Wavefront OBJ read/write, asset paths [EXISTS from 3.5]
│   │   └── obj.cpp         # the index problem: (v, vt, vn) -> one vertex
│   ├── game/               # NOT engine — game code, see §2.1.1        [EXISTS from 1.8]
│   │   ├── pong.hpp        # the Module 1 checkpoint game
│   │   └── pong.cpp
│   └── platform/           # window + event pumping (Module 5; see the note below)
├── shaders/                # HLSL sources (Module 4+)                   [EXISTS from 4.3]
│   ├── triangle.vert.hlsl  # 4.4 draws with this pair; no resources at all
│   ├── triangle.frag.hlsl
│   ├── textured.vert.hlsl  # 4.6/4.7's pair — the register spaces, in real code
│   ├── textured.frag.hlsl
│   ├── mesh.vert.hlsl      # 4.5: six inputs across two buffers; 4.6: a real camera
│   ├── mesh.frag.hlsl      # 4.6: a lighting block in space3
│   ├── uniform_probe.vert.hlsl  # 4.6's instrument: a triangle from SV_VertexID,
│   ├── uniform_probe.frag.hlsl  #   and one uniform field per pixel, read back
│   ├── depth_probe.vert.hlsl    # 4.7's: a full-target surface at an exact
│   ├── depth_probe.frag.hlsl    #   distance, and a flat colour to count
│   └── texture_probe.frag.hlsl  #   one texel per pixel, for the sRGB readback
├── assets/                 # meshes, textures, fonts                   [EXISTS from 3.5]
│   ├── cube.obj            # 20 readable lines; the index problem, by hand
│   ├── twisted.obj         # …with one face reversed: 3.4's precondition, violated
│   ├── quirks.obj          # CRLF, negative indices, mixed corner formats, an n-gon
│   ├── torus.obj           # 2,304 triangles, written by save_obj from make_torus
│   └── uv_grid.png         # 4.7: a colour per corner, so ORIENTATION is machine-
│                           #   readable; gridlines, an arrow, a checkerboard
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

**Colour, as of Lesson 6.1 — the rule became a pipeline.** 1.6's three rules stand entire; what
6.1 added is *where* the conversions go. **Two conversions, at the edges**, with light in between
— not one per operation, and not "carefully". The difference is structural rather than a matter of
diligence: converting at the edges makes the middle a region where ordinary arithmetic is valid,
so a new operation added there is automatically working on light, and there is exactly one
quantisation to 8 bits instead of one per step.

The lesson audited all fifteen conversion sites outside `colour.{hpp,cpp}` and found one row left
over: **the GPU path had an input edge and no output edge at all.** SDL claims a window with
`SDL_GPU_SWAPCHAINCOMPOSITION_SDR`, whose header says "pixel values are in sRGB encoding", and
`scene.frag.hlsl` had been returning linear *light* into it since Lesson 4.8. Measured on real
downloaded pixels: linear 0.5 stored as **128 where 188 was meant**, and the error is a *ratio* —
13× at linear 0.02, 1.1× at 0.95 — which is why four modules of looking at the picture did not
find it.

`gpu_device::create` now asks for `SDR_LINEAR`, reads the resulting format back rather than
believing its own request, and records the answer in two `gpu_report` fields. `scene.frag.hlsl`
carries the exact piecewise curve for the machines that refuse. **Prefer the swapchain and keep
the fallback**, and the reason is the position of the blend stage: hardware blending happens
*after* the fragment shader, so a shader that encodes hands the blend unit codes to interpolate —
correct for opaque geometry and wrong the moment anything is transparent.

Three things are named as still owed rather than left to be discovered: headroom above 1.0 with a
float target and tonemapping (the HDR lesson); the software renderer's three remaining
per-operation conversions, which are *correct* but not a pipeline; and colour **spaces** as
opposed to transfer functions — this course stays in sRGB primaries throughout.

**Shading, as of Lesson 6.2 — the equation has units, and they are separable.** 6.1 settled what
the numbers mean at the output stage; 6.2 settles what they are on the way there. The shading
equation is now three factors carrying three distinct physical claims, and every later lesson in
Module 6 refines one of them rather than replacing the shape:

```
L_o = ( f_diffuse + f_specular ) · E_perp · cos θ  +  albedo · L_ambient
```

**The BRDF is what the surface does**, measured in **inverse steradians** — `lambert_brdf(albedo)`
= albedo/π and `specular_brdf(surface, lobe)` = colour·lobe/π, both in `gfx/light.hpp`. **`E_perp ·
cos θ` is what the light delivers**: `directional_light::irradiance` (renamed from `intensity`)
projected onto the surface, and `irradiance_on(normal)` is where that cosine now lives — which is
also where a point light's 1/d² will go. The **ambient term stands outside the product** because
its own π cancels against the hemisphere it is integrated over, which makes `albedo * ambient`
exactly right for a uniform environment rather than the fudge it was called in 3.6.

The rename is load-bearing. `intensity` had no unit, and four lines of algebra show what it
silently was: `intensity = E_perp / π`. The Lambert BRDF's own constant had been living inside the
light since Lesson 3.6. Renaming the field was the only tool that reaches every call site, because
the correct new value is π rather than the old 1.0 and there is no diagnostic for "same type, new
meaning". Its default is `k_reference_irradiance` (= π) so that defaulted lights stay correctly
exposed — **that constant is this engine's exposure, named and derived**, and Lesson 6.10's
tonemapper is what demotes it from rule to default.

`scene.frag.hlsl` mirrors all of it, with its own `k_inv_pi` because HLSL has no `<numbers>`;
`verify_62` §F parses the shader source and compares bit patterns, since a constant that exists
twice can disagree in its last bit. `scene_light_uniforms` did **not** change — the field was
always the product `colour × scalar` rather than the two factors, so giving one factor a unit
could not reach the GPU. A boundary that carries results rather than inputs is one the far side
cannot be wrong about.

**The energy test is now engine apparatus, not prose.** `R(v) = ∫ f_r cos θ dω ≤ 1` is twenty
lines in `verify_62`, and every BRDF from here on is run through it unchanged. Its first two
results: the raw Blinn-Phong lobe returns **2.6650** at shininess 1 with no constant, and — the
defect that survives the 1/π — diffuse and specular are **added with no coupling**, so a white
surface with a white highlight reflects **1.1386** of what arrives. `specular_brdf`'s division by π
is therefore documented as *not* a normalisation. Lesson 6.4's Fresnel term is the fix, and it is a
mechanism rather than a constant.

Named as owed: real photometric units (6.10) and
transmission — a BRDF describes only light leaving the side it arrived on, which is why this
renderer cannot make a convincing wax candle.

**Surfaces, as of Lesson 6.3 — `shininess` is retired in favour of a testable claim.** 6.2 gave
the equation units; 6.3 gives it a *surface*. `gfx/microfacet.hpp` (new, header-only, 56 → 57
public headers) models a surface as a landscape of microscopic perfect mirrors: a facet reflects
**l** to **v** only if its own normal is **h**, so the highlight's shape is a histogram of surface
slopes and **roughness is that histogram's width** — a quantity an instrument can measure, which
`shininess` never was.

The header supplies `ndf_model{blinn, beckmann, ggx}` behind one `ndf()` entry point (pipeline
state, not a material parameter — the same call 3.7 made for `specular_model`), the roughness
remap, and two forms of the Smith geometry term. **Nothing is wired in.** `shade()` and
`scene.frag.hlsl` are untouched; 6.4 assembles, because Fresnel is what couples the two lobes and
shipping *D* and *G* live would leave the renderer running a model that is neither the old one nor
the new one.

**The governing identity is the architectural fact**, because it is what makes every later
distribution checkable rather than merely plausible:

```
∫ D(h) · cos θₕ · dω  =  1
```

Read backwards: the microfacets' projected areas add up to the flat area they stand on. `verify_63`
§A is the test, and it is reused unchanged for every distribution added from here — a test
rewritten per subject is a test of the test.

What that test established about the engine as it stood: **Blinn-Phong satisfies the identity given
`(s+2)/2π`** — it was a microfacet distribution all along — and the engine ships `1/π`, wrong by
`(s+2)/2`, **17× at the default shininess of 32**. It is *not* fixed in 6.3, deliberately: the
materials were authored against the wrong constant, so `specular::colour` absorbed it, and
correcting one without the other blows every highlight out. **6.4 changed both in one commit — by
deleting the struct**, so that every one of the forty-four call sites had to be revisited rather
than silently recompiled.

Two conventions arrive with it. **`α = roughness²` is Disney's remap, not physics** — an imported
roughness from a tool that squares differently will not match, so convert at the import edge and
record which convention is stored, exactly the discipline 6.1 arrived at for colour. And
**height-correlated Smith is the default**, not by preference: the separable form's independence
assumption is false by a measured 1.715× at α = 0.8 and 80°.

**The BRDF, as of Lesson 6.4 — assembled, coupled, and measured.** 6.3 built `D` and `G` and wired
neither in, because a microfacet BRDF is `D G F` over a denominator and **Fresnel is the piece that
couples the two lobes back together**. 6.4 added `F` and assembled:

```
f_r = D·G·F / (4 (n·l)(n·v))  +  (1 − F(n·l))(1 − F(n·v)) · albedo/π
```

`gfx/microfacet.hpp` grew Schlick's Fresnel, `f0_from_ior` / `ior_from_f0`, `microsurface`
(roughness, metallic, F0 — **replacing the deleted `specular`**), `f0_of`, `diffuse_albedo_of`, and
`cook_torrance_specular`. `light.hpp` gained `cook_torrance_brdf` and a fourth `specular_model`
which is now the default; `scene.frag.hlsl` gained the same equation in HLSL, in the same commit —
**the first time in the course the two implementations had to move together** rather than one
following the other. `verify_48` §F is what made that survivable: 4096 fragments through both
paths, agreeing to **2.384 × 10⁻⁷**. A shader that drifts from the CPU does not fail loudly; it
renders something plausible.

The denominator is *derived* here rather than quoted, because it is the single most-repeated
unexplained line in real-time graphics. Spherical coordinates on the fixed direction give
`θ_out = 2θ_h`, hence `dω_out/dω_h = 2 sin2θ / sinθ = 4 cos θ = 4(v·h)` — and the `(v·h)` then
cancels against the facets' projected area toward the light, which is `(l·h)` and equal to it
because **h** bisects. That cancellation is exactly why the shipped formula looks arbitrary.

**The diffuse coupling is a measured decision, not a default.** The form nearly every engine ships,
`1 − F(v·h)`, is exact at normal incidence and reaches **1.3395** at grazing — it accounts for the
light that got *in* and says nothing about the light that fails to get *out*. Adding an exit factor
fixes the energy and **fails reciprocity**, which disqualifies it as a BRDF at all; the symmetric
two-crossing form is what remains, worst **0.9255**. `diffuse_coupling::half_vector` is kept, named
and measured, the same way 6.3 kept `smith_g_separable` — and 6.6's glTF loading may need it.

Its honest cost is ~8.5% less diffuse light at normal incidence than the half-vector form. That
light is the portion reflecting back *inside* at the exit boundary, which this model forgets —
the same class of loss as 6.3's missing 69%, and both want the same fix.

Two limits are named rather than left to be discovered. Single-scattering theory **loses 69% of
the light at full roughness** (R(v) = 0.3069 with F = 1), because a facet bounces light once and
the model forgets it — which is why rough metal renders dark, proportionally, so a brighter light
cannot fix it. And the **textbook GGX denominator is numerically wrong in `float`**: written
`c²(α²−1)+1` it is a catastrophic cancellation costing 1.1% of the model's energy at mirror
roughness, so `ndf()` computes the algebraically identical `(1−c)(1+c) + α²c²`, where Sterbenz's
lemma makes `1.0f - c` exact for `c ≥ 0.5`. `verify_63` §A asserts the comparison, so a later
"tidy" back to the textbook form fails loudly.

We convert **per operation**, which is both slower and lossier than a real pipeline — each round
trip requantises to 256 steps. A properly linear renderer decodes once on the way in and encodes
once on the way out, which needs a float or half-float framebuffer, headroom above 1.0, and a
tonemapping step. That is Module 6's HDR work, and 1.6 states the debt explicitly rather than
implying the problem is solved.

**The material, as of Lesson 6.5 — one home, and a rule for what belongs in it.** `gfx/material.hpp`
holds `material`: a tint, a `texture_handle` for the albedo map, a sampler and 6.4's `microsurface`.
The membership rule is the hardware's rather than an aesthetic one — **can this be a number in a
buffer?** Everything in `material` can, so it is per-draw data and two objects differing only in it
draw back to back. Cull mode, fill mode, the choice of BRDF and the shader cannot; they are baked
into a pipeline object, and two objects differing in them need two pipelines and a sort. That is
Lesson 4.8's measured result (three pipelines, `pipeline_binds` against `ideal_pipeline_binds`)
promoted from an observation to the line that defines a type. `verify_65` asserts it as
`is_trivially_copyable`.

**Handles store, pointers are used, and the resolve is a named per-draw step.** `material` refers to
its texture by `texture_handle` into a `texture_pool`; `bind_albedo(m, pool)` produces the
`texture_binding` the fill loop reads. A handle survives a reallocation (measured: 64 insertions
moved the pool, the handle did not care) and can be *asked* whether it still resolves, which a
pointer cannot — a stale one degrades to "no image" rather than to freed memory. Resolving inside
the loop would put a bounds check and a generation compare on every pixel, which is what 5.4 priced.

`texture_pool` deliberately did **not** live in `asset_store` when 6.5 wrote this: that store is
about files, and every texture at the time was generated in memory. **Lesson 6.6 answered the
deferred question with "yes"** — a glTF material arrives naming an image on disk, two materials in
one file routinely name the same one, and "load it once and hand out the same reference" is the
store's job description. The pool itself did not change; what grew around it is in §"Assets, and
the second format" below.

**Two texture handles as of Lesson 6.7**, and the second one passes the membership test for exactly
the same reason the first does: a `normal_map` is a number in a buffer, an index the fragment stage
reads, not a decision about which pipeline runs. They share one `sampler`, which is a compromise
named rather than hidden — glTF gives every texture its own, and the right fix is the one 6.5
already recorded as a debt (intern the sampler into a handle, at which point a second one costs four
bytes instead of sixteen).

**Derived, not stored.** `material::textured()` reads `albedo_map.valid()`. The previous arrangement
— a `textured` float in the uniform block beside a separately-chosen texture pointer — could express
"textured but no image" (debug magenta) and "image but not textured" (silently flat), neither of
which fails where the mistake is. `uniforms_of()` is now the single site that packs a material for
the GPU, and it computes the flag.

**A pool is for sharing, not for size.** `scene_object` holds its material by value; `ecs_swarm`'s
ninety-six drones share six materials by handle (3,456 bytes to 600, and one write where there was a
loop). The cost is named: `scene_object` grew 96 → 112 bytes, all of it the 16-byte `sampler`, which
a later lesson should intern the same way this one interned the texture.

**A fact about the geometry is not a decision about the draw.** `closed` stayed on `scene_object`
rather than moving onto the material, correcting a comment that had predicted otherwise since 3.4:
`validate()` reports whether a mesh is closed (`mesh_report::closed()`), the caller supplies the
intent, and `cull_of()` is the single place the rule — culling is valid only on closed geometry —
is written down.


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

**Meshes, as of Lesson 2.12 — Module 2's close.** `src/gfx/mesh.hpp` introduces the representation
everything downstream assumes: a `mesh` is a vertex array plus an index array taken in triples, one
triple per triangle. Three decisions govern it.

- **Triangles, not edges — even for a wireframe.** A triangle list is what a mesh *is*: Module 3
  fills these triangles, 3.4 culls them by winding, Module 4 uploads them. Storing edges would mean
  discarding face information and rebuilding it two lessons later. The cost is that a wireframe draws
  each shared edge twice (60 draws for the icosahedron's 30 edges); we name that waste rather than
  hide it, and it vanishes the moment triangles are filled.
- **`mesh` is two `std::span`s — a non-owning view.** Four words, trivially copied, and *cannot
  outlive the arrays it points at*. Correct for geometry that is `inline constexpr` data with program
  lifetime; incorrect the moment meshes are loaded at runtime, which is exactly the pressure that
  produces Module 5's handle-based asset system. The `inline` on those arrays is not decoration: a
  plain `constexpr` array in a header is a separate object per translation unit.
- **Mesh data is validated, never trusted.** Sixty hand-typed indices is data, and data is wrong in
  ways code cannot be. The harness checks Euler's `V − E + F = 2`, that every undirected edge belongs
  to exactly two faces, that every *directed* edge appears exactly once (consistent winding), and that
  each face's normal points away from the centre (outward winding). All faces are authored
  counter-clockwise-from-outside even though nothing consumes winding until Lesson 3.4 — so the
  meshes never need re-authoring. This is the same instinct as `put_pixel` bounds-checking while
  `at(row, col)` does not (Lesson 2.5): check where the input can genuinely be wrong.

The demo's `scene_object` (transform + mesh + name) is deliberately *not* a fatter
`engine::transform`. A transform is a placement, not a thing; keeping geometry separate is the shape
Module 5's ECS formalises, where an entity carries a transform component and a mesh component
attached independently.

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

**The plane is its own corner, as of Lesson 7.3.** `complex.hpp` breaks the layering described
below on purpose, and the reason is worth stating so it is not "fixed" later. `rotation.hpp` sits
above `euler.hpp` and `axis_angle.hpp` because all three are about rotations of **space** and share
a metric. `complex.hpp` is about rotations of the **plane**: it includes `mat2.hpp` and `vec2.hpp`
and nothing else in the rotation layer, and its `angle_between` is deliberately *not* an overload of
`rotation.hpp`'s `angle_between_rotations` because the plane's answer is **signed** and space's
cannot be. Two files that look like they should share an abstraction and do not, with the reason
written down, beats an abstraction that forces a signed and an unsigned answer to be the same
function.

Its real role is pedagogical and is a deliberate piece of sequencing: **7.4's `quat.hpp` is written
as a diff against it**. `conjugate`, `length_squared`, `normalised`, `inverse`, `operator*`,
`renormalised_fast` and `complex_slerp` each acquire a quaternion twin with the same body, one more
imaginary unit, and one loss — the product stops commuting. The names in `complex.hpp` are slightly
more formal than a two-float type needs, and that mapping is why.

**That diff was written, as of Lesson 7.4, and one line of it did not survive.** Every twin's *body*
carried up unchanged, because none of those proofs used commutativity — the things that broke are
exactly the things that mentioned the **order** of a product, and none of `conjugate`,
`length_squared`, `normalised` or `inverse` does. What did not carry is `renormalised_fast`'s
*failure mode*, and that is the more useful half of the exercise. In the plane it returns `−z` at
`|z| = 2`: a perfect modulus wearing a 180° error, which is why 7.3 said to test the rotation and
never the modulus. In space it returns `−q`, and `−q` **is** `q` as a rotation, so the plane's worst
input is a bit-for-bit perfect one here and the damage is in the middle of the range. A failure mode
carried from one dimension to another is a hypothesis about the new dimension, not a fact about it.

**`quat.hpp` is the top of the rotation layer, as of Lesson 7.4**, and its include list is the
claim. It includes `axis_angle.hpp` — not for Rodrigues, which it does not call, but for
`axis_angle` and `axis_angle_extraction`, which `axis_angle_from_quat` **returns**. That is a
deliberate refusal to grow a parallel `quat_extraction` struct with the same three fields and the
same `axis_route` enum: the question "which route produced this axis, and how far can I trust it?"
is a question about the *answer*, not about the representation it was recovered from, so the
vocabulary is shared. It also includes `euler.hpp`, for `quat_from_euler`, which is
`rotation_from_euler` with `rotation_*` swapped for `quat_*` and nothing else touched — the two
agreeing to 5.96e-07 is the check that this file's product and `mat3`'s describe the same
composition.

**The engine stores orientations as quaternions, as of Lesson 7.5.**
`transform::rotation` was a `mat3` for five modules and is now a `quat`, and
`parent_from_local` converts it back with `mat3_from_quat` — one line, and the arithmetic below it
did not move a character. **That conversion is engine policy rather than a detail.** Lesson 7.4
§10.3 measured the crossover: a quaternion is 1.57× *dearer* than a matrix at rotating a vector and
converting costs 4.619 ns, so the matrix pays for itself after about eight vectors. A mesh has
thousands. So: store four floats, convert exactly once per object per frame, multiply vertices by a
matrix. Anything that steps a rotation incrementally (an accumulator — `collector`'s carousel and
spinner are the first two in this repository) calls `renormalised_fast` on the result; anything that
rebuilds from an angle each frame does not.

**The swap was not a rename, and the reason is an engine-wide constraint.** Three call sites were
assigning something that was not a rotation to a field named `rotation` — `gfx/renderable.cpp`
(shipping since 5.11), `demos/ecs_swarm`, and `demos/collector`'s camera boom (shipping since 5.12).
The first two put `linear_of(w.matrix)` there, which for an ECS `world_transform` carries the
**scale** that came down the hierarchy, and that is why their recomposition reproduced the original
affine matrix "to the bit": **a `mat3` will hold anything**. A quaternion will not, so narrowing the
type turned those lines into compile errors.

`math/transform.hpp` grew **`transform_from_affine`** as the repair, because three places were doing
that job and two were doing it wrong. It returns a `transform_extraction` — value plus
`out_of_square` plus `mirrored` — the same shape as `axis_angle_extraction` (7.2) and
`euler_extraction` (7.1), for the same reason: a recovery that can lose information should hand back
what it lost in the same breath. Two details in it are load-bearing. A **negative determinant** is a
mirror, and since column lengths are non-negative the sign has to be handed back deliberately or a
mirrored object silently becomes an unmirrored *rotated* one. A **zero-length column** is rebuilt as
the cross product of the surviving pair, not as the parent's axis: the inherited `gltf_view` version
did the latter, its comment correctly said this "keeps the matrix finite instead of producing NaNs",
and a non-orthonormal basis makes `quat_from_rotation` wrong in *every* column — 0.399 of entry
error on a flattened object whose other two axes were perfectly recoverable. **Finite is not right.**

**The limit is shear, and it is now counted rather than hidden.** A `position + quat + vec3`
transform can represent translation, rotation and axis-aligned scale, and **cannot represent
shear** — which a non-uniformly scaled parent with a rotated child produces. Column lengths cannot
see it (a shear changes the *angles* between columns, not their lengths), so `out_of_square`
measures the angles and `renderable_report::skewed` counts the objects.

**`T · R · S` per node is a restriction, not a representation**, and the fix for a matrix outside it
is usually another node rather than a wider field. `collector`'s boom needed
`S⁻¹ · Rz(−bank)` — scale *first* — which a single `transform` cannot express because its scale is
innermost. Two entities in a chain express it exactly, parent-first, and reproduce the old basis to
0.000e+00. Real engines split nodes for exactly this reason.

**What is still owed:** `scene_object` holds a `transform` where it should hold a `mat4`. Two
functions now take a matrix apart only to hand the pieces to `parent_from_local`, which puts it back
together; the GPU path (`gpu_draw_item::world_from_model`) and Lesson 5.9's hierarchy both already
carry the matrix. Lesson 7.5 names that and does not make it, on the same grounds 5.12 named
`view<const T>`: it is a published struct with a dozen callers.

**Rotation gets a layer, as of Lesson 7.2.** `math/` now has an internal shape rather than a flat
pile of headers. `rotation.hpp` holds what is true of a rotation *whatever you store it in* — today
the metric, `angle_between_rotations`, which answers "how far apart are these two orientations?"
without caring how either was written down. `euler.hpp` and `axis_angle.hpp` both include it and
**neither includes the other**, which is the dependency shape to preserve as 7.3 and 7.4 add two
more representations.

The rule that produced it is worth stating, because it is the opposite of the usual advice: the
metric was written in 7.1 and deliberately left in the wrong file, with a doc comment saying so and
naming the lesson that should move it. **A header earns its existence when a second inhabitant
justifies the shelf** — a `rotation.hpp` created in 7.1 would have held one function and been
indistinguishable from premature organisation. When the move happened, `euler.hpp` was left
including the new header so that not one call site had to change; a refactor that makes its callers
edit is a refactor that keeps being postponed.

Note what is still *not* here: no rotation type is the engine's storage format. `transform::rotation`
is a `mat3` and stays one until Lesson 7.4's quaternion. 7.3 did not change that either — a
`complex` rotates the plane, and nothing in this engine's scene graph is two-dimensional. Euler angles are an interface for humans and
file formats; axis-angle is an interface and an interpolator — 7.2 measured that it has no usable
composition formula at all, which makes it a *worse* storage format than the matrix it would replace.

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
> Lesson 5.1 resolved this directory: `pong.{hpp,cpp}` now lives in `demos/common/`, which is
> where a game belongs. The reasoning below is kept because it is what made the move a rename.

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
  what will let Module 9 serialise it in one call. A single global read would silently destroy
  replayability; see [conventions.html §9](docs/conventions.html).

### 2.2 Now (Lesson 5.1 onward): engine / demos / tools

Module 5 opened with a dedicated refactor arc, taught as a **first-class architecture lesson**
(what makes a good public API, physical design, dependency direction) — not rushed through as a
chore. What follows is on disk.

```
├── CMakeLists.txt          # acquires SDL3 + stb + ImGui, declares the shaders and the
│                           #   imgui target ImGui does not ship, adds the two subdirs
├── cmake/
│   ├── EngineHelpers.cmake # engine_set_warnings / _use_assets / _use_shaders   [5.1]
│   └── Shaders.cmake       # add_hlsl_shader(name stage) -> a GLOBAL PROPERTY   [4.3, reshaped 5.1]
├── engine/                 # THE LIBRARY                                        [5.1]
│   ├── CMakeLists.txt      # produces engine::engine (STATIC)
│   ├── include/engine/     # ---- THE PUBLIC API. 102 headers. Nothing else. ---
│   │   ├── engine.hpp      # the umbrella. UNTIL 5.12 it listed 40 of the 55 headers
│   │   │                   #   it could have — missing the whole ECS, the asset
│   │   │                   #   store, handles, the logger and the action map —
│   │   │                   #   because NOTHING IN THE TREE INCLUDES IT, so it was
│   │   │                   #   never compiled and nothing could fail. Now complete,
│   │   │                   #   and CHECKED at configure time by engine/CMakeLists.txt   [5.12]
│   │   ├── asset/          # NAMES, ROOTS AND LIFETIMES                       [5.5]
│   │   │   ├── search_path.hpp # ordered roots; the ONLY caller of
│   │   │   │                   #   SDL_GetBasePath() in the engine
│   │   │   └── asset_store.hpp # load / find / insert / derive / unload
│   │   │                       #   + textures, materials and load_model      [6.6]
│   │   ├── core/           # clock, fixed_step, input, profile,
│   │   │   │               #   log.hpp + assert.hpp                          [5.3]
│   │   │   ├── actions.hpp # actions, bindings, and the input_snapshot     [5.10]
│   │   │   │               #   CONCEPT. Buttons and axes are one mechanism.
│   │   │   │               #   + masked_input<Source>: the concept's SECOND  [5.11]
│   │   │   │               #   implementor, and it is not a test
│   │   │   ├── bench.hpp   # A/B timing: alternate, median, keep, agree      [5.6]
│   │   │   ├── handle.hpp  # handle<T>: 20 index / 12 generation, in 32 bits  [5.4]
│   │   │   └── pool.hpp    # pool<T>: sparse slots + dense items + free list  [5.4]
│   │   ├── ecs/            # THE WORLD. Header-only, so NO entry in the         [5.8]
│   │   │   │               #   library's source list. See §5's ECS notes
│   │   │   ├── entity.hpp  # entity (20/12, like a handle) + entity_allocator
│   │   │   ├── pool.hpp    # pool_base (cold virtuals) + pool<T> (the sparse set).
│   │   │   │               #   engine::ecs::pool<T> IS NOT engine::pool<T>
│   │   │   ├── registry.hpp# the world; component_id_of<T>(); type erasure, no RTTI
│   │   │   ├── view.hpp    # the query. LEADS WITH THE SMALLEST POOL
│   │   │   ├── hierarchy.hpp # parent + world_transform; LEVEL-ORDER resolve  [5.9]
│   │   │   │               #   set_parent (refuses cycles), destroy_subtree
│   │   │   └── camera.hpp  # camera + active_camera (a TAG); look_along;      [5.9]
│   │   │                   #   view_from_camera = rigid_inverse(placement)
│   │   ├── math/           # vec2/3/4, mat2/3/4, transform, euler  (header-only)
│   │   │   └── euler.hpp   # euler_angles + rotation_from_euler + the inverse,   [7.1]
│   │   │                   #   euler_rate_jacobian (det = -cos pitch: gimbal lock
│   │   │                   #   as a number), wrap_angle, angle_between_rotations.
│   │   │                   #   AN INTERFACE, NOT STORAGE — transform keeps a mat3
│   │   ├── platform/       # HOW A PROGRAM STARTS                          [5.2]
│   │   │   ├── platform.hpp  # surface, app_config, platform — SDL's lifecycle,
│   │   │   │                 #   owned once. YOU keep the loop
│   │   │   ├── app.hpp       # app (8 hooks — on_input added in 5.10), app_runner
│   │   │   │                 #   (the 4 SDL callbacks) — the engine keeps the loop
│   │   │   └── main.hpp      # ENGINE_MAIN. ONE .cpp per program; no main() in it.
│   │   │                     #   NOT in engine.hpp, deliberately
│   │   ├── phys/           # HOW A STATE ADVANCES, WHAT MOVES IT, HOW IT TURNS,
│   │   │                   #   WHERE ITS SURFACE IS, WHERE TWO OF THEM TOUCH,
│   │                   #   WHICH TWO ARE WORTH ASKING, AND WHAT TO
│   │                   #   DO ABOUT IT                       [8.1-8.10]
│   │   │                   #   THE FIFTH NEW DIRECTORY SINCE THE REFACTOR, and
│   │   │                   #   the second (after audio/) that never touches a
│   │   │                   #   pixel. NOT under math/: the test is what a file
│   │   │                   #   KNOWS. math/ knows about numbers, which is why it
│   │   │                   #   has no .cpp at all; this knows that a velocity is
│   │   │                   #   metres per second and that a step size has a
│   │   │                   #   STABILITY LIMIT. Those are claims about a
│   │   │                   #   simulation, not about a vector space.
│   │   │   └── inertia.hpp   # point_mass, inertia_of_point(s), the solid-body
│   │   │                     #   tensors (box/sphere/cylinder/rod/CAPSULE),
│   │   │                     #   shift_/unshift_inertia (parallel axis),
│   │   │                     #   rotate_/world_inertia (the R I Rᵀ sandwich),
│   │   │                     #   inverse_inertia, inertia_assembly,
│   │   │                     #   inspect_inertia.                          [8.3]
│   │   │   └── shape.hpp     # shape_kind{sphere,box,capsule} + `shape` (24 bytes,
│   │   │                     #   BODY axes, CENTRED on the centre of mass) + the
│   │   │                     #   factories + volume_of/bounding_radius +
│   │   │                     #   inertia_of (the ONE arrow into inertia.hpp) +
│   │   │                     #   the world placements `obb`/`capsule`/`hull` +
│   │   │                     #   support and support_local for all four.
│   │   │                     #   `hull` IS NOT A shape_kind: it holds a span of
│   │   │                     #   somebody else's vertices, and a `shape` is a
│   │   │                     #   value you can copy into a save file. The sharper
│   │   │                     #   reason is that inertia_of would have to answer
│   │   │                     #   for it and cannot — a tensor needs faces.
│   │   │                     #                                          [8.4, 8.5]
│   │   │   └── collide.hpp   # `separation` (unit axis FROM a TOWARD b, signed
│   │   │                     #   depth, axis_index, axes_tested), axis_source,
│   │   │                     #   the projection primitives, collide() for every
│   │   │                     #   pair of {sphere, aabb, obb}, overlaps(), the
│   │   │                     #   closest-point pair — and 8.5's collide(convex,
│   │   │                     #   convex) facade over GJK.               [8.4, 8.5]
│   │   │   └── convex.hpp    # `convex`: a support FUNCTION POINTER, a context
│   │   │                     #   pointer and an origin. 24 bytes, header only.
│   │   │                     #   THE THIRD DISPATCH PATTERN IN THE ENGINE, after
│   │   │                     #   the ECS's type-erased pools and 6.17's frame-graph
│   │   │                     #   callbacks, and chosen the same way: 5.2 forbids
│   │   │                     #   virtuals in the core, a template would instantiate
│   │   │                     #   GJK once per PAIR of shape types, and the switch
│   │   │                     #   cannot reach a `hull`. Measured at 1.906 ns
│   │   │                     #   against 1.908 direct. `support` returns a point
│   │   │                     #   RELATIVE to `origin`, which is the file's whole
│   │   │                     #   numerical argument: 96,270x smaller error a
│   │   │                     #   thousand km out. Four DELETED rvalue overloads,
│   │   │                     #   because a view that outlives its subject is the
│   │   │                     #   one mistake this shape of interface invites. [8.5]
│   │   │   └── epa.hpp       # epa_status/epa_result/epa_config, epa_penetration,
│   │   │                     #   depth_along, and the two capacity constants.
│   │   │                     #   HOW DEEP AND WHICH WAY, which is the question a
│   │   │                     #   solver asks and the one GJK cannot answer: the
│   │   │                     #   depth is a distance to the BOUNDARY of A ⊖ B and
│   │   │                     #   GJK only ever looks inward. `depth_along` is the
│   │   │                     #   certificate in one line — h_{A⊖B}(n), which the
│   │   │                     #   definition minimises over all directions — and
│   │   │                     #   `manifold()` is Euler's F = 2V − 4 as a runtime
│   │   │                     #   test computable from the result alone.       [8.6]
│   │   │   └── manifold.hpp  # contact_feature/contact_id/contact_point/
│   │   │                     #   manifold_status/contact_manifold/manifold_config,
│   │   │                     #   build_manifold, collide_manifold, carry_impulses,
│   │   │                     #   pair_key and manifold_cache.
│   │   │                     #   WHERE two shapes touch, not how far into each
│   │   │                     #   other: a set of up to FOUR points sharing one
│   │   │                     #   normal, because a contact force acts at a point
│   │   │                     #   and therefore exerts no torque about it, and a
│   │   │                     #   body held up by one point is balanced on a pin.
│   │   │                     #   FOUR IS FORCED: four non-negative impulses place
│   │   │                     #   the resultant anywhere in a planar convex hull
│   │   │                     #   and a fifth is a linear combination, so it makes
│   │   │                     #   the answer non-unique rather than better.
│   │   │                     #   `contact_id` is six named fields and every one is
│   │   │                     #   an INDEX into geometry that does not move, which
│   │   │                     #   is what lets 8.10 warm start. epa.hpp needed NO
│   │   │                     #   CHANGE: its winning triangle is built from support
│   │   │                     #   points and carries their ambiguity.          [8.7]
│   │   ├── broadphase.hpp  # proxy/broadphase_pair/broadphase_config/
│   │   │                   #   broadphase_stats, brute_force_pairs,
│   │   │                   #   auto_cell_size, uniform_grid, owner_cell.
│   │   │                   #   WHICH PAIRS ARE WORTH ASKING ABOUT. THE FIRST
│   │   │                   #   FILE IN phys/ THAT DEPENDS ON NO SHAPE AT ALL —
│   │   │                   #   six floats and an index is everything it knows
│   │   │                   #   about a body, deliberately: the stage whose whole
│   │   │                   #   value is being cheap must not be able to look
│   │   │                   #   inside anything. A hashed uniform grid, rebuilt
│   │   │                   #   every frame in four linear passes, hashed rather
│   │   │                   #   than arrayed because a dense array's memory
│   │   │                   #   follows the WORLD and has an EDGE that must be
│   │   │                   #   clamped, wrapped or rejected. May report pairs
│   │   │                   #   that do not touch; may NOT omit one that does,
│   │   │                   #   which is why every approximation rounds outward.
│   │   │                   #   `a < b` on every pair, so 8.7's pair_key indexes
│   │   │                   #   the manifold_cache and the normal never flips.
│   │   │                   #   `owner_cell` is the duplicate fix and it is EXACT
│   │   │                   #   rather than heuristic.                    [8.8]
│   │   ├── solver.hpp      # contact_material/combine_rule/friction_model,
│   │   │                   #   tangent_basis, contact_velocity, effective_mass,
│   │   │                   #   solver_config, contact_constraint/contact_batch,
│   │   │                   #   prepare_contacts, warm_start_contacts,
│   │   │                   #   solve_contacts, write_back, resolve_contact, and
│   │   │                   #   six closed forms.                          [8.9]
│   │   │                   #   THE FIRST FILE IN phys/ THAT WRITES A VELOCITY.
│   │   │                   #   Everything from 8.4 to 8.8 answers a question;
│   │   │                   #   this one changes the world. A collision is an
│   │   │                   #   EVENT rather than an interval, so the arithmetic
│   │   │                   #   is one linear equation per contact in one scalar
│   │   │                   #   unknown, and the constant in it is the effective
│   │   │                   #   mass. Materials are a PAIR property and are an
│   │   │                   #   argument rather than a field on rigid_body.
│   │   │                   # + position_correction/pseudo_velocity/prepare_bias/
│   │   │                   #   solve_positions/apply_pseudo_velocity,
│   │   │                   #   contact_pair/island/build_islands,
│   │   │                   #   sleep_config/wake_islands/update_sleep,
│   │   │                   #   solver_stats/contact_solver, arrival_depth,
│   │   │                   #   baumgarte_time_constant.                  [8.10]
│   │   │                   #   THE LOOP AROUND IT, and three orderings are law:
│   │   │                   #   warm starting ONCE per step; every velocity
│   │   │                   #   iteration before any position one; WAKING before
│   │   │                   #   the solve and the SLEEP TEST after it, because
│   │   │                   #   before the solve every resting body in the scene
│   │   │                   #   is travelling at g*h. Islands come from a
│   │   │                   #   union-find in which a body that CANNOT MOVE IS
│   │   │                   #   NOT A BRIDGE — break that and the floor welds the
│   │   │                   #   level into one island and nothing ever sleeps.
│   │   │                   # + constraint_solver (the class renamed; the old
│   │   │                   #   `contact_solver` is an alias), joint_pair, the
│   │   │                   #   joint ranges on `island`, a four-argument
│   │   │                   #   build_islands, solver_config::joints and the
│   │   │                   #   joint fields of solver_stats.             [8.11]
│   │   │                   #   JOINTS RIDE THE SAME LOOP: prepared and warm-
│   │   │                   #   started once, visited FIRST in every sweep of
│   │   │                   #   every island, then the contacts. A joint is an
│   │   │                   #   island edge by `union_edge` — the same function
│   │   │                   #   contacts use — or a chain sleeps a link at a time.
│   │   ├── constraint.hpp  # jacobian_row + point_row/angular_row/prepare_row/
│   │   │                   #   solve_row; joint_kind {distance, ball_socket,
│   │   │                   #   hinge}, joint_limit, joint_motor, joint and its
│   │   │                   #   make_* functions, hinge_angle, measure_joint,
│   │   │                   #   joint_config, joint_batch (3x3 point block, 2x2
│   │   │                   #   hinge block), prepare/warm_start/solve_joint,
│   │   │                   #   solve_joint_positions, write_back,
│   │   │                   #   collision_filter, pseudo_velocity (moved here
│   │   │                   #   from solver.hpp), and six closed forms including
│   │   │                   #   lever_ratio.                              [8.11]
│   │   │                   #   A CONSTRAINT IS A ROW AND TWO BOUNDS. It sits
│   │   │                   #   BELOW solver.hpp — solver includes it and it
│   │   │                   #   includes nothing from the solver — because a
│   │   │                   #   contact is a special case of a row and not the
│   │   │                   #   other way round. Joints are authored in each
│   │   │                   #   body's own frame and cache their impulses as
│   │   │                   #   WORLD vectors, so no cached number outlives its
│   │   │                   #   frame (8.10's bug, designed out).
│   │   │                   #   8.12 added split_swing_twist, swing_angle,
│   │   │                   #   joint_cone and make_cone_twist (a ball-socket
│   │   │                   #   with a swing cone and a twist range; the twist
│   │   │                   #   row is the HALF-WAY axis over cos(phi/2), never
│   │   │                   #   the bone), and FIXED 8.11's position pass: a
│   │   │                   #   satisfied one-sided row takes its speculative
│   │   │                   #   target, or a limit's far stop cancels the near
│   │   │                   #   one's correction.                        [8.12]
│   │   ├── ragdoll.hpp     # ragdoll_part_desc/ragdoll_desc (model space, bind
│   │   │                   #   pose), build_ragdoll + ragdoll_report, and the
│   │   │                   #   seam: part_targets, spawn, steer (kinematic
│   │   │                   #   bodies moved by VELOCITY: the chord, and the
│   │   │                   #   Rodrigues vector the linearised spin inverts),
│   │   │                   #   simulate/animate, add_joints, exclude_pairs,
│   │   │                   #   read_pose, realign_model.                 [8.12]
│   │   │                   #   THE FIRST FILE IN phys/ THAT INCLUDES FROM
│   │   │                   #   anim/: skeleton.hpp, which is maths only. The arrow points at the more general
│   │   │                   #   (5.12's rule): an animation system in a game
│   │   │                   #   with no physics must still compile. EVERY BODY
│   │   │                   #   HAS ONE OWNER: the clip (kinematic, steered) or
│   │   │                   #   the solver (dynamic). Bodies are a contiguous
│   │   │                   #   run of the world's dense array from first_body.
│   │   │   └── gjk.hpp       # gjk_vertex/simplex/gjk_status/gjk_result/gjk_config,
│   │   │                     #   gjk_distance, gjk_intersects, certify, and
│   │   │                     #   cso_support + reduce_simplex — the last two public
│   │   │                     #   because demos/gjk single-steps the loop and the
│   │   │                     #   harness measures the solver directly. 8.6 needed
│   │   │                     #   NO CHANGE to this file: `terminal` was designed
│   │   │                     #   for EPA a lesson before EPA existed. 8.7 did not
│   │   │                     #   change it either — but it DID widen convex.hpp,
│   │   │                     #   which is the first break in that run, and the
│   │   │                     #   break is the lesson: distance and depth are
│   │   │                     #   questions about a SET and answerable from a
│   │   │                     #   support function; a manifold is a question about
│   │   │                     #   a FEATURE and a support function cannot name
│   │   │                     #   one.                                         [8.5]
│   │   │   └── integrate.hpp # motion (position + velocity, and nothing else),
│   │   │                     #   integrator (explicit_euler, semi_implicit_euler,
│   │   │                     #   velocity_verlet), integrate() x2, apply_drag,
│   │   │                     #   damping_factor, natural_frequency,
│   │   │                     #   max_stable_step, spring_energy, shadow_energy,
│   │   │                     #   area_factor, name_of.
│   │   │   └── rigid_body.hpp # rigid_body (HAS a motion, does not BE one),
│   │   │                     #   body_kind (dynamic/kinematic/FIXED — `static` is
│   │   │                     #   a keyword), body_id, mass_of/set_mass,
│   │   │                     #   add_force/add_impulse/add_acceleration,
│   │   │                     #   terminal_speed_damped/_dragged, free_fall_time,
│   │   │                     #   time_scale_for_length_scale, frame_report/
│   │   │                     #   inspect_frame, place_in_parent, step_report,
│   │   │                     #   body_world, make_dynamic/_fixed/_kinematic.
│   │   │                     #   THE ACCUMULATOR IS THE DESIGN and it shows up as
│   │   │                     #   an ABSENCE: no force-generator list, no visitor,
│   │   │                     #   no registry. Forces add because F = ma is linear
│   │   │                     #   in F, so anything that wants to push does `+=`.
│   │   │                     #   BODIES ARE WORLD SPACE, FULL STOP. body_world has
│   │   │                     #   no concept of a parent; inspect_frame exists so
│   │   │                     #   that rule is a MEASURABLE claim and is called by
│   │   │                     #   nothing. A parent scaled by 2 makes a body fall
│   │   │                     #   at 2 g, and a spinning one is non-inertial in a
│   │   │                     #   way no matrix can report.                  [8.2]
│   │   │                     #   THE GENERAL STEPPER IS A TEMPLATE over the
│   │   │                     #   acceleration callable and stays in the header on
│   │   │                     #   purpose: std::function costs +65% and the
│   │   │                     #   non-template overload in the .cpp is SLOWER than
│   │   │                     #   the template, because it cannot inline across
│   │   │                     #   libengine.a. POSITION-ONLY accel, because the
│   │   │                     #   area-preserving property is about forces from a
│   │   │                     #   potential; drag gets apply_drag instead.
│   │   │                     #   NO implicit_euler ENUMERATOR, and the enum says
│   │   │                     #   why: a root find per step, det = 1/(1 + h^2 w^2),
│   │   │                     #   so costly AND lossy.
│   │   ├── audio/          # THE FIRST SUBSYSTEM WHOSE OUTPUT YOU CANNOT     [7.8]
│   │   │                   #   LOOK AT, and the first with a thread the engine
│   │   │                   #   does not own. Three headers, ~1,000 lines.
│   │   │   ├── sound.hpp     # sound (interleaved f32), wav_report, load_wav,
│   │   │   │                 #   make_tone, make_noise, apply_fade, measure.
│   │   │   │                 #   THE ASSET HALF of the split: immutable, shared,
│   │   │   │                 #   and with no idea it is playing
│   │   │   ├── spatial.hpp   # listener, listener_from, falloff (4 laws), emitter,
│   │   │   │                 #   stereo_gain, spatial_result, attenuation, pan_of,
│   │   │   │                 #   pan_constant_power/pan_linear, spatialise,
│   │   │   │                 #   amplitude_to_db/db_to_amplitude. A LISTENER IS A
│   │   │   │                 #   TRANSFORM — the first use of a placement for
│   │   │   │                 #   something that is not drawing
│   │   │   └── mixer.hpp     # voice (INCOMPLETE — phantom tag for voice_id),
│   │   │                     #   mixer_config, voice_params, mixer_report, mixer.
│   │   │                     #   THE ONLY public header that includes SDL_audio.h,
│   │   │                     #   and it has no choice: SDL's callback is a member
│   │   │                     #   and its declaration needs SDLCALL. mix_into() is
│   │   │                     #   PUBLIC so that it can be measured, and
│   │   │                     #   open_offline() builds the voice table with NO
│   │   │                     #   DEVICE — which is what makes the real-time path
│   │   │                     #   a pure function you can diff
│   │   ├── anim/           # SKELETONS, THE SURFACES THEY BEND, AND THE
│   │   │                   #   RECORDED MOTION THAT DRIVES THEM         [7.6-7.7]
│   │   │   ├── clip.hpp      # keyframe<T>, joint_track, clip, track_cursor,   [7.7]
│   │   │   │                 #   clip_report; sampling, wrapping, cross-fading,
│   │   │   │                 #   canonicalising and reduction. A CLIP IS A
│   │   │   │                 #   FUNCTION FROM TIME TO POSE and has NO STATE:
│   │   │   │                 #   the playhead and the search cursor live in the
│   │   │   │                 #   caller, because a clip is an ASSET and two
│   │   │   │                 #   characters must be able to play it out of phase
│   │   │   ├── skeleton.hpp  # joint, skeleton, skeleton_report; the bind pose,
│   │   │   │                 #   its inverse, and pose -> palette. A skinning
│   │   │   │                 #   matrix is model_from_joint(posed) *
│   │   │   │                 #   joint_from_model(bind) — SAME JOINT, TWO TIMES
│   │   │   └── skin.hpp      # skin_influence (4 joints, 4 weights — a GPU
│   │   │                     #   register's width), skinned_mesh, validation,
│   │   │                     #   and linear blend skinning for POINTS and for
│   │   │                     #   DIRECTIONS, which differ by one column
│   │   ├── ui/             # TOOLING UI. Never gameplay UI (§4, binding)   [5.11]
│   │   │   └── debug_ui.hpp  # the Dear ImGui lifecycle + the capture flags.
│   │   │                     #   Does NOT include <imgui.h>: owning the
│   │   │                     #   lifecycle and speaking the widget language
│   │   │                     #   are different jobs
│   │   └── gfx/            # everything from §2.1's gfx/, plus four new headers:
│   │       ├── projector.hpp     # near_mode, projector, screen_point, to_clip,
│   │       │                     #   to_pixel, screen_from_clip. Its own header
│   │       │                     #   because soft_renderer AND debug_draw need it
│   │       ├── scene.hpp         # trs_order, model_matrix, scene_object —
│   │       │                     #   the type BOTH renderers consume
│   │       ├── renderable.hpp    # the COMPONENT half of scene_object, and
│   │       │                     #   collect_renderables(). Keeps scene.hpp's
│   │       │                     #   5.1 promise that "Module 5's ECS replaces the
│   │       │                     #   struct with components". In gfx/ and not ecs/
│   │       │                     #   because the arrow points at the more general;
│   │       │                     #   FORWARD-DECLARES ecs::registry               [5.12]
│   │       ├── soft_renderer.hpp # the CPU pipeline: raster_triangle,
│   │       │                     #   projection_scratch, camera_view,
│   │       │                     #   render_options, collect_stats,
│   │       │                     #   collect_triangles / sort / draw_triangles
│   │       ├── debug_lines.hpp   # THE QUEUE: world-space segments +         [5.11]
│   │       │                     #   lifetimes, bounded and counted. INCLUDES
│   │       │                     #   NOTHING THAT CAN DRAW — that list is the
│   │       │                     #   interface, and it is what lets a physics
│   │       │                     #   system queue without compiling a renderer
│   │       ├── debug_draw.hpp    # line3, draw_mesh, draw_axes3, show_depth,
│   │       │                     #   count_differences — grouped by PURPOSE.
│   │       │                     #   + draw_debug_lines(): the SOFTWARE BACKEND
│   │       │                     #   for the queue above, four lines long   [5.11]
│   │       ├── gltf.hpp          # gltf_status/report, gltf_material_desc,    [6.6]
│   │       │                     #   gltf_primitive, gltf_scene_data,
│   │       │                     #   parse_gltf / load_gltf. DESCRIPTIONS, not
│   │       │                     #   handles — see §"Assets, and the second format"
│   │       ├── bounds.hpp        # aabb, transformed(). Header-only, and it     [6.8]
│   │       │                     #   arrived at its FOURTH call site. The
│   │       │                     #   inside-out default is the identity element
│   │       │                     #   for expand(), which deletes the
│   │       │                     #   first-vertex branch from every caller.
│   │       │                     #   + sphere, bounds_of, bounding_sphere  [6.16]
│   │       │                     #   — the promise this header made to 6.16
│   │       │                     #   BY NAME, redeemed. The sphere is the LOOSE
│   │       │                     #   conversion on purpose: as a PRE-test it may
│   │       │                     #   only ever produce false keeps
│   │       ├── frustum.hpp       # plane, frustum, frustum_of, classify,      [6.16]
│   │       │                     #   intersects (aabb and sphere), cull_report,
│   │       │                     #   cull_visible. THE PLANES COME FROM THE ROWS
│   │       │                     #   OF clip_from_world, never from a camera —
│   │       │                     #   so there is no second copy of the truth to
│   │       │                     #   drift from the matrix the renderer uses.
│   │       │                     #   k_near_z / k_far_z, because <windows.h>
│   │       │                     #   makes `near` and `far` macros
│   │       ├── instancing.hpp    # gpu_instance (112 B — exactly             [6.16]
│   │       │                     #   sizeof(object_uniforms), and a
│   │       │                     #   static_assert keeps it so), instance_of,
│   │       │                     #   describe_instances, batch_key,
│   │       │                     #   instance_batch, batch_report,
│   │       │                     #   batch_instances. INCLUDES gpu_scene.hpp,
│   │       │                     #   which is why gpu_scene.hpp forward-declares
│   │       │                     #   `struct instance_batch` rather than
│   │       │                     #   including back — and why render_batched
│   │       │                     #   takes a pointer and a count, not a span
│   │       ├── frame_graph.hpp    # frame_graph, fg_texture, fg_texture_desc,  [6.17]
│   │       │                     #   fg_init, fg_use, fg_pass_context,
│   │       │                     #   fg_execute_fn, dump_frame_graph. THE FRAME
│   │       │                     #   IS DECLARED, NOT ASSEMBLED: a resource is a
│   │       │                     #   VARIABLE WITH VERSIONS (hdr@1 is what the
│   │       │                     #   scene pass made, hdr@2 what the skybox made
│   │       │                     #   from it), each produced by exactly one pass.
│   │       │                     #   From `discard_write` / `clear` / `keep` the
│   │       │                     #   compiler derives the ORDER, the LOAD op, the
│   │       │                     #   PRODUCER's STORE op and the LIFETIME. It
│   │       │                     #   includes only gpu_device + gpu_texture:
│   │       │                     #   nothing about a bloom, a shadow or a scene
│   │       │                     #   reaches this file, which is why culling the
│   │       │                     #   bloom needs no flag in it
│   │       ├── font.hpp          # glyph, kern_pair, font_atlas,               [6.18]
│   │       │                     #   font_bake_options, font_status, bake_font,
│   │       │                     #   load_font, next_codepoint, glyph_quad,
│   │       │                     #   text_layout_options, text_metrics,
│   │       │                     #   layout_text, measure_text. A GLYPH IS A
│   │       │                     #   COVERAGE MASK, not a picture — one byte per
│   │       │                     #   texel, never _SRGB, averaged with no
│   │       │                     #   transfer function. Names NO third-party type
│   │       ├── overlay.hpp       # overlay_vertex, overlay_batch,              [6.18]
│   │       │                     #   overlay_blend, composite_overlay,
│   │       │                     #   apply_stem_darkening. 2D quads in PIXEL
│   │       │                     #   SPACE (origin top-left, +y down) plus the
│   │       │                     #   software compositor. MENTIONS NEITHER
│   │       │                     #   RENDERER, which is what lets hello_cube have
│   │       │                     #   text with no GPU device and what lets one
│   │       │                     #   batch be rendered both ways and compared
│   │       ├── gpu_overlay.hpp   # gpu_overlay + its two uniform blocks.       [6.18]
│   │       │                     #   THE ONLY ONE OF THE THREE THAT NAMES
│   │       │                     #   SDL_GPU. Handed a render pass rather than
│   │       │                     #   beginning one — gpu_post_stack's split, for
│   │       │                     #   the same reason: the target is the
│   │       │                     #   swapchain and the swapchain's pass belongs
│   │       │                     #   to the frame
│   │       ├── shadow.hpp        # light_camera, fit_directional, shadow_bias,  [6.8]
│   │       │                     #   shadow_settings, slope_from_cosine,
│   │       │                     #   pcf_reach_texels, slope_scaled_bias,
│   │       │                     #   quantisation_bias, shadow_map. INCLUDES
│   │       │                     #   raster.hpp — a shadow pass IS a rasterizer
│   │       │                     #   pass, which is why raster.hpp only
│   │       │                     #   forward-declares shadow_map back
│   │       └── gpu_shadow.hpp    # gpu_shadow_map: a depth-only pipeline        [6.8]
│   │                             #   (num_color_targets = 0), a SAMPLED depth
│   │                             #   texture, a comparison sampler, its own
│   │                             #   render pass, and fill_uniforms() so the two
│   │                             #   renderers cannot disagree about a bias
│   └── src/                # ---- PRIVATE. 68 sources; no demo can name this path ----
│       ├── phys/           # integrate.cpp [8.1], rigid_body.cpp [8.2],
│       │                   # inertia.cpp [8.3], shape.cpp [8.4],
│       │                   # collide.cpp [8.4], gjk.cpp [8.5], epa.cpp [8.6],
│       │                   # manifold.cpp [8.7], broadphase.cpp [8.8],
│       │                   # solver.cpp                      [8.9, 8.10, 8.11]
│       │                   # constraint.cpp                              [8.11]
│       │                   # ragdoll.cpp                                 [8.12]
│       │                   #   gjk.cpp is the only one of these with no header of
│       │                   #   its own shape knowledge: it includes gjk.hpp, which
│       │                   #   includes convex.hpp, and nothing in it names a box.
│       │                   #   Everything that is NOT a template: the constant-
│       │                   #   acceleration overload (the gravity path), the two
│       │                   #   drag helpers, and the four diagnostics. Nothing in
│       │                   #   it is hot, and 8.1 section I.4 measures what
│       │                   #   crossing the archive boundary costs — 1.222 ns
│       │                   #   against the header template's 0.963
│       ├── audio/          # mixer.cpp, sound.cpp, spatial.cpp                [7.8]
│       │                   #   THE FOURTH NEW DIRECTORY SINCE THE REFACTOR and
│       │                   #   the first whose contents never touch a pixel. NOT
│       │                   #   under asset/, although sound.cpp loads a file:
│       │                   #   5.5 predicted this directory by name and predicted
│       │                   #   the wrong home for it. The asset system FINDS,
│       │                   #   CACHES and OWNS; decoding a WAV into floats is a
│       │                   #   subsystem's own business, exactly as gfx/image.cpp
│       │                   #   decodes a PNG and lives under gfx/.
│       │                   #   mixer.cpp is the ONLY file in the library that
│       │                   #   runs on a thread SDL owns: no allocation, no
│       │                   #   logging, no unbounded wait, one mutex whose
│       │                   #   compromise is written down rather than hidden
│       ├── anim/           # clip.cpp [7.7], skeleton.cpp, skin.cpp          [7.6]
│       │                   #   A NEW DIRECTORY, and the argument is asset/'s in
│       │                   #   5.5 and ui/'s in 5.11: animation is not a
│       │                   #   graphics subsystem. Neither file mentions a
│       │                   #   framebuffer, a pipeline or a colour, and a
│       │                   #   character keeps moving when nobody is looking.
│       │                   #   skeleton.cpp runs once per JOINT and skin.cpp
│       │                   #   once per VERTEX — 2.34% / 97.66% of the pair.
│       │                   #   clip.cpp runs once per CHANNEL and is smaller
│       │                   #   than either: 0.159 us for 23 joints, two thirds
│       │                   #   of compose+palette and noise beside skinning
│       ├── core/           # actions [5.10], clock, fixed_step, input, log, profile
│       ├── platform/       # platform.cpp, app.cpp                            [5.2]
│       ├── ui/             # debug_ui.cpp — THE ONLY engine TU that          [5.11]
│       │                   #   includes <imgui.h>. That containment is what
│       │                   #   made a PUBLIC dependency acceptable
│       └── gfx/            # …+ soft_renderer.cpp, debug_draw.cpp,
│                           #   debug_lines.cpp [5.11], shadow.cpp and
│                           #   gpu_shadow.cpp [6.8]; image.cpp is the
│                           #   ONE unit that contains stb_image + save_ppm,
│                           #   and gltf.cpp [6.6] is the ONE unit that
│                           #   contains cgltf. Same containment, same reason.
│                           #   frustum.cpp + instancing.cpp [6.16] are TUs for
│                           #   draw_order.cpp's stated reason: each is a real
│                           #   algorithm over a span, and frustum.cpp's six sign
│                           #   conventions must have exactly one home.
│                           #   frame_graph.cpp [6.17] is a TU because `compile`
│                           #   is four real passes over two arrays and nothing
│                           #   that merely DECLARES a pass should recompile when
│                           #   the scheduler changes.
│                           #   font.cpp [6.18] is the ONE unit that contains
│                           #   stb_truetype — the third instance of the same
│                           #   containment rule, after image.cpp and gltf.cpp.
│                           #   overlay.cpp + gpu_overlay.cpp [6.18] are separate
│                           #   TUs because the FIRST knows nothing about either
│                           #   renderer and the second is entirely SDL_GPU:
│                           #   collapsing them would put an SDL_GPU dependency
│                           #   on the software path and end the two-renderer
│                           #   comparison that 6.18 §11 rests on
├── demos/                  # executables; link engine, include ONLY public headers
│   ├── CMakeLists.txt
│   ├── common/             # demo_common: CONTENT, shared so nothing is transcribed
│   │   ├── demo_scene.hpp  # spin, scene_kind, floor_geometry, model_state,
│   │   ├── demo_scene.cpp  #   texture_set, orbit_camera, build_scene, draw_world,
│   │   │                   #   and write_reference_shot() — the characterization test
│   │   ├── pong.hpp        # the Module 1 game, finally in a directory for games
│   │   └── pong.cpp
│   ├── collector/          # THE CHECKPOINT GAME — the only program here whose
│   │   └── main.cpp        #   requirements were not chosen to flatter the engine.
│   │                       #   Links engine::engine and NOT demo_common. Its
│   │                       #   --shot runs 240 deterministic steps and prints four
│   │                       #   numbers, which makes it a characterization test for
│   │                       #   the ECS, the hierarchy and the pools               [5.12]
│   ├── gjk/main.cpp        # THE MINKOWSKI DIFFERENCE, SEARCHED               [8.5]
│   │                       #   Two panels. Left, the world; right, `A ⊖ B`
│   │                       #   OUTLINED BY TRACING ITS OWN SUPPORT FUNCTION —
│   │                       #   a convex set's shadow is convex, and its support
│   │                       #   function is the original's composed with the
│   │                       #   transpose of the projection, so one loop draws a
│   │                       #   box, a capsule, a hull or a difference set.
│   │                       #   Drawing the support POINTS instead — the obvious
│   │                       #   first attempt — draws almost nothing for a
│   │                       #   polytope: 900 directions on a box land on 8
│   │                       #   corners. [S] steps the search one iteration at a
│   │                       #   time, which is the one reason this demo drives
│   │                       #   the loop itself through `cso_support` and
│   │                       #   `reduce_simplex` rather than calling
│   │                       #   `gjk_distance`: a visualiser needs the iteration,
│   │                       #   not the answer. Preset 3 is the lesson — two
│   │                       #   boxes on one floor, and the simplex never leaves
│   │                       #   the plane y = 0, which is why that arrangement
│   │                       #   can never produce a tetrahedron for 8.6's EPA.
│   ├── epa/main.cpp        # THE BALLOON IN THE DARK ROOM                    [8.6]
│   │                       #   Two panels again, and the LEFT one carries the
│   │                       #   new idea: a GHOST of the second shape drawn where
│   │                       #   the current answer would push it, so an
│   │                       #   unconverged lower bound is a crate still buried
│   │                       #   in a wall rather than a number that is 60% of
│   │                       #   another number. The right panel is the gjk
│   │                       #   demo's with two differences that say everything:
│   │                       #   the origin cross is INSIDE the outline, and the
│   │                       #   pink vector only ever gets LONGER. Builds its own
│   │                       #   polytope from `cso_support` for the same reason
│   │                       #   the gjk demo drove its own loop, and prints the
│   │                       #   real `epa_penetration`'s answer beside its own so
│   │                       #   the two can be seen to agree. [F] drops the flood
│   │                       #   fill from the visibility test and the horizon
│   │                       #   tears on screen.
│   ├── broadphase/main.cpp # THE CHEAP QUESTION, ASKED OF EVERYTHING          [8.8]
│   │                       #   THE FIRST COLLISION DEMO WHOSE SUBJECT IS THE
│   │                       #   SCENE rather than a pair. Seen from directly
│   │                       #   above, so the lattice is visible, with each cell
│   │                       #   shaded by C(n,2) rather than by occupancy —
│   │                       #   because the picture is meant to show where the
│   │                       #   WORK is, and the work is quadratic. [B] runs
│   │                       #   brute_force_pairs beside the grid every frame and
│   │                       #   colours the verdict, so the contract is checked
│   │                       #   live at 60 Hz against a function that cannot be
│   │                       #   wrong. [Up]/[Down] move entries and bucket tests
│   │                       #   by orders of magnitude and leave the PAIRS line
│   │                       #   untouched. [T] adds the ground plate, [Y] toggles
│   │                       #   the guard. Preset 4 — mixed sizes — is the one to
│   │                       #   look at: one proxy in seventeen is metres across
│   │                       #   and each draws a visible STAR of candidate pairs.
│   ├── manifold/main.cpp   # ONE CONTACT IS NOT A CONTACT                     [8.7]
│   │                       #   Two panels, and the RIGHT one carries the new
│   │                       #   idea: the contact seen FACE ON, in the reference
│   │                       #   face's own plane, with [S] applying one
│   │                       #   Sutherland-Hodgman side plane per press — the
│   │                       #   plane dashed, its outward normal a stub. The left
│   │                       #   panel draws what a solver needs and 8.6 could not
│   │                       #   give it: the contact POINTS and the closed loop
│   │                       #   of the support polygon they span, with its AREA,
│   │                       #   because the area is the number that decides
│   │                       #   whether a body can stand up. Preset 2 plus [Z] is
│   │                       #   the lesson in one gesture: tilt the crate and
│   │                       #   four points become two, the polygon collapses to
│   │                       #   a line, its area goes to zero. Preset 4 is the
│   │                       #   honest one — a ball gets ONE point and should.
│   ├── impulse/main.cpp    # THE FIRST DEMO IN WHICH SOMETHING MOVES BECAUSE
│   │                       #   THE PHYSICS SAID SO                          [8.9]
│   │                       #   Five collision demos before it each ANSWERED a
│   │                       #   question and changed nothing. Four scenes, each
│   │                       #   one measurement made watchable: the bounce with
│   │                       #   e^(2n)*h0 drawn as ghost peaks, the slope, the
│   │                       #   roll to 5/7, and the creep that is 8.10's reason
│   │                       #   to exist. The inset at bottom right is Coulomb's
│   │                       #   clip drawn as itself — press [F] and the square
│   │                       #   appears around the disc, and the extra area in
│   │                       #   its corners IS the 43% of anisotropy.
│   ├── ragdoll/main.cpp    # A CHARACTER ANIMATED, DROPPED, AND ANIMATED AGAIN
│   │                       #                                              [8.12]
│   │                       #   The trip (a jog steered into crates that fell
│   │                       #   asleep first — 8.12's kinematic-wake fix is why
│   │                       #   they move — the handoff, three seconds down, and
│   │                       #   a realigned slerp return), the hang from one
│   │                       #   hand ([U] sub-steps), and a pile of five. Draws
│   │                       #   capsule silhouettes in an orthographic 3/4 view,
│   │                       #   and the skinned skeleton through them from the
│   │                       #   clip or from read_pose, passengers included.
│   ├── joints/main.cpp     # RODS, ROPES, SOCKETS AND HINGES, IN THE LOOP THAT
│   │                       #   STACKS CRATES                              [8.11]
│   │                       #   Four scenes, each a measurement made watchable:
│   │                       #   pendulums (a plank and a ball with the same
│   │                       #   pivot-to-centre distance drift apart — the
│   │                       #   parallel axis, heard — and a rope goes slack),
│   │                       #   a turnstile and a gate KICKED into their stops
│   │                       #   (the gate bounces at rho^8; a motor would have
│   │                       #   hidden it), a weighted chain with [U] for
│   │                       #   sub-steps, and a hinged bridge carrying crates.
│   │                       #   Uses collision_filter, because the narrow phase
│   │                       #   is the caller's.
│   ├── stack/main.cpp      # A PILE OF CRATES, AND THE FOUR KNOBS THAT DECIDE
│   │                       #   WHETHER IT IS A PILE                        [8.10]
│   │                       #   `impulse` could draw the whole of one contact;
│   │                       #   this one cannot, because its subject is what a
│   │                       #   hundred contacts do to EACH OTHER — so the
│   │                       #   interesting quantities are counts and colours
│   │                       #   rather than vectors. A CRATE'S COLOUR IS ITS
│   │                       #   ISLAND, which is the one thing about the
│   │                       #   partition invisible in the geometry, and [B]
│   │                       #   recomputes the labels with fixed bodies bridging
│   │                       #   — labels ONLY, because letting the bug into the
│   │                       #   solve would change what the demo is showing. A
│   │                       #   sleeping crate is drawn in a DIMMED version of
│   │                       #   its island colour rather than a flat grey, since
│   │                       #   a settled scene otherwise loses its partition
│   │                       #   exactly when a reader wants to see it.
│   ├── collide/main.cpp    # FIFTEEN BARS, AND THE ONE THAT CROSSES ZERO      [8.4]
│   │                       #   All fifteen SAT candidates as bars against a zero
│   │                       #   line, so the reader watches the verdict flip at
│   │                       #   the instant the FIRST one crosses. [F] keeps the
│   │                       #   six face normals and throws the nine cross
│   │                       #   products away, turning two visibly separated
│   │                       #   planks red. A negative claim — these fifteen
│   │                       #   found no gap, therefore no direction has one — is
│   │                       #   the hardest kind to believe from a statement.
│   ├── bodies/main.cpp     # THREE MASSES, THREE WAYS TO FALL                [8.2]
│   │                       #   World-space trajectories, not phase space, because
│   │                       #   the claim it settles is one a player could see:
│   │                       #   [M] cycles none / damping / drag and the first TWO
│   │                       #   produce the same picture. EACH BODY DRAWS ONE DASH
│   │                       #   IN THREE — three solid curves on top of one another
│   │                       #   are indistinguishable from one, so the picture that
│   │                       #   proves they agree would look exactly like a bug
│   │                       #   that lost two bodies.
│   │                       #   Its right panel is the only place in this repo that
│   │                       #   integrates OUTSIDE world space, on purpose: four
│   │                       #   bodies dropped from the SAME WORLD POINT in four
│   │                       #   parent frames, of which one is falling.
│   ├── integrate/main.cpp  # THREE RULES, ONE SPRING, DRAWN IN PHASE SPACE   [8.1]
│   │                       #   — position across, velocity up — because watched
│   │                       #   as a bouncing dot all three look correct for the
│   │                       #   first few seconds, which is exactly why the bug in
│   │                       #   one of them ships. Red spirals out and leaves;
│   │                       #   green and violet trace one closed curve.
│   │                       #   RUNS fixed_step NESTED INSIDE THE APP'S OWN: the
│   │                       #   app's rate is a platform decision and the
│   │                       #   simulation's rate is the variable under study, so
│   │                       #   [Up]/[Down] change what physics does without
│   │                       #   changing how often the screen updates.
│   │                       #   Carries a 20-line Cohen-Sutherland clip, because
│   │                       #   engine::draw_line deliberately does not clip (2.1
│   │                       #   says so in a comment) and a diverging red spiral
│   │                       #   would otherwise be drawn across the energy chart
│   │                       #   next door. That exercise came due 70 lessons later.
│   │                       #   VIEW SCALE IS CAPPED at three amplitudes: without
│   │                       #   the cap the camera follows the broken curve out and
│   │                       #   every correct one collapses to a dot
│   ├── audio/main.cpp      # A LISTENER, THREE EMITTERS, AND A MAP OF WHY   [7.8]
│   │                       #   IT SOUNDS LIKE THAT. The first demo here whose
│   │                       #   OUTPUT IS NOT THE PICTURE: the map is an
│   │                       #   explanation of a result that cannot be looked at.
│   │                       #   Rings at 2/4/8/16/32 m from the listener, so each
│   │                       #   one is a halving. --shot uses open_offline, so a
│   │                       #   headless run opens NO DEVICE and is silent and
│   │                       #   deterministic. No assets, no shaders, no .wav —
│   │                       #   every sound is synthesised with make_tone
│   ├── gimbal/main.cpp     # THREE RINGS AND A NUMBER. One aircraft, three hoops  [7.1]
│   │                       #   whose planes contain their own pivot axles, and a
│   │                       #   live |det J| beside them. [E] drives the weakest
│   │                       #   knob combination so the response can be watched
│   │                       #   dying. Its one API finding: debug_lines has no
│   │                       #   ellipse(), so the rings are built from line()
│   ├── sandbox/main.cpp    # Lessons 2.1–4.9 on [Tab] and four flags. 5,625 lines.
│   │                       #   Uses engine::platform; keeps its own main() ON PURPOSE
│   ├── pong/main.cpp       # Lesson 1.8's game, on engine::app. 87 code lines,
│   │                       #   no main, no SDL_Init, no loop                  [5.2]
│   ├── hello_cube/main.cpp # public headers only. THE ACCEPTANCE TEST for the API
│   └── ecs_swarm/main.cpp  # 154 entities, THREE LEVELS, NO SCANCODES,   [5.8-5.11]
│                           #   and now DRAWING ITS OWN TREE: 152 debug lines
│                           #   (96 ring + 32 moons + 24 waypoints) queued by a
│                           #   function whose signature has no framebuffer in
│                           #   it, plus two ImGui panels. The acceptance test
│                           #   for the ECS: 24 entities are invisible because
│                           #   they LACK a geometry component, and [F] drifts
│                           #   the sun so everything follows it
└── tools/                  # editor, asset cooker (Module 9). Not yet.
```

**The boundary is the include path, not the style guide.** One property does it:

```cmake
target_include_directories(engine PUBLIC include PRIVATE src)
```

`include/` holds exactly one directory, `engine`, so from outside the only spellings that resolve
are `<engine/gfx/raster.hpp>` and its siblings — and `#include "gfx/raster.hpp"`, which every file
in this repository used until Lesson 5.1, resolves to nothing at all. A boundary maintained by
discipline lasts until the first time somebody is in a hurry; this one is maintained by a compiler.

Three more properties finish the job:

| line | what it buys |
|---|---|
| `add_library(engine::engine ALIAS engine)` | a name with `::` cannot be mistaken for a file, so a typo fails at *configure* time rather than becoming `-lengine` at link time |
| `target_link_libraries(engine PUBLIC SDL3::SDL3)` | contagious, and it has to be: our public headers name `Uint32`, `SDL_Window*`, `SDL_Event`. **An admission, not a choice** |
| `target_include_directories(engine PRIVATE ${stb_SOURCE_DIR})` | stb stops at the boundary. One TU includes it; no demo can see it |

**Which side does a file go on?** One question, and it settled every case but four:

> Would a different game, one we have not written, want it?

Yes → engine. No, it exists to show/teach/drive *this* program → demo. Applied to `next_cull()`,
which advances a cull mode so the `[U]` key can cycle it: an engine shipping that function is
shipping this demo's key bindings to everybody who links it. Demo.

`demos/common/` looks like a second engine and is kept honest by one rule: **nothing in it may be
needed by a shipped game.** The moment something is, it is not demo content — it is an engine
feature nobody has named yet.

**What is still on the wrong side**, recorded rather than quietly left (lesson §7):

| leftover | why it is wrong | paid in |
|---|---|---|
| `cull_choice` is applied twice | `collect_triangles` reads one of its four values; `draw_triangles` applies the rest via `fill_style` | 6.5, the material system |
| ~~SDL is in the public API~~ | **Settled in 5.2, and the answer is "keep it".** A wrapper that hides a library you have no intention of replacing is cost with no benefit. The platform layer owns *lifetime*, not *vocabulary* | 5.2 — closed |
| `render_options` ships teaching switches | `trs_order::tsr` exists so a lesson can show a bug | exercise 5.1.4, honestly never |
| `demos/sandbox/main.cpp` is 5,625 lines | the application layer now exists and Pong has left; four screens remain welded together | one at a time, as later lessons need them |
| no window-resize handling | `platform` makes a resizable window and ignores `SDL_EVENT_WINDOW_RESIZED` | 6.x, when render targets care |
| `surface::gpu` hands you a window and stops | device, swapchain and present target are still the program's job | a future `gpu_app` |

### 2.2b The platform and application layers (Lesson 5.2)

Two arrangements, and the engine supports both because they are not interchangeable:

| | you write | the loop lives in | used by |
|---|---|---|---|
| **library** | `main()`, `while (plat.running())` | your code | `sandbox` |
| **framework** | 7 overrides + `ENGINE_MAIN(T)` | SDL, via the main callbacks | `pong`, `hello_cube` |

**The rule that keeps them one engine: `engine::app` is implemented *on* `engine::platform`,
never beside it.** Everything the framework offers is reachable from the library path, and
`app.cpp` does nothing a hand-written `main()` could not. `verify_52` §E renders six frames down
each path and compares the framebuffers byte for byte. If that ever diverges, the framework has
started growing a private engine inside itself.

`platform` owns **lifecycle and order**, not vocabulary. `window()` returns `SDL_Window*`;
`handle()` takes an `SDL_Event`. The ladder — `SDL_Init` → window → renderer → texture, and the
exact mirror on the way down — is what it exists for, together with the fact that every failure
path and the destructor all funnel through one `stop()`.

The surface is chosen in `app_config`, before anything exists, because by then it is too late:

| `surface` | window | renderer | `SDL_INIT_VIDEO` | for |
|---|---|---|---|---|
| `renderer` | yes | yes + streaming texture | yes | Module 1's presentation path |
| `gpu` | yes | **no** | yes | `gpu_device::create` — Lesson 4.2's claim rule |
| `headless` | no | no | **no** | `--shot`, CI, any machine with no display |

`headless` not initialising video is the point, not an optimisation: `SDL_Init(SDL_INIT_VIDEO)`
*fails* on a build server, and the pre-5.2 `sandbox --shot` called it and never used it.

**Entry point.** `<engine/platform/main.hpp>` defines `SDL_MAIN_USE_CALLBACKS` and includes
`<SDL3/SDL_main.h>`, which emits a *non-inline* `SDL_main()` plus the platform entry point. So:
one translation unit per program, and that file must not define `main()`. It is why the entry
point cannot live in `libengine.a` — an entry point is not a library's to own — and why it is the
one public header deliberately absent from `engine.hpp`. Lesson 5.12's umbrella lint has it as its
single hard-coded exception, and `verify_512` §F asserts its **absence** — because a completeness
check with no exceptions would have "fixed" the one thing that was right.

### 2.2c Diagnostics: logging, assertions, errors (Lesson 5.3)

**Two axes, never one.** A *category* says who is speaking; a *priority* says how much it
matters. Collapsing them — a `LOG_ERROR` **category** — means you can never ask for "errors from
the GPU but not from assets".

Five categories, based at `SDL_LOG_CATEGORY_CUSTOM` (never at a literal; SDL reserves everything
above that enumerator for applications). A `static_assert` ties the name table to the enum.

| Category | Who |
|---|---|
| `log_core` | clock, input, fixed_step, profiler |
| `log_platform` | window, renderer, the app lifecycle |
| `log_gfx` | the CPU rasterizer — **no users yet**; it never logs |
| `log_gpu` | SDL_GPU: devices, pipelines, uploads |
| `log_asset` | files: images, OBJ, the asset system |

Six levels, and the meanings are promises the engine's 92 call sites obey. The mechanical test
between the two that get confused: **did the operation happen?** If yes and we carried on, it is
`warn`; if no and the caller is being told so, it is `error`.

**Who logs a failure: the deepest point that knows why.** Callers propagate silently and add only
the *consequence*. Chosen over "the caller decides the level" because this rule's cost is visible
(a recovering caller gets an error line it did not deserve) and the other's is not (detail lost at
every layer boundary).

**The default costs nothing, and that is the argument against a wrapper.** SDL's documented
default table is `app=info, assert=warn, test=verbose, *=error`, so every category SDL does not
know about — every one of ours — is silent unless something failed. Moving the engine off
`SDL_LOG_CATEGORY_APPLICATION` was the whole change. Demos keep `SDL_Log` deliberately: a demo
*is* the application.

`--log SPEC` works on every program (`platform::start` reads it first; `app_runner::init` fills in
`argv` when the app left it null). The grammar is SDL's own `SDL_LOGGING` grammar, so two things a
person might type do not need two mental models. **The parser validates everything before applying
anything** — a rejected spec is a no-op.

#### Assertions

One question separates an assertion from an error: *could a correct program, on a working machine,
encounter this?* Yes → an error, returned, shipping. No → an assertion, which may be compiled out.

| Macro | Survives | For |
|---|---|---|
| `ENGINE_ASSERT` | debug only | the default; condition must have no side effects |
| `ENGINE_CHECK` | every build | only where continuing is worse than stopping |
| `ENGINE_VERIFY` | expression always runs | an operation whose result is an invariant |

**SDL decides its assertion level from `__OPTIMIZE__`, not `NDEBUG`.** So `-O2` alone disables
`SDL_assert`, with no `-DNDEBUG` in sight, while our log floor (which keys off `NDEBUG`) is still
fully compiled in. `-O0 -DNDEBUG` is the exact inverse. Measured in `scratch/measure_53.py`; this
mismatch produced a real bug in `ENGINE_VERIFY`'s first version.

#### Error reporting

The engine converged on one shape twice, independently, before anybody named it: a **status enum**
that names the failure, the **facts you ask for next**, and a **`bool ok()`**. `obj_report` (3.5),
`gpu_report` (4.2), `image_report` (5.3).

| Rule | |
|---|---|
| status + report + `ok()` | when a caller might branch on the failure, or there are diagnostics worth having on success |
| a bare `bool` | when there is genuinely nothing more to say (`platform::set_vsync`) |
| per-subsystem status enums | never one engine-wide enum — that becomes a junk drawer |
| no "already logged" contracts | superseded by the who-logs rule above |

`std::expected` is C++23 and deliberately not adopted: the loaders return *facts*, not just
values, and return them whether or not they worked — `expected` has nowhere to put `obj_report`'s
twelve statistics.

### 2.3 Dependency direction

Strictly one-way. Arrows point at what a layer is allowed to know about:

```
demos/ ──► engine public API ──► engine private impl ──► platform (SDL3) ──► OS
tools/ ──►

within the public API, since 5.2:
   demos ──► engine::app ──► engine::platform ──► SDL3
   demos ─────────────────►  engine::platform          (sandbox takes this one)
```

`math` depends on nothing but the standard library — which is exactly why it is the first thing
under test. `core` may not include `gfx`. Nothing in `engine/` may include from `demos/`. A
cycle here is a design error, not an inconvenience to work around.

**Lesson 8.12 added an arrow from `phys/` to `anim/`:** `phys/ragdoll.hpp` includes `anim/skeleton.hpp`. It points that way because a skeleton is joints and bind transforms and includes nothing but maths, while a ragdoll is a physics object built *from* one — 5.12's rule that the arrow points at the more general. The reverse would make every animated character compile the solver. `anim/` still includes nothing from `phys/`.

**Lesson 5.11 added a second, finer-grained direction *inside* `gfx/`, and it is enforced by
include lists rather than by the build:**

```
anybody (physics, ECS, loaders) ──► gfx/debug_lines.hpp   (says what to draw)
                                          ▲
        code that already owns a  ────────┘
        framebuffer               ──► gfx/debug_draw.hpp   (draws it)
```

`debug_lines.hpp` includes `colour`, `mat4`, `vec3` and three standard headers, and **nothing
that can draw**. That list *is* the interface: C++ include dependencies are transitive, so one
renderer type in it would make every physics translation unit compile the framebuffer, the depth
buffer, the projector and the handle system in order to draw a box. It is also why
`wire_mesh()` takes two spans instead of the `mesh` that owns them — `mesh.hpp` drags in
`core/pool.hpp`.

**Lesson 6.15 added the third instance of the same discipline, and this one is a forward
declaration.** `gpu_texture::create_cube` takes a `const cube_map&`, so `gpu_texture.hpp` needs the
*name* and not the definition:

```cpp
namespace engine {
class cube_map;                      // gfx/cubemap.hpp, defined there, included in the .cpp
```

Including `cubemap.hpp` instead would drag `hdr.hpp`, `microfacet.hpp`, `texture.hpp` and
`math/vec2` into every translation unit that wants to make a *depth buffer* — because
`gpu_texture.hpp` is what a depth target comes from too. One translation unit,
`gpu_texture.cpp`, needs the full type, and that is where the include lives. Same rule as
`debug_lines.hpp` above and as `stb_image`'s containment: **an interface's include list is part of
the interface.**

`cubemap.hpp` itself sits in `gfx/` rather than under a new `env/`, and the argument is the one
`gpu_texture.hpp` already makes for keeping depth targets beside sampled ones: *an environment map
is a texture type, not a subsystem.* What is genuinely new about it is the direction of the
indexing — a `cube_map`'s mip levels mean **roughness**, where Lesson 6.10's `mip_chain` levels
mean **screen footprint**. The mechanism is identical and the meaning is not, which is why both
types exist rather than one gaining a flag.

**Dear ImGui is the one dependency that is deliberately PUBLIC** (5.11), against `stb_image`'s
`PRIVATE` (4.7). The distinction is *concept* versus *vocabulary*: stb wraps to one function and
one type, while ImGui's value is four hundred widget calls, and a wrapper around those is a
re-spelling with no content that must be re-spelt for every widget forever. The containment is
therefore a rule about **which code may speak it — tooling only** — rather than a link flag, and
`engine/src/ui/debug_ui.cpp` is the only engine translation unit that includes `<imgui.h>`.

Since Lesson 5.1 this is no longer a rule people follow — it is a rule the build enforces, in two
independent ways. Horizontally, `engine/src` is `PRIVATE`, so a demo cannot name it. Vertically,
`add_subdirectory(engine)` comes before `add_subdirectory(demos)` in the root `CMakeLists.txt`:
swap the two lines and the configure fails, because `engine::engine` would not exist yet when
`demos/` asked to link it.

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
├── shared/
│   ├── course.css          # THE stylesheet — one copy, linked by every page
│   └── course.js           # THE page script — theme, TOC, syntax highlighter
└── _template/
    ├── lesson-template.html   # canonical lesson skeleton (links the shared files)
    ├── apply-shared.py        # authoring-time: verifies every page's shared links
    ├── check-page.js          # authoring-time: browser-side page verification
    ├── check-curriculum.py    # authoring-time: index vs lesson table consistency
    ├── check-builders.py      # authoring-time: every build_NN.py still makes its page
    └── README.md              # authoring & visual style guide
```

`index.html`, `conventions.html` and `math-toolbox.html` are **living pages**, reissued updated
at every module boundary.

### The shared CSS and page script

**There is exactly one copy of each: `docs/shared/course.css` and `docs/shared/course.js`.**
Every page links them. Edit the shared file; no page carries a copy to keep in step.

It was not always so, and the history explains the tooling. The original constraint made each
lesson *individually* self-contained, which forced both blocks to be **duplicated into all 36
pages** — 26.6 KB of CSS and 8.0 KB of script apiece, 18% of the entire `docs/` tree. Duplication
that nothing propagates drifts, and it did: by Lesson 1.2 the trailing `<script>` existed in six
mutually inconsistent versions — three different C++ keyword lists, a CMake highlighter present
in exactly one lesson, and a Windows-batch comment rule present in two. Each was a silent
mis-render, never a crash, which is what let it survive review. `apply-shared.py` was written to
stamp one canonical copy into every page and hold them identical.

The extraction removed the cause rather than managing it. The premise turned out to be checkable
rather than merely assumed: a relative `<link>` and a classic `<script src>` **do** load over
`file://`, verified in Chromium, Firefox *and* WebKit, including the upward `../shared/`
traversal from `docs/lessons/`. So the no-build-step guarantee never depended on inlining.

**What it cost, stated plainly:** a lesson file is no longer portable alone. Copy one out of the
tree and it renders unstyled. The `docs/` directory is now the unit you move, not the file.

**What now needs policing** is the link, not the content — and that failure is *silent*: a wrong
href throws nothing, it just yields an unstyled, inert page that reads as unfinished rather than
broken. The correct relative prefix depends on the page's depth (`shared/…` at `docs/`,
`../shared/…` at `docs/lessons/`), so it breaks by **moving** a page, not by editing one.

`apply-shared.py` therefore kept its marker architecture and changed its job — from copying
content into a region to computing and verifying that region's link:

```
<!-- SHARED-CSS:BEGIN -->    …    <!-- SHARED-CSS:END -->
<!-- SHARED-SCRIPT:BEGIN --> …    <!-- SHARED-SCRIPT:END -->
```

```sh
python3 docs/_template/apply-shared.py           # fix every page's links
python3 docs/_template/apply-shared.py --check   # verify only; exit 1 on drift
```

Three properties matter architecturally. **It is not a build step** — readers never run it and
the published files are static HTML linking static assets, so the "no build tooling" guarantee in
§1 holds; it is an authoring-time tool. **It is region-scoped, not file-scoped**, so a page keeps
its own page-specific JavaScript (Lesson 1.2's key-state widget, and any future interactive
diagram) outside the markers where the stamp cannot reach it. And the **KaTeX loader stays inline
inside the SCRIPT region** — both tags carry SRI hashes and one carries an inline `onload`, so
neither survives being moved into a linked file.

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

**Where the two stages meet, as of Lesson 4.2.** Stage B has started, and the two stages coexist
inside one executable rather than replacing one another:

```
./engine             Stage B — Module 3's scene, drawn by the GPU        (from 4.8)
./engine --software  Stage A — the software demo of Modules 1–3, SDL_Renderer, HUD and all
./engine --probe     Stage B — the GPU probe Lessons 4.2–4.7 were built on
./engine --gpu       an alias for --probe, kept because six lessons say to type it
./engine --trace     print one frame's command stream and exit          (from 4.9)
```

The split is **forced, not stylistic**. A window can be claimed by an SDL_GPU device *or* driven
by an `SDL_Renderer`, never both, and every HUD in Modules 1–3 is drawn with
`SDL_RenderDebugText`. So the mode is parsed at the top of `main` and the branch is taken before
`SDL_CreateRenderer` is reached. **Lesson 4.8 inverted the default**, as 4.2 said it would.

What 4.2 did *not* predict is that Stage A would stay. It is not scaffolding and it has no removal
date: it is the **reference implementation**. Every measured claim in Modules 2 and 3 was made
against it, so it is the only thing that can tell you whether the GPU path is right — and Lesson
4.8's per-pixel audit exists only because both renderers can draw the same scene from one
description. `[V]` runs them side by side in one window.

Lesson 4.2's own contribution to the spine is that the *presentation* path is proven on the GPU
before any shader exists: framebuffer → transfer buffer → texture → blit → swapchain, checked
bit-identical over a full image. When Module 4 ports the geometry, presentation is already
known-good, so a black screen can only be the new code.

**The shader toolchain, as of Lesson 4.3.** Shaders are authored once in HLSL and compiled at
build time into every format the local toolchain can produce, plus a JSON reflection file:

```
shaders/x.hlsl  --DXC or glslc-->  build/shaders/x.spv  --SPIRV-Cross-->  x.msl, x.dxil, x.json
```

Two structural points. **SPIR-V is the hub** — everything else is translated from it, so a
missing front end costs the whole toolchain rather than one backend. And **the build probes the
tool at configure time** rather than assuming it: SDL_shadercross can be built without
DirectXShaderCompiler, in which case it cannot read HLSL at all, and only running it reveals
that. `cmake/Shaders.cmake` prints which of three routes it took.

Compiled shaders land beside the executable, like assets, and are found with `SDL_GetBasePath()`.
The four resource counts `SDL_GPUShaderCreateInfo` demands are read from the reflection file and
never typed by hand — SDL validates none of them, so the file is the only check that exists.

**Stage B draws, as of Lesson 4.4.** The GPU path now has the full chain — device, shaders,
pipeline, vertex buffer, draw — and the probe's frame composes both stages into one window:

```
render pass 1: CLEAR  ->  blit the software framebuffer  ->  render pass 2: LOAD + draw
```

That composition is deliberate rather than transitional. It puts the CPU rasterizer's output and
the GPU's output side by side in one image, which is how Lesson 4.4 compares them — and the
comparison is the point of the two-stage spine: **zero disagreements in the triangle's interior**,
102 boundary pixels apart, the difference being a fill rule.

**Stage B draws real geometry, as of Lesson 4.5.** `assets/torus.obj` — the same file Module 3
rendered on the CPU — now goes through `gpu_mesh`, which converts Lesson 3.5's parallel arrays
into one interleaved 32-byte-per-vertex buffer plus a 16-bit index buffer, and is drawn seven
times from one call with per-instance data in a second buffer slot. Three architectural points
follow from that lesson and outlive it:

- **The vertex layout is named in exactly two places** — `gpu_mesh::describe` and
  `describe_instances` — and both are written entirely in `sizeof` and `offsetof`. A literal
  offset is correct until somebody inserts a field, at which point nothing reports an error.
- **`pipeline_desc::check_layout` is the only thing in the engine that checks a layout at all.**
  SDL refuses one of six broken layouts; the compiler cannot help, because the two halves are in
  different languages. The reflection JSON that Lesson 4.3's build already emits is the third
  document that joins them, and the check is honest that it catches three of five mistakes.
- **Two kinds of device buffer, distinguished by write frequency rather than by contents.**
  `gpu_buffer` is written once at load with per-upload staging and `cycle = false`;
  `gpu_stream_buffer` is written every frame with permanent staging and `cycle = true` on both
  hops. Getting it backwards wastes memory in one direction and corrupts data in the other — and
  only on a machine slower than the one it was written on.

**Stage B has a camera, as of Lesson 4.6.** The third rate of change — data shared by every vertex
of every instance — reaches the shader by a mechanism with no counterpart elsewhere in this
engine: `SDL_PushGPU{Vertex,Fragment}UniformData` writes into the **command buffer**, and every
draw recorded after it reads those bytes. There is no resource, so `src/gfx/gpu_uniform.hpp` is
the one GPU header here with no move-only wrapper class in it — only the two blocks the engine
sends, and `packed_offset`, which is HLSL's constant-buffer packing rule written as a `constexpr`
function so that each block can `static_assert` its field offsets against the rule rather than
against remembered numbers.

**Stage B is a renderer, as of Lesson 4.7.** The two remaining pieces both arrived as ports that
Modules 3 had designed for: the depth test is an attachment on the render pass plus three fields
of `SDL_GPUDepthStencilState`, and the sampler is two `static_cast`s from `engine::filter` and
`engine::address_mode`, guarded by an assertion that has run on every build since Module 4 opened.
`depth_buffer.hpp` and `texture.hpp` both said in Module 3 that this was the plan; the plan held.

Two structural points from that lesson outlive it:

- **`src/gfx/image.{hpp,cpp}` is the project's first non-SDL dependency and its containment.**
  `STB_IMAGE_IMPLEMENTATION` is defined in exactly one translation unit, `image.hpp` mentions no
  third-party type, and warnings are suppressed at the include rather than by editing the library.
  A dependency reaches exactly as far into a codebase as its types appear in headers; stb's reach
  is one file, so replacing it is one file. The test for hand-rolling that admitted it — *is the
  hard part the subject?* — is the same test the remaining four approved libraries will face.
- **Depth precision is now a number the engine can compute, not a folk belief.** The relation
  Δd = (f−n)·d²/(f·n·N) is derived in 4.7 §3.2 and measured to within a few per cent, which makes
  "how far can this renderer see before surfaces stop separating" a design input rather than a
  surprise. It will be a design input: Module 6's shadow maps are a second depth buffer with a
  second frustum, and cascades exist entirely because of this arithmetic.

That header is where the engine's answer to a recurring shape lives. Three times now a layout has
been declared twice in two languages with nothing checking the halves — vertex attributes (4.5),
uniform blocks (4.6), and textures still to come (4.7) — and the engine's response has been
different each time because the available evidence is different: `offsetof` and the reflection
JSON for vertex layouts, a `constexpr` rule and `static_assert` for uniforms. The reflection
carries no cbuffer offsets, so 4.5's cross-check has no counterpart here; the compiled SPIR-V does
carry them, which is what exercise 4.6.2 is for.

**Stage B draws a SCENE, as of Lesson 4.8.** Everything above draws one piece of geometry. A
scene is several, with different meshes, different transforms and different materials, and the
structural consequence is the whole of `src/gfx/gpu_scene.{hpp,cpp}`:

- **`surface_style` is the part of a material that cannot be a number.** Cull mode and fill mode
  are baked into an immutable pipeline object, so `solid` / `two_sided` / `wireframe` are three
  `SDL_GPUGraphicsPipeline`s created at startup. Three and not more, because every additional axis
  of pipeline state *multiplies* the count — the combinatorial growth is why real engines either
  enumerate a small fixed set or build them lazily and cache.
- **A fourth rate of change: per DRAW.** 4.5 had per-vertex and per-instance; 4.6 added per-frame.
  An object's model matrix is none of those, and SDL licenses the answer in one sentence
  ("Subsequent draw calls in this command buffer will use this uniform data"). So the frame is
  128 bytes pushed once plus 144 per object. **Per-draw overhead scales with object count, not
  object size.**
- **`render()` deliberately does not sort.** Sorting a draw list has several conflicting right
  answers (by pipeline, front-to-back for early-z, back-to-front for transparency, by distance for
  LOD) and the choice belongs to whoever knows what the frame is for. It *counts* instead:
  `pipeline_binds` against `ideal_pipeline_binds`. A policy you can measure is a policy you can
  argue about, and Module 6's frame organisation is where the buckets appear.

Two things that lesson changed elsewhere, both of which outlive it:

- **`engine::with_normals` moved a fallback out of the renderer and into the importer.** A vertex
  shader is handed one vertex and cannot compute a face normal; `collect_triangles` could, and
  did, whenever a mesh carried none — which is three of the four built-in meshes. Generating
  normals at import is what every real engine does, and the consequence is that flat-vs-smooth
  stopped being a runtime toggle (Lesson 3.8's `[Q]`) and became a property of the vertex buffer,
  because flat forces unshared vertices.
- **The mesh cache in `main.cpp` is the strongest argument yet for Module 5's asset system.** It
  keys geometry by the address of its first vertex and needs two extra fields to notice that a
  rebuilt `std::vector` keeps its address. Fifth lesson to name that pressure (3.2, 3.5, 3.9, 4.5,
  4.8) and the first where it costs something.

**Stage B is debuggable, as of Lesson 4.9 — and Module 4 is complete.** The last piece is not a
rendering feature at all: it is the ability to find out what the renderer actually did, which
matters because a breakpoint cannot. Recording and executing are separated by a submit (Lesson
4.2's sentence, collecting for the last time), so a GPU bug is a *state* question and a debugger is
a *control-flow* instrument.

`src/gfx/gpu_debug.{hpp,cpp}` holds three things, and the design of each is a decision worth
keeping:

- **Names at creation, not through the setters.** `SDL_SetGPUBufferName` exists and SDL's own
  documentation tells you to prefer `SDL_PROP_GPU_BUFFER_CREATE_NAME_STRING` instead, "to avoid
  thread safety issues". This codebase used the setter from 4.2 and, in 4.3, wrote a comment
  *explaining* the asymmetry with shaders by inferring a reason (immutability) rather than reading
  the docs of the function it was already calling. There was no asymmetry. Both the calls and the
  comment are corrected, and the general rule is recorded in LEARNINGS.md: **a confident
  explanation of why somebody else's API is shaped as it is, is a hypothesis.**
- **`debug_group` is an RAII scope that is neither copyable nor movable.** That is SDL's Metal rule
  in the type system — a group pushed inside a render pass is scoped to that pass, so push and pop
  must be in the same one, and an immovable scope cannot escape its block. On D3D12 all three debug
  calls need `WinPixEventRuntime.dll` and are *inert without it*, which is the only cross-platform
  difference in Module 4 that produces no diagnostic at all.
- **`frame_log` records from the statements that issue the calls**, never from a parallel
  description of what `render()` is believed to do — instrumentation that can drift from the code
  it describes is worse than none, because it is believed. `verify_49` §D asserts it against
  `draw_stats`: two counters incremented from the same statements, which is a test *a capture
  cannot give you*, having no independent account of what should have happened.

`engine --trace` prints one frame and exits. It exists because a keypress-armed log cannot be
tested headlessly, and the flag that made it testable turned out to be the useful artifact: a
deterministic frame dump that runs in CI and can be diffed between commits.

Two measurement disciplines came out of this lesson and belong to the whole project from here:
**measure the noise floor before comparing anything against it** (the first attempt reported a
negative cost for adding work), and **when an effect is below the floor, scale the workload until
it clears and divide** — two independent estimates converging is the evidence, not either number.

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

- **Handles, not pointers** (Module 5, Lesson 5.4 — *implemented*). Resources are addressed by
  *generational indices* — an index plus a generation counter — rather than raw pointers. Stale
  references are detectable (generation mismatch) instead of undefined behaviour, storage can be
  relocated and compacted, and serialization becomes trivial because a handle is just a number.
  This is why engines look the way they do, and it is taught as such.

  `engine/core/handle.hpp` is one 32-bit word split **20 index / 12 generation**, packed
  generation-high, with `T` as a *phantom* parameter so `handle<mesh_data>` and
  `handle<texture>` are different types at zero runtime cost. Generation 0 is reserved, which
  makes an all-bits-zero handle the null handle for free. `engine/core/pool.hpp` is the
  container that issues them: **`slots_` sparse and stable** (generation + dense index, indexed
  by the handle), **`items_` dense and mobile** (live objects only, packed, free to reallocate),
  and `owners_` mapping dense back to slot. `slot::dense == 0xFFFFFFFF` *is* the occupancy flag,
  so occupancy costs no extra byte and cannot fall out of sync. Removal is **swap-and-patch**:
  the last item moves into the hole and its owning slot is patched, which means a live object's
  address changes while its handle keeps working — the property that distinguishes a handle from
  a pointer with extra steps.

  The **dependency direction** is the thing to note: `pool<T>` knows nothing about geometry, and
  meshes were converted (`mesh_handle`, `mesh_pool`, `to_mesh_data`) without the pool gaining a
  line. Lesson 5.5 added `image_handle` / `image_pool` in *two lines* of `gfx/image.hpp` and the
  pool still did not change — which is where an abstraction is decided.

  **The cost is a parameter, and it is paid in the open.** A handle is half a reference; the pool
  is the other half, so `collect_triangles` takes a `const mesh_pool&` and resolves **once per
  object** at the top of its loop (never per vertex — measured at +0.13 ns per resolution).
  A global pool was refused for a concrete reason: `demos/sandbox` holds two `mesh_pool`s in one
  frame, one of loaded geometry and one of geometry derived from it by `with_normals`.
- **Assets are named, loaded once, and explicitly unloaded** (Module 5, Lesson 5.5). The policy
  layer on top of handles. `engine/asset/search_path.hpp` turns a **name** (`"torus.obj"` —
  stable, recorded in scene data, the cache key) into a **path** (where it resolved today) by
  trying an *ordered* list of roots; order is the whole feature, because prepending a root
  shadows a shipped asset without moving anything, which is simultaneously how mods,
  localisation packs, live editing and test fixtures work. It is also the only caller of
  `SDL_GetBasePath()` in the engine — there were two before, written by copying.

  `engine/asset/asset_store.hpp` owns the pools, the name → handle maps and the lifetimes.
  **There is no reference counting**, and the argument is from the properties handles have
  rather than from taste: a count needs a copy constructor, a destructor and a pointer to the
  store, which costs a handle its four bytes, its trivial copyability, its `memcpy`-ability into
  a component and its fixup-free serialization — every property Lesson 5.4 existed to obtain.
  So the policy is *explicit unload*, which is safe to get wrong precisely because staleness is
  detectable: forgetting leaks (found by `live_count()`), unloading early yields a null handle
  and a bump in `collect_stats::unresolved`, and neither is a crash.

  **The one unavoidable lifetime rule is derived assets.** `with_normals` output, a mipmap chain,
  a shader variant and a GPU upload are all the same shape: a real asset nobody asked for, with
  no name, existing only because its source does. `derive_mesh` records a dependency edge and
  unloading a source cascades **transitively**; deriving from a dead source is refused, because
  the result would have no name to find it by and no source to free it with.

  **Import settings are part of an asset's identity** — the same OBJ imported with and without
  the uv flip is two meshes with two vertex arrays — so the key is name + settings, with the rule
  that *the default configuration serialises to nothing* so that generated and loaded content
  share one key space. **The store is not a singleton**: `demos/sandbox` holds three.

- **Assets, and the second format** (Module 6, Lesson 6.6). glTF is the first format the engine
  reads that describes a **scene** rather than a shape, and that difference reaches the API:
  `load_model` returns *vectors* — meshes, placements, materials, and the material each mesh
  wants — because a glTF mesh is a list of primitives and each primitive has its own material.
  Flattening them would produce geometry that is correct and unpaintable.

  **The parser returns descriptions, not handles**, and that layering is the load-bearing
  decision. `gltf_material_desc` holds a `std::string base_colour_uri` and an `int` material
  index; it knows nothing about pools, caches, handles or search paths. Handing `parse_gltf` an
  `asset_store&` would be fewer types and would cost three things: the parser could no longer be
  tested without a filesystem (Lesson 3.5 split `parse_obj` from `load_obj` for exactly this and
  used the split to test a dozen malformed inputs from string literals); Module 9's offline asset
  cooker could not use it, because a handle is meaningless outside the pool that issued it and
  therefore cannot be serialised; and "is this the same image we already loaded?" would acquire a
  second answer, in a second place, that will eventually disagree with the first.

  **A texture is a derived asset, and that is a dependency edge rather than a reference count.**
  `load_texture` goes through `load_image`, so one decode serves however many materials name the
  file, and `derive_texture` records the image → texture edge; unloading the image releases both.
  The distinction that makes this safe where refcounting was refused: **nobody outside the store
  can hold a derivation edge.** The store made the texture, nobody asked for it by a name of its
  own, and the rule is enforced by a private member rather than by a contract. Note the second
  edge list — `mesh_derivations_` and `texture_derivations_` are separate rather than one untagged
  `{uint32, uint32}` list, because an untagged one would compile and would happily resolve a
  texture's handle bits against the mesh pool: Lesson 5.4's aliasing failure in a new costume.

  **Materials are named assets with no file**, and there is deliberately no `load_material`. The
  names are derived and deterministic — `"shapes.glb#2"` for a primitive, `"shapes.glb:gold"` for
  a material (namespaced, because two models may both call one "Metal"), and `"uv_grid.png"`
  *bare* for the image, because that one is a real file shared across models. That determinism is
  what makes the model cache work without a second map.

  **`mesh_import` is refused for glTF, on purpose.** `flip_uv_v` defaults to `true` because OBJ
  puts (0,0) at the lower left; glTF puts it at the upper left, the same as this engine. **The
  flip belongs to the format, not to the caller**, so `load_model` takes no import settings at
  all — honouring the flag there would turn every glTF asset upside down for everyone who left
  the default alone.

  **The one conformance gap is counted, not hidden.** glTF multiplies a base colour factor by its
  base colour texture; this engine's albedo image *replaces* the tint (Lesson 3.9's rule, because
  both are the albedo and a surface has one). They agree exactly when the factor is white, which
  is the common case, so the gap is only the *combination* — and it is reported as
  `model_load::factor_texture_conflicts` plus a warning, logged at the only point that knows both
  halves. The parser sees the factor and the URI but not whether the image resolved; the renderer
  sees a bound texture but has long since lost the factor.

- **Surface detail is a lie about the normal** (Module 6, Lesson 6.7). Shading has consulted the
  geometry's normal since 3.6, so a surface can only look bumpy if it is bumpy — which prices
  millimetre-scale detail on one prop at tens of millions of sub-pixel triangles. A normal map
  replaces that with one texture fetch, at the cost of a **frame** to express the stored direction
  in.

  **The frame is derived, not asserted.** A triangle's edge is one walk across the surface described
  twice — in metres and in texture units — so `e = Δu·T + Δv·B`, two edges give two equations, and
  inverting the 2×2 of uv deltas solves them. The determinant is twice the signed area the triangle
  occupies in the chart, which makes `det == 0` an ordinary case (an untextured face, a collapsed
  unwrap) rather than a degeneracy to guard: the face contributes nothing instead of an infinity.

  **The handedness is stored and the bitangent is not.** `mesh::tangents` is a span of `vec4`, with
  `w = ±1`, and `B = w·cross(N, T)`. The saving is 12 bytes a vertex; the *reason* is that after
  interpolation three separately-carried vectors are no longer mutually perpendicular, so a stored
  bitangent can disagree with the N and T beside it and a recomputed one cannot. Same argument as
  `material::textured()`. It matters because every symmetric model has a mirrored uv chart — an
  artist unwraps one half and reflects it — and a dropped sign lights one side as the mirror image
  of the other.

  **A texture now knows what it holds.** `texel_space{srgb, linear}` lives on the `texture` rather
  than on the sampler, because SDL_GPU declares the decode in the texture's *format* and performs it
  in the sampler — so one image cannot be sRGB in one binding and linear in another. Lesson 3.9 built
  these types to mirror SDL_GPU's; mirroring includes mirroring where a decision lives. The space is
  part of the asset's **identity** (5.5's `mesh_import` rule, second type), so the same PNG read two
  ways is two textures over one decode, keyed `name` and `name|linear`.

  Note the shape of the gap this closed: **the GPU had `create_sampled(…, srgb)` from Lesson 4.7 and
  the software renderer had no counterpart at all.** One half of an idea, complete; the other absent;
  and no code path crossing between them until a feature needed both. That is the same shape 6.6
  found between `load_image` and `sample`, and it is worth looking for deliberately.

  **The vertex layout became per-pipeline state.** `gpu_vertex_pnu` went 32 bytes to 48, and since
  attribute locations are numbered across the whole pipeline the new one collided with Lesson 4.6's
  instancing at location 3. `gpu_mesh::describe` therefore takes `with_tangent`: a shader that never
  reads a tangent should not declare the attribute, because it is a fetch paid for nothing and the
  location is a resource other buffers want. The buffer carries the tangent either way, so opting out
  pays the memory and not the bandwidth.

- **A shadow map is the z-buffer, aimed somewhere else** (Module 6, Lesson 6.8). `lambert(n, l)` asks
  whether a surface *faces* the light; the question we want is whether it can *see* the light, and
  nothing in the shading equation has ever consulted the rest of the scene. Rendering depth from the
  light answers it, because "the nearest surface along every ray from a viewpoint" is precisely what
  a z-buffer computes. `shadow_map::render` is `collect_triangles` + `draw_triangles` with a
  different camera; the rasterizer is untouched.

  **The projection is orthographic, and that fact is spent three times.** A directional light has no
  position, so the frustum is a box; `mat4::orthographic` maps it onto the clip cube by scale and
  offset alone, leaving `w` **exactly 1**. Because `w` is 1, device depth is affine in view depth, so
  `depth_range` is a single number valid everywhere in the box and the quantisation term is a
  constant — where `perspective` crowds precision against the near plane by a factor measured at
  over 100×. Because `w` is 1, the near plane may be *negative*, so the light's eye sits at the
  scene's centre. And because `w` is 1, the whole world-to-light map is affine, so a fragment's
  light-space position can be recovered from its interpolated world position instead of being
  interpolated — **zero new varyings**, on every draw in the scene, shadowed or not.

  **The bias is derived, and the derivation is the lesson.** The map stores one depth per texel,
  sampled at one point; the fragment is elsewhere inside that texel, so half of every texel's
  footprint is downhill of its own sample and half of every lit surface shadows itself (measured:
  50.1%). The worst error is the lateral travel — `reach × world_per_texel` — times the surface's
  depth gradient `tan θ`, over `depth_range`, and the measurement reaches **92% of that bound**.
  Everything else follows: a constant bias cancels an error proportional to `tan θ` with a number
  that is not, so it must be sized for the steepest surface present and unshadows everything whose
  caster is within `bias × depth_range` — which is peter-panning, worst exactly where a caster
  touches its receiver.

  **`reach` is where the formula hides a dependency.** It is half a texel diagonal for one lookup and
  `(r + ½)√2` for a PCF kernel — three times as far at 3×3 — so a correct bias stops being correct
  the moment the kernel widens, and the artefact arrives looking like a filtering bug. Found by
  rendering; `pcf_reach_texels` is the fix and `verify_68` §E is the regression test.

  **The GPU port is where the pass structure becomes real.** `gpu_shadow_map` owns a pipeline with
  `num_color_targets = 0`, a depth texture created with `SAMPLER` usage (which forces `STORE` where
  the scene's own depth buffer has been `DONT_CARE` since 4.7 — the whole tile write, on tiled
  hardware), and a comparison sampler, because filtering depths and then comparing is not a blurrier
  answer but a wrong one. `gpu_scene_renderer` gains a third fallback binding, and the pattern is now
  explicit: hand the shader the identity element of the feature it is missing.

- **The frame is declared, not assembled** (Module 6, Lesson 6.17 — *implemented*).
  `engine/include/engine/gfx/frame_graph.hpp`. By 6.16 the engine could record **seventeen render
  passes** in a frame — four cascades, the scene, eleven bloom stages, the resolve — across four
  owners, two of which begin their own attachments and two of which are handed one. Nothing knew
  the whole order, and four facts were kept in step by hand: the pass order, every load op, every
  store op, and which passes run at all (`gpu_post.cpp`'s `if (!s.enabled) { return; }`).

  The fix is one modelling decision. **A resource is not a texture; it is a variable with values
  over time.** `hdr@1` is what the scene pass produced and `hdr@2` is what the skybox produced from
  it, each version has exactly one producer, and a pass names the *version* it wants. That is
  single static assignment applied to render targets, and it turns all four facts into
  consequences: a pass says what it does with what was already in a target — `discard_write`,
  `clear` or `keep` — and the compiler derives the ordering edge, the load op, the **producer's**
  store op and the lifetime. One rule, *STORE iff some live pass consumes this version or the
  resource is imported*, reproduces Lesson 4.7's `DONT_CARE` on the scene depth and Lesson 6.8's
  `STORE` on the shadow map, which were argued out in comments nine lessons apart.

  Culling is backward reachability from writes to **imported** resources — the only values
  observable after the frame ends — so stopping the resolve from sampling the bloom drops eleven
  passes with no flag anywhere and nothing in `frame_graph.cpp` knowing what a bloom is.

  Two boundaries are load-bearing and must not move. **The graph orders passes, not draws:**
  everything inside a pass body is recorded by its callback, which is why 6.11's back-to-front
  blended tail and 6.16's instanced batching are structurally out of reach. And **ordering that
  comes from data may be handed over; ordering that comes from semantics may not** — until it can
  be declared, it lives inside an atom the scheduler cannot open, and here that atom is the render
  pass.

  What it does *not* buy, measured rather than assumed: **memory**. SDL_GPU 3.4.12 has no placed
  resource and no heap, so the strongest reuse available is a whole texture whose descriptor
  matches exactly; and the frame is a *chain*, so its peak is 85.7% of its sum. Perfect aliasing
  would save 14.3% — 5.27 MB at 1920×1080, which is exactly the bloom pyramid — and this engine
  collects none of it. The pool is built anyway, costs a dozen lines, and is shown working on a
  graph shaped like a tree.

  Two consequences for the rest of the engine, both of the same shape: `gpu_shadow_map::render_into`
  and `gpu_bloom::record_stage` record into a pass **somebody else began**, and neither can see a
  load op, a store op or a layer. A function that never sees a setting cannot get it wrong. The
  old entry points were rewritten to call the new ones rather than left beside them, because the
  verification compares the two assembly paths bit for bit and two copies of the same draw code
  would make that comparison pass while proving nothing.

- **ECS, not a scene tree** (Module 5). Data-oriented storage chosen after demonstrating —
  with cache-line reasoning and measurements — why OOP scene graphs creak at scale. Archetype
  vs sparse-set is a justified choice made in the lesson, not a coin flip.

  **Lesson 5.6 is the evidence, and it is deliberately unflattering to the usual story.** Six
  layouts, two workloads, five scene sizes, timed on the engine's own `transform` with
  `engine/core/bench.hpp`. Below a thousand objects every layout is within 13% of a flat array
  — scattering objects across the heap costs *nothing*, and walking a linked tree costs
  *nothing*. The knee sits between 1,000 and 10,000 objects, which is where 96 bytes an object
  stops fitting in L2, so it is a property of the cache and not of the design.

  Three of its findings shape what the ECS must be, each with a number:

  - **Storage must be dense and compactable.** Not because contiguity is virtuous — an array of
    pointers in allocation order is 1.00× up to ten thousand objects — but because a long-lived
    scene *decays* into disorder, and a shuffled linked tree is **6.47×** while a shuffled array
    of pointers is only **2.38×**. The gap is memory-level parallelism: an array's addresses are
    known in advance so its misses overlap, a list's are not so they cannot. A tree also offers
    no way to fix it, because node addresses are the identity — which is exactly what Lesson
    5.4's handles removed.
  - **A system must declare which components it touches.** Splitting a struct buys nothing when
    a loop reads 60 of 96 bytes and buys **5×** when it reads 12. That, not "SoA is faster", is
    the mechanism an ECS query exists to exploit.
  - **Iteration must not be virtual.** A flat **1.5–1.7×** at every scene size — and monomorphic
    and polymorphic agree within 4%, so it is not branch misprediction, and being flat across N
    it is not the vtable load. It is the inlining the call prevents.

  What 5.6 explicitly does **not** settle is archetype versus sparse set: both are dense, both
  are contiguous, both permit a subset.

  **Lesson 5.7 settles it, on its own evidence, and the answer is a sparse set** — one dense
  array per component *type* plus a sparse map from entity to dense index, as EnTT does, and
  not the archetype model of Unity DOTS, Flecs and Unreal Mass. The measurement was taken
  *before either design was built*, which is possible because the two differ in exactly two
  operations — reading K components of every matching entity, and adding or removing one
  component from one entity — and both are access patterns that `scratch/ecs_probe.hpp`
  simulates in three hundred lines with no entity manager, registry or type erasure in the way.

  The argument is **not** that sparse sets are faster. On the query the archetype wins:

  - **Query, all entities match, real body** (build a model matrix per entity): sparse costs
    0.96×–1.34× at 100,000 entities, and **1.00× below a thousand**. The control that says the
    harness is honest is K = 1, where both designs perform the same walk and the ratio is
    0.99×–1.01× at every size.
  - **The mechanism, isolated by rebuilding with the vectoriser off:** with codegen held still
    the redirect costs *nothing at all* — 1.00× — on the worst-case world up to a thousand
    entities. It is **latency in the shadow of work already in flight**, and it only bites when
    the working set leaves cache *and* the pools' dense orders have diverged. Either condition
    alone is free. A cheap body with nothing to hide behind pays up to 2.79×.
  - **Query, one entity in four matches:** the archetype's best case, and it wins by 1.46×–1.94×.

  What decides it is the other operation, and the shape of the escape hatch:

  - **Structural change costs the entity's total width in one design and nothing in the other.**
    Widening an entity from four components to twelve — eight the operation never reads — takes
    an archetype from **13.10 ns to 58.48 ns** and leaves the sparse set at **4.25 → 4.23 ns**.
    The cost model is not bytes moved (that predicts 2.3×; the truth is 4.5×) but *independent
    memory streams touched*, two per column per move, because a column is a separate allocation.
    This engine's entity passes twelve components during Modules 7–8.
  - **A group is an archetype you can add later.** A pool's dense order is nobody else's
    business, so two pools can be sorted into a common order; index *i* then means the same
    entity in both, the query reads no sparse entry at all, and what it walks is byte-identical
    to an archetype chunk. Measured at **0.99×–1.01×** of a real archetype on the archetype's
    own best case. **The migration only runs one way:** a sparse set can be given an archetype's
    query later, one group at a time; an archetype cannot be given O(1) structural change at any
    price.

  Honest counterweights, recorded because they argue the other way: archetype **fragmentation
  costs only 1.12× at worst** even at two entities per chunk (iteration only — query matching
  and per-archetype column lookup are unmeasured); the sparse set pays **12.8 MB of index at 32
  component types and 10⁵ entities**, roughly 80% of it meaning "this entity does not have this
  component"; and every archetype number is an *upper bound*, because the probe's columns are
  statically typed and a real one's cannot be. The strongest unmade argument for archetypes is
  **batched** structural change, which DOTS's command buffers exist for and which 5.7 measures
  one operation at a time and says so.

  **When the choice would be wrong**, in one sentence: a world past ~10⁴ entities whose
  component sets are stable and whose systems are narrow — a streaming open world, a crowd, a
  million-agent boid simulation. Even then the fix is to group two or three pools, not to
  rewrite the storage.

  Four rules follow for the runtime Lesson 5.8 builds, each paid for with a measurement:
  components stay small and single-purpose; there is **one shared id space**, minted once and
  handed to every pool (which is the single thing `engine::pool<T>` lacks — its three arrays are
  already a sparse set, but each pool mints its own keys); a view **leads with the smallest
  pool** (worth 1.73× → 1.46× at one-in-four selectivity); and the sparse arrays are **not
  paged**, because Lesson 5.4's free list keeps the id space's high-water mark at peak *live*
  entities rather than entities ever created.

  **Lesson 5.8 builds it**, in four header-only files under `engine/include/engine/ecs/` — so
  the library's source list is untouched and the public header count goes 47 → 51. All four
  rules are honoured, and the physical split is what makes them testable:

  | file | knows about | deliberately does *not* know |
  |---|---|---|
  | `entity.hpp` | ids, generations, a free list | that components exist at all |
  | `pool.hpp` | one component type, keyed by id | which ids are alive — it never met the allocator |
  | `registry.hpp` | both of the above | what any component type *is*, once the pool is built |
  | `view.hpp` | a set of pools | the registry, liveness, or systems |

  Five decisions inside it are worth recording, because each closes off a plausible alternative:

  - **The entity is its own type, not `handle<entity_tag>`.** It *reuses* `core/handle.hpp`'s
    constants — 20/12, generation 0 reserved, bump on removal — so the bit budget has one home.
    But `handle<mesh_data>` names an item *in* one container and an entity names a row *across*
    every pool there is; aliasing them would make `pool<T>::get(handle<T>)` and a component
    lookup the same spelling for opposite things.
  - **`dense_` stores the full 32-bit entity word, not a bare index.** `contains()` is three
    tests and the third — `dense_[at] == e` — is what rejects a stale or recycled id. Store an
    index and that test cannot be written, and Lesson 5.4's *aliasing* failure walks back in.
  - **Type erasure without RTTI.** `component_id_of<T>()` is a monotonic counter behind a
    function-local static; the id indexes `vector<unique_ptr<pool_base>>` and `static_cast`
    recovers the type — safe **because the id is what created the pool**, so the downcast is
    justified by construction rather than checked at use. The ids are global to the program, not
    per registry, which trades ~80 wasted bytes per registry for an array index instead of a hash.
  - **Every `pool_base` virtual is cold, and that is a constraint rather than an accident.**
    Lesson 5.6 measured virtual dispatch on an iteration at 1.5–1.7× *at every world size*, so
    nothing on the hot path goes through the base: a view holds concrete `pool<T>*`, and
    `pool<T>` is `final` so even the cold calls devirtualise. `erase` runs once per pool per
    entity destruction; `entities()` once per view, never per entity.
  - **`entities()` returning `span<const entity>` for every `T` is what makes rule 3 five
    lines.** A pack of `pool<Ts>*` becomes an ordinary array of spans, and the smallest is picked
    by a `for` loop — no dispatch, no metaprogramming, no runtime type information. The lead pool
    then earns its keep twice, because its own component needs no sparse read: the dense position
    *is* the loop counter.

  **The one rule a view imposes** is the familiar one: do not add or erase a component the view
  names while walking it. The walk is over the lead pool's dense array *by position* and both
  `insert` and `erase` move it — the same hazard as mutating a `std::vector` inside a
  range-`for`. A debug build catches it in `view::fetch`; the safe patterns are collect-then-act
  (`lifetime_system` in `ecs_swarm`) or a deferred command list, which Module 9 builds.

  **Groups are deliberately absent.** 5.7's 0.99× measurement is *why* the sparse set was
  chosen — the migration only runs one way — but building one before there is a profile is
  optimising on a hunch. `pool::components()` documents that its dense order is nobody's business
  *precisely so* a future group may sort it. Also absent, each with a reason: exclusion queries,
  const views, signals, and thread safety (Module 9 revisits every container at once).

  **What 5.8 did not do: render through it.** `collect_triangles` still takes
  `span<const scene_object>`, so `ecs_swarm`'s render system walks a view and *fills one* — a
  named, temporary bridge that Module 6 deletes. Because nothing on the render path moved, the
  reference shot from Lesson 5.1 is byte-identical for the eighth lesson, and that is a fact
  worth being able to check rather than a coincidence.

- **Transform hierarchy: level order, not recursion** (Lesson 5.9). Two components —
  `parent {entity}` and `world_transform {mat4}` — and one composition rule,
  `world(child) = world(parent) × parent_from_local(child)`. Nothing in `math/transform.hpp`
  changed: Lesson 2.8 named that function `parent_from_local` rather than `world_from_local`
  precisely so this day would cost nothing, and said so in a comment at the time.

  **The maths was free; the order was the problem.** A parent must be resolved before its
  children, and Lesson 5.7 established that a pool's dense order is *insertion* order, disturbed
  by every swap-and-pop. That is not theoretical: churn a 48-entity, 4-level tree the way a
  running game churns it and **twelve of them sit ahead of their own parent**, which a
  dense-order walk would compose against the previous frame's matrix — wrong in a way nothing
  reports.

  Three orders were measured before one was shipped, on a probe of three plain arrays rather
  than on the ECS (for 5.7's reason: an experiment built on the container measures the
  container). The deciding table holds the entity count still at 100,000 and moves only the
  shape:

  | depth | 1 | 2 | 4 | 8 | 16 | 32 |
  |---|---|---|---|---|---|---|
  | recurse from roots (ns/entity) | 3.34 | 6.55 | 10.08 | 11.81 | 13.29 | 13.55 |
  | level order (ns/entity) | 3.39 | 4.42 | 4.83 | 4.78 | 5.04 | 4.94 |
  | ratio | **1.01×** | 0.67× | 0.48× | 0.40× | 0.38× | **0.36×** |

  The depth-1 row is the **control**: no hierarchy, both arms do identical work, 1.01×. Without
  it nothing below it would be worth reading. Everything after it says the same thing —
  **recursion is depth-dependent and level order is very nearly not** — and it saturates around
  depth 8.

  Depth bucketing works because **depth is a topological order**: a parent's depth is always
  exactly one less than its child's, so a counting sort by depth is O(*n*) and puts every parent
  first. Two consequences follow. The resolve loop has *no recursion, no stack, no visited set
  and no “has my parent been done yet” test*, because the order already guarantees what those
  would check. And **within a level nothing depends on anything else in it**, so a level is a
  `parallel_for` that Module 9 will not have to design — it arrived with the choice of order.

  **`rebuild()` is split from `resolve()` and that split is worth more than the order itself.**
  A rebuild costs about one resolve (measured: 0.93–1.39), and a game re-parents rarely while
  moving things constantly, so the shape index is rebuilt only when the shape changes.
  `mark_topology_changed()` is a flag the caller sets — the registry ships no signals — and
  `resolve()` asserts what it can (the order's length against the transform pool's), which
  cannot see an add plus a remove between two resolves. That limitation is documented rather
  than papered over.

  **Two policies, both stated.** An *orphan* (its parent died) becomes a root, keeps its local
  transform, and is counted in `hierarchy_report::orphans` — destroying the subtree is a policy a
  game may want and a transform system must not impose, so `destroy_subtree()` is the explicit
  tool. A *cycle* is broken, counted and logged, and it has two defences because `parent` is a
  public component: `set_parent()` refuses to create one (walking the whole chain, not one link),
  and `rebuild()` survives one written directly. **A wrong picture is recoverable; a hang is
  not.**

  **Two optimisations were measured and refused**, each with its number. Physically packing rows
  into level order buys ~30% at 100,000 *decayed* entities and nothing at all in a freshly built
  world, and charges ~1.6 resolves per topology change. And dirty-subtree resolution crosses over
  at about **25% of the world moving** — because moving 10% of a depth-8 tree dirties 36% of it —
  and is **1.29× slower** when everything moves, which is exactly what an animated scene does.

  The shipped resolver costs **1.22–1.40× the probe**, which is the ECS's own indirection on this
  pass, and that number is published rather than hidden.

- **The camera is an entity** (Lesson 5.9). Four components: `transform`, `world_transform`,
  `camera {fovy, near_plane, far_plane}` and `active_camera` — an **empty struct**, because a
  component with no data is a tag and its presence is the information. Three consequences fall
  out without being designed: a camera can be **parented** (attach it to a car and it rides,
  resolved by the same pass as everything else); “which camera is active” is a component rather
  than a pointer, so destroying it dangles nothing; and it is findable by
  `view<camera, active_camera>()`.

  Aspect ratio is deliberately **not** a field — it belongs to the surface, which the user can
  resize, and storing it would put a machine-specific number in every scene file.

- **Debug drawing is a queue, and the queue includes nothing that can draw** (Lesson 5.11).
  `debug_lines` holds world-space segments with lifetimes — world rather than view, because the
  queuer does not know where the camera is and often runs before it is resolved, and because a
  split-screen game flushes the same queue twice. `draw_debug_lines()` in `debug_draw.hpp` is the
  software backend and is a four-line loop over `line3_world`, which has clipped correctly since
  Lesson 3.3: **nothing was rewritten, a queue was put in front.**

  Three problems came from one fact — two of `line3`'s six parameters were renderer state — and
  removing them fixes all three: only the renderer could call it, it worked on one surface, and a
  line lived exactly one frame. That last one is the argument for lifetimes and it is the one
  people dismiss: **16.7 ms at 60 Hz against the ~250 ms a person needs**, so a per-frame drawer
  can show you *state* and never *events*, and it is usually events you are hunting.

  The expiry rule **tests before it subtracts**, which removes a dependency rather than getting it
  right: measured in `float` at 60 Hz on a half-second line, test-first gives 31 advances,
  subtract-then-test with `<=` gives 30 and is also correct, and subtract-then-test with `<`
  gives 30 *and keeps a one-frame line for ever when `dt` is zero* — a paused clock, a
  single-frame `--shot`, a breakpoint. The order in the frame is **queue → flush → advance**, with
  `advance()` last and outside every branch.

  Bounded at 4,096 lines **with a drop counter**, because a bound with no counter is a bug that
  presents as a rendering artifact: the missing line looks exactly like a thing that does not
  exist. No depth test and no batching, both named rather than discovered — and the queue is
  precisely what makes batching possible later, since you cannot batch calls that already
  happened.

- **The debug UI is the one singleton, and not by choice** (Lesson 5.11). `asset_store`,
  `registry`, `action_map` and `debug_lines` are all values; `debug_ui` cannot be, because ImGui
  keeps its context in a library global that every `ImGui::` call reads. `start()` refuses a
  second instance and logs why — an enforced limit you can read beats an undocumented one you
  discover.

  On a surface with no renderer it does nothing, safely: `start()` returns false and logs at
  *info* (an error line in every headless run trains the reader to ignore error lines), and every
  other call is a no-op **including `wants_keyboard()`**, so the mask blocks nothing and a program
  behaves exactly as it did before the UI existed. That is what keeps Lesson 5.1's reference image
  byte-identical.

  `begin_frame()` goes **first in `on_input()`**, not beside the panels where it looks like it
  belongs: ImGui computes its capture flags inside `NewFrame`, and the mask reads them two lines
  later. Put `NewFrame` in `on_overlay` and every read answers about the *previous* frame — one
  frame of input leakage, every time, invisible unless you look for it.

- **Two consumers of one keyboard are separated on levels, never by routing events** (Lesson
  5.11). `engine::input` tracks levels, and **a level is only ever corrected by the event that
  contradicts it** — so a key-up routed to the UI and not passed on leaves that key held down
  for ever. Both consumers see every event; `masked_input<Source>` withholds what the UI has
  claimed one layer later, and because it satisfies Lesson 5.10's `input_snapshot` concept,
  `action_map::update` took it with **not one character changed**.

  The map is still updated while the UI has focus. Skipping it would freeze every level — hold a
  movement key, click into a text field, and the camera flies away — whereas updating through the
  mask reports masked keys as *up*, which fires the release edge, which is the behaviour you
  actually want.

  **A delta cannot be masked by masking one of its endpoints.** The cursor is not a level:
  `action_map` derives a delta by differencing two frames. With the cursor travelling 100 → 160
  over three blocked frames and on to 170, reporting zero gives **+170** on the release frame,
  freezing the last position gives **+70** (this is the version that ships), and the shipped
  virtual cursor — `reported = real − offset`, with `offset` absorbing exactly the movement that
  happened while blocked — gives **+10**, which is one frame of real motion. The wheel needs none
  of it, because `input` publishes the wheel as a per-frame delta already.

  The view matrix is `rigid_inverse` of the resolved placement, and that is Lesson 2.9's
  derivation extracted into `math/mat4.hpp` now that a camera has a placement to invert. The
  identity `rigid_inverse(parent_from_local(look_along(e, t, u))) == look_at(e, t, u)` holds
  **bit for bit**. There is still no general 4×4 inverse, for the reason `mat4.hpp` gave in
  Module 2: it would answer a question we never ask.

- **Actions, not keycodes** (Lesson 5.10). An action is a name, a binding maps a signal onto it,
  and a frame publishes the value. The four things this buys are all about *expression* rather
  than cost — rebinding, multi-device input, recordable intentions, and code that says what it
  does — so **this is the one subsystem in Module 5 chosen without a benchmark**, and the lesson
  says so rather than manufacturing one.

  **One mechanism covers buttons and axes:** every binding contributes a signed float and an
  action's value is the sum. `steer` bound to A at −1 and D at +1 gives exactly zero when both
  are held, and no rule anywhere says so. A design with separate button and axis kinds would have
  needed one.

  Two decisions in it are invisible until they bite, and both are tested on purpose:

  - **An edge is a change in the ACTION, not in a signal.** Bind one action to a key *and* a
    mouse button; press the second while the first is held, and a binding-derived edge fires
    again — two presses for one intention. Deriving from the summed level gives one. The bug
    works perfectly with a single binding, which is what a first implementation and its first
    test both have.
  - **A fixed-timestep engine needs a second kind of edge.** `on_fixed_step` runs zero or more
    times per frame, so a frame-scoped edge read there fires twice on a two-step frame *and* is
    lost entirely on a zero-step one. `consume_pressed()` pops one queued press and fires exactly
    once however the steps fall. The queue is four deep and overflow is dropped, which is a
    decision — an unbounded queue replays a burst of jumps after the player has stopped asking.

  **`app` gained its eighth hook**, `on_input()`, because none of the existing ones runs exactly
  once *before* the simulation. `on_frame` is the near miss: right frequency, wrong place, and a
  map updated there leaves every step reading last frame's actions — invisible in a demo and a
  real 16 ms of input delay in a game.

  **`action_map::update` is templated on a C++20 concept**, `input_snapshot`, which names the six
  questions it asks. That is the course's first concept and it was forced by a test that could
  not otherwise exist: `input::update()` samples SDL's live keyboard state, so a harness wanting a
  key held would have to persuade SDL of it. A concept beats an abstract base on three counts —
  it changes nothing in `input.hpp`, costs no virtual call on a per-binding-per-frame loop, and is
  open, since `engine::input` satisfies it without knowing it exists.

  **There is no gamepad support, and the lesson says why:** it could not be run on the machine
  this was written on, and untested device code is a liability that looks like support. What *is*
  demonstrated is the property the gamepad claim rests on — one action, several bindings, more
  than one device.

- **Fixed timestep + render interpolation** (Module 1). The accumulator loop, derived rather
  than pasted as folklore. Simulation determinism is a property you design in early or retrofit
  painfully; physics in Module 8 depends on it already being right.
- **No exceptions, no RTTI in engine core.** Explicit error handling instead. The tradeoff is
  taught honestly in its own section rather than asserted.
- **Colour and depth are separate attachments** (Module 3, Lesson 3.1). `framebuffer` and
  `depth_buffer` are independent types with independent lifetimes, and the depth test lives in
  the *rasterizer*, not in the buffer. Both choices copy the hardware:
  `SDL_BeginGPURenderPass` takes colour targets as an array and the depth-stencil target as a
  separate, nullable parameter, and `SDL_GPUDepthStencilState` carries `compare_op`,
  `enable_depth_test` and `enable_depth_write` as three independent pipeline knobs. Modelling
  that split in the software rasterizer means the Module 4 port is a rename rather than a
  redesign — which is the whole argument for Stage A targeting SDL_GPU's conventions exactly.
- **Render state is an object, not a parameter list** (Module 3, Lesson 3.2). `fill_style`
  gathers interpolation mode, shading and blend space into one struct passed to the rasterizer.
  This is the shape the hardware has — a GPU bakes state into a *pipeline object* built once and
  bound before drawing, because validating it per draw call would be ruinous, and
  `SDL_GPUGraphicsPipelineCreateInfo` is this struct several times over. Adopting the shape while
  it holds three fields means Module 4 is a rename, and each new knob costs one field rather than
  one more argument at every call site.
- **Clipping is a pipeline stage, not a guard** (Module 3, Lesson 3.3). The perspective divide
  has a *precondition*, not a branch: `x/w` cannot be made safe from the inside, because for a
  vertex behind the eye it is not merely large but *sign-flipped*, and once divided the
  information needed to detect that is gone. So `src/gfx/clip.hpp` introduces a second vertex
  type — `clip_vertex`, living in clip space, where `w` is still signed — and the geometry
  pipeline gains a stage between the projection matrix and the divide. The near plane is
  `z_clip >= 0`, which the projection's depth row was *built* to make a coordinate plane; it is
  emphatically not `w >= 0`, which is the plane through the eye and admits screen coordinates in
  the hundreds of thousands. Only the near plane is clipped, and that asymmetry is the load-bearing
  part: the near plane is a **correctness** requirement, the other five are an **optimisation**
  the rasterizer's bounding-box clamp already covers. Keeping those categories distinct is what
  lets a later lesson skip one deliberately (guard-band clipping, Module 4).
- **Culling reads a sign the rasterizer already had** (Module 3, Lesson 3.4). `fill_triangle` has
  computed the triangle's signed area since Lesson 2.2 and then immediately destroyed its sign by
  reorienting to a positive area — so back-face culling is one comparison inserted into the single
  window between those two events. It lives in the rasterizer, not in the caller, because that is
  where the hardware runs it: after clipping and the perspective divide, before rasterization. The
  test deliberately takes *screen-space* vertices, which makes the classic bug — asking
  `dot(normal, camera_forward)` in view space, wrong on 15% of triangles at a 55° field of view —
  impossible to write by accident. And it is the second entry in `fill_style` to mirror
  `SDL_GPURasterizerState` field for field, so the Module 4 port stays a rename.
- **Untrusted data enters at exactly one place, and is measured there** (Module 3, Lesson 3.5).
  Up to 3.4 every mesh was typed into `mesh.hpp`, so "closed", "consistently wound", "indices in
  range" and "faces are triangles" were true *by construction*. A loader makes all four into
  claims about a file, and the engine's answer is `validate()` — welded vertex count, edges,
  Euler characteristic, boundary and non-manifold edges, winding conflicts, signed volume —
  computed **once, at the boundary**, so everything downstream may assume. That is why
  `scene_object::closed` is no longer a `bool` somebody typed next to the geometry: it is the
  validator's output, and back-face culling is gated on a measurement. Validation is not a
  per-frame cost and is not shaped like one; it reaches for `std::map` while the loader's hot
  de-duplication loop reaches for `std::unordered_map`, and matching the container to how hot
  the loop actually is is most of what performance-awareness means in practice.
- **Owning and viewing are different types** (Module 3, Lesson 3.5). `mesh_data` owns four
  `std::vector`s; `mesh` is four `std::span`s over them. Every function that *draws* takes the
  view, so it works identically on compiled-in arrays and on a file loaded a moment ago; only
  the loader needs the owner. The rule that comes with it — a view must not outlive its owner —
  is not enforceable by the type system, so the API is shaped to remove the mistake: loaders
  fill a caller-owned out-parameter rather than returning geometry, because returning
  `data.view()` from a function that built `data` locally compiles cleanly and dangles. Module
  5's handles replace "safe because of the order two things happen in" with something checkable.
- **Lighting is a vertex-stage concern, and the pipeline said so before we did** (Module 3,
  Lesson 3.6). Adding a real light changed no rasterizer code at all: `fill_style` gained no
  field and `fill_triangle` gained no branch, because lighting's output is a *vertex colour* and
  interpolating vertex colours has been the fill's job since Lesson 2.4. That is the
  vertex/fragment boundary showing up on its own, ahead of Module 4 making it literal with two
  separate shaders — and it is also why `fill_style` is now visibly the wrong home for shading
  parameters, which is the pressure that produces a material system in Module 6.
- **Normals are transformed by the inverse transpose, everywhere, forever** (Module 3, Lesson
  3.6). `normal_matrix` lives in `src/math/mat4.hpp` rather than in the renderer because it is a
  statement about matrices, not about light — normal mapping (Module 6) and collision response
  (Module 8) need the same function. The rule exists because a normal is defined by a
  *relationship* (perpendicular to every tangent) rather than by being an arrow, and only
  `(M⁻¹)ᵀ` preserves it. Critically, it is **identical to the model matrix for rotations and
  parallel to it for uniform scales**, so a codebase can carry the bug indefinitely while its
  hero assets look perfect; ours makes the failure a keypress and a pixel count.
- **View dependence has a structural cost, and it is worth naming** (Module 3, Lesson 3.7).
  Through 3.6 the vertex loop composed `view_from_world * world_from_model` once per object and
  sent each vertex straight to view space — one matrix multiply instead of two. That optimisation
  existed *because* the shading was view-independent: Lambert compares two directions and no world
  position was ever needed. A specular highlight needs `eye - position`, so the composition comes
  apart and every vertex pays a second multiply. Nothing regressed; a fast path was **bought out by
  a feature**. The general shape — an optimisation is usually a simplifying assumption with a name,
  and features cash those assumptions in — is worth carrying into Module 9's profiling work, where
  "why is this slower than last month?" is usually answered by a feature nobody connected to the
  loop it slowed.
- **`specular` is the material system arriving one field at a time** (Module 3, Lesson 3.7). Two
  fields — a highlight colour and a shininess — that belong to the *surface* and to nothing else:
  not the light, not the rasterizer, not `fill_style`. It is deliberately not called `material`,
  because a material is also the albedo, the cull mode, the blend mode, the textures and eventually
  the shader, and inventing four fifths of that here would be guessing. This is the **fourth** pull
  in the same direction (3.2's shading enum, 3.4's `closed`, 3.6's "`fill_style` is the wrong
  home", now this), and Module 6 answers it with arguments rather than by accretion. Watching a
  struct try to draw its neighbours in is the design telling you what it wants to be.
- **Which shading model you evaluate is pipeline state; how shiny a surface is, is not** (Module 3,
  Lesson 3.7). `specular_model` is a parameter to `shade()` rather than a field of `specular`,
  because in a real engine the choice of Phong vs Blinn is baked into a shader at compile time and
  a scene does not mix the two. It is a runtime knob here for exactly one reason: so both can be
  rendered and the difference counted. The same distinction — per-material data versus
  per-pipeline state — is what decides, in Module 6, which fields of a material become uniforms and
  which become shader variants.
- **`vertex` is a position plus varyings, as of Module 3, Lesson 3.8.** Adding a world-space
  normal and world position to it was not "two more fields": every field before them was either
  geometry or a finished answer, and these are *inputs to a calculation that has not happened
  yet*, carried across the triangle so the fragment can do it. That is what a GPU vertex shader
  emits and what Module 4 will call a varying. Three consequences worth carrying: `vertex::colour`
  now means different things under different pipelines (a lit result, or the albedo); **every
  stage between the vertex and the fragment must interpolate them**, and the clipper is the one
  that gets forgotten because it usually does nothing; and the cost is real — 28 bytes to 52, six
  more interpolated floats per pixel — which is why minimising varyings is a genuine optimisation
  on a GPU rather than a micro-concern.
- **The rasterizer now depends on the lighting model *and* on textures, deliberately and
  temporarily** (Module 3, Lessons 3.8 and 3.9). `raster.hpp` includes `light.hpp` and
  `texture.hpp`; `shading::lit` calls `shade()` per fragment and `shading::textured` calls
  `sample()`.
  This is a layering violation: coverage and interpolation are the rasterizer's job, and what a
  covered pixel should *look* like is not. The clean fix is to make the fragment calculation a
  parameter the caller supplies — which is precisely what a fragment shader is, so building it
  here would mean inventing Module 4's answer before the question is fully formed. Lesson 2.4's
  header called the `shading` enum "a placeholder for a fragment shader" when it was created;
  3.8 is where the placeholder started costing something. Fixed-function graphics hardware made
  this exact trade and the programmable-shader era is the industry paying it off.
- **Two axes, not one knob** (Module 3, Lesson 3.8). Where a normal comes from is a property of
  the *mesh*; where the shading equation is evaluated is a property of the *pipeline*. Lesson 3.6
  shipped them as one enum and could not extend it — a sure sign the type is enumerating a
  product rather than a sum. Splitting them made three previously-invisible facts checkable,
  because a grid has cells whose behaviour you can predict and a list does not: with a face normal
  all three evaluation points agree to zero pixels, and *only* while the shading is
  view-independent. The general rule for this codebase: **when a new case will not fit an enum,
  check whether two of the existing values differ in more than one respect.**
- **A sampler is an addressing scheme, not a lookup** (Module 3, Lesson 3.9). `src/gfx/texture.hpp`
  mirrors `SDL_GPUSamplerCreateInfo` — same names, same enumerator order — so Module 4's port is a
  rename. The one thing to carry structurally is *where the conversions live*: the **sRGB decode
  is inside the sampler**, which puts it before the filter with no way for a call site to get the
  order wrong, and is exactly what an `_SRGB` texture format does in hardware. Consequently
  `sample()` returns `linear_rgb` and an image drops straight into the albedo slot of the shading
  equation with no conversion at all — texture × light is then a consequence of what an albedo
  *is* (a reflectance) rather than a convention anybody chose.
- **Convention reconciliation belongs at the import boundary** (Module 3, Lesson 3.9). Wavefront
  OBJ counts `v` upwards from the bottom; SDL_GPU counts it downwards from the top. Three places
  could hold the flip and only one is right: not the **parser**, because Lesson 3.5 made a loader
  a place where data must not differ from its source; not the **sampler**, because it has to match
  hardware that will not flip anything for us; but the **import step**, which is where every real
  engine puts it (Assimp's `aiProcess_FlipUVs`). `flip_uv_v(mesh_data&)` is that step, and the
  rule generalises: *a loader must not alter its input; a pipeline may.* It must also be applied
  to **every** mesh a load produces, in-memory ones included, or a round-trip test starts
  measuring the import rather than the loader.
- **The `shading` enum has stopped describing a fragment** (Module 3, Lesson 3.9), and this is the
  clearest single signal that Module 4 is due. "Textured **and** lit" is not a value — it is `lit`
  plus a binding. Lesson 3.8 fixed the previous instance of this by splitting one enum into two,
  which worked because both questions had small closed answer sets; here the combinations of
  albedo source × what is done with it × which of several textures are a *program*, not a grid,
  and no third enum helps. Six pressures now point the same way (3.4 cull modes, 3.6 the wrong
  home for `fill_style`, 3.7 specular, 3.8 per-triangle material, 3.9 this, and 3.10's
  `encode_mode` — a per-fragment branch on a draw-constant value, which is a compile-time
  shader variant in a runtime disguise), and all six are answered by the same thing: a
  fragment stage the caller programs.
- **The engine measures itself, and the instrument is calibrated before it is trusted** (Module 3,
  Lesson 3.10). `src/core/profile.hpp` lives in `core/` rather than `gfx/` because measuring time
  is not a graphics concern — the physics step and the asset loader will each want a zone without
  including a rasterizer to get one, and that is the first placement in this codebase decided by
  Module 5's argument rather than by convenience. The design turns on three facts that are
  *measured*, not assumed: the counter's tick (41.667 ns on the reference machine) is eight times
  *longer* than the cost of reading it, so the cheap operation is the limiting one; a zone must
  therefore last 100× that before its number means anything; and instrumenting anything finer
  destroys what it measures (1.86× slower, while simultaneously under-reporting, because the
  closing clock read sits inside the interval it closes). Fine-grained answers come from
  **subtraction** instead — a ladder of variants differing by one thing each, which is a harness
  concern (`scratch/verify_310.cpp`) and deliberately not an engine one. Zones are a **partition**,
  not a call tree, and the unmeasured remainder is displayed beside them, because six honest bars
  summing to 60% of a frame look complete right up until you double the biggest one and gain 9%.
- **Cost is paid per pixel, and the axis is pixels per triangle** (Module 3, Lesson 3.10). The
  renderer's work happens at exactly two frequencies — per-vertex, scaling with triangles, and
  per-pixel, scaling with covered pixels — and every performance surprise in this codebase so far
  has come from attributing a cost to the wrong one. Measured: the **2-triangle floor costs eight
  times what the 2,304-triangle torus does**, and sixteen times the pixels moved the vertex stage
  by 3.7% while moving the fill by 15×. The two curves *cross* near one pixel per triangle, which
  is the only place on either axis where "optimise the fill" stops being right — and nothing about
  the renderer has to change to move across it. Consequently this repository quotes **ns per
  covered pixel** and **ns per triangle**, never milliseconds per frame: the first two are
  properties of the fill loop and the vertex stage, and the third is a fact about one scene at one
  resolution on one computer.
- **An optimisation ships with its error, or it does not ship** (Module 3, Lesson 3.10).
  `encode_mode` is the first knob in this engine that is neither right nor wrong —
  `blend_space::encoded`, `interpolation::affine` and `draw_line_naive` are all kept so a *mistake*
  can be summoned, whereas this is a defensible speed/accuracy point. Three rules came out of
  choosing it. Judge an approximation by its error **before** rounding, because the worst-rounded
  column saturates and cannot separate 0.0115 of a code from 0.4022 — a distinction that becomes
  3.0 against 103.4 the moment Module 6 stops being 8-bit. Time every candidate rather than
  reasoning about it: the *exactly correct* threshold table turned out to be **slower than
  `std::pow`**, because eight dependent L1 loads is a longer latency chain than a modern `powf`.
  And `fill_style::encode` defaults to `exact` even though the demo selects `fast`, because every
  measured claim in Lessons 3.1–3.9 was made against the exact encode and a default that silently
  moved 0.60% of those pixels would falsify nine lessons' arithmetic — the same bargain as
  `vertex::inv_w = 1` and an unbound `albedo`: *a default that changes nothing is what lets a
  feature be added to a pipeline object without auditing its call sites.*
- **The rasterizer can imitate the hardware, so the hardware's costs can be measured** (Module 4,
  Lesson 4.1). `fill_style::traverse` adds a 2×2 quad walk beside the scanline one: it shades the
  lanes a triangle *misses*, discards them, and counts them. The output is bit-identical in colour
  **and** depth — verified over 2,306 triangles — so the only thing that changes is how much work
  was done, which is what makes it an *instrument* rather than a feature. Its default is
  `scanline`, because that is what a CPU rasterizer should do and what every measurement before
  4.1 was taken against. The reason the model cannot be avoided on real hardware is that
  `ddx`/`ddy` are differences between neighbouring lanes, so the neighbours must run — which is
  also why a derivative inside a divergent branch is undefined, and why mipmap selection is paid
  for out of wasted lanes. Measured lane efficiency falls from 96.3% on a 64-pixel triangle to
  25.0% on a one-pixel one, and 1/efficiency predicts the slowdown to within 0.09× across a 560×
  range of triangle counts. **This is the precise content of "small triangles are expensive", and
  it is a statement about size in pixels rather than count.**
- **Extracting the fragment revealed what it had been since Lesson 3.6** (Module 4, Lesson 4.1).
  Two traversals needed to share the per-pixel work, so it came out of the loop into a lambda —
  and what came out is a function from three barycentric weights to a colour with all pipeline
  state captured, which is a **fragment shader**. The only thing separating it from the real
  article is that the caller cannot supply it. It also acquired a new obligation, because it now
  runs for lanes outside the triangle: it must be **total**, which it is only because
  `wrap_texel` folds any index into range (3.9) and `linear_to_srgb_u8` refuses a NaN (3.3). Both
  guards were written for other reasons and both are load-bearing here.
- **`fill_style` is a pipeline object, and the usual justification for pipeline objects is wrong**
  (Module 4, Lesson 4.1). Ten top-level fields against `SDL_GPUGraphicsPipelineCreateInfo`'s nine
  (53 once its nested state structs are expanded) — the same object, by deliberate design since
  3.2. The folk explanation is that baking state in saves the inner loop a branch; our loop
  branches per pixel on draw-constant state, and hoisting it measured **0.93×**, i.e. nothing. A
  perfectly predicted branch is free. The real reason is that the driver must *validate* the
  combination and *compile* a shader specialised to it — milliseconds, ruinous per draw and free
  once, which SDL's own header states by calling pipelines "precalculated rendering state". Our
  `fill_style` already spans 96 combinations decided at runtime per pixel; a GPU compiles the one
  you asked for, and that is what a shader is.
- **The real-time path is a pure function, and it is public** (Module 7, Lesson 7.8).
  `audio::mixer::mix_into()` produces N frames into a caller's buffer and touches nothing else;
  `open_offline()` builds the voice table with no device at all. That is not a testing hook bolted
  on afterwards — it is the only thing that makes the subsystem legible. A real-time callback is
  the worst place in a program to learn anything: you cannot breakpoint it without changing the
  result, you cannot print from it, and its output is a pressure wave. Offline, the same code is a
  function from a voice table to an array of floats, and an array of floats can be diffed, plotted
  and asserted on. **Every number in Lesson 7.8 was produced on a machine that was never asked
  whether it had speakers**, and `demos/audio --shot` uses the same entry point, so a screenshot is
  silent and deterministic. The generalisation is the rule: *real-time code you cannot call from a
  test is real-time code you cannot debug.*
- **The audio thread reports by counting, never by logging** (Module 7, Lesson 7.8). Sixty-four
  voices cost **29.76 µs of a 10,667 µs deadline** — 0.28% — so the thing that will cause a dropout
  is never the mixing; it is anything *unbounded* on that thread. One log line costs 82 ns to
  format and 1,022 ns to format, write and flush, and the second number is a **mean**: it is a
  syscall, and a syscall has no upper bound. So `mixer_report` is eleven counters the audio thread
  increments with relaxed atomics and the main thread reads without taking the lock. The one field
  that *does* take the lock is `live_voices`, because it is a container's size rather than a
  counter, and reading a size while another thread inserts is a race rather than a stale number.
- **A counter that fires on the healthy case is not a counter** (Module 7, Lesson 7.8). The
  mixer's first dropout proxy incremented when the device's queue was empty as the callback began,
  which is the obvious reading and is wrong for SDL3's pull model — the callback is invoked
  *because* the device wants more. It reported **28 of 58 buffers "starved"** on a mix at 0.2% load
  with nothing wrong. It is now `queue_empty` and says what it observes; the question it was meant
  to answer is `late`, which compares the mixer's own time against the mixer's own deadline and
  needs nothing from the driver, so a healthy run prints zero. **Prefer counters computed entirely
  from quantities you own.**
- **Public API surface is a deliberate artifact,** not whatever headers happen to be reachable.

---

## 6. Critical workflows

Commands that are not obvious from reading files.

### Build

```sh
cmake -S . -B build              # first run compiles SDL3 and ImGui via FetchContent — minutes
cmake --build build

./build/demos/sandbox                        # the GPU scene (4.8)
./build/demos/sandbox --software             # the five CPU demos, on [Tab]
./build/demos/sandbox --probe                # the 4.2–4.7 instrument
./build/demos/sandbox --trace                # one frame's command stream, then exit  (4.9)
./build/demos/sandbox --shot scratch/x.ppm   # seven pinned frames, no window          (5.1)
./build/demos/hello_cube                     # the acceptance test: public API only    (5.1)
./build/demos/hello_cube --shot cube.ppm     # …one frame, no window
./build/demos/ecs_swarm                      # 154 entities, 3 levels, 152 debug lines,
                                             #   and two ImGui panels               (5.9-5.11)
./build/demos/ecs_swarm --shot swarm.ppm     # …one frame, no window, deterministic;
                                             #   the UI declines to start and says so
```

> **The executable moved and was renamed in Lesson 5.1.** It was `build/engine` through Module 4;
> that name now belongs to the library. Anything in Modules 0–4 that says `./build/engine` means
> `./build/demos/sandbox`.

`FetchContent` pins SDL3 and Dear ImGui to tags, and `stb` to a commit SHA because it has no
tags. **ImGui ships no `CMakeLists.txt`**, deliberately — so `FetchContent_MakeAvailable` only
*populates* it and the root `CMakeLists.txt` declares the `imgui` target itself, naming the seven
files it compiles: the four core sources, `imgui_demo.cpp` (kept, because
`ImGui::ShowDemoWindow()` is the fastest widget reference there is), and the SDL3 platform +
SDL_Renderer backend pair. `engine_set_warnings` is deliberately **not** applied to it.

A fresh clone needs no SDL install and no submodule ritual.
The tradeoff (a one-time source build) was accepted over vendored submodules specifically to
kill the "forgot `git submodule update --init`" failure mode.

### Warnings are errors in spirit

The build runs at `-Wall -Wextra` (`/W4`) and all warnings get fixed. A warning left standing is
a broken window.

### Authoring a lesson page

```sh
python3 docs/_template/apply-shared.py --check   # before committing docs/ changes
python3 docs/_template/apply-shared.py           # after editing lesson-template.html
python3 docs/_template/check-curriculum.py       # index vs the lesson table
python3 docs/_template/check-builders.py         # every generator still makes its page
cd docs && python3 -m http.server 8000           # then verify in a real browser
```

Edit the shared CSS or the shared page script **in `lesson-template.html` only**, then stamp
(§3). `--check` exits 1 on drift, so it works unchanged as a pre-commit hook or CI gate; it also
lints for inline `fill=` on SVG `<text>`, which the shared stylesheet silently overrides.

Verify pages in **Playwright/Chromium over HTTP, never an embedded preview pane** — the pane
misreports computed styles, so a dead highlighter or a broken theme toggle can look correct
there.

#### The generators, and why they are checked

Each lesson page under `docs/lessons/` is assembled by a `scratch/build_NN.py` out of prose
fragments, computed SVGs and code listings. **A listing must be pinned, never read live.** A
builder that opens a repository path renders the code as it stands *today*, so every later
lesson's edits leak backwards into an earlier page — and when Module 5's refactor deleted `src/`,
eleven builders stopped running at all. The pin is a `LISTING_SOURCE` dict mapping each listed
path to a frozen copy in `scratch/`.

`check-builders.py` runs every generator in a copy-on-write clone of the tree and compares the
result with the published page, reporting **crashes, empty output and content differences as
separate outcomes** — because a crashing builder writes nothing, so "the page did not change" is
not evidence that it reproduces. It is in the pre-flight checklist (CLAUDE.md §11); the repair
workflow and its helpers are `docs/_template/README.md` §15.

### Shaders (Module 4+)

HLSL under `shaders/` → SDL_shadercross → SPIR-V / DXIL / MSL, compiled **offline** as a CMake
build step. Compiled artifacts are gitignored: HLSL is source, everything else is derived. Never
commit bytecode — stale bytecode that silently disagrees with its source is a miserable bug.

### Measuring

```sh
# The harness for a lesson, built against the configured SDL3 tree. Since Lesson
# 5.1 there is a library to link, so this names ONE include directory and ONE
# archive instead of listing every engine translation unit by hand — which is
# what 4.9's script had to do, with eighteen of them.
c++ -std=c++20 -O2 -Wall -Wextra \
    -I engine/include -I build/_deps/sdl3-src/include \
    scratch/verify_50.cpp \
    build/demos/libdemo_common.a build/engine/libengine.a \
    -L build/_deps/sdl3-build -lSDL3 -Wl,-rpath,"$PWD/build/_deps/sdl3-build" \
    -o build/demos/verify_50 && ./build/demos/verify_50
```

Link `libdemo_common.a` only when the harness needs the demo's *content* — the scene, the floor,
the reference shot. A harness that checks the engine should link the engine alone, and that
distinction is now expressible.

**Since Lesson 5.11 a harness that touches `engine::debug_ui` needs `build/libimgui.a` on the
link line as well.** `verify_45` through `verify_510` do not, and that is not luck: a static
library only contributes the objects somebody references, and `debug_ui.cpp` is the only engine
translation unit that calls into ImGui.

**Output goes in `build/demos/`** so that `SDL_GetBasePath()` finds `assets/` and `shaders/`,
which the build copies next to each executable. Run the script from the repository root.

Four rules, all of which have caught something (Lesson 3.10, conventions §7e):

- **`-O2`, never a debug build.** A debug build is not slower by a constant factor; it is slower
  by a factor that varies per function, so profiling one produces a ranking of a *different
  program*. Lesson 1.5 measured `put_pixel` versus a row pointer at 5.1× under `-O0` and 14.8×
  under `-O2`.
- **`-O2` on the harness does not reach the library the harness LINKS** — Lesson 7.6. `cmake -S .
  -B build` leaves `CMAKE_BUILD_TYPE` **empty**, so `libengine.a` carries no `-O` flag at all, and
  an unoptimised static library links exactly as quietly as an optimised one. Measured on the same
  machine in the same minute: 153.90 ns/vertex against 8.51, and 26.32 µs per matrix palette
  against 0.99 — **18.1× and 26.6×**. Any harness that times code living in the library must build
  and link a configured tree:

  ```sh
  cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release
  cmake --build build-rel --target engine
  ```

  `scratch/build_verify_76.sh` is the first harness to enforce this: it reads the build type out of
  the cache, prints which archive it is linking and at what type, and **refuses to run** when there
  is none (`ENGINE_ALLOW_UNOPTIMISED=1` overrides, which is how the slow row above was produced).
  The rule above this one was written when a harness *was* the whole program, and had never been
  reached by anything.
- **A `volatile` sink**, or the compiler deletes the work whose cost you are measuring.
- **Both variants in the same run**, back to back, because thermal and scheduling state drift
  between runs and not within one.

In the engine itself, `[3]` shows the frame budget and `[4]` toggles the sRGB encode. Read the
engine time rather than the wall clock: with vsync on, the wall clock is pinned to the refresh
interval and an fps counter cannot tell you that you made anything faster.

### Debugging

- **CPU:** the debugger from day one (Module 0), not printf. Breakpoints, watch, stepping.
- **GPU:** RenderDoc, with a dedicated lesson in Module 4. Frame captures are gitignored.
- **Profiling:** measure before optimizing. Module 3 profiles the software rasterizer; Module 9
  does CPU/GPU profiling case studies *on our own engine*.

### Refactoring

Since Lesson 5.1 there is a documented procedure, and it is not optional for anything structural:

1. **Write the characterization test first**, before a single file moves. `sandbox --shot PATH`
   renders seven pinned frames to one binary PPM — 1,209,600 bytes, hash `905BF27E`. Everything
   that could vary is a constant inside `write_reference_shot()`.
2. **Check the test's own coverage.** The first version had six frames and every one reported
   `straddle = 0`, meaning the near-plane clipper was never called. Read the instrument's counters
   and ask what they missed.
3. **Move without changing. Verify.** Then **change without moving. Verify.** Relocation and
   redesign break in completely different ways; one edit containing both leaves two suspects.
4. `cmp` the golden. Zero bytes differ, or you have work to do.

### Tests

Unit tests under `tests/`, starting with `math` — it is pure, dependency-free, and every later
subsystem's correctness rests on it. Module 9 covers a pragmatic testing strategy for the parts
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
| Axis colours | x/y/z = **red/green/blue**, course-wide, in every diagram — and since 5.11 in exactly one place in the code: `k_axis_{x,y,z}_colour` in `gfx/debug_lines.hpp` |
| Colour storage | **sRGB-encoded**; arithmetic is **linear**. Convert twice, at the edges (6.1) |
| Swapchain | `SDR_LINEAR` where supported, so the hardware encodes on write; `gpu_report::output_encodes_in_hardware` says which (6.1) |
| Shading equation | `L_o = (f_d + f_s)·E_perp·cos θ + albedo·L_ambient` — BRDFs in **sr⁻¹**, light in **irradiance** (6.2) |
| Light units | `directional_light::irradiance`, E⊥ on a surface square-on to the beam; `k_reference_irradiance` = π is the engine's exposure (6.2) |
| Energy test | `R(v) = ∫ f_r cos θ dω ≤ 1`, integrated numerically; every BRDF is run through it (6.2) |
| Material membership | **Can it be a number in a buffer?** Yes → `material` (per-draw uniform). No → pipeline state, and two objects differing in it cost a pipeline bind (6.5) |
| Asset references | Handle to store, pointer to use, `bind_albedo` to resolve — once per draw, never per pixel (6.5) |
| Derived state | `material::textured()` is computed from the handle; storing it as a second field is a bug the type can no longer express (6.5) |
| Surface model | Microfacet: `D` must satisfy `∫ D(h) cos θₕ dω = 1`, checked for every distribution (6.3) |
| Roughness | `α = roughness²` — Disney's remap, a **convention**; convert at the import edge (6.3) |
| Geometry term | Height-correlated Smith, not separable — the independence assumption is off by 1.715× at grazing (6.3) |
| Fresnel | Schlick at **v·h** (the microfacet is the mirror, not the surface); `F0 = ((1−n)/(1+n))²`, so glass's 0.04 is derived, not typed (6.4) |
| Metals | `F0 = lerp(0.04, albedo, metallic)`, `diffuse = albedo × (1 − metallic)` — a consequence of a conductor having no diffuse lobe, not a workflow choice (6.4) |
| Diffuse coupling | `(1 − F(n·l))(1 − F(n·v))` — **two interface crossings**. Reciprocal, and worst R(v) = 0.9255. The common `1 − F(v·h)` reaches 1.3395 (6.4) |
| Conformance to a published BRDF | The spec's **parameters** are the conformance surface, not its shading. glTF §3.9.6 permits BRDF variation; Appendix B requires a physically accurate one be energy conserving — which selects our coupling over the spec's own sample form (6.6) |
| Comparing two BRDFs | Reduce both to named terms and find the ones that are not the same expression; then price the survivor at **both** ends of its range. Five of six terms are identical to glTF's, and the survivor is 1.036× at normal incidence and 5.62× at 88° (6.6) |
| Importing a new format | Audit the conventions against ours **before** writing conversion code, and write the audit down (Conventions §7k). A mismatch does not throw — it produces a plausible picture, and a plausible picture survives review (6.6) |
| Loader layering | Parsers return **descriptions** (URIs, indices); the asset store returns **handles**. A parser that needs a pool cannot be tested from a string literal nor run offline in Module 9 (6.6) |
| A texture's colour space | On the **texture**, not the sampler — because that is where the hardware puts it: an `_SRGB` format decodes, a `_UNORM` one does not, and one image cannot be both at once (6.7) |
| Tangent frames | Derived from the uv chart, orthogonalised against the normal (which is the one held fixed), handedness stored as `tangent.w` because a recomputed bitangent cannot disagree with the N and T beside it (6.7) |
| Transforming a direction | A **normal** takes the inverse transpose, because it is defined by being *perpendicular*. A **tangent** takes the model matrix, because it lies *in* the surface and is a difference of positions (3.6, 6.7) |
| Vertex layouts | **Per-pipeline state.** A shader that does not read an attribute should not declare one — locations are numbered across the whole pipeline, so a widened layout takes a location away from whatever was using it (6.7) |
| Test tolerances | **Derive them from the encoding**, never choose them. A tolerance that is wrong rather than merely loose fails a correct implementation, which is the more expensive mistake (6.7) |
| A shadow's place in the equation | It multiplies **E**, beside the cosine — a shadow is a fact about whether light *arrives* — and never the ambient term, which is exactly what a shadowed surface is left with (6.8) |
| Shadow bias | **Derived, not tuned**: `reach × world_per_texel × tan θ ÷ depth_range`. A constant cancels an error proportional to tan θ with a number that is not, so it must peter-pan by `bias × depth_range` world units (6.8) |
| Which normal a bias uses | The **geometric** one. Acne is a disagreement about where the *triangles* are, and a normal map does not move a triangle — the first place 6.7's two normals must be told apart (6.8) |
| Filtering a comparison | **Compare, then filter.** The values you average must be the values you want the average of, and a depth is not a visibility (6.8) |
| A derived tolerance's own test | **Measure how close the worst real case gets to the bound.** 92% is a derivation; 22% means the bound describes something else (6.8) |
| Anything sized to one sample | Re-derive it when the footprint widens. A 3×3 kernel reaches 2.12 texel-diagonals, not 0.71, so a one-tap bias fails the moment PCF is switched on (6.8) |
| A missing binding | Bind the **identity element** of the feature: white for a multiply, lavender for a basis change, **1.0 for a depth comparison**. A declared sampler slot with nothing bound draws nothing, silently (4.7, 6.7, 6.8) |
| Fitting a light to a scene | Fit to the **casters**, not the receivers. A receiver changes no answer in the map and triples the box, cutting texel density to a ninth (6.8) |
| A format limit | **Report it, never truncate.** A narrowed index renders — as a spray of triangles between the wrong corners — with nothing anywhere saying so. `skipped_too_large` + `max_primitive_vertices` is what a caller can act on (6.6) |
| Specular denominator | `4(n·l)(n·v)`: the 4 is the half-vector Jacobian, the (v·h) cancels against projected area, (n·v) is radiance's, (n·l) is the BRDF's (6.4) |
| Angles | Radians. Always. |
| Performance units | **ns per covered pixel** and **ns per triangle** — never ms/frame |
| Timing statistic | **median** for a frame, **minimum** for a kernel, never the mean |
| Instrumentation floor | `100 × max(clock tick, timer cost)` — measured per machine, not assumed |

Right-handed world space was chosen to match glTF 2.0 (Module 6 loads it with zero axis
conversion) and every reference the course cites. Matching the references matters more than
matching the clip space, because the projection matrix mediates between them anyway.

**Winding deserves a special note:** it is *per-pipeline state* in SDL_GPU, not a global rule, and
both relevant enums default to zero — meaning "CCW front, cull nothing." A forgotten `cull_mode`
therefore manifests as *no culling*, not as an error. We set it explicitly, every time.
