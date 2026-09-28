# Module 4 — learnings from its lessons

Moved verbatim from LEARNINGS.md on 2026-09-27, in the original order; LEARNINGS.md
is now the index. Append new sections at the end, and add each heading there.

---

## I fell into 3.10's own pitfall within a day (Lesson 4.1)

Extracting the fragment body of `fill_triangle` into a lambda (so the scanline and
quad traversals could share it) appeared to make the fill **10% faster** — 46.46 → 40.46 ns per
covered pixel on the lit-and-textured rung. A behaviour-preserving refactor with a free 10% is a
result worth reporting, so it got measured properly first: two binaries differing *only* by the
extraction, run alternately in one session.

```
        old      new
     41.309   40.314
     41.608   41.446
     41.628   42.209
     42.231   41.968
     43.083   42.536

old  min 41.309  median 41.628
new  min 40.314  median 41.968
```

**The minimums say 2.5% faster and the medians say 1% slower**, which together say *no measurable
difference*. The 10% was entirely session drift — note that both columns climb monotonically
through the run as the machine warms up, which is the drift made visible.

The original comparison broke the rule Lesson 3.10 states in its own pitfalls section: *never
compare two numbers taken hours apart; measure both variants in the same run, back to back.* It
took one day to violate it, on the code that lesson was written about.

Two consequences worth keeping. **Lesson 3.10's published numbers stand** — nothing needed
restating. And a refactor's performance claim needs the same control as a feature's: "it should
be the same speed" is a prediction, and predictions get measured.

## Helper lanes are not waste, they are what derivatives cost (Lesson 4.1)

Every GPU shades fragments in **aligned 2×2 blocks**. A triangle covering one pixel of a block
makes all four lanes run the fragment shader; the three it misses are *helper lanes* and their
results are discarded.

This looks like an inefficiency to engineer away, and it cannot be, because `ddx`/`ddy` — the
screen-space derivatives every automatic mipmap selection depends on — are computed as
**differences between neighbouring lanes**: `ddx` is lane 1 minus lane 0, `ddy` is lane 2 minus
lane 0. A lane cannot subtract against a neighbour that never ran.

Two consequences worth carrying:

- **A derivative inside a divergent branch is undefined.** If the neighbouring lane took the other
  side, it has no value at that point in the program. Sample outside the branch, or use an
  explicit-gradient sample.
- **Lane efficiency is a function of triangle SIZE, not count.** Covered lanes live in the area
  and helpers along the perimeter, so efficiency ≈ `A/(A + cP)`. Measured over 32 rotations:

| circumradius | 64 px | 16 px | 8 px | 4 px | 1 px |
|---|---|---|---|---|---|
| efficiency | 96.3% | 86.7% | 78.5% | 67.8% | **25.0%** |

At one pixel, three lanes in four are thrown away. **That is the precise content of "small
triangles are expensive"** — and note it says nothing about how many there are.

Our numbers *understate* it: `vertex::x`/`y` are integers, so a sub-pixel triangle rounds away and
draws nothing at all. Real hardware rasterizes at ~1/256 px and draws them, at efficiencies below
25%.

**Why 2×2 and not wider**, measured at r = 8: 2×2 keeps 75.5%, 4×4 48.7%, 8×8 30.4%, 16×16 8.9%.
The choice is forced from both ends — 2×2 is the smallest block containing a neighbour in x and
one in y, and every wider block wastes more. A 32-lane warp is *eight quads*, possibly from eight
different triangles: the grouping for scheduling and the grouping for derivatives are different
things, and only the second is 2×2.

## Divergence costs what your DATA decides, not what your code says (Lesson 4.1)

Lanes in a warp share a program counter, so a warp whose lanes disagree at a branch executes
**every side any lane takes**, masking the rest. Measured against a warp that never diverges:

| coherence (px in a run) | 1024 | 64 | 16 | 8 and below |
|---|---|---|---|---|
| cost | 1.00× | 1.03× | 1.52× | **1.93×** |

**The step falls exactly at the warp width of 32**, which is the check that says the model is
right rather than merely plausible.

So the same shader, with the same branch and the same work, is fast or slow depending on how the
two sides are *arranged across the screen*. Branching on a material id is fine when whole objects
share materials and ruinous when the id comes out of a texture. This is why "sort draws by
material" and "use separate pipelines instead of a branch" are real advice.

**Report the controlled ratio.** The first draft compared the SIMT loop against a plain CPU loop
and published a 5.0× penalty; those are two different loops, so that figure carries the
emulation's own overhead. Comparing the SIMT loop against *itself* on data that never diverges
gives 1.93×, which is the number that means something.

## The reason state lives in a pipeline object is not the one everybody gives (Lesson 4.1)

The folk explanation for immutable pipeline objects is that they spare the inner loop from
branching on state that never changes during a draw. It is testable, and our fill loop is the
test — it branches per pixel on `style.shade`, which is constant for the whole draw:

```
per-element branch on draw-constant state: 0.458 ns
the same loop, specialised:                0.492 ns  (0.93x)
```

**Nothing.** A perfectly predicted branch is free, so that cannot be the reason.

The real reason is that the driver must **validate** the state combination (is this blend mode
legal with this target format, does this vertex layout match the shader) and **compile** machine
code specialised to it, because a GPU has no runtime "depth test on/off" switch — that decision is
compiled in. Both are measured in milliseconds: free once per pipeline, ruinous per draw call.
SDL's own header says it, listing pipelines under things created once and used over and over:
*"Render pipelines (precalculated rendering state)"*.

Worth knowing the scale of what would have to be compiled: `engine::fill_style` already spans
**96 combinations** (4 shading × 2 encode × 2 interpolation × 2 blend space × 3 traversal), every
one of which our loop decides at runtime, per pixel. A GPU compiles the one you asked for. That
is what a shader is.

## SDL_GPU facts verified at release-3.4.12 (Lesson 4.2)

Everything below came out of `scratch/verify_42.cpp` running against the pinned headers, not out
of memory. Numbers are one machine's (Apple M4 Pro, Metal); the *facts* are not.

| Fact | Value | How it was established |
|---|---|---|
| `SDL_GetGPUShaderFormats` ≠ the mask you passed | asked `SPIRV\|DXIL\|MSL` (0x1a), granted `MSL\|METALLIB` (0x30) | The mask says what the app can supply; the query says what the device accepts. Lesson 4.3 needs the second. |
| A `_UNORM` clear | `byte == round(255 * v)`, exactly, at all five test values | Clear a 1×1 target, download it, read the byte. |
| A `_UNORM_SRGB` clear | the sRGB encode — 0.5 → **188** where `_UNORM` gives 128 | Same method; matches our own `engine::linear_to_srgb_u8` to the code. |
| The sRGB encode and alpha | **RGB only.** One clear of (.5,.5,.5,.5) gives R=188, A=128 | Alpha is coverage, not colour. Measured rather than assumed from the format's name. |
| Out-of-range clear values | **clamped, not wrapped** (−0.5 → 0, 1.5 → 255) | Same method. |
| `SDL_PIXELFORMAT_ARGB8888` in GPU terms | `SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM` (enum 12) | `SDL_GetGPUTextureFormatFromPixelFormat`. ARGB8888 is *packed*, so the byte order is endianness-dependent — ask, never reason. |
| Upload → 1:1 nearest blit → download | **bit-identical**, 921,600 bytes | `memcmp` against the source framebuffer. |
| `SDL_GPUTextureTransferInfo::pixels_per_row` | a count of **pixels**, not a pitch | Passing `width * 4` shears the image — Lesson 1.5's bug, four modules on. |
| Reading a download before the fence | wrong **64 / 64** | Destination poisoned to `0xAB` first, so "not written yet" is visible. 0/64 wrong after `SDL_WaitForGPUFences`. |
| `SDL_ReleaseGPU*(dev, NULL)` | **not documented as safe at 3.4.12** | The null-safety wording exists on `main` but not in the pinned release. `gpu_present.cpp` null-checks rather than relying on it. |

**Present-mode support is per window, not per platform.** This Mac reports VSYNC and IMMEDIATE and
**not MAILBOX**. A program that hard-codes MAILBOX works on the developer's Linux box and fails at
startup elsewhere; `SDL_WindowSupportsGPUPresentMode` is the only honest way to find out.

