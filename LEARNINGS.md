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

## SDL_shadercross is pre-1.0

`SDL_shadercross` (HLSL → SPIR-V / DXIL / MSL) is real, actively maintained by libsdl-org, and
the sanctioned path for SDL_GPU shaders — but as of this writing it ships as **3.0.0-preview**.
Present it honestly as preview software; its API may move. Pin the version. Do not imply
stability it does not claim.

It is built on **SPIRV-Cross** (SPIR-V → high-level source) and **DirectXShaderCompiler** (HLSL
→ SPIR-V or DXIL), and offers both a runtime library and an offline CLI. We use the offline CLI
via CMake, with runtime translation mentioned but not relied upon.

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
