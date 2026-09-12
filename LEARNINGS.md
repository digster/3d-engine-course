# Learnings

Hard-won facts about this codebase and its dependencies. Read this before writing lessons or
code. Every entry here exists because getting it wrong costs real debugging time — or, worse,
ships a confidently wrong statement to a student who will type it in.

---

## SDL_GPU conventions — verified, not assumed

Master prompt §10 forbids guessing these. All of the below was checked against the SDL3 wiki
and `include/SDL3/*.h` on `main` (SDL version at time of writing: **3.5.0**). Re-verify rather
than trusting memory if anything looks off.

| Fact | Value | Source |
|---|---|---|
| NDC extents | lower-left `(-1,-1)`, upper-right `(1,1)` → **+Y is up** | [wiki CategoryGPU](https://wiki.libsdl.org/SDL3/CategoryGPU) |
| NDC depth | **z ∈ [0,1]**, `0` = near plane | [wiki CategoryGPU](https://wiki.libsdl.org/SDL3/CategoryGPU) |
| Viewport coords | top-left `(0,0)` → bottom-right `(w,h)`, **+Y down** | [wiki CategoryGPU](https://wiki.libsdl.org/SDL3/CategoryGPU) |
| Texture coords | top-left `(0,0)` → bottom-right `(1,1)`, **+Y down** | [wiki CategoryGPU](https://wiki.libsdl.org/SDL3/CategoryGPU) |
| Vulkan +Y-down NDC | SDL **converts behind the scenes**; do *not* flip Y in shaders | [wiki CategoryGPU](https://wiki.libsdl.org/SDL3/CategoryGPU) |
| Clip-space handedness | **Left-handed** (+X right, +Y up, +Z into screen), D3D12/Metal-style | derived from the NDC + depth facts above |

**+Y up in NDC but +Y down in viewport and UVs is not a contradiction** — they are different
spaces. This trips people constantly. NDC is the output of the vertex shader; the viewport and
texture spaces are pixel/texel addressing. Keep them mentally separate.

### Winding is NOT an SDL-wide convention

This is the subtle one. Winding is **per-pipeline state**, not a global rule, so any sentence
of the form "SDL_GPU uses CCW" is wrong. From `SDL_gpu.h`:

```c
typedef enum SDL_GPUFrontFace
{
    SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,  /* = 0 */
    SDL_GPU_FRONTFACE_CLOCKWISE
} SDL_GPUFrontFace;

typedef enum SDL_GPUCullMode
{
    SDL_GPU_CULLMODE_NONE,   /* = 0 */
    SDL_GPU_CULLMODE_FRONT,
    SDL_GPU_CULLMODE_BACK
} SDL_GPUCullMode;
```

Both enums have their first entry at zero, so a **zero-initialised** `SDL_GPURasterizerState`
means "CCW is front-facing, cull nothing." That default is why a forgotten `cull_mode` shows up
as *no culling at all* rather than as an error — a classic silent bug. We choose CCW-front /
cull-back explicitly on every pipeline. See `docs/conventions.html`.

---

## The NDC-parity decision (highest-leverage choice in the course)

The **software rasterizer (Modules 2–3) targets SDL_GPU's exact NDC**: +Y up, z ∈ [0,1] with
0 at the near plane.

**Why:** it makes Module 4's port to the GPU an *API change, not a math change*. The projection
matrix, the viewport transform, and the depth test all carry over untouched. If the software
rasterizer used, say, OpenGL-style z ∈ [-1,1], every one of those would need re-deriving at
exactly the moment the student is already drowning in a new API. The two-stage rasterizer→GPU
spine only holds together because the two stages agree numerically.

**Consequence:** the perspective projection matrix we derive in Module 2 is the **D3D-style
[0,1]-depth** one, not the OpenGL `[-1,1]` one found in most tutorials. When cross-referencing
LearnOpenGL or Scratchapixel, expect their projection matrix to differ in the third row. Say so
in the lesson rather than letting the student discover the mismatch alone.

---

## World space is right-handed; clip space is left-handed

World/view space: **right-handed, Y-up, −Z forward.** Clip space: **left-handed** (fixed by
SDL_GPU, see above). The **projection matrix absorbs the handedness flip** — that is a normal,
correct thing for a projection matrix to do, and it is worth teaching explicitly rather than
hiding, because "my scene is mirrored" is a rite-of-passage bug.

Right-handed world space was chosen because glTF 2.0 is right-handed Y-up (Module 6 loads glTF
with zero axis conversion) and because every reference the course cites — Scratchapixel,
Real-Time Rendering, LearnOpenGL — is right-handed. Matching the references matters more than
matching the clip space, since the projection matrix mediates between them anyway.

---

## SDL_shadercross has no releases — pin a commit (corrected in Lesson 4.3)

`SDL_shadercross` (HLSL → SPIR-V / DXIL / MSL) is real, actively maintained by libsdl-org, and
the sanctioned path for SDL_GPU shaders. An earlier version of this note said it "ships as
3.0.0-preview"; that was imprecise, and here is what was actually checked on 2026-08-15:

| Asked | Answer |
|---|---|
| `SDL_shadercross.h` version macros | `3`, `0`, `0` — no preview suffix |
| `sdl3-shadercross.pc` | `Version: 3.0.0` |
| upstream `CMakeLists.txt` | `VERSION 3.0.0`, no suffix |
| upstream tags | **none** |
| upstream releases | **none** |

So the version *string* is a plain 3.0.0, and there is nothing to pin it to. Our `CMakeLists.txt`
pins SDL with `GIT_TAG release-3.4.12` and a comment reading "never a branch name" — that rule
cannot be followed here, because no tag exists. **Pin a commit SHA**, which is exactly as
reproducible.

It is built on **SPIRV-Cross** (SPIR-V → high-level source) and **DirectXShaderCompiler** (HLSL
→ SPIR-V or DXIL), and offers both a runtime library and an offline CLI. We use the offline CLI
via CMake, with runtime translation mentioned but not relied upon.

### A build without DXC cannot read HLSL at all

Not "cannot emit DXIL" — cannot read the source language. Both dependencies are optional at
build time and a clone without `--recursive` produces exactly this. The machine Lesson 4.3 was
written on has such a build:

```
ERROR: Failed to compile SPIR-V From HLSL: Shadercross was not built with DXC support,
       cannot compile using DXC!
```

**The version number does not reveal this**, which is why `cmake/Shaders.cmake` runs the tool
once at configure time on a real shader and reads the exit code. When a tool's behaviour depends
on how it was built, run it; do not reason about it.

The fallback for the first hop is `glslc -x hlsl` (shaderc), which compiles HLSL to SPIR-V and is
already on many machines via the Vulkan SDK. You lose DXIL — that hop *is* DXC — and nothing
else.

---

## SDL3 API signatures that differ from SDL2 muscle memory

Verified against the headers at **`release-3.4.12`** (the tag we pin — see below). SDL2 idioms are
a constant source of plausible-looking fiction, and a snippet copied from the wider internet is
very likely SDL2. The compiler catches most of these, but only if you use the SDL3 form.

| SDL3 | SDL2 (do NOT use) | Note / source |
|---|---|---|
| `SDL_Window *SDL_CreateWindow(const char *title, int w, int h, SDL_WindowFlags flags)` | `SDL_CreateWindow(title, x, y, w, h, flags)` | **No x/y** (`SDL_video.h`). Use `SDL_SetWindowPosition` if needed. |
| `bool SDL_Init(SDL_InitFlags flags)` | `int SDL_Init(...)` (`==0` was success) | **Returns bool**; test `if (!SDL_Init(...))` (`SDL_init.h`). `SDL_INIT_VIDEO`=0x20, implies EVENTS. |
| `bool SDL_PollEvent(SDL_Event *event)` | `int SDL_PollEvent(...)` | **Returns bool** (`SDL_events.h`). |
| `SDL_Renderer *SDL_CreateRenderer(SDL_Window *window, const char *name)` | `SDL_CreateRenderer(win, index, flags)` | `name`=`nullptr` for default backend (`SDL_render.h`). |
| `event.key.key` (SDL_Keycode), `event.key.scancode`, `.down`, `.repeat` | `event.key.keysym.sym` | **The `keysym` nesting is gone** (`SDL_events.h`, `SDL_KeyboardEvent`). |
| `#include <SDL3/SDL_main.h>` separately, once, in the `main` file | (SDL2main link) | **`<SDL3/SDL.h>` does NOT include it** — it's "special". Omit → Windows link error on WinMain. Standard sig `int main(int argc, char *argv[])`. |

Other verified constants/functions used so far: `SDLK_ESCAPE`=0x1b (`SDL_keycode.h`);
`SDL_WINDOW_RESIZABLE`=0x20 (`SDL_video.h`); `SDL_EVENT_QUIT`=0x100, `SDL_EVENT_KEY_DOWN`=0x300
(`SDL_events.h`); `void SDL_Log(const char *fmt, ...)` (`SDL_log.h`);
`const char *SDL_GetError(void)` (`SDL_error.h`); `SDL_SetRenderDrawColor/RenderClear/RenderPresent`
all return bool, `SDL_DestroyRenderer/DestroyWindow/Quit` return void (`SDL_render.h`, `SDL_video.h`).

### Input specifics (verified at `release-3.4.12` for Lesson 1.2)

| Fact | Detail |
|---|---|
| `const bool *SDL_GetKeyboardState(int *numkeys)` | **`bool`, not SDL2's `Uint8`.** Both are 1 byte, so the wrong type may appear to work. `SDL_keyboard.h`. |
| `SDL_SCANCODE_COUNT` | `= 512` (`SDL_scancode.h`). SDL2's name was `SDL_NUM_SCANCODES`. |
| Scancode values | Letters run alphabetically from `SDL_SCANCODE_A = 4`, so W = `4 + 22` = **26**; SPACE = 44. Useful for worked examples. |
| `SDL_MouseButtonFlags SDL_GetMouseState(float *x, float *y)` | Buttons via return value, position via **`float`** out-params. `SDL_mouse.h`. |
| `SDL_BUTTON_MASK(n)` | `= 1u << (n-1)`; buttons are **1-based** (`SDL_BUTTON_LEFT` = 1 … `X2` = 5). SDL2 spelled it `SDL_BUTTON(n)`. |
| `event.key.{scancode,key,down,repeat}` | `down` and `repeat` are **`bool`** in SDL3 (`Uint8` in SDL2). |
| `event.wheel.{x,y}` | **`float`**, plus `direction`; `SDL_MOUSEWHEEL_FLIPPED` means the values are inverted and must be multiplied by −1. Natural scrolling is the macOS default, so skipping this ships an inverted-scroll bug. |

**`SDL_PollEvent` pumps.** It forwards to `SDL_WaitEventTimeoutNS(event, 0)`, which calls
`SDL_PumpEventsInternal` (`src/events/SDL_events.c`). Since pumping is what refreshes the keyboard
and mouse state arrays, the frame order **drain → sample → simulate** is mandatory, not stylistic.
Sampling first costs a full frame of input latency.

**Focus loss does not strand held keys — but only if you drain.** `SDL_SetKeyboardFocus()` calls
`SDL_ResetKeyboard()` when focus leaves every SDL window (`src/events/SDL_keyboard.c:350`), and
`SDL_ResetKeyboard` fixes state by **sending key-up events**, not by zeroing the array. So SDL's
own fix for "alt-tab and the character keeps running" is delivered through the event queue and is
missed entirely by an input system that samples before draining.

**The state array cannot see a tap between samples.** The header says so plainly: a key pressed
and released before you process events never shows up in `SDL_GetKeyboardState`. Fine for a
keyboard at 60 fps (a human tap is 30–50 ms), but worth naming again in 1.4, where a fixed
timestep can run several sim steps per input sample.

### Timing specifics (verified at `release-3.4.12` for Lesson 1.3)

| Fact | Detail |
|---|---|
| `Uint64 SDL_GetTicks(void)` / `SDL_GetTicksNS(void)` | Milliseconds / nanoseconds since SDL init. |
| Both are **monotonic** | Not stated in the header — traced it. `SDL_GetTicksNS` → `SDL_GetPerformanceCounter` (`src/timer/SDL_timer.c`) → `CLOCK_MONOTONIC_RAW` (the Unix source comments that it picked that clock because it "is not subject to adjustment by NTP"), else `CLOCK_MONOTONIC`; `mach_absolute_time` on Apple; `QueryPerformanceCounter` on Windows. |
| `SDL_Delay` / `SDL_DelayNS` | Documented to wait **at least** the requested time, "but possibly longer due to OS scheduling". Measured: `SDL_Delay(10)` averaged ≈11.8 ms. |
| `SDL_DelayPrecise(Uint64 ns)` | Gets as close as it can, "busy waiting if necessary" — tighter, at the cost of CPU and battery. |
| Constants | `SDL_NS_PER_SECOND` (1000000000LL), `SDL_NS_PER_MS`, `SDL_MS_TO_NS(x)`, `SDL_NS_TO_MS(x)` etc. in `SDL_timer.h`. |
| `bool SDL_SetRenderVSync(SDL_Renderer*, int)` | `SDL_RENDERER_VSYNC_DISABLED` = 0, `SDL_RENDERER_VSYNC_ADAPTIVE` = −1, or an integer interval. Can fail per backend — check the return. |
| `SDL_RenderDebugText` / `SDL_RenderDebugTextFormat` | Built-in 8×8 bitmap font (`SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE`), ASCII only, no wrapping, drawn in the current draw colour. Scale it with `SDL_SetRenderScale` — which multiplies the coordinates too. Ideal for a debug HUD before real text exists. |

**Absolute time must be `Uint64` nanoseconds, never `float` seconds.** A `float` loses precision
proportionally to its magnitude: ulp(3600.0f) = 0.244 ms, ulp(86400.0f) = **7.8 ms**. Compiled and
ran it — `86400.0f + (1.0f/500)` evaluates to exactly `86400.0f`, so after a day of uptime at
500 fps *time stops advancing entirely*. The same sum in `Uint64` ns advances by exactly 2 000 000.
`Uint64` ns does not overflow for ~584 years. Only the per-frame delta becomes `float`, and the
ns→s division happens in `double` before narrowing (a `float` cannot hold 1e9 to the nearest
integer, so converting first corrupts the numerator).

**Milliseconds are too coarse to measure a frame.** At 300 fps the true frame is 3.333 ms and an
integer-ms clock reports 3 or 4 — a ±20% error *generated by the measurement*, which reads as
judder on a machine that is performing fine. Above 1000 fps truncation reaches zero and nothing
moves at all.

**dt-scaling is exact for constant velocity and first-order for everything else.** `Σ(v·dtᵢ) = v·T`
only because `v` factors out of the sum; the moment velocity changes during the interval it does
not. Explicit Euler on free fall gives `p = g·T·(T−h)/2`, i.e. error = `½·g·T·h` — **proportional
to the step**, so the frame rate is an input to the physics. Verified against a hand-run table at
h = 1, ½, ¼, ⅛, 1/60 (0, 2.5, 3.75, 4.375, 4.9167 m for g=10, T=1). Semi-implicit Euler (velocity
first) overshoots by the same magnitude: `g·T·(T+h)/2`. This is *the* reason for the fixed timestep,
and it is worth deriving rather than asserting.

### The fixed-timestep loop (settled in Lesson 1.4, not changing again)

```
drain -> clk.tick() -> in.update() -> stepper.begin_frame(clk.dt())
      -> while (stepper.next_step()) { previous = current; simulate(current, h); }
      -> render(lerp(previous, current, stepper.alpha()))
```

**The invariant is the whole design.** After the step loop, `0 <= accumulator < h` — guaranteed by
the loop condition. Everything else depends on it: `alpha = accumulator / h` is in `[0,1)`, so the
lerp can never extrapolate. Keep the `accumulator -= h` inside the type that owns the accumulator;
a hand-written loop that skips it hangs *inside one frame*, with no crash and no output.

**The accumulator may be a `float`, and `clock`'s absolute time may not.** No contradiction: 1.3's
rule is about quantities that grow without bound. The accumulator is drained below `h` every frame
by construction.

**Interpolation renders at exactly `T - h`.** Proof: `T = S + accumulator`; the lerp draws
`(S - h) + alpha·h = S - h + accumulator`; substitute. The accumulator cancels, so a lag that
*swings between 0 and h* becomes one that is *always h*. On average interpolation is further
behind — and it looks dramatically better, because the eye tracks changes in velocity and ignores
constant delay. **Smoothness is consistency, not immediacy.**

**Verified numerically** (60 Hz sim, 100 fps render): step pattern has period 5 — `0,1,0,1,1` —
i.e. 7 steps over 12 frames. Raw staleness sawtooths `10.0, 3.33, 13.33, 6.67, 0.0` ms; interpolated
is a flat 16.67 ms. A render rate that *divides evenly* into the sim rate has zero variation and no
judder at all — which is exactly why this bug survives testing at 60 fps.

**Spiral of death:** diverges when `cost per step / h > 1` (e.g. a 20 ms step at 60 Hz → 1.2, losing
3.33 ms per step, compounding). Two guards, both needed: `clock`'s 0.25 s `dt` clamp bounds one
frame to 15 steps at 60 Hz, and `fixed_step`'s per-frame cap handles the machine that is simply too
slow every frame. On hitting the cap, **drain the remaining whole steps** rather than just
returning — otherwise the accumulator stays above `h`, `alpha` exceeds 1, and the renderer silently
starts extrapolating exactly when the machine is already struggling.

**A fixed timestep gives same-binary, same-machine determinism only.** Cross-machine results still
diverge through FMA contraction, x87 80-bit intermediates, `libm` differences, and vectorisation
reordering sums. Necessary for lockstep, nowhere near sufficient — do not promise it.

**Never interpolate across a teleport.** A lerp assumes the two states are a short continuous
motion apart. A respawn, portal, screen-wrap or camera cut violates that and the lerp faithfully
draws positions that never existed. Snap `previous = current` at the discontinuity. This is why the
1.4 demo's box *bounces* rather than wrapping.

**`previous = current` goes INSIDE the step loop.** It must end up holding the second-newest state,
and a frame may run several steps. Hoisting it out works perfectly whenever the frame rate exceeds
the sim rate — i.e. on the developer's machine — and rubber-bands on everyone else's.

### Pixels and textures (verified at `release-3.4.12` for Lesson 1.5)

| Fact | Detail |
|---|---|
| `SDL_CreateTexture(renderer, format, access, w, h)` | `SDL_TEXTUREACCESS_STREAMING` for a buffer rewritten every frame. |
| `SDL_LockTexture(tex, NULL, &pixels, &pitch)` | **Write-only.** The header: "the pixels made available for editing don't necessarily contain the old texture data… if you need to keep a copy of the texture data you should do that at the application level." So keep the master copy app-side and treat the lock as a one-way push. |
| The returned **pitch** | Bytes from one row's start to the next; may exceed `width * 4` because drivers pad rows for alignment. **Copy row by row** — a single whole-buffer `memcpy` shears the image on exactly the machines where the pitches differ. |
| `SDL_UpdateTexture` | Documented as "fairly slow… intended for use with static textures"; for streaming textures the locking functions are preferred. |
| `SDL_RenderTexture(r, tex, NULL, NULL)` | `NULL` dst = "the entire rendering target", so a small framebuffer scales to the window. Window resize then needs **no code at all**. |
| `SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST)` | Crisp upscaling. `SDL_SCALEMODE_PIXELART` also exists (3.4+) for non-integer scale factors. |

**Pixel format naming is the trap.** From `SDL_pixels.h`:

- **`ARGB8888`-style** (bit count per component) = packed into a **native-endianness integer**, most
  significant component first. `ARGB8888` is `0xAARRGGBB` as a `Uint32`.
- **`RGBA32`-style** (single total bit count) = **byte order in memory**, platform-independent.

On little-endian these are *reversed*, and the header proves it:
`SDL_PIXELFORMAT_RGBA32 = SDL_PIXELFORMAT_ABGR8888`, and
`SDL_PIXELFORMAT_BGRA32 = SDL_PIXELFORMAT_ARGB8888`.

**Our escape:** store `Uint32` and build pixels with shifts only, never by writing bytes. That keeps
us purely in the packed-integer view, so `ARGB8888` matches `pack_argb` on every platform including
big-endian. Endianness becomes a real problem only when something reads bytes — an image loader —
which is Module 6's `stb_image` work. Symptom to recognise: **red and blue swapped while green looks
fine** is always a channel-order mismatch, never gamma.

### Framebuffer facts, measured on this codebase

`index = y * width + x`, row-major. Moving right is `+1`; moving down is `+width`.

**An x past `width` is not an error.** It lands on the *next row*: in an 8-wide buffer, `(8,2)`
computes index 24, which is `(0,3)`. The visible symptom is candy-striping — each row leaking its
overshoot onto the left of the next. A stray `y`, by contrast, leaves the buffer entirely and is
undefined behaviour; the crash is the *lucky* outcome, because the stripe can survive review.

Measured on an Apple M4 Pro (L1d 64 KB, L2 4 MB), median of repeated runs, 320×180 = 57,600 pixels:

| Comparison | Result |
|---|---|
| `put_pixel` vs row pointer | **5.1×** at `-O0`, **14.8×** at `-O2` |
| Rows-outer vs columns-outer, 320×180 (225 KB) | **10.9×** |
| …1280×720 (3.5 MB) | 32.7× |
| …3840×2160 (31.6 MB) | **48.8×** |

A 64-byte cache line holds 16 `Uint32` pixels, so a row walk uses all 16 and a column walk uses one
before moving 1,280+ bytes away. The penalty grows as the buffer outgrows the caches. **Rows outer,
columns inner — always.**

**The benchmark that lied, and the lesson in it.** The first version of that second experiment
measured **1.00×** — no difference at all between row and column order. The cause was not that
locality is a myth: both loops were written through `put_pixel`, whose per-call bounds check and
index multiply swamped the memory-access difference entirely. **The instrument was louder than the
signal.** Rewriting both paths to use row pointers, so the *only* remaining difference was access
order, revealed the 10.9× gap. When a benchmark says a well-founded effect is absent, suspect the
measurement before believing the result — and always measure at `-O2`, since the ratio was 3× larger
there than in a Debug build.

### Colour: sRGB encoding (Lesson 1.6) — all numbers computed twice

Stored channel values are **sRGB-encoded**, not quantities of light. Every figure below was derived
in Python and then reproduced by the C++ implementation, so the lesson and the engine agree.

| Fact | Value |
|---|---|
| What the stored value **128** emits | **21.6%** of white's light |
| What stores as **half the light** | **188** (which decodes to 0.5029) |
| Red ⊕ green at `t = 0.5`, naive | `(128, 128, 0)` — a dark olive |
| …the same mix in linear light | `(188, 188, 0)` — a bright yellow |
| Fade table (light → correct store, naive store, what naive emits) | 75% → 225 / 191 / 52.1% · 50% → 188 / 128 / 21.6% · **25% → 137 / 64 / 5.1%** · 10% → 89 / 26 / 1.0% |
| Code budget | Evenly spaced light puts **26** of 256 codes in the darkest 10%; sRGB puts **90**. Evenly spaced wastes **128** on the brightest half; sRGB spends **68**. |

**Use the exact piecewise transform, not `pow(x, 2.2)`.** Threshold `0.04045`, slope `12.92`, offset
`0.055`, exponent `2.4`. The linear toe near black exists because the pure power curve has infinite
slope at the origin. "Close" is how a pipeline accumulates an error nobody can later locate.

**The encode/decode round trip is lossless for all 256 values** — verified, zero failures. Storing a
colour, decoding it and re-encoding it unchanged costs nothing, which is what makes per-operation
conversion tolerable as a stopgap.

**Tabulate decode, not encode.** Decode has exactly 256 possible inputs, so the whole function fits
in a kilobyte and replaces a `pow` with a load. Encode's input is a continuous float and cannot be
tabulated. The asymmetry is structural, not a matter of which deserves optimising.

**ALPHA IS COVERAGE, NOT LIGHT.** Never put it through the transfer function. `mix_linear` converts
three channels and leaves the fourth alone. Getting this wrong makes 50% alpha behave like ~20%, and
because nothing composites yet, the bug can wait months before surfacing as "transparency looks
wrong" in a system nobody would connect to a colour function.

**Safe on stored values:** copying, comparing, picking a colour by eye. **Wrong:** mixing,
cross-fading, alpha blending, averaging, downscaling/mipmapping, adding light from two sources,
anti-aliased edges. The rule is exact — anything that *combines* two colours arithmetically is
wrong; anything that merely *moves* them is fine.

**We are not linear yet, and the lesson says so.** Converting per operation is slow and lossy. A
real pipeline decodes once at the inputs and encodes once at the output, which requires a
float/half framebuffer (8-bit *linear* bands visibly — that is what the code-budget row above
means), headroom above 1.0 for values brighter than white, and a tonemap step to bring it back.
Module 6.

**The diagnostic fingerprints,** worth memorising: red and blue swapped with green fine = channel
order, never gamma. Muddy cross-fades, fades that fall off a cliff, darkening mipmaps and fringed
anti-aliased text = gamma. Washed-out milky output = the conversion applied twice or in the wrong
direction.

### Vectors (Lesson 1.7) — derived, then verified

**A vector is an arrow: direction and length, no position.** Components are its **shadows on the
axes**, and that single fact is what makes the dot product's component formula derivable rather than
memorised:

```
a . b = (ax*x_hat + ay*y_hat) . b        write a as its components
      = ax*(x_hat . b) + ay*(y_hat . b)  shadows add (projection is linear)
      = ax*bx + ay*by                    because x_hat . b = |b|cos(alpha) = bx
```

That route needs only "shadows add" and "components are projections" — **no law of cosines**, which
matters given the course assumes no trigonometry beyond the basics.

Verified numerically (Python first, then the C++ harness), with 3-4-5 triangles throughout:

| Claim | Value |
|---|---|
| `(3,4) · (4,3)` by components | `12 + 12 = 24` |
| …and geometrically | `|a|=|b|=5`, `cos θ = 0.96`, `θ = 16.2602°`, shadow `= 4.8`, `4.8 × 5 = 24` |
| Sign table | `(6,8) → 50` (front) · `(−4,3) → 0` (perpendicular) · `(−5,1) → −11` · `(−3,−4) → −25` (opposite) |
| The diagonal bug | `|(1,−1)| = 1.41421`; raw step **127.27922** vs normalised **90.00000** — the 41.4% of Exercise 1.2.3 |

**`dot(v, v) == length_squared(v)`**, since a vector is perfectly aligned with itself — a free
consistency check on the formula, and a genuine reuse in the implementation.

**Prefer `length_squared` for every comparison.** `sqrt` is monotonic, so `|a| < |b|` exactly when
`|a|² < |b|²`. Square the constant once outside the loop; do not root the variable inside it.

**Normalise the direction, THEN scale by speed.** `normalised(input) * speed * dt`, never
`normalised(input * speed * dt)` — the latter throws the speed away and yields a step of length 1.

**`normalised({0,0})` must not be `0/0`.** That is `NaN`, which spreads silently (1.3 §3.5). There is
no correct answer for "which way does a zero-length arrow point", so ours returns `(0,0)` — no input
means no movement — with `normalised_or(v, fallback)` where a direction must exist. The harness
confirms the naive version really does produce `NaN`, so the guard is demonstrably earning its keep.

**`dot(v, perpendicular(v))` is EXACTLY 0**, not approximately: `perpendicular({x,y}) = {-y,x}`, so
the products are equal and opposite and cancel before rounding can occur. Exact zeros are rare in
float work and worth recognising.

**`sizeof(vec2) == 8`** — measured — which is why everything passes by value. A `const vec2&` would
hand over an address to dereference.

**Header-only, deliberately.** Small, hot, stable code the compiler must be able to inline; a
definition in another TU generally cannot be. Not a general licence. A header-only addition also
needs **no CMake change at all**.

**Does not generalise to 3-D:** `perpendicular()` (in 3-D there is a whole plane of them). The
operation that exists only in 3-D is the **cross product** — Module 2.

### Line rasterisation (Lesson 2.1) — derived, verified, and benchmarked

**`y = mx + b` is the wrong tool, in two independent ways.** It lights one pixel per *column*, so
any line steeper than 45° comes apart into disconnected dots (measured: a line spanning 101 rows
and 21 columns lights **21** pixels, every one of them disconnected; a correct routine lights
101). And it cannot represent a vertical line at all — there is no `m` for which `y = mx + b`
describes `x = 5`. Any formulation needing an `if` to survive one of its own inputs is suspect.

**The fix is one line: `steps = max(|dx|, |dy|)`.** The major axis then advances exactly ±1 per
step and the minor by at most 1, so a gap becomes *impossible* rather than unlikely. It also
disposes of the vertical case — we never form `dy/dx`, only `dy/max(...)`, whose denominator is
zero only for a zero-length line. The special case stops existing rather than being handled.

**Bresenham, derived rather than pasted.** Error `e = y_true − y_plotted`; each step `e += m`; when
`e ≥ ½`, step the minor axis and `e −= 1`. Then the key move: `e`'s only uses are a comparison
against ½ and additions, and **comparisons survive multiplication by a positive constant**, so
scale by `2·dx` to clear every denominator:

```
E = 2*dx*e   ->   E += 2*dy ;   test E >= dx ;   E -= 2*dx      (all integers, E starts 0)
```

VERIFIED `(2,2)->(12,8)`: pixels `(2,2)(3,3)(4,3)(5,4)(6,4)(7,5)(8,6)(9,6)(10,7)(11,7)(12,8)`, and
every one is the nearest row to the true line `y = 2 + 0.6(x−2)` — computed without ever evaluating
it. The general "scale an order-preserving quantity until the fractions vanish" technique reappears
as fixed-point sub-pixel precision and as depth encoding.

**The compact all-octant form terminates, provably.** With `dx = |Δx| ≥ 0` and `dy = −|Δy| ≤ 0`,
both tests failing would need `dx < 2·err < dy ≤ 0 ≤ dx`, i.e. `dx < dx`. So at least one test
fires every iteration. VERIFIED pixel-identical to a reflect-in/reflect-out first-octant reference
over 1600 lines in all eight octants.

**Ties, and a theorem worth keeping.** With major extent `M`, minor `n`, and `p = M/gcd(M,n)`:
an exact tie (the line passing precisely through a midpoint) occurs **iff `p` is even**. Proof:
a tie needs `2kq = (2j+1)p` with `q = n/gcd` coprime to `p`; if `p` is odd the left side is even
and the right odd — impossible; if `p` is even, `k = p/2` works. **Exactly those lines are
asymmetric under endpoint swap** — VERIFIED exhaustively over 23,103 lines, 7,692 asymmetric,
zero disagreements with the prediction in either direction. Slope 1/2 ties constantly; slope 3/5
and a 45° diagonal never do. This is why filled shapes need a **fill rule** (2.2): adjacent
triangles traverse a shared edge in opposite directions, and a third of all edges would disagree.

**BENCHMARK — the folklore is inverted on modern hardware.** ns/pixel, stepping only (framebuffer
write excluded), M4 Pro, Apple clang 21, `-O2`, 3000 reps × 360 lines:

| | all octants | shallow | steep | 45° |
|---|---|---|---|---|
| Bresenham, compact (2 branches) | 1.320 | 1.247 | 1.251 | 1.248 |
| Bresenham, major-axis (1 branch) | 0.738 | 0.781 | 0.776 | 0.788 |
| DDA (`lround`) | **0.595** | **0.648** | **0.643** | 0.660 |
| DDA (`+0.5`, truncate) | 0.645 | 0.667 | 0.667 | **0.630** |

**DDA is ~2.2× faster than the compact Bresenham and ~1.2× faster than the best Bresenham.** The
floats were never the problem — swapping `lround` for truncation changes almost nothing. The
**branches** are: Bresenham asks a data-dependent question per pixel that the predictor cannot
learn, and restructuring to one branch instead of two nearly halves the cost with identical
arithmetic. Through `put_pixel` the whole-routine ratio is ~1.5×, i.e. the store is nearly free
here because the loop is branch-bound rather than memory-bound.

**We ship Bresenham anyway, and the reasons are not speed:** (1) exactness — integers have no
rounding modes, no FMA contraction, no accumulation, so the pixel set is identical on every
machine, which 1.8 established that floats cannot promise; (2) its error term *is* 2.2's edge
function; (3) lines are not the hot path, and optimising them would be 1.5's benchmark trap again.
Record the measurement, choose deliberately, and let the reader disagree.

### Barycentric coordinates (Lesson 2.3) — verified in a scratch harness

**Definition:** `w_i` = area of the sub-triangle **opposite** `v_i`, over the total. In practice
the edge functions 2.2 already computes, divided by the total area — one reciprocal, three
multiplies. No new computation, just one that stops being discarded.

**THE PAIRING IS THE BUG.** `w0` uses the edge `v1→v2` — the edge that does *not* touch `v0`. It
is very natural to reach for `v0→v1` instead. A rotated pairing produces weights that are all in
`[0,1]`, **still sum to exactly 1**, and describe a different point. VERIFIED: for
`v0(2,2) v1(10,4) v2(4,10)` and `P(5,5)`, the correct weights `(0.4,0.3,0.3)` reconstruct
`(5.0,5.0)`; rotated by one they reconstruct `(5.2,5.8)`.

**So assert RECONSTRUCTION, never the sum.** `w0·v0 + w1·v1 + w2·v2 == P` is the *defining*
property; summing to 1 is a consequence that all three wrong rotations also satisfy. VERIFIED over
3,721 points spread well beyond the triangle: worst reconstruction error **0.000015 px**.

The general habit, which is bigger than this lesson: **when choosing an invariant to assert,
prefer the property that *determines* the answer over one that merely *constrains* it.**

**The sum identity is exact in integers, everywhere.** `e0 + e1 + e2 == area` holds for *every*
point in the plane — inside or outside — because expanding the three edge functions cancels every
term containing `P`. Free assertion, catches a mis-ordered edge at the line that is wrong.

**Geometry worth memorising** (all verified): 1 at its own vertex, 0 on the opposite edge, 1/3 at
the centroid, **negative outside**. "All three weights ≥ 0" *is* 2.2's inside test divided by a
positive constant — checked identical over 5,041 points. Negative weights are a feature: the same
formula extrapolates, which is what texture derivatives and conservative rasterization need.

**A constant weight traces a line PARALLEL to the opposite edge**, evenly spaced — because that
edge is a fixed base, so equal area means equal height. MEASURED: `w0` varies by **exactly 0.0**
along such a line.

**The interpolation is UNIQUE, not merely reasonable.** `f(P) = w0·f0 + w1·f1 + w2·f2` is the only
affine function of position matching three values at three non-collinear points: an affine
`ax + by + c` has three coefficients, three corners impose three independent conditions. Worth
stating that way — students otherwise assume it is one blend among several.

Affine in **screen** space, which is not surface-affine under perspective. That is Lesson 3.2's
`1/w` correction, and the artifact is swimming textures. Without projection, screen-space is
exactly right.

**PRECISION, measured over 32,761 points:** worst `|sum − 1|` = **2.4 × 10⁻⁷**, i.e. one rounding
— not accumulation, because the integer identity is exact and only the final division is inexact.
But the sum is **bitwise `1.0f` only ~85% of the time**. So: **never compare weights for
equality.** `if (w0 == 0.0f)` to find edge pixels misses most of them. Do such tests on the
**integer** edge values, where "on the edge" really is `== 0`.

**Use UNBIASED edge values for interpolation.** The top-left rule's `−1` bias decides coverage and
is not part of the geometry; interpolating with biased values shifts attributes by a fraction of a
pixel and shows up as a seam where two triangles meet. Lesson 2.4 carries both sets through one
loop and this is the detail it has to get right.

**Degenerate input returns all zeros** — the one case where the weights do not sum to 1 — rather
than dividing by zero. A `NaN` here would spread through every later calculation while comparing
false to everything (1.3 §3.5). Collinear triangles are not exotic: welded vertices and
zero-scaled geometry produce them routinely.

**Drawing a contour without testing equality:** a level set of anything is "this pixel and its
neighbour fall in different bands", i.e. `floor(a/step) != floor(b/step)`. No tolerance to tune,
works for any step, and sidesteps the equality problem above entirely.

### Triangles and edge functions (Lesson 2.2) — verified in a scratch harness

**The edge function** `E(A,B,P) = (Bx-Ax)(Py-Ay) - (By-Ay)(Px-Ax)` is
`dot(P - A, perpendicular(B - A))` — Lesson 1.7's parts — and equivalently the z component of the
2-D cross product. **Sign** = which side (0 = exactly on the line). **Magnitude** = twice the area
of triangle ABP. Inside a triangle = all three agree in sign = three half-planes intersected.

**The three edge functions SUM to the total area** — for points inside *and* outside. VERIFIED:
A(2,2) B(10,4) C(4,10), total 60; at P(5,5) they are 18/24/18 (sum 60, inside); at Q(9,9) they are
42/−24/42 (sum 60, outside). The P terms cancel algebraically, so the sum is independent of P.
**Leave this as a debug assertion** — if the three do not sum to the area, one edge has its
vertices in the wrong order, and you find out at the bug rather than three lessons later when a
texture looks skewed. These are 2.3's barycentric weights before normalisation.

**Sign convention, MEASURED not assumed.** In a y-down framebuffer, a triangle that appears
**counter-clockwise on screen has NEGATIVE signed area**: `(5,0),(0,10),(10,10)` → `−100`, reversed
→ `+100`. That does not contradict the course's `CCW = front`, which is an **NDC** statement —
the viewport transform's y-flip reverses orientation on the way to pixels. Two true statements
about one triangle in two spaces; knowing which space you are in is the whole skill.

**So `fill_triangle` does not cull.** It measures the area once and swaps two vertices if negative,
which flips all three edge functions at once and lets the per-pixel test be a plain `>= 0`. A fill
that silently dropped backwards triangles would be indistinguishable from a bug. Culling is 3.4's,
made in NDC.

**Affine in the pixel**, so stepping is constant and multiply-free: `dE/dx = Ay − By`,
`dE/dy = Bx − Ax`. Evaluate once at a bounding-box corner, then three adds per pixel. Same
technique as 2.1's error term, for the same reason: the tracked quantity is affine in the stepped
one.

**MEASURED: bounding box + incremental stepping is 75× faster** than direct evaluation over the
whole buffer (5763 ms → 77 ms; 200 triangles, 512×512, 400 reps, M4 Pro, clang 21, −O2) — and
**pixel-identical** to it across 144 triangles including many straddling the buffer edge. Most of
the win is the bounding box; the stepping tightens what is left. Clipping the *search box* is
exact and is not clipping the *geometry* — that distinction is three lines here and a whole lesson
in Module 3.

**Clipping the box first is what licenses `fb.row(y)`** in the inner loop: every visited pixel is
in bounds by construction, so `put_pixel`'s per-pixel check and index multiply can go.

**`edge_function` overflows int32 past roughly ±16000 coordinates.** Signed overflow is UB — not a
wrapped number but a licence for the optimiser to assume it cannot happen — so this is documented
in the header rather than left to be found. Module 3's clipping is what keeps us inside it.

**THE TOP-LEFT FILL RULE.** For a triangle oriented to positive area: a **top** edge is
`dy == 0 && dx > 0`; a **left** edge is `dy < 0`. Bias `−1` on all others, folded into the loop's
starting value so it costs **nothing per pixel**.

Why it needs no coordination between triangles: two triangles share an edge *by traversing it in
opposite directions*, so any rule phrased on edge direction necessarily answers oppositely for the
two of them. Proof sketch: for a non-horizontal edge `dy` flips sign, so exactly one direction has
`dy < 0`; for a horizontal edge `dy == 0` both ways and `dx` flips, so exactly one has `dx > 0` —
**provided `dx ≠ 0`, which is guaranteed only because zero-area triangles were rejected up front.**
That degeneracy check is load-bearing for the rule's correctness, not housekeeping.

VERIFIED: a quad and a 12-triangle fan both give **0 px drawn twice, 0 interior gaps**; with the
rule off the same quad double-draws its entire seam.

**Coverage becomes HALF-OPEN**, and that is the intended trade. A lone 37×37 quad loses exactly
**73 px** = bottom row (37) + right column (37) − shared corner (1). Nothing else is dropped and
nothing is ever added. Same reasoning as `[start, end)` ranges: half-open tiles, closed cannot.
A lone triangle therefore renders one pixel short of its wireframe on the bottom and right — not a
bug.

**Double-draws only hit pixels whose centres are EXACTLY on the seam.** So an axis-aligned or 45°
shared edge fails *totally* (40 px of a 40 px seam) while a rotated one loses 2–3 stray pixels that
read as noise. Since quads-split-into-triangles, terrain grids and UI rectangles are overwhelmingly
axis-aligned, **the catastrophic case is also the common case** — the same shape as 1.8's
tunnelling, which was unreachable at 60 Hz and certain at 10.

**To see an idempotent defect, instrument the operation rather than inspecting the result.** A
pixel drawn twice looks exactly like one drawn once, so no amount of looking finds it. Draw into a
per-pixel *counter* and colour by the count, and the seam lights up instantly. The demo does this.

### Collision and reflection (Lesson 1.8) — all numbers verified in a scratch harness

**`reflect(v, n̂) = v − 2 (v·n̂) n̂`**, derived by splitting `v` into its shadow on the normal plus
whatever is left, keeping the leftover and reversing the shadow — hence subtracting it *twice*.

- **`n̂` MUST be unit length.** `project_onto` divides by `|b|²`; `reflect` has no such division
  because with a unit normal it is a division by one. A normal of length *k* scales the correction
  by *k²*, so the ball gains or loses energy on every bounce with nothing to warn you.
- The invariant that catches it instantly: **`|reflect(v, n̂)| == |v|`**.
- Verified: `reflect((3,4), (0,−1)) = (3,−4)` — matches the obvious sign flip, which is the point
  of the check. And `reflect((0,5), (0.70711,−0.70711)) = (5,0)`: a vertical drop onto a 45° wall
  leaves horizontally, which no single-component negation can produce.
- **Framebuffer normals point *into* the court**, and +y is down: the ceiling at `y=0` has normal
  `(0,+1)`, the floor has `(0,−1)`.

**A bounce needs BOTH the velocity turned and the position mirrored back inside.** Velocity only →
the object spends a step outside the wall. Position only → it sticks to the wall and re-collides
every step. This is the single most common bounce bug.

**Tunnelling — the headline result, and it generalises far past Pong.** A discrete "do the boxes
overlap *now*?" test is a question about an instant; collision is a fact about an interval. The
overlap window along an axis is as wide as the two boxes put together (`size_a + size_b` — the
Minkowski-sum idea), so the test is guaranteed to catch a mover only while:

```
|v_axis| * h  <  size_a + size_b
```

With our numbers (paddle 4 px + ball 4 px = 8 px window, ball capped at 260 px/s):

| sim rate | h | safe up to | reachable at 260 px/s? |
|---|---|---|---|
| 120 Hz | 0.00833 s | 960 px/s | no |
| 60 Hz | 0.01667 s | **480 px/s** | **NO — the bug is unreachable, and still there** |
| 30 Hz | 0.03333 s | 240 px/s | yes, after ~20 hits (intermittent — the worst kind) |
| 10 Hz | 0.1 s | 80 px/s | yes, on the opening serve at 105 px/s |

**The lesson beyond collision: a passing test suite tells you the bug is not reachable under the
conditions you tested, not that the code is correct.** Whenever you write a discrete test of a
continuous process, compute the bound at which it stops being valid and put it in a comment even
when today's numbers are safely inside it. Same shape recurs as aliasing (M2), texture shimmer
(M3), shadow acne (M6).

**The swept fix.** Within one step the velocity is constant, so the path is a straight line and
`t = (face − lead_from) / (lead_to − lead_from)`. Two details that matter:

- Require the leading edge **began** on the near side *and* **ended** on the far side. "Ended past
  it" alone also fires for something that got behind the paddle on an earlier step. The pair also
  makes the denominator non-zero **by construction** — crossing implies motion — which is a safer
  guarantee than a guard someone can edit away.
- Interpolate the *other* axis **at `t`**, not at the endpoint. Using the endpoint's `y` is the
  same mistake rotated 90°.
- **Spend the remaining `(1 − t)` of the step** in the new direction, or the object loses a sliver
  of motion at every bounce — invisible once, a measurable drag over a long rally.
- Verified end to end: ball 4 px at `x=16` going left at 105 px/s, `h=0.1`, paddle face at `x=14`
  → `t = 0.190476`, speed 105→112, velocity `(109.5525, −23.2861)`, final `x = 22.8685`. The naive
  test on the identical step reports **no collision** and the ball scores.

**Keep the PRNG seed inside the simulation state.** `SDL_srand`/`SDL_rand`/`SDL_randf` exist and
work, but keep state in a hidden global, which means the sim is no longer a function of its inputs:
no replays, and two copies cannot be compared. xorshift32 is three shifts and three xors —
**seed 0 is a fixed point**, every operation maps 0→0, so guard it. For a float in [0,1) take the
top 24 bits (`>> 8`) and scale by 2⁻²⁴: exact, and it avoids letting xorshift's worst bits decide
the rounding.

**Determinism means: same binary + machine + seed + inputs + step size.** Verified bit-identical
over 20 000 steps with a shared seed. It is **not** step-size-independent — same seed at 60 Hz vs
120 Hz diverges within two simulated seconds, because `h` is an input too. (Nor cross-platform:
FMA contraction, x87 excess precision, libm, vectorisation.)

**A game-design fact worth keeping.** A physically honest paddle — `reflect` with normal `(±1,0)`
— conserves `v.y` for the whole match, because walls negate it and paddles then ignore it. Neither
player can influence it, so there is no way to place a shot and no game. Pong's angle-from-hit-
position paddle is a deliberate physical lie, and the useful generalisation is: *a game needs the
player to be able to change state in ways the opponent must respond to.*

**Loop model:** we use classic `int main` + our own `while` loop, NOT SDL3's callback model
(`SDL_MAIN_USE_CALLBACKS` with `SDL_AppInit`/`SDL_AppIterate`/`SDL_AppEvent`/`SDL_AppQuit`), because
the engine owns its loop (0.1 thesis). The callback model exists and is fine for simple apps; it is
just the wrong fit here.

**Version pin:** SDL `main` is 3.5.0 but **unreleased**. Latest *release* tag is **`release-3.4.12`**
(commit f87239e71e42); the 3.2.x line tops out at release-3.2.30. Pin to `release-3.4.12`. CMake
target to link is **`SDL3::SDL3`** (alias → shared if built, else static); `SDL_TEST_LIBRARY OFF`
skips SDL's test lib.

Extend this table whenever a signature surprises you. The fastest check:
`curl -sL https://raw.githubusercontent.com/libsdl-org/SDL/release-3.4.12/include/SDL3/<hdr>.h | grep -n ...`

---

## Authoring conventions worth remembering

- **Never state an API detail you have not verified.** §10 mandates an inline `⚠ VERIFY:` marker
  naming the header or wiki page to check, plus the conceptually correct usage. An honest flag
  beats a confident fabrication.
- **The `curl raw.githubusercontent.com | grep` trick** on `libsdl-org/SDL/main/include/SDL3/*.h`
  is the fastest way to settle an enum or signature question, and the header *is* the authority
  — the wiki lags it. This is how the winding facts above were settled after the wiki came up
  short.
- **Zero placeholders in code listings** (§8). No `// ...`, no "rest of file as before". A
  changed file appears whole. Continuity errors across lessons are correctness bugs.
- **Generate code listings from the real files, never retype them.** Author the lesson with
  `<!--INCLUDE:path-->` inside the `<code>` element and splice the escaped file contents in with a
  throwaway script before publishing. Hand-copying a 250-line listing is how a lesson comes to show
  code that no longer matches `src/` — the exact continuity bug §8 calls a correctness error.
- **The trailing `<script>` block drifted between lessons because nothing propagated it.**
  *(Resolved after 1.2 — kept because the shape of the failure recurs.)* The stamper covered
  `<style>` only, so by 1.2 the script existed in six inconsistent versions: three C++ keyword
  lists, a CMake highlighter in exactly one lesson, a Windows-batch `::` rule in two. The fix was
  `apply-shared.py` with a second `SHARED-SCRIPT` marker region, stamped across all 12 pages.
  **Every one of those defects was a silent mis-render rather than a crash** — which is why they
  survived review, and the general lesson: duplication that nothing propagates always drifts, and
  drift in *presentation* code is invisible precisely when you most need to see it.
- **"Take the union" is the wrong merge rule for a keyword list.** Reconciling the drifted
  highlighters looked like a set union, but two lists disagreed on *classification*, not
  membership: one filed `bool char int long unsigned void` under `CPP_KEYWORDS`, the other under
  `CPP_TYPES`. Since the tokeniser checks `kw` before `ty`, a naive union would have recoloured
  every fundamental type in the course from `.tok-t` to `.tok-k` — a 12-page regression that
  compiles, throws nothing, and looks plausible. Superset means superset of *coverage*; keep the
  more correct classification and fold in only what is genuinely absent.
- **A propagation template must carry every marker the pages carry.** After adding
  `SHARED-SCRIPT` markers to `lesson-template.html`, the template still lacked `SHARED-CSS`
  markers — those had been hand-added to the pages. Because a new lesson is authored by *copying
  the template*, the next lesson would have inherited script propagation and silently missed CSS
  propagation, with `apply-shared.py` reporting only a `skip` line. The general rule: if the
  source of truth does not itself carry the opt-in marks, every artifact derived from it starts
  un-opted-in, and the failure is a quiet omission rather than an error. Test it the cheap way —
  copy the template to a scratch page and run `--check`.
- **Anchor a `::` comment rule to the line start.** The batch-comment rule inherited from 1.2 was
  `/(#[^\n]*|::[^\n]*)/` — unanchored, so it also matches the `::` in a CMake target and eats the
  rest of the line. `cmake --build . --target SDL3::SDL3 --config Release` renders as
  `cmake --build . --target SDL3` followed by a comment. `SDL3::SDL3` is *the* link target in this
  course, so this was a live trap. Use `(^|\n)(\s*)(::[^\n]*)` and renumber the capture groups.
  Worth unit-testing a tokeniser change in `node` before stamping it into every page — the
  invariant is that stripping the emitted tags must reproduce the input exactly.
- **Anything every page needs must live BETWEEN the shared markers, not next to them.** The KaTeX
  loader `<script>` tags sat immediately *below* `<!-- SHARED-SCRIPT:END -->` in the template, so
  `apply-shared.py` never propagated them. Lessons authored by copying the template shipped with
  the KaTeX **CSS** (in `<head>`, above the markers, hand-copied) but no renderer — and the
  symptom, raw TeX where an equation should be, is *identical to the documented CDN-unreachable
  fallback*. It therefore read as working-as-intended for six lessons. It only surfaced at 1.8,
  the first lesson with display math: lessons 1.1–1.7 have zero `.eq` blocks, so there was nothing
  to fail. Fixed by moving the block inside the region and deleting the now-duplicate standalone
  copies from `conventions.html` and `math-toolbox.html` (which had them, being hand-authored) —
  otherwise those two pages load KaTeX twice.
  **The general shape:** a fallback that is indistinguishable from the failure it guards against
  will hide that failure indefinitely. Check for the *positive* signal instead —
  `document.querySelectorAll('.katex').length > 0` — not for the absence of an error.

---

## Verifying a lesson page — what actually catches things

Eyeballing a page misses SVG defects. Serve `docs/` over HTTP and drive a real browser (the
preview pane reports impossible computed styles). Three checks earned their keep on 1.2:

1. **Label spill** — compare every `<text>` bounding box against its `<svg>` box. Caught a
   monospace row label clipped off the left edge of Figure 2 and a caption running 10 px past the
   right edge of Figure 4.
2. **Label collision** — pairwise overlap test over each SVG's `<text>` nodes.
3. **Look at the rendered figure.** Neither check above catches a *line* crossing the wrong row:
   1.2's Figure 1 had its comparison ramp drawn straight through the event-tick timeline, and only
   a screenshot revealed it. When a diagram has stacked rows, compute the path coordinates in
   Python and check the extremes land inside their band.

**Text-vs-*shape* collision is the one the bounding-box checks keep missing.** In 1.3, two labels
sat directly on top of the curves they annotated (the Euler figure) and a legend ran through the
frame markers (the decoupling figure) — all invisible to a text-vs-text test. Extending the check
to `<circle>` elements caught the third case; the first two needed eyes. Rules of thumb that would
have prevented all three: put a label in the *empty* quadrant of a plot rather than near the line
it describes, and give legends their own row below the artwork instead of tucking them into
whitespace that only looks empty.

**An arrow's direction is an assertion.** 1.3's clock figure drew "NTP corrects backwards" with a
rightward arrow because the path was written left-to-right out of habit. It renders perfectly and
says the opposite of the caption. Read every `marker-end` back as a sentence.

Also worth confirming per page: zero inline `fill=` on `<text>` (the stamper lints this), both
themes, code `white-space: pre` with horizontal scroll, and `scrollWidth == clientWidth` at 375 px
wide.

**Verify an interactive widget against the prose it illustrates.** 1.3's Euler slider was driven
through all 60 positions and its readout checked against the closed form `½·g·T·h`, plus the five
rows of the lesson's static table. A widget that quietly disagrees with the table beside it is
worse than no widget, and it is a one-minute check.

**Drive a widget to its degenerate inputs, not just its interesting ones.** 1.4's interpolation
slider showed `raw lag: 0.0–0.0 ms` at 15/30/60 fps, which looks broken but is *correct*: a render
rate dividing evenly into the sim rate has no judder. Left as-is it would read as a bug; the fix was
to have the widget say so. Also caught a `-0.0` from float error in a quantity the invariant says is
non-negative — clamp display values to the range the maths guarantees.

**A diagram that needs a 7-pixel difference to make its point needs a different diagram.** 1.4's
Figure 3 originally showed interpolation as a line "parallel to the truth, one step below" — at
honest scale that offset was ~7 px and read as noise. Replotting the *lag itself* over time (a red
sawtooth against a flat amber line) made the identical claim unmissable. When an effect is small in
the natural units, plot the effect rather than the thing it affects.

**Step functions should be drawn as step functions.** The same figure first joined per-frame samples
with straight segments, which shows the values but hides the behaviour. A proper hold-then-jump path
makes "two frames at the same position, then a double-sized jump" literally visible.

**Watch for escaping artifacts when editing HTML from a script.** A Python-generated SVG label
shipped as `can''t` — a doubled apostrophe from quoting. Grep the rendered text for `''` and similar
after any scripted edit; the browser will render it happily.

**A widget whose content changes size will outgrow its viewBox.** 1.5's index widget rebuilt its
grid from a slider; at the narrowest width the grid grew to twelve rows and pushed the memory strip
below the fixed `viewBox` height, where it was silently clipped. Nothing errored and the geometry
checks passed — only the screenshot showed it. Two fixes worth reusing: size cells from *both*
constraints (`min(max, availableWidth/cols, availableHeight/rows)`), and place anything that must
always be visible at a **fixed** coordinate rather than relative to variable-height content.

**Constrain a widget's inputs so no configuration has loose ends.** The same widget allowed any
width from 4 to 12 over a 48-box strip, so at width 10 the grid covered 40 boxes and 8 sat
unexplained at the end of the line. Restricting the slider to exact divisors removed the question
entirely. If a control can reach a state the caption does not explain, either explain it or make it
unreachable.

**Check a widget's initial state against the prose.** 1.5's widget must open showing
`(3, 2) → 2 × 8 + 3 = 19` because that is the worked example in the surrounding text. Verified
explicitly after a fresh load, since interacting with it during testing leaves it elsewhere.

**A perceptual demo needs device-pixel accuracy, and needs saying so.** 1.6's gamma test relies on
alternating one-pixel black and white lines optically averaging to 50% light in the viewer's eye.
Built with `repeating-linear-gradient` and **hard stops** — the two colour stops must sit at the
same position (`#000 1px, #fff 1px`), or the browser interpolates between them and the stripes blur
into a solid grey, silently destroying the test. Verify the stops programmatically; a screenshot at
device scale confirms the rest. The page also has to tell the reader it needs 100% zoom, because any
scaling resamples the pattern.

**Colours that are the subject must not follow the theme.** 1.6's swatches are literal
`fill="rgb(188,188,0)"` values, deliberately outside the `--dia-*` token system, because the whole
figure is a claim about those exact numbers. Checked that they stay literal in both themes — the
usual "never hard-code colours in diagrams" rule has this one principled exception, and it is worth
flagging in the source so nobody "fixes" it later.

**A figure whose two quantities nearly coincide needs a dimension line, not an overlay.** 1.7's
shadow figure drew a's projection (4.8) directly on top of b (5.0) — 96% overlap, so it read as one
two-tone line. Redrawing the shadow as a separate bar offset perpendicular to b, with short
connector ticks at each end, made it legible without changing the numbers. Standard technical-drawing
practice, and worth reaching for whenever a measurement lies along the thing being measured.

**Check a diagram's scale against the space it occupies.** The same lesson's normalisation figure
was drawn at 30 px per unit inside a 340×190 region, so the whole construction huddled in one corner
while the annotation panel took the rest. Raising it to 80 px per unit fixed it. Symptom to watch
for: arrows overlapping each other and labels with nowhere to go.

**The SVG-lint applies to widget JavaScript too.** 1.7's drag widget generated labels with
`fill="var(--dia-hi)"` and tripped `apply-shared.py`'s inline-fill lint. The fix is not to suppress
it: use `style="fill:…"` instead, which is an inline *style* rather than a presentation attribute and
therefore beats any stylesheet rule — the exact problem the lint exists to prevent. Lint clean and
more correct.

**Count figure references excluding the caption.** A `Figure N` search hits the `fignum` caption too,
so a count of 1 means the figure is captioned and **never referenced from the prose** — which the
style guide forbids. Four of 1.7's five figures were in that state on the first pass. Search for
mentions *outside* `.fignum`, or expect a count of at least 2.

**Watch the working directory when a session mixes `cd` and scripts.** A `cd docs && python3 -m
http.server` left the shell in `docs/`, and the next repo-root-relative script failed with a
confusing `FileNotFoundError`. Prefer absolute paths, or `cd` back explicitly, in any command that
follows a directory change. **This recurred in 1.8** — a link-checker run from `docs/` looked for
`docs/docs/` and cheerfully reported "0 broken links" from an empty glob. A check that scans
nothing passes. Always print the count of things *examined*, not just the count of failures.

**Use `getBoundingClientRect()`, not `getBBox()`, for spill and collision checks.** `getBBox()`
returns coordinates in the element's *local* space, so every `<text>` inside a
`<g transform="translate(...)">` is compared against the wrong origin. In 1.8 that produced three
confident false positives ("11 px above the top edge") for labels that were correctly placed.
`getBoundingClientRect()` accounts for the full transform chain and needs no viewBox arithmetic:
compare each text's rect against the `<svg>`'s own rect.

**The checks now live in `docs/_template/check-page.js`** — run it in Chromium and require
`pass: true`. Added in 2.1, after the text-vs-shape defect shipped for a second time (1.3's Euler
figure, then 2.1's Figure 4, where the threshold label sat on the rising sawtooth). It samples
points along every stroke and tests them against text boxes, which is the only one of the three
SVG checks that can see that class of bug.

Two false positives are designed out of it, and both are worth knowing about because they will
recur in anything similar:

- **Skip `<defs>` / `<marker>`.** An arrowhead's `<path>` is never painted at its own coordinates
  but still answers `getTotalLength()` and `getScreenCTM()`, so it reports collisions wherever the
  marker template happens to sit.
- **Skip `.grid` with `closest()`, not `getAttribute()`.** Graph paper is *meant* to sit under
  labels. The class is on the wrapping `<g>`, so reading it off the `<path>` returns null and
  every single gridline reads as an unclassed stroke crossing every label near it.

Running it over the back catalogue immediately found ten pre-existing text-on-shape defects
(1.5 ×2, 1.6 ×3, 1.7 ×4, conventions ×1) — cosmetic, but real: 1.7's Figure 5 has a dashed
drop-line running straight through its `u + v` label. **A check written after the fact will find
history.** Budget for that, and do not let it silently expand the current lesson's scope.

**All ten are now fixed** (plus conventions' `+y`/`up` text-vs-text overlap), by moving label
coordinates only — no diagram geometry was touched. Four things learned doing it:

- **Measure in viewBox units before choosing coordinates.** Invert the SVG's `getScreenCTM()` and
  print every `<text>`'s client rect back in viewBox space; the numbers then paste straight into
  the `x=`/`y=` attributes. Guessing from the source is hopeless because **`y` is the baseline, not
  the top** — conventions' `+y` at `y=58` in a bold face occupies `40.8…61.6`, so the 16-unit gap
  the author left above it was smaller than the glyph box, and the collision was invisible in the
  markup.
- **A label wider than the cell it sits in cannot be fixed by nudging.** 1.5's Figure 2 has 37-unit
  grid cells and a 50-unit shortest label, so *every* in-grid placement straddles a border. Check
  that ratio first: if the label does not fit, the only fix is to leave the artwork entirely. The
  replacement — a row of labels below the grid, each column-aligned with the run it names and
  already colour-coded to it — needs no leader at all. **Alignment and colour are cheaper pointers
  than a leader line**, and a leader dragged across the artwork is worse than the original defect.
- **The checker's 2-unit threshold hides near misses.** The obvious fix for 1.8's Figure 6 put a
  104-unit label's right edge 0.1 units from the neighbouring note — `pass: true`, and it would
  have shipped looking broken. Do not treat clearing the checker as clearing the figure; compute
  the gap to the *nearest* neighbour, not just the overlapping one.
- **The fills confirm what the checker cannot see.** 1.7's Figure 3 labels straddled the unit
  square's *fill* as well as its stroke — the stroke is what got flagged, but the dark-mode
  screenshot is what proved the move fixed both. Screenshot in **both** themes: `--dia-fill` is
  near-invisible in light mode and obvious in dark.

The same sweep also surfaced defects on pages outside that original count, left unfixed and
reported rather than folded in: **0.3** Figure 1 (`C++ standard library`, `+ C runtime` on a `hi`
line), **0.6** Figure 2 (`observe`), **1.2** Figure 1 (`SDL_EVENT_KEY_DOWN`), and — a different
class entirely — **0.1 and 0.2 carry four `script[src*="katex"]` tags instead of two**, i.e. a
duplicated KaTeX block that double-renders. That last one is the failure the KaTeX entry above
predicted, still live on two pages.

**Verify a renderer by its positive signal.** See the KaTeX entry above: checking "no console
errors" passed for six lessons while the maths renderer was entirely absent. The check that works
is `document.querySelectorAll('.katex').length === document.querySelectorAll('.eq').length`, plus
asserting exactly **two** `script[src*="katex"]` tags (more means a duplicated block, which
double-renders).

**Splice code listings mechanically; never retype them.** 1.8 has five full-file listings totalling
~700 lines. Writing `@@LISTING:src/game/pong.cpp@@` in the page and substituting the real file
(HTML-escaped) with a small script makes drift between the listing and the compiled source
*impossible* rather than merely unlikely, which is exactly what §8's "every listing compiles at its
point in the course" demands. It also gets the `&lt;`/`&gt;`/`&amp;` escaping right every time —
and the highlighter's round-trip check then confirms the escaping survived, end to end.

## Testing engine code that talks to SDL

`SDL_GetKeyboardState` has no injection point — pushing a synthetic `SDL_EVENT_KEY_DOWN` with
`SDL_PushEvent` does **not** update the state array, because SDL updates it in
`SDL_SendKeyboardKey` when the event is *generated*, not when it is dequeued.

The seam that works: compile the unit under test together with a test TU that **defines**
`extern "C" const bool *SDL_GetKeyboardState(int *)` (and `SDL_GetMouseState`) itself. The linker
prefers the definition in the object file over the one in `libSDL3.dylib`, so key state becomes
fully drivable with no changes to the production code. This is how 1.2's six-frame edge table was
verified value-for-value rather than asserted. Keep such harnesses in the scratchpad — they are
authoring-time verification, not course content, until Module 9's testing lesson.

## Sub-pixel errors have a damage profile you cannot sample (Lesson 2.4)

The top-left rule's `-1` bias, left in the accumulators when they are divided into barycentric
weights, does **not** distort the attribute field. It *translates* it, rigidly, by

```
displacement = 1 / ‖e‖   pixels, perpendicular to the edge opposite that weight's vertex
```

The area cancels — which is the whole result. A triangle with a 100-pixel edge is off by 1/100 of
a pixel; a triangle with a 4-pixel edge is off by a quarter of one. **The bug therefore lives in
small triangles**, i.e. dense meshes, which is exactly the geometry nobody inspects individually.

The part worth internalising is what happened when we tried to demonstrate it. On a *smooth*
attribute the shift changes a channel by a fraction of one level out of 256 — undetectable. On a
*quantised* attribute (texel index, stripe, checker cell) it flips whole pixels. Sweeping the
stripe frequency over one fixed triangle with one fixed 0.088-pixel error:

| bands | 2.00 | 2.50 | 3.00 | 3.50 | 4.00 | 5.00 |
|---|---|---|---|---|---|---|
| wrong pixels (of 52) | 4 | 0 | 0 | **15** | 4 | 0 |

Nothing about the error changed across that row. Only where the thresholds happened to fall did.
So "it looked fine when I tried it" is a sample of one from a distribution containing both 0 and
15, and the discipline is to **derive the magnitude of a sub-pixel error rather than look for it**
— `1/‖e‖` is computable in your head, and the fix is one exact integer subtraction, so there is no
decision left to make once you know the number.

This also settled how to build the demo: the band count is swept live with `[` and `]` precisely
so the count jumps around. A panel showing one impressive fixed number would have taught that the
bug is visible, which is the opposite of true.

## Do not oversell a real principle with a fake symptom (Lesson 2.4)

I expected to find that stepping barycentric weights as floats drifts visibly, and to use that as
the argument for integer accumulators. Measured on a hostile 4000×900 triangle:

| method | worst error | in 8-bit colour levels |
|---|---|---|
| integer accumulator, then divide | **0** (bit-exact) | 0 |
| float weight, 4,000 adds along one row | 4.94 × 10⁻⁶ | 0.0013 |
| float weight, never reset across 900 rows | 4.26 × 10⁻⁵ | 0.011 |

One eight-hundredth of a colour level. The scary version of the claim is simply false at this
scale, and the honest argument is narrower and still sufficient: **integers are exact,
reproducible, and cost the same, so take the exact option and stop having to reason about its
error.** Where float accumulation genuinely bites is where comparisons are against tiny
differences — z-buffer tests in Module 3, which this table now feeds into.

Lesson learned about the course itself: an overstated principle backed by a symptom the student
cannot reproduce teaches them to distrust the principle. Measure first, then decide how strong a
claim the measurement supports.

## Name a type after what it is, not what it is shaped like (Lesson 2.4)

`linear_rgb` and the rasterizer-private `rgb3` have identical layout — three floats — and reusing
one for both was tempting. It would have been a lie that compiles: under `blend_space::encoded`
the numbers are stored 0–255 channel values, and a variable named `linear_rgb` holding those is
precisely the confusion Lesson 1.6 exists to prevent. Two structs, one distinction, zero runtime
cost.

The same instinct produced `struct vertex`. Bundling a position with the attributes that corner
carries is not tidiness — it makes "swap the coordinates, forget the colours" *unwritable*, and
that bug has no geometric symptom at all: right shape, right place, shading rotated by one corner,
and only for one winding.

## What correct colour actually costs in a software rasterizer (Lesson 2.4)

Measured on a 20,760-pixel triangle, 400 iterations, release build:

| fill | per triangle | per pixel | relative |
|---|---|---|---|
| flat `fill_triangle` | 20.2 µs | 0.97 ns | 1.0× |
| shaded, encoded blend (wrong) | 57.9 µs | 2.8 ns | 2.9× |
| shaded, linear blend (correct) | 232.0 µs | 11.2 ns | **11.5×** |

Nearly all of the gap is `std::pow` inside `linear_to_srgb`, three times per pixel. Note the
asymmetry that causes it: **decode has 256 possible inputs and fits in a table; encode takes a
continuous float and does not.** A 4096-entry encode table is under 0.4 stored levels of error
everywhere — the bound comes from the curve's steepest slope, `12.92 × 255 ≈ 3295` levels per unit
of light near black, so one table step moves the output ~0.8 levels and nearest-entry rounding
halves it. Left undone deliberately: 232 µs inside a 16.6 ms budget is not a problem we have, and
the entire cost disappears in Module 4 where the GPU encodes sRGB on write for free.

Hoisting matters more than micro-optimisation here. Decoding the three corner colours *once per
triangle* rather than per pixel, and taking one reciprocal instead of 20,760 divisions, are what
make the correct path affordable at all.

## A diagram can pass every automated check and still argue the wrong thing (Lesson 2.5)

The basis-transform demo draws the image of the integer lattice under the current matrix. The first
version skipped `i == 0` in the loop, reasoning that the axes were drawn separately — which left
the images of the lines `x = 0` and `y = 0` missing. The cell containing the origin therefore had
no left or bottom edge, appeared to be twice its true size, and the unit square drawn inside it
looked as though it did not line up with the grid at all.

Nothing failed. No check fired, no pixel was out of place, and the picture looked plausible. It
simply undermined the one claim the figure exists to make — *the transformed unit square is one
cell of the transformed grid*. It was found by rendering the view offscreen and looking at it.

That is now two lessons running where `check-page.js` returned `pass: true` over a defective
diagram (2.4's Figure 4 had iso-lines sprawling outside their triangle — itself a repeat of a
defect 2.3's demo had to fix). The script catches labels that collide, spill or sit on strokes. It
cannot evaluate whether the picture makes the argument. **Budget a pass where you screenshot every
figure and ask what a reader would conclude from it**, and treat a repeat of a previously-fixed
defect as a signal that the check belongs in the tooling or the authoring notes, not in memory.

## Let the type carry the convention (Lesson 2.5)

`mat2` stores two `vec2` columns rather than a `float[4]`. The payoff is larger than it looks:

- **Column-major storage stops being a convention to enforce** and becomes a consequence of naming
  the right things. Two adjacent `vec2`s are four adjacent floats in column order, which is what
  SDL_GPU and HLSL want — so there is no transpose at the API boundary and no opportunity to apply
  one twice or not at all.
- **The arithmetic can be written as its own derivation.** `operator*(mat2, vec2)` is `c0*x + c1*y`;
  `operator*(mat2, mat2)` is `{a * b.c0, a * b.c1}`. Compare with the row-times-column form, which
  is four lines of indices and offers four chances to transpose something silently. Both compile to
  the same code; only one can be verified by reading it.

The general principle: when a convention is causing bugs, look for a type whose shape makes the
convention automatic, rather than for a comment reminding people about it.

## Verify a continuous claim with a discrete measurement — and know the bias (Lesson 2.5)

The determinant claims to be an area factor, and we own a rasterizer, so the claim is testable:
transform a square, fill it with `fill_triangle`, count lit pixels, compare with
`side² × |det|`. At 140 px: identity, scale and shear exact; rotation −0.26%; `rotation · scale`
−0.06%.

The residual is not an error in the determinant. It is the fill rule counting pixel *centres*, so
it scales with the **perimeter** while the total scales with the **area** — meaning the relative
error falls as roughly `1/side`, and the demo at 44 px sees around 1%. Axis-aligned squares are
exact at any size, because the top-left rule makes shared boundaries come out right.

Worth generalising: a discrete measurement of a continuous quantity is always biased. Know which
way and how fast the bias vanishes before you use the measurement as evidence, or you will
eventually mistake a sampling artifact for a bug in the mathematics — or, worse, tune the
mathematics until the artifact goes away.

## A zero-argument function cannot be overloaded (Lesson 2.6)

Adding `mat3` broke exactly one thing in `mat2`'s published API: the free function

```cpp
[[nodiscard]] constexpr mat2 identity() { return {}; }
```

A `mat3` version would take the same arguments — none — and differ only in return type, and C++
does not overload on return type. Not "should not": cannot, because at the point of decision the
compiler may have nothing to tell it which was wanted (`auto x = identity();`).

Everything else in the file survived, and the pattern of what survived is the useful part.
`transpose`, `inverse` and `determinant` overload on the parameter type. `rotation` versus
`rotation_x/y/z` differ by name. `scale(sx, sy)` versus `scale(sx, sy, sz)` differ by arity. The
only casualty was the function with **nothing at the call site to disambiguate it**.

Fix: a static member, `mat2::identity()`, which names the type at the call site and scales to as
many matrix types as we like. The general rule worth carrying: *if a zero-argument function will
ever need a per-type version, give it a type scope or a distinct name now, while that is still
free.* The uniform `scale(float)` overload went at the same time — unused, and it would have become
a trap the moment somebody wanted a uniform 3-D scale.

## Transcribe formulas in the notation they were derived in (Lesson 2.6)

The first `mat3::inverse` was written directly in terms of `c0.x`, `c1.y` and so on, transcribing
the adjugate straight into column-stored members. Two of its nine cofactors used the wrong
component. It compiled, it looked entirely plausible, and it was wrong.

The rewrite names the elements in **written** notation first:

```cpp
const float m00 = m.c0.x, m01 = m.c1.x, m02 = m.c2.x;
const float m10 = m.c0.y, m11 = m.c1.y, m12 = m.c2.y;
const float m20 = m.c0.z, m21 = m.c1.z, m22 = m.c2.z;
```

Nine extra lines, and now every subsequent expression can be compared against any textbook
derivation without translating between row and column indexing in your head. That fixes the
*class* of error rather than the instance.

The check that caught it is worth stating too: `M * inverse(M) == I` over 300 assorted matrices is
not something you can pass by accident. When a formula is too long to verify by reading, verify it
by its defining property instead.

## Two lessons running, the same class of diagram defect

2.4's Figure 4 had iso-lines sprawling outside their triangle. 2.5's lattice was missing its centre
lines. 2.6's Figure 1 had the first leg of a vector walk drawn exactly along the x axis in a dim
dashed grey, where it was simply invisible — so the "a vector is a recipe" picture did not visibly
build the vector.

`check-page.js` returned `pass: true` for all three. It checks that labels do not collide, spill or
sit on strokes; it has no way to evaluate whether the picture makes its argument. The screenshot
pass is now a fixed part of the workflow rather than something to remember, and the specific
recurring failure is worth naming: **a diagram element drawn collinear with, or underneath,
something else is invisible even when it is geometrically correct.** Draw it brighter, thicker, or
offset — or accept that it is not communicating.

## When machinery looks incomplete, check whether it is missing an *input* (Lesson 2.7)

Lesson 2.6 built a 4×4, put a translation in its fourth column, and demonstrated that it moved a
point by exactly `(0,0,0)`. The natural conclusion is that the matrix code is missing something.

It was not. `operator*(mat4, vec4)` in Lesson 2.7 is **byte for byte** the function 2.6 wrote:

```cpp
return m.c0 * v.x + m.c1 * v.y + m.c2 * v.z + m.c3 * v.w;
```

What was missing was a reason for `v.w` to be anything in particular. Supplying that reason — 1 for
a position, 0 for a direction — made the whole thing work with no change to the arithmetic at all.

This is a recurring shape and worth recognising: **the code you are staring at is correct, and the
defect is in what the caller is saying about the data.** It is unusually hard to debug because
reading the implementation more carefully cannot help. The tell is that the implementation is
simple and obviously right, and the behaviour is still wrong.

## A magic literal at a call site is a bug waiting for a hurried reader (Lesson 2.7)

`to_vec4(n, 0.0f)` and `direction(n)` compile to identical code. They are not equally good.

The first is a magic number, and magic numbers get changed by whoever is trying to make something
compile — flipping `0.0f` to `1.0f` looks like a harmless adjustment. The second states an
intention, and changing `direction(n)` to `point(n)` is visibly a claim about what the vector *is*.

The bug that distinction prevents is the worst-shaped one in this module: a direction transformed as
a position has the translation added to it, so **the error equals the translation** — measured at
10.77, then 107.70, then 1077.03 as the object moves ×1, ×10, ×100 from the origin. It is invisible
in a test scene at the origin and ruinous in a real level, which is exactly backwards from how you
would like a bug to behave.

Cheapest test for it: a unit direction through a rotation must come back **unit length**. If it
comes back with the magnitude of your translation, that is this bug.

## Show a difference against a fixed reference, or you have shown nothing (Lesson 2.7)

Figure 1 of Lesson 2.7 drew a room twice — before and after being moved — with a lamp inside it and
an arrow. The lamp was supposed to move and the arrow was supposed not to. Both rooms were drawn
identically, so *relative to the room* nothing had changed and the reader had to take the labels'
word for it. `check-page.js` was green.

The fix was to draw an identical ruler under both copies and a dashed line from the lamp down to it:
now the lamp is visibly above tick 3 and then above tick 5, while the arrow is visibly the same
arrow. The claim became checkable by looking.

Generalising, and this is now three lessons of diagram defects in a row: **a before/after figure
needs something in it that provably did not change.** Without a fixed reference, "this moved and
that did not" is a caption rather than a picture.


## The nastiest transform bugs are invisible in the degenerate cases (Lesson 2.8)

The wrong model-matrix order `T·S·R` shears a non-uniformly-scaled object as it turns. But it is
*bit-for-bit correct* at θ = 0° and θ = 90° — the two orientations anybody types while testing — and
it is *identical to the correct order* for any uniformly-scaled object or any object with no
rotation. Verified: sweeping the uniformly-scaled demo post through 360° a degree at a time, the
worst difference across all sixteen matrix elements against `T·R·S` is `0.000e+00`.

A real scene is mostly uniform scales and mostly axis-aligned props, so a codebase with the order
wrong renders almost everything perfectly and gets caught only by the one rotating,
non-uniformly-scaled object added weeks later — by which point the wrong order is buried in a working
renderer and the new asset is the obvious suspect.

The lesson for authoring and for engine code alike: **a transform that is invisible in the
degenerate cases and wrong everywhere else cannot be tested away by trying the easy cases.** The
defence is to derive the thing once and write it down where it cannot be re-derived wrongly
(`parent_from_local()` builds `T·R·S` and nothing else). This failure profile recurs — normal
transforms under non-uniform scale (3.6), shadow bias — so it is worth recognising by shape now.


## Prove "rigid" with a measurement, not a picture (Lesson 2.8)

"Is the object deformed?" looks like a question you answer by eye, but the projection can lie about
it — an oblique or perspective view distorts shapes on purpose, so a rigid object can *look* sheared
and a sheared one can look fine. The reliable test is numeric and reads straight off the matrix:
transform the model's `x̂` and `ŷ` **as directions** (`w = 0`, so translation cannot touch them) and
check their dot product is zero and their lengths equal the intended scale. If so the object is rigid
whatever it looks like. The demo's HUD does exactly this and prints `DEFORMED` only when the measured
corner leaves 90°.

Two traps embedded in that check, both real: send the axes as `point()` instead of `direction()` and
you fold the object's *position* into what you think is its shape; and `acos` needs its argument
clamped to `[−1, 1]` first, because `dot/(|a||b|)` rounds just past 1.0 at an exact axis alignment —
which the demo hits every few seconds — and `acos(1.0000001)` is `NaN`, not 0.


## Name for the code you are going to write, not the code you have (Lesson 2.8)

`parent_from_local()`, not `world_from_local()`, even though in Module 2 the parent *is* the world.
Module 5 adds a transform hierarchy where "parent" becomes another object, and at that point the
function does not change by a character — only the meaning of the word widens. The alternative name
would have forced either a rename touching every call site or a name that lies. This is *not*
speculative generality (no parent pointer, no hierarchy machinery ships today); it is only declining
to hard-code an assumption already known to be temporary, which costs nothing. The `a_from_b`
convention is the same instinct applied to composition: the name is chosen so a wrong product is a
spelling mistake.


## Introduce math where it is first needed, not where a plan filed it (Lesson 2.9)

`vec3.hpp` shipped from Lesson 2.5 with a comment promising the cross product would arrive in Lesson
3.4, when back-face culling needed a surface normal. But Lesson 2.9's `look_at` needs a vector
perpendicular to two others — the camera's `right` axis, from the look direction and an up hint —
and there is *no honest way* to build an orthonormal basis without it. The options were: hand-roll
the specific computation inline without naming it (dishonest — it *is* the cross product, and
pretending otherwise breaks "the student should never type a line they couldn't explain"), or
introduce the cross product here. We introduced it here, and revised the deferral comment.

The general rule this reinforces: **a "just-in-time math" plan is a guess, and the first lesson that
actually needs a tool wins over the lesson that expected to introduce it.** The course's spiral is
undamaged — 2.9 introduces the cross product for a camera basis, 3.4 deepens it for a triangle
normal and its tie to signed area. Introduced where used, deepened where it recurs. The cost of
getting the plan "wrong" was one revised comment; honoring the plan would have cost a decreed
formula in the middle of a derive-everything course.


## A projection can make a rigid thing look sheared — verify frames numerically (Lesson 2.9)

When the demo's camera orbits, the whole scene turns. Is it turning *rigidly*, or is the view
matrix quietly shearing it? You cannot tell by eye, because the orthographic projection already
distorts on purpose (and perspective, in 2.10, will distort more). The check is the same shape as
Lesson 2.8's deform test, moved up a space: a correct view matrix is built from an **orthonormal**
basis, so its three axis rows must stay mutually perpendicular unit vectors at every camera angle.
The demo prints those rows and they read as a clean tripod throughout; the harness asserts
`V · world_from_camera == I` (the definition of "inverse") to `1e−4` over many random cameras.

The companion trap: a **left-handed** basis passes the orthonormality check and still mirrors the
world. `cross` anticommutes, so a single swapped argument order (`cross(backward, up)` instead of
`cross(up, backward)`) negates `right`, and the scene renders mirrored — invisible until text or
winding reveals it. The cheap guard is a handedness assertion: for a right-handed frame,
`cross(right, up)` must equal `backward` (i.e. `x × y = z`), not `−backward`.


## A matrix can't divide, so perspective defers the divide — that's why w exists (Lesson 2.10)

The single most clarifying fact about the projection pipeline: a matrix is a *linear* map, and
`x' = x/z` is not linear, so **no matrix can do perspective by itself.** What the projection matrix
does instead is copy `−z` (the depth) into `w` via its bottom row `(0,0,−1,0)`, and a *separate*
step — the perspective divide, `v/v.w` — does the actual shrink afterwards. Every piece of
"projection jargon" falls out of that one deferral: `w` stops being 1 (this is the third case
Lesson 2.7 flagged); "clip space" and "NDC" are just the before and after of the divide; and the
reason clipping (Lesson 3.3) happens in clip space is that it is the last place an edge is still
straight, before the non-linear divide bends it. If you remember one sentence about perspective,
remember "the matrix can't divide, so it stashes the depth in `w` for later."

Corollary that keeps the code honest: `xyz()` (drop `w`) and `perspective_divide()` (divide by `w`)
are kept as **two separate named functions**, never one accessor with a mode. Before a projection is
in the chain `w` is always 1 and the two agree; the moment one appears they diverge, and a silent
wrong choice is a bug that looks like a tuning problem. Two names make the call site declare intent.


## Perspective depth is 1/z-nonlinear, and the near plane is the master precision knob (Lesson 2.10)

Because the projection divides by `−z`, depth does **not** map linearly into `[0,1]`. With
`near = 1, far = 100`, a point at `z = −2` — one unit past the near plane — is already at
`z_ndc = 0.5`: half the entire depth buffer's range is spent in the first 1% of the frustum, and the
remaining 99 units share the other half. Consequences worth having as instinct: depth precision is
lavish up close and starving far away; **z-fighting is a distant-geometry problem**; and the single
highest-leverage fix is to **push the near plane out** (moving `near` from `0.01` to `0.5` buys back
orders of magnitude of far precision), which costs nothing. This is why "set near as large as your
scene tolerates" is standard advice, and it is the setup for reversed-Z (Exercise 2.10.5) and the
z-buffer (Lesson 3.1). Made it a number in a harness so it is an instinct, not a warning.


## The clean way to A/B two rendering modes is one matrix, one path (Lesson 2.10)

The demo's `[P]` perspective/orthographic toggle changes **nothing** in the draw code — it selects
which matrix `project()` receives. Both projections run the identical `clip = proj * v` →
`perspective_divide` → viewport path; the orthographic matrix simply keeps `w = 1`, so the same
divide harmlessly divides by one. This makes the comparison *honest*: the only variable between the
two pictures is whether the matrix wrote depth into `w`, so anything that differs on screen is
genuinely perspective and not an incidental difference in how the two modes are drawn. General rule
for "show the artifact" toggles: route both sides through one code path and vary a single input, so
the toggle isolates exactly the thing under study — the same discipline as 2.4's fill-rule coverage
counter and 2.8's `[O]` order toggle.


## A convention that lives in twelve places will eventually be wrong in one (Lesson 2.11)

The `+y`-up-to-`+y`-down flip appeared as a bare minus sign in Lesson 2.5's `to_screen`, again in
2.6's cube demo, again in 2.8, again in 2.9, and finally inside 2.10's `project` — five copies, each
with a comment gesturing at what it was for, none deriving it. Every copy was correct, and that is
precisely the danger: nothing was broken, so nothing forced the question, and the sixth copy would
have been written by someone who had only ever seen the fifth.

Lesson 2.11 moved it into one function, `viewport::to_screen`, written as `(1 - t_y)` rather than
`-ndc.y` so the code *states* that it is reversing a fraction instead of just looking negative.
**The general rule: when the same non-obvious sign or constant appears in a third place, it is telling
you a transform is missing a name.** Give it a type and one home. The tell is not that the copies are
wrong — it is that each one needs a comment to justify itself.

Corollary for refactors of this kind: **prove it moved nothing.** The harness swept an NDC grid
through both the old constants and the new `viewport` and reported a worst difference of
`0.000e+00`. For a "give this a name" refactor that is the whole acceptance test — if the picture
changes, the refactor is a rewrite wearing a refactor's clothes.


## Design your types to shadow the API you will port to (Lesson 2.11)

`engine::viewport` has the same six fields, with the same names and meanings, as SDL's
`SDL_GPUViewport` (`x, y, w, h, min_depth, max_depth`) — verified by reading the fetched
`SDL3/SDL_gpu.h`, not assumed. That is deliberate: in Module 4 the conversion is a field-by-field
copy rather than a translation, and any convention mismatch (origin top-left vs bottom-left, depth
range) is forced into the open *now*, while the software rasterizer is the only consumer and a
mistake costs minutes.

This is the same bet already made twice: `mat4` is column-major because that is what HLSL constant
buffers want (2.6), and the projection targets SDL_GPU's `[0,1]` NDC depth rather than OpenGL's
`[-1,1]` (2.10, the NDC-parity decision). The pattern is worth naming — **match the destination
early, port cheaply later** — with the caveat that "mirrors the C struct" is a claim about a header
that can change, so it belongs behind a `⚠ VERIFY` and a real grep of the header, never memory.


## Hand-authored geometry is DATA — validate it, don't eyeball it (Lesson 2.12)

The icosahedron ships as sixty hand-typed index numbers. Exactly one of them being wrong produces a
missing face or a doubled edge that is easy to miss in a spinning wireframe and impossible to
diagnose by staring. But mesh data is *checkable*, cheaply and mechanically:

- **Euler's formula** `V − E + F = 2` for a closed surface. Breaks if you merged or duplicated an edge.
- **Manifold:** every *undirected* edge belongs to exactly two faces — one means a hole, three a pinch.
- **Consistent winding:** every *directed* edge appears exactly once, because adjacent faces traverse
  their shared edge in opposite directions. This is strictly stronger than the manifold check.
- **Outward winding:** each face's `cross(b−a, c−a)` dotted against the face centre (relative to the
  mesh centroid) must be positive.

The last one matters most for a reason that is easy to miss: **a wireframe does not care about
winding, so a winding error is completely invisible until Lesson 3.4 turns on back-face culling and
random triangles vanish.** Authoring the data correctly *and verifying it* while the check is free
means the mesh never needs re-authoring. The general rule: when a convention has no consumer yet,
that is the cheapest possible moment to get it right and prove it — not the moment to defer it.

This is the same "check where the input can actually be wrong" judgement as `put_pixel` bounds-checking
while `at(row, col)` does not (Lesson 2.5). Code you wrote is wrong in ways review catches; *data* is
wrong in ways only a validator catches.


## The saving from indexed geometry is WORK, not bytes (Lesson 2.12)

The obvious pitch for a vertex array plus an index array is memory: the icosahedron stores 12
positions instead of 60. True, and much the less interesting half. The real win is that the render
loop transforms **each vertex once** — 12 matrix multiplies per frame instead of 60 — because the
indices are consulted *after* the transform, not before. Order the two loops the other way (walk
triangles, transform each corner) and you keep the memory saving while throwing away the compute
saving entirely.

That is exactly why GPUs carry a post-transform vertex cache, and why the loop structure in
`draw_mesh` — transform all vertices, *then* walk indices — is the structure Module 4 hands to the
hardware. Worth internalising as a general shape: **an indirection is only a saving if the expensive
work happens on the small side of it.**


## "Show the failure" only works if the failure is BIG ENOUGH TO SEE (Lesson 3.1)

The cycle scene — three woven panels whose depth order is a loop — was correct geometry from the
first attempt. The harness confirmed A over B over C over A at all three corners. And the demo
looked *fine*: the painter's algorithm and the z-buffer disagreed on **7 pixels**, a smudge you
would never notice.

The construction was right and the *parameters* were wrong. Planks laid end to end along the sides
of a triangle only overlap in a sliver near each shared corner; the wrongness was real and
microscopic. Sweeping the circumradius, plank width and overhang and measuring the disagreement
took ten minutes and moved it from 7 px to **144 px** — a fifth of the covered area, unmissable.

The lesson generalises beyond this demo. Pedagogy §5 says *show the artifact*, and it is easy to
read that as "construct a case where the bug occurs". It is not enough. The artifact has to be
loud, and whether it is loud is a **quantitative** property of your test scene that you should
measure and tune deliberately, exactly as you would tune the scene for a screenshot. A failure
demo that requires the reader to squint has failed.

Corollary: build the measurement *before* the demo. The pixel-difference counter was written to be
a HUD readout and turned out to be the tool that made the scene right.


## A verification harness will tell you your explanation is wrong, if you let it (Lesson 3.1)

The demo prints how many pixels the painter's algorithm and the z-buffer disagree about. On the
milestone scene — where sorting is genuinely correct — it read **29**, not 0. The hypothesis was
easy to reach and easy to believe: silhouette edges, where a front face and a back face share an
edge, tie exactly in depth, and the two algorithms break ties in opposite directions (`<` keeps the
first drawn; painting keeps the last).

Believing it would have been a mistake. The check was four lines: drop screen-space back-facing
triangles and re-measure. Result: `6 of 12 tris kept, 0 px differ`. The explanation was right —
*and now it is verified*, which is a different thing, and it is what let the pitfall entry state it
as fact and hand the fix to Lesson 3.4 by name.

The general rule this codebase keeps re-learning: **a plausible explanation for a measured anomaly
is a hypothesis, and hypotheses are cheap to test when you already have a harness.** The cost of
the check was minutes; the cost of publishing a confident wrong explanation is a reader who cannot
reproduce it.


## Interpolate the quantity the projection already fixed (Lesson 3.1)

The reason a z-buffer stores *device* depth rather than view-space `z` is not a convention or an
efficiency: it is that barycentric interpolation computes the unique **affine** function agreeing
with three corner values, and only one of the two candidates is affine in screen space.

Substituting the projection into a triangle's plane equation makes `w` factor straight out and
leaves `1/w` as a constant plus constants times the screen coordinates. Since
`z_ndc = −A + B·(1/w)`, device depth inherits that affinity exactly. View-space `z = −1/(1/w)` is
the reciprocal of an affine function — a hyperbola — and interpolating it linearly reads **−50.5
where the truth is −1.98** at the screen midpoint of a near-to-far edge.

Two things to carry forward:

- **The bug hides on flat walls.** If a triangle's plane is parallel to the screen, `1/w` is
  constant and both choices agree exactly. Test scenes are made of boxes and floors, which is
  precisely the geometry that cannot reveal the error. When something "works on my test scene",
  ask what family of input the test scene structurally excludes.
- **`1/w` is affine in screen space** is the reusable fact, not the depth conclusion. It is the
  entire tool Lesson 3.2 needs for perspective-correct interpolation of every *other* attribute.


## Precision is a formula, not a vibe (Lesson 3.1)

Z-fighting gets diagnosed by staring at shimmering surfaces and then nudging geometry until it
stops. It does not need to be. One depth code spans

    Δw = Δz · w² · (1/near − 1/far)      with  Δz = 1/(2^bits − 1)

of real distance at distance `w`. Put your numbers in and you get an answer in metres, and you can
compare it against the gap between your surfaces *before* rendering anything.

The formula also ranks the fixes, which staring never does. The bracket is dominated by `1/near`,
so with `near = 1, far = 100`: pulling `near` to 0.1 costs **10.09×** precision everywhere, while
pushing `far` to 1000 costs **0.9%**. The near plane is the expensive knob and the far plane is
nearly free — the opposite of most people's intuition, and it is arithmetic rather than opinion.

The demo's numbers back it: two panels 1 mm apart at ~6.4 units, where one D16 code spans
0.00208 units, means a 0.48-code gap — and 478 of 875 covered pixels (54.6%) show the wrong panel.
At D24 and D32_FLOAT, zero do. The prediction and the pixel count agree.


## Widget SVGs are not `figure.dia` SVGs, and an unstyled `<text>` is black (Lesson 3.1)

The shared stylesheet themes diagram text with `figure.dia svg text { fill: var(--dia-ink) }`.
Interactive widgets live in `.widget`, which that selector does not reach — so an SVG `<text>` in a
widget falls back to the SVG default fill, which is **black**, and disappears against the dark
theme's raised background. It had been that way since Lesson 2.12's widget shipped.

The trap has two halves and both are worth remembering. Adding `fill="var(--dia-ink-soft)"` inline
*works* in a widget (nothing overrides it) but is flagged by `apply-shared.py`'s lint, which is
unanchored — and the lint is right in spirit even where it is wrong in detail, because the fix
belongs in the shared stylesheet. `.widget svg` now carries the same `text` / `.muted` / `.mono` /
`.sm` / `.xs` vocabulary as `figure.dia svg`, so a widget label is written exactly like a diagram
label, and 2.12's widget was repaired by re-stamping.

General rule: when a lint tells you not to do the obvious thing, check whether the obvious thing is
a symptom of a missing shared rule rather than a local mistake.

## Interpolate the quantity that is affine in the space you are walking (Lesson 3.2)

Lessons 3.1 and 3.2 are the same sentence applied twice. Barycentric interpolation promises exactly
one thing: **the unique affine function of the pixel position** that agrees with three corner
values. So the only question worth asking about any quantity is *is this affine in screen space?*

- Device depth: **yes**, because the projection matrix's depth row already made it so. Interpolate
  it directly. Correcting it again is a genuine bug.
- View-space `z`: **no** — it is the reciprocal of an affine function.
- Texture coordinates, colours, normals: **no**, but `a/w` is, and so is `1/w`. Interpolate both
  and divide.

The derivation for the third case is five lines and it never mentions texture coordinates: write
`a = αx_v + βy_v + γz_v + δ`, substitute the projection, divide by `w`, and the constants cancel
out of the conclusion. One derivation therefore covers every attribute a vertex will ever carry —
which is why hardware documentation can describe varying interpolation in a paragraph.

The reusable habit: when an interpolation looks wrong, do not reach for a fudge factor. Ask which
space the interpolator walks in, and what is affine *there*.


## An error that is zero at the corners is a chord under a curve (Lesson 3.2)

Affine texture warping is invisible to every check you would naturally run. Print the corner uvs —
correct. Verify the mesh — correct. Look at the wireframe — correct. The error is **exactly zero at
all three vertices and maximal in the interior**, because that is what a chord does under a curve.

Recognising the shape tells you three things at once: where to look (the middle), why your tests
passed (they sampled the endpoints), and how it will respond to subdivision (quadratically — chord
error over an interval `h` is `O(h²)`).

That last one is worth having as a number rather than a feeling. Measured on this course's floor,
the improvement ratios per doubling run 2.31, 2.42, 2.62, 2.84, 3.10 — climbing toward 4 — and at
2,048 triangles the picture is *still* wrong on 381 pixels. **Convergence is not termination.** Any
time subdivision is proposed as a fix for an interpolation error, that distinction is the whole
argument.


## Ask what your test scene structurally cannot show (Lesson 3.2)

Both of Module 3's interpolation bugs — view-space depth in 3.1, affine attributes in 3.2 — are
*exactly zero* on a triangle whose plane is parallel to the screen, because `w` is then constant
across it and every interpolation scheme agrees. Sprites, UI quads, billboards and the front faces
of axis-aligned boxes are all in that family. A test scene built from them cannot reveal either
bug, no matter how carefully it is inspected.

This reframes "it works on my test scene" from an embarrassment into a question with a findable
answer: **what family of input does my test set structurally exclude?** Here the answer was
"anything steeply angled", and the fix was to put a steeply-angled surface in the demo permanently,
one keypress away. That is cheaper than remembering to test for it.


## A saturating metric hides the thing it is measuring (Lesson 3.2)

The demo counts pixels where two renders disagree. On the tessellation sweep it read 48.5%, 48.2%,
51.2%, 46.7% — flat, and then fell off a cliff to 4.9%. That looks like a threshold effect in the
*error*. It is not: it is the **metric** saturating. Once the uv error exceeds half a checker cell
the pattern is scrambled, and two scrambled two-colour images differ on about half their pixels
however much worse one of them gets. *You cannot be more wrong than a coin flip.*

Measuring the underlying quantity instead — worst uv error, in cells — gave a clean
`4.23 → 1.83 → 0.76 → 0.29 → 0.10 → 0.03`, from which the second-order convergence is obvious.

The general hazard: **measuring a continuous error through a quantised output caps your dynamic
range at the quantisation.** Pixel-difference counts, pass/fail rates and anything else derived
from a thresholded comparison all have this ceiling. Keep one metric on the raw quantity.


## A parameter list that keeps growing is asking to be a state object (Lesson 3.2)

`fill_triangle` collected a `blend_space` in 2.4, was about to collect an interpolation mode and a
shading mode in 3.2, and will want a lighting term in 3.6. Four trailing enums at every call site
is where an API starts to rot — and the fix was not invention but *recognition*: the hardware
already solved this, and calls the answer a **pipeline object**. A GPU does not take render state as
draw-call arguments; it bakes it into an object built once and bound before drawing, because
validating that state per draw would be ruinous.

So `fill_style` is not tidiness, it is adopting a shape that is known to be right and that Module 4
will make literal. Two properties worth copying deliberately: **every field defaults to the correct
value**, so the right call is the short call and each deliberately-broken mode must be named; and
adding a knob costs one field rather than one argument at every call site.


## The fixed-size scratch array that was fine until it wasn't (Lesson 3.2)

`collect_triangles` transformed vertices into `engine::vec3 view_pos[64]`, clamped with
`std::min(mesh.vertices.size(), std::size(view_pos))`. Correct and cheap while the largest mesh was
a twelve-vertex icosahedron. Lesson 3.2's tessellated floor has **289** vertices, and the clamp
would have silently drawn a fraction of it — no crash, no warning, just a floor with a bite out of
it and no obvious cause.

The clamp is the trap. A hard limit that *truncates* rather than failing converts a capacity bug
into a rendering bug, which is much harder to trace. Two honest options: assert, or remove the
limit. We removed it — `std::vector` scratch owned **across frames** by the caller, so `clear()`
keeps the capacity and the steady state allocates nothing.

That ownership detail is the whole trick, and it is the smallest possible preview of Module 9: the
fix for allocation in a hot loop is almost never a faster allocator, it is not allocating.


## A destructive step has a precondition, not a guard (Lesson 3.3)

The perspective divide is `x/w`, and for a vertex behind the eye `w` is negative, so the result is
not merely large — it is **sign-flipped**. A point up and to the right of the camera lands down and
to the left. `x_ndc = -0.43` is that point; it is *also* an ordinary point slightly left of centre,
comfortably in front. The divide collapses the two onto one number and leaves no residue.

So there is no test you can put *after* the divide that recovers the distinction, and no branch you
can put *inside* it that helps: the correct answer for a straddling triangle is not "draw it" or
"drop it" but "draw a different, smaller triangle". That is work, and it has to happen upstream.

**The general form:** when a step destroys information, the check that needs that information cannot
live at or after the step. It has to be a precondition enforced by whoever comes before, and the
step's contract should say so out loud. `screen_from_clip` has no guard on `w`, and its doc comment
says why: a `w` at the eye is not a case to branch on, it is a case that must not arrive.


## Two states that must not be confused should be two types (Lesson 3.3)

`engine::vertex` is a screen-space type: integer pixels, device depth in `[0,1]`, a pre-divided
`inv_w`. Every one of those fields assumes the divide has already happened, which is exactly what
makes it the wrong type to clip with. A single struct with a "have I been divided yet" flag would
compile, and would let a screen-space vertex reach the clipper, where the arithmetic is silently
meaningless.

`clip_vertex` makes that a compile error instead. This is the third time the same move has paid in
this codebase — `point()` vs `direction()` (2.7), `xyz()` vs `perspective_divide()` (2.10), and now
this — and the pattern is worth naming: **when two things have the same shape and different
meanings, spending a type is cheaper than spending a comment.**


## Prove the bound instead of clamping to it (Lesson 3.3)

`clip_polygon_near` writes into a caller-supplied buffer with no capacity check in the loop. That is
defensible only because the bound is a *proof*, written where the reader will meet it: emissions are
(vertices inside) + (crossings), one plane produces at most one crossing in each direction around a
convex polygon, a triangle has three vertices, so the worst case is exactly 4.

Lesson 3.2 learned the other half of this the hard way — a fixed array plus `std::min` turns a
capacity bug into a *rendering* bug, which is far harder to trace. The resolution is not "always
clamp" and not "always assert". It is: make the bound exact and say why, or make exceeding it fail
loudly. What must never happen is a limit that silently truncates.


## The measurement is allowed to prove *you* wrong (Lesson 3.3)

This lesson's first draft asserted that "no amount of subdivision removes the artifact" — a
satisfying line, parallel to 3.2's genuine finding about quadratic convergence. The harness
disagreed immediately: at 8×8 on the demo's floor, dropping straddling triangles and clipping them
produced **bit-identical frames**.

The reason is specific and it is the interesting part. On a *ground plane* the strip that straddles
the near plane is the one under and behind your feet, so once tessellation makes it thin enough it
falls off the bottom of the view and costs nothing. Subdivision does not fix the bug; it moves the
hole somewhere the camera is not looking.

What replaced the false claim is stronger than it was: measured over every camera the demo allows,
2 triangles lose the whole frame, 32 triangles lose the whole frame, 512 triangles still lose 51%,
and it takes **8,192** — four thousand times the geometry — before the hole is finally pushed off
every reachable view. The clipper is thirty lines and exact.

Pedagogy §5 says show the failure. It cuts both ways: build the harness so it can tell you the
failure you are describing is not the one that happens.


## Do not use a broken baseline to test the fix for the breakage (Lesson 3.3)

The obvious winding check is "signed screen area before clipping vs after". It fails, and the
clipper is innocent: the *before* triangle has a vertex behind the eye, so its projection is exactly
the garbage this lesson exists to prevent. Comparing against it measures nothing.

The test that works is **continuity**. Slide a triangle steadily through the plane, clipping at every
step, and assert the sign never changes — it cannot, because the shape does not turn inside out as
it moves. Generalisation: when testing a fix for a degenerate case, the reference has to be drawn
from the *non*-degenerate regime, and a sweep through the boundary is usually how you get one.


## `std::clamp` cannot remove a NaN (Lesson 3.3)

`std::clamp(v, lo, hi)` is `v < lo ? lo : hi < v ? hi : v`. Every comparison against a NaN is false,
so both tests fail and the NaN is handed straight back. Code that clamps "to be safe" before a cast
is therefore not safe at all, and converting a NaN — or any float outside the target's range — to an
integer is **undefined behaviour**, not a large number.

Two consequences worth carrying:

- Write the test in the form that catches NaN: `!(x < limit)` and `!(x > 0)`, never `x >= limit` or
  `x <= 0`. The negated form is true for a NaN; the direct form is false.
- A cast that is undefined for a value the program can reach is a latent bug regardless of which
  lesson first reaches it. Lesson 3.3's deliberately-broken mode is what *found* the reachable NaN
  in `linear_to_srgb_u8`, but Module 6's HDR pipeline and Module 8's physics would both have found
  it eventually, in circumstances far less convenient.


## One constant, two undefined behaviours (Lesson 3.3)

`to_pixel` clamps to ±8,000, and the number is doing two jobs at once. It keeps the float inside
`int`'s range so the conversion is defined — and it keeps `edge_function`'s products inside 32 bits,
because that function multiplies coordinate *differences* and signed overflow is undefined too.
Picking ±100,000 would have fixed the first and quietly created the second.

Worth the habit: when a limit exists to hold off one failure, check what else downstream has a
range, and pick a value that satisfies all of them. Then say so at the constant, because the next
person will otherwise assume the smaller number was arbitrary and raise it.


## Read the sign before you destroy it (Lesson 3.4)

`fill_triangle` had computed the triangle's signed area since Lesson 2.2, and then immediately
swapped two vertices to force it positive — because the top-left rule is stated for a
positively-oriented triangle. That swap *destroys the facing*. Back-face culling is therefore not a
new computation at all; it is one comparison inserted into the single window between the sign being
known and the sign being thrown away.

The generalisable habit: when you find yourself adding a test, look first for a quantity the code
already computes for another reason. Twice now in this module the answer was already on the stack —
`1/w` in Lesson 3.2, the signed area here — and in both cases the "expensive" feature turned out to
cost one line.


## A function signature can make a bug unwritable (Lesson 3.4)

`is_front_facing` takes three **screen-space** vertices. There is no overload that accepts a
view-space position, so the classic culling bug — asking the question before the projection, against
the camera's forward axis — cannot be written by accident against this API. The type says where the
test belongs.

That is a cheaper defence than a comment and a much cheaper one than a code review. It is the same
move as `point()` vs `direction()` (2.7), `xyz()` vs `perspective_divide()` (2.10), and
`clip_vertex` vs `vertex` (3.3): **when two things have the same shape and different meanings,
spend a type.**


## The wrong test is often the right test for a camera you are not using (Lesson 3.4)

`dot(normal, camera_forward)` misjudges 15.46% of triangles at a 55° field of view and 32.43% at
120°. Under an **orthographic** projection it is wrong **0 times out of 200,000** — because
orthographic projection is exactly the statement that every ray to the eye *is* the camera axis.

So it is not a sloppy approximation. It is a correct implementation of a different question, and
that is why it survives in codebases: it is exactly right in the orthographic views a level editor
shows you, and subtly wrong in the wide-FOV gameplay camera nobody is looking at while they write
the culler. When a bug's incidence depends on a *parameter* (here, field of view), find the value at
which it vanishes — it usually explains why the bug exists.


## Folklore deserves a measurement (Lesson 3.4)

"Back-face culling removes half your triangles" is repeated everywhere and is false. Measured over
6,000 random orientations: a cube shows **2 to 6** of its 12 triangles (mean 5.55), an icosahedron
**7 to 10** of its 20 (mean 8.80). Half is a *ceiling*, not a rule, and the geometry says why —
pair the faces whose planes are parallel, and the eye is in front of at most one of each pair, and
in front of *neither* when it lies in the slab between them. Look a cube square in the face and you
see one face, not three.

The time saved is smaller again: 54.8% of triangles removed bought **31.6%** of the frame
(19.95 µs → 13.64 µs), because back faces were precisely the triangles the z-buffer was already
rejecting on their first depth comparison. They were the cheapest pixels in the frame, not the most
expensive.

Both numbers are better teaching than the folklore was, and neither could have been guessed.


## An optimisation that quietly fixes a bug is worth understanding, not glossing (Lesson 3.4)

Culling should be invisible on closed geometry. Measured over 1,008 camera and rotation
combinations, it changes up to **44 pixels** — and **100% of the changed pixels are ones a back face
had been drawn on**. That framing needed no threshold and is the strongest form of the claim: the
only thing culling can touch is a place where you were seeing the inside of a solid.

Two mechanisms put a back face there, both ties. Along a silhouette, a front face and a back face
share an edge in 3-D and therefore have *equal* depth; the test is a strict `<`, so mesh order
decides. (Drawing front faces first drops the worst case from 44 px to 29 px — which is exactly the
number Lesson 3.1 measured and could not explain.) The remaining 29 px are quantisation: the two
faces round to integer pixels independently, so the back face's outline can stick out where the
front face's does not reach. No draw order fixes that; only removing the back face does.

So back-face culling is an optimisation *and*, marginally, a correctness improvement — not because
the z-buffer was broken, but because a tie has to break somewhere and "never show the inside of a
solid" is a better rule than "whichever triangle the index buffer listed first". Lesson 3.3 drew a
firm line between correctness and optimisation; this is the case that shows the line is real but not
always sharp.


## Choose a normaliser that reflects where the error comes from (Lesson 3.4)

Checking `dot(n, a) == det[a,b,c]` numerically "failed" at a relative error of 1.1e-4, and the
identity is exact algebra. The error was in the *test*: I divided by the magnitude of the result,
and the triple product is a difference of large products that cancels almost completely near
degeneracy — so a relative-to-result error is unbounded and meaningless there. Normalising by
`|a||b||c|`, the size of the *terms*, gives 1.06e-6 and a threshold that means something.

The general rule: when a quantity is computed as a difference, its error scales with the
**inputs**, not with the answer. Normalise by what the floats actually were.


## The extension your build system reserves may be someone else's data format (Lesson 3.5)

`*.obj` is MSVC's object-file extension. It is in essentially every C++ project's `.gitignore`,
including this one since Lesson 0.4. It is also Wavefront's model extension.

So `git add assets/cube.obj` silently does nothing. Not an error, not a warning — the file is
simply not staged, the build works perfectly on the machine that has it, and the repository is
broken for everybody else in a way that looks like a missing feature rather than a missing file.

Two habits fall out of it. **Negate deliberately** (`!assets/*.obj`) rather than deleting the
broad rule, because the broad rule is still right for build output. And **check the decision
rather than the intent**: `git check-ignore -v <path>` prints the exact rule that decided, so a
pattern with `!` in the output means "not ignored" and you have proof rather than a belief.
`git status --untracked-files=all` is the other half — plain `git status` collapses a whole
untracked directory to one line and will happily hide that only three of its four files matter.


## A file's idea of a vertex and the hardware's idea of a vertex are different ideas (Lesson 3.5)

This is the whole of what makes writing an OBJ loader worth a lesson rather than an afternoon.
OBJ gives each face corner **three independent indices** — `f 1/3/7` means position 1, texture
coordinate 3, normal 7 — while a vertex buffer has **one** index that selects position, uv and
normal together, because the hardware fetches a vertex as a unit.

So a vertex is not a position. A vertex is the *triple* `(i_v, i_vt, i_vn)`, and a position shared
by two faces that disagree about its normal has to be stored twice. That is why a cube arrives as
**24 vertices, not 8** — every corner is three corners of paper, one per face, and the three
disagree about which way the surface faces. Measured on our `assets/cube.obj`: 8 positions, 4 uvs,
6 normals, 24 corner tokens, 24 distinct triples, zero reuse.

The generalisable part is not about OBJ. It is that **an asset pipeline exists because the shape
data is authored in is not the shape hardware consumes**, and reconciling the two is real work with
a real cost you should be able to quote. Anyone who has wondered why an exporter turns a tidy
model into a much bigger vertex buffer has met this without being told what it was.


## Number parsing is where a loader most easily starts lying (Lesson 3.5)

Three separate traps, all of which fail silently.

**Accept the whole token or reject it.** `strtof("1.0abc", &end)` returns 1.0 and points `end` at
the `a`. If you do not compare `end` against the token's end, a typo in a model file becomes
geometry instead of an error.

**`SDL_strtod` is not `strtod`.** SDL's header documents it as making *fewer* guarantees than the
C runtime's: "the handling of scientific and hexadecimal notation is unspecified". Exporters emit
`1.0e-5` constantly. Checked in `SDL3/SDL_stdinc.h`, not assumed — and it is the sort of thing that
would have looked like a natural choice for a program that already links SDL.

**`strtof` reads the decimal point through `LC_NUMERIC`.** On a machine whose locale writes `1,5`,
a program that has called `setlocale(LC_ALL, "")` parses `"1.5"` as **1** and drops the fraction,
for every number in every asset. We are safe only because neither we nor SDL calls `setlocale`.
`std::from_chars` is the principled fix — locale-independent by definition — but floating-point
`from_chars` was the last piece of C++17 to reach the standard libraries and arrived very late in
libc++, so it needs a `__cpp_lib_to_chars` guard rather than an assumption.


## Topology is a property of the surface, not of the array that encodes it (Lesson 3.5)

A uv seam stores one point of the surface twice, because the two copies need different texture
coordinates and a vertex cannot hold two values. That is correct and unavoidable. It also means
**the vertex array is an encoding, not the set of points** — so every topological question (is it
closed? is the winding consistent? what is V − E + F?) has to be asked of the *welded* mesh.

Ask them of the raw arrays and a perfectly watertight torus reports 48 boundary edges: a
seam-shaped hole in a model with no hole. It is a spectacularly confusing false alarm, because the
model renders perfectly and the number is precise.

The same distinction settles what a round-trip test should compare. Writing a mesh and reading it
back must preserve the *geometry* — the expanded list of triangle corners — not the vertex array's
ordering, which the loader has no way to reproduce and which means nothing.


## "Same point in principle" is not "same number", and welding needs the second (Lesson 3.5)

Generating a torus, the natural way to place the seam column is to compute its angle from its
texture coordinate: the last column has `u = 1`, so its angle is `1.0f * 2π`, and the first has
`u = 0`, so its angle is `0`. Mathematically identical positions. In `float`:

```
sin(0.0f)                    = 0
sin(1.0f * 6.28318530718f)   = 1.74845553e-07
```

A float cannot hold 2π exactly, so the two columns land 1.7 × 10⁻⁷ apart. Nothing renders
differently. But no welder recognises them, so the seam becomes a boundary and every check in the
previous entry reports a hole that is not there.

The fix is to compute the angle from the **wrapped index** (`i % nu`) so the last column literally
reuses the first column's angle — the two positions are then the same *number*, not merely the same
*point*. The general lesson: if two values must compare equal later, arrange for them to be
produced by the same computation, rather than by two computations that agree in exact arithmetic.


## Euler's formula is a statement about spheres (Lesson 3.5)

`V − E + F = 2` is how everyone learns it, and it is the special case. The real statement is
`χ = V − E + F = 2 − 2g`, where `g` counts the holes: a sphere, cube or icosahedron gives 2, a
torus gives **0**, a two-holed pretzel gives −2. χ is invariant under subdivision — cutting a face
in two adds one face and one edge, which cancel in the alternating sum — which is exactly why it is
a fact about the *shape* and not about the mesh, and why it can be computed on the triangulated
form and still be talking about the cube.

The practical consequence is a rule about validators: **χ is a diagnostic, not a validity
condition.** A loader that asserts 2 rejects every handle, link, chain and pair of glasses ever
modelled. The conditions that really are errors for a renderer are different ones — boundary edges,
non-manifold edges, inconsistent winding — and they should be reported as separate counts, because
"this mesh has 4 boundary edges" tells you where to look and "invalid" does not.


## The general test costs the same as the test that only works sometimes (Lesson 3.5)

Lesson 2.12 checked "wound outward" by taking each face's normal and dotting it against the vector
from the centroid. That silently assumes the solid is **star-shaped about its centroid** — that a
ray from the centre hits the surface once. It is true for an icosahedron and false for the first
torus you meet, whose centroid is in the hole.

The assumption-free test is the signed volume by the divergence theorem: sum `dot(a, cross(b, c))`
over every triangle and divide by six. Space outside the solid is swept an even number of times
with opposite signs and cancels; the interior is swept once. **The origin's position is
irrelevant**, which is precisely what makes it general. Positive means wound counter-clockwise seen
from outside — the property back-face culling depends on and cannot check for itself.

It is also barely more code than the wrong test, and it comes with a free numeric check: a unit
cube reads exactly 1.0, and a torus converges to `2π²Rr²` from below with second-order error
(12.3% → 3.2% → 0.8% → 0.09% as the resolution doubles). Verify a geometric predicate against a
closed form whenever one exists — a predicate that returns a *number* can be checked, and one that
returns a *bool* can only be believed.


## Distinguish "malformed" from "silly", and report both (Lesson 3.5)

A loader for real-world data has to answer a policy question before it answers any technical one:
which inputs stop the load, and which are absorbed?

The line that worked: **malformed is fatal, silly is counted.** `f 1/x/2` is not an OBJ file, so
guessing what it meant helps nobody — that stops with a line number. A face whose three corners are
the same vertex is a perfectly good OBJ file describing a triangle with no area, usually left by a
merge operation; real files contain these, so it is dropped and counted and the load succeeds.

Both halves have to appear in the report, which is the part that is easy to skip. A loader that
silently absorbs oddities and a loader that dies on real data are both unusable; what makes one
usable by somebody who is not its author is that the report says *how much* it ignored. `skipped
412 lines` is a very different statement from silence.


## A check-page pass is a floor, not a ceiling — two figures said false things (Lesson 3.5)

`check-page.js` reported `pass: true` on figures that were actively wrong, because it checks
*collisions*, not *claims*.

Figure 5 shaded two overlapping cones from an origin and asserted they cancelled outside the solid;
they were drawn at angles where the nesting was invisible, so the picture demonstrated nothing.
Figure 4 was worse: it drew a concave pentagon, highlighted the fan triangle `(0,2,3)`, and
captioned it "leaves the polygon" — and a point-in-polygon test on that triangle's centroid put it
firmly *inside*. The figure's central claim was false, and it looked plausible.

Both were caught by rendering the figure and reading it against its own caption, which is now the
habit. And the second one produced a better lesson than the wrong one would have: a fan is correct
**iff the anchor corner can see the whole polygon** (star-shaped about it). Convexity is the
*sufficient* condition everyone quotes, and it is sufficient because in a convex polygon every
corner works. So a concave face may fan perfectly from one corner and grow fins from another —
which means the bug depends on where the exporter started listing the face, and that is what makes
it appear in one file and not the next one that looks just like it.

When a diagram makes a geometric claim, **test the claim numerically**, not just the layout.


## A constraint nobody rechecked cost 18% of the docs tree (CSS extraction)

The shared stylesheet and page script were duplicated into all 36 pages — 26.6 KB and 8.0 KB
each, **1.18 MB, 18% of `docs/`** — because the spec said each lesson had to be "fully
self-contained … no external assets". That rule was written to protect a real property: a lesson
must render by double-clicking it, offline, with no server and no build step.

The rule outlived its justification. **`file://` does not block a relative `<link
rel=stylesheet>` or a classic `<script src>`.** The restriction people remember is on
`fetch`/XHR/ES modules, which are a different mechanism. So the no-build-step guarantee never
actually required inlining — the duplication was protecting against something that was not there.

Two lessons, and the second is the sharper one:

- **Test the constraint, don't inherit it.** The premise was checkable in about five minutes with
  a three-file fixture and a headless browser. It had instead been carried, unexamined, through
  36 pages and a purpose-built propagation tool.
- **Check it in the engine that is strictest, not the one you have open.** Lesson pages link
  *upward* (`../shared/course.css`), and WebKit — Safari's engine, the one with the tightest
  `file://` policy — is the one that could plausibly have refused. Verifying in Chromium alone
  would have proved almost nothing about the macOS reader who double-clicks a lesson. All three
  engines pass, including the upward traversal; that is the claim worth having.

What it cost, stated so nobody rediscovers it as a bug: **a lesson file is no longer portable on
its own.** Copied out of the tree it renders unstyled. The `docs/` directory is the unit now.

### The failure mode traded for the old one

Duplication drifts loudly enough to be findable (six versions of the highlighter by Lesson 1.2).
A **wrong relative href does not fail at all** — no error, no console warning, just an unstyled,
inert page that reads as unfinished rather than broken. And it breaks by *moving* a page, not by
editing one, so it arrives in commits that look unrelated. The prefix depends on depth (`shared/…`
at `docs/`, `../shared/…` at `docs/lessons/`), which is why `apply-shared.py` computes it per page
rather than trusting anyone's eye, and why `check-page.js` now asserts the sheet is *in effect*
rather than merely linked.

### Do not ask the CSSOM whether a stylesheet loaded

The obvious probe — `document.styleSheets[…].cssRules.length > 0` — reports a **perfectly good
page as broken** over `file://` in WebKit, which treats every file as its own origin and throws a
`SecurityError` on CSSOM access. The sheet had loaded and applied; only the introspection was
blocked. The check flagged all nine sample pages while `getComputedStyle` showed the shared
`--bg: #fdfdfb` and 22,829 highlighted tokens on the very same pages.

Same shape as the KaTeX trap in this file: **a probe that cannot distinguish "absent" from
"unreadable" is not a check.** Judge by the effect (computed style), and treat a thrown CSSOM read
as *inconclusive* — never as failure.


## A normal is not a direction — it is a relationship (Lesson 3.6)

The single most useful reframing in this lesson, and it generalises far past lighting.

Lesson 2.7 established that points have `w = 1` and translate, directions have `w = 0` and do
not. That makes it look as though "direction" is one kind of thing transformed one way — and a
normal is not that kind of thing. A **tangent** is a direction: it joins two nearby points on the
surface, so it goes wherever `M` sends those points. A **normal** is defined by a *property* —
perpendicular to every tangent — and it is the property, not the arrow, that has to survive.

Write the property down and the answer falls out in three lines. Demand
`dot(M·t, X·n) = 0` whenever `dot(t, n) = 0`; use `a·b = aᵀb` to get `tᵀ Mᵀ X n`; observe that
this reduces to `tᵀn` exactly when `Mᵀ X = I`, so `X = (M⁻¹)ᵀ`.

The transferable habit: **when you do not know how something transforms, ask what defines it and
require that to be preserved.** Tangent vectors, normals, planes, and covectors generally all
fall out of this, and it saves memorising a table of rules that look arbitrary.


## The bug that only appears on objects nobody is looking at (Lesson 3.6)

The inverse transpose is **identical** to the model matrix for a pure rotation (measured:
`max |R − normal_matrix(R)| = 5.96e−08`) and **parallel** to it for a uniform scale (`0.0000°` of
difference). It differs only under a non-uniform scale.

Rotation and uniform scale describe an enormous fraction of a typical scene. In our own demo the
uniformly-scaled icosahedron — the hero, the thing you are actually looking at — shows a
worst-case normal tilt of `0.03°`, i.e. float noise, while the squashed slab reaches **67.99°**
and the plinth **66.46°**. Rendered on a flattened torus, **97.5% of the object's covered pixels
differ**, by up to 135/255 in a channel.

So the failure mode is: *the hero object is perfect and the set dressing is subtly wrong.* Nobody
files that bug. The lesson for testing is to **choose test geometry that violates the assumption
you are least sure about** — and, better, to make the invariant checkable directly: take any
tangent, transform it with `M` and the normal with your candidate matrix, and assert the dot
product is still zero. Two lines, no rendering, no eyes.


## Fake shading gives itself away by what it does NOT do (Lesson 3.6)

`face_shade(base, face_index)` coloured every surface in this course for five lessons, and no
individual frame it produced ever looked wrong. The tell is not a bad colour — it is the
**absence of a relationship**: spin the object and the shading does not move, because
`face % 5` does not care which way the surface is pointing.

This is worth generalising into a debugging instinct. When something looks plausible but you
suspect it is not real, do not stare harder at one frame — **change an input that the correct
implementation must respond to, and check that it responds.** Rotate the object and watch the
shading; move the camera and watch the specular (and watch the diffuse *not* move, which is
equally informative). A still image cannot distinguish a computation from a lookup table; a
derivative can.


## Adding a light required no rasterizer changes, and that was informative (Lesson 3.6)

`fill_style` gained no field. `fill_triangle` gained no branch. Lighting's output is a vertex
colour, and interpolating vertex colours across a triangle has been the fill's job since Lesson
2.4, so a whole new subsystem dropped in with zero changes to the code that draws pixels.

That is not luck — it is the **vertex/fragment split** appearing before it was named. Lighting is
per-vertex work whose result feeds per-pixel work, and the pipeline already had that boundary
implicitly. Module 4 makes it literal with two shaders.

The corollary is the useful part: it also revealed that `fill_style` is the *wrong home* for
shading parameters. Specular colour and shininess belong to the surface, not the fill, and 3.7
will not be able to fit them there. When a new feature slots in with no changes, that is evidence
the boundary is in the right place; when the next one cannot, that is where the next abstraction
goes.


## Verify a figure's claim numerically, not just its layout (Lesson 3.6)

Lesson 3.5 already learned that `check-page.js` cannot see a false claim. Lesson 3.6 produced
three more instances, in one lesson, all of which passed the collision checker:

- **Figure 1** was drawn with the surface at 30° while its labels said 60°, so a reader measuring
  the picture would have got a footprint of 1.15 where the text said 2.00.
- **Figure 2** shaded the *wrong half* of the disc as unlit, drew the terminator along the wrong
  diagonal, and mislabelled one cosine as 0.28 where the geometry gives 0.10.
- **Figure 3** and its interactive widget disagreed with the worked example, because the figure
  squashed one axis and the example squashed two.

The fix that worked was to **compute the figure's coordinates in a script and read the labels off
the computation**, rather than placing them by eye and annotating them from memory. For Figure 2
that meant a five-line program printing each sample point's normal, its dot product with the
light, and the arrow's endpoint. Every number in the final SVG came from that output.

For an interactive widget the same rule applies with more force, because a reader *will* drag it
to the value the prose quotes: the widget, the figure and the worked example must all use the
same inputs. Ours now all use the slab's `(1.8, 0.35, 0.9)`, and dragging the slider to 0.35
reproduces the prose's 137.5° and 90.0° exactly.


## Two correct branches can leave a hole between them (docs tooling)

The change that extracted the shared CSS and script into `docs/shared/` converted all 36 pages
that existed **on its branch**. That branch was cut before Lesson 3.6 landed on `main`. So when it
merged, 3.6 was not converted — not because the run missed it, but because the page did not exist
when the run happened, and a merge has nothing to say about a file neither side changed.

Both branches were individually correct and fully verified. The defect lived in the gap, and 3.6
shipped for weeks carrying an 881-line inline copy of a stylesheet that by then had a single source
of truth.

Two properties made it survive review:

- **It is introduced by merging, not by editing.** No diff shows anything wrong, because nothing
  about the page changed. It simply missed a change that happened elsewhere. There is no hunk to
  review.
- **It fails silently.** A missing or wrong shared link throws nothing; the page renders unstyled
  and inert, which reads as a page nobody finished rather than a page that is broken.

The general shape is worth carrying beyond stylesheets: **a branch that adds an item and a branch
that transforms every item are a bad pair**, and no amount of care on either one closes the gap.
Codemods, renames, lint-rule rollouts and dependency bumps all have it. The only reliable defence
is a checker that enumerates the current tree rather than the changed files — `apply-shared.py
--check` does exactly that, which is why it found this in one run — and the discipline is to run it
**after merges**, not only after the edits you remember making.

## Folklore survives because nobody arranges the case that breaks it (Lesson 3.7)

The first draft of this lesson said, in five places including a figure and its `alt` text, that
Phong's highlight is cut off at grazing angles because **the mirror ray dips below the surface**.
That is the standard explanation. It is also false, and one line of algebra says so: `R` is `l`
mirrored about `n`, so `dot(n, R) == dot(n, l)` exactly — if the light is above the surface then
`R` is above it too, always, without exception.

What is actually happening is sharper and more useful. `cos^p` answers only over the hemisphere
*around R*, and that is not the *visible* hemisphere. With the light `a` degrees off the normal,
the visible directions the lobe fails to cover form a wedge exactly `a` degrees wide. So the
condition is not "grazing" — it is **the light and the eye on the same side of the normal**, which
on a floor means the sun is behind you.

Two things made the error survive as long as it did:

- **The measurements agreed with it.** Every number in the harness — `dot(R,v) <= 0` for 50.4% of
  above-surface pairs, 0 of 30,806 lit pixels highlighted at 35° sun elevation — is correct and is
  *equally* consistent with the wrong explanation. Passing tests confirm the arithmetic, not the
  story you tell about it.
- **The experiment was arranged to succeed.** The first plane render put the sun behind the
  camera, which is the configuration that shows the cut-off. Adding the *other* arrangement — sun
  ahead, the sunset-on-water case — showed Phong highlighting all 30,806 pixels with no cut-off at
  all, and that contrast is what forced the re-derivation.

The habit worth keeping: **when you find yourself repeating a phrase you did not derive, derive
it.** And ship the control that could have embarrassed you; `render_37` §6 now runs both
arrangements on purpose.


## A figure can pass every automated check and still hide the claim (Lesson 3.7)

`check-page.js` reported `pass: true` on a Figure 5 that was useless. It was a polar lobe plot on
a linear radial scale, and the entire point of the figure — that Blinn still returns 0.063 at 90°
where Phong returns nothing — was a dot fifteen pixels from the origin, indistinguishable from the
dot for 0.004. No label overlapped, nothing spilled the viewBox, and the geometry was exactly
right.

The fix was to change what was plotted, not where the labels went: value against angle, on
Cartesian axes, where 0.063 is 6% of the height and plainly visible. The polar view still earns
its place in Figure 1, where the question is the *shape* of the spray rather than the size of the
tail.

Generalisable: a collision checker verifies that a figure is *legible*. Whether it is *informative*
is a question about the mapping from data to ink, and the test is to state the figure's one claim
in a sentence and ask whether a reader could extract it by measuring the picture. Lesson 3.6
learned to compute a figure's coordinates rather than place them by eye; 3.7 adds that computing
them correctly is not sufficient.

Two smaller instances of the same thing in the same lesson, both invisible to the checker:

- The lobe in Figure 1 and in the interactive widget was drawn **below the surface line**, because
  `cos^p` is defined there. The function is; the surface is not. Both are now clipped at the
  horizon.
- The widget's default exponent was `p = 8`, at which *both* models read 0 in the cut-off region —
  so the widget's own caption ("watch Phong go to zero while Blinn does not") was refuted by
  dragging it. The default is now `p = 2`, and the caption explains that the difference between
  the models is a *rough-surface* difference.


## A fast path can be bought out by a feature, and it is worth naming when it happens (Lesson 3.7)

Through 3.6, `collect_triangles` composed `view_from_world * world_from_model` once per object and
sent each vertex straight to view space — one matrix multiply instead of two. That optimisation
was available *because* the shading was view-independent: a directional light is a direction,
Lambert compares two directions, and no world position was ever needed.

A highlight needs `eye - position`, which is a question about places. So the composition comes
apart and every vertex pays a second multiply.

Nothing went wrong here. But the instinct to record is that **an optimisation is usually a
simplifying assumption with a name**, and features cash those assumptions in. Being able to say
which of your fast paths depend on which of your assumptions is most of what performance work
actually is — and it is why "why is this slower than last month?" is so often answered by a
feature that nobody connected to the loop it slowed down.


## Passing tests do not mean the test measured the right thing (Lesson 3.7)

Three of this lesson's checks passed on the first run while measuring something other than what
they claimed:

- **The per-vertex peak on a 48×24 torus.** The claim was "per-vertex evaluation misses the
  highlight". The measurement found 99.6% of the true peak and looked like a refutation. It was
  the wrong measurement: with 1,225 vertices some vertex almost always lands near the peak. The
  real defect is the **chord error** *between* vertices (up to 0.722 of full strength at shininess
  128, on 87.8% of the lit area) and the **flicker** on coarse meshes (`cube.obj`: no highlight at
  all in 157 of 180 frames). Same claim, three different instruments, only two of them sensitive.
- **The grazing comparison on a torus.** A torus presents every incidence angle at once, so
  "lower the sun" changes nothing that was not already happening somewhere on the surface. The
  experiment needs a *plane*, where the incidence angle is the sun's elevation.
- **The `n·l` leak.** The first version parked the eye exactly on the mirror ray and swept the
  light past the terminator, which drives `dot(n,h)` to −1 and returns 0 for the right reason
  rather than the one under test. The geometry that exposes it is a fixed light and eye on
  opposite sides with the *normal* sweeping between them.

The pattern in all three: **the test was sensitive to something adjacent to the claim.** Before
trusting a green check, ask what result would have falsified it — and if you cannot construct one,
the test is decorative.


## A knob that will not extend is usually two knobs (Lesson 3.8)

Lesson 3.6 shipped `shade_mode { palette, flat, smooth }` and could not add `per_pixel` to it.
The instinct is to blame the missing value. The actual problem was that the enum held **two
independent questions** — where the normal comes from, and where the shading equation is
evaluated — and their combinations form a *grid*, which a list cannot represent.

The tell was there in the code and was even written down. 3.6's own doc comment said
"interpolating the *colour* across a triangle and interpolating the *normal* and shading each
pixel are different things". A comment explaining why a type cannot express something is a
comment describing a type that is the wrong shape.

The generalisable version: **when a new case will not fit an enum, check whether the enum is
enumerating one thing.** If two of its values differ in more than one respect, they are a
product and not a sum. Splitting them is nearly always cheaper than it looks — here it also made
three previously-invisible facts checkable, because a grid has cells you can predict and a list
does not.


## Predict, then measure — and write the prediction down where it can be wrong (Lesson 3.8)

`verify_38` §A computes, for each of six cells, whether it *should* differ from per-pixel, and
then measures. Writing the prediction as code rather than as a comment caught a real error within
one run: I had claimed `face × gouraud` was degenerate unconditionally, and the harness found
**761 differing pixels**.

The reasoning that was wrong is worth keeping, because it was nearly right. With a face normal
the normal is constant across the triangle, so the shading is constant, so all three evaluation
points agree. True — while the shading depends only on the normal. Lesson 3.7 added a term that
depends on the *position*, which varies across a face even when the normal does not, and the
argument silently stopped holding one lesson earlier.

Two habits come out of this:

- **A degeneracy is a theorem with hypotheses.** When you assert that two configurations produce
  identical output, list what the output depends on and check each item — rather than checking
  the one that motivated the claim.
- **The corrected rule was shorter than the wrong one.** `if (gouraud) return true;` became a
  single condition covering both cells. A rule with a special case carved into it is often a rule
  stated at the wrong level, and simplifying it is a signal you have found the right one.


## The folklore about shading cost has a precondition nobody states (Lesson 3.8)

"Per-vertex shading is cheaper than per-pixel" is universal, and at the sizes this engine renders
it is **false**. Measured on one mesh with the resolution swept:

| px / triangle | per-pixel ÷ Gouraud |
|---|---|
| 2.8 (320×180) | **0.91×** — cheaper |
| 11.0 | 1.41× |
| 44.1 | 1.84× |
| 396.9 (4K) | 2.15× |

The argument behind the folklore is sound: a mesh has fewer vertices than covered pixels, so
per-vertex is fewer calls. The unstated assumption is that a triangle covers *many* pixels. Our
torus has 2,304 triangles covering 6,346 pixels — 6,912 vertex shading calls against 6,346
fragment ones — and the comparison inverts.

Three things worth keeping:

- **The asymptote is the number to quote** (2.15×), not any single measurement. A ratio taken at
  one resolution is a ratio taken at one point on a curve.
- **Always report the px/triangle ratio with a shading timing.** Without it the number is not
  reproducible and not transferable, which makes it not a measurement.
- Modern content lives near the crossover: a 50k-triangle character on a quarter of a 1080p
  screen averages about ten pixels per triangle. This is why GPUs shade in 2×2 quads and why
  "too many small triangles" is a named performance problem.


## Fixing the wrong axis fixes nothing, however hard you push (Lesson 3.8)

The plan for this lesson predicted that per-pixel shading would take `cube.obj` from 157 blank
frames out of 180 to zero. It took it to **157**. The 12×8 torus went 58 → 0.

Per-pixel shading fixes an *interpolation* error. A cube has six normals, and whether any of them
points near the halfway vector is settled by the geometry before shading begins — so evaluating
the equation ten thousand times instead of twenty-four changes nothing, because all ten thousand
evaluations get the same normal.

This is the same lesson 3.6 learned from the other direction ("a faceted mesh is faceted because
of the split, not the shading model"), and it is the strongest argument for having separated the
two axes at all: **each fixes a class of defect the other cannot touch.** When a fix does not
work, the first question is not "did I implement it correctly" but "is this the axis the defect
lives on".


## A figure can be geometrically perfect and still refute its own caption (Lesson 3.8)

Figure 4 was captioned "continuous but not smooth" and plotted the brightness across six facets
under Gouraud against the true curve. Both were computed correctly. `check-page.js` passed. And
the dashed line hugged the true curve so closely that the figure appeared to show the two were
*the same* — the exact opposite of the point.

The content was in the derivative, not the value. Adding a second panel underneath, plotting the
slope of each, made it immediate: the true slope is a smooth curve, Gouraud's is a staircase that
jumps at every knot. Same data, same claim, and now the reader can see it.

This is the second time in two lessons that a figure passed every automated check while hiding
its claim (3.7's Figure 5 buried a tail on a linear radial scale). The rule that has emerged:
**state the figure's one claim in a sentence, then ask whether a reader could extract that exact
sentence by measuring the picture.** If the claim is about a rate of change, plot the rate of
change.

---

## A test failing is not evidence the code is wrong (Lesson 3.9)

Five of `verify_39`'s checks failed on the first run. **Three were defects in the test.** That
ratio is not unusual and it is worth internalising, because the instinct on a red test is to go
and read the implementation.

The three, and what each one was actually measuring:

| Symptom | What the test was really measuring |
|---|---|
| The 1:1 blit test failed while its **control passed** — which is impossible if the test is sound | This rasterizer samples attributes at **integer** pixel coordinates, so pixel `i`'s sample point is `i`, not `i + 0.5`. Lining that up with a texel centre at `(i+0.5)/N` requires offsetting the quad's uvs by half a texel. |
| Clamp addressing "smeared the wrong texel" | The probe sat at `v = 0.5`, which on an 8-texel image is **halfway between rows 3 and 4**. The sample was a blend of two rows, so comparing it against one texel was meaningless. |
| 141 of 1,225 uvs "differed" after a uniform import step | `load_obj` numbers vertices by **order of first appearance** (Lesson 3.5); `make_torus` numbers them by its construction loop. `uvs[i]` named different vertices in the two arrays. Compared as sorted multisets: worst difference `0.000e+00`. |

The rule: **when a test fails, first check that it is asking the question you think it is
asking.** Two of the three above failed by not holding a second variable still — a test of one
thing has to pin everything else at a value where it does nothing.

A control that *passes* when the real test fails is the loudest possible version of this signal.
It means the two are not measuring what their names say.

## The half-texel question exists at both ends of a pipeline (Lesson 3.9)

A texel is a **sample**, so its value lives at `(i + 0.5)/N`. That is the famous half.

The one nobody mentions: a **framebuffer** has exactly the same question, and this engine
answered it differently. `fill_triangle` steps its edge functions over integers, so a fragment's
attributes are evaluated at the pixel's integer coordinate. Pixel `i`'s sample point is `i`.

So a "1:1" blit is only bit-identical when *both* answers are reconciled — the quad's uvs must
run `0.5/N` to `1 + 0.5/N`, not 0 to 1. Neither convention is wrong; assuming they agree is.

Any time a continuous coordinate is mapped onto a discrete grid, ask where in the cell the value
lives. There will be a half somewhere, and there may be two.

## Nearest-neighbour sampling cannot see a half-texel error (Lesson 3.9)

Measured over 160,801 sample positions: removing the half-texel offset changes **0** samples
under nearest filtering and **160,632** under bilinear.

This is why the bug ships. A texture pipeline can carry it for years while everything looks
crisp and correct, and reveal it the day somebody enables filtering — at which point the symptom
is "filtering makes everything soft and slightly misaligned" and **the filter gets blamed**.

Generalises: a defect that only one of two modes can express will be attributed to whichever
mode was switched on last. When a feature "introduces" a problem, check whether it merely made a
pre-existing one visible.

## Hold the confound constant, or your benchmark measures the wrong thing (Lesson 3.9)

The obvious benchmark for "what does a texture fetch cost" compares a procedural rule against a
texture lookup. It reported **5.04×** for nearest and **6.80×** for bilinear.

Both numbers are nearly meaningless. `shading::uv_checker` returns a packed pixel and encodes
nothing; `shading::textured` decodes texels and **re-encodes** the result, and `linear_to_srgb`
calls `std::pow` — three per pixel. The benchmark was mostly measuring the sRGB transfer
function.

Making all three variants `shading::lit`, so every one pays exactly one encode, gives the honest
numbers: a nearest fetch costs **1.12×** a lit fragment and bilinear **1.41×**, i.e. bilinear is
**1.26×** nearest for four fetches and three lerps instead of one.

Both tables are kept in `verify_39` §I, labelled. **A plausible benchmark that measures the wrong
thing is more dangerous than no benchmark**, because it comes with a number and numbers end
arguments. Before believing a ratio, list what *else* differs between the two things you timed.

## Aliasing is not caused by a large footprint (Lesson 3.9)

The usual explanation — "one pixel covers many texels, so it aliases" — is incomplete, and the
measurement shows why. Four test images, all 64 texels wide, so the footprint at any given screen
row is **identical** for all four: 62.46 texels per pixel two rows below the horizon. The sparkle
under a sub-pixel camera nudge still went 8.0% → 16.2% → 32.6% → **64.8%** as the checker went
from 4 to 32 cells.

What changed was the **contrast inside the footprint**. Aliasing is caused by a pixel covering
many texels *that disagree*, and the sampler having no way to average them. A large footprint over
a smooth image is harmless.

The practical consequence: if shimmer scales with texture *fineness* at a fixed camera distance,
it is aliasing and not a filtering bug — and enabling bilinear will not help.

## Duplicate SVG marker ids are silent, and stop being harmless later (Lesson 3.9)

Every generated figure emitted the same `<defs>` block with the same four marker ids, so a page
with six figures declared each id six times. `url(#e-i)` resolves to the **first** match, so every
figure was quietly using figure 1's markers.

Harmless while all six definitions are byte-identical — which is exactly what makes it a trap. The
day one figure wants a different arrowhead, it silently gets somebody else's, and nothing in
`check-page.js` looks for it. Marker ids are now namespaced per figure (`e-i-f391`); Lessons 3.7
and 3.8 still carry the old pattern.

Worth adding to a page check: `[...document.querySelectorAll('[id]')].map(e => e.id)` and look for
repeats.

## `.t-inv` needs an opposite, and the choice is a computation (Lesson 3.9)

`course.css` provides `.t-inv` — a fixed white text fill — for labels sitting on a saturated
shape, with the right justification: the shape is the same colour in both themes, so its label
must **not** follow the theme's ink.

The first draft of Lesson 3.9's figures used it for numerals on *every* swatch, including two pale
ones (`#e2ded2`, `#c2bdae`), where white on light grey was barely readable. `check-page.js` cannot
see this — it checks label *geometry*, not contrast.

The fix is a page-local `.t-onlight` (fixed dark) and picking between them by **relative
luminance** in the figure generator, not by eye:

```python
lum = 0.2126 * r + 0.7152 * g + 0.0722 * b
return "t-onlight" if lum > 0.45 else "t-inv"
```

Contrast on generated figures is a computation, not a judgement call — and it is one of the
things only a screenshot will catch.

## The clock is coarser than it is expensive, and that inverts the rule (Lesson 3.10)

Two independent properties decide what you may measure, and the intuitive one is not the binding
one. On the reference machine (Apple M4 Pro):

| | |
|---|---|
| `SDL_GetPerformanceFrequency()` | **24 MHz** → a tick every **41.667 ns** |
| one `SDL_GetPerformanceCounter()` call | **5.42 ns** |
| one `scope_timer` (two reads + an add) | **13.46 ns** |

**Reading the clock is roughly eight times faster than the clock changes.** So the limit on what
can be timed is the *tick*, not the overhead — the cheap operation is the one that stops you.

One sRGB encode takes 9.3 ns, which means four and a half of them fit inside a single tick, and
bracketing exactly one returns **0.00 ns or 41.67 ns and never 9.3**. Measured, twelve trials at
N = 1: min 0.00, max 666.67.

The rule that falls out: **never instrument anything shorter than `100 × max(tick, timer)`** — the
duration at which quantisation and overhead are both under 1%. That is 4.17 µs here, and it is why
`profiler` exposes `resolution_ns()` and `overhead_ns()` and calibrates itself at construction
rather than carrying a number in a comment. Do not assume 1 GHz: the frequency is 24 MHz here, 1e7
on Windows, 1e9 on modern Linux.

A corollary worth keeping: **a zone that reads 0.00 is either work you are not doing or work you
cannot measure, and the two look identical in a table.** `zone::build` reads zero for the second
reason and is kept, deliberately, as the rule appearing in the engine's own output.

## Instrumentation makes things slower *and* under-reports, at the same time (Lesson 3.10)

Putting a `scope_timer` around each iteration of a loop measured 9.71 → **18.08 ns/iteration**,
a 1.86× slowdown. That much is expected. The part that is not:

**The reported total is also too small.** The closing clock read happens *inside* the interval it
is trying to close, so it cannot be part of what it measures. The instrument therefore makes the
program run twice as slow while claiming it ran faster than it did — two errors, in opposite
directions, from one decision.

Fine-grained answers come from **subtraction**, not from finer instruments: two variants differing
by exactly one thing, both timed coarsely. The resolution comes from the experiment's design
rather than from the clock's.

## A differential benchmark without a control is a story (Lesson 3.10)

This lesson's first draft reported that a texture fetch costs **6.40×** more in situ than in a
microbenchmark. The number was produced by comparing a loop containing *fetch + encode + two
stores* against a loop containing *fetch*, and attributing the entire difference to the fetch.

With a proper control — the same loop, the same encode, the same two stores, and only the fetch
replaced by a value that varies the same way — the honest figure is **2.27×**:

```
fetch alone, tight loop:          2.26 ns
fetch + encode + 2 stores:       14.60 ns
CONTROL: same, minus the fetch:   9.47 ns
-> the fetch IN SITU costs 5.13 ns, against 2.26 alone (2.27x)
```

Still a striking result, and it has the advantage of being true. **A function's cost is not a
property of the function**: in a tight loop consecutive fetches overlap in the pipeline, and in
situ each one sits in a dependency chain — its uv comes from the perspective divide, its result
feeds the encode — with nothing to overlap with.

The general shape, and this is the second sighting (Lesson 3.9 §5.1 was the first): **when a
measured ratio is much larger than you can explain, the two sides differ in more than one way.**

## Median for a frame, minimum for a kernel — never the mean (Lesson 3.10)

Timing noise is **one-sided**. The machine can always be slower than your code deserves — a
context switch, a core migration, a page fault, a frequency drop — and can never be faster. So the
distribution is a hard floor with a long right tail, and the mean, which assumes symmetry, is
wrong for both cases:

- **A kernel benchmark takes the minimum.** There is one true cost and everything above it is
  interference, so the smallest observation is the closest you got to running uncontended.
- **A frame budget takes the median.** There is *no* single true cost — a frame genuinely varies —
  and the question is what a typical frame costs. One 40 ms hitch moves a mean of 120 frames by a
  third of a millisecond and moves the median by nothing.

Say which one you used. A performance number quoted without it is a rumour.

## The unmeasured remainder is the most important row in a budget (Lesson 3.10)

Six honest bars that sum to 60% of a frame look like a complete picture, right up to the moment
you make the biggest one twice as fast and the frame improves by nine percent. **Display the
remainder.** Ours is 0.2% of the frame — *because it is displayed*, which is the only reason it
stayed small.

Two related rules. Zones must be **disjoint**: two live timers count the same nanoseconds twice,
so nesting is an error rather than a feature (`zones_overlapped()` catches it). And a **sum of
medians is not the median of a sum** — they differ whenever two zones peak on different frames.
Leave the discrepancy visible; deriving one number from the others so the table adds up perfectly
is how a budget stops being a measurement.

## Triangle count is close to irrelevant; pixels per triangle is the axis (Lesson 3.10)

The prediction everyone makes, including the first draft of this lesson: the 2,304-triangle torus
must cost about a thousand times the 2-triangle floor. Measured:

| | triangles | covered px | fill | ns/triangle | ns/px |
|---|---|---|---|---|---|
| floor | 2 | 30,882 | **1,390.14 µs** | 695,069 | 45.01 |
| torus | 2,304 | 2,678 | **171.78 µs** | 74.6 | 64.14 |

**The floor is eight times more expensive with 1,152× fewer triangles.** The only column where the
two objects resemble each other is the last one, and that is what the fill is paid for.

The two phases sit on perpendicular axes, and both were swept to confirm it: 16× the pixels moved
`collect` by 3.7% (noise) and `fill` by 15×; at constant screen coverage, 36 → 36,864 triangles
moved `collect` by 840× and `fill` by 3×. **The two curves cross near one pixel per triangle**,
and that crossing is the only place on either axis where "optimise the fill" stops being right —
with nothing about the renderer changing to get there.

Consequence for how numbers get recorded: **quote ns per covered pixel and ns per triangle, never
ms per frame.** The first two are properties of the fill loop and the vertex stage and transfer
between machines and resolutions; the third is a fact about one scene on one computer, and it is
the only one most people write down. Reference values: **45.47 ns/px**, **23.0 ns/triangle**.

## Cost does not follow attention (Lesson 3.10)

The differential ladder over the fragment loop, in ns per covered pixel:

```
coverage + colour interpolation   2.682
+ perspective divide              2.664   (-0.018 — FREE, below the noise)
+ depth test & write              2.936   (+0.272)
+ sRGB encode (std::pow)          9.024   (+6.088)  <-- the largest single item
```

The perspective divide had a 5,000-word lesson written about it (3.2, which warned it would cost
you) and is free — it hides entirely behind other latency. The depth test has a lesson named after
it and costs a quarter of a nanosecond. `to_encoded` is a one-line call at the end of the fragment
that nobody has thought about since Lesson 2.4, and it is **2.1× everything above it combined**.

## Judge an approximation before rounding, not after (Lesson 3.10)

Four implementations of `linear_to_srgb`, all four passing the sRGB round trip on all 256 stored
values:

| candidate | ns/call | speedup | error (codes, un-rounded) | worst rounded | % differing |
|---|---|---|---|---|---|
| exact `std::pow` | 3.497 | 1.00× | — | 0 | 0.00% |
| **fitted sqrt chain** | **1.856** | **1.88×** | **0.0115** | 1 | 0.60% |
| threshold table + bsearch | 6.919 | 0.51× | exact | 1 | 0.00% |
| uniform input table, 4096 | 0.994 | 3.52× | 0.4022 | 1 | 3.66% |

Two findings worth carrying:

**The exactly-correct table is slower than `pow`.** Tabulating the 255 *output* thresholds and
binary-searching them is exact by construction and a genuinely good idea — and eight dependent L1
loads is a longer latency chain than a modern `powf`. It exists in the harness only because
somebody timed it instead of shipping the argument for it.

**The "worst rounded code" column cannot decide anything.** It saturates: every candidate accurate
to better than half a code reports 1. The column that separates them is the error *before*
rounding — 0.0115 against 0.4022, a factor of 35 — and that gap becomes the entire answer the
moment the target stops being 8-bit: at 16 bits they are **3.0 and 103.4 codes**. An approximation
that is only good enough because the output format is coarse has an expiry date.

## A hypothesis that fails should ship with its result, not be deleted (Lesson 3.10)

The argument for rejecting the fastest sRGB candidate wrote itself: a 4,096-entry table occupies
16 KiB of cache, and a fill loop is already dragging four megabytes of colour and depth buffers
past the same cache, so its microbenchmark advantage should evaporate in situ.

Measured, with a framebuffer-sized stream running alongside each candidate:

```
exact (std::pow)                 3.492     3.576     1.02x
fitted sqrt chain                1.853     1.870     1.01x
threshold table + bsearch        6.959     7.234     1.04x
uniform input table, 4096        1.020     1.003     0.98x
```

**No penalty, for any of them.** This machine's L2 is large enough that 16 KiB alongside four
megabytes of streaming costs nothing measurable. The hypothesis is false here — and it is kept, in
the harness and in the lesson, *with its result*, because shipping the right decision with the
reasoning that failed is exactly how folklore gets manufactured. The next person inherits "tables
blow the cache" as received wisdom with a measurement sitting right there that says otherwise.

The real reason the polynomial ships is the accuracy column above, and the comment in
`colour.cpp` says so.

## The cache cliff is not where the folklore puts it (Lesson 3.10)

A texture-size sweep at constant sample count and constant access pattern — 2²¹ random samples,
so only the working-set size varies:

```
   size        KiB   nearest ns   vs 32x32
  32x32          4         2.25      1.00x
 128x128        64         2.32      1.03x
 512x512      1024         2.29      1.02x
1024x1024     4096         2.34      1.04x
2048x2048    16384         3.88      1.72x
```

Everything up to **four megabytes** is free on this machine. "The texture must fit in L1" is advice
for a different computer, and repeating it here would be quoting somebody else's constants.

Two method notes. Sample **randomly**, not sequentially — a sequential walk is prefetched perfectly
and measures the prefetcher. And hold the sample count fixed, so the only thing varying is where
the data lives.

## Amdahl's law is an arithmetic check on your own work, not a slogan (Lesson 3.10)

`speedup = 1 / ((1 − p) + p/s)`. Use it in both directions, and the second is the one people skip:

- **Before** — as a filter. Measure `p`, compute the ceiling `1/(1 − p)`, and decide whether the
  work is worth starting at all. A phase that is half your frame can never buy more than 2×.
- **After** — as a check on yourself. `p`, `s` and the whole-frame speedup are *not independent*.
  If the frame improved by more than the formula allows, you mismeasured something. Verified in
  `verify_310` §H: ceiling 1.284×, measured 1.287×.

## An engine-wide default is a decision about the repository, not about renderers (Lesson 3.10)

`fill_style::encode` defaults to `exact` even though the demo selects `fast` and `fast` is the
right answer for any real-time renderer. The reason is not technical: every measured claim in
Lessons 3.1–3.9 — "0 of 4096 texels differ", "bit-identical", "17,275 px, 0 differ" — was made
against the exact encode, and a default that silently moved 0.60% of those pixels by one code
would quietly falsify nine lessons' arithmetic.

Same bargain as `vertex::inv_w = 1` (3.2), `lights = nullptr` (3.8) and an unbound `albedo`
(3.9): **a default that changes nothing is what lets a feature be added to a pipeline object
without auditing its call sites.**

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

## Refactoring facts, learned the hard way (Lesson 5.1)

### A refactor's claim is falsifiable, so build the falsifier first

The claim is "nothing observable changed". If nothing can prove it wrong, it is not a claim — it
is a hope with a commit message. Before a single file moved, `sandbox --shot PATH` was written:
seven pinned frames to one binary PPM, everything that could vary (time, camera, light, every
mode) a `constexpr` inside the function. 1,209,600 bytes, hash `905BF27E`.

Then the work was done in **two passes, verified separately**:

1. move 2,425 lines and 57 files without changing them → `cmp` → identical
2. redesign `collect_triangles` without moving anything → `cmp` → identical

Relocation breaks on a missing include or a namespace slip; redesign breaks on a mis-ordered
argument. One edit containing both leaves two suspects and no way to separate them.

### A characterization test that does not reach a branch has not pinned it

The first version had six frames, and the log said `straddle = 0` on every one of them — no
triangle crossed the near plane, so Lesson 3.3's clipper, several hundred lines about to be moved,
was **never called**. Frame 6 stands the camera on the floor: 32 triangles in, 8 straddling, 36
out.

The general form: **read your instrument's own counters and ask them what you missed.** The
counter that revealed this (`clip_stats::straddling`) existed for the HUD and was doing a second
job nobody had asked it to do.

### The include path is a better boundary than the style guide

```cmake
target_include_directories(engine PUBLIC include PRIVATE src)
```

`include/` holds exactly one directory, `engine`, so from outside the only spellings that resolve
are `<engine/…>`. `#include "gfx/raster.hpp"` fails with *file not found*. A boundary maintained
by discipline lasts until the first time somebody is in a hurry; this one is maintained by a
compiler, which is never in a hurry. Prefer rules a machine enforces over rules people remember.

`add_library(engine::engine ALIAS engine)` for the same reason: a name with `::` cannot be
mistaken for a file, so a typo fails at *configure* time rather than becoming `-lengine` at link
time.

### A fifteen-parameter function is a design nobody was asked to defend

`collect_triangles` had fifteen parameters at eight call sites. Every one was added by a lesson
that needed it; every addition was locally reasonable; nobody ever read the total. Read by *kind*
rather than in order, seven of the sixteen rows were one thing (policy) and three more were the
camera spelled as separate values a caller had to remember to derive together.

15 → 8, and the cost of the old shape was not ugliness: **a ninth knob meant editing eight calls
forty lines apart, so nobody would ever add one.** A bad signature is a tax on improving the thing
it belongs to.

Four rules that fell out, each with a receipt:

- the **caller's** vocabulary, not the implementation's (`camera_view`, not two loose values)
- state that always travels together travels as one thing — `fill_style` (3.2), `projector` (3.3),
  `render_options` (5.1): three for three
- **every default is the correct answer**; reaching a wrong one costs a line that says its name
- instrumentation is optional and says so in the type — a required `clip_stats&` had produced
  **seven** throwaway `clip_stats ignored;` variables

### A header is a promise about rebuild time

Measured on this tree, best of three, incremental:

| touched | seconds |
|---|---|
| `demos/sandbox/main.cpp` | 0.38 |
| `engine/src/gfx/raster.cpp` (private) | 0.42 |
| `engine/include/engine/gfx/gpu_scene.hpp` (public) | 0.81 |
| `engine/include/engine/gfx/raster.hpp` (public) | 0.97 |

2.3× on ~22k lines. The seconds are trivial and saying otherwise would be dishonest; **the ratio
is what carries** to a codebase fifty times the size, where the same two edits are a coffee break
apart. Related, and checked: 37/37 public headers compile **alone**. A header that only works
because of what came before it in your TU breaks for the next person who reorders two lines.

### The umbrella header is cheaper than folklore says, for an unflattering reason

`<engine/engine.hpp>` versus one header, same program: 262 ms / 82,507 preprocessed lines against
215 ms / 72,942. Only 22% worse — because **SDL already dominates**. Nearly 73,000 lines arrive
before we contribute anything, since `framebuffer.hpp` includes `<SDL3/SDL_stdinc.h>` for one
typedef. If we cared about compile time we would go after that first, not the umbrella.

### Names only collide once they can see each other

`enum class demo` in the sandbox's anonymous namespace **hides** `namespace demo`, so
`demo::build_scene` would not compile — lookup finds the enum and stops. Four modules of private
names had never had to be distinct from anything. The enum was the vaguer name and became
`screen`.

### A measurement taken on content that cannot express the effect is not a measurement

Two of `verify_50`'s probes reported 0 px and both were the probe's fault:

- `correct_normal_matrix = false` on the stock scene — Lesson 4.8's theorem again: a box's
  model-space normals **are** its axes, a diagonal scale sends an axis to a multiple of itself,
  and `normalize()` discards the length. Invisible on crates *by construction*. A squashed
  icosahedron reports 454 px.
- `normals = face` — the stock meshes carry **no vertex normals at all**, so both settings fall
  back to the face normal. `normal_stats::fell_back = 132` said so, which is why that counter
  exists.

### A test that asserts a known defect is not a mistake

`cull_choice` is applied in two places: `collect_triangles` reads exactly one of its four values
(`back_by_forward`, which must happen before the divide) and `draw_triangles` applies the rest via
`fill_style`. `verify_50` pins that — `cull = back, in collect only → 0 px, expected 0`. It stops
one known defect from quietly becoming two, and tells whoever repairs it which line the repair
must change.

### If a refactor does not make the harnesses simpler, it was decoration

`build_verify_49.sh` listed eighteen engine translation units by hand and grew by a line every
time the engine gained a file. It now names one include directory and one archive. All five
earlier harnesses were rebuilt against the new layout and re-run (`ALL PASS`), at a cost of 68
respelled include directives — real work, and worth counting rather than hiding.

## Course-infrastructure facts (docs/, 2026-08-26)

### `max-height` is a no-op on anything already shorter than it

The whole listing-fold feature is one rule on `.listing pre`, applied unconditionally to all 798
listings across 50 pages, and it changed only the ~230 that are whole files. The ~560 short
excerpts clear the bar and render pixel-identically. No markup migration, no builder change, no
re-stamping. When a retrofit looks like it needs an attribute on every element, check first whether
the property you want is already inert on the elements you meant to skip.

### The corpus was safe to automate because its shape is bimodal, and that was measured first

Listing lengths: median 9 lines, p75 74, p90 290. The count exceeding a threshold barely moves
between 24 and 60 lines (233 → 209) — the pages are made of short excerpts *and* whole files, with
almost nothing between. That measurement is what justified an automatic threshold instead of a
per-listing opt-in flag. A threshold over a distribution you have not plotted is a guess.

### A clamp that must survive first paint belongs in CSS, not in the page script

`course.js` is a classic `<script>` at end of body, so it runs *after* the browser has painted and
after it has jumped to any `#anchor`. Collapsing the document there would yank a reader who was
already positioned. Clamping in CSS happens at parse time and cannot. The script's whole job is to
*add controls*, and the bottom bar is `position: absolute` while collapsed so even that is free:
removing every injected bar changes document height by exactly 0 px. Measured end to end, the
script now moves Lesson 5.1's height by 33 px — one 13-line listing being released.

### `<details>` hides content with `display: none`, which blinds computed-style probes

It was the semantically obvious choice for a collapsible listing and the wrong engineering one:
`check-page.js` decides `sharedCssApplied` and `wrappedListings` from
`getComputedStyle('.listing pre')`, and a closed `<details>` returns `rgba(0,0,0,0)` and `normal`
for a subtree that is perfectly styled. Clipping with `overflow: hidden` keeps every probe honest —
and keeps the text in the accessibility tree, which is the right trade for a screen reader anyway.

### Decide by line count, not by measuring the box

`textContent.split("\n").length` needs no layout, so it is correct before webfonts settle and
correct for the one listing in the course nested inside a closed `<details>`
(`01-07-vectors-2d.html`), where `scrollHeight` and `clientHeight` both read 0 and every
measurement-based test calls a 400-line file "short".

### A verifier must expand what it is about to measure

`check-page.js` now opens every listing before running the geometry and layout checks. Otherwise a
clamp hides the very lines that cause a horizontal-overflow or wrapped-listing regression, and the
suite passes on a broken page. The fold-specific checks run *first*, against the collapsed state a
reader actually loads — including `clippedWithoutToggle`, which fires en masse if `course.js` fails
to load at all, exactly when every other signal still looks fine.

### Two vocabularies for one concept will get crossed, so make the checker enumerate them

`.badge` takes `mod`; `.tag` takes `modified`. The builders emitted `class="tag mod"` — right word,
right-looking source, no matching rule — so 82 badges across 14 pages rendered neutral grey next to
144 amber ones, for six lessons, without anyone noticing. It was fixed at the emitter rather than
by aliasing `.tag.mod`, because the alias would have blessed both spellings permanently. The guard
is three lines: list the modifiers a class actually defines and report anything else.

### `scroll-behavior: smooth` makes fixed-delay anchor tests lie

`course.css` sets it, and lesson pages are tens of thousands of pixels tall. A Playwright check
that navigates to `#id`, waits 300 ms and measures the heading reads it mid-animation: headings
measured at y=2091 and y=5014 looked exactly like "content grows after the anchor scroll", which is
a real failure mode and cost a diagnosis detour. Use `reducedMotion: 'reduce'`, or poll until
`scrollY` stops moving.

### A class with no rule fails silently, and looks like a styling opinion

`<p class="mono">` was authored from Lesson 3.3 onward for hand-aligned numeric walkthroughs —
columns padded with `&nbsp;`, continuation lines indented under an `=`. No `p.mono` declaration was
ever written. All 16 blocks across two lessons rendered in the body serif at 18px: no alignment, no
monospace, and wide enough to push the page into horizontal scroll at 390px. It survived thirteen
lessons because unstyled prose still *reads* fine — the failure only shows if you know the columns
were meant to line up. The nearest neighbour in the sheet, `figure.dia svg .mono`, is a different
class in a different context and only sets the family, which is exactly the kind of near-miss that
makes a missing rule look deliberate.

### `overflow-x` was a listing rule when it should always have been a `pre` rule

Six bare `<pre>` blocks across three lessons — compiler errors in pitfall callouts, harness
transcripts in `.worked` boxes — sat outside `<figure class="listing">` and so matched no overflow
declaration at all. A 677px error message inside a 302px column spilled visibly and dragged the
document into horizontal scroll. The rule now hangs on `pre`, not on `.listing pre`, because the
"code scrolls, never wraps" bargain is a property of preformatted text and not of the chrome that
happens to be wrapped around it.

### A dedup key that ignores position hides duplicates of the same defect

`check-page.js` dedupes text-on-shape hits by `figure|text|tagName|class`. Lesson 0.6 figure 2 has
*two* labels reading "observe", and had they both collided the checker would have reported one.
When a finding says "a label named X collides", verify how many X there are before assuming the fix
is done — measuring both is what proved only the second one needed moving.

### A gutter is a width budget, and long identifiers overrun it

Lesson 1.2's timeline reserves x=10..100 for row labels. `SDL_EVENT_KEY_DOWN` at 11px is 131 user
units and ran past the axis into the t=0 event spike. Shrinking to `xs` still needs 113; anchoring
right pushes the text to negative x and trips the viewBox-spill check instead. Wrapping onto two
`<text>` elements is the fix that keeps the exact SDL3 constant, which is the whole reason the
label is there.

---

## SDL3 main-callback facts, verified at SDL 3.4.12 (Lesson 5.2)

All of the below was read out of `build/_deps/sdl3-src/` rather than remembered. Master prompt
§10 forbids guessing entry-point signatures, and this is the one place where a wrong guess does
not produce a compiler error — it produces a program with no entry point.

| Fact | Value | Source |
|---|---|---|
| Enable the callbacks | `#define SDL_MAIN_USE_CALLBACKS 1` **before** `#include <SDL3/SDL_main.h>` | `SDL_main.h:99` |
| Init | `SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])` | `SDL_main.h:345` |
| Iterate | `SDL_AppResult SDL_AppIterate(void *appstate)` | `SDL_main.h:396` |
| Event | `SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)` | `SDL_main.h:~446` |
| Quit | `void SDL_AppQuit(void *appstate, SDL_AppResult result)` | `SDL_main.h:~485` |
| Results | `SDL_APP_CONTINUE` / `SDL_APP_SUCCESS` / `SDL_APP_FAILURE` | `SDL_init.h:109` |
| Linkage | declared inside `SDL_main.h`'s `extern "C" {` block | so C++ definitions get C linkage automatically |
| Quit always runs | *"called in all cases, even if SDL_AppInit requests termination at startup"* | `SDL_main.h` doc comment |
| SDL inits events itself | `SDL_InitSubSystem(SDL_INIT_EVENTS)` **after** `SDL_AppInit` returns `CONTINUE` | `src/main/SDL_main_callbacks.c` |

### `SDL_main.h` emits a non-inline function definition, and that is the whole rule

Under `SDL_MAIN_USE_CALLBACKS`, `SDL_main.h` ends by including `SDL_main_impl.h`, which emits:

```c
#define SDL_MAIN_CALLBACK_STANDARD 1
int SDL_main(int argc, char **argv)
{
    return SDL_EnterAppMainCallbacks(argc, argv, SDL_AppInit, SDL_AppIterate, SDL_AppEvent, SDL_AppQuit);
}
```

…plus the platform's real `main`/`WinMain`. Neither is `inline`. So:

- **One translation unit per program may include it.** Two is `duplicate symbol _main`. This is
  the One Definition Rule, not an SDL quirk.
- **That file must not define `main()`.** The header ends with `#define main SDL_main`, so a
  `main` written below the include is renamed and collides with SDL's. The header says the app
  *"SHOULD NOT ALSO SUPPLY"* one.
- **The entry point therefore cannot live in a static library.** Not a linker subtlety — it is
  that `libengine.a` defining `main` would impose it on every program that links the engine, with
  no way to opt out, and `demos/sandbox` deliberately wants its own.

Corollary for the umbrella header: `engine/platform/main.hpp` is the one public header **not**
included by `engine.hpp`. An umbrella whose promise is "include everything, it is harmless" must
not be a way to acquire a `main()` by accident.

### The event/iterate ordering guarantee is three lines, and they are readable

The worry when inverting control is that Lesson 1.2's "drain, then `update()`" contract dies,
because events now arrive one at a time from a function SDL calls whenever it likes. It does not,
and the proof is in the tree we already fetch:

```c
/* src/main/SDL_main_callbacks.c */
SDL_AppResult SDL_IterateMainCallbacks(bool pump_events)
{
    if (pump_events) { SDL_PumpEvents(); }
    SDL_DispatchMainCallbackEvents();
    /* …then, only if the result is still CONTINUE, the iterate callback. */
}
```

**When a guarantee moves out of your code, go and read the code that now provides it.** The
contract survived the inversion; its enforcer changed.

Also from the same file: SDL reads its result atom *before* calling iterate, so a callback that
returned `SDL_APP_SUCCESS` is never iterated again. That is worth knowing precisely because it
means a missing guard in your own `iterate` cannot show up in a running program — only in a test
that drives the callbacks directly.

### `SDL_Init(SDL_INIT_VIDEO)` fails where there is no display

Obvious in hindsight, and it invalidated a test we had already written. Lesson 5.1 built
`sandbox --shot` specifically so a refactor could be checked automatically — and the code called
`SDL_Init(SDL_INIT_VIDEO)` first, then never used video. On the machine it was written on that is
free. On a build server it is a hard failure, so the automation-shaped test could not run anywhere
automatic.

The fix is that the surface implies the subsystems rather than the caller requesting them, which
is also why `app_config`'s field is named `extra_subsystems`. The first draft was
`SDL_InitFlags subsystems = SDL_INIT_VIDEO`, which would have forced the class to *subtract* a
flag the caller had explicitly set whenever the surface was headless — **a class quietly
overruling its own configuration is worse than a field with a clumsier name.**

### `event.key.repeat` is information `input::key_pressed()` cannot give you

`SDL_KeyboardEvent` carries `scancode`, `key`, `mod`, `down` and `repeat` (verified at
`SDL_events.h:360`). Holding a key produces a stream of key-down events with `repeat = true`.
`engine::input`'s edge queries collapse that to one edge per physical press, which is usually what
you want — but when you need to distinguish "pressed" from "held down and auto-repeating", the
event hook is the only place the answer exists. That is the clearest single reason `on_event`
earns its place beside `on_fixed_step`.

### Ownership through a `void*`: publish before you can fail

`SDL_AppInit` hands you a `void** appstate` and SDL carries that pointer for the life of the
program. The instinct is to publish it only once construction has succeeded. Do the opposite.

`SDL_AppQuit` runs *in all cases*, including a failed init. If a failure path destroys the
instance and leaves `*appstate` null, then null means both "never built" and "already cleaned up",
and you now have two teardown paths. Publishing first — `release()`, store, *then* do everything
that can fail — gives one owner, one teardown path, and an unowned interval containing no
branches at all.

### 0, 1, 2, 1, 2 — the fixed step is not once per frame, and callbacks make that easier to forget

Five frames from a machine at very close to 60 fps, run through `engine::fixed_step` (h =
0.01666667 s) — measured, not hand-computed:

| frame | dt (s) | steps | alpha |
|---|---|---|---|
| 1 | 0.0161 | **0** | 0.9660 |
| 2 | 0.0172 | 1 | 0.9980 |
| 3 | 0.0170 | **2** | 0.0180 |
| 4 | 0.0165 | 1 | 0.0080 |
| 5 | 0.0333 | 2 | 0.0060 |

Under a hand-written loop the `while` is visible; under `on_fixed_step` it is a function somebody
else calls, and assuming it runs once per frame is much easier. **Edges belong to the frame,
levels belong to the step**: `key_down()` inside a step is safe (input is frame-coherent since
Lesson 1.2), `key_pressed()` is not — on a two-step frame it fires twice from one press.

### Measure with comments stripped

`measure_52.py`'s first run reported that `sandbox` still made four lifecycle SDL calls. One of
them was the sentence *explaining that `SDL_Init(SDL_INIT_VIDEO)` is no longer called there*.
**A measurement that counts its own footnotes is not a measurement.** Strip `//` and `/* */`
before any grep-based count of API usage; string literals are safe as long as the pattern requires
a following `(`.

---

## Course-infrastructure facts (docs/, Lesson 5.2)

### `nofold` was half a feature until something used it

`course.js` has honoured `class="listing nofold"` since the fold shipped, by declining to add an
expand control. `course.css` never did — the `max-height` clamp on `.listing pre` applied
regardless. So a `nofold` listing longer than the 12-line peek was **clamped by CSS and given no
control by JS**: silently unreachable code on a page that looks fine.

Nothing had used the class until Lesson 5.2, and `check-page.js`'s `clippedWithoutToggle` caught it
on the first run. The fix is one rule, `.listing.nofold pre { max-height: none; }`, and the lesson
is general: **a half-implemented opt-out is worse than no opt-out**, because the documentation
promises the behaviour and only one of the two files delivers it.

### Long URLs need `class="reading"` on the Further Reading list

`course.css` has `.reading a { overflow-wrap: anywhere; }` — and *only* there. A Further Reading
`<ul>` written without the class renders link text as one unbreakable token;
`martinfowler.com/bliki/InversionOfControl.html` is 394px wide and pushed a 302px column into
horizontal page scroll at 390px. `pageScrollsX` catches it, but the report is a single boolean, so
finding the culprit needs a walk over every element whose `right` exceeds the viewport **and**
which has no scrollable ancestor — the ancestor test is what filters out the hundreds of tokens
legitimately scrolled off inside a `<pre>`.

### `overPx` in `svgSpill` is not always horizontal

A label sitting below the viewBox floor reports exactly like one running off the right edge. When a
spill makes no sense horizontally, check the figure's height against the y of its last row — in
Lesson 5.2's Figure 6 a caption line added late sat 12 units under a 348-unit viewBox.

---

## SDL3 logging and assertion facts, verified at SDL 3.4.12 (Lesson 5.3)

### `SDL_Log()` is not neutral — it is a specific choice made for you

```c
/* src/SDL_log.c */
void SDL_Log(const char *fmt, ...)
{
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, fmt, ap);
}
```

So every `SDL_Log` claims to be *the application speaking, at information level*. For a demo that
is exactly right. For an engine it is a lie that cannot be filtered, and it is how this codebase
ended up with 197 calls at one category and one level.

### The default priority table, and why custom categories are free

From `SDL_HINT_LOGGING`'s documentation in `SDL_hints.h`, implemented in `src/SDL_log.c`:

```
app=info, assert=warn, test=verbose, *=error
```

**Every category SDL does not know about defaults to ERROR.** `SDL_LOG_CATEGORY_CUSTOM` is where
SDL stops and applications begin, so any category an app invents lands in the `*` arm. Moving an
engine's messages off `SDL_LOG_CATEGORY_APPLICATION` therefore makes it silent-by-default with no
configuration and no filtering code — which is the single strongest argument for using SDL's
logger instead of wrapping it. A wrapper would have had to reimplement this and would have got a
different answer.

(`DEBUG_INVOCATION=1` in the environment changes the defaults to `assert=warn,test=verbose,*=debug`.
Worth knowing before concluding that your levels are broken.)

### `SDL_SetLogOutputFunction` replaces; chain it if you mean "also"

Call `SDL_GetLogOutputFunction(&prev, &prev_userdata)` *before* installing yours, and call `prev`
from your hook. Otherwise "log to a file" silently costs the user their console, which is not what
anybody means by that phrase. SDL holds a mutex across the callback, so a file sink is thread-safe
for free.

**SDL filters before the hook.** A message below its category's threshold never reaches your
output function at all, so the file and the level are independent controls — and an empty log file
is almost always the level, not the file.

### SDL's assertion level keys off `__OPTIMIZE__`, not `NDEBUG`

```c
#elif defined(_DEBUG) || defined(DEBUG) || \
      (defined(__GNUC__) && !defined(__OPTIMIZE__))
#define SDL_ASSERT_LEVEL 2
#else
#define SDL_ASSERT_LEVEL 1
#endif
```

`-O2` **alone** takes you to level 1 — `SDL_assert` disabled, `SDL_assert_release` live — with no
`-DNDEBUG` anywhere. Measured, per-function code size from the object file:

| | `-O0` | `-O0 -DNDEBUG` | `-O2` | `-O2 -DNDEBUG` |
|---|---|---|---|---|
| empty function | 20 B | 20 B | 4 B | 4 B |
| `ENGINE_LOG_TRACE` | 80 | 28 | 44 | **4** |
| `ENGINE_ASSERT` | 148 | 148 | **4** | **4** |
| `ENGINE_CHECK` | 148 | 148 | 100 | 100 |

Columns 2 and 3 are exact inverses: `-O0 -DNDEBUG` gives live assertions and dead trace logging;
`-O2` gives the opposite. **When you gate your own machinery on a build flag, find out what your
dependencies gate theirs on.**

### `SDL_disabled_assert` wraps the condition in `sizeof`

```c
#define SDL_disabled_assert(condition) \
    do { (void) sizeof ((condition)); } while (SDL_NULL_WHILE_LOOP_CONDITION)
```

Compiled, never evaluated. That is good — the condition cannot rot, and
`assert(fp = fopen(path, "r"))` still compiles and still does not open the file — and it is the
trap that made this lesson's first `ENGINE_VERIFY` wrong:

```cpp
#ifdef NDEBUG
#  define ENGINE_VERIFY(e) ((void)(e))     // fine
#else
#  define ENGINE_VERIFY(e) SDL_assert(e)   // NOT fine at -O2 with no -DNDEBUG
#endif
```

At `-O2` without `NDEBUG` we take the `#else`, SDL is at level 1, and the expression is never
evaluated — the macro whose entire purpose is guaranteed evaluation silently stopped evaluating.
**Measured at 4 bytes: a bare `ret`.** Fix: evaluate into a named `bool` unconditionally and gate
only the check, on `SDL_ASSERT_LEVEL` rather than on `NDEBUG`.

### `SDL_enabled_assert` is a `while` loop, so RETRY genuinely re-tests

```c
while ( !(condition) ) {
    static struct SDL_AssertData sdl_assert_data = { … };
    const SDL_AssertState st = SDL_ReportAssertion(&sdl_assert_data, …);
    if (st == SDL_ASSERTION_RETRY) { continue; }
    else if (st == SDL_ASSERTION_BREAK) { SDL_AssertBreakpoint(); }
    break;
}
```

Fix the state in a debugger, answer RETRY, and the program proceeds as though the bug had not
happened. `SDL_ASSERTION_ALWAYS_IGNORE` latches in that `static`, which is *per expansion site* —
so an assertion fires once and then goes quiet, which is a feature when grinding past a known
glitch and a trap when counting.

### Assertions are testable, and almost nobody tests them

`SDL_SetAssertionHandler` + a handler returning `SDL_ASSERTION_IGNORE`, then walk
`SDL_GetAssertionReport()`'s linked list: condition text, filename, line, `trigger_count`. That is
enough to prove an assertion fires on the input that should fire it, and to prove it does *not*
fire in a build where it was compiled out. Use `IGNORE`, not `ALWAYS_IGNORE` — the latter latches
and your second count comes back short.

### `if constexpr`, not `#if`, for a compile-time log floor

`#define X(...) ((void)0)` in release means the **arguments are never compiled**, so a trace call
naming a since-renamed variable keeps building in release and breaks in debug — discovered by the
person least able to explain it. `if constexpr` with a literal condition discards the statement
(nothing in the object file, and the symbol is not even referenced) while still parsing and
type-checking it. Deletion *plus* type-checking.

### Validate a whole spec before applying any of it

`--log gfx=debug,gpu=nonsens` must be a no-op, not "the first entry took effect". Parsing straight
into `SDL_SetLogPriority` leaves the user with a configuration they did not ask for and cannot
see. Two passes: parse everything into a small stack array, then apply. The array is sized from
the category count, so it needs no allocation.

---

## Course-infrastructure facts (docs/, Lesson 5.3)

### `svgSpill`'s `overPx` is *usually* vertical, in practice

Second lesson running where every `svgSpill` finding turned out to be a note line below the
viewBox floor rather than text off the right edge. The generator computes note positions as
`y0 + i * 14` and the height is a literal — add a line and the last one falls off. Worth checking
the figure's height against its last row *before* hunting for a long string.

### A figure can pass every automated check and still be wrong

`check-page.js` verified geometry, overlap, spill and theming on all six of this lesson's figures,
and two of them had **each other's captions** — the builder's `FIGURES` dict mapped
`FIG_REPORT → l53_fig5.svg` while the generator wrote `l53_fig5.svg = fig_build_matrix`. The page
rendered a table of byte counts under the caption "three subsystems, arrived at independently".

Only a screenshot catches this. The fix that prevents it recurring is to **number the SVG
filenames by page order and keep the generator's map in that order too**, so a filename that
disagrees with its figure number is visible in one file rather than across two.

### Recreate throwaway tooling inside the repo, not `/tmp`

`/tmp` is cleared between sessions, so `check-pages.mjs` and the screenshot driver had to be
rewritten from memory. They now live in `scratch/` (gitignored) where they survive:

```sh
(cd docs && python3 -m http.server 8765 &)
node scratch/check-pages.mjs $(cd docs && ls *.html lessons/*.html)
node scratch/shot-figs.mjs lessons/05-03-logging-and-errors.html scratch/figs53
```

Note also that `pageScrollsX` is a bare boolean in the checker's result, so a reporting loop that
only prints non-empty **arrays** shows a failing page with no reason at all. `check-pages.mjs`
special-cases it.

---

## C++ and container facts (Lesson 5.4)

### A member function named `handle()` blocks the type `handle<T>`

`engine::handle<T>` is a good name and it collides with an idiom this codebase uses everywhere —
`gpu_device::handle()`, `gpu_texture::handle()`, `gpu_mesh::handle()` all return the underlying
SDL object. Inside such a class, unqualified `handle<mesh_data> h;` does not compile:

```
error: explicit qualification required to use member 'handle' from dependent base class
```

Reproduced in four lines. Name lookup finds the member first. The fix is to use an alias
(`mesh_handle`) or qualify (`engine::handle<mesh_data>`), which is why every line of engine and
demo code uses the alias. Worth knowing before naming any short, generic type that shares a word
with a common accessor.

### `pop_back()` does not free what a moved-from element owned

Removing from a dense container by "move the last element into the hole, then `pop_back`" leaves
the moved-from `T` sitting in the vector's *capacity*, and a moved-from `std::vector` is only
required to be "valid but unspecified" — keeping its buffer is a perfectly valid unspecified
state. So a pool of `mesh_data` can hold a freed mesh's megabytes indefinitely while every size
and count reads correctly. Assign `T{}` over the vacated slot before popping.

### Guard the self-move in swap-and-patch

`items_[dead] = std::move(items_[last])` when `dead == last` is self-move-assignment, which is not
required to leave the object in any particular state — a `std::vector` member is entitled to end
up empty. Removing the *last* element is the common case, so an unguarded version is wrong in the
case it will hit most often, and intermittently. `if (dead != last)` is correctness, not an
optimisation.

### A dense index doubling as the occupancy flag costs nothing and cannot desync

`slot::dense == 0xFFFFFFFF` means "this slot holds nothing". A separate `bool occupied` would be
a byte per slot, a second thing to update on every insert and remove, and a second thing to forget
to check. One sentinel answers both questions and cannot disagree with itself.

### Bump the generation on *removal*, never on insertion

Bumping on free means a vacated slot immediately carries a value no outstanding handle holds, so
the window between free and re-allocation needs no separate flag. Bumping on allocate leaves that
window open, and closing it needs exactly the extra flag the sentinel above avoided.

### Resolving a handle costs about 0.13 ns more than a dereference

Measured over 4,096 objects × 20,000 reps, alternating the two loops so thermal drift hits both:
`pool.get(h)` 0.98 ns against a raw `const T*` at 0.85 ns, stable across five runs. That is well
under a cycle — the two extra loads are adjacent in one cache line and sit in the shadow of a
dependent load that misses anyway. It is negligible **once per object** and would not be once per
vertex, which is the entire argument for resolving at the top of the object loop rather than at
each use.

### LIFO free lists concentrate generation churn; FIFO spreads it

Reusing the most recently freed slot is the cheapest insert (that slot is still in L1) and it
means a hot allocate/free pair hammers *one* slot's generation — which is the only thing that
makes a 12-bit generation wrap reachable at all. FIFO reuse multiplies time-to-wrap by the number
of slots and costs a `deque` and some cache locality. Know which one you picked and why.

### The "handle from the wrong pool" hazard turned up the same afternoon

Lesson 5.4's Pitfall 5 warns that the type system cannot catch a handle minted by a *different*
pool of the same type. `scratch/verify_50.cpp` hit it immediately: it built a `scene_object` by
hand with `geometry = icosahedron_mesh()`, which is now a type error, and the obvious fix — mint a
handle in the harness and assign it — would have produced a handle the render's own pool could not
resolve. The honest fix is that **a struct that describes an object for somebody else to render
cannot carry a handle at all**; it carries a `mesh` view and the renderer's pool owner stores it.

General rule: a handle is only meaningful alongside the pool that issued it, so any type that
crosses a boundary where the pool changes should carry the *data* or take the pool with it.

### Adding a header can give a previously SDL-free header an SDL dependency

`gfx/mesh.hpp` included no SDL until it included `core/pool.hpp`, which includes `core/log.hpp`
for its one warning, which includes `SDL_log.h`. Harmless here — `SDL3::SDL3` is `PUBLIC` on the
engine target, and all 44 public headers still compile standalone — but worth noticing, because a
container template pulling in a logging dependency is the kind of thing that is free until the day
somebody wants the container somewhere SDL is not.

### Count, do not increment

Lesson 5.3's STATE recorded "43 public headers"; `find engine/include -name '*.hpp' | wc -l` said
42 before 5.4 and 44 after. The 43 was arrived at by adding to a previous number rather than by
counting, and it had been wrong for a lesson. Any number in a document that can be produced by a
one-line command should be produced by that command every time it is written down.

---

## Asset-system facts (Lesson 5.5)

### `SDL_GetPathInfo(path, NULL)` is the documented existence check — and it hands you the size

SDL3's `SDL_GetPathInfo` "returns true on success or false if the file doesn't exist", and fills
an `SDL_PathInfo` with `type`, `size` and three timestamps. So a search path can report which root
answered *and* how many bytes the file is without a second syscall. Test
`info.type == SDL_PATHTYPE_FILE`: a **directory** of the right name is otherwise a hit, and the
caller then fails much later, inside a loader, with a far worse message. Both are SDL 3.2.0.

### A cache's map must nest a `contains()` on the pool

`std::unordered_map<key, handle>` is a **hint**, not the truth. If an asset is freed by handle,
the map entry survives and looks live. Every probe therefore reads

```cpp
if (auto it = by_key_.find(key); it != by_key_.end()) {
    if (pool_.contains(it->second)) { /* hit */ }
    by_key_.erase(it);            // the pool freed it; the map catches up
}
```

This is only possible because a generational handle cannot lie about whether its asset exists. A
pointer-keyed cache has no way to ask, which is precisely the 4.8 mesh cache's problem.

### The default configuration must serialise to nothing

`asset_key(name, settings)` originally always appended `"|flip=1"` or `"|flip=0"`. Generated
content (`insert_mesh("cube", …)`) has no settings and was stored under the bare name, so
`find_mesh("cube")` — which encodes default settings — looked up `"cube|flip=1"` and missed.
**Two key spaces wearing one name**, silent, no crash, no log. Making the *default* encode to
nothing puts generated and loaded content in one key space by construction. Same shape as
reserving generation 0 so an all-bits-zero handle is null for free.

### Collect, then recurse, when a cascade erases from the container it walks

`release_mesh` walks `mesh_derivations_` looking for children and recurses; the recursion erases
from that vector. With **one** dependent per source — which is every mesh the demo has — the
unguarded version is correct. Copy the children into a local first. The worst class of bug:
correct in every test you would think to write.

### Timing stages separately double-counts unless you know what nests inside what

The first version of `measure_55.py` stacked `read` beside `parse` and summed them. `load_obj`
**opens the file itself**, so the separately-timed read is a *subset* of the parse. It also
stacked `validate`, which the asset store does not perform at all. The bar totalled 6.20 ms for
an operation that takes 3.69. Before stacking timings, write down which call contains which.

### On this machine, an OBJ acquire is 99.9% parse

torus.obj, 200 KB, 1,225 vertices: resolve 0.0022 ms, read 0.0148, parse 3.6922, uv flip 0.0034,
store into the pool ~0. "Asset loading is slow because disks are slow" is a sentence from a
different decade. It is also why a *cooked* format is a real optimisation and a faster text format
is not — the win is not reading fewer bytes, it is not parsing them.

And a finding that fell out of separating the stages: the demo's `validate()` pass costs **2.45 ms,
another 66% of the acquire**, and has been quietly doubling load times since Module 3. Cost you
can see is cost you can choose.

### A member function hides a namespace-scope function of the same name

`asset_store::load_image` hides `engine::load_image`, so an unqualified call inside the class
fails with "too few arguments" — a confusing way to be told about name lookup. Qualify it. This is
the second lesson running to hit the shape (5.4: a member `handle()` blocking the type
`handle<T>`); short, generic names collide with accessor idioms.

### Test edits are honest only when they record what changed

Lesson 5.5's `load_model` stopped freeing the previous mesh (a load is not a free), which broke a
`verify_54` check written one lesson earlier asserting the opposite. The harness was right to
fail. The fix is to assert the *new* contract with the reason written next to it — a test edited
to match the code teaches nothing unless the edit says what moved.

---

## Measurement facts (Lesson 5.6)

### Three ways a microbenchmark lies, all three hit in one afternoon

1. **The timer is coarser than the work.** `SDL_GetPerformanceFrequency()` is 24 MHz on this
   machine, so one tick is 41.67 ns and a pass over four objects finishes inside one. Every sample
   read 0 or 1 ticks and the table said `0.0000 ns/item`. *Tell:* a suspiciously round number,
   especially zero. *Fix:* repeat the pass inside the timed region until a sample is ~100 µs.
2. **The compiler deleted the loop.** Repeating a pure function of nothing lets it be computed
   once. The tree arm read 0.166 ns/item — 2.3 cycles for four matrix builds of nine multiplies
   and sixteen stores each. *Tell:* a number that is not physically possible. *Fix:* make each
   pass genuinely different work — a per-pass bias added **per element**, so the running sum's
   rounding depends on it and FP non-associativity forbids hoisting.
3. **The accumulator is the bottleneck.** `hits += 1.0` is a serial chain of ~4-cycle double
   additions against a body of three multiplies. Both arms were latency-bound and the layout
   difference was completely masked. *Tell:* two arms that should differ, agreeing exactly.
   *Fix:* an `int` counter.

### Sanity-check every published number against the hardware

ns → cycles → instructions, and instructions are countable. All three lies above were found by
asking "could the machine physically do that", not by suspecting the code. No tool finds this one.

### An effect that survives the removal of its explanation had a different explanation

SoA beat a flat array by 0.66× on a full-transform loop, identically at 384 bytes and at 9.6 MB.
The cache story fits perfectly and is wrong: rebuild with `-fno-vectorize -fno-slp-vectorize` and
the win is **1.00× at every size**. It was auto-vectorisation — a contiguous `mat3` array
vectorises, a 96-byte stride does not. The *same* header's cull workload (12 of 96 bytes) keeps a
0.36× win with the vectoriser off, and only above the knee: that one really is the cache.

The general move: find the knob that disables your hypothesised mechanism and see whether the
effect goes with it.

### The real rule is not "use SoA"

Split the data a loop does not read away from the data it does, and **the size of the prize is the
fraction you leave behind**. 60 of 96 bytes buys nothing from the memory system; 12 of 96 buys 5×.

### An array of pointers is not a linked list

Both are "pointer chasing" and they differ by 3×. An array's addresses are known in advance, so a
core issues a dozen dependent loads before any returns and the misses **overlap** — 2.38× at
100,000 objects. A linked list stores each address inside the previous node, so the misses are a
serial dependency chain and **add** — 6.47×. That, not the hierarchy and not the OOP, is what
makes a decayed scene tree slow.

### A virtual call costs inlining, not prediction

1.5–1.7× on this workload, and *monomorphic and polymorphic agree within 4% at every size* — so it
is not branch misprediction. It is also flat across N, including four objects inside L1 — so it is
not the vtable load. What is left is that the compiler cannot see through the call.

### Dead-code elimination is not symmetric across arms of different inlinability

The accumulator read 5 of a matrix's 16 entries. `parent_from_local` is inlined into the flat,
pointer, SoA and tree arms — so the compiler could skip computing the other 11 — and it **cannot**
see through the virtual call. The virtual arm built whole matrices while its rivals built a third
of one. "The cost of `virtual`" read **3.4×**; reading all 16 entries it reads **1.7×**.

### You cannot make the allocator fragment, so measure what it did

A benchmark placed spacers between its heap objects so that "pointer chasing" would not quietly
measure a contiguous walk. 24-byte spacers between 96-byte objects went into a different size class
and separated nothing — **497 of 499 consecutive objects exactly `sizeof(T)` apart**. Matching the
sizes moved a headline from 1.09× to 1.98×, which looked like the fix.

It is not a fix. The *same layout code* gives different answers in different processes:

| process | n | consecutive objects `sizeof(T)` apart |
|---|---|---|
| the benchmark, `-O2` | 4 … 100,000 | **0** at every size |
| the benchmark, `-fno-vectorize` | 100,000 | 99,411 of 99,999 |
| a standalone probe | 100,000 | 0 of 99,999 |
| a standalone probe | 500 | 485 of 499 |

macOS's allocator interleaves same-size blocks, or does not, depending on that size class's
magazine state — which depends on everything the process allocated earlier. **Adjacency is a
property of the process, not of the layout**, no separate harness can assert another program's heap
state, and matching the spacer size only changes the odds.

So the resolution is a protocol, not a fix: the benchmark **reports its own allocation layout at
every size**, the driver flags any build whose objects came out contiguous, and the test asserts
only what the code controls (that the spacers were allocated) and prints the rest.
**A number whose provenance you cannot state is a number you should not publish.**

### Alternating two arms introduces cache interference

Alternation is the fix for thermal drift (3.10's pitfall) and it has its own cost: at large N the
arms evict each other, and the *same* flat arm read 0.98 ns/item in one pairing and 1.83 in
another. Quote **ratios within a pairing**, never an absolute from one pairing against an absolute
from another.

### Do not assert a property of the data and call it a property of the design

A check asserted "at least one reordering arm is not bit-identical". It failed: at n = 2,000 all
three reorderings happened to land on the same bits. Whether a reordering differs is a property of
the numbers. Replaced with `(a+b)-a != (a-a)+b` at 1e16, which is true regardless of the data.

## ECS storage and measurement facts (Lesson 5.7)

### A design can be measured before it is built, when the candidates differ in an access pattern

Archetype and sparse set differ in exactly **two** operations — reading K components of every
matching entity, and adding or removing one component from one entity. Everything else about them
(entity creation, single-component get, registry, scheduling) is either identical in shape or
orthogonal. So both were simulated in ~300 lines with **no entity manager, no component registry,
no type erasure, no views and no scheduler**, and timed against each other. That is worth
generalising: when an architecture argument reduces to "which of these two access patterns is
cheaper", you can answer it in an afternoon instead of building both and then defending the sunk
cost.

The corollary is a duty: **say what the simulation gives away.** The probe's archetype has
statically typed columns, so it pays no per-archetype column lookup and no type-erased stride the
compiler cannot see. Every archetype number is therefore an *upper bound* on a real archetype's,
and the lesson has to state that rather than quietly benefit from it.

### The sparse-set redirect is latency, and latency hides behind work

`data[sparse[e]]` is a two-deep dependency chain, which is the shape 5.6 measured at 6.47× for a
linked list. It is not the same cost, because the chains of *different entities* are independent
and the entity ids that start them come off a dense array sixteen to a cache line — 5.6's
memory-level parallelism, so the misses overlap.

With the vectoriser off, so codegen is held still, the redirect on a real body (build a model
matrix per entity) costs:

| entities | 4 | 100 | 1,000 | 10,000 | 100,000 |
|---|---|---|---|---|---|
| pools aligned | 0.99× | 0.99× | 0.98× | 1.02× | 1.02× |
| pools scrambled | 1.00× | 1.00× | 1.01× | 1.21× | **1.39×** |

**1.00× up to a thousand entities on the worst-case world.** Two conditions must hold together
before it costs anything: the working set must exceed cache (so the latency to hide is ~200 cycles
rather than ~4) *and* the pools' dense orders must have diverged (so the prefetcher cannot have
fetched the line already). Either alone is free. On a cheap body with nothing to hide behind, the
same code pays up to **2.79×** — so the amount of arithmetic a system does per entity is a
first-class parameter of any such comparison, and quoting one number for "sparse set overhead"
without stating it is meaningless.

### A cost model in bytes under-predicts when the bytes live in separate allocations

An archetype move copies an entity's components out of one chunk and into another and re-packs the
source. Counting bytes: 92 B travelling + 92 B re-packed ≈ 200 B for a 4-component entity, ≈ 456 B
for a 12-component one, predicting **2.3×**. Measured at 100,000 entities: 13.10 ns → 58.48 ns,
which is **4.5×**.

The missing term is that **a column is a separate allocation**. Eight extra components are eight
more vectors in eight unrelated places, each touched *twice* per move (the entity's row, and the
chunk's last row moved into the hole). 45.4 ns for sixteen additional independent touches is
~2.8 ns each, which is the right order for a partially-overlapped memory access. The model to
carry forward is **independent memory streams touched**, not bytes moved.

Meanwhile the sparse set went 4.25 → 4.23 ns across the same widening, because an insert is three
writes into one pool and has no way to discover what else the entity owns.

### A group is an archetype you can add later — and that asymmetry decides the design

A pool's dense order is nobody else's business: no id outside the pool depends on where an element
sits. So two pools can be **sorted into a common order**, putting the entities that have both
components at the front of both dense arrays in the same sequence. Index *i* of one then means the
same entity as index *i* of the other, the query reads **no sparse entry at all**, and what it
walks is byte-identical to an archetype chunk. Measured at **0.99×–1.01×** of a real archetype, on
the archetype's own best case (a query matching one entity in four, where the ungrouped sparse set
pays 1.46×–1.94×).

EnTT calls this a group; it is a maintenance obligation on top of a storage design, not a
different one. **The migration only runs one way** — a sparse set can be given an archetype's
query later, incrementally, guided by a profile; an archetype cannot be given O(1) structural
change at any price, because moving between archetypes *is* what an archetype is. When two designs
are close on the numbers, prefer the one that can become the other.

### Convenience is admissible as a tiebreaker and inadmissible as evidence

`engine::pool<T>` (5.4) already *is* a sparse set: `slots_` sparse and stable, `items_` dense and
packed, `owners_` the way back, plus generations. Noticing that is worth real credit and it is not
an argument — adopting sparse sets *because* we own one is choosing an architecture by an accident
of what meshes needed two lessons earlier. Measure first; reach for the convenience only after the
numbers have decided. (What `pool<T>` actually lacks is one **shared** id space: each pool mints
its own slot indices, so a handle from one means nothing to another.)

### Measure the argument against your own position too

The standard case against archetypes is fragmentation. Measured — the same entities split across
up to 4,096 chunks — it is **1.12× at worst**, at nineteen entities per chunk. Splitting a
contiguous array into 4,096 contiguous arrays does not stop it being contiguous. The lesson
publishes that even though it argues for sparse sets, and separately marks what it did *not*
measure (query matching over thousands of archetypes, per-archetype column lookup, allocator
pressure) so the reader knows which part of the claim is evidence and which is silence.

### The bug that needs N tries: write the index map after the re-pack

`archetype_churn::add_material` recorded where the entity landed in the destination chunk, then
called `pop_row` on the source. `pop_row` moves the source's last row into the hole and patches
**that** entity's row index — and when the moved entity was itself the last row, the stale index
overwrites the fresh one. Silent, no crash, one entity corrupted.

A single-frame test passes forever. It was caught because `verify_57` §E churns **200 frames** and
then walks the entire row map. **For a 1-in-N bug, the test has to run N times** — and the cheap
way to get that is to repeat the operation rather than to enumerate the cases.

## Course-infrastructure facts (docs/, Lesson 5.7)

### A colour pattern carried by fill alone does not read

`figs_*.py`'s `box()` emits `fill="{colour}" stroke="{colour}"` plus `fill-opacity`, so the
**stroke is always full opacity**. Figure 6 drew 32 small elements at fill-opacity 0.26 and 0.07 to
show "one element in four is wanted" and rendered as 32 identical amber outlines — the whole point
of the row, invisible. When a diagram distinguishes elements by weight, mute the stroke as well:
emit the rect directly with `stroke-opacity` (and a grey stroke for the de-emphasised ones).

### `eval(check-page.js)` returns a Promise

`const r = eval(src); return { pass: r.pass }` yields `undefined` for every field, and the MCP
result serializer **drops undefined keys** — so the output looks like a small, clean result rather
than an error. Either `await eval(src)` or `return eval(src)` directly and let Playwright await it.

### `figure.dia:nth-of-type(N)` counts every `<figure>`, listings included

Code listings are `<figure class="listing">`, so `nth-of-type` numbers them alongside diagrams and
asking for figure 6 hands you figure 4. Use Playwright's `figure.dia >> nth=N`, which indexes the
matched set rather than the sibling type.

### Two adjacent SVG text runs with no space between them — the third occurrence

5.5 fig 6, now 5.7 fig 3: `"AND THE COST IS PER COLUMN."` at x=24 ends at ~x=176 in the 9.5 px
face, and the following run started at x=178. No check looks for it — `svgTextOverlap` needs >2 px
of actual overlap — so only reading the rendered figure finds it. Leave ≥ 12 px between the end of
one run and the start of the next when they share a line.

### An annotation that contradicts its own caption

Figure 3 drew a dashed box around the material pool — the one thing that *is* in the picture —
under a caption whose point was the pools that are *not*. It passed every automated check because
it is geometrically fine. Read each figure against its own caption once, out loud.

## Course-infrastructure facts (docs/, STATE-block consolidation)

### A rule written before its replacement existed does not retire itself

CLAUDE.md §9 said "end every lesson with a fenced `STATE` block" because, when it was written,
there was nowhere else to put one. `STATE.md` arrived five lessons later (`cba92f1`, with 0.6) and
took over the resume-key job — but §9 was never amended, so **both** ran for another 46 lessons.
By 5.7 the in-page copies were **3.97 MB, 22.2% of `docs/lessons/`**, and 5.7's own block was
331 KB of a 550 KB page (60%) and byte-identical to `STATE.md`'s, 4,433 lines each.

This is the *second* time this exact shape has bitten `docs/` — the shared-CSS extraction was the
first, at 18% duplication. The tell is identical both times: a cost that grows monotonically with
lesson count while nobody re-reads the rule that causes it. **When a new artifact takes over an
old rule's job, amend the rule in the same breath.** Three places still demanded the block (§6
item 13, §9, §11's pre-flight checklist), plus `docs/_template/README.md`'s authoring checklist.

### "Only the newest copy is accurate" means every other copy is a bug

The standing rule was already *"only the newest lesson's STATE block tracks reality"* — an
explicit admission that 51 of 52 pages carried a knowingly-stale snapshot. A duplicate that is
documented as unreliable is not documentation, it is 4 MB of it. Worth asking of any per-page
copy: if it were wrong, would anything catch it? Here nothing would, because nothing read it.

### Invisible duplication still costs at write time

The blocks were inside `<details>`, collapsed, muted — a reader never saw one, so this never
showed up as a rendering complaint. The cost was entirely on the authoring side: every lesson
regenerated 4,400 lines that had to stay consistent with a file that already held them.
**Absence of a reader-facing symptom is not evidence that duplication is free.**

### Deleting a rule means deleting its scaffolding too

The block was gone but four things still referenced it: three `.state` rules in `course.css`, the
template's `SECTION 12 — STATE BLOCK` comment banner, and two checklist lines. One more was
subtler — `course.css`'s rationale comment for `pre { overflow-x: auto; }` cited "`.listing pre`
and `.state pre` restate this", naming a selector that no longer existed. **Grep for the class
name, not just the markup**, and read the comments the grep hits.

### Verify a mass deletion by proving it deleted nothing else

`git diff --numstat` over 53 files gave **54,907 deletions and 0 insertions**. That single number
is a stronger guarantee than reading any diff: a regex that over-matched, ate a `<nav>`, or
re-indented a line would have shown up as insertions. Pair it with a tag-balance parse across
every touched file and `apply-shared.py --check` (which re-verifies all 57 pages' relative
`../shared/` prefixes), and the change is proven without opening a browser — though a real
Chromium pass over HTTP still confirmed CSS applies and each page now ends Further Reading → nav.

---

## ECS runtime facts (Lesson 5.8)

### The one thing a slot map lacks is a shared id space, and it is structural

`engine::pool<T>` from Lesson 5.4 is a sparse set in every respect — `slots_` sparse and stable,
`items_` dense and mobile, `owners_` the way back, generations, O(1) everything. What it cannot do
is answer "does the thing in mesh slot 7 also have a texture?", and the reason is not that the
answer is slow: **each pool mints its own keys off its own free list, so the question is not well
formed.** An ECS is the arrangement in which it is. Mint the id once and hand it to every pool;
everything else — views, systems, composition — is a consequence of that single inversion. Recorded
because it is easy to look at 5.4's three arrays and conclude the ECS is already written.

### Reuse the bit layout, not the type

`entity` uses `core/handle.hpp`'s constants (20 index / 12 generation, generation 0 reserved, bump
on removal) so the bit budget has exactly one home — and is still a **different type**, because
`handle<mesh_data>` names an item *in* one container while an entity names a row *across* every
pool there is. Aliasing them compiles, and then `pool<T>::get(handle<T>)` and a component lookup
are the same spelling for opposite operations. The phantom parameter exists to stop ids mixing;
spending it to make them mix is an odd use of it.

### Store the whole id in the dense array, or the third test cannot be written

`contains()` is three tests: index in range, sparse entry not the "absent" sentinel, and
`dense_[at] == e`. Only the third rejects a **stale or recycled** id, and it is writable only
because `dense_` holds the full 32-bit entity word rather than a bare index. Storing an index saves
nothing and reinstates Lesson 5.4's *aliasing* failure — a probable crash converted into a
guaranteed silent wrong answer.

### The sparse patch is one line and its absence has no local symptom

```cpp
if (at != last)
{
    dense_[at] = dense_[last];
    data_[at]  = std::move(data_[last]);
    sparse_[dense_[at].index()] = at;   // <- this one
}
```

The entity swapped into the hole did not ask to move, and its sparse entry still names its old
position. Leave the patch out and the pool is not obviously broken but **silently** broken: one
entity reads another's data, an arbitrary number of frames later, in a different system. The
invariant that catches the whole class in one line is `sparse_[dense_[i].index()] == i` for every
`i`, asserted after a few hundred frames of churn — not after one.

### The last-element case is saved by the order, not by the branch

When `at == last` the entity that "moves" *is* the entity being erased. If the swap ran anyway it
would copy the element onto itself and patch its sparse entry to the position it already occupies —
immediately before the final `sparse_[e] = k_none` overwrites it. Harmless either way. **A test
that only ever erases from the middle passes forever**, so the last-element erase needs its own
check.

### Type erasure without RTTI: the cast is justified by construction

`component_id_of<T>()` is a monotonic counter behind a function-local static; the id indexes
`vector<unique_ptr<pool_base>>`, and `static_cast<pool<T>*>` recovers the type. That is safe
because **the id is what created the pool** — the object at that slot was constructed as a
`pool<T>` and nothing else can ever be stored there. This is the difference between a cast that is
safe and a cast that is merely usually right, and it is why `dynamic_cast` is not merely
unavailable but unnecessary.

Two consequences worth writing down. The ids are **global to the program**, not per registry, so a
registry using only the tenth type ever registered allocates eleven slots, ten of them null —
about eighty wasted bytes, in exchange for an array index instead of a hash. And the magic-static
initialisation is thread-safe while the **counter increment is not**: touch every component type
once before any job system starts.

### A type-erased base is fine as long as every virtual on it is cold

Lesson 5.6 measured virtual dispatch on an iteration at 1.5–1.7× *at every world size*, four
objects included, so "no virtual on the ECS hot path" is a constraint rather than a preference.
`pool_base` keeps it by having only cold virtuals: `erase` (once per pool per entity destruction),
`clear` (teardown), `size`, and `entities()` (once per **view**, never per entity). The hot path
holds concrete `pool<T>*`; `pool<T>` is `final` so even the cold calls devirtualise. **Design the
base around what may be cold, rather than adding virtuals and hoping.**

### One type-independent accessor removes all the metaprogramming

`entities()` returns `std::span<const entity>` for *every* `T`. That single signature is what turns
"lead with the smallest pool" into five lines:

```cpp
const std::span<const entity> spans[] = {pools->entities()...};
for (std::size_t i = 1; i < sizeof...(Ts); ++i)
    if (spans[i].size() < spans[lead_].size()) { lead_ = i; }
```

A pack of differently-typed pointers becomes an ordinary array, and the choice is a `for` loop.
**When a variadic problem looks like it needs dispatch, look for the projection that erases the
type first** — the type-dependent part (fetching components) can stay in the pack expansion where
it belongs.

### The lead pool pays twice

Fewer candidates is the advertised win — over pools of 60 / 30 / 12 the walk considers twelve
rather than sixty, six rejections instead of fifty-four, for the same six answers. The unadvertised
one: for the lead pool the dense position **is the loop counter**, so its own component needs no
sparse read at all. Only the other *K*−1 pools pay the redirect. `I == lead_` compares a
compile-time constant against a value fixed for the whole loop, so the branch predicts perfectly
after the first iteration.

### A rule nothing can observe quietly stops being true

Every correctness test of a view passes just as well if the view walks the *biggest* pool — the
answer is identical, only slower. So `view` exposes `lead()` and `size_hint()` purely so the
harness can assert that a 60/30/12 view leads with the 12. **When a decision is about behaviour
rather than output, give it an observable and test the observable.**

### Do not cache the span you are about to iterate

The view re-reads `lead_pool_->entities()` at the top of every walk instead of caching it at
construction. A cached span dangles the moment anything inserts into the lead pool between building
the view and walking it — a bug that reproduces only when a vector happens to reallocate, which is
the worst possible schedule for finding it. One virtual call per walk (not per entity) buys the
whole class away.

### Two containers may share a name if the namespace explains the relationship

`engine::pool<T>` and `engine::ecs::pool<T>` are the same three arrays doing opposite jobs: one
mints its own keys, the other is keyed by an id it did not mint. Renaming the second to
`component_storage` would have hidden a relationship the course spends two lessons drawing out. The
nesting keeps them apart, both header comments state the difference, and the one arrangement that
breaks — `using namespace engine;` plus `using namespace engine::ecs;` — produces a compiler
ambiguity, which is the good kind of breakage.

### Two independent defences mean a single targeted test proves nothing

A recycled entity does not inherit its predecessor's components for **two** reasons:
`registry::destroy` erases the rows, *and* the generation moved so `dense_[at] == e` fails. Either
alone would prevent the symptom, so a test aimed at the symptom passes with one of them broken.
That is why the harness runs 200 frames of churn against an independent `std::map` shadow model
that shares no code with the thing under test, and then walks every invariant.

### The free list's real payoff is the size of every sparse array

Because a freed slot is reused before a new one is minted, the id space's high-water mark is **peak
simultaneously live entities**, not entities ever created — measured at **220 slots after creating
518 entities** over 200 frames of churn. Every component pool's sparse array is sized by that same
number, which is the entire reason paged sparse arrays can wait. The failure mode is the one 5.4
already named: high-water marks do not come back down, so **things that exist in millions and die
in seconds should not be entities.**

---

## Course-infrastructure facts (docs/, Lesson 5.8)

### Figure numbers follow page order — for the third time

`figs_58.py` authored the type-erasure figure before the view figure, so `l58_fig5.svg` was the
erasure one while §3.4 (the view) came first on the page. The reader saw "Figure 6" above
"Figure 5". Caught by asking the DOM for `.fignum` and `<title>` together rather than by reading
the source. **Number figures by where they land, name the file to match, and put the section in a
comment** — this is the same trap as 5.3 and 5.6.

### `browser_navigate` to a URL you just rebuilt can serve the old page

A figure fix looked like it had not applied: the check reported `unique_ptr<pool_base>` overflowing
its box when the SVG on disk had already been split into two lines. The page was cached. `grep` the
built file first, then force `location.reload(true)` (and `fetch(..., {cache:'no-store'})` for the
checker itself) before believing a post-rebuild verification result.

### A label can overflow its own box without spilling the viewBox

`check-page.js` catches labels outside the SVG, labels on top of each other, and labels on strokes
— none of which sees a 21-character monospace run in a 108-unit box. `unique_ptr<pool_base>` at
9.5px is about 120px wide; it rendered across the box edges in every one of five columns. A
throwaway check (find the `<rect>` whose bounds contain the text's vertical centre, compare
horizontal extents) found all five at once and is worth keeping in the authoring pass.

### A diagram that carries a derived value must be re-derived, not pattern-matched

Figure 3's third panel showed the free list as `[1]` after `create()` had popped slot 1 — because
the code derived the display from "is this panel highlighted?" rather than from what the free list
actually holds. The panels were right about generations and wrong about the list, in a figure whose
whole subject is reuse. **Spell out per-panel state explicitly instead of deriving it from a
rendering flag.**

### Count what you claim, including in the figure's own text

The lesson said "five component types" in six places — the deck, the milestone, a figure caption,
the figure's `<title>`, the figure's on-canvas heading, and the index card — while the demo has six
(`spin` was omitted). Only building the demo and reading its own printed totals caught it. Two
sub-lessons: the SVG's accessible `<title>`/`<desc>` are prose too and drift with the rest, and a
program that prints its own arithmetic (`121 entities, 468 components, 5 pools, 97 drawn`) turns a
claim into something checkable — 4 + 384 + 32 + 48 = 468 can be verified by hand.

### `pool_count()` is 5 while the demo has 6 component types, and that is the feature

A component type nobody has attached has **no pool**, because every read path goes through
`storage_if<T>()` rather than `storage<T>()`. Nothing has a `lifetime` until `[Space]` spawns a
spark, so the at-rest count is five. Worth stating in the lesson rather than quietly reconciling:
if `has<T>()` created a pool as a side effect, asking a question would be a mutation and a `const`
registry would be impossible.

### A retired build script keeps stamping what the rule retired

`build_57.py` still had the `<details class="state">` block in its `TAIL` and still read
`l57_state.txt`, months after the STATE-block consolidation stripped that markup from all 53 pages.
Re-running it to repoint one navigation link would have re-added a 331 KB block. Fixed by editing
the builder *and* proving it: rebuilding 5.7 to a temp file and `diff`-ing it against the shipped
page, which also caught a one-line whitespace drift left by the original strip. **When a builder is
the source of a page, "the page is correct" is not the same claim as "the builder is correct".**

---

## Transform hierarchy facts (Lesson 5.9)

### Name the function for the day it will be needed, and that day costs nothing

`math/transform.hpp` has called its composition function `parent_from_local` since Lesson 2.8,
with a comment explaining that in Module 2 an object's parent *is* the world and that Module 5
would widen the meaning. Adding a hierarchy seven lessons later changed **not one character** of
that file, or of `transform`. The general form: when you know a concept will generalise, name it
for the general case and document why it currently looks like the special one. The alternative —
`world_from_local` — would have been a lie that had to be gone back and corrected in every
caller.

### The maths of a scene graph is one line; the ordering constraint is the whole problem

`world(child) = world(parent) × parent_from_local(child)` reads the parent's *world* matrix, so
the parent must be finished first. That is a constraint on the **graph**, and a component pool's
dense order is insertion order — the two are in direct tension, and reconciling them is what the
lesson costs. Anyone who says a hierarchy is "just a matrix multiply" has not said where the
multiply happens in the loop.

### Depth is a topological sort, and that is the cheapest one available

A parent's depth is always exactly one less than its child's, so **sorting by depth puts every
parent before every child** — and a counting sort by a small integer key is O(*n*). Two things
follow, and the second is the one worth having: the resolve loop needs no visited set and no "has
my parent been done yet" test, because the order guarantees what those would check; and **within
a level nothing depends on anything else in it**, so a level is a `parallel_for`. That parallelism
was not designed. It arrived with the choice of order.

### Recursion's locality advantage loses to level order's write pattern

A depth-first walk has the best temporal locality available — the parent's matrix was written one
call ago and is certainly in L1 — and it still loses, because its *writes* jump. Measured at
100,000 entities with only the shape varying: recursion runs 3.34 → 13.55 ns/entity from depth 1
to 32, level order 3.39 → 4.94. **Recursion is depth-dependent; level order is very nearly not**,
and it saturates around depth 8. The lesson generalises: when two orders trade read locality
against write locality, measure, because the intuition points the wrong way as often as not.

### A control row that costs nothing is worth more than an extra data point

The depth-1 row — no hierarchy at all, both arms doing identical work, ratio 1.01× — is what says
the harness measures the tree rather than itself. Had it come out at 0.6× or 1.4×, nothing else in
the table would have meant anything. **Every ratio table wants a row where the ratio must be 1.**

### Split the operation that runs on change from the one that runs every frame

`rebuild()` (produce the level order) costs about **one resolve**, measured at 0.93–1.39. A game
re-parents rarely and moves things constantly, so rebuilding only when the shape changes is worth
more than any choice of visit order. This is the same shape as a dirty flag, applied at the level
where the ratio of "changes" to "frames" is genuinely enormous rather than merely favourable.

### The amplification is what makes dirty flags stop paying

Moving 10% of a depth-8 tree dirties **36%** of it, because every descendant of a moved entity has
to move too; 25% moved reaches two thirds of the world. So the crossover sits at about a quarter
moved, and past it the "optimisation" is *slower* — 1.29× when everything moves, which is exactly
what an animated scene does. **Quote the reach, not the fraction you marked.** A partial-update
scheme's cost is set by its closure, not by its input.

### Publish what the container costs on top of the algorithm

The probe measured a visit order over three plain arrays; the shipped resolver walks the same
order through `registry` and `pool<T>` and costs **1.22–1.40×** more — three sparse lookups per
entity instead of three array reads. Quoting the probe's number for shipped code would be quoting
a measurement of something else. Measure both, publish both, and the gap tells you whether a
caching exercise is worth attempting.

### A hang is a different class of failure from a wrong picture

A parent cycle does not produce a bad frame; it produces no frame. So it gets two independent
defences, and the second exists because the first is not enough: `set_parent()` refuses to create
one (walking the *whole* chain — a one-link check misses a grandchild), and `rebuild()` survives
one written directly into the component, because `parent` is public data and a scene file can
carry anything. Break it, count it, log it, and keep resolving everything else — **one bad link
must not cost the frame.**

### Choose an orphan policy, state it, and give the caller the other one

A child whose parent died becomes a root and is *counted*. Destroying the subtree is a policy a
game may want and a transform system must not impose — silently deleting entities during a resolve
pass is a surprise nobody asked for. So the engine ships `destroy_subtree()` as an explicit,
named alternative. The general rule: when two behaviours are both defensible, the *less
destructive* one is the silent default and the other one gets a function whose name says it.

### Scale composes down the chain, and it is the one that surprises people

A child under a parent of scale 0.16 is 0.16 of its authored size; under two such parents, 0.0256.
The demo's moons are deliberately built this way, with the arithmetic in a comment, because the
first time a "1-metre" prop turns out to be 20 cm everybody assumes a bug. It is not a bug in the
composition — it is what "relative to my parent" means, applied to size.

### `transform` can carry any affine matrix exactly, and that is a bridge rather than a design

`parent_from_local` builds `affine(columns scaled by scale, position)` and `transform::rotation`
is a general `mat3` with no orthonormality requirement, so
`{position = translation_of(m), rotation = linear_of(m), scale = 1}` reproduces **any** affine
matrix bit for bit. That let the demo feed composed world matrices into a renderer that wants a
`transform`, with no engine change and therefore no risk to the golden. It works because a field
named `rotation` is typed as something more general than a rotation — which is exactly why it is
a temporary bridge and is labelled as one.

---

## Measurement and tooling facts (Lesson 5.9)

### A `double` accumulator over `float` data makes order-dependence disappear

Lesson 5.6 established that two arms visiting the same elements in different orders sum them
differently, so `bench_ab::agree` should be false. In 5.9 every arm reported `agree`, exactly, and
the reason is the widening: each addend is a `float` (24-bit mantissa) going into a `double` (53),
and a running sum of 10⁵ values of magnitude ~1 never exceeds 2¹⁷ — so every partial sum needs at
most 17 + 24 = **41 significant bits and nothing is ever rounded**. An exact sum is
order-independent. The knob that proves it: change the accumulator to `float` and the agreement
vanishes (49928.1484 forwards, 49928.2734 backwards). **When a rule you trust does not fire, find
out why before assuming the test is broken.**

### `DIFFER` can be the correct outcome, and the harness has to say which kind it is

The dirty arm legitimately disagrees with the full pass's checksum, because it touched fewer rows
and therefore summed fewer of them. Its claim is not "same checksum" but "same *matrices*", and
that is checked element-by-element in the verification harness with no clock running. **Put the
exactness claim where exactness is affordable**, and make the benchmark say which of its rows are
expected to differ.

### Swap-and-pop makes some scrambles impossible, and a test can pass because its situation never arose

Producing a pool whose dense order puts children before parents took three attempts. Destroying
leaves scrambled nothing — swap-and-pop fills a hole with the **last** element, and in a tree built
level by level the last elements are leaves, so a leaf replaced a leaf. Destroying fillers created
at the *end* failed identically. What works is destroying something **early** while deep entities
sit at the end. Twice in a row the test passed for a reason that had nothing to do with the code
under test, which is the failure mode a test of an *ordering* property is most prone to. The
assertion is now on a count greater than zero, not on the answer being right.

### A rule nothing observes is a rule that quietly stops being true — again

Every correctness test of the resolver passes just as well if it walks in the wrong order and
happens to get lucky, because the *answer* is the same whatever order you compute it in. So
`hierarchy` exposes `level(i)` and `depth_of()` purely so the harness can assert that every
entity's parent appeared in a strictly earlier level. Same lesson as 5.8's `view::lead()`, arriving
one lesson later on a different property.

### A builder is not frozen just because its page is shipped

Re-running `build_58.py` to repoint one navigation link spliced Lesson **5.9's** demo into Lesson
5.8's listings, because listings are read from the live repository. The page then carried code
referencing `engine::ecs::hierarchy`, which does not exist at 5.8's point in the course. This is
the second time an old builder has misbehaved when re-run (5.7's was still stamping a retired
STATE block).

**The fix is a pinned snapshot**: any file a later lesson modifies is frozen into
`scratch/lNN_<name>.cpp` and the builder reads that instead. And the snapshot is *verified* rather
than trusted — `git show <commit>:<path> | diff - <snapshot>` proves it byte-identical to what the
lesson shipped. The reconstruction attempted before the commit existed was one comment off, which
is exactly the error rate you should expect from reconstructing 600 lines from memory.

### A label can overflow its own box, and none of the geometry checks can see it

`check-page.js` catches labels outside the viewBox, labels overlapping each other, and labels
sitting on strokes. A 34-character run in a 132-unit box is inside the viewBox, on top of nothing,
and overlapping no other label — it just renders straight through both edges. The throwaway check
(find the `<rect>` whose vertical span contains the text's centre, compare horizontal extents)
found it, plus a full-width prose line running through a dashed box beneath it. Worth keeping in
the authoring pass permanently.

### SVG `<text>` collapses leading whitespace, so indented code in a diagram is not indented

Three lines of pseudo-code written with leading spaces rendered flush left, destroying the nesting
that was the reason to show a loop at all. Indent with explicit `x` offsets. (`xml:space="preserve"`
also works and brings its own surprises with trailing whitespace.)

### Put a chart's annotation where the data is not

The "break even" label on the dirty-flag chart was right-anchored at the plot's right edge —
directly on top of the 1.29× bar it was there to explain. The right-hand bars are the tall ones in
any chart whose series rises. Anchor annotations on the side the data has left empty.

---

## Input mapping facts (Lesson 5.10)

### Four reasons to add a layer, none of them performance

A hard-coded scancode cannot be rebound, is about one device, cannot be recorded (a replay wants
the *intention*, because the binding may have changed since), and **says the wrong thing** —
`key_pressed(SDL_SCANCODE_SPACE)` inside a jump handler is a sentence about hardware in a file
about jumping. Recorded because the fourth is the one that gets dismissed and is the one that
compounds: code that fails to describe itself gets harder to change every year.

### A design lesson should say it is a design lesson

Modules 5.6, 5.7 and 5.9 each turned on a benchmark because each was choosing between designs
that differed in *cost*. This one was not, and manufacturing a benchmark anyway would have been
worse than useless: it would attach a number to a decision the number did not make, and the next
reader would believe the number was the reason. **Say "this measures nothing, and here is why"
rather than measuring something irrelevant to look rigorous.**

### One signed float per binding covers buttons and axes

`jump` bound to Space with +1; `steer` bound to A with −1 and D with +1; `look_x` bound to a mouse
delta with 0.5. Same rule, three jobs. The payoff is the case nobody thinks about: **holding both
halves of an axis gives exactly zero, and no rule anywhere says so.** A design with separate
button and axis kinds would have needed to define, document and test that case.

The corollary is that the value must *not* be clamped. Two keys on one button action sum to 2,
which `held()` does not care about; a mouse flick should be big. Clamping would have to know which
kind of action it was looking at, and not knowing that is the whole point.

### An edge is a change in the action, not in a signal

Bind one action to a key *and* a mouse button. Press the key (one rising edge, correct). Press the
mouse while the key is held — a binding-derived edge fires **again**, and the player jumps twice.
Release the key while the mouse is down — a binding-derived edge fires a *release* while the
action is plainly still active.

Derive from the summed level and all four frames are right. **The bug is invisible with one
binding per action**, which is what a first implementation has and what a first test writes; it
ships as "sometimes it jumps twice" the week a player binds a controller alongside their keyboard.
The test that catches it is four lines of setup and nothing else in the harness would have.

### A fixed-timestep engine needs two kinds of edge

`on_fixed_step` runs zero or more times per frame, so a frame-scoped edge read there fails in
**both** directions: a two-step frame acts on it twice, and a zero-step frame loses it entirely
because the edge came and went while nothing was listening. Neither is fixable by the caller
without cross-frame state, so the map keeps it — every rising edge queues a press,
`consume_pressed()` pops one.

This is Lesson 1.4's trap *closed* rather than documented, and it is worth noticing the
difference. Documenting a hazard moves the cost to every future caller; closing it costs one
`uint8_t` per action.

### A bounded queue is a decision, not an overflow bug

Four deep, and presses past that are dropped. An unbounded queue turns a hitch into a burst of
jumps arriving after the player has stopped asking, which feels worse than losing them. Four
survives a stutter and is short enough not to feel like a recording; a fighting game with a
deliberate input buffer raises it and calls the number a design parameter, **which is exactly
what it is.** The engine's job is to make the number visible and the behaviour predictable, not
to guess the genre.

### "Once per frame" is not the whole requirement

`on_frame` runs exactly once per frame and is still the wrong place to update an input map,
because it runs *after* the simulation steps — so every step reads last frame's actions. The
requirement is **once per frame, after input is published, and before anything reads it**. That
gap had no hook, so 5.10 added `on_input()`.

The symptom of getting it wrong is worth knowing because it gets misattributed: everything works,
nothing is broken, and the game feels 16 ms mushy. People blame the display, the driver or the
mouse.

### A hook that serves one caller is a smell — check before adding it

`on_input` is defensible because the once-per-frame, pre-simulation moment is also where you read
a network snapshot, sample a debug scrubber, or latch a replay's recorded intentions. It was
missing before this lesson happened to need it. If the only answer to "what else is this for" had
been "nothing", the right move would have been to let the caller update the map at the top of its
own first fixed step and document the ordering.

### A concept when the test cannot otherwise exist

`input::update()` samples SDL's live keyboard state, so a harness that wants a key held would have
to persuade SDL that a key is held — which SDL offers no supported way to do. Templating
`action_map::update` on an `input_snapshot` concept (six accessors) makes a hand-written struct a
complete input device, and the shipping logic runs unchanged.

A concept beats an abstract base here on three counts, and all three are worth separating:
**it changes nothing in `input.hpp`** (not one character of Lesson 1.2's header moved);
**it costs no virtual call** on a loop that runs per binding per frame; and **it is open** —
`engine::input` satisfies it without knowing it exists, where a base class is implemented only by
types whose author agreed to. Put a `static_assert` beside the concept so a rename in the source
type fails at the requirement rather than deep inside a template.

### Do not ship device code you cannot run

The lesson's second motivation is "a gamepad has no scancodes", so the obvious move is to add
gamepad support — and it was declined, deliberately and out loud. It could not be exercised on
this machine, and **untested device code in an engine is a liability that looks like support**: it
compiles, it appears in the API, and the first person to plug in a controller discovers it was
never run.

What was shipped instead is the property the claim actually rests on — one action, several
bindings, more than one device — demonstrated with a keyboard and a mouse, plus the five steps and
an explicit ⚠ VERIFY on the SDL3 signatures. **A named gap beats an unverified feature.**

### The hardest part of a new device is disconnection, and it does not belong in the mapper

An action bound to a controller that has been unplugged must go inactive, and the release edge
must fire. The right place for that is the *snapshot*: a disconnected pad answers false, the sum
drops to zero, the level goes false, and the release edge falls out of the existing machinery with
no new code. **If you find yourself special-casing the mapper, the abstraction boundary is in the
wrong place.**

### A lookup that declares turns a typo into a silent no-op

`find()` returns an invalid id and does not create. If it declared on miss, a misspelled action
name in a config file would produce a brand-new action with no bindings and no code reading it —
a perfectly functioning thing that does nothing, with no error anywhere. Same shape as Lesson
5.5's "no fallback asset": substituting something plausible removes the program's ability to
notice.

### `reset()` has to forget the mouse origin too

Clearing levels, edges and the queue on focus loss is obvious. Forgetting the *previous cursor
position* is not, and without it the first frame back delivers the entire distance the cursor
travelled while the window was in the background — as a single camera-flinging delta. The same
applies at start-up, which is why the first frame ever reports a delta of zero.

### A HUD that reads the binding table cannot go stale

The demo prints `spawn on [Space]/[LMB]` by asking the map, not from a string literal. Press the
rebind key and it says `[Enter]`, because the binding really changed. Small, and it is the first
dividend the layer pays: **a program that can describe its own controls**, which a switch
statement could never do.

### Count the thing before writing the number down

The lesson said "Lesson 5.2's application layer offers six hooks". It offered seven. Caught by
grepping `virtual ` in `app.hpp` while updating ARCHITECTURE.md, not by re-reading the prose —
a number in a sentence is a claim, and claims about the codebase are checkable in one command.

---

## Debug drawing and tooling UI facts (Lesson 5.11)

### Two of six parameters were the whole bug

`line3(fb, a, b, colour, pr)` needs a `framebuffer` and a `projector`, so **only code holding
renderer state can call it** — and the code with something worth drawing (a collision system, an
ECS pass, a loader) has neither, and should not. Hand a framebuffer down so one of them can draw a
line and every caller of every caller needs one too. Three apparent problems, one root: it works on
one surface only, and a line lives exactly one frame, both fall out of the same two parameters.

**Removing an argument is a design change. It made nothing faster and it moved the ceiling.**

### The include list is the interface, and it is transitive

The queue could have gone into `debug_draw.hpp` and everything would compile. It would also mean
that a physics translation unit including it to draw one box compiles the framebuffer, the depth
buffer, the projector, the viewport and — via `mesh.hpp` — the whole handle system. Forever, in
every TU.

That constraint is why `wire_mesh()` takes `span<const vec3>` and `span<const uint16_t>` rather
than the `mesh` that owns them. **Take the data, not the type**, when the type's header costs more
than the data does.

### A single-frame marker for a single-frame event is invisible

16.7 ms at 60 Hz, against roughly 250 ms for a person to register that something appeared. So a
debug drawer without lifetimes **can show you state and never events** — and the events (a
contact, a re-parent, a spawn, a raycast hit) are usually what you are hunting. This is the
argument for lifetimes and it is the one that gets dismissed as a nicety.

### The expiry rule: test before you subtract

Measured in `float` at 60 Hz on a 0.5-second line, three rules that all look right:

| rule | advances | 0-second line at `dt == 0` |
|---|---|---|
| `if (r <= 0) drop; r -= dt;` — shipped | 31 | dropped |
| `r -= dt; if (r <= 0) drop;` | 30 | dropped |
| `r -= dt; if (r < 0) drop;` | 30 | **kept for ever** |

The third is the one you write the moment you want a line to survive the frame its clock runs out
on — a one-character change. And `dt == 0` is a paused clock, a single-frame `--shot`, a
breakpoint: **the moments you are looking hardest.** Testing first does not get the answer right,
it *removes the dependency* on `dt` and on the comparison operator, which is the more durable fix.

### Queue → flush → advance, and `advance()` outside every branch

Ageing before the flush deletes every default-lifetime line before it is drawn. The symptom is a
debug system that draws **nothing**, which reads as "my queueing code never ran" and sends you to
instrument the wrong file. Diagnose it in one step: queue something with `seconds = 5`; if that
appears and the default ones do not, it is the order.

Ageing *inside* the "we have a camera" branch is the same bug with a slower fuse — the queue grows
without limit on exactly the frames that draw nothing.

### A bound with no counter is a bug that looks like a rendering artifact

The 4,097th line simply is not there, and a missing line looks exactly like a thing that does not
exist. `queued`, `drawn` and `dropped` only mean anything as a set. Every primitive funnels
through one private `push()` so the bound has one implementation, which is also why a 12-edge box
with two free slots is 2 kept and 10 dropped rather than something bespoke.

### Report what you actually know: `line3` now returns whether it survived

The HUD claimed "drawn below queued means something clipped", and `draw_debug_lines` was counting
what it *submitted*. Giving `line3` a `bool` return made the claim true. It changes no pixel —
both new `return false` paths were already `return` and already drew nothing — and it is
deliberately not `[[nodiscard]]`, because six lessons of call sites correctly ignore it.

**A number in a HUD is a checkable claim. One the code cannot back is worse than no number.**

### Never withhold an event from a level tracker

The tempting fix for two consumers of one keyboard is to route: give the event to the UI, and if
it took it, stop. `engine::input` tracks **levels**, and a level is only ever corrected by the
event that contradicts it — so a key-*up* routed away leaves that key held down for ever, and no
later event fixes it, because the key is not going to be released twice.

Both consumers see every event. Arbitrate one layer later, on the levels.

### …and keep updating the map while the UI has focus

Skipping `action_map::update()` while a text field is focused freezes every level: hold a movement
key, click into the box, and the camera flies away. Updating through a mask reports the masked
keys as *up*, which fires the **release edge** — which is not damage limitation, it is the
behaviour you want. Giving focus to a text field genuinely should let go of the movement keys.

### You cannot mask a delta by masking one of its endpoints

A key is a level, so reporting it as up is a complete lie and it works. The cursor is not:
`action_map` *derives* a delta by differencing two frames. Cursor at 100, UI takes the mouse for
three frames while it travels to 160, then lets go as it moves to 170:

| approach | delta on the release frame |
|---|---|
| report 0 while blocked | **+170** — the whole screen |
| freeze the last position | **+70** — the whole excursion. *This is the version that ships.* |
| virtual cursor | **+10** — one frame of real movement |

The middle one is the trap, because it behaves perfectly until somebody drags a slider, and then
letting go whips the camera round by the distance they dragged.

The fix is a change of frame of reference, not a mask: report `real − offset`, and grow `offset`
by exactly this frame's real movement on every blocked frame. Reported position stops dead while
blocked, and the offset stops *growing* the instant the block lifts, so both boundaries are free.
The cursor stays permanently 60 px behind and is permanently right about how far it moved — which
is the only question anybody asked it.

The wheel needs none of this, and noticing why tells you when the machinery is needed: `input`
publishes the wheel as a per-frame **delta** already, so zeroing it is exact. Only a delta the
*consumer* derives needs the virtual-cursor treatment.

### A concept written for testability turned out to be an architecture

Lesson 5.10 made `action_map::update` a template over an `input_snapshot` concept so a test could
hand it a six-function fake. One lesson later the mask is the concept's second implementor, and
**`action_map` did not change by one character**. Against `const input&` the only options were an
`if` inside the map (the input mapper learning what a UI is) or a copy of `input` with fields
cleared (a second source of truth about the keyboard).

### `NewFrame` computes the capture flags, so it goes first

ImGui's `WantCaptureKeyboard` / `WantCaptureMouse` are computed *inside* `NewFrame`, from the
events processed since the last one. Put `NewFrame` next to the panels in `on_overlay` — where it
looks like it belongs — and every read during the frame answers about the **previous** frame. The
symptom is exactly one frame of leakage: the first keystroke after clicking into a text field also
reaches the game, every time, and it is invisible unless you know to look.

The three backend calls also have a fixed order — renderer, then platform, then core — because the
core derives from what the two backends just filled in.

### Concept versus vocabulary decides whether a dependency can be wrapped

`stb_image` is `PRIVATE` and hidden behind `engine::image` because it wraps a **concept**: decode
these bytes into pixels, one function and one type. Dear ImGui is `PUBLIC` because its value **is
its vocabulary** — four hundred widget calls — and a wrapper around that is a re-spelling with no
content that has to be re-spelt for every widget, forever, by somebody who did not write the
widget.

So the containment is a *rule about which code may speak it* (tooling only) rather than a link
flag, and exactly one engine translation unit includes `<imgui.h>`.

### Some libraries ship sources and no build, and that is a feature

ImGui has no `CMakeLists.txt`. `FetchContent_MakeAvailable` notices and only *populates*; the
target is yours to declare. Being forced to name the seven files you compile is a better position
than inheriting somebody's build options — and it is where you decide, consciously, to keep
`imgui_demo.cpp` (11,299 lines) because `ShowDemoWindow()` is the fastest widget reference there
is and its source is the documentation.

### A static library only contributes what somebody references

`verify_45` … `verify_510` still link without `libimgui.a`, even though the engine archive now
contains `debug_ui.o`. Only `verify_511` needs it, because only `verify_511` touches
`engine::debug_ui`. Worth knowing before you go adding libraries to every link line "just in
case".

### The headless path must be silent, safe, and *logged at info*

`debug_ui::start()` returns false with no window — which is `--shot`, and a build server, and both
are legitimate. Logging that at error level would put a red line in every headless run in the
repository and train the reader to ignore red lines. Same argument `engine_set_warnings` makes
about compiler warnings, one layer up.

Every subsequent call being a no-op — *including* `wants_keyboard()`, which answers false — is
what lets one program be written once and run correctly on all three surfaces.

---

## Course-infrastructure facts (docs/, Lesson 5.11)

### "Pin the demo" was the wrong rule; pin every file the page lists

`build_510.py` carried a warning to pin `demos/ecs_swarm/main.cpp` before 5.11 touched it. Done,
verified, and **still not enough**: re-running the builder produced a 173-line diff, because
`actions.hpp` is also one of 5.10's listings and 5.11 added `masked_input` to it. It read as
"finished" precisely because it was *5.10's own new header*.

The rule is: **pin every file the page lists that a later lesson touches** — and the cheap way to
find out which is to re-run the builder and `git diff` the page *before* shipping anything else.
Third occurrence of an old builder misbehaving (5.7's stamped a retired STATE block, 5.8's spliced
a newer demo, 5.10's needed two pins).

### `check-page.js` must be run at a stated viewport

Run in the Browser pane at its natural (narrow) size, the spill check reported **every label in
every figure** as spilling and `pageScrollsX` as true. Nothing was wrong: the SVGs are scaled and
the comparison degenerates. At 1280×900 and again at 390×844 the same page returns `pass: true`
with zero findings.

A checker that reports 170 failures for a page with none is worse than no checker, because the
next real finding is in that list somewhere. **Set the viewport explicitly before believing the
output.**

### Box-sampling destroys a line drawing; take the peak instead

`figs_45.box_sample` averages each cell, which is right for a shaded surface and catastrophic for
lines: a one-pixel debug line inside a 3×3 block contributes one ninth of its brightness, so 152
crisp radial lines averaged into a uniform haze and the figure showed scattered dots. Taking each
block's **brightest** pixel keeps thin bright features at full strength.

### …and the quantiser's background test is per-channel, not luminance

`figs_45.quantise` calls a cell background only when `r < 14 && g < 14 && b < 14`. This demo's
background is `(12, 14, 20)` — blue is 20 — so every empty cell snapped to the nearest tint at its
dimmest step and the whole panel came out solid slate with the lines invisible inside it. The fix
is to floor dim cells to true black in the sampler, so a cell says "nothing here" in the
vocabulary the quantiser already speaks.

Both bugs were found by *looking at the rendered figure*. No geometry check can see either.

### A label can overflow its own box, still

Third occurrence. Figure 3's one-frame bar is 92 units wide and carried "queued, drawn, gone" —
about 99 units of text, spilling out both ends. Inside the viewBox, on top of nothing, overlapping
nothing, so every automated check passes. Budget roughly **5.2 units per character** for `xs`
text and check in-bar labels against the bar.

### Calibrate the text-width estimate against pages that shipped

A pre-flight width check flagged fourteen labels using a guessed 5.3 units/character. Measuring
the *shipped* 5.10 figures — which were browser-verified — showed they pack 134 characters into
696 units, i.e. 5.19. Half the "failures" were the estimate, not the figures. **Calibrate against
known-good output before acting on a heuristic.**

### The reliable way to look at a figure is one page per figure

Scrolling a 400 KB lesson page and screenshotting lands somewhere unpredictable — `offsetTop` is
relative to the offset parent, and `course.css` sets `scroll-behavior: smooth`. Generating a
throwaway HTML page per SVG that links `course.css`, then `navigate` + `screenshot`, is
deterministic and shows the figure at its real size in both themes. Delete them afterwards.

---

## Colour pipeline facts (Lesson 6.1)

### The two integers

Code 128 emits **0.2159** of white's light, not half. Half the light is code **188**. Sixty codes
apart, and every mistake in this subject is a variation on that gap. If a 50% grey looks too dark
in a renderer, this is the first thing to check and it takes one line of arithmetic.

### The encoding is a budget, not a CRT artefact

The CRT story is true and useless — CRTs are gone and the encoding is not. Derive it from
perception instead: the eye judges **ratios**, so equal code steps should be equal ratios. Counted
over 256 codes:

| codes falling in… | evenly spaced in light | spaced by sRGB |
|---|---|---|
| the darkest tenth of the range | 26 | **90** |
| the brightest half of the range | 128 | **68** |

The even scheme spends half its budget where the eye can barely tell two neighbours apart. 8 bits
of *linear* light needs about **12 bits** to look as smooth as sRGB does in 8. Framed as
perceptual compression, the encoding stops being history and starts being a design.

### The toe is derived, not decreed

The derivative of x^(1/2.4) is (1/2.4)·x^(−0.583), which goes to **infinity** at zero. A pure
power curve therefore has unbounded gain at black: sensor noise becomes visible banding, the
inverse is numerically unstable, and an 8-bit code boundary is arbitrarily sensitive. Replacing
the bottom with a line of slope 12.92 fixes all three.

The four constants are not independent — 0.04045 = 12.92 × 0.0031308, and 1.055/0.055 are chosen
so the pieces **meet** (measured step at the join: 8.02 × 10⁻⁵, a fiftieth of a code).

`pow(x, 1/2.2)` is a **different curve**: worst disagreement **8 codes**, near linear 0.0010 —
down in the toe, exactly where the eye has the most codes to notice with. Fine as a stylistic
brightness knob; never as the output transfer function, which has to agree to the code with every
hardware sampler in the pipeline.

### Convert twice, at the edges — and the difference is structural

Per-operation conversion (which is what Lesson 1.6 shipped, and said so) is not merely slower. It
makes every *new* operation a chance to forget, and it quantises to 8 bits after every step.
Converting at the edges makes the middle a region where ordinary arithmetic is valid **because
there is nothing else there** — a property of the pipeline's shape rather than of anyone's care.

### The audit is the method

Not "be careful about colour spaces". Grep every conversion, list them, and give each one a job:
**input edge, output edge, or per-operation.** Fifteen sites in this engine outside
`colour.{hpp,cpp}`, and one row left over — the GPU path had an input edge and **no output edge at
all**.

Do this to a codebase you did not write and the interesting outcome is not "found a bug"; it is
discovering how many pipelines have *three* conversions and stay correct by cancellation. Those
break when somebody adds a post-processing pass.

### SDL claims windows with an sRGB-**encoded** swapchain

Straight from `SDL_gpu.h`, and it had been on disk since Lesson 0.4:

> `SDR: B8G8R8A8 or R8G8B8A8 swapchain. Pixel values are in sRGB encoding.`
> `SDR_LINEAR: B8G8R8A8_SRGB … accessed in shaders in "linear sRGB"`

and `SDL_ClaimWindowForGPUDevice` creates the swapchain with **SDR**. A fragment shader that
returns light into that is writing a linear value into a slot that means a code. Measured on this
machine, that is what the engine did for four modules.

### An error that is a *ratio* hides

Linear 0.5 stored as 128 where 188 was meant is 60 codes. But expressed as emitted light the error
is **13× at linear 0.02 and 1.1× at 0.95** — savage in shadow, absent near white. The bright half
of every image looked nearly right and the dark half read as a deliberate moody grade.

**An error shaped like a ratio hides wherever there is least contrast to spare.** Worth carrying
as a general diagnostic instinct: when a defect survived a long time, ask what shape it had.

### A test that constructs its own environment tests the environment it constructed

Lesson 4.8 compared the two renderers pixel by pixel and reported **87% byte-identical**. It was a
sound measurement of a configuration the shipped program does not use: the harness created its
render target as `R8G8B8A8_UNORM_SRGB` while the program renders to the swapchain, which was
`UNORM`. One enum, and it was the entire subject.

The fix is not "be more careful" — it is to make the production value a **readable field**
(`gpu_report::output_encodes_in_hardware`) so a harness can assert on it instead of writing its own
literal.

### Ask, set, then read it back

`SDL_WindowSupportsGPUSwapchainComposition` → `SDL_SetGPUSwapchainParameters` →
`SDL_GetGPUSwapchainTextureFormat`. The third call is the one people skip, and it is the only one
that reports what actually happened. SDL guarantees only `SDR`; everything else is a request.

### A two-parameter setter must pass through the parameter it is not changing

`SDL_SetGPUSwapchainParameters` sets composition **and** present mode together.
`set_present_mode()` passed a literal `SDL_GPU_SWAPCHAINCOMPOSITION_SDR` — correct when written,
because there had only ever been one composition — and would have silently undone this lesson's
fix the first time anybody toggled vsync. **This lesson's own bug, latent three functions away, by
a different route.**

### Encoding in the fragment shader is the second-best answer, and the reason is the blend stage

Hardware blending happens **after** the fragment shader. A shader that encodes hands the blend
unit *codes* to interpolate — which is the whole mistake, moved one stage later. Correct for
opaque geometry, wrong the moment anything is transparent. Prefer an `_SRGB` target; keep the
shader path as a fallback; read a field to know which one you are on.

### `pad2` was never wasted space

HLSL packs a `float3` and a `float` into one 16-byte register, so `scene_light_uniforms` already
had an addressable float sitting at offset 44 doing nothing. Renaming it `encode_output` cost
**zero bytes and zero repacking** — the struct is still 64 bytes and Lesson 4.6's `static_assert`s
did not move. Padding created by an alignment rule is an *unused field*, and it is free to use.

### A prediction that fails for a reason you can state is worth more than one that succeeds

Module 5's plan said the golden was "probably over" — surely a lesson that settles colour changes
the reference render. It did not, and the reason **is** the result: the software renderer had been
correct since Lesson 1.6, and the entire defect was on the GPU path's output stage, which the
golden never touches.

---

## Course-infrastructure facts (docs/, Lesson 6.1)

### Inline KaTeX fails the page check, every time

`check-page.js` asserts `katexRendered === eqBlocks`, and `eqBlocks` counts `<div class="eq">`. A
single inline `\(…\)` anywhere makes the two disagree — this page had seven and reported
`katexRendered: 10, eqBlocks: 3`. Write inline maths as HTML (`<em>x</em><sup>1/2.4</sup>`) and
keep KaTeX for display blocks only. Already recorded in the pipeline notes; recorded again because
it was still made.

### A new listing tag needs the CSS rule *and* the checker's allowlist

6.1 added a third state, `unchanged`, for a page that reproduces a file it did not edit (labelling
that "modified" is a small lie in a caption). `check-page.js` keeps its own `TAG_MODIFIERS` list —
deliberately, since the check exists to catch a class that matches no CSS rule. So both files have
to be edited, and **the CSS rule goes first**.

### When a label cannot find clear space among the data, stop looking among the data

Figure 1's curve label was placed twice inside the plot and sat on something both times — first
the polyline it named, then a dashed leader. The interior of a plot is mostly lines. Moving it to
the surrounding text column and naming the curve in words solved it immediately.

### Write the throwaway probe before the lesson

`scratch/probe_61.cpp` — forty lines, no test framework — established every fact the lesson rests
on before a word was written: the default swapchain format, which compositions this window
supports, whether the request succeeds, and what raw linear values look like when read as codes.
Reasoning found the bug; the probe is what made it a claim.

---

## Radiometry and BRDF facts (Lesson 6.2)

### The π in `albedo/π` is the area of a disc, and it has to be derived every time

The cosine-weighted hemisphere measures **π**, not 2π, and that single number is why π appears in
shading constants at all. The geometric reading is the one that sticks: every patch of the
hemisphere, projected straight down, casts a shadow of its own size times cos θ, and those shadows
tile the unit disc exactly once. So a constant BRDF *k* returns *k·π* of everything arriving, and
demanding that equal the albedo forces `k = albedo/π`.

Two diagnostics fall straight out, and they are worth memorising because they identify the bug
from the value alone:

| Your hemisphere integral of a white Lambert BRDF returns | What is missing |
|---|---|
| 1.0 | nothing — correct |
| 2.0 | the **cos θ** weight (you integrated 1/π over 2π sr) |
| 0.5 | the **sin θ** in `dω = sin θ dθ dφ` |
| 6.28 | the **π** in the BRDF |

And: **convergence is the diagnostic**. Quadrature error shrinks as the grid refines (1.0001004 at
64×128 → 1.0000004 at 1024×2048); a wrong constant does not shrink at all.

### A BRDF above 1 invents no energy — its unit is per steradian

The confusion is real and it is settled by the unit. Radiance (W/m²·sr) over irradiance (W/m²)
leaves **sr⁻¹**. A surface reflecting 100% of arriving light into a cone of half-angle α spreads it
over a projected solid angle of `π sin²α`, so its BRDF is the reciprocal: **0.3183 sr⁻¹** at
α = 90° (Lambert), **2.7210** at α = 20°, unbounded for a mirror. The quantity actually capped at 1
is the *integral*. Confusing the function with its integral is the whole of the mistake.

### A renderer with no units has a constant hiding somewhere — find it by asking one question

**What value of the light makes a perfect white surface render at exactly full scale?** If the
answer is 1, the diffuse BRDF has no π and the π is inside the light. If it is π, the BRDF has it.
Anything else means a third constant nobody has mentioned.

This works because it is a *measurement* rather than a reading of the code — two minutes with a
white quad settles what an hour of grepping might not. In this engine the answer was 1, and four
lines of algebra then said exactly what the field had been: `intensity = E_perp / π`, since Lesson
3.6.

### Rename the field when the meaning changes, even though the type has not

Moving the π into the BRDF makes every existing `intensity = 1.0f` wrong by 3.14 — and there is no
compiler diagnostic for "same type, new meaning". Renaming to `irradiance` turns each call site
into an error a human must answer. The engine has now made this bargain three times (3.1's `z`
ahead of `colour`, 3.7's defaultless `to_eye`, 6.2's rename) and it has never been the wrong call.

**And set the default to the meaningful value, not to 1.** A default of 1.0 compiles everywhere,
mentions nothing, and darkens every defaulted scene by π — which is the exact silent failure the
rename existed to prevent, walked back in through the constructor.

### Re-associating float multiplies is not a no-op, and 8-bit output absorbs it

`albedo * (key * 1.0f * ndl)` against `(albedo * inv_pi) * (key * pi * ndl)` moved **154,240 of
342,225** sampled results, worst relative error 2.465e−07 — one to two ULP. Not one 8-bit code
moved, through either encoder. Report both halves: "byte-identical" alone reads as "you changed
nothing", and "the arithmetic changed" alone reads as "the picture moved".

A related fact worth having: `std::numbers::pi_v<float> * std::numbers::inv_pi_v<float>` is
**exactly 1.0f**, and `1.0f/pi_v<float>` has the same bits as `inv_pi_v<float>`. Neither is
guaranteed by anything; both were checked rather than assumed.

### An empty diff can be evidence — but only next to a measurement that something changed

The claim was "the π is hiding in the light". That predicts that taking it out and putting the
light at π reproduces the image *exactly*. 1,209,616 bytes of agreement is the negative control
passing. It is evidence **because** the float sweep independently shows the arithmetic differed; a
change that did nothing at all would prove nothing at all.

### A constant that exists in two languages will eventually disagree

HLSL has no `<numbers>`, so `scene.frag.hlsl` carries its own 1/π. Write more digits than a float
can hold so the compiler rounds once — then *assert it*: `verify_62` §F reads the shader source,
parses the literal and compares bit patterns against `engine::k_inv_pi` (both `0x3EA2F983`). Three
lines against a class of bug otherwise found months later in a one-code pixel diff.

### "Not energy conserving" was true for a different reason than expected

The guess was that the un-normalised Blinn-Phong lobe would exceed unit reflectance at the
shininess values the engine actually uses. **It does not** — on the 1/π scale it reaches only
0.1386 at shininess 32. The failure that survives is structural: diffuse and specular are *added
with no coupling*, so a white surface with a white highlight reflects **1.1386** of what arrives,
and 1.7333 at shininess 2. The same light is counted once as having bounced off and once as having
gone in.

This reframes what Cook–Torrance's Fresnel term is *for*: not a better lobe shape, but the
mechanism that makes the two terms dependent — `kD = 1 − F`. **Measure before writing the
paragraph that explains the failure**; the probe that overturned this took ten minutes and the
wrong version would have shipped as a confident sentence.

### Give a lobe and a BRDF different names in a function that holds both

`specular_term()` returns a bare `cos^s` — a *shape*. `specular_brdf()` returns that shape with
units. `shade()` contains both, so the local variable was renamed `spec` → `lobe`. Dividing the
lobe by π is also documented as explicitly **not** a normalisation, because it normalises nothing;
calling it one would stop the next reader from asking the question that found 1.1386.

### A harness that builds its own environment tests the environment it built — second occurrence

`verify_48` §F filled the GPU light uniform from the lamp's *colour alone*, dropping the scalar.
Silently correct for four modules because the scalar was 1; the moment it became π, worst
|CPU − GPU| went from 1.2e−07 to **5.3e−01**. The shipped renderer never had the bug —
`gpu_scene.cpp` has always multiplied. **The instrument was wrong, and only changing a value the
instrument assumed constant could expose it.**

Lesson 6.1 recorded the same failure with a different constant (a harness building an `_SRGB`
target while the program used `UNORM`). Two occurrences make it a rule: when a harness constructs
inputs the shipped path also constructs, one of them will drift, and the drift is invisible while
the value is a default.

**The ratio names the bug.** Divide the two disagreeing values; if you get π, look for a missing
multiply by the light's scalar, not for a shading error.

---

## Course-infrastructure facts (docs/, Lesson 6.2)

### Pinning a published page's listings before editing — the rule applied on time

`build_61.py` was pinned to commit `373dd4b` **before a line of 6.2 was written**, all six
repository listings, each verified byte-identical with `git show <commit>:<path> | diff - <pin>`.
Re-running the builder then produced a diff of exactly the four nav lines that were *meant* to
change. That is what a correct pin looks like, and it is the first time in this pipeline the rule
was applied ahead of the damage rather than after a `git diff` caught it.

Pin **all** the listings, not only the ones you expect to touch: 5.10 needed two pins and only
predicted one.

### A wrong relative href in a prereq link fails silently — check every link, mechanically

Two of this lesson's four prereq links were wrong (`03-06-lighting.html` for
`03-06-normals-and-lambert.html`; `03-07-specular.html` for `03-07-specular-blinn-phong.html`).
Nothing throws and nothing looks broken; the link is simply dead. `check-page.js` does not cover
this. Twelve lines of Python that resolve every `href`/`src` against the filesystem do:

```python
for href in re.findall(r'href="([^"]+)"', html):
    if href.startswith(("http", "#", "mailto:")): continue
    if not os.path.exists(os.path.join(os.path.dirname(page), href.split("#")[0])):
        print("BROKEN", href)
```

Guessing a filename from a lesson *title* is the specific trap — the titles and the slugs diverge.

### A label on a shape the eye reads as background

`check-page.js` flagged figure 3's `n` label as sitting on an `ink-soft` line. It was true and
invisible: at θ = 0 the surface normal points straight back up the beam, so the label and the
centre beam arrow wanted the same pixels. Removing the arrow fixed it. **The geometry check sees
what the eye slides over**, which is exactly why it exists — a drawing can be wrong and still look
fine.

### Write the second probe when the first overturns a guess

`probe_62.cpp` answered its four questions and turned one over: the raw Blinn lobe does *not*
exceed unity where the engine actually runs. `probe_62b.cpp` existed only because of that, and it
found the failure that does survive. The habit is not "write a probe"; it is **write another one
the moment a probe disagrees with you**, because the paragraph you were about to write is now
wrong and you do not yet know what replaces it.

---

## Microfacet facts (Lesson 6.3)

### A distribution must integrate to 1 — and that is the only reason any of this is checkable

```
∫ D(h) · cos θₕ · dω  =  1
```

Read it backwards for the statement worth keeping: **the microfacets' projected areas add up to
the area of the flat surface they stand on.** Nothing else could be true, so the `cos θₕ` is not a
convention — it is the same projected-area factor as everywhere else in Module 6.

`shininess` could not be wrong; *D* can be, in a way a machine detects. Every distribution added
from here goes through the same test, unchanged. The diagnostic table, when it fails:

| Your NDF integral returns | What is missing |
|---|---|
| 2.0 | the `cos θₕ` weight |
| 0.5 | the `sin θ` in `dω = sin θ dθ dφ` |
| 2π | the φ integral |

Establish the integrator first on something with a known answer — integrate 1 and check for 2π,
then cos θ and check for π. Two lines, and they separate the instrument from the subject.

### Blinn-Phong was always a microfacet distribution; it lacked only its constant

The bare `cos^s` lobe integrates to `2π/(s+2)` — 0.1848 at *s* = 32. Give it `(s+2)/2π` and it
satisfies the identity exactly, at every exponent. Lesson 3.7 called it "the first, crudest
microfacet model" and was being literal.

This engine multiplies by `1/π`, so it is off by `(s+2)/2` — **exactly 17× at the default
shininess of 32**, and the ratio has no π in it because both constants carry one.

**And nothing ever looked wrong, which is the transferable part.** `specular::colour` absorbed it:
an artist turns the specular up until the highlight looks right and lands on seventeen times a
plausible reflectance. **An error a parameter can absorb is invisible until someone tries to
author against physical values** — which is exactly what Lesson 6.2 did, and why it found 1.1386.

Corollary for the fix: do **not** correct the constant alone. The materials were authored against
the wrong one. Change both in the same commit or every highlight blows out.

### GGX beats Beckmann on the tail, and only on the tail

Same peak for the same α (3.5368 at α = 0.3), so every difference is shape. GGX is *lower* near
the peak — 0.71× at 20° — and **456× higher at 45°**. It must trade: both integrate to 1.

The mechanism is in the formulas. Beckmann's exponential dies faster than any power and is
numerically zero (1.9e−13) by 60°, a hard edge where a real surface glows; GGX's rational
denominator has a power-law tail that never quite stops.

### "Matches" needs a criterion — the folklore mapping matches only the peak

`s = 2/α² − 2` comes from equating the two distributions **at h = n**, and it does that to one
float ULP. Fit the whole *lobe* instead and the exponent lands consistently **0.80×** lower (0.806
at α = 0.1, 0.759 at 0.5), because a lobe fit trades peak height for a tail Blinn cannot reproduce
at any exponent.

Both numbers are right. **When you meet a conversion formula, derive it far enough to see what it
optimised** — two minutes of algebra showed a widely-quoted mapping answers a narrower question
than the one people use it for.

### The textbook GGX denominator is wrong in `float`, and the normalisation test is what finds it

Written as every reference prints it:

```
d = cos²θₕ · (α² − 1) + 1
```

this is the difference of two numbers of size 1 that nearly agree — catastrophic cancellation. A
`float` resolves that to ~1e−7 absolute, while the true value of `d` at the peak is α². At
α = 0.01 that is 1e−4, so a thousandth of it is noise before the term is *squared*.

The identical expression, rearranged, does not cancel:

```
d = (1 − c)(1 + c) + α²c²
```

**Sterbenz's lemma** is why: for `c ≥ 0.5`, `1.0f - c` is computed *exactly*, with no rounding, in
any IEEE format — and `c ≥ 0.5` covers the whole peak region. Measured, integrated against the
identity:

| α | textbook | rearranged | double (truth) |
|---|---|---|---|
| 0.001 | 0.9891063 | 1.0023035 | 1.0000013 |
| 0.010 | 0.9998169 | 1.0000004 | 1.0000000 |
| 0.050 | 1.0000006 | 1.0000000 | 1.0000000 |

**The textbook form loses 1.1% of the model's energy to rounding at mirror roughness.** The bug is
bounded by the data — it lives entirely below α ≈ 0.05 — which is why it survives in production
and shows up on chrome and still water rather than everywhere.

`verify_63` §A asserts the comparison inline, computing the textbook form and requiring ours to
beat it by 4×, because the rearrangement looks like a pointless shuffle and the next person to
tidy it back should be told by a failing test.

### Quadrature error shrinks when the grid refines. Arithmetic error does not.

This is the whole diagnostic, and it took two runs. Refining 5× moved 0.9998169 to 0.9998154 —
nothing. The identical formula in `double` on the identical grid gave 1.0000000. Diagnosis
complete: the error is in `float`, and in the formula rather than the integrator.

**Do not widen the tolerance.** The value of a test with a known answer is entirely in believing
the known answer; a tolerance widened to admit a failure is a test deleted slowly. 1.8e−4 looked
small and was a real 1.1% energy loss hiding at the smooth end.

### Shadowing and masking are a consequence, not a correction

Once you have said "landscape", you have said "some of it is hidden". Smith's *G* derives from the
same slope distribution that produced *D*, which is what makes it a consequence rather than a
second model.

It is also **the term Lesson 6.2 was missing** when it measured a 5.36× view-angle swing it could
not explain: at 75° a near-smooth surface still shows 0.9920 of itself and a fully rough one
0.4112. The effect was always real; it lacked a reason.

**The separable form's independence assumption is false, and measurably so.** Multiplying two
`G₁`s assumes being lit and being visible are unrelated events; they share a height field.
Height-correlated against separable: identical to four decimals on smooth surfaces, and **1.715×
apart at α = 0.8 with both directions at 80°**. Grazing angles on rough surfaces are sunsets, wet
roads and worn metal — "either is fine" would have been comfortable and wrong.

### Single-scattering microfacet theory loses 69% at full roughness

With *F* = 1 — a surface that absorbs nothing — R(v) for `D·G/(4 n·l n·v)` runs 0.9976 at
roughness 0.22 down to **0.3069 at 1.00**. It never *exceeds* 1, which is what the machinery
bought; but *G* removes the light that hits a hidden facet and never asks where it went. It hit
another facet and carried on.

The symptom is specific: **rough metal renders too dark, and it is a fraction rather than an
offset**, so raising the light scales the problem instead of fixing it. Kulla–Conty's
multiple-scattering compensation is the standard fix and needs an assembled BRDF to attach to.

### `α = roughness²` is a convention and a real interoperability trap

Disney's remap, shipped by UE4 and Filament, chosen because almost all the visible change lives in
α's bottom fifth — the same argument 6.1 made about sRGB spending codes where the eye is. Not
physics. A roughness copied from a tool that squares differently will not match, so convert at the
import edge and write down which convention you store.

---

## Course-infrastructure facts (docs/, Lesson 6.3)

### A figure whose numbers contradict its own caption

The first draft of the microfacet landscape counted **32 of 74** facets as facing **h** — 43% of
the surface, which says "most of it reflects at you" and is the opposite of the model being drawn.
The threshold was a guess. Tightening it to a ~4° cone gave 4 of 48, which is what a glossy surface
actually looks like.

**No geometry check catches this**: nothing overlaps, nothing spills, and the picture is perfectly
well-formed. The only test is reading the caption against the drawing.

### Draw the negative case too

The same figure only became legible when the *non*-participating facets got their normals drawn
faintly. A picture of the four that count is a picture of four sticks; a picture of four heavy
normals among forty-four faint ones is the model. **The contrast is the message, so the thing that
does not qualify has to be visible.**

### An arrow that ends in mid-air reads as a drawing error

Lesson 6.3's masking figure first drew its light rays as a floating row above the surface, pointing
the wrong way because the direction vector's sign was never checked against the screen's y-down
convention. Terminating each ray *on the facet it strikes* fixed both problems at once — and the
sign error was invisible until the arrows had somewhere to land.

### Pick plot limits from the data, not from the first curve you tried

The NDF figure clipped its sharpest curve at a hand-chosen ceiling, which renders as a flat top
and reads as a bug. With cosine weighting the peak is `1/(πα²)`, so the ceiling is computable —
and once it was, the honest picture (peaks differing 16×, areas all exactly 1) turned out to be a
better illustration than the clipped one.

---

## Lesson 6.4 — Cook–Torrance, assembled

### Write a second probe the moment the first disagrees with you — then a third

Three probes, and each existed because the previous one overturned something. `probe_64` was
supposed to confirm that Fresnel coupling closes the energy door; it left **1.2031** at grazing.
`probe_64b` asked whether that was the instrument (refine the grid — it *converges*, so no) and
where it sat (specular takes 0.40, diffuse gives up 0.05). `probe_64c` tested the resulting
*diagnosis* — that the missing factor is the light which fails to get **out** — by predicting an
exit factor would remove it. It did.

The chain is the technique. **A probe that confirms your plan has told you nothing you did not
already believe; a probe that contradicts it has told you where the lesson actually is.**

### A fix that passes the test it was written for can fail a test you forgot

The exit-factor repair killed the over-unity exactly as predicted — and it is **not reciprocal**
(0.262327 one way, 0.293236 the other), which means it is not a BRDF. The prediction was right and
the patch was still wrong.

Keep the *defining properties* of the thing you are building in a list, and check a candidate
against all of them, not only against the symptom that prompted it. For a BRDF that list is:
non-negative, reciprocal, and energy-conserving. The repair scored two out of three.

### An error a parameter can absorb is invisible — so give every parameter an inverse

Lesson 6.3 found a 17× normalisation error hiding inside `specular::colour`. 6.4 found the other
half of the same story by writing `ior_from_f0` — three lines whose only purpose is to run a
material backwards and ask what it claims about the world. The demo's authored 0.85 comes back as
an **index of refraction of 24.6**, where diamond is 2.42.

**A parameter that round-trips into a physical quantity can be audited automatically; one that
cannot, cannot.** That is a design argument for physical parameterisations that has nothing to do
with realism — it is about testability.

### When the meaning of a value changes, delete the type

`specular::colour = 0.85f` and `f0 = 0.04f` are both perfectly good floats. There is no diagnostic
anywhere in C++ for "same type, new meaning", so a struct that kept its name would have let all
forty-four call sites keep compiling while rendering twenty-one times too bright.

Deleting `specular` outright turned every one of them into a compile error. That is the third time
this course has made the trade (3.7's `to_eye` with no default, 6.2's `intensity` → `irradiance`),
and it is worth stating as a rule: **a rename or a deletion is the only refactoring tool that
reaches every caller.** Doc comments do not.

### Corrections that compensate must land in one commit

The specular constant was 17× too small and the authored specular colours were ~21× too large.
Either fix alone makes the picture *worse* than leaving both wrong — highlights blow out, or
everything goes black. There was no tidy sequence of small commits here, and pretending otherwise
would have meant shipping a broken intermediate state.

**When two errors are compensating, the unit of work is both of them.** Say so in the commit
message, because a reviewer looking at half of it will be right to object.

### The moment to extend a characterization test is the moment it breaks

Replacing the entire shading model moved **0.60%** of the reference render. Not because the change
was small — because `shading::textured` is unlit by construction and one scene faces away from the
light, so **three of seven frames exercised the shading equation at all**, and the torus chosen in
3.8 *because* it shows highlights was drawn unlit.

This is Lesson 5.1's finding a second time (there it was the near-plane clipper: *a
characterization test that does not reach a branch cannot pin it*). The new half is about timing:
an eighth frame costs **nothing** at a lesson that re-baselines the golden anyway, and costs a
whole re-baseline at every other lesson. So the moment a golden breaks for a good reason is the
cheapest moment it will ever be to improve it.

### Re-baseline honestly: record the direction, not just the hash

`905BF27E → E917C06C` says nothing. What made the new baseline believable was the *direction*:
2,400 pixels changed and **every one got darker**, none brighter — which is exactly what an
energy-conserving model replacing one that emitted light must do. A single brighter pixel would
have been a question to answer before shipping.

Record: old hash beside new, the per-channel magnitude, and a claim about the sign that the data
can contradict.

### HLSL cbuffer members share one global namespace

Adding `pad0` to a second cbuffer is a *redefinition*, not a local name — and the error, "nameless
block contains a member that already has a name at global scope", does not obviously say so. Name
padding per buffer.

### A truncated bar axis lies about the data it is there to report

The energy figure first drew its bars from 0.6, which made 0.71 and 1.24 look like a five-fold
difference in a chart whose entire subject is *magnitudes*. Zero-based, with the unity line drawn
in, is both honest and still perfectly legible — the 1.0 crossing is what the eye needs, not the
bar heights.

### Put the label where the geometry cannot reach, and compute where that is

Three separate label placements failed `check-page.js` in this lesson, and the instructive one was
the arc label: I placed "2θ" at its arc's *midpoint*, which by construction **is** the h ray. The
obvious choice was the one guaranteed to collide.

Two habits came out of it. Give a label an explicit angle rather than deriving it from the shape it
annotates. And when a figure needs four labels and four strokes in one quadrant, the fix is usually
to draw fewer things — the second ray pair in the Jacobian figure was cut, and the figure got
better.

### Geometry checks cannot see clutter

`check-page.js` passed a version of Figure 1 that a reader would have struggled with: two panels,
each with three rays crossing, labels technically clear of every stroke. The rewrite abandoned the
ray diagram entirely for a **budget bar**, because the claim being made was an accounting claim.
Look at the rendered figure and ask what it is *for*; no geometric check will ask that for you.

---

## Lesson 6.5 — A material system

### A demo that has to invent one of your types is measuring a hole in your API

`ecs_swarm` declared `struct material { Uint32 tint; microsurface surface; }` in Lesson 5.7, with a
comment naming exactly why. It is restricted to the engine's public headers, so it could not reach
inside to work around the gap — which is what makes it evidence rather than an opinion.

**When a consumer that cannot see your internals builds one of your types, the type is missing.**
Not "would be nice"; missing. Two engine structs had been *saying* the same thing in comments for
three modules and nobody acted; the demo's forty lines of workaround were the thing that finally
counted as proof.

### The membership rule for a container type should come from the machine, not from taste

"What belongs in a material?" has no good answer in English — everything surface-shaped sounds
right. It has a sharp answer in hardware: **can this be a number in a buffer?** Per-draw data can be
a uniform; pipeline state cannot, and two objects differing in it need two pipelines and a sort.

The test is even mechanically checkable — `static_assert(std::is_trivially_copyable_v<material>)` is
the compiler agreeing that the type can be pushed. **When you can find a rule the machine already
enforces, use that one**; it survives arguments that a style guide does not.

### Store a handle, use a pointer, and name the step between

Both of these are true and they pull in opposite directions: a stored reference must survive a
reallocation and be checkable, and an inner loop cannot afford a lookup per pixel. The resolution is
not to pick one — it is a named function that runs **once per draw**.

The measurement that makes it concrete: 64 insertions moved a pool from `0x928c03500` to
`0x928c16000`. Every pointer taken before that is now wrong, and nothing about it says so. The
handle names a slot and does not care.

And the case a pointer cannot express at all: a **stale** handle — well-formed, naming something
gone — resolves to "no image" and the surface falls back to its tint. Undefined behaviour became a
defined fallback, which is worth more than the four bytes it saved.

### An argument that is only true of *your* caller is not an argument about the type

The comment defending the old raw pointer said: *"`textures` lives for the whole program, so there
is nothing here that can dangle."* That was **true**. It was also a claim about one demo's lifetime
discipline, unavailable to anyone storing a material in a file, a component, or a scene.

When you find yourself justifying a type's design with facts about its current callers, you have
found a constraint that will break the first time the type is used somewhere else.

### Derive it, and the bug becomes unrepresentable

A `textured` flag stored beside a separately-chosen texture pointer can say "textured but no image"
(debug magenta) and "image but not textured" (silently flat). Neither fails where the mistake is.
Computing the flag from the handle removes both states from the program's vocabulary.

**A bug you cannot express beats a bug you detect.** — and note the trap on the way: replacing the
hand-assembly with the derived version compiled cleanly and would have silently lost every GPU
texture, because the *other* opinion (a keypress and a scene test) had never been written onto the
material. Deriving a value only helps if the field you derive from is the one that knows.

### A pool is for sharing, not for size

Ninety-six drones over six tints: 3,456 bytes of copies against 600 of handles. But the byte count
is the weaker half. With copies, "make the drones rougher" is a loop over the registry that has to
find every entity carrying that appearance; with handles it is one write.

The folk rule — "use handles for big things" — gets this case backwards. A material is small and the
handle is still right, because ninety-six things point at six. Meanwhile `scene_object` holds its
material *by value*, because it has exactly one. **Ask whether anything shares it, not how big it
is.**

### A comment predicting a future design cannot be tested

`scene_object::closed` carried a comment from 3.4 saying it would move onto the material in Module
6, "because cull mode is pipeline state and pipeline state is what a material *is*". It repeated
through three modules and was wrong twice over — a material is explicitly *not* pipeline state, and
`closed` is not cull mode anyway but a fact about the mesh.

This is not an argument against writing such comments; 3.7's identical prediction about the material
was exactly right and is why the lesson happened. It is an argument for **re-reading them when you
arrive, as claims to check rather than instructions to follow.**

### The measurement you take to strengthen an argument can be the only false part of it

Splitting `cull_mode` into its own header is justified by one rule: a type two components share gets
a header. That was enough. I reached for a second, quantitative justification anyway — compile time,
citing 5.1's measured 0.97 s for `raster.hpp`.

Re-measured on the machine actually building: `raster.hpp` 0.258 s, and `material.hpp` **already**
0.261 s via `texture.hpp`. The marginal saving is zero. Worse, 5.1's own note says in capitals
**"THE RATIO IS WHAT TRAVELS, NOT THE SECONDS"** — I read past the caveat to the number I wanted.

Two rules out of one mistake. **A recorded caveat does not protect you if the figure beside it is
more useful.** And: when a design rule stands on its own, adding a number to it is not free — it is
a new claim, with its own chance of being wrong.

### After a struct's layout changes, an incremental build is not evidence

Capturing a "before" render meant `git stash`, rebuild, shoot. The rebuild was incremental and
`scene_object` had changed size, so some translation units had the old layout and some the new.

The symptom was not a crash. It was a reference shot with the *wrong scenes in it* — frame 1
rendering `solids` instead of `cycle`, a triangle count of 20 where 44 was right, one frame's name
printing as `?`. **Plausible garbage**, and it took a minute to tell apart from a real regression.
`cmake --build build --target clean` fixed it. This is the same failure class as a uniform block
disagreeing with its shader, arriving on the CPU side.

### Two unclosed `<figure>` tags, and every automated check passed

`check-page.js` verifies highlighter round-trips, KaTeX, SVG geometry, shared assets, badge classes
and listing folds — and passed a page whose figures were nested inside an unclosed
`figure.listing`, silently inheriting `display: flex` on their captions. Two captions rendered as
forty-pixel columns of single stacked characters.

No geometric check sees that: the SVG is fine, the labels are clear, the links resolve. It was found
by **looking at the rendered figure**, which check-page.js's own header says is the thing it cannot
do for you. `scratch/check-tags.py` now balances the container tags, which is the cheap half — but
the expensive half stays: *look at the picture*.

---

## Lesson 6.6 — glTF 2.0 loading

### The write-it-or-take-it test is not "is it hard?" — it is "is the hard part the subject?"

This course has hand-rolled the maths library, the rasterizer, the OBJ parser and the ECS, and taken
`stb_image`, Dear ImGui and now `cgltf`. Difficulty does not separate those lists: the rasterizer
was harder than a PNG decoder.

What separates them is whether the difficult core belongs to the discipline you came to learn. For
OBJ the hard part was unifying `v/vt/vn` triples into vertices, which **is** the index problem. For
glTF the hard part is that an accessor may be any of five component types, normalized or not,
tightly packed or interleaved at a stride, dense or sparse — roughly forty legal encodings of the
same eight positions, every one of which some exporter emits, and **not one of them about
graphics**. `cgltf_accessor_read_float` collapses the whole matrix in one call.

Stated as a question you can ask of a library you have never seen: *if I write this myself, what
will I have learned when it works?*

### Two conventions agreeing is worth asserting, not just noting

Three of the four convention checks between glTF and this engine came out "do nothing" — handedness,
winding, and texture origin. The temptation is to write a sentence in the lesson and move on.

Assert them instead. `verify_66` §B checks that vertex 6 of the loaded cube is exactly
`(+0.5, +0.5, +0.5)` and that all eight positions match `k_cube_vertices` **bit-exactly** — no
tolerance, because every coordinate in the file is exactly representable, so a difference would be a
real one. The reason it is worth a test rather than a sentence: **a cube survives the wrong answer
looking like a cube.** Negate z and it is still a cube, wound inside out. Nobody reviewing a
screenshot catches that.

### The convention that agrees can be more dangerous than the one that differs

glTF and this engine both put texture coordinate `(0,0)` at the *upper* left. OBJ puts it at the
lower left, which is why `mesh_import::flip_uv_v` defaults to `true` — correct for every mesh this
engine had ever loaded.

Apply that default to a glTF and every asset arrives upside down, for everyone who left it alone.
**The flip belongs to the FORMAT, not to the caller's preference**, so `asset_store::load_model`
takes no import settings at all and its header says why in as many words. A parameter that is right
for one format and silently wrong for the next is worse than no parameter.

And the one that *does* differ turned out not to need code at all: glTF says an asset's front faces
+Z and our camera looks down −Z, which is an **authoring fact**, fixed with a yaw. Negating
coordinates in the loader would also mirror the geometry and reverse every triangle — two wrongs,
the second hiding the first.

### An unverified claim in a comment survives exactly as long as nobody needs it

Lesson 6.4 wrote that `1 − F(v·h)` is "what the glTF 2.0 reference BRDF uses" and marked it
`⚠ VERIFY`. Two lessons passed. Checking it took twenty minutes and found the claim **true in
substance and imprecise in a way that changed the design**: the spec writes
`mix(diffuse, specular, F)`, so the same Fresnel that scales the diffuse down scales the specular
*up*, and our specular already carried it. Reading "glTF uses `1 − F(v·h)`" as a statement about the
diffuse alone misses half the sentence.

The habit worth keeping is not "always verify immediately" — 6.4 was right to defer, and said so
with a marker and a lesson number. It is that **the marker has to name the lesson that discharges
it**, or it becomes furniture.

### Reduce two models to named terms, then price the survivor at BOTH ends

Comparing BRDFs by rendering them is useless: two that differ by 4% look identical and two that
differ by 5× look like a lighting change. Write both as a product of named terms, line them up, and
find the terms that are not the same expression. Against Khronos Appendix B, six terms line up and
**five are identical**.

Then price the survivor at both ends of its range, because one number is not a measurement. Our
diffuse coupling against the spec's is **1.036×** at normal incidence and **5.62×** at 88°.
Reporting only the first says "the models agree"; reporting only the second says "they are
unrelated". Together they say **where to look** — the difference lives at silhouettes and vanishes
in the middle of every surface, which is exactly why it went unnoticed for two lessons.

### Two things that look like different models can be the same expression

The glTF spec blends two complete BRDFs by `metallic`; this engine blends the F0 and evaluates one.
They commute **exactly**, because Schlick is affine in F0: `F(f0) = f0(1 − w) + w` is a straight
line, so lerping the inputs and lerping the outputs agree. Measured at 1.19 × 10⁻⁷ over 4,851
points — float rounding and nothing else.

Worth looking for whenever two implementations of "the same thing" appear to disagree structurally.
**Check whether the operation you are interpolating through is linear** before concluding one of
them is an approximation of the other.

### A gap between two working halves is invisible to every test, because no path crosses it

The engine has been able to decode a PNG since Lesson 5.3 and to sample a texture since Lesson 3.9,
and **nothing has ever joined them**. Every CPU texture was generated (`make_checker`,
`make_uv_grid`) and every loaded image went straight to the GPU as bytes. Two complete, tested,
well-documented halves with no middle.

No test could have caught it: there was no wrong behaviour, only absent behaviour, and coverage
measures the code that exists. It surfaced the moment a glTF material named an image file and the
software renderer had to sample it — twelve lines, `to_texture`.

The generalisation: **look for pairs of subsystems that ought to compose and have never been asked
to.** They are where the next feature will discover a hole.

### A conformance gap belongs at the point that knows both halves

glTF multiplies a base colour factor by its texture; this engine's albedo image replaces the tint.
They agree exactly when the factor is white, so the gap is only the *combination*.

The parser sees the factor and the URI but not whether the image resolved. The renderer sees a bound
texture but has long since lost the factor. Only `load_model` holds both, so that is where the
warning lives — and it is a **counter** as well as a log line, because a log line scrolls past and a
number can be asserted. `verify_66` asserts `factor_texture_conflicts == 0` on an asset built so it
should be zero, which tests the *detector* as much as the asset.

### Report a format limit; never truncate to meet it

`mesh_data::indices` is `uint16_t`, so a primitive can name 65,536 vertices. Real glTF assets exceed
that routinely. The lazy version is `static_cast<uint16_t>(index)` and let it wrap — and that
version **renders**, as a spray of triangles connecting the wrong corners, looking like a corrupt
file rather than a loader limit, with nothing anywhere saying what happened.

Widening the index type costs bytes on every mesh in the engine; splitting the primitive costs code.
Both are real answers and neither is a thing to guess at inside a loader. What the loader owes the
caller is `skipped_too_large = 1, max 197,346` — a number they can act on.

### An assertion in a test can be wrong about the code's *correct* behaviour

`verify_66` §A asserted the loaded cube had 8 vertices, matching `cube_mesh()`. It got 36, and the
loader was right: the test asset shipped no normals, so flat normals were generated, and flat
shading forces one vertex per face-corner. Lesson 3.5's index problem, arriving in a second format.

The fix was not to relax the assertion — that is a test deleted slowly (6.3's rule). It was to make
the **assets say which case they test**: `cube.gltf` now ships smooth normals so its round trip stays
index-for-index *and* proves the loader honours authored normals, and `shapes.glb` ships none so the
generation path and its 4.5× vertex-count cost have an asset of their own. One failing check turned
one test into two better ones.

### A physically correct render can be a bad picture, and saying so is the lesson

`shapes.glb`'s metals render nearly black, or clipped to white. That is not a bug: a conductor has no
diffuse lobe, so a metal with nothing to reflect but one directional light has exactly two states.
Measured, the bright one is **64× displayable white** at roughness 0.25.

The temptation is to fudge the asset until the screenshot looks good. The better move is to keep the
values defensible, *measure* the clipping, and let the number be the argument for the two lessons
that fix it — image-based lighting gives the metal something to reflect, and a tonemapper gives the
highlight somewhere to go. A demo whose flaw has a lesson number attached is teaching.

### Check forward lesson references against the index, not against memory

Lesson 6.4 shipped three stale "Lesson 6.7" references for glTF, which is 6.6. Lesson 6.6 then wrote
"6.9's image-based lighting" and "6.11's frustum culling" — both wrong, because IBL is **6.12** and
frustum culling is **6.13**, and it also got the next lesson's *title* wrong ("TBN Basis" against the
index's "TBN Derivation").

Twice in three lessons, from the same cause: writing a forward reference from memory of the plan
rather than from `docs/index.html`, which is the plan. **The index is the authority.** Grep the
lesson body for `6\.[0-9]` before building the page; it takes ten seconds and it has now caught six
errors.

---

## Lesson 6.7 — Normal mapping and the TBN derivation

### Three of the hardest bugs here share one shape: they produce a plausible picture

A normal map read as sRGB tilts every surface by 38.8°, which reads as "this map was authored too
strong". A dropped handedness lights one half of a symmetric model as the mirror image of the other,
which reads as a modelling error. A tangent transformed by the normal matrix skews the frame within
the plane, which reads as an asset authored at the wrong angle.

None of them looks like the thing it is, and each has a *plausible wrong fix* that makes the picture
less wrong without making it right. **That is the argument for a type or an assertion rather than a
comment**: a `vec4` that cannot lose its sign, an enum on the texture that no caller can forget, and
a round trip whose floor is predicted from the encoding.

### Look for pairs of subsystems that ought to compose and have never been asked to

The GPU path has had a colour-space flag since Lesson 4.7 — `create_sampled(…, srgb)` picks
`_UNORM_SRGB` or `_UNORM`. The software renderer had **no counterpart at all**: `sample` decoded
unconditionally, and was right every time, because every texture it had ever been given was an
albedo.

Two halves of one idea, one complete and one absent, and no test could see it because there was no
wrong behaviour — only absent behaviour, and coverage measures the code that exists. This is the
second time in two lessons (6.6 found the same shape between `load_image` and `sample`), which makes
it a habit worth having rather than a coincidence.

### Derive a test's tolerance from the encoding; never choose it

`verify_67` §D asserted an exact round trip through a flat normal map and measured 5.55e−03. The
renderer was right and the assertion was wrong: **0.5 is not an 8-bit code**, so 128 decodes to
1/255 rather than 0, and every flat normal map in existence tilts its surface by 0.318°.

The wrong repair is to loosen the tolerance until it passes — 6.3's rule, a test deleted slowly. The
right one is to *predict the floor from the encoding* and assert the measurement equals it, which is
strictly stronger: it now also catches an implementation that is somehow **better** than the
quantisation floor, which would mean the encoding is not what we think it is.

§E made the same mistake in the other direction. Its first bound was `1/510` — which forgot that the
`[-1,1]` decode **doubles** a half-code error — and produced a bound *below* the true floor. **A
tolerance that is wrong rather than merely loose fails a correct implementation**, which is the more
expensive of the two mistakes.

### A parameter whose effect changes when you change an unrelated parameter accuses the wrong subsystem

`make_normal_bumps` first took `strength` as the height field's amplitude. `A·cos(ku)·cos(kv)` has a
peak gradient of `A·k`, and `k` grows with the cell count — so an amplitude of 1 at six cells is a
slope of 37.7, a surface tilted 88° everywhere. Every normal points sideways, `n·l` collapses, and
the render goes dark.

The symptom pointed straight at the renderer. It took a minute to realise the *shading was correct*
and the input was absurd. Dividing the amplitude by `k` makes `strength` mean the maximum **slope**,
so 1.0 is a 45° tilt at any cell count.

The general form: a parameter you cannot reason about in isolation produces failures that accuse
whatever consumes it.

### Assert the numbers you also put in a diagram

`gpu_vertex_pnu` grew from 32 bytes to 48, and the build stopped at Lesson 4.5's
`static_assert(sizeof(gpu_vertex_pnu) == 32)` before a pixel was drawn. That assertion exists
because 4.5 drew a memory-layout figure, and the alternative to asserting the number is a lesson page
that says 32 bytes forever while the code says 48.

Two more caught the same growth: `verify_54`'s `sizeof(mesh)` and `verify_56`'s
`sizeof(scene_object)`, the latter for the **second** lesson running. A struct quietly crossing a
cache line is 5.6's whole subject, and it is only visible if somebody wrote the number down.

### A shared vertex layout is an interface, and widening it renumbers every consumer

Adding a fourth mesh attribute made `gpu_mesh::describe` emit location 3 — which Lesson 4.6's
instancing pipeline already used for per-instance placement. Vertex attribute locations are numbered
**across the whole pipeline**, not per buffer.

4.5's `check_layout` caught it in `verify_46`: it compares declared attributes against the shader's
*reflected* inputs and reports `extra` and `duplicate`. Without it the symptom would have been
`placement` silently fetching a tangent's bytes — seven instances at nonsense positions, in a demo
three modules old, with nothing pointing at the lesson that broke it.

Two fixes were available and the smaller one was also the better design. Renumbering every consumer
teaches "a layout is an interface" and is real churn; making the attribute **optional** teaches that
**a vertex layout is per-pipeline state**, which is the more useful fact — a shader that never reads
an attribute should not declare one, because it is a fetch paid for nothing.

### Adding a field to a struct is a good reason to convert its initialisers

Two positional aggregate initialisations in `soft_renderer.cpp` broke when `vertex` grew a
`tangent` between `normal` and `world`. That was a *compile error* — a `vec3` where a `vec4` was
expected — which is the good outcome.

The version worth designing against is the one that **compiles**: two same-typed fields swapping
places, silently. `mesh.hpp` made exactly this argument in Lesson 3.5 and this file never took it.
The moment a struct grows is the moment to convert.

### Gram-Schmidt twice is not redundancy

The tangent frame is orthogonalised at the vertex *and* again at the fragment, and both are
necessary. The vertex pass makes the frame orthonormal **at the vertices**; interpolation across the
triangle destroys both orthogonality and unit length again, exactly as it does for the normal (3.8
measured that). The per-vertex pass makes the *inputs* sane; the per-fragment pass makes the *frame*
sane.

Worth stating because "we already normalised that" is a very natural objection and it is wrong for a
reason that is easy to say once and hard to reconstruct.

### An asset's defect can be invisible until a feature makes it visible

`assets/cube.gltf` carries one uv per box corner, which Lesson 6.6 chose deliberately so its
round-trip test could compare index-for-index against `cube_mesh()`. The consequence — four of six
faces have **zero uv area**, and therefore no tangent frame — was not detectable by anything until
normal mapping arrived and made those faces band instead of dimple.

Nothing was wrong with the asset, the test, or the loader. A property that had no observable
consequence acquired one. Worth remembering when a new feature makes old content look broken: check
whether the content was always like that.

### A regular sampling grid aliases against a regular texel grid

`verify_68` §C measures shadow acne by walking a grid of points across a plane and counting how many
report themselves shadowed. The first version used a plain 96×96 grid and reported **3.3%** where the
true figure is **50.1%**, and a worst-case depth error at 22% of a bound that is in fact tight.

Nothing was wrong with the renderer. The grid's spacing came out at 4.8 shadow texels, so every
sample landed at nearly the same position *inside* its texel: the measurement covered a sliver of the
sub-texel space instead of all of it. The fix is a **Weyl sequence** — step the sub-texel offset by
the golden ratio's fractional part, the number hardest to approximate by a rational and therefore the
one that never falls back into step with the grid.

This is not a testing footnote. It is the same aliasing the shadow map itself suffers from, it is why
a shadow's edge crawls when the light moves, and it is the reason cascaded shadow maps snap their box
to texel boundaries. **When a measurement disagrees with your eyes, suspect both.**

### An assertion stricter than its own message fails correct code

`verify_48` carried `sizeof(scene_light_uniforms) == 64 && sizeof(material_uniforms) == 32` under the
message *"the two fragment blocks fill whole registers"*. Lesson 6.8 grew the light block to 176
bytes — still a whole number of 16-byte registers, so the property the message names was never
violated — and the harness failed.

The invariant is `% 16 == 0`. Pinning an exact size under a message about packing meant a legitimate
change failed a test that was not about it, and the diagnosis cost more than the fix. Write the
assertion the message claims; if an exact size is also worth pinning, pin it *separately*, with its
own message saying which lesson set it and why.

### Where your rasterizer samples decides `round` versus `floor`

This engine's `fill_triangle` evaluates every attribute — depth included — at **integer** pixel
coordinates, not at pixel centres. So the depth stored "for texel *i*" is the surface's depth at
exactly *x* = *i*, and a shadow lookup must take `round(x)` to find the nearest stored sample.

Hardware samples at pixel *centres*, and `SampleCmp` selects the texel *containing* the uv — a
different spelling of the same [−0.5, +0.5) offset. Write `floor` on the CPU side to "match the GPU"
and every offset is biased half a texel in one direction: the sampling reach doubles, the derived
bias is half what is needed, and acne returns on one side of every slope while the other side looks
perfect.

The general form: **a lookup and the pass that wrote the thing being looked up must agree about where
a sample sits**, and that agreement is a property of the rasterizer, not of the API.

### A bound that is never reached is not a derivation

Lesson 6.8 derives shadow bias as `reach × world_per_texel × tan θ ÷ depth_range` and then measures
the worst actual disagreement: 1.931e−3 against a bound of 2.101e−3, **92% of it**.

That last number is the point of the test. A bound you cannot exceed is easy to write and tells you
nothing; a bound that is *reached* means the derivation describes what actually happens. Had the
measurement come out at 22% — as it did, from the aliasing above — the bound would have been
describing something else, and the bias built on it would have been a superstition that happened to
work.

Applies to any derived tolerance: **measure how close the worst real case gets to it, and treat a
large gap as a bug in the derivation rather than as safety margin.**

### Anything derived from one sample's footprint must be re-derived when the footprint widens

The bias above was derived for a single lookup, where the fragment sits somewhere inside its own
texel — half a texel diagonal, `√2/2 = 0.707` texels of reach. A 3×3 PCF kernel reads a texel one
step out in each direction, so its furthest tap is **2.121** texels away: three times as far.

A bias sized for one tap therefore covers a third of what a 3×3 needs, and the acne walks back in **at
the exact moment PCF is switched on** — which makes it look like a filtering bug rather than a bias
one. Measured: 78 stray pixels at radius 0, **10,348** at radius 1, 20,265 at radius 2; after the
correction, 174 and 224, which is the legitimate soft edge.

Found by rendering, not by reasoning. The general rule is worth carrying: a formula whose inputs
include "how far apart are the two things I am comparing" has a hidden dependency on every filter,
kernel or footprint downstream of it.

## Renumbering a live course (roadmap reshape, 2026-09-08)

- **A bare `N.M` in this corpus is almost never a lesson reference.** Renumbering Module 8 → 9
  turned up 58 candidate `8.N` strings; exactly **4** were lesson references. The rest were
  intra-page section headings (`<h3>8.2`, and pages number their own sections 1–15), exercise
  numbers (`Exercise 4.8.3`), and measurements (`8.3 MB`, `8.4 × 10⁻⁸`, `farZ = -8.2`). A blind
  `sed` over `8.N` would have silently corrupted ~54 sites, most of them numeric data inside
  published prose. **Sweep the unambiguous long form (`Module 8`, 177 occurrences, verified total);
  review the short form by hand, every time.**

- **The dangerous replacements are the ones a newline splits.** Three misses survived a 37-rule
  table purely because the text read `Module 7's\n    /// collision lessons`. Re-scan after any
  bulk edit with a `re.S` pattern and a keep-list, rather than trusting the rule table's own
  report — the rules that matched *nothing* are the ones worth reading.

- **Source comments are copied verbatim into published pages, so a fix must be paired.**
  `scratch/build_NN.py` inlines engine source via `@@LISTING:path@@`. `bounds.hpp`'s comment string
  is physically embedded in `docs/lessons/06-08-shadow-mapping.html`. Editing only the header
  leaves the page disagreeing with the repo, and `scratch/` is gitignored so regeneration is not
  guaranteed. **Edit both, always.**

- **Fix a misattribution to its *current* correct value before renumbering, not after.**
  `bounds.hpp` said "Lesson 6.10's frustum culling" when 6.10 was HDR and culling was 6.13.
  Correcting it to 6.13 first let the general 6.13 → 6.16 map carry it home; correcting it
  afterwards would have needed a special case that the map would have fought.

- **Keeping one lesson number fixed can be worth more than a tidy sequence.** 6.9 stayed put
  because 6.8 references it six times in prose plus both nav labels and calls cascades "an
  extension of this file". Inserting before it would have cost 8 edits and a narrative thread;
  inserting after it cost nothing. **Renumber around the references, not through them.**

- **An unchecked hand-maintained page drifts in every direction at once.** `docs/index.html` had
  never been validated and had five simultaneous inconsistencies, two of which contradicted *each
  other* (95 vs 94 lessons) on the same page. `docs/_template/check-curriculum.py` now checks it,
  and CLAUDE.md §11's pre-flight requires it green.

- **A new linter's first run is mostly its own bugs, and that is normal.** The checker's first pass
  reported 27 problems; 16 were entity-encoding false positives (`&mdash;` vs `—`, `&middot;` vs
  `·` — both render identically and pages use them interchangeably). Normalise punctuation entities
  before comparing, and never `&lt;`/`&gt;`, which appear inside code listings. A linter that cries
  wolf gets muted, which is worse than no linter.

- **zsh does not word-split unquoted parameters.** `node check-pages.mjs $PAGES` passed 42 page
  paths as one argument and reported a single bogus FAIL. Use `${(f)PAGES}` (split on newlines).
  This will bite again in any loop over a captured file list.

## Retrofitting a published lesson (2026-09-08)

- **A pinned listing means three copies, not one.** `build_66.py` splices `gltf.hpp` from
  `scratch/l66_gltf.hpp`, not from the live header, so correcting a doc comment meant editing the
  header, the pin, *and* the rendered listing already inside the page. Editing only the header
  leaves the page disagreeing with the repo and nothing reports it; editing only the page means the
  next rebuild silently reverts you. **Check `LISTING_SOURCE` before touching any file a published
  lesson lists.** Diff the pin against that lesson's commit first, so you know you are starting
  from a clean snapshot.

- **The most damaging gaps are the ones that fire no status.** 6.6's importer reports every limit
  it has — `too_many_vertices`, `unsupported_primitive`, an assertable `factor_texture_conflicts`
  count — except `alphaMode`, which is silent. And the reason is worth generalising: **a status
  code needs a concept to compare against.** The engine had no blend state, so the importer could
  not report a conflict with something that did not exist. When a subsystem "forgets" to report a
  case, check whether it has anywhere to put the answer before calling it an oversight.

- **When correcting a published measurement, separate the number from the reading.** 4.8's 87% is
  *sound for what it measured* — geometry, winding, depth and interpolation genuinely agreed. What
  was wrong was the broader inference that the GPU path was correct. Saying "keep the result, here
  is what it cannot support" is both more accurate and more useful than retracting a figure that
  was never false.

- **A harness that builds its own configuration tests that configuration.** 4.8's comparison
  constructed its own `_SRGB` target to isolate the rasterizers from presentation — a reasonable
  instinct that removed the exact surface the bug lived on, and let a real defect survive four
  modules *and* a pixel-by-pixel comparison. When a test constructs its own setup, ask which part
  of the shipped path it just stopped covering.

- **Qualify the recap too.** A reader who skims takes the recap's summary as the finding. A caveat
  buried in §3 does not reach them.

## Lesson 6.9 — Cascaded shadow maps

- **A derived formula proves itself when you change what it was derived from.** 6.8's bias was
  `reach·world_per_texel·tanθ/depth_range`. Cascades change `world_per_texel` by 7.3×, and not one
  line had to move — because both terms live in `light_camera` and `visibility()` already read them
  from there. That is the difference between a formula and a tuned constant with a good story: the
  constant needs a new value per cascade, the formula does not. **Design the audit into the next
  lesson, not into the one making the claim.**

- **The strongest result was the one nobody arranged.** In device depth the bias is *constant*
  across cascades 1–3 (2.031e-03), because a sphere-fitted cascade has `wpt = 2r/res` and
  `range ≈ 2r`, so the radius cancels and `bias ≈ reach·tanθ/resolution` — 2% from the prediction.
  And cascade 0 disobeys **exactly** where the derivation says it should, its range being set by the
  casters rather than its sphere. An exception you can predict from the mechanism is worth more than
  a rule with no exceptions.

- **Anchor a quantisation basis somewhere other than the thing you are quantising.** Texel snapping
  did nothing at all, silently, because the light basis was built with
  `look_at(centre, centre + fwd, up)` — making `basis * point(centre)` exactly `(0,0,0)`, so there
  was nothing left to round. The harness caught it as "§E reports an identical 0.499 texels with
  snapping on and off", which is a far better error message than a picture that looks slightly
  crawly. **A no-op that compiles is why the measurement has to be numeric.**

- **Measure the case where the feature loses, and put it in the lesson.** On this course's own demo
  scene (2.4 m across, camera 9.8 m back) cascades come out *worse* than 6.8's single map — 0.02169
  against 0.0207 — because there is no far field to over-serve and the sphere fit charges 29%
  regardless. The 2.8× win only appears on a 40 m scene. Shipping both numbers is what stops a
  reader cargo-culting the technique.

- **A uniform slot has no null.** A `cbuffer` the shader declares and nobody pushes reads as
  whatever was last in that slot — not as zero. The cascade block is therefore pushed on every
  frame, with a deliberate identity fallback, even when shadows are off. The nullable-pointer
  bargain the rest of the engine makes does not survive the crossing to GPU state.

- **Radial distance is not axial depth, and the difference has a shape.** Splits are computed in
  view-space depth; a fragment knows a position. `length(eye − world)` overstates depth by
  1/cos(off-axis) — ~22% at the corner of a 60° frame — so selecting a cascade on it bends the seam
  into a curve that follows the frame edge. Sixteen bytes of `view_forward` keeps selection and
  fitting talking about the same quantity.

- **Making one case the degenerate case of another beats maintaining two.** `Texture2D` and
  `Texture2DArray` are different HLSL binding types, so supporting both means two shaders. Making
  the shadow map *always* an array — one layer for 6.8's single map — left exactly one code path.

- **The rebuild-and-diff discipline caught a real regression this session.** Retrofit edits made to
  a *rendered page* were silently reverted by the first rebuild, because the source fragment in
  `scratch/` still held the old text. Pins must be taken from the state you actually intend to
  ship, not from the lesson's original commit — the renumber had moved on since. See
  `state-md-is-append-and-merge` and the pipeline notes.

## Lesson 6.10 — Mipmaps, LOD, and anisotropic filtering

- **Decide the golden question before writing code, not after the diff.** Unlike every lesson since
  6.4, this one's reference render was *not* automatically safe: the fixture samples textures, so a
  mip chain built by default would have moved it. Two honest options existed — re-baseline and say
  so, or make the chain opt-in — and opt-in won on its own merits (33% memory, a 1×1 fallback has
  nothing to average). Had I written the code first, the choice would have been made by whichever
  was easier to retrofit.

- **A deferral repeated in a shipped public header is a debt with the student's name on it.**
  Mipmaps were promised six times: prose, two exercises, a `num_levels = 1` comment, and a doc
  comment in `texture.hpp`'s sampler struct. Two external reviewers found the gap **from the
  published outline alone**, without the code. Keep a list of what you are deferring, and check it
  against what you have shipped.

- **`rule()` takes a class, not a colour — and the failure is invisible.** Passing a hex string
  gives `class="#e05c5c"`, which matches no CSS rule, so the line renders as nothing at all. No
  error, no warning, and `check-page.js` cannot see a line that is not drawn. Two dashed markers
  were missing from a figure for three build cycles. **Only the visual pass catches this**; added a
  `cline()` helper so the mistake is not available.

- **Figure numbers must follow page order, and nothing checks it.** The numbers live in
  `build_NN.py`'s `FIGURES` dict, the order lives in the body fragments, and they drifted the
  moment a figure was authored before the section that shows it — producing `fig4.png` captioned
  "Figure 5". Read the captions in order, every time; it is a five-second check that no tool does.

- **`svgSpill` can be vertical.** A label past the bottom of the viewBox reports as `overPx` just
  like one past the right edge, and I spent a cycle shortening text that was already narrow enough.
  Check the y coordinate against `H` before shortening the string.

- **At 390 px the SVG scales but the text does not.** Font size is CSS px, not SVG units, so a long
  string that fits comfortably at 1280 px overflows at mobile width regardless of the viewBox.
  Split long captions into two lines rather than widening the canvas.

- **On a log axis, adjacent values collide however you draw them.** 62.46 and 64 are three pixels
  apart at this width, so a vertical marker at the measurement will always land on the "64" tick. A
  short leader out to clear space says the same thing and collides with nothing.

- **Two SDL sampler fields silently disable mipmapping, and they are the same shape:** a default
  that is correct for a one-level texture and wrong for a nine-level one. `max_lod = 0` clamps the
  entire chain away, and `max_anisotropy` is ignored without `enable_anisotropy`. Neither errors,
  because both are legal requests. When a feature "does nothing", suspect a clamp before a bug.

- **SDL's GPU validation is conditional on debug mode.** All three checks on
  `SDL_GenerateMipmapsForGPUTexture` — no pass in progress, `num_levels > 1`, usage carrying
  `SAMPLER|COLOR_TARGET` — live inside `if (device->debug_mode)`. On a release device the
  requirement is unchecked and the result undefined. Related: SDL's asserts key on `__OPTIMIZE__`
  rather than `NDEBUG`, so `-O2` alone switches them off.

## Lesson 6.11 — transparency

- **An explicit `destroy()` just before a scope ends is almost always a bug.** `verify_611` §H
  segfaulted on exit with every check passing. The device was declared first — so C++ was already
  going to destroy it *last*, which is exactly the order the shaders, meshes and samplers need.
  Calling `gpu.destroy()` at the end of the function ran the device's teardown in the *middle* of
  that sequence, and the shader destructors then released handles against freed memory. Deleting
  the cleanup fixed it. RAII's ordering guarantee is the feature; hand-written teardown is how you
  opt out of it without meaning to.

- **A refactor can keep a golden, but only if the float operations keep their ORDER.** `sample`
  and `sample_mipped` were both rewritten as wrappers over four-channel versions, and
  `average_2x2` grew a per-texel weight — and the reference render stayed byte-identical, because
  the colour channels are combined by the identical expressions in the identical sequence and the
  default weights are exactly `1.0f` over exactly `4.0f`, so `sum * (1/4)` is bit-for-bit
  `sum * 0.25f`. "Equivalent in effect" and "identical to the last bit" are different claims, and
  only the second one survives a byte comparison. Plan for the second when a golden is in play.

- **Do not test a symmetric-looking property at its symmetric point.** The check that `over` is
  not commutative was written at `a = 0.5`, where `src*a + dst*(1-a)` weights both operands
  equally and the two orderings coincide — so the test failed while the code was right. The most
  obvious value to pick was the one value at which the property is invisible.

- **`enable_blend` is the third member of a family.** Fill in every field of
  `SDL_GPUColorTargetBlendState` and leave the enable false, and the state is perfect, the picture
  is unchanged, and nothing errors. Same shape as 6.10's `max_lod = 0` and `max_anisotropy`
  without `enable_anisotropy`. Also note that the zero-initialised alpha factors are
  `SDL_GPU_BLENDFACTOR_INVALID`, which is not a synonym for "the same as the colour ones".

- **A harness section that creates a GPU device should not be followed by one that does not.**
  `verify_611`'s golden ran after the GPU section and inherited its initialised video subsystem and
  destroyed device. Moving the golden ahead of it costs nothing and removes a whole class of
  "the reference render moved" that has nothing to do with the reference render.

- **"Empty space" in a plot is not a measurement.** Two figures placed their annotations in
  regions that looked clear; `check-page.js` reported six texts sitting on a polyline. On a
  diagram whose curves cross most of the box there is very little genuinely empty interior — put
  the legend outside the plot and stop guessing.

- **A published assertion can legitimately go red.** `verify_67` asserted
  `sizeof(material_uniforms) == 32`, which was true of 6.7 and stopped being true when 6.11 spent a
  third register. The fix is to amend the assertion to what it was ever *about* (the flag is
  derived) and record the supersession in a comment — not to leave a harness failing, and not to
  pretend the earlier lesson was wrong.

## Lesson 6.12 — HDR and tonemapping

- **A monotonic relationship coming out backwards means the measurement is missing its target.**
  `probe_612.cpp`'s first version swept the eye through a plane and reported that *sharper*
  surfaces had *lower* specular peaks — roughness 0.05 peaking below roughness 0.35. The physics
  was not surprising; the sweep never passed through the mirror direction, and a sharp lobe is a
  few degrees wide. Evaluate at `reflect(−to_light, n)`. The same trap bites the demo: a
  flat-faced scene at roughness 0.15 reports **zero** pixels over the lid, which is how a renderer
  with a real clipping problem looks completely fine.

- **Keeping codes and light apart does not get easier because you are the one writing about it.**
  A doc comment in `hdr.hpp` claimed Reinhard needs an input of ~768 to reach code 255. The real
  answer is **224**: the 254.5/255 threshold is an *encoded* value, and inverting the sRGB
  transform first gives a linear 0.995545. I inverted in the wrong space — Module 6's recurring
  mistake, committed in the module about it, and caught only because the harness measured the
  number instead of repeating it. **Assert quantities you state in prose.**

- **"Decide the golden question before writing code" is now three for three**, and 6.12 produced
  the strongest form of the answer yet. 6.10 and 6.11 made their features opt-in *by default*;
  6.12 did not need to, because tonemapping is a stage over a **different buffer type** and the
  reference fixture has no such buffer. A structural reason beats a flag: nobody can turn it off by
  accident.

- **Hoisting can be free, and worth proving separately.** Moving a hundred lines of shading out of
  `fragment` into `shade_lit` was pure code motion, and running the previous lesson's harness
  immediately afterwards — before adding anything — is what made the later HDR work safe to do.
  Verify the no-op step on its own; a refactor and a feature landing together have no control.

- **Two files that must agree, with nothing checking them, will drift.** Figure numbers live in
  `build_NN.py` and figure order lives in the body fragments. 6.10 shipped `fig4.png` captioned
  "Figure 5"; 6.12's first build did the same thing with two figures. Added `figOrder` to
  `check-page.js`, which then found the identical defect in **three already-published lessons**
  (2.5, 3.10, 4.1). The lesson generalises past figures: when a fact is stated in two places, either
  derive one from the other or check them, because discipline is not a mechanism.

- **An explicit `destroy()` before a scope ends is still a bug.** 6.11's segfault taught this and
  6.12's harness was written with the fix already in place — declare the device first so it is
  destroyed last, and delete the hand-written teardown. Worth recording that the *second*
  application of a lesson is where it pays.

## The figure-order sweep — three lessons, and what it uncovered underneath

- **A checker's first run is worth more than its hundredth.** `figOrder` was added in 6.12 after
  that lesson's own build misnumbered two figures. Sweeping the back catalogue with it found the
  same defect in **three published lessons** — 2.5, 3.10 and 4.1 — which had been shipping a
  numbered cross-reference that landed on the wrong picture. Same pattern as
  `check-curriculum.py`, which found three dead prerequisite links on its first run.

- **Renumber, do not move.** In all three, every figure already sat in the section that discusses
  it and every prose reference was adjacent to its own figure; only the numbers were out of step.
  Moving a figure would have moved it away from the prose that introduces it. The rule that fell
  out: **when placement and numbering disagree, the placement is almost always the deliberate one**,
  because it was chosen while writing the argument and the number was assigned in a draft order.

- **Prove the builder reproduces the page BEFORE changing anything.** Both old builders turned out
  to be unrunnable, and in two different ways that would each have silently corrupted a page:
  - `build_310.py` and `build_41.py` still **stamped a `STATE` block**, retired from lesson pages at
    5.7. Re-running either would have re-added 60% of a file.
  - Both read their listings from `src/`, a directory **Module 5's refactor deleted**. They had been
    unreproducible since 5.1 and nobody had noticed, because nobody had needed to rebuild them.
  Fixed by retiring the STATE stamping and pinning every listing from the commit that shipped each
  lesson — after which both rebuilt **byte-identically**, which is the only thing that makes the
  subsequent diff trustworthy.

- **A rendered page can be more correct than its source.** The 2026-09-08 Module 8→9 renumber and
  4.1's "next" nav link had been applied to the shipped HTML and never to the body fragments or the
  listings. A rebuild would have reverted all four. **If a page is edited by hand, the edit has to
  go back into whatever generates it, the same day** — this is the third time this exact drift has
  been found (6.9's retrofit links, and now twice here).

- **Not every page has a generator.** Lesson 2.5 predates the `build_NN.py` pipeline, which starts
  at 3.7 — so for it the rendered HTML *is* the source and editing it directly is correct. Check
  before assuming a build script exists.

- **Swapping two values needs a sentinel.** Renaming `fig3 ↔ fig4` (files, caption numbers, and
  `aria-labelledby` ids) one at a time clobbers one of them. Every swap here went through a
  temporary name. Related: an assertion of the form "the substitution changed something" is wrong
  for an identity mapping — 1→1 legitimately changes nothing, and asserting otherwise aborted a
  script halfway through a rename.

---

## From Lesson 6.13 (bloom and the post-processing stack)

- **A `POST_BUILD` command runs only when its own target is rebuilt — so it cannot be trusted to
  deploy a dependency.** `engine_use_shaders()` copied compiled shaders beside each executable with
  `add_custom_command(TARGET x POST_BUILD ...)`. Edit only a shader and the shader target
  recompiles, but no executable has any reason to relink, so the copy never fires and every program
  keeps the shader it was last linked beside. **The build reports success the whole time**, and the
  new HLSL really is compiled and sitting in `build/shaders/`. This had been live since Module 4 and
  hid because nobody had ever edited a shader without also touching C++ in the same build.
  The fix is the standard shape: a command whose `OUTPUT` is a stamp file and whose `DEPENDS` are
  the compiled shader **files**, wrapped in a target the executable depends on — so the copy runs
  *before* the executable is considered built. Note that the `add_dependencies` call already present
  ordered the **compile** and never the **deploy**; having one is not evidence of the other.

- **A GPU result that is constant across a parameter sweep means the parameter is not reaching the
  code.** That is what exposed the above: the bloom composited as exactly zero at intensities from
  0.05 to 50. Reading the bloom target back showed it held exactly the derived value, which
  relocated the fault from the chain to the composite — and the decisive diagnostic was that the
  *deployed* `.msl` was a different size from the canonical one. **When a GPU answer disagrees with
  everything else, check that the binary on disk is the one you think you compiled.**

- **`check-page.js`'s spill check tests text only.** A legend *box* ran 6 px off its viewBox and the
  page passed. Shapes drawn from computed coordinates can leave the frame exactly as labels can, and
  nothing is watching.

- **The automated page checks are necessary and not sufficient — look at every figure.** The chain
  diagram passed every check while its labels were crowded, its arrows did not meet its boxes, and
  one label sat on a corner. Crowding, dead space and arrows that fail to connect are invisible to a
  geometric test and obvious in a screenshot.

- **A caption that names another figure by number is a reference the `figOrder` check cannot see.**
  Renumbering left a caption referring to "figure 2's tail" *while being figure 2*. `figOrder`
  compares a figure's number to its position and is blind to prose. Grep for `figure [0-9]` after
  any renumber.

- **The index's hours cell must be an integer.** `check-curriculum.py`'s `ROW_RE` ends
  `[0-9]+\s*h`, so `4.5 h` makes the whole row invisible: the page reads as an orphan and the module
  comes up one lesson short. Also note nothing checks the index's hours against the lesson header's
  own estimate — align them by hand.

- **A null result on a test that cannot vary is not evidence.** The firefly probe compared a bright
  pixel at `x` and at `x+1`, which fall in the *same* 2×2 block, so the box downsample returned
  identical output and the test reported a confident 0.000%. Before believing a null, check the
  measurement is capable of a non-null.

- **Measure a tail in the units the data is stored in.** The same probe sampled radii 1 and 2 of a
  half-resolution buffer, both of which land on the delta's core rather than its tail, and reported
  a slope that was an artefact of measuring the spike.

- **A free hardware optimisation can foreclose an option you will want later.** One bilinear tap
  *is* a 2×2 box average — but the averaging happens inside the fetch, so the four texels are gone
  before the shader sees anything. Per-texel firefly weighting therefore becomes impossible, at a
  measured cost of 4× (1.19% drift against 0.30%). That, not kernel width, is the real reason
  shipping engines use Karis's 13-tap downsample.

- **A tuple whose elements are both strings will not tell you when you swap their meanings.**
  `LISTING_META`'s entries are `(status, status_word)` and `listing()` emits
  `<span class="tag {tag}">{word}</span>`. Putting the *language* in the second slot produced a
  correctly-classed pill reading "cpp", beside a `.lang` span already reading "C++" — so the caption
  said path / language / language instead of path / status / language. It ran from **6.9 to 6.12**
  before anyone looked at a pill, because nothing type-checks a pair of strings and
  `check-page.js`'s `unknownTagClasses` validates only the **class**, which was right the whole
  time. When a check validates one half of a pair, the other half is unguarded.

- **Scope a repeated defect by sweeping, not by extrapolating from where you noticed it.** This was
  filed as "two lessons" because that is where it was spotted. A grep over every page found **four**
  builders and 18 entries — and also 16 *deliberate* pills ("excerpt", "the type", "the resolve")
  that are hand-written in body fragments and must not be touched. The counts reconciling exactly
  (3+3+5+7 = 18 generated; the rest authored) is what made it safe to change one set and leave the
  other.

---

## From Lesson 6.14 (antialiasing)

- **A constant that refuses to vary is a derivation you have not done yet.** The probe bisected the
  GGX lobe's half-width and the ratio to α came out at 0.6436 at *every* roughness. Empirical
  constants do not do that. Ten minutes of algebra turned it into
  `sin t = α√((√2−1)/(1−α²))` — exact rather than a small-angle fit, and it showed that the familiar
  "the lobe is about α wide" overstates by 55%.

- **Check that a measurement can produce a non-null result before believing a null one.** This bit
  twice in one lesson, both times as a statistic blind to its own subject:
  - the supersampling test took the **mean over the whole image** and found it already correct at
    1×, because errors of opposite sign cancel across pixels whose phases differ. Aliasing is
    per-pixel, so the statistic has to be.
  - the specular-AA test averaged each method over 64 sub-pixel phases and compared the means — but
    **averaging over phases *is* antialiasing**, so it flattered the single sample to a 4.6%
    "error" at roughness 0.05, where the honest per-phase figure is 230%.

- **"Unchanged" is not "reproduced" when the thing under test can fail without writing.** An audit
  of every page builder reported 4 failures; the real number was 27, because eleven of them *crash*
  before writing and the check only compared the file before and after. **Check the exit status as
  well as the diff.** (This is the same shape as the two items above: a check that cannot observe
  the failure mode it was written for.)

- **A note in LEARNINGS is not a fix.** 6.13 discovered that `check-page.js`'s spill test only
  examined `<text>`, so a legend *box* ran off a viewBox and passed. That was written down here —
  and the very next lesson shipped the identical defect in its own figure. Writing the check took
  five minutes and it caught the live defect within a minute of existing. **When a note describes a
  gap in a tool, the note is the interim measure; closing the gap is the fix.**

- **Prefer making an error impossible to detecting it.** Figure 7's boxes were hand-placed at
  arithmetic offsets and the last one overflowed. Computing the layout from the canvas width removed
  the failure mode rather than catching it.

- **A design is only tested when something it was not written for arrives.** 6.13's ownership rule —
  the stack owns what crosses between stages, a pass owns its own intermediates — placed MSAA's
  multisample target without amendment, and that is worth more evidence than the argument that
  produced it.

- **Check the capability, do not assume it.** `SDL_GPUTextureSupportsSampleCount` exists because
  MSAA support is per *format*: on this machine 8× works on neither the float nor the 8-bit target,
  while 2× and 4× work on both. Falling back with a warning is better behaviour for a quality
  setting than failing to start.

- **A free hardware optimisation can be the thing that makes a feature impossible.** MSAA's
  affordability comes from shading once per primitive per pixel — which is exactly why no sample
  count reaches shading aliasing. The same shape as 6.13's one-tap downsample foreclosing per-texel
  firefly weighting. **Ask what an optimisation removes access to, not just what it saves.**