**A window is measured in points and a swapchain in pixels.** They agree only when the window was
created *without* `SDL_WINDOW_HIGH_PIXEL_DENSITY` — which ours is, so both read 1280×720 here. Use
the width and height `SDL_WaitAndAcquireGPUSwapchainTexture` fills in; they are out-parameters
precisely so nobody has to compute them.

## The cost of a GPU sync is the overlap you gave up, not the wait (Lesson 4.2)

Two measurements, and shipping either one alone would teach something false.

**GPU-bound** — 32 submissions of 8 blits at 1024², waiting on every fence against waiting only on
the last: 9.438 ms vs 5.904 ms, **1.60×**. The pipelined figure works out to 0.184 ms per
submission, which *is* the GPU time per submission — pipelined, the GPU never idles.

**Display-bound** — the probe at 60 Hz, with and without a full fence wait every frame:

| | draw | record | acquire | fence | frame |
|---|---|---|---|---|---|
| fence off | 0.301 | 0.226 | **16.002** | — | 16.667 |
| fence on | 0.284 | 0.223 | **15.272** | 0.761 | 16.667 |

The fence costs 0.761 ms and the acquire falls by 0.730. **The frame time does not change.** There
was 16 ms of waiting already there, so a sync had nothing to take.

That second row is why this bug ships: a renderer that syncs every frame measures perfectly on a
simple scene and falls apart the moment the GPU becomes the limit.

## A bandwidth figure above the bus is a broken benchmark, twice over (Lesson 4.2)

The first version of §C reported **763 GB/s** on a machine whose memory bandwidth is **273**. Two
independent causes, found in that order:

1. **Elision.** Blitting src → dst 48 times is 48 copies of one answer, and the driver is entitled
   to notice. Fixed by ping-ponging, so blit *i+1* reads what blit *i* wrote.
2. **Lossless render-target compression.** Both textures were cleared to a *flat colour*, which
   compresses to nearly nothing — the copy was real, the bytes were not. Fixed by filling with
   xorshift noise.

| texture | flat GB/s | noise GB/s | ratio |
|---|---|---|---|
| 512² | 129.6 | 124.9 | 1.04× |
| 1024² | 466.0 | 399.9 | 1.17× |
| 2048² | 686.3 | 309.1 | 2.22× |
| 4096² | 797.9 | **277.6** | 2.87× |

The noise column converges on the published 273 GB/s, which is the strongest evidence a benchmark
can offer: it landed on a number nobody in the experiment chose. The flat column is not an error
once you know what it is — it is the compressor, and it is why a cleared render target is cheaper
to work with than a busy one. **Check that the number can be true** costs nothing and has now
caught more errors in this course than any other habit.

## A vsync measurement is only comparable on the same display (Lesson 4.2)

An unchanged binary measured **120 fps** and then **60 fps** minutes apart, and for a few minutes
this course believed it had found that fencing halves the frame rate. The window had opened on a
different monitor: a 120 Hz laptop panel first, a 60 Hz external one afterwards.

This is Lesson 3.10's rule about comparing numbers taken hours apart, on a new axis. The fix is
the same: build the variants as separate binaries and run them **alternately in one session**,
twice. Table 3 of Lesson 4.2 was retaken that way after the false result.

**Two things to check before believing any vsynced measurement:** which display the window is on,
and what that display's refresh rate is. `SDL_GetWindowSizeInPixels` and the swapchain dimensions
will not tell you — they were identical in both runs.

## Shader facts verified at SDL 3.4.12 + shadercross 3.0.0 (Lesson 4.3)

| Fact | Value | How it was established |
|---|---|---|
| Register spaces → SPIR-V sets | `space1`→set 1, `space2`→set 2, `space3`→set 3 | `spirv-dis x.spv \| grep DescriptorSet` on our own compiled shaders. |
| A wrong register space | **compiles, translates, loads and runs** | Nothing in the chain objects; the shader reads the slot it named. |
| MSL entry point | **`main0`**, not `main` | SPIRV-Cross renames it; `main` is reserved in MSL. Read it in the generated `.msl`. |
| Passing `"main"` to an MSL shader | **REFUSED** at creation | `SDL_CreateGPUShader` returns null, SDL logs `Creating MTLFunction failed`. |
| Wrong resource counts | **accepted**, every variant | Including `num_samplers = 99` on a shader with one. Nothing validates them at creation. |
| shadercross `-d JSON` | the four counts SDL wants, plus inputs/outputs | `{ "samplers": 1, "storage_textures": 0, "storage_buffers": 0, "uniform_buffers": 1, ... }` |
| `SDL_SetGPUShaderName` | **does not exist** | Only buffers and textures have name setters; a shader is named via `SDL_PROP_GPU_SHADER_CREATE_NAME_STRING` at creation. Assuming the setter existed cost one compile error. |
| `SDL_CreateGPUShader` cost | **0.008 ms** cold, all four in 0.028 ms | Against 61.2 ms of build-time compilation. Cold equals repeat, so not a cache. |

**The API checks the name and not the numbers.** A wrong entry point fails immediately, at
creation, with a message. A wrong resource count sails through and fails later, somewhere else,
possibly on someone else's machine. They need opposite defences: get the name right, and never
type the numbers at all — read them from the reflection file, and refuse to load when it is
missing rather than defaulting to zero.

**Where the compile happens is an open question, deliberately.** Eight microseconds cannot be MSL
→ machine code. The prediction written into Lesson 4.3, for 4.4 to check: it happens at
*pipeline* creation, because only there does the driver know the target formats, depth and blend
state and vertex layout that the code must be specialised to. That is 4.1's measured reason
pipeline objects exist and what SDL_gpu.h means by "precalculated rendering state".

## Two CMake facts that cost time (Lesson 4.3)

**`add_custom_command(OUTPUT ...)` declares a recipe, it does not schedule work.** With no target
depending on the named output, the rule never runs — and the build *succeeds*, with an empty
output directory and no diagnostic. Wrap the outputs in `add_custom_target` and
`add_dependencies(exe that_target)`. Note also that CMake target names may not contain a dot, so
a shader called `triangle.vert` needs `string(REPLACE "." "_" ...)` before it can name a target.

**A tool can be installed and still not runnable.** `/usr/local/bin/shadercross` on this machine
fails with `dyld: Library not loaded: @rpath/libSDL3_shadercross.0.dylib — no LC_RPATH's found`:
the install placed the binary and its library correctly but recorded no search path.
`cmake/Shaders.cmake` derives the prefix from the executable's own location and adds
`<prefix>/lib` to the platform's loader variable (`DYLD_LIBRARY_PATH`, `LD_LIBRARY_PATH`, or
`PATH`), which is harmless when it is unnecessary.

## Pipeline facts, verified at SDL 3.4.12 (Lesson 4.4)

**The shader compile happens at pipeline creation.** Predicted in 4.3 from the fact that
`SDL_CreateGPUShader` takes ~0.03 ms; confirmed here, because pipeline creation is never that
cheap and is sometimes tens of milliseconds.

**How much it costs depends on the driver's cache, and that cache is on disk.**

| what is created | cost |
|---|---|
| a shader object | 0.031 ms — in every run, every configuration |
| the **first** pipeline in a process | ~32 ms — one-time driver and compiler setup |
| each **new state permutation** after it | ~2.4 ms — the compile, about 80× a shader |
| a description compiled before, *including in a previous run* | 0.01–0.6 ms |

**Two wrong conclusions were drawn from this call before it was measured properly**, and both are
worth recording because the confound is the same and it is easy to fall into:

1. Timing one pipeline, then timing the same description again, and calling the difference the
   compile. The second measurement is a cache hit.
2. Comparing a release run (0.5 ms) with a `debug = true` run (33.5 ms) and concluding that the
   **validation layer costs sixty-six times the compile it checks**. It does not. That run was
   simply the first time those shaders had ever been compiled on the machine. Measured with a
   freshly-generated blend permutation in both configurations, validation costs almost nothing:
   **31.8 ms against 34.5** for a first pipeline, and ~2.5 against ~2.4 for each one after.

