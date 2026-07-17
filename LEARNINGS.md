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
