# STATE — resume key

This file mirrors the `STATE` block from the master prompt (CLAUDE.md §9). It is the
single source of truth for "where the course is". Update it at the end of every lesson.
To resume: read CLAUDE.md (the binding spec), then this file, then continue from `next`.

```STATE
course: Build a Professional 3D Game Engine (SDL3 + C++20)
version: 1.0
updated: 2026-07-21 (after Lesson 2.1 — Module 2: 1 of 12, 15 of 94 lessons)

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
  frame-order: THE loop, settled as of 1.4 and not changing again:
            drain events -> clk.tick() -> in.update() -> stepper.begin_frame(clk.dt())
            -> while (stepper.next_step()) { previous = current; simulate(current, h); }
            -> alpha = stepper.alpha() -> render(lerp(previous, current, alpha))
            Drain first because SDL_PollEvent pumps (that is what refreshes the state
            arrays); tick each subsystem exactly once so the whole frame sees one snapshot.
  fixed-step: h = 1/60 s default. INVARIANT: after the step loop 0 <= accumulator < h, so
            alpha in [0,1) and a lerp can never extrapolate. The accumulator may be float
            (it is BOUNDED — 1.3's Uint64 rule is about unbounded quantities).
            `previous = current` goes INSIDE the step loop (a frame may run 0..N steps;
            hoisting it out only breaks below the sim rate, i.e. never on the dev machine).
            simulate() receives h, never clk.dt() — the separation as a signature.
            Interpolation renders at exactly T - h: a CONSTANT lag replaces one swinging
            0..h. Smoothness is consistency, not immediacy.
            Spiral of death when cost-per-step / h > 1. Two guards: clock's 0.25 s dt clamp
            (bounds a frame to 15 steps at 60 Hz) + fixed_step's per-frame cap, which
            DRAINS the excess (not just returns — else alpha > 1) and REPORTS the dropped
            time. Past the cap the sim falls behind permanently: slow motion, a real loss.
            Determinism = same binary + same machine + same inputs. NOT cross-platform
            (FMA contraction, x87, libm, vectorisation).
            NEVER interpolate across a teleport — snap previous = current instead. That is
            why the 1.4 demo's box bounces rather than wrapping.
  sdl3-pixels: SDL_CreateTexture(renderer, format, access, w, h) with
            SDL_TEXTUREACCESS_STREAMING for a per-frame buffer.
            SDL_LockTexture(tex, NULL, &pixels, &pitch) = WRITE-ONLY, previous contents
            UNDEFINED — SDL's docs say to keep the master copy app-side, which the
            framebuffer is. The returned pitch MAY EXCEED width*4 (driver row padding),
            so copy ROW BY ROW or the image shears on other people's machines.
            SDL_UpdateTexture is documented as slow / for static textures.
            SDL_RenderTexture(r, tex, NULL, NULL): NULL dst = entire render target, so a
            small framebuffer scales to the window and resize needs NO code.
            SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST) for crisp upscaling
            (SDL_SCALEMODE_PIXELART also exists, 3.4+).
            FORMAT NAMES: "8888" = packed into a native-endian integer, MSB first
            (ARGB8888 = 0xAARRGGBB). "32" = byte order in memory. On little-endian they
            are REVERSED: RGBA32 == ABGR8888 and BGRA32 == ARGB8888 (header-verified).
            We store Uint32 and build with shifts only, so ARGB8888 matches everywhere
            and endianness never enters the engine until an image loader (Module 6).
            Symptom: red/blue swapped with green fine = channel order, never gamma.
  framebuffer: row-major, index = y*width + x. Right = +1, down = +width.
            An x past width is NOT an error — it lands on the next row (candy-striping);
            a stray y leaves the buffer entirely (UB, and the crash is the lucky case).
            put_pixel is bounds-checked; row(y) is the documented fast path (row index
            still clamped). fill_rect CLIPS ONCE then std::fill_n per row. No clear() in
            the demo because the gradient covers every pixel — a real optimisation with a
            real trap attached.
            MEASURED (M4 Pro, median): put_pixel vs row pointer = 5.1x (-O0), 14.8x (-O2);
            rows-outer vs columns-outer = 10.9x @320x180, 32.7x @720p, 48.8x @4K
            (64-byte line = 16 pixels). Rows outer, columns inner, always.
            BENCHMARK TRAP: measuring both loop orders THROUGH put_pixel gave 1.00x —
            its overhead swamped the effect. Make the paths differ ONLY in what is under
            test, and measure at -O2.
  colour: stored channel values are sRGB-ENCODED, not light. VERIFIED BOTH IN PYTHON
            AND IN THE C++: 128 emits 21.6% of white's light; half the light is stored
            as 188; fades 75%->225, 50%->188, 25%->137, 10%->89 (naive 64 for 25% emits
            only 5.1%). Red+green at t=0.5: naive (128,128,0) dark olive vs linear
            (188,188,0) bright yellow. Encode/decode round trip is LOSSLESS for all 256.
            Code budget: evenly spaced light puts 26 of 256 codes in the darkest 10%,
            sRGB puts 90; evenly spaced wastes 128 on the brightest half, sRGB 68.
            Use the EXACT piecewise transform (0.04045 / 12.92 / 0.055 / 2.4), never
            pow(x,2.2). Tabulate DECODE only (256 inputs); encode's input is continuous.
            ALPHA IS COVERAGE, NOT LIGHT — never transfer-function it. mix_linear
            converts three channels and leaves the fourth.
            SAFE on stored values: copy, compare, pick. WRONG: mix, fade, average,
            downscale/mipmap, add light, anti-alias edges.
            NOT YET LINEAR: we convert per-operation (slow + lossy). A real pipeline
            decodes once in, encodes once out, and needs a float/half framebuffer,
            headroom above 1.0, and tonemapping = Module 6.
            Fingerprints: red/blue swapped + green fine = channel order, NOT gamma;
            muddy fades / early-dying fades / darkening mipmaps / fringed text = gamma;
            washed-out milky = the conversion applied twice or backwards.
  vec2: an ARROW — direction and length, NO position. Components are its SHADOWS on
            the axes, which is WHY dot(a,b) = ax*bx + ay*by. Derivation needs only
            "shadows add" + "x_hat . b = |b|cos(alpha) = bx" — NO law of cosines
            (the course assumes no trig beyond basics).
            VERIFIED: (3,4).(4,3) = 24 both ways; |a|=|b|=5, cos=0.96, theta=16.2602 deg,
            shadow 4.8, 4.8*5 = 24. Signs: (6,8)->50 front, (-4,3)->0 perpendicular,
            (-3,-4)->-25 opposite.
            dot(v,v) == length_squared(v). PREFER length_squared for comparisons (sqrt is
            monotonic): square the constant, never root the variable.
            NORMALISE the direction THEN scale — normalised(input)*speed*dt, never
            normalised(input*speed*dt). |(1,-1)| = 1.41421 so a raw diagonal step is
            127.27922 vs 90.00000: Exercise 1.2.3's 41.4%, now closed.
            normalised({0,0}) MUST NOT be 0/0 = NaN; ours returns (0,0), with
            normalised_or(v, fallback) where a direction must exist.
            perpendicular({x,y}) = {-y,x}; dot(v, perpendicular(v)) is EXACTLY 0.
            sizeof(vec2) == 8 -> PASS BY VALUE, never const&.
            HEADER-ONLY on purpose (small/hot/stable, must inline) — and a header-only
            addition needs NO CMakeLists change. Not a general licence.
            Does NOT generalise to 3-D: perpendicular() (a whole plane of them in 3-D);
            the cross product is the 3-D-only operation Module 2 adds.
  collision: a discrete overlap test answers about an INSTANT; collision is a fact about an
            INTERVAL. Overlap window along an axis = size_a + size_b (Minkowski sum), so a
            naive test is guaranteed only while |v_axis|*h < size_a + size_b. Ours: 8 px window,
            ball capped 260 px/s -> safe to 480 px/s at 60 Hz (bug UNREACHABLE and still there),
            240 at 30 Hz (intermittent), 80 at 10 Hz (fails on the opening serve).
            SWEPT FIX: t = (face - lead_from)/(lead_to - lead_from); require the edge BEGAN near
            and ENDED far (also makes the divisor non-zero by construction); interpolate the
            other axis AT t, not at the endpoint; then spend the remaining (1-t) of the step.
            VERIFIED: ball at x=16, -105 px/s, h=0.1, face x=14 -> t=0.190476, speed 105->112,
            vel (109.5525,-23.2861), final x=22.8685; naive test on the same step says NO HIT.
            One impact per step for now — Module 7 iterates until the budget is spent.
  reflect: reflect(v,n) = v - 2*dot(v,n)*n, derived as "subtract the shadow twice". n MUST be
            unit (project_onto's /|n|^2 is what is missing); a length-k normal scales the
            correction by k^2 and the ball silently gains/loses energy. Assert |reflect|==|v|.
            VERIFIED (3,4) off (0,-1) -> (3,-4); (0,5) off a 45-deg wall -> (5,0).
            A bounce needs BOTH velocity turned AND position mirrored back inside: velocity-only
            leaves the ball outside for a step, position-only sticks it to the wall.
            Framebuffer normals point INTO the court: ceiling (0,+1), floor (0,-1) — +y is down.
  determinism: same binary + machine + seed + inputs + STEP SIZE. Verified bit-identical over
            20000 steps; the same seed at 60 vs 120 Hz diverges within 2 simulated seconds,
            because h is an input too. PRNG seed lives IN the state (not SDL_rand's hidden
            global) or the sim is not a function of its inputs. xorshift32: seed 0 is a fixed
            point — guard it; take the top 24 bits for an exact float in [0,1).
  engine/game: src/game/ is the first NOT-engine directory. Test: could a different game use
            this unchanged? game -> engine only, never back. Enforced by discipline today, by
            the compiler in Module 5. pong.hpp FORWARD-DECLARES engine::framebuffer rather than
            including it (include what you use, forward declare what you mention).
  lines: endpoint-INCLUSIVE at BOTH ends (so a rectangle's corners close). Bresenham,
            integer, all 8 octants. Ties break toward NE (E >= dx, not >).
            Derivation: e = y_true - y_plotted; e += m each step; e >= 1/2 -> step minor,
            e -= 1. Scale by 2*dx to clear denominators (comparisons survive multiplication
            by a positive constant): E += 2dy, test E >= dx, E -= 2dx. All integers, E=0.
            TERMINATION PROOF: with dx=|Dx|>=0, dy=-|Dy|<=0, both tests failing needs
            dx < 2*err < dy <= 0 <= dx, i.e. dx < dx. So one always fires.
            TIE THEOREM: with p = major/gcd(major,minor), an exact tie exists IFF p is even
            — and EXACTLY those lines are asymmetric under endpoint swap (verified over
            23103 lines, 7692 asymmetric, zero disagreements). Slope 1/2 ties constantly;
            3/5 and 45 degrees never do. This is WHY 2.2 needs a fill rule.
            BENCHMARK (M4 Pro, clang 21, -O2, ns/px, stepping only): Bresenham compact 1.32,
            Bresenham major-axis 0.74, DDA lround 0.60, DDA trunc 0.65. DDA IS FASTER —
            the folklore is inverted. Cost is the two data-dependent BRANCHES, not floats
            (swapping lround for truncation changes nothing). We ship Bresenham for
            EXACTNESS (integers are bit-identical everywhere; 1.8 showed floats are not)
            and because its error term IS 2.2's edge function. Lines are not the hot path.
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
         Use getBoundingClientRect(), NOT getBBox(), for spill/collision checks —
         getBBox is in LOCAL coords, so anything inside a <g transform> is compared
         against the wrong origin (three false positives in 1.8).
         Verify a renderer by its POSITIVE signal: .katex count == .eq count, and
         exactly two script[src*=katex] tags. "No console errors" passed for six
         lessons while KaTeX was entirely absent.
         Listings are SPLICED from the real files (@@LISTING:path@@ + a small script),
         never retyped — makes drift from the compiled source impossible.
  katex-trap: the KaTeX loader used to sit just BELOW <!-- SHARED-SCRIPT:END --> in the
         template, so apply-shared.py never propagated it and every lesson shipped
         without a maths renderer. Invisible because raw TeX is also the documented
         CDN-unreachable fallback, and 1.1-1.7 have zero .eq blocks so nothing failed.
         FIXED in 1.8: moved inside the region, duplicate standalone copies removed
         from conventions.html and math-toolbox.html. ANYTHING every page needs goes
         BETWEEN the markers.

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
  - 1.4  The Fixed Timestep with Interpolation, Derived
  - 1.5  The Framebuffer: Your First Owned Pixels
  - 1.6  Colour, and an Honest Teaser of sRGB
  - 1.7  2D Vectors, Geometrically
  - 1.8  Checkpoint: Pong
  ===> MODULE 1 COMPLETE <===
  - 2.1  Lines: DDA, then Bresenham

capabilities:
  - verified C++20 toolchain (MSVC / GCC / Clang), 64-bit
  - portable CMake build, now six translation units; FetchContent SDL3 (release-3.4.12)
  - engine app: 1280x720 window, complete switch-based event dispatch (quit,
    window-close, window-resize), clean shutdown, startup version log
  - input subsystem (src/core/input): keyboard levels + edges addressed by scancode,
    mouse buttons (levels + edges) and cursor position, wheel accumulation with
    flipped-scroll correction; one frame-coherent snapshot published per frame
  - clock subsystem (src/core/clock): monotonic ns timing, clamped dt + raw dt +
    was_clamped(), elapsed(), frame_count(), smoothed fps() for display
  - fixed_step subsystem (src/core/fixed_step): accumulator, alpha, per-frame step cap
    with drop reporting, runtime set_rate
  - THE loop: fixed-timestep simulation with render interpolation, spiral-guarded
  - demo: variable-dt vs fixed-raw vs fixed-interpolated boxes + a bouncing ball whose
    apexes are now identical at 2000 / 240 / 60 / 20 fps; sim-rate keys [1-4], vsync
    toggle, throttle; on-screen readout via SDL_RenderDebugTextFormat
  - framebuffer subsystem (src/gfx/framebuffer): 320x180 ARGB8888 CPU buffer,
    clear / put_pixel / pixel_at / fill_rect / row(), pack_argb
  - presentation: streaming texture, locked + row-wise upload honouring the driver pitch,
    NEAREST 4x upscale; render resolution independent of window size
  - demo now drawn ENTIRELY into our own pixels: gradient background (two addressing
    paths, timed on screen via [G]), 1.4's three timing boxes and ball, and a pixel trail
    with one dot per simulation step
  - colour subsystem (src/gfx/colour): pack/unpack, exact sRGB transfer functions with a
    256-entry decode LUT, mix_encoded vs mix_linear
  - demo: a comparison board — black-white and red-green ramps mixed both ways, drawn
    TOUCHING so the seam shows the error — plus the bouncing ball, whose trail fade rule
    switches with [M]
  - maths: src/math/vec2.hpp (header-only) — arithmetic, length/length_squared,
    normalised(+_or), dot, perpendicular, project_onto, reflect, lerp, distance
  - game: src/game/pong.{hpp,cpp} — a COMPLETE, WINNABLE Pong. Swept collision with a
    runtime toggle back to the naive test so tunnelling can be watched happening;
    angle-from-hit-position paddles; a beatable AI (0.82 speed, chase-only-when-incoming,
    3 px deadzone); deterministic in-state xorshift32; 3x5 bitmap digits for the score;
    teleport-aware interpolation. Verified: perfect tracker beats the AI 11-4, longest
    rally 61 hits, ball reaches the 260 px/s cap, never leaves the court in 300k steps.
  - main.cpp is now a HOST only: window, framebuffer, loop, input->intent, upload, HUD.
    The rules of Pong are not in it and could not be.
  - RASTERIZER (src/gfx/raster) — framebuffer sets ONE pixel; raster decides WHICH pixels
    a SHAPE is made of. draw_line = integer Bresenham, all 8 octants, endpoint-inclusive,
    VERIFIED pixel-identical to a reflect-in/out first-octant midpoint reference over
    1600 lines. draw_line_dda and draw_line_naive kept so the argument can be reproduced.
  - demo: rotating 32-spoke fan (crosses every octant; steep=coral, shallow=green) +
    an 8x magnified pixel inspector that reads back the REAL routine's output, algorithm
    switchable [1][2][3], live pixel count and per-fan timing. Naive lights 1483 px where
    DDA/Bresenham light 2081. Pong preserved on [Tab].
  - known-and-deliberate: no line clipping — put_pixel discards out-of-range writes, so an
    off-screen line still costs a full walk (Exercise 2.1.4; Module 3 makes clipping
    mandatory for CORRECTNESS, not speed). No anti-aliasing (Ex 2.1.5, Module 6).
    Two demos in one executable is deliberately awkward — it is the argument for Module 5's
    demos/ split, accumulating where it can be felt;
    explicit Euler still gains energy — but identically everywhere,
    at a rate set by h, which is ours to choose (Module 7 fixes the integrator);
    colour converts per-operation rather than at the pipeline edges (Module 6);
    the debug-text overlay is the only thing on screen SDL still draws;
    ONE impact resolved per simulation step (Module 7 iterates until the budget is spent);
    the naive DDA line routine was RETIRED with 1.7's demo — Lesson 2.1 derives line
    drawing properly and puts it in gfx/ (nothing draws lines at the moment)
  - skills: reading SDL headers as source of truth; debugging with lldb/gdb/VS

decisions:
  - input lives in src/core/, not src/platform/ — there is no platform layer until
    Module 5, and input is a state cache rather than a device driver. Revisit at the
    Module 5 refactor. Recorded in ARCHITECTURE.md §2.1.
  - src/game/ created in 1.8, three modules before the Module 5 refactor needs it. The
    boundary costs nothing now and decides how hard that refactor is. ARCHITECTURE.md §2.1.1.
  - src/gfx/raster.hpp forward-declares engine::framebuffer rather than including it —
    the physical-design habit from 1.8, now applied by default in gfx/.
  - draw_line stays Bresenham despite MEASURING SLOWER than DDA. Reasons recorded above
    and in the lesson; revisit with evidence, not deference.
  - the naive collision test is KEPT in the shipped code behind state::swept_collision
    rather than deleted, so the failure can be reproduced on demand (pedagogy §5:
    show the artifact). It is dead weight only if you think a bug you can summon is
    worth less than one you can only describe.

files:
  /: CLAUDE.md, README.md, ARCHITECTURE.md, LEARNINGS.md, PROMPT.md, LICENSE,
     .gitignore, CMakeLists.txt, STATE.md
  src/: main.cpp
  src/core/: input.hpp, input.cpp, clock.hpp, clock.cpp,
            fixed_step.hpp, fixed_step.cpp
  src/gfx/: colour.hpp, colour.cpp, framebuffer.hpp, framebuffer.cpp,
            raster.hpp, raster.cpp
  src/math/: vec2.hpp
  src/game/: pong.hpp, pong.cpp
  docs/: index.html, conventions.html, math-toolbox.html, cpp-style.html
  docs/lessons/: 00-01-what-is-an-engine.html, 00-02-how-this-course-works.html,
                 00-03-toolchain.html, 00-04-cmake-from-zero.html,
                 00-05-first-window.html, 00-06-headers-and-debugger.html,
                 01-01-events-properly.html, 01-02-input-state-vs-events.html,
                 01-03-delta-time.html, 01-04-fixed-timestep.html,
                 01-05-framebuffer.html, 01-06-colour.html,
                 01-07-vectors-2d.html, 01-08-pong.html,
                 02-01-lines.html
  docs/_template/: lesson-template.html, README.md, apply-shared.py, check-page.js
  memory/: 2026-07-16.md, 2026-07-18.md
  (retired: hello.cpp)

next: 2.2 — The Triangle: Edge Functions
      (planned filename: docs/lessons/02-02-triangle-edge-functions.html — 2.1 links to it)
      A line divides the plane; Bresenham's error term has been answering "which side?"
      all along. Ask it for three edges at once and a point is inside iff all three agree
      — which is a filled-shape rasterizer, and is how GPUs actually do it. Also resolves
      2.1's loose end: edge functions come from the edge's GEOMETRY, not a traversal, so
      the tie-breaking that makes Bresenham asymmetric becomes a FILL RULE that two
      adjacent triangles cannot disagree about.
```