What caught it: running the engine again the next day and seeing **0.077 ms** where the log had
said 42. *A number that moves five hundredfold between runs of an unchanged binary is not a
property of the call.* To measure a real compile, vary something the driver must specialise for —
the harness uses a run-unique blend permutation — so that "new" means new.

**Consequences for an engine.** Build every pipeline at load time: 2.4 ms is fifteen percent of a
60 Hz frame. And **state permutations are compilations** — a material system with five booleans is
thirty-two pipelines and roughly eighty milliseconds of startup, which is what "shader compilation
stutter" means when you read it in patch notes.

### Pipeline creation validates almost nothing

| description | result |
|---|---|
| correct | created |
| a colour target format the target texture does not have | **created** — and the frame drew |
| an attribute at a location the shader never declared | **created** |
| no vertex layout while the shader has inputs | REFUSED: *"Vertex function has input attributes but no vertex descriptor was set."* |

When it does refuse, the driver's message is excellent. When it does not, you get silence — the
same temperament 4.3 found in shader creation, which checked the entry-point name and not the
resource counts.

**A shader in the wrong slot is worse than an error.** Reproduced three times in an isolated
one-trial program, because it cannot be tested inside a harness that has other work to do:

- a **vertex** shader in the fragment slot → refused, with a Metal error about an interrupted
  compiler connection — after which the *next* pipeline creation in that process crashed;
- a **fragment** shader in the vertex slot → **SIGSEGV**, no error, no return.

Both are `SDL_GPUShader*`, so the type system cannot help, and SDL does not check which stage a
shader was compiled for.

### `SDL_GPUGraphicsPipelineCreateInfo` holds pointers

Its vertex input state points at two arrays and its target info at a third. **A function that
fills one in and returns it by value returns a struct aimed at its own dead stack frame**, and
nothing warns. Hence `pipeline_desc` is a class that owns the arrays and returns the create-info
by `const&`. Same shape as a dangling `string_view` or `span`; same fix as 3.5's
`mesh_data`/`mesh` pair — give the view and the viewed one lifetime.

## Our rasterizer and the hardware compute the same triangle (Lesson 4.4)

The identical triangle through `engine::fill_triangle` and through the GPU, rendered into a
256×256 offscreen target, downloaded, and compared pixel by pixel:

| | pixels |
|---|---|
| covered by both | 20,808 |
| the GPU only | **0** |
| ours only | 102 (0.49%) |
| disagreements strictly inside the triangle | **0** |

Our coverage is a strict superset, and every extra pixel is on one edge, one per row — the
pattern is in the lesson's Figure 5, drawn from the actual comparison.

**The difference is a fill rule, not a bug.** Hardware uses the top-left rule; Lesson 2.2's edge
test uses `>= 0` and includes every boundary pixel. On a lone triangle that is 102 pixels; on two
triangles sharing an edge it is a seam or a double-draw. Sub-pixel precision differs too — our
vertices are integers, hardware rasterizes at roughly 1/256 of a pixel — so the two can never
agree exactly, and they do not need to.

Also measured, and worth having: **coverage matched the geometry's area to 0.9922** (20,808
against 20,972 predicted by ½ × 204.8 × 204.8), and **the centroid read 85, 86, 85** where one
third of 255 is 85 — Lesson 2.4's barycentric interpolation, in silicon, agreeing to one code.

## Reversing two vertices deletes a triangle (Lesson 4.4)

With `cull_mode = BACK` and `front_face = CCW`: counter-clockwise vertices cover 20,808 pixels,
the same three points in clockwise order cover **0**, and with culling set to `NONE` the
clockwise order covers 20,808 again — *identical* to the first case.

That last measurement is what makes this diagnosable: **if setting `CULLMODE_NONE` brings the
triangle back unchanged, the problem is winding and nothing else.** It is the two-minute test for
the commonest "my first triangle is invisible", and it rules out geometry, transforms, buffers and
formats in one step.

Related, and the reason we set all three rasterizer fields explicitly: every enum in
`SDL_GPURasterizerState` has its first enumerator at zero, so a zero-initialised state means "CCW
front, cull nothing" — a forgotten `cull_mode` is not an error, it is no culling at all.

---

## Vertex-layout facts, verified at SDL 3.4.12 (Lesson 4.5)

Measured with `scratch/verify_45.cpp` on Metal, one machine. Every row is a thing the API does
not tell you and does not check.

| Fact | Value |
|---|---|
| The fetch | `address = base + i × pitch + offset`, in fixed-function silicon |
| `SDL_GPUVertexBufferDescription` | `{ slot, pitch, input_rate, instance_step_rate }` |
| `instance_step_rate` | **reserved, must be 0** |
| `SDL_GPUVertexInputRate` | `{ VERTEX = 0, INSTANCE }` — so a zeroed description is per-vertex |
| Attribute locations | must be **unique**; SDL states the rule and no consequence |
| Locations are numbered | across the **pipeline**, not per buffer |
| `SDL_BindGPUIndexBuffer` | takes the element size — the width is **not** in the buffer or the pipeline |
| `SDL_DrawGPUIndexedPrimitives` | `(pass, num_indices, num_instances, first_index, vertex_offset, first_instance)`; `vertex_offset` is `Sint32` and is added to every index |
| `SDL_SetGPUViewport` | **render-pass state** — survives a pipeline change |

### Pipeline creation refuses one broken layout in six

| the layout | `SDL_CreateGPUGraphicsPipeline` |
|---|---|
| correct | created |
| pitch four bytes short | **created** |
| pitch four bytes long | **created** |
| position and normal offsets swapped | **created** |
| an attribute the shader never declares | **created** |
| a shader input nothing supplies | REFUSED — *"Vertex attribute input_uv(2) is missing from the vertex descriptor"* |

The one it catches is the one that would have been obvious anyway: an unfed input draws geometry
stretching to infinity. The four silent ones draw a plausible wrong picture. Same temperament as
Lesson 4.3's shader creation, which validates the entry point's name and none of the four
resource counts.

**Our own `check_layout` catches three of the five, and says so.** It reads the `inputs` array
out of the reflection JSON the build has emitted since Lesson 4.3 and compares. It catches a
too-short pitch (attribute end > pitch), an undeclared location, an unsupplied location,
duplicate locations, and a base-type mismatch. It **cannot** catch a too-long pitch or swapped
offsets — nothing about either declaration is inconsistent; they are simply wrong. A checker
that implies total coverage is worse than no checker.

**Bound the scan to the array.** The same JSON has an `outputs` array with byte-identical key
names, so an unbounded search for `"location"` walks out of one and into the other and reports a
six-input shader as having nine.

### A wrong pitch shatters; a wrong offset deforms

| layout | pixels | bounding box |
|---|---|---|
| pitch 32 (correct) | 3,696 | 88 × 52 |
| pitch 28 | 5,076 | 88 × 84 |
| pitch 36 | 5,027 | 88 × 84 |
| position reads the normal's bytes | 3,126 | 62 × 64 |

Three things worth knowing before you next see this:

1. **Coverage goes up, not down.** The instinct on a wrong picture is to check culling, winding
   and the near plane. A wrong pitch draws *more* than the correct one, because the garbage
   sprays outward.
2. **The error accumulates**: it is *i* × (pitch error), so vertex 1 is 4 bytes off and vertex
   1,224 is 4,896 off. **Vertex 0 is always right**, which is exactly how this bug passes a
   three-vertex test and fails on a real mesh.
3. **Too long and too short look the same.** The symptom says *that* the pitch is wrong, not
   which way. Print `sizeof`.

A wrong **offset** is diagnostically different, and the difference is useful: every vertex is
still read from its own record, just from the wrong bytes of it. A normal is a unit vector, so
reading it as a position collapses the whole mesh onto a sphere of radius 1. **Scattered means
the pitch; coherent but wrong means an offset.**

### A vertex element format answers two different questions

`size_of` is bytes in the buffer; `shader_type_of` is the type in the shader. They are not the
same question:

| format | bytes | shader type | conversion |
|---|---|---|---|
| `FLOAT3` | 12 | `float3` | none |
| `UBYTE4` | 4 | `uint4` | none — integers stay integers |
| `UBYTE4_NORM` | **4** | **`float4`** | ÷ 255, in the fetch unit, free |
| `SHORT2_NORM` | 4 | `float2` | ÷ 32,767 |
| `HALF4` | 8 | `float4` | 16-bit → 32-bit float |

