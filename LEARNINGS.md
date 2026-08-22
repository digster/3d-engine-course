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
authoring-time verification, not course content, until Module 8's testing lesson.

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

That ownership detail is the whole trick, and it is the smallest possible preview of Module 8: the
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
  in `linear_to_srgb_u8`, but Module 6's HDR pipeline and Module 7's physics would both have found
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
