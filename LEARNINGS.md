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