Cashed in: Lesson 4.4's vertex went **28 → 16 bytes (−43%)** by changing one enum, with
`triangle.vert.hlsl` untouched. Same 47,124 pixels covered; 22,494 of them differ by **at most 1
code out of 255**, which is below the precision of the 8-bit render target they are written to.

`UBYTE4` versus `UBYTE4_NORM` is six characters and the same four bytes. Use the first where you
meant the second and a colour arrives as 235.0, 89.0, 79.0 — white after clamping.

### Supplying fewer components than the shader declares

`FLOAT3` into a `float4` input is legal. The hardware fills the missing components, and SDL's
header does not say with what. **Measured on Metal: w = 1**, by making the missing component the
instance scale so coverage reads the answer off the screen — the `FLOAT3` draw covered 9,944
pixels, *exactly* equal to a `FLOAT4` draw at scale 1.0. This matches the `(0, 0, 0, 1)` rule
Vulkan and D3D12 both require. **⚠ VERIFY on other backends** before relying on it.

### What an index buffer buys, on a real mesh

`assets/torus.obj`: 1,225 vertices, 2,304 triangles, 6,912 index slots — every vertex named 5.64
times on average.

| | vertices | vertex bytes | index bytes | total |
|---|---|---|---|---|
| indexed | 1,225 | 39,200 | 13,824 | **53,024** |
| expanded | 6,912 | 221,184 | 0 | **221,184** |

**4.17× smaller**, and note the shape of the win: the index buffer costs 13,824 bytes and removes
181,984, because an index is 2 bytes and a vertex is 32. The two draws produce **0 differing
pixels, maximum channel delta 0** — worth testing precisely because the only possible result is
zero, so a nonzero one has exactly one explanation.

