# STATE — resume key

This file mirrors the `STATE` block from the master prompt (CLAUDE.md §9). It is the
single source of truth for "where the course is". Update it at the end of every lesson.
To resume: read CLAUDE.md (the binding spec), then this file, then continue from `next`.

```STATE
course: Build a Professional 3D Game Engine (SDL3 + C++20)
version: 1.0
updated: 2026-07-24 (after Lesson 2.5 — Module 2: 5 of 12, 19 of 94 lessons)

conventions:
  world: right-handed, Y-up, -Z forward
  clip: left-handed, +Y up, z in [0,1] (SDL_GPU-fixed; projection absorbs the flip)
  sw-rasterizer: targets SDL_GPU's exact NDC (Module 4 port = API change, not maths change)
  matrices: column vectors, v' = M*v, COLUMN-MAJOR storage.
        A matrix IS the pair of answers to "where do the basis vectors land":
        c0 = image of (1,0), c1 = image of (0,1). Everything else is a
        consequence, not a rule to memorise.
        WRITTEN IN ROWS, STORED IN COLUMNS. The first two floats in memory are
        the LEFT COLUMN read downwards, NOT the top row. Getting this backwards
        transposes the matrix, which turns a rotation into the opposite rotation
        AND a shear into the wrong axis — if both break at once it is ONE layout
        bug, not two.
        A*B means B FIRST. Forced by (A*B)v == A(Bv), not a convention to look
        up. Row-vector codebases (v' = v*M) read the other way; mixing the two
        gives code that is transposed AND backwards.
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
  triangles: edge_function(a,b,p) = (bx-ax)(py-ay) - (by-ay)(px-ax) = z of the 2-D
            cross product = dot(P-A, perpendicular(B-A)). SIGN = which side (0 = exactly
            on the line); MAGNITUDE = 2 * area of triangle ABP.
            The three edge functions SUM to the total area, for points INSIDE AND OUTSIDE
            — verified; a superb debug assertion, and 2.3's barycentric weights unnormalised.
            AFFINE in the pixel, so stepping is constant: dE/dx = ay-by, dE/dy = bx-ax.
            fill = bbox (clipped) + incremental stepping + top-left rule. MEASURED 75x
            faster than direct full-buffer evaluation (5763 ms -> 77 ms), and pixel-identical
            to it over 144 triangles incl. many straddling the buffer edge.
            Inner loop writes through fb.row(y) — safe because the bbox was clipped first.
            edge_function OVERFLOWS int32 past ~+-16000 coords. Signed overflow is UB, so
            this is documented in the header; Module 3's clipping keeps us inside it.
  winding-screen: "CCW = front" is an NDC statement. In the FRAMEBUFFER (+y down) the
            viewport flip reverses it: screen-CCW has NEGATIVE signed area. MEASURED:
            (5,0),(0,10),(10,10) -> -100; reversed -> +100. fill_triangle accepts EITHER
            winding (measures area once, swaps two vertices if negative) so nothing
            vanishes; culling by sign is Lesson 3.4's job, in its own space.
            Zero area = collinear = draws nothing. That check is LOAD-BEARING: the fill
            rule's proof needs a non-degenerate edge.
  fill-rule: top-left. For a triangle oriented to POSITIVE area:
            top edge = (dy == 0 && dx > 0); left edge = (dy < 0). Bias -1 on the others,
            folded into the loop's starting value, so it costs NOTHING per pixel.
            WHY IT WORKS WITHOUT COORDINATION: two triangles share an edge by traversing
            it OPPOSITELY, so any rule phrased on edge direction answers oppositely.
            VERIFIED: quad + 12-triangle fan, 0 px drawn twice, 0 interior gaps; with the
            rule off the same quad double-draws its whole seam.
            COVERAGE BECOMES HALF-OPEN — a lone 37x37 quad loses exactly 73 px (bottom row
            + right column - shared corner = 37+37-1). Nothing else dropped, nothing added.
            That is the [start, end) trade: half-open tiles, closed cannot.
            Double-draws only hit pixels whose centres are EXACTLY on the seam, so an
            axis-aligned or 45-degree edge fails TOTALLY (40 px on a 40 px seam) while a
            rotated one loses 2-3 stray pixels. Common geometry is the catastrophic case.
  barycentric: w_i = area of the sub-triangle OPPOSITE v_i, over the total.
            w0 uses the edge v1->v2. THE PAIRING IS THE BUG: a rotated pairing still
            sums to 1 and still looks plausible — VERIFIED, it reconstructs (5.2,5.8)
            instead of (5,5) for the standard example. ALWAYS assert RECONSTRUCTION
            (w0*v0 + w1*v1 + w2*v2 == P), never just the sum: the sum passes for all
            three wrong rotations, reconstruction for exactly one.
            e0+e1+e2 == area EXACTLY in integers, for EVERY P in the plane (inside or
            outside) — the P terms cancel symbolically. Free assertion; leave it in.
            Geometry: 1 at its own vertex, 0 on the opposite edge, 1/3 at the centroid,
            NEGATIVE outside. "All three >= 0" IS 2.2's inside test divided by a
            positive constant — verified identical over 5041 points.
            Constant weight = a line PARALLEL to the opposite edge, evenly spaced,
            because that edge is a fixed base so equal area means equal height.
            MEASURED: w0 varies by EXACTLY 0 along such a line.
            Interpolation with these weights is the UNIQUE affine function matching the
            three corners (3 coefficients, 3 independent conditions) — not merely a
            reasonable blend. Affine in SCREEN space, which stops being surface-correct
            under perspective: that is Lesson 3.2's 1/w trick, and the artifact is
            swimming textures.
            PRECISION (measured over 32761 points): worst |sum-1| = 2.4e-7 — one
            rounding, NOT accumulation, because only the final division is inexact.
            But the sum is bitwise 1.0f only ~85% of the time. NEVER compare weights
            for equality; test the INTEGER edge values, where "on the edge" is == 0.
            USE UNBIASED edge values for interpolation — the top-left rule's -1 bias is
            for coverage only and shifts weights by a fraction of a pixel. 2.4 must
            carry both sets.
            Degenerate (collinear) triangle -> all zeros, the one case where the weights
            do not sum to 1. Documented in the header; a NaN here would spread silently.
  interpolation: a(P) = w0*a0 + w1*a1 + w2*a2. The UNIQUE affine function through the
            three corners — 3 coefficients, 3 conditions, one solution — so there is
            nothing to tune and no better scheme to find. Works for ANY payload you can
            scale and add; the rasterizer never learns what it carries, which is also why
            it returns an interpolated NORMAL that is no longer unit length (renormalise
            per pixel, 3.6) and a DEPTH that is not perspective-correct (3.2).
            VERIFIED: an attribute that is itself affine in position (3x - 2y + 7)
            reproduces itself over 3721 points, worst error 3.05e-5 on values up to 120.
            UNBIAS BEFORE INTERPOLATING — the single silent trap of 2.4. The top-left
            rule's -1 is a COVERAGE decision. Left in the accumulators when you divide it
            (a) breaks sum-to-one by (b0+b1+b2)/2A, and EXACTLY one or two of the three
            biases is -1, never zero of them and never all three — because the three
            directed edges' dy sum to 0 and cannot all be 0, so at least one is negative
            (a left edge, bias 0) and at least one positive (bias -1). Brute-forced over
            all 495,648 non-degenerate triangles in a 9x9 grid: 247,824 sum to -1,
            247,824 to -2, none to anything else.
            (b) TRANSLATES the whole attribute field — rigidly, not tilted — by
                displacement = 1 / |edge opposite that weight's vertex|, in PIXELS,
                perpendicular to that edge. The area CANCELS. Derived from
                |grad w0| = |e| / 2A; both verified numerically.
            So the bug lives in SMALL triangles, i.e. dense meshes. On a smooth attribute
            it is a fraction of one colour level and invisible. On a QUANTISED one (texel
            index, stripe, checker cell) a sub-pixel shift at a threshold flips WHOLE
            pixels: measured 0 to 15 wrong out of 52 on ONE triangle with ONE fixed
            0.088 px error, depending only on where the thresholds happened to fall.
            THEREFORE: derive the magnitude of a sub-pixel error, never look for it.
            "It looked fine when I tried it" is a sample of one from a distribution
            containing both 0 and 15.
            The fix is one exact integer subtraction: the accumulator holds E + bias and
            bias is a known constant, so E = accumulator - bias, with no drift to unwind.
            Coverage keeps using the BIASED value in the same loop, one line above.
  colour-interpolation: DECODE TO LINEAR, INTERPOLATE, ENCODE ONCE. A stored channel is
            a CODE for a quantity of light, not the quantity. MEASURED on an R/G/B
            triangle: centre pixel (156,156,156) in linear light vs (85,85,85) on stored
            values — 0.3325 vs 0.0908 of white's light, a factor of 3.66, so the naive
            blend emits 27% of what it claims. R/G edge midpoint 188 vs 128, matching
            Lesson 1.6 exactly (same arithmetic, arriving through a triangle).
            At w=(0.4,0.3,0.3): (170,149,149) correct vs (102,77,77) naive.
            HONEST EXCEPTION: an artist's UI-gradient swatches may genuinely mean the
            encoded blend. Our vertex colours become LIGHTING RESULTS in 3.6, and a
            quantity of light is averaged as light. Ask what the number measures.
  stepping-precision: integer accumulators are BIT-EXACT against direct evaluation
            (worst difference 0 over 4000 steps). Float-stepped weights drift only
            4.94e-6 over 4000 adds = 0.0013 of an 8-bit colour level = 0.020 texels of a
            4096 texture; carried across 900 rows without a reset, 4.26e-5 = 0.011 levels.
            SO DO NOT OVERSELL IT: the case for integers is exactness, reproducibility
            and zero cost — NOT a visible artifact. Overselling a real principle with a
            fake symptom teaches students to distrust the principle. Where it WILL matter
            is z-buffer comparisons against tiny differences (3.1).
  linear-algebra: LINEAR means T(a+b)=T(a)+T(b) AND T(ca)=cT(a). Consequence:
        T(v) = x*T(i) + y*T(j), so TWO ARROWS DETERMINE EVERYTHING — not
        approximately, exactly. Grid picture: straight stays straight, parallel
        stays parallel and evenly spaced, and THE ORIGIN NEVER MOVES (put c=0 in
        the scaling rule). That last one is a one-line proof that NO 2x2 CAN
        TRANSLATE. Left open ON PURPOSE — 2.7 earns the fourth component from it,
        and the payoff dies if the gap is closed early.
        ROTATION IS DERIVED, never looked up: i -> (cos,sin) because that is what
        sin/cos MEAN; j is a quarter turn ahead so j -> (-sin,cos). The minus sign
        lands on c1.x, the top-RIGHT element as written.
        R(a)*R(b) == R(a+b) VERIFIED over 1716 pairs (worst 2.98e-7) — and
        multiplying it out DERIVES the angle-addition formulas. Spot-checked:
        top-left of R(.6)*R(.9) == cos(1.5) == 0.070737.
        DETERMINANT = signed area factor = edge_function with its first point at
        the ORIGIN = the 2-D cross product. VERIFIED identical over 28,561 integer
        matrices, 0 mismatches. det>0 orientation kept; det<0 FLIPPED, so a
        negative determinant turns every front face into a back face (3.4) —
        verified: the standard triangle's doubled signed area goes +60 -> -60
        under scale(-1,1), and a det of 1.0200 takes +60 -> +61.2, ratio exactly
        1.0200. det==0 folds the plane onto a LINE: no inverse, and information is
        genuinely gone (verified — (1,0) and (-1,1) both map to (2,1), and all
        1681 sampled inputs land on the single line x = 2y).
        det IS MULTIPLICATIVE. Shear has det 1 — it slants without changing area.
        MEASURED against our own rasterizer (140-px square, fill + count pixels):
        identity/scale/shear EXACT, rotation -0.26%, rot*scale -0.06%. The
        residual is the fill rule counting pixel CENTRES, so it scales with
        PERIMETER and falls as ~1/side (demo at 44 px sees ~1%). Axis-aligned is
        exact at any size — the top-left rule paying off somewhere unexpected.
        ORDER MATTERS: R*S and S*R map (1,0) to (0,2) vs (0,1) — but BOTH have
        det 2. Same area factor, different shape; the determinant is a summary and
        summaries lose information. Uniform scale DOES commute with rotation.
        Composition is ALWAYS associative even though it never commutes.
        transpose == inverse ONLY for a rotation (orthonormal). VERIFIED false for
        scale(2,0.5). Using transpose as a cheap inverse is the worst kind of wrong
        — it looks almost right. 3.6 meets this properly with normals.
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
  - 2.2  The Triangle: Edge Functions
  - 2.3  Barycentric Coordinates from Signed Areas
  - 2.4  Interpolating Attributes Across a Triangle
  - 2.5  Matrices as Basis Transforms

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
  - maths: src/math/mat2.hpp (header-only, NEW in 2.5) — two vec2 COLUMNS, so
    column-major layout is a CONSEQUENCE of naming the right things rather than a
    convention to enforce. Verified: raw floats come out 2,0,1,3 and sizeof is
    exactly 4 floats with no padding. operator*(mat2,vec2) = c0*x + c1*y and
    operator*(mat2,mat2) = {a*b.c0, a*b.c1} — both are their derivations
    transcribed, two lines total, with nothing in them to get backwards.
    at(row,col) bridges written notation and column storage (ROW first,
    deliberately). identity / rotation / scale / shear / determinant / transpose /
    inverse; inverse returns ZEROS when det==0, never NaN — same discipline as
    normalised() and barycentric_at.
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
  - raster: edge_function (constexpr), fill_triangle (bbox + incremental + top-left rule,
    either winding, degenerate- and offscreen-safe), draw_triangle (wireframe),
    struct barycentric + barycentric_at (one reciprocal, three multiplies)
  - raster 2.4: is_top_left is now PUBLIC (interpolation has to UNDO the bias, so the
    rule producing it must be inspectable — and the demo must ask the engine's question,
    not a re-typed copy of it). struct vertex {x, y, colour} — bundling position with
    attributes is CORRECTNESS, not tidiness: reorientation does std::swap(v1,v2) and the
    colour moves with the position it belongs to. enum class blend_space {linear,
    encoded}, defaulting to linear. fill_triangle OVERLOAD taking three vertices =
    Gouraud shading, unbiased weights, linear light. VERIFIED: identical coverage to the
    flat fill over 400 random triangles (0 mismatches), and identical output for both
    windings over 200 (0 differing pixels).
  - raster internals: fill_setup + prepare_fill — bbox, biases, steps and starting values
    extracted so two fills (and Module 3's more) share ONE copy of the subtle part.
    ORIENTATION IS DELIBERATELY LEFT TO THE CALLER: only the caller knows what a vertex
    carries. Private rgb3 + corner_in/pixel_from: NOT linear_rgb, because under
    blend_space::encoded the numbers are 0..255 stored values and a type called
    linear_rgb holding those is a lie that compiles.
  - colour 2.4: struct linear_rgb + to_linear / to_encoded — the type that keeps light
    arithmetic out of stored values. No alpha in it, deliberately (coverage is not light).
    Values above 1.0 permitted; to_encoded clamps only because 8 bits has nowhere to put
    the excess (Module 6 HDR depends on that not being an error).
  - COST, measured on a 20,760-px triangle x400, release: flat fill 20.2 us, encoded
    blend 57.9 us, linear blend 232.0 us = 11.2 ns/px = 11.5x the flat fill. Nearly all
    of it is pow() in linear_to_srgb; decode is a 256-entry table, encode has no obvious
    one. Corner colours decoded ONCE per triangle and 1/area hoisted — without those it
    is worse for no gain. A 4096-entry encode LUT is under 0.4 levels of error everywhere
    (slope 12.92*255 = 3295 levels per unit of light near black) — Exercise 2.4.3.
    NOT done on purpose: 232 us of a 16,600 us budget is not a problem we have, and the
    whole cost vanishes in Module 4 where the GPU encodes sRGB on write for free.
  - demo: [Tab] now CYCLES THREE demos. Triangles (2.2): a rotating triangle in filled /
    wireframe / HALF-PLANE view (colour by how many of the three edge tests a pixel passes
    — Figure 1 rendered live from the shipped code), plus a coverage COUNTER proving the
    fill rule: an axis-aligned square split by its diagonal, green = drawn once, red =
    twice, [R] toggles the rule (0 px -> 40 px).
    Three demos in one binary is now visibly too many: that IS the Module 5 argument.
    Triangle views extended for 2.3: [4] w0 as a ramp INCLUDING the negative region
    outside in red, [5] the iso-line grid (three families of parallel lines). Both
    carry a mouse probe that draws the three sub-triangles — using the same pairing
    barycentric_at uses, so the picture and the maths cannot drift — and prints the
    three weights with their sum.
  - demo: rotating 32-spoke fan (crosses every octant; steep=coral, shallow=green) +
    an 8x magnified pixel inspector that reads back the REAL routine's output, algorithm
    switchable [1][2][3], live pixel count and per-fan timing. Naive lights 1483 px where
    DDA/Bresenham light 2081. Pong preserved on [Tab].
  - demo 2.4: [6] GOURAUD — R/G/B corners, [M] switches blend space, and the centre pixel
    is READ BACK out of the framebuffer and printed (156 vs 85), so the HUD reports what
    was drawn rather than what we believe was drawn. [7] UV CHECKER — the same loop
    carrying (u,v) instead, written out longhand in main.cpp so it can be compared line
    for line against the engine's, and so the by-hand attribute swap on reorientation is
    visible. Right panel is view-dependent: 2.2's coverage counter for [1]-[5], 2.4's
    BIAS MAGNIFIER for [6]/[7] — one 11-px triangle drawn twice at 5x, unbiased vs biased,
    disagreeing cells ringed and counted, band count swept with [ and ] so the count
    visibly jumps between 0 and 15 for one unchanging error. The sweep IS the lesson;
    a fixed impressive number would have taught the wrong thing.
  - demo 2.5: [Tab] now cycles FOUR demos — basis (2.5) / triangles (2.2-2.4) /
    lines (2.1) / Pong (1.8). Four in one binary is well past awkward; that IS the
    Module 5 argument and it is now loud.
    Basis view: the image of the integer lattice (the i==0 lines are drawn too and
    BRIGHTER — omitting them was a real bug, see below), the transformed unit
    square, the two basis vectors as red/green arrows, and an asymmetric F glyph.
    [Z] cycles identity / rotation / scale / shear / R*S / S*R; [,] [.] adjust the
    one parameter; [0] resets; [Space] animates. The two composition modes GHOST
    THE OTHER ORDER as a gold outline, so non-commutativity is watched rather than
    described. The readout prints the matrix BOTH as written and as stored, the
    determinant, and the unit square's area MEASURED by rasterising into a scratch
    buffer and counting pixels.
  - known-and-deliberate: NO TRANSLATION — every linear map fixes the origin, so no
    2x2 can express it. Left broken ON PURPOSE; 2.7 earns the fourth component from
    exactly this gap, and Exercise 2.5.3 (rotate about a point, the painful way) is
    the setup. No mat3/mat4 yet (2.6). rotation() is not constexpr because std::cos
    is not until C++26.
  - known-and-deliberate: no perspective correction — everything is affine in SCREEN
    space, exact for a flat triangle and wrong the moment depth varies (3.2, and the
    artifact is swimming textures); no depth buffer (3.1);
    the encode pow is NOT tabulated (Ex 2.4.3), number written down instead;
    the demo's uv loop is a SECOND copy of the fill loop — deliberate pressure, resolved
    in stages: Module 3 grows `vertex` as attributes earn their place, Module 4 hands it
    to GPU varyings. Two copies is not yet evidence; four would be;
    no line clipping — put_pixel discards out-of-range writes, so an
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
  - fill_triangle does NOT cull by winding, deliberately. Module 2 rasterises in
    framebuffer space where the sign is flipped relative to the NDC convention, and a
    fill that silently dropped "backwards" triangles would be indistinguishable from a
    bug. Culling is Lesson 3.4's, made in NDC where "CCW = front" actually means something.
  - the fill rule's -1 bias is folded into the loop's starting value rather than tested
    per pixel, so correctness here costs zero instructions in the inner loop.
  - src/gfx/raster.hpp forward-declares engine::framebuffer rather than including it —
    the physical-design habit from 1.8, now applied by default in gfx/.
  - draw_line stays Bresenham despite MEASURING SLOWER than DDA. Reasons recorded above
    and in the lesson; revisit with evidence, not deference.
  - struct vertex bundles position WITH attributes so that reorientation cannot leave
    them behind. Chosen for correctness, not tidiness: swapping loose coordinates and
    forgetting loose colours produces a triangle of exactly the right shape, in the right
    place, shaded one corner out of step — and it fires for only ONE winding, so a
    spinning triangle looks right half the time and a static test scene may never show it.
  - prepare_fill does NOT orient the triangle, by design. Orientation moves vertices and
    a vertex carries attributes, so only the caller can do it. Everything AFTER
    orientation is mechanical and identical for every fill — that is what gets shared.
    Rule applied: not "never repeat yourself" but NEVER REPEAT SOMETHING SUBTLE.
  - is_top_left promoted from raster.cpp's anonymous namespace to the header. The bias is
    no longer an internal coverage detail once interpolation must undo it; and a rule you
    cannot inspect is a rule you cannot check (the magnifier would otherwise be comparing
    the demo against itself). Same discipline as 2.1's pixel inspector.
  - blend_space::encoded SHIPS, defaulting off. Third time this bargain has been made
    (draw_line_naive 2.1, pong swept_collision 1.8): a failure you can summon with one
    keystroke teaches more than a paragraph describing it. The WRONG option must be
    asked for by name; the right one is what you get by not thinking.
  - rgb3 is a separate type from linear_rgb despite identical layout. Name a type after
    what it IS, not what it is shaped like — reusing linear_rgb for encoded 0..255 values
    would be exactly the confusion Lesson 1.6 exists to prevent.
  - the encode pow is NOT replaced with a LUT yet. Measured (11.2 ns/px), bounded
    (< 0.4 levels for 4096 entries), written down, and deferred — optimising a 232 us
    cost inside a 16.6 ms budget would be optimising by reflex, which is precisely what
    Module 3's profiling lesson teaches against.
  - mat2 stores two vec2 COLUMNS rather than float[4]. The columns are the images
    of the basis vectors — the whole lesson — so the type makes the idea structural
    and gets column-major layout for free rather than by decree.
  - no constructors on the maths types, deliberately: default member initialisers
    give a safe default AND keep the struct an aggregate, preserving brace init,
    constexpr, and a layout guaranteed to match what it looks like — which matters
    the moment Module 4 uploads one as raw bytes.
  - at(row,col) takes ROW first even though the lookup goes to the column first. It
    exists so code can be read against a written derivation without transposing in
    your head. No bounds check: indices are literals at every call site, a DIFFERENT
    trade from put_pixel whose indices come from arithmetic that can genuinely go
    out of range. The rule is not "always check" — it is "check where the input can
    actually be wrong".
  - the demo's y-flip lives in ONE function (to_screen). mat2 is +y up (maths
    convention, rotation is CCW); the framebuffer is +y down. One minus sign at the
    boundary — NOT negations sprinkled through drawing code, and NOT baked into the
    matrices, which would leave "which way does rotation() turn?" permanently
    ambiguous. 2.11 names that boundary.
  - the demo glyph is an F, not a blob. A symmetric shape cannot show a reflection,
    and the reflection case is the entire point of the determinant's sign.
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
  src/math/: vec2.hpp, mat2.hpp
  src/game/: pong.hpp, pong.cpp
  docs/: index.html, conventions.html, math-toolbox.html, cpp-style.html
  docs/lessons/: 00-01-what-is-an-engine.html, 00-02-how-this-course-works.html,
                 00-03-toolchain.html, 00-04-cmake-from-zero.html,
                 00-05-first-window.html, 00-06-headers-and-debugger.html,
                 01-01-events-properly.html, 01-02-input-state-vs-events.html,
                 01-03-delta-time.html, 01-04-fixed-timestep.html,
                 01-05-framebuffer.html, 01-06-colour.html,
                 01-07-vectors-2d.html, 01-08-pong.html,
                 02-01-lines.html, 02-02-triangle-edge-functions.html,
                 02-03-barycentric.html, 02-04-attribute-interpolation.html,
                 02-05-matrices.html
  docs/_template/: lesson-template.html, README.md, apply-shared.py, check-page.js
  memory/: 2026-07-16.md, 2026-07-18.md, 2026-07-21.md, 2026-07-22.md,
           2026-07-23.md, 2026-07-24.md
  (retired: hello.cpp)

next: 2.6 — Building mat4 by Hand
      (planned filename: docs/lessons/02-06-mat4.html — 2.5 links to it)
      The same question in 3-D: where do THREE basis vectors land? Build mat3 first
      so the idea carries over with nothing new to learn — rotation about each axis
      derived the same way (each is a 2-D rotation acting in the plane of the OTHER
      two axes, with the third column left alone), scale, and the determinant as a
      VOLUME factor.
      THEN the fourth column and row — but WITHOUT explaining w. Build mat4 as
      "a mat3 in the top-left corner plus a spare column", show that the spare
      column does nothing useful under the rules we have so far, and STOP. 2.7 is
      where w earns its keep. The temptation to explain homogeneous coordinates
      early is strong: RESIST IT. 2.5 deliberately left translation broken and the
      payoff depends on the gap staying open one more lesson.
      Storage stays column-major: mat4 = four vec4 columns, same shape as mat2 and
      the same "columns are where the basis vectors land" reading.
      Exercise 2.5.3 (rotate about a point, done the painful way) is the setup —
      2.6 should reference it and 2.7 should resolve it.
```
