# STATE — resume key

This file mirrors the `STATE` block from the master prompt (CLAUDE.md §9). It is the
single source of truth for "where the course is". Update it at the end of every lesson.
To resume: read CLAUDE.md (the binding spec), then this file, then continue from `next`.

```STATE
course: Build a Professional 3D Game Engine (SDL3 + C++20)
version: 1.0
updated: 2026-07-18 (after Lesson 1.3)

conventions:
  world: right-handed, Y-up, -Z forward
  clip: left-handed, +Y up, z in [0,1] (SDL_GPU-fixed; projection absorbs the flip)
  sw-rasterizer: targets SDL_GPU's exact NDC (Module 4 port = API change, not maths change)
  matrices: column vectors, v' = M*v, column-major storage
  winding: CCW = front, cull back (per-pipeline state; set explicitly every time)
  units: 1 unit = 1 metre; radians internally; linear colour in the renderer
  axis colours: x/y/z = red/green/blue (every diagram, no exceptions)
  sdl3: FetchContent, pinned GIT_TAG release-3.4.12 (main is 3.5.0 but unreleased);
        target SDL3::SDL3; SDL_TEST_LIBRARY OFF
  sdl3-api: bool SDL_Init / bool SDL_PollEvent; SDL_CreateWindow(title,w,h,flags) — no x/y;
            SDL_CreateRenderer(window,name); event.key.key (NOT SDL2's keysym.sym);
            #include <SDL3/SDL_main.h> separately; classic main + our own loop
  sdl3-events: SDL_Event is a tagged union — type@0, timestamp@8 identical in every
            variant; sizeof 128 (explicit MSVC/GCC ABI padding). Types grouped by range:
            0x100 quit · 0x2xx window · 0x3xx keyboard · 0x4xx mouse · 0x6xx joystick ·
            0x650 gamepad · 0x8000 user. SDL turns SIGINT/SIGTERM into SDL_EVENT_QUIT.
  sdl3-input: const bool *SDL_GetKeyboardState(int*) — bool in SDL3, NOT SDL2's Uint8;
            indexed by SDL_Scancode; SDL_SCANCODE_COUNT = 512 (A=4, W=26, SPACE=44).
            SDL_GetMouseState returns SDL_MouseButtonFlags + writes float x,y;
            SDL_BUTTON_MASK(n) = 1u<<(n-1), buttons 1..5.
            event.key.{scancode,key,down,repeat} — down/repeat are bool in SDL3.
            Wheel is event-only (no pollable state): event.wheel.{x,y} float, and
            direction == SDL_MOUSEWHEEL_FLIPPED means negate (macOS natural scrolling).
            SDL_PollEvent pumps (-> SDL_WaitEventTimeoutNS(e,0)), so the state arrays are
            only as fresh as the last drain. Focus loss -> SDL_SetKeyboardFocus(NULL) ->
            SDL_ResetKeyboard() sends key-UP EVENTS, so drain-then-sample is what stops
            keys sticking after alt-tab.
  sdl3-time: Uint64 SDL_GetTicks() ms / SDL_GetTicksNS() ns since SDL_Init. Both are
            MONOTONIC — not stated in the header, traced through SDL_GetPerformanceCounter
            to CLOCK_MONOTONIC_RAW / mach_absolute_time / QueryPerformanceCounter.
            SDL_Delay/SDL_DelayNS wait AT LEAST the requested time (measured: Delay(10)
            ~= 11.8 ms); SDL_DelayPrecise busy-waits to get closer.
            SDL_NS_PER_SECOND etc. in SDL_timer.h. SDL_SetRenderVSync(renderer, n) with
            SDL_RENDERER_VSYNC_DISABLED = 0 — can fail per backend, check the return.
            SDL_RenderDebugText/Format = built-in 8x8 ASCII bitmap font in the current
            draw colour; SDL_SetRenderScale also scales its coordinates. Real text = M6.
  time-model: absolute time = Uint64 ns; NEVER float seconds — ulp(86400.0f) = 7.8 ms, so
            86400.0f + 1/500 == 86400.0f and time FREEZES after ~24 h at 500 fps (compiled
            and verified). Only the small delta becomes float, and the ns->s division runs
            in double before narrowing. Milliseconds are too coarse to measure a frame
            (+-20% at 300 fps; truncates to 0 above 1000 fps).
            dt() clamped to 0.25 s, raw_dt() unclamped, was_clamped() reports the lie;
            fps() smoothed over 0.5 s = DISPLAY ONLY.
            dt-scaling is EXACT for constant velocity (v factors out of the sum) and only
            first-order for anything accelerating: explicit Euler error = 0.5*g*T*h,
            proportional to the step, so frame rate is an input to the physics. That is
            the whole argument for 1.4's fixed timestep.
  input-model: poll levels, DERIVE edges (pressed = cur && !prev, released = !cur && prev);
            COPY SDL's array into std::array, never alias the pointer (aliasing makes
            every edge x && !x = false); scancodes for positions, keycodes for symbols
  frame-order: drain events -> clk.tick() -> in.update() -> simulate -> render.
            Drain first because SDL_PollEvent pumps (that is what refreshes the state
            arrays); tick each subsystem exactly once so the whole frame sees one snapshot.
  shaders: HLSL -> SDL_shadercross (3.0.0-preview) -> SPIR-V/DXIL/MSL  [Module 4+]
  cpp: C++20, no exceptions/RTTI in core, snake_case, private members trailing _,
       .hpp + #pragma once, [[nodiscard]], -Wall -Wextra // /W4, all warnings fixed
  build: CMake >= 3.24, out-of-source (build/), 64-bit; two phases (configure, build);
         Debug build = -DCMAKE_BUILD_TYPE=Debug (adds -g); sources listed explicitly
         (never file(GLOB)); target_include_directories(engine PRIVATE src)
  docs-tooling: pages stay self-contained, so the shared <style> AND the shared trailing
         <script> are DUPLICATED into every page — and propagated by
         docs/_template/apply-shared.py, never by hand. Two marker regions:
         <!-- SHARED-CSS:BEGIN/END --> and <!-- SHARED-SCRIPT:BEGIN/END -->.
         Edit lesson-template.html, then stamp. NEVER edit one lesson's copy in
         isolation — that is how the script drifted into 6 versions before 1.2.
         The template carries both marker pairs, so a new lesson started by copying
         it is already opted in; keep the markers when filling the template in.
         Page-specific JS (interactive widgets) goes OUTSIDE the markers — the
         stamper rewrites only what is between them (see 1.2's key-state widget).
         Highlighter word lists: kw is checked before ty, so fundamental types
         (bool/char/int/Uint32/...) belong in CPP_TYPES only. Shell `::` comments
         are anchored to line start (SDL3::SDL3 must not read as a comment).
         `apply-shared.py --check` exits 1 on drift, 2 on a broken template, and
         also lints inline fill= on SVG <text>. Run it before committing docs/.
  docs-verify: serve docs/ over HTTP and drive REAL Chromium (Playwright). The
         preview pane reports impossible computed styles — it will show a dead
         highlighter or broken theme toggle as fine. Strongest highlighter check:
         live textContent after highlighting == DOMParser parse of the same file.

curriculum: 94 lessons, ~433 h, 9 modules
  M0:6  M1:8  M2:12  M3:10  M4:9  M5:10  M6:15  M7:13  M8:11

completed:
  - 0.1  What a Game Engine Actually Is
  - 0.2  How This Course Works
  - 0.3  Setting Up Your Toolchain
  - 0.4  CMake From Zero
  - 0.5  Your First Window
  - 0.6  Reading Headers & the Debugger
  ===> MODULE 0 COMPLETE <===
  - 1.1  Events, Properly
  - 1.2  Input: State vs Events
  - 1.3  Frames, Delta Time, and Why Naive Loops Lie

capabilities:
  - verified C++20 toolchain (MSVC / GCC / Clang), 64-bit
  - portable CMake build, now three translation units; FetchContent SDL3 (release-3.4.12)
  - engine app: 1280x720 window, complete switch-based event dispatch (quit,
    window-close, window-resize), clean shutdown, startup version log
  - input subsystem (src/core/input): keyboard levels + edges addressed by scancode,
    mouse buttons (levels + edges) and cursor position, wheel accumulation with
    flipped-scroll correction; one frame-coherent snapshot published per frame
  - clock subsystem (src/core/clock): monotonic ns timing, clamped dt + raw dt +
    was_clamped(), elapsed(), frame_count(), smoothed fps() for display
  - demo: per-frame box vs per-second box vs dt-scaled bouncing ball, with a runtime
    vsync toggle and a throttle key; on-screen readout via SDL_RenderDebugTextFormat
  - known-and-deliberate defect: simulation still steps by a variable dt (1.4 fixes it)
  - skills: reading SDL headers as source of truth; debugging with lldb/gdb/VS

decisions:
  - input lives in src/core/, not src/platform/ — there is no platform layer until
    Module 5, and input is a state cache rather than a device driver. Revisit at the
    Module 5 refactor. Recorded in ARCHITECTURE.md §2.1.

files:
  /: CLAUDE.md, README.md, ARCHITECTURE.md, LEARNINGS.md, PROMPT.md, LICENSE,
     .gitignore, CMakeLists.txt, STATE.md
  src/: main.cpp
  src/core/: input.hpp, input.cpp, clock.hpp, clock.cpp
  docs/: index.html, conventions.html, math-toolbox.html, cpp-style.html
  docs/lessons/: 00-01-what-is-an-engine.html, 00-02-how-this-course-works.html,
                 00-03-toolchain.html, 00-04-cmake-from-zero.html,
                 00-05-first-window.html, 00-06-headers-and-debugger.html,
                 01-01-events-properly.html, 01-02-input-state-vs-events.html,
                 01-03-delta-time.html
  docs/_template/: lesson-template.html, README.md, apply-shared.py
  memory/: 2026-07-16.md, 2026-07-18.md
  (retired: hello.cpp)

next: 1.4 — The Fixed Timestep with Interpolation, Derived
      (planned filename: docs/lessons/01-04-fixed-timestep.html — 1.3 already links to it)
```