**Vertex-shader invocations are a range, not a figure.** Indexed: 1,225–6,912, depending on how
often the post-transform cache hits, which is a property of the order the triangles appear in.
Expanded: 6,912 exactly, with no reuse possible. That range is what vertex-cache optimisation
(Forsyth's linear-speed algorithm) exists to narrow.

**Where `k_max_mesh_vertices` came from.** `SDL_GPUIndexElementSize` has exactly two values;
16 bits names 65,536 vertices. `mesh.hpp` has declared the ceiling since Lesson 3.5 with that
justification, and `gpu_mesh::create` is the first line in the engine that depends on it.

### Instancing is one enum value

`input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE`, and nothing else changes: same buffer type, same
`SDL_GPU_BUFFERUSAGE_VERTEX` bit, same `SDL_BindGPUVertexBuffers`, same attributes at ordinary
locations. **The shader cannot tell** — `mesh.vert.hlsl` declares six inputs in one struct and
nothing marks three of them as per-instance. Every tool built for vertex layouts therefore works
on instance data unchanged.

The default is the trap, again, exactly as with `rasterizer_state.cull_mode` in Lesson 4.4: every
enum in the description has its first enumerator at 0, so a zero-initialised description means
per-vertex. At vertex rate, instance 0's *vertices* walk the placement buffer and every instance
past the first reads off the end.

The number that makes the case: **196 bytes of placement rewritten per frame against 53,024 bytes
of geometry that never moves again — 0.370%.**

### Two kinds of device buffer, and the difference is write frequency

Not what they hold — how often they are written.

| | staging | `cycle` | for |
|---|---|---|---|
| `gpu_buffer` | created and released per upload | `false` | geometry, written once at load |
| `gpu_stream_buffer` | created once, kept | `true` on both hops | per-instance data, written every frame |

Getting it backwards costs memory in one direction and correctness in the other — and the
correctness bug appears only when the GPU falls behind, which is to say on a machine slower than
the one you are testing on. Same hazard `gpu_present_target` faced in Lesson 4.2, arriving on the
geometry side.

### Interleaved or separate: both answers are right, for different passes

Measured on 1,225 vertices with 64-byte cache lines:

| | interleaved | separate |
|---|---|---|
| cache lines to fetch one vertex | **1** | up to **5** |
| cache lines for a positions-only sweep | 613 | **230** |

A 32-byte vertex is half a line exactly, so it never straddles a boundary and two consecutive
vertices share a line with nothing wasted. But an interleaved line carries 12 useful bytes in 32
when a shadow or depth pass reads positions only — and separate wins that by 2.7×. The production
answer is a hybrid (position in its own buffer, the rest interleaved), and it costs no new
concepts in SDL_GPU, because a "separate" layout is simply more `vertex_buffer` slots.

### `line` is a reserved word in HLSL

`const float line = smoothstep(...)` fails to compile, and the error points at the semicolon
rather than at the name — `error: ';' : Expected` at the column after `float`. It names a
geometry-shader primitive type. Two minutes lost; recorded so it is zero next time.

---

## Uniform-data facts, verified at SDL 3.4.12 (Lesson 4.6)

Measured with `scratch/verify_46.cpp` on Metal, one machine.

### There is no uniform buffer object

The complete list of what a buffer can be: `VERTEX`, `INDEX`, `INDIRECT`,
`GRAPHICS_STORAGE_READ`, `COMPUTE_STORAGE_READ`, `COMPUTE_STORAGE_WRITE`. **No `UNIFORM` bit.**
Uniform data reaches a shader through `SDL_PushGPUVertexUniformData(cb, slot, data, bytes)` and
its fragment twin, whose first parameter is a **command buffer** — you are recording bytes into
the command stream, not binding a resource.

Three consequences, all simplifications: no lifetime to manage (the only GPU thing in this engine
that needs no wrapper class), no cycling hazard (the bytes are copied at the call, into a command
buffer that is not executing), and ordering as the only rule. Measured: push 201, draw, push 77,
draw — the second draw reads 77, with nothing bound or rebound in between.

For bulk data — many object transforms, a bone palette — the answer is a **storage buffer**:
`GRAPHICS_STORAGE_READ`, declared as a `StructuredBuffer` in space0 or space2, *bound* rather than
pushed, so the bytes move once instead of once per draw.

### The packing rule is HLSL's, not the std140 SDL's header names

SDL says: *"The data being pushed must respect std140 layout conventions… vec3 and vec4 fields are
16-byte aligned."* What the HLSL toolchain actually emits is HLSL constant-buffer packing:

> Fields are placed in declaration order and packed tightly, except that **a vector may not
> straddle a 16-byte register boundary** — if it would, it starts at the next boundary. A scalar
> is never moved.

Measured, from the `Offset` decorations in our own compiled SPIR-V:

| field | HLSL packing | std140 would say |
|---|---|---|
| `float4x4 m` | 0 | 0 |
| `float a` | 64 | 64 |
| `float3 b` | **68** | 80 |
| `float c` | 80 | 96 |
| `float2 d` | 84 | 100 |
| `float3 e` | **96** | 112 |

**Follow SDL's advice anyway**, because std140 is a *superset*: a layout satisfying it also
satisfies HLSL packing, so the question of which rule applies stops mattering — including under a
GLSL front end later. The habit that achieves it: **pair every `float3` with a `float`.** The two
fill a register exactly, nothing can straddle, and both rules agree.

`engine::packed_offset` in `gpu_uniform.hpp` is that rule as a `constexpr` function, so blocks
`static_assert` against the rule rather than against numbers someone worked out once. A comment
describing a layout cannot fail; a `static_assert` can.

### A uniform layout bug corrupts the TAIL

Wrote `e = (241, 242, 243)`:

| the struct | e.x | e.y | e.z |
|---|---|---|---|
| naive, no padding | 242 | 243 | **0** |
| padded to 96 | 241 | 242 | 243 |

Shifted by exactly one float, with a zero where the read ran past what was written — and **every
field before the divergence arrived intact**. That is the mirror image of Lesson 4.5's vertex
pitch bug, where vertex 0 was always right and things degraded further in. Both present as "the
beginning looks fine", for opposite reasons.

### The matrix crosses untouched — and the SPIR-V lies about it

Lesson 2.6 chose column-major `mat4` storage and claimed it was what HLSL constant buffers want.
Checked at last, by pushing a matrix whose element at written *(row, col)* is `16·row + col + 1`
and having a probe shader report `m[row][col]` one element per pixel:

```
         col 0  col 1  col 2  col 3
  row 0      1      2      3      4
  row 1     17     18     19     20
  row 2     33     34     35     36
  row 3     49     50     51     52
```

Every element where our storage put it. **No transpose. `memcpy` is the entire conversion.**

And the trap: the compiled SPIR-V decorates the member `RowMajor`, which looks exactly like the
transpose that table proves is not happening. It is an artefact of how DXC maps HLSL's packing
onto SPIR-V's naming. **An intermediate representation is allowed to describe your data in its own
vocabulary** — measure the endpoint you actually care about.

Related traps in the same family: `mul(M, v)` is the column-vector convention (2.5), and
`float4(world, 1.0f)` — a `w` of 0 makes it a direction, so the matrix's fourth column is
multiplied away and the scene spins about a point the camera never leaves. `projection * view`,
in that order, because `A*B` applies `B` first.

### A wrong register space is caught by the BUILD, not the reflection

This **revises** Lesson 4.3's inference that a wrong space would be silent. Moving the fragment
`cbuffer` to `space0` and asking each tool in the chain:

| tool | verdict |
|---|---|
| `glslc`, HLSL → SPIR-V | accepted — it does not care |
| the JSON reflection | **byte-identical** to the correct shader |
| `spirv-dis \| grep DescriptorSet` | 0 instead of 3 — visible |
| `shadercross`, SPIR-V → MSL | **REFUSED**: *"Descriptor set index for graphics uniform buffer must be 1 or 3!"* |

So Lesson 4.3's argument for compiling offline pays off from an unexpected direction: it moved a
would-be black screen into a build error on your own machine. The reflection cannot see it, so
Lesson 4.5's cross-check is no help. **⚠ VERIFY:** a Vulkan build consumes the SPIR-V directly,
with no translation step to refuse — untested here.

`verify_46` §D reads the `DescriptorSet` decorations out of the `.spv` itself, in about twenty
lines with no dependency: SPIR-V is a five-word header followed by instructions whose first word
packs `(word_count << 16) | opcode`; `OpDecorate` is 71 and the `DescriptorSet` decoration is 34.
That turns Lesson 4.3's advice from something a person must remember into something that runs.

### What a push costs, and the ceiling that crashes silently

Best of seven runs of 256 pushes each:

| pushed | per call | effective rate |
|---|---|---|
| 108 bytes | 0.015 µs | 7.1 GB/s |
| 4 KB | 0.058 µs | 70.3 GB/s |
| 16 KB | 0.259 µs | 63.4 GB/s |

Roughly 14 ns of call overhead plus a `memcpy` at ~65 GB/s — which is what "copied into the
command buffer" predicts, and a sign the measurement is measuring the right thing.

**The first version of this measurement was wrong and said so**: scaled rep counts, one run each,
and 16 KB came out at 43 GB/s against 4 KB at 3.8 — an elevenfold difference in the throughput of
a `memcpy`, which cannot be true. Fixed with a fixed rep count and best-of-seven, taking the
*minimum* because every source of error here adds time. Lesson 4.4's rule caught it: check the
number *can* be true before writing it down.

**And there is an undocumented ceiling.** Repeating a large push into one command buffer exhausts
something and the process dies with **no message at all** — no SDL error, no validation output.
Measured in an isolated program: 16,000 pushes of 4 KB (62 MB) are fine, while 64 KB pushes die
somewhere between 24 and 32 of them, and *not at the same count twice*. Non-determinism at a
resource boundary is the signature of a pool being exhausted rather than a limit being enforced.
Kept out of the harness per Lesson 4.4's rule: a test that destabilises the process is not a test.

**Corrected 2026-09-27 — the measurement was right and the explanation was wrong.** SDL's source
at `release-3.4.12` says why: every backend takes pushes from 32 KiB blocks (`UNIFORM_BUFFER_SIZE`,
`src/gpu/SDL_sysgpu.h`) and none checks `length`, so a push larger than a block is copied whole
into a fresh one — a 64 KB push overruns it by 32 KB. Non-determinism was the signature of memory
corruption, not of a pool. The ceiling is per push (32 KiB), and on Vulkan the bound descriptor
covers only 4 KiB of each push (`MAX_UBO_SECTION_SIZE`). Found while writing 4.6.5's solution.

### Reading a uniform back, when no API offers it

Uniform data goes one way, so the only way to find out what arrived is to ask the shader and let
it answer in the one currency it has — the colour of a pixel. `uniform_probe.frag.hlsl` reports
one field per pixel, encoded `value / 255.0` into a `_UNORM` target so a value of *n* returns as
the byte *n*.

Two details that make it trustworthy:

- **`SV_Position` is the pixel centre**, so the first pixel is (0.5, 0.5). **Truncate, do not
  round** — rounding shifts the whole probe by one and produces a table that looks plausible and
  is wrong in every entry.
- **The probe has no vertex buffer.** `SV_VertexID` generates the three corners of a full-target
  triangle, so the pipeline needs no vertex layout — partly convenience, mostly hygiene, since an
  instrument with a vertex layout might be measuring one by accident (4.5). One triangle rather
  than two also means no shared edge, so no pixel is rasterised twice and no value is written
  twice.

---

## Depth and texture facts, verified at SDL 3.4.12 (Lesson 4.7)

Measured with `scratch/verify_47.cpp` on Metal, one machine.

### Depth precision falls off as the square of distance

From Lesson 2.10's projection, with *d* the positive distance in front of the eye:

```
z_ndc  = (f/(f-n)) * (1 - n/d)          dz/dd = (f/(f-n)) * n/d²
```

With near 0.3 and far 100, **z at one metre is 0.7021** — seventy per cent of the entire
representable range is spent in the first metre, and the remaining ninety-nine metres share the
rest. The smallest world-space separation *N* evenly spaced codes can resolve is:

```
Δd = (f - n) * d² / (f * n * N)
```

Measured against that formula, worst case over many probe distances:

| d | D16 measured | D16 predicted | D32_FLOAT measured |
|---|---|---|---|
| 1 m | 0.050 mm | 0.051 mm | 0.000 mm |
| 5 m | 1.3 mm | 1.27 mm | 0.008 mm |
| 10 m | 4.8 mm | 5.07 mm | 0.036 mm |
| 25 m | 31.1 mm | 31.69 mm | 0.206 mm |
| 50 m | 125.7 mm | 126.8 mm | 0.967 mm |
| 90 m | 412.4 mm | 410.8 mm | 2.2 mm |

Agreement to a few per cent across two orders of magnitude, which means the model is not an
approximation of what the hardware does — it *is* what it does. **Two walls 41 cm apart at ninety
metres share a D16 depth code.**

**Fixes, in order of value:** push the *near* plane out (it is in the denominator, so 0.3 → 1.0 is
3× everywhere, and most scenes have nothing within a metre); reversed-Z with a float format; a
wider format; moving the far plane in helps least.

### Reversed-Z is 180× on a float and exactly nothing on a UNORM

A float stores its precision near zero. The ordinary mapping puts the **far** plane at z = 1 — the
coarse end — which is precisely where 1/d² has already thrown the resolution away, so the two
losses compound. Reversing the mapping (near → 1, far → 0) makes them nearly cancel.

Three changes: one subtraction in the vertex stage, `COMPAREOP_GREATER`, and clear to 0.

| d | D32_FLOAT ordinary | D32_FLOAT reversed | D16_UNORM either way |
|---|---|---|---|
| 10 m | 0.036 mm | 0.001 mm | 5.1 mm |
| 50 m | 0.967 mm | 0.007 mm | 125 mm |
| 90 m | 2.2 mm | **0.012 mm** | 412 mm |

**D16 is unchanged**, because evenly spaced codes do not care which end is which. A large effect
where the theory predicts one and *no* effect where it predicts none is what makes a measurement
believable.

### Two measurement bugs worth keeping

Both produced plausible-looking wrong tables.

1. **The answer at a single distance is not a property of the format.** It depends on where that
   distance falls between two representable codes — just below a boundary and a nanometre crosses
   it, just above and you need nearly a whole code. Measured that way D16 reported 1.4 mm at ten
   metres and 2.6 mm at twenty-five: *a curve going the wrong way*, which is the signal that the
   experiment rather than the hardware is being measured. Fix: sample several nearby distances and
   take the **maximum**, which is also the number a renderer has to survive.
2. **Evenly spaced probe distances aliased against the code spacing.** At twenty-five metres the
   step happened to be almost exactly one code wide, so sixteen samples measured one situation
   sixteen times and reported a quarter of the true spacing. Fix: offsets at fractional multiples
   of the golden ratio, which never lock to any period. Third appearance of aliasing in this
   course — a texture (3.9), a vsync measurement (4.2), and now a probe.

### Only D16_UNORM is guaranteed

| format | supported here |
|---|---|
| `D16_UNORM` | yes — the only one SDL guarantees |
| `D24_UNORM` | **NO** |
| `D32_FLOAT` | yes |
| `D24_UNORM_S8_UINT` | **NO** |
| `D32_FLOAT_S8_UINT` | yes |

`D24_UNORM` — the format a desktop renderer would have hard-coded — is unavailable on this
machine. Use `SDL_GPUTextureSupportsFormat` with a preference list and a guaranteed fallback.

### Depth attachment mechanics

- The pass takes `SDL_GPUDepthStencilTargetInfo*` as a **separate** parameter that may be NULL;
  the pipeline carries `enable_depth_test`, `enable_depth_write`, `compare_op`. Both required,
  neither implies the other — which is what makes *test without write* (transparency) and *write
  without colour* (shadow passes) expressible.
- `COMPAREOP_LESS` with `clear_depth = 1.0f`, because SDL_GPU's NDC is 0 at near. **Clear to 0 and
  every fragment fails** — a black screen with no error. The clear value and the compare op always
  change together.
- `STOREOP_DONT_CARE` unless a later pass reads it. On tile-based hardware that skips writing the
  buffer back to memory.
- **The attachment must match the colour target's size, and the window is resizable.** Recreate on
  change. This never reproduces on the machine where the code was written, because nobody resizes
  the window there.
- Depth targets ask for `DEPTH_STENCIL_TARGET` and nothing else. Adding `SAMPLER` (which Module
  6's shadow maps will need) can force the driver into a layout that is slower for the usage you
  actually have.

### The sampler port is two casts, with a test behind it

Lesson 3.9 defined `engine::filter` and `engine::address_mode` to match `SDL_GPUFilter` and
`SDL_GPUSamplerAddressMode` enumerator for enumerator, and `verify_42` §G has asserted the
correspondence on every run since. So `static_cast` here is a rename with a regression test, not a
coincidence being relied on — and if SDL ever inserts an enumerator the test fails there instead
of the cast silently selecting the wrong mode.

**A sampler is an object, which is the whole difference from 3.9** — the fourth time this module
has moved a decision out of a call and into an object, after pipelines (4.4), vertex layouts (4.5)
and uniform blocks (4.6). A texture and a sampler are **separate** objects bound as a pair
(`SDL_GPUTextureSamplerBinding`, `t0` with `s0`, in `space2`), which is better than fusing them:
one image can be read three ways in one frame, and one sampler serves every texture.

Fields the object has that the CPU call had no room for: `min_filter` and `mag_filter`
*separately*, mipmap mode with LOD clamps and bias, three address modes for three axes, anisotropy,
and `enable_compare` — which makes the sampler perform the depth comparison itself and return
filtered occlusion, which is why percentage-closer shadow filtering is nearly free.

### `_SRGB` is one enum and it decides whether the lighting is correct

Lesson 3.9's argument: an albedo is a reflectance, a reflectance multiplies a quantity of light, so
both sides must be linear. That lesson measured the cost of skipping the decode — two texels
blended in encoded space give 0.2139 where 0.5 is correct, 43% of the light.

Measured here: file byte **222 comes back as 186** through an `_SRGB` texture. sRGB-decoding
222/255 = 0.871 gives 0.7305, which is 186.3 out of 255. Exact — and performed by the sampler for
free, *before* the filter rather than after, which is the ordering the software path had to
construct by hand.

**`_SRGB` for colours, `_UNORM` for numbers.** Normal maps, roughness maps and masks are `_UNORM`;
decoding them corrupts data that was never encoded. This is one of the commonest material-system
mistakes.

### Image orientation, tested rather than reasoned

`assets/uv_grid.png` carries a different flat colour in each corner so a program can read the four
corners back. Sampled through a `_UNORM` texture they match the file **byte for byte** — a much
stronger claim than "it looked right", since the decoder, the upload, the sampler and the readback
would all have had to agree. Lesson 3.9's import-time uv flip stands.

`pixels_per_row` in `SDL_GPUTextureTransferInfo` is **pixels, not bytes** — the third appearance of
this bug in the course after Lesson 1.5's framebuffer pitch and Lesson 4.5's vertex pitch. Treat
any field named for a row with suspicion.

### When not to hand-roll: is the hard part the subject?

We wrote `parse_obj` because OBJ's difficulty *is* this course's subject — the mismatch between how
a file describes a vertex and how hardware fetches one. We do not write a PNG decoder: baseline PNG
is an afternoon, but PNG in the wild is DEFLATE plus five filter modes plus Adam7 plus sixteen-bit
channels plus palettes plus `tRNS` plus colour profiles, and a decoder that handles only your test
files fails on a *user's* asset. There is nothing about game engines in the fifth filter mode.

Containment rules that came with it:

- **A dependency reaches exactly as far as its types appear in headers.** `STB_IMAGE_IMPLEMENTATION`
  is defined in one `.cpp`; `image.hpp` mentions no third-party type; replacing stb is one file.
- **Suppress warnings at the boundary, never by editing the dependency** — an edited dependency is
  one you can no longer update.
- **`STBI_NO_STDIO`**, so the decode consumes bytes `SDL_LoadFile` already read. Two file-opening
  paths in one program is two answers to "why can it not find my asset".
- **Always ask for four channels.** Costs a byte per pixel on opaque images, and means nothing
  downstream ever branches on what shape a file happened to be.
- stb has no tags, so pin a **commit SHA** — the same situation Lesson 4.3 hit with
  SDL_shadercross.

### A duplicate symbol was the right answer arriving as a build failure

Writing a second `name_of(SDL_GPUTextureFormat)` in `gpu_texture.cpp` failed to link against the
one Lesson 4.2 put in `gpu_device.cpp`. The fix was to *extend* the existing table with the depth
formats rather than start a second one — which is how a table like that should grow: when a lesson
starts printing something, it adds the row.

---

## Scene-porting facts, verified at SDL 3.4.12 (Lesson 4.8)

### A material and a cull mode cannot ride on a triangle

Lesson 3.8 wrote `raster_triangle::surface` and flagged it, in the field's own doc comment, as a
cheat that would not survive Module 4. It did not. The *reason* is worth stating in terms of the
machine rather than the API, because "the GPU is less flexible" is the wrong intuition and sends
people looking for a way around it:

A software rasterizer is one thread walking one loop, so "between two iterations" is a moment that
exists and it can change its mind there. **A GPU draw is a launch, not a loop** — thousands of
fragments enter at once, across dozens of cores, in an order nobody controls, with no
synchronisation. There is no moment between triangle 7 and triangle 8, because triangle 8 may have
finished first. Anything that must be the same for every fragment in flight is therefore fixed
*before* the launch: as **pipeline state** (cull, fill, depth op, blend, the shaders) or as a
**uniform push** (the numbers). Four objects with four materials is four draws.

### The fourth rate of change, and SDL's one licensing sentence

Per vertex and per instance are vertex buffers (4.5); per frame is a uniform push (4.6). An
object's model matrix is none of those. `SDL_PushGPUVertexUniformData`'s documentation settles it
in one line: *"Subsequent draw calls in this command buffer will use this uniform data."* A push
is not bound to a pass and not bound to a pipeline, so pushing between draws is the supported
per-draw mechanism and needs no object.

Measured on a three-object scene: **128 bytes per frame + 144 per draw = 560 bytes**, and the same
560 if each object had a million triangles. **Per-draw overhead scales with object count, not
object size.** Group uniform data by *rate of change*, never by subject.

### Push a 3×3 as three explicit columns, never as `float3x3`

Both spellings occupy 48 bytes — a matrix in a constant buffer is one column (or row) per 16-byte
register, so a 3×3 wastes 12 bytes either way. What the matrix *type* adds is a dependence on the
compiler's matrix-packing default: `column_major` for DXC, `row_major` if any `#pragma pack_matrix`
upstream says so, **with no diagnostic when it is not what you assumed**. A transposed normal
matrix does not crash; it lights every non-symmetric object from a slightly wrong direction, which
reads as a modelling problem.

Measured (`verify_48` §C, `shaders/matrix_probe.frag.hlsl`, which declares the same 48 bytes twice
at two slots): on this toolchain `float3x3` **would have worked** — DXC packs column-major and the
matrix-typed reading matches exactly. It is still not what the engine ships. A default that happens
to be right on the machine you tested is the most expensive kind of correctness there is.

**Corollary for debugging:** never test a suspected transpose with a symmetric matrix. Use one
whose transpose is unmistakably different — the probe uses columns (1,2,3), (40,50,60),
(700,800,900), which give (12, 15, 18) one way and (1.23, 45.6, 789) the other.

### A vertex shader sees one vertex, so normals become data

`collect_triangles` computed a face normal from `cross(b-a, c-a)` inside its per-triangle loop
whenever `normal_at()` returned zero — which is three of the four built-in meshes (cube, quad,
icosahedron) plus the ground plane. A vertex shader cannot: it is handed one vertex and cannot see
the other two corners. (A geometry shader could. SDL_GPU has no geometry stage, and geometry
shaders are slow enough on modern hardware that their absence is closer to a feature.)

So the fallback moves into the **importer**, which is where every real engine puts it
(`aiProcess_GenNormals`). Consequences:

- **Flat forces unshared vertices.** Three faces meet at a cube's corner and each wants a different
  normal; a vertex holds one. A cube's 8 positions become **36**, an icosahedron's 12 become **60**,
  and the index buffer is 0,1,2,3,… with no sharing left to express.
- **A runtime toggle became a build-time decision.** Lesson 3.8's flat/smooth key is now a property
  of the vertex buffer, and the two options no longer share one. First time in this course that
  moving to the GPU took something away.
- **Area weighting is free.** `|cross(b-a, c-a)|` *is* twice the triangle's area, so accumulating
  the **un-normalised** cross products and normalising once at the end weights every face by its
  area at no cost. Normalising each face normal first is more work *and* discards the weighting —
  and unweighted averaging lets a corner's twenty tessellation slivers outvote its one large face,
  which shows up as lighting that ripples along seams.
- **Geometry that already has normals is returned unchanged**, whatever style was asked for. A
  file's normals are authorship — Lesson 3.5's loader rule, one level up.

### The naive normal matrix is invisible on boxes BY CONSTRUCTION

Not "usually invisible". Provably zero. With linear part `R*S` and `S` diagonal, the naive matrix
is `R*S` and the correct one is `(R*S)^-T = R*S^-1`. Feed either an axis-aligned normal `e_x`:

```
naive:    R*S*e_x   = s_x * (R*e_x)
correct:  R*S⁻¹*e_x = (1/s_x) * (R*e_x)
```

**Parallel.** They differ only in length, and the fragment's `normalize()` throws length away. A
box's model-space normals *are* its own axes, so the bug cannot show.

Measured: **0 px** change on two non-uniformly scaled boxes; **2,160 px** by up to 143 codes once
the icosahedron is squashed to (1.7, 0.5, 1.0), whose twenty face normals are not axes. Worked
example — `n = (0.7071, 0.7071, 0)` under `S = diag(1.7, 0.5, 1.0)` gives `(0.9594, 0.2822, 0)`
naive and `(0.2822, 0.9594, 0)` correct: **57.2° apart**.

**The general rule is about testing, not about normals.** A test scene made of crates would have
reported a clean pass while the renderer was broken. Choose test geometry that is *able to fail* —
the same discipline as Lesson 3.1's cyclic planks and Lesson 4.5's deliberately wrong pitch.

### How closely a software rasterizer and a GPU can agree

Same scene, same camera, same light, and literally the same `mesh_data` handed to both, at
320×180:

| what | result |
|---|---|
| the shading equation (`shade()` vs `scene.frag.hlsl`, 4,096 fragments × 3 models) | worst 1.192e-07 — **one float ULP** |
| pixels both renderers covered, byte-identical | **86.99%** |
| …differing by one code | 7.88% |
| …differing by more than 16 codes | 2.46%, **all on silhouettes** |
| covered by only one renderer | 42 px (CPU) + 60 px (GPU) — a one-pixel sliver per object |

The coverage sliver is not a bug in either: both use the same fill rule (centre-in, top-left
tie-break) at different **sub-pixel resolutions** — our fill rounds projected corners to whole
pixels, the hardware snaps them to a fixed sub-pixel grid. A >16-code disagreement at a boundary is
one pixel of coverage difference wearing a large number.

### The floor of a CPU/GPU pixel comparison is one code, and it is worst in the darks

`engine::to_encoded`'s `powf` and the hardware's `_SRGB` render-target write are two approximations
of a **curve**, not two readings of a table. They agree exactly above linear 0.006. Below that the
sRGB slope (12.92 near zero) makes a linear value cross a code boundary more than twelve times
faster, and the measured 14/15 boundary sits at **0.004580** for us and somewhere between
**0.0045148 and 0.0045186** for this hardware — which then holds 15 through 0.0050, where the exact
answer is 16.

Neither is wrong; sRGB is specified as a function and a fixed-function converter in a ROP is
entitled to a tolerance. **A claim of bit-identity across this boundary would be a claim about the
hardware.**

### Never report a percentage without a magnitude

The untextured ground plane reports **100% of 39,202 pixels differing** — all by one code, in one
channel, because it is one flat colour whose blue (0.0045186) lands inside that gap. That finding
is worth nothing. Report a **histogram**: "100% differ, all by one code, in one channel" and "2%
differ, by up to 134 codes" are opposite results and only the second is a bug.

The same discipline saved the textured-floor result. Textured, that quad is 66.76% exact with
14.38% differing by >16 codes — which looks alarming until the **untextured control** on identical
geometry comes back at one code maximum. The difference is texture *undersampling*: where one
screen pixel covers many texels the answer depends on which texel you land in, and neither renderer
is wrong. Turn one thing off; if the difference goes with it, you have a cause.

### Three harness bugs worth recognising by shape

1. **A reference implementation that has been simplified is not a reference.** The software
   comparison path was written without near clipping, so it skipped every triangle crossing the
   near plane — and the ground quad's near edge is behind the camera. The GPU "covered" 35,532
   pixels the CPU did not and the port looked catastrophic. The fix was to call
   `engine::clip_polygon_near`: Lesson 3.3's code, doing Lesson 3.3's job.
2. **A pass whose attachments disagree with the bound pipeline is undefined, and can look fine.**
   The sRGB sweep ran without a depth attachment while its pipelines declared one, and produced an
   entirely plausible table. Caught only by a second measurement disagreeing with it.
3. **A sweep is only evidence where it has samples.** The first sRGB sweep took fourteen points
   across the whole range, none near the boundary the disagreement lived at, and "refuted" a
   hypothesis that was merely unmeasured. **Measure the thing you are about to blame** — and
   measure it at the resolution the effect lives at.

---

## Frame-debugging facts, verified at SDL 3.4.12 (Lesson 4.9)

### RenderDoc does not support Metal, and will not

Quoted from its own front page: "a graphics debugger currently available for **Vulkan, D3D11,
D3D12, OpenGL, and OpenGL ES** development on **Windows, Linux, Android, and Nintendo Switch**."

SDL_GPU picks Metal on macOS, so a Mac reader cannot use it at all. `SDL_gpu.h` has a **"Debugging"
section** — a page long, on your disk — that says the same thing and gives the Xcode procedure:
*Debug → Debug Executable…*, set "GPU Frame Capture" to "Metal" in the scheme's Options, run, click
the Metal icon. No Xcode *project* is needed; it attaches to a CMake-built binary.

PIX is D3D12-only and is the better **GPU profiler**; RenderDoc is the better **state inspector**.

### Name resources at creation; the setter is the API SDL steers you away from

`SDL_gpu.h`, on `SDL_SetGPUBufferName`:

> "You should use `SDL_PROP_GPU_BUFFER_CREATE_NAME_STRING` with `SDL_CreateGPUBuffer` instead of
> this function to avoid thread safety issues."
>
> "This function is not thread safe, you must make sure the buffer is not simultaneously used by
> any other thread."

The same applies to textures and transfer buffers. **There is no getter** — no
`SDL_GetGPUBufferName` — so a name is write-only from the program's side and cannot be asserted on
in a test; only a capture shows it.

Cost, measured: creating a 256-byte buffer is 0.0011 ms unnamed and 0.0018 ms named. Paid once, at
load, never per frame.

### A confident explanation of somebody else's API is a hypothesis

This is the transferable finding, and it cost four lessons. `gpu_shader.cpp` carried a comment
explaining that shaders are named through a creation property "because a shader is immutable the
moment it exists — there is no later at which to set anything on it", inferring a reason from the
asymmetry with the buffer and texture setters.

There was no asymmetry to explain. The property is the recommended path for all three; the setters
are simply the older API. The comment was written by somebody who had read `SDL_CreateGPUShader`'s
docs carefully and `SDL_SetGPUBufferName`'s not at all — and it survived four lessons, a full
published code listing and several readings, **because nothing depends on a comment being right**.
No compiler, no test, no reviewer.

Be most suspicious of comments that explain *why somebody else's API is shaped as it is*. Comments
about your own code are checked constantly by people reading the code beside them; comments about a
dependency's design are checked by nobody.

### Debug groups: a C++ scope, because Metal makes it one

```c
void SDL_PushGPUDebugGroup(SDL_GPUCommandBuffer* cb, const char* name);
void SDL_PopGPUDebugGroup(SDL_GPUCommandBuffer* cb);
void SDL_InsertGPUDebugLabel(SDL_GPUCommandBuffer* cb, const char* text);
```

From the header: "On some backends (e.g. Metal), pushing a debug group during a
render/blit/compute pass will create a group that is **scoped to the native pass** rather than the
command buffer. For best results, if you push a debug group during a pass, always pop it in the
same pass."

So `engine::debug_group` is RAII **and deliberately not movable** — a movable one could be returned
from a function or stored in a container and outlive its pass, and the failure is not a crash, it
is a capture whose tree is quietly wrong.

**On D3D12 all three calls require `WinPixEventRuntime.dll`** in PATH or beside the executable.
Without it they are *inert, not an error*: no tree, no diagnostic. This is the only cross-platform
difference in Module 4 that produces no message at all.

Measured cost: ~183 ns per push+label+pop. Four a frame is 0.73 µs, 0.004% of a 16.7 ms budget.

### Measure the noise floor before comparing anything against it

This took three attempts and the failures are more instructive than the number.

1. **No floor at all.** Compared one instrumented frame against one bare frame and got
   `+ debug group and label  0.0238 ms  (-0.0003)` — **adding work made the frame faster**. That is
   not a surprising result, it is the measurement announcing it has nothing to say.
2. **A two-run floor.** Ran the identical workload twice and took the difference. Still let
   `-541.7 ns` through the guard, because *one difference is itself a sample of a noisy quantity*.
3. **A five-run spread, plus a 2× threshold.** Range of five medians = 2.46 µs, and nothing counts
   as a result unless it beats twice that.

Then, when an effect is below the floor, **scale the workload until it clears and divide**: 1 and 8
debug groups are unmeasurable; 32 and 128 give 183.6 ns and 182.0 ns. **The agreement between two
independent estimates is the evidence**, not either number — a real per-call cost is constant, so
convergence is what separates a measurement from a coincidence. (Lesson 3.10 used the same trick on
a profiler zone reading 0.00 µs.)

### The validation layer costs 1.17× — and that ratio is misleading

Recording one **three-draw** frame: 0.0267 ms with debug mode on, 0.0228 ms off. Measured on the
*recording*, not the whole frame, because validation runs on the CPU as each call is recorded and a
whole-frame number would dilute it with GPU time the layer cannot affect.

The cost is **per API call**, so it scales with how chatty the frame is; a frame with two thousand
draws pays roughly a thousand times this. The useful form of the finding is not a ratio: it is
"validation costs about X per call, and I know how many calls I make". Keep it on while developing
— the alternative to a validation message is a black window with no message at all.

### Vertex reuse is a property of the index ORDER

Lesson 4.5 promised that "4.9's RenderDoc capture is where the real number finally shows up". **It
does not** — no Metal support, and Metal's pipeline-statistics counters are not reachable through
SDL_GPU. Where to read it on hardware that answers: RenderDoc's Pipeline State statistics or
`VK_QUERY_TYPE_PIPELINE_STATISTICS` / `VERTEX_SHADER_INVOCATIONS`; D3D12's
`D3D12_QUERY_TYPE_PIPELINE_STATISTICS` → `VSInvocations`; Xcode's Metal Debugger per-pipeline
counters.

Simulating a FIFO post-transform cache over `torus.obj`'s own index order gives a **staircase**,
not a slope:

| cache size | invocations |
|---|---|
| 0 (none) | 6,912 |
| 4 | 4,608 |
| 6 – 32 | 2,400 (identical) |
| 48 | 2,306 |
| 50+ | 1,225 (the best case) |

`make_torus(48, 24)` emits quads ring by ring, so reuse happens at *two* distances — a few index
positions (the neighbouring quad) and about 2 × 24 (the quad one ring away) — with nothing in
between. A cache of 8 and a cache of 32 therefore catch exactly the same reuse.

**The control is the real finding.** Take the same 2,304 triangles, the same vertices, and shuffle
the order the triangles are listed in: 2,400 invocations becomes **6,784 at cache 32, 2.83× worse**,
with nothing about the geometry changed. This is why real pipelines run an index-buffer optimiser
(Tom Forsyth's linear-speed reorder, `meshoptimizer`) as a build step, and why a mesh exported
straight out of a modelling tool often leaves half its vertex throughput on the floor.

Model assumptions, each a way it can be wrong: FIFO (may be LRU); whole vertices (may be
fixed-size post-transform outputs); strictly in-order indices (hardware batches and may reorder);
one cache (usually several).

### Instrumentation must be recorded from the statement that issues the call

`frame_log` is emitted from inside `gpu_scene_renderer::render`, beside each SDL call, rather than
from a function that walks the draw list and describes what it believes the renderer does. The
second design drifts the first time somebody edits the renderer, drifts *silently*, and produces a
log that is **wrong and believed** — which is worse than having no log.

`verify_49` §D asserts the log's draw count, uniform bytes and triangle count against
`draw_stats`. Two counters incremented from the same statements. **This is a test a capture cannot
give you**: a capture is a picture of what happened and has no independent account of what should
have happened to compare against.

### How a capture works, and every limit follows from it

The tool records the full **contents** of every resource at frame start plus the ordered call
list, then **replays**. That is why it can show any resource at any event, why captures are
enormous (proportional to what you have allocated, not to what you drew), why capturing is slow,
and why the tool must sit at the driver level — which is what makes Metal support a port rather
than a feature.

Four questions worth arriving with: is my draw there at all (missing → CPU-side; present but
invisible → culled, clipped, depth-rejected, or offscreen); is the right thing bound; are the
bytes what I think; where did the geometry go. **Predict before you look** — browsing a capture
without a prediction produces the feeling of investigating and no information.

---

## A measurement can be right and its explanation wrong — read the source before naming a cause

Lesson 4.6 measured SDL_GPU's uniform-push crash correctly (4 KB × 16,000 fine, 64 KB dying between
24 and 32 pushes, never at the same count) and then *explained* it — "the signature of a pool being
exhausted" — without looking. The explanation stood through the next fifty-four lessons. Writing the exercise that
asks students to find the pool is what exposed it: a pool predicts a boundary at a constant
*product* of size and count, and the source (`UNIFORM_BUFFER_SIZE`, 32 KiB, no length check in any
backend) predicts a boundary at a *size*, which the 4 KB result already fitted and the pool did not.
When the library's source is on disk (`build/_deps/sdl3-src`), a mechanism is a grep away; a
plausible story about the numbers is a hypothesis, and should be written as one.

