# STATE — resume key

This file mirrors the `STATE` block from the master prompt (CLAUDE.md §9). It is the
single source of truth for "where the course is". Update it at the end of every lesson.
To resume: read CLAUDE.md (the binding spec), then this file, then continue from `next`.

```STATE
course: Build a Professional 3D Game Engine (SDL3 + C++20)
version: 1.0
updated: 2026-08-07 (after Lesson 3.7, 33 of 94 lessons)

conventions:
  world: right-handed, Y-up, -Z forward
  clip: left-handed, +Y up, z in [0,1] (SDL_GPU-fixed; projection absorbs the flip)
  sw-rasterizer: targets SDL_GPU's exact NDC (Module 4 port = API change, not maths change)
  matrices: column vectors, v' = M*v, COLUMN-MAJOR storage.
        A matrix IS the set of answers to "where do the basis vectors land":
        c0 = image of (1,0,..), c1 = image of (0,1,..), and so on. Everything else
        is a consequence, not a rule to memorise.
        mat2 / mat3 / mat4 ALL HAVE THE SAME SHAPE: N columns of vecN, and
        identity() as a STATIC MEMBER on each (a free identity() became impossible
        the moment mat3 existed — no arguments, so it could differ only by return
        type, which C++ cannot overload on).
        WRITTEN IN ROWS, STORED IN COLUMNS. The first two floats in memory are
        the LEFT COLUMN read downwards, NOT the top row. Getting this backwards
        transposes the matrix, which turns a rotation into the opposite rotation
        AND a shear into the wrong axis — if both break at once it is ONE layout
        bug, not two.
        A*B means B FIRST. Forced by (A*B)v == A(Bv), not a convention to look
        up. Row-vector codebases (v' = v*M) read the other way; mixing the two
        gives code that is transposed AND backwards.
  depth: DEVICE DEPTH, in [0,1], 0 = NEAR, 1 = FAR. Smaller is nearer.
        CLEAR TO 1.0. Clearing to 0 claims the whole screen is already covered by
        something touching the lens, so EVERY fragment fails and the screen is
        empty — a total failure that reads like a broken transform.
        COMPARE WITH < (strictly). On a tie the pixel already there keeps it, so
        coplanar geometry has ONE STABLE winner (the first drawn) instead of
        flickering. Matches SDL_GPU_COMPAREOP_LESS. Note the consequence: at a
        SILHOUETTE, a back face and a front face tie exactly along their shared
        edge and the back face wins — a one-pixel artifact that 3.4's culling
        removes (measured: 29 px on the milestone scene, 0 with back faces culled).
        STORE DEVICE DEPTH, NEVER VIEW-SPACE z. Device depth is EXACTLY AFFINE in
        screen space (1/w is affine in screen space; z_ndc = -A + B*(1/w)), so
        barycentric interpolation of it is exact rather than approximate. View z
        is the RECIPROCAL of an affine function — a hyperbola — and interpolating
        it linearly reads -50.5 where the truth is -1.98 (near=1, far=100, screen
        midpoint). THE BUG HIDES on surfaces parallel to the screen, where 1/w is
        constant and both choices agree — i.e. on exactly the boxes-and-floors
        geometry test scenes are made of.
        PRECISION: dw = dz * w^2 * (1/near - 1/far). Quadratic in distance; the
        bracket is dominated by 1/near. near 1 -> 0.1 costs 10.09x EVERYWHERE;
        far 100 -> 1000 costs 0.9%. The near plane is the expensive one.
        THE TEST IS NOT PART OF THE BUFFER. depth_buffer stores and clears; the
        rasterizer compares. Same split as SDL_GPUDepthStencilState's three
        independent knobs (compare_op / enable_depth_test / enable_depth_write).
  clipping: NEAR PLANE ONLY, IN CLIP SPACE, BEFORE THE DIVIDE. Inside is
        z_clip >= 0 — NOT w >= 0, which is the plane through the EYE, `near` units
        closer, and which admits a point 0.0001 units in front of the camera that
        divides to x = 691714 px on a 320-px-wide buffer (measured).
        WHY BEFORE THE DIVIDE: the divide is the ONE information-destroying step in
        the pipeline. x/w for a point behind the eye and x/w for an ordinary point
        slightly left of centre are the SAME NUMBER. Nothing downstream can tell
        them apart, so nothing downstream can clip.
        WHY z_clip = 0 IS THE NEAR PLANE: 2.10's depth row is z_clip = A*z_v + B
        with A = f/(n-f), B = f*n/(n-f); setting it to zero cancels f and (n-f) and
        leaves z_v = -n exactly. The plane was ARRANGED to be a coordinate plane,
        which is what makes the test one comparison. Equivalent w form: w >= near.
        CROSSING PARAMETER t = da/(da - db), and it equals (z_a + n)/(z_a - z_b) —
        the far/(near-far) factor is common to both distances and cancels, so t does
        not depend on the far plane, the fov or the aspect at all. If yours changes
        when you change `far`, you have a bug.
        EVERY COMPONENT lerps with that one t, INCLUDING w. Interpolating x,y,z and
        forgetting w gives a vertex in the right place with the wrong depth scale —
        right shape, wrong size, and only on clipped geometry.
        THE OTHER FIVE PLANES ARE AN OPTIMISATION, not correctness: the rasterizer's
        bbox clamp already draws off-screen triangles correctly, just wastefully.
        Say which is which; it is the whole reason this lesson exists and 3.4's
        culling does not.
  culling: FROM THE SIGN OF THE SCREEN-SPACE SIGNED AREA, in the rasterizer, at
        the front of fill_triangle. FRONT-FACING IS edge_function < 0 in framebuffer
        coordinates — the convention is CCW-in-NDC (+y up), the viewport flips y, and
        a reflection reverses signed area. Measured: a CCW-in-NDC triangle through the
        real viewport gives -5184. DO NOT discover this sign by trying both; a wrong
        guess looks plausible (you see the inside of everything) and leaves you with
        two unexplained flips the day a mirrored transform arrives.
        NEVER dot(normal, camera_forward). That asks about the camera's AXIS; the
        question is about the RAY FROM THE EYE. Measured wrong on 15.46% of triangles
        at 55° fovy, 32.43% at 120°, and 0% under an ORTHOGRAPHIC projection — which
        is exactly why it survives: it is the ray test, for a camera we are not using.
        The correct view-space form is dot(n, centroid) < 0.
        WHY THE SCREEN TEST IS THE VIEW TEST: dot(n,a) = det[a,b,c], and
        2*area_ndc = kx*ky*(-det)/(wa*wb*wc). Clipping guarantees every w >= near > 0,
        so the divisor is positive and the sign is the determinant's alone. Culling
        without clipping is not merely unsafe, it is WRONG — a negative w flips the
        winding of a triangle that never moved.
        PRECONDITION: CLOSED geometry. "Facing away" implies "hidden" only for a
        surface enclosing a volume. Ground planes, quads and billboards need
        cull_mode::none; it is a property of the OBJECT, not the renderer, which is
        why it belongs on a material (Module 6) and why two cull modes = two batches.
        NOT "half the triangles" — that is a CEILING, not a rule. Measured: a cube
        shows 2..6 of its 12 (mean 5.55), an icosahedron 7..10 of 20 (mean 8.80).
        The eye can lie BETWEEN a parallel face pair's planes, in front of neither.
        And 54.8% of triangles removed bought 31.6% of the time (19.95 -> 13.64 us):
        back faces were the ones the z-buffer was already rejecting on their first
        depth test. Culling is worth MORE the more expensive the fragment work is.
  interpolation: BARYCENTRIC INTERPOLATION PROMISES ONE THING — the unique AFFINE
        function of the PIXEL POSITION agreeing with three corner values. So it is
        correct for a quantity affine in screen space and wrong for one that is not.
        Two answers, and the asymmetry is load-bearing:
          DEPTH        interpolated DIRECTLY. Already affine (3.1). Correcting it
                       twice is a real bug: self-consistent, so it reads as a depth
                       PRECISION problem and sends you to the near plane.
          EVERYTHING   perspective-corrected: interpolate a/w and 1/w, divide.
          ELSE         a is linear over the SURFACE; substituting the projection
                       gives a/w = (affine in x_n,y_n) + delta*(1/w), affine by 3.1.
                       One derivation covers uv, colour, normals — the constants
                       cancel and are never computed.
        vertex::inv_w stores 1/w PRE-DIVIDED (the loop wants 1/w, and the divide-back
        becomes a multiply). DEFAULTS TO 1 = the orthographic case, where the
        correction is the identity — so every 2-D fill written before 3.2 goes
        through the new path and comes out BIT-IDENTICAL (verified, 17275 px, 0 differ).
        COST: one divide per pixel, shared by every attribute, so it amortises the
        moment a fragment carries more than one thing.
        THE BLIND SPOT: a triangle whose plane is PARALLEL TO THE SCREEN has constant
        w, so affine and correct agree EXACTLY. Sprites, UI quads, billboards and the
        front faces of axis-aligned boxes are all in that family — which is why BOTH
        of Module 3's interpolation bugs survive any test scene made of them. The
        question to ask is not "what is different about the game" but "WHAT FAMILY OF
        INPUT DOES MY TEST SCENE STRUCTURALLY EXCLUDE?"
        THE SHAPE TO RECOGNISE: an error that is EXACTLY ZERO AT THE CORNERS and
        maximal in the middle is a chord drawn under a curve. That is why checking
        vertex values — the first thing anyone does — never finds it. Chord error is
        O(h^2), so subdividing converges QUADRATICALLY and never terminates: measured
        ratios 2.31, 2.42, 2.62, 2.84, 3.10 (-> 4), and 381 px still wrong at 2048 tris.
  homogeneous: w SAYS WHAT KIND OF THING THIS IS.
        w = 1  a POSITION  -> the translation column is added in full
        w = 0  a DIRECTION -> the translation column is multiplied by 0
        NOT a flag and NOT arbitrary: w is the MULTIPLIER ON THE TRANSLATION
        COLUMN, so 1 is the value that applies t exactly once. MEASURED: w of
        0 / 0.5 / 1 / 2 applies none / half / one / two of the offset. This is
        also WHY no purely linear map could translate — it had no input component
        that was always the same number.
        DIRECTIONS MUST CARRY 0. Sent as a position, a direction has the
        translation added, so THE ERROR EQUALS THE TRANSLATION and scales with
        distance from the origin: measured 10.77 / 107.70 / 1077.03 at x1 / x10 /
        x100. Invisible at the origin, ruinous far away — the worst possible
        detection profile. CHEAPEST TEST: a unit direction through a rotation must
        come back UNIT LENGTH (1.00, not 10.82).
        THE BOTTOM ROW (0,0,0,1) IS LOAD-BEARING. w_out = 0x+0y+0z+1w = w, so a
        position stays a position EXACTLY. The product of two matrices with that
        bottom row has it too, so the guarantee survives ANY depth of composition.
        VERIFIED: after 40 affine compositions the bottom row is EXACTLY
        (0,0,0,1) and w returns exactly 1.0 / 0.0. Such a matrix is AFFINE.
        THE PAYOFF: affine(A,ta) * affine(B,tb) == affine(A*B, A*tb + ta),
        verified. Exercise 2.5.3's hand-carried bookkeeping is ABSORBED into
        ordinary matrix multiply. Rotation about any pivot is now ONE matrix:
        T(c) * R * T(-c) — pivot fixed, distances preserved, both verified.
        "HOMOGENEOUS" = the representation is defined only up to overall scale,
        because you DIVIDE BY w to read a position out: (2,4,6,2) is the same
        point as (1,2,3,1). With w = 1 the divide is invisible, which is why
        xyz() DROPS w rather than dividing — deliberately, so 2.10 introduces the
        perspective divide under its own name instead of it turning out to have
        been hiding inside an accessor.
        A direction has w = 0 so its position is undefined — a point at infinity.
        W CAN BE NEITHER 0 NOR 1. Bottom row (0,0,-1,0) gives w_out = -z = the
        distance in front of the camera, and x/w then SHRINKS WITH DISTANCE:
        measured 1.0, 0.5, 0.2, 0.1 at z = -1, -2, -5, -10. THAT IS PERSPECTIVE.
        2.10 derives it from similar triangles — do not derive it early. Its two
        failure cases (z = 0 -> w = 0, undefined; z > 0 -> w negative and geometry
        from BEHIND the camera appears mirrored on screen) are why CLIPPING exists
        and are Lesson 3.3's. Clipping is not an optimisation.
  spaces: A COORDINATE IS THREE FLOATS AND A ROOM. vec3 is identical bytes in
        model or world space; nothing in the type or the arithmetic distinguishes
        them, and w CANNOT carry it (space does not change how a vector multiplies,
        only what the answer means). So the defence is NAMING, not the type system.
        MODEL space = the mesh's own room (k_cube_v: half a unit from the object's
        OWN centre). WORLD space = the one shared frame everything agrees on.
        NAME MATRICES a_from_b: produces space a FROM space b. Then a product's
        adjacent labels must match — view_from_world * world_from_model — and a
        wrong-ordered composition is a SPELLING mistake, visible before running.
        Values too: v_model before the multiply, v_world after. (2.8 §3.1)
  model-matrix: M = T * R * S. SCALE FIRST, ROTATE, TRANSLATE LAST — and the order
        is DERIVED, not conventional (2.8 §3.2): scale is along the object's own
        axes (so must act while coords are still the object's); rotation is about
        the object's own origin (so must act before it is moved off the origin);
        translation is a statement about the world (so it goes last). Written order
        is the REVERSE of what happens, because A*B applies B first.
        THE COLUMNS ARE THE OBJECT'S FRAME: c0/c1/c2 = the object's own x/y/z axis
        in world space, each times its size along that axis; c3 = position. Read a
        placed object straight off the matrix, no multiply. (2.8 §3.5, verified.)
        TWO WRONG ORDERS, TWO FAILURES: T*S*R applies the scale after rotation
        (along WORLD axes) so a non-uniform object SHEARS as it turns — a mesh
        right angle opening to 157.99deg for a 1.8:0.35 slab at 45deg, |x axis|
        sweeping 0.35..1.8 over a turn. R*T*S translates before rotating so the
        object ORBITS the world origin instead of spinning (== Exercise 2.5.3's
        "rotate about a point", correct for a moon/hinge). BOTH equal T*R*S when
        the scale is UNIFORM or the rotation is IDENTITY — verified bit-for-bit
        (worst diff 0.000e+00 over 360deg) — which is why the bug hides in most of
        a scene and only the one rotating non-uniform object catches it.
  transform: struct { vec3 position; mat3 rotation; vec3 scale{1,1,1}; }. The
        AUTHORING interface — position/rotation/scale in the order a human thinks,
        not the order applied. parent_from_local(t) is the ONLY place that knows
        the T*R*S order. Named parent_from_local not world_from_local because
        Module 5's hierarchy widens "parent" without changing a character. scale
        defaults to (1,1,1) not (0,0,0): the do-nothing scale is one. Rotation is a
        mat3 FOR NOW — 7.1 replaces it with a quaternion, touching one line.
        Rebuilt from a scalar angle every frame, NEVER accumulated into (a running
        matrix * small-delta drifts out of being a rotation — the SAME shear, by a
        different route).
  view-matrix: view_from_world = inverse(world_from_camera). A CAMERA IS AN
        OBJECT WITH A TRANSFORM; looking through it is UNDOING that transform, so
        moving the camera one way moves the world the other — exactly, not as a
        mnemonic (slide eye +2 in x -> a fixed world point's view x drops 2,
        verified). No general 4x4 inverse: a camera has no scale, so its placement
        is RIGID and inverse(affine(R, eye)) = affine(transpose(R), -transpose(R)*
        eye) — an orthonormal rotation's inverse is its transpose. Transposing puts
        the camera's right/up/backward axes into the view matrix's ROWS (fastest
        way to read a camera off its matrix). Last column of each row is -axis·eye.
        BASIS from eye/target/up_hint (right-handed, -Z-forward): backward =
        normalise(eye - target) [+z, because we look down -z]; right = normalise(
        cross(up_hint, backward)); up = cross(backward, right) [already unit].
        VERIFIED: eye -> origin, target -> (0,0,-d); V * world_from_camera == I;
        worked point (0,3,0) with eye (0,3,8) -> view (0, 1.94, -7.76).
        SINGULARITY: look straight up/down => look dir parallel to up_hint =>
        cross is zero => right undefined. Demo CLAMPS elevation to ~+-83 deg. The
        real fix (orientation with no preferred up axis) is the quaternion, 7.1 —
        this is its motivation, met early.
  projection: perspective(fovy, aspect, near, far) -> CLIP space. PERSPECTIVE IS
        ONE DIVIDE BY DEPTH, from similar triangles: x' = f*x/(-z), y' = f*y/(-z),
        so distant things shrink. A matrix is LINEAR and CANNOT DIVIDE, so P writes
        -z into w (the -1 in the bottom row) and a SEPARATE step divides by w. THAT
        IS WHERE w STOPS BEING 1 (2.7's third case, finally cashed). The divide is
        the PERSPECTIVE_DIVIDE, and it is why clip space (pre-divide) and NDC
        (post-divide) are distinct spaces. Matrix (column-major), written as rows:
          | f/aspect  0    0    0 |   f = cot(fovy/2)
          |    0      f    0    0 |   A = -far/(far-near)
          |    0      0    A    B |   B = -far*near/(far-near)
          |    0      0   -1    0 |   bottom row copies -z into w
        DEPTH maps near->0, far->1 (SDL_GPU range, NOT OpenGL's [-1,1]) and is
        1/z-NONLINEAR: near=1,far=100 puts z=-2 already at z_ndc=0.5. Precision is
        lavish near, starved far; PUSH THE NEAR PLANE OUT to fix z-fighting (3.1).
        The HANDEDNESS FLIP (right-handed view -> left-handed clip, conventions §5)
        happens INSIDE this matrix; +y stays up (the +y-down framebuffer flip is
        the VIEWPORT's job, 2.11). VERIFIED: (2,1,-10) -> clip (1.949,1.732,9.091,
        w=10) -> ndc (0.195,0.173,0.909); near->0, far->1 exactly.
  viewport: NDC -> framebuffer pixels + depth, the LAST hop of the chain (2.11).
        THREE INDEPENDENT AFFINE MAPS (a scale and an offset each; no division,
        nothing coupled):
          t = (ndc + 1)/2                      remap [-1,1] -> [0,1]
          screen.x = vx + t_x * w
          screen.y = vy + (1 - t_y) * h        <-- THE FLIP, and only y flips
          screen.z = min_depth + ndc.z*(max_depth - min_depth)
        THE Y-FLIP IS THE POINT: NDC's +y is UP, the framebuffer counts rows DOWN
        from the top (row 0 = top), so NDC's top edge (y=+1) must land on the
        SMALLEST screen y. Drop the (1 - t) and the scene renders UPSIDE DOWN — the
        classic beginner bug. This is the same lone minus sign that has drifted
        through to_screen since 2.5's basis demo and through project() in 2.10; as
        of 2.11 it lives in EXACTLY ONE function, viewport::to_screen, and nowhere
        else. src/gfx/viewport.hpp (header-only, NEW) mirrors SDL_GPUViewport
        FIELD-FOR-FIELD (x, y, w, h, min_depth, max_depth — VERIFIED against
        SDL3/SDL_gpu.h; x/y are the LEFT/TOP offset), so Module 4 fills SDL's struct
        by copying ours.
        min/max depth are usually [0,1] but narrowing is a real trick: render a HUD
        or gizmo at [0, 0.1] over a world at [0.1, 1] and it always wins the depth
        test, no extra pass (Exercise 2.11.4).
        PIXEL CENTRES: NDC +-1 maps to viewport EDGES, not pixel centres; column i's
        centre is ndc.x = (2i+1)/w - 1. The rasterizer samples centres at (x+.5,y+.5)
        (2.2) and that half-pixel is what keeps the two conventions consistent.
        VERIFIED: the new viewport reproduces 2.10's ad-hoc constants EXACTLY (worst
        diff 0.000e+00 over an NDC grid) — 2.11 moved no pixels, it named a transform.
  meshes: INDEXED GEOMETRY (2.12). A mesh is a VERTEX ARRAY (positions, each stored
        once) plus an INDEX ARRAY of uint16 taken in TRIPLES, one triple per
        TRIANGLE — never an edge list, because triangles are what gets filled (3.x),
        culled (3.4) and uploaded (Module 4). src/gfx/mesh.hpp (header-only, NEW):
        struct mesh { span<const vec3> vertices; span<const uint16_t> indices; }
        — two std::spans, so a mesh is NON-OWNING, four words, and cannot outlive
        its data (fine for inline constexpr arrays; Module 5's asset system is what
        happens when meshes are loaded at runtime). Data arrays are INLINE constexpr
        (ODR: a plain constexpr array in a header is one object PER TU).
        WHY INDEXED: the icosahedron's 12 vertices serve 20 faces, so unshared would
        store 60 positions and do 60 matrix multiplies/frame instead of 12. The
        saving is WORK, not just bytes; it is why GPUs have a post-transform cache.
        WIREFRAME FROM TRIANGLES draws each shared edge TWICE (60 draws for 30
        edges, 2x). Named, not hidden; it evaporates once triangles are filled.
        ALL FACES WOUND CCW FROM OUTSIDE, authored that way from the start even
        though nothing consumes winding until 3.4, so meshes never need re-authoring.
        VALIDATE, NEVER TRUST mesh data: Euler V-E+F=2; every UNDIRECTED edge in
        exactly 2 faces (manifold); every DIRECTED edge exactly once (consistent
        winding); each face normal cross(b-a,c-a) pointing away from the centre
        (outward). VERIFIED on the shipped data: icosahedron 12/30/20 -> Euler 2,
        all edges 2-shared, all 20 faces outward, degree uniformly 5, radius exactly
        1.000000, all 30 edges 1.051462. Cube 8/18/12 -> Euler 2 (18 edges, not 12,
        because triangulating each square face adds a diagonal).
        THE ICOSAHEDRON AND WHY phi IS FORCED: three mutually perpendicular
        rectangles of width 2 and height 2h have 12 corners = cyclic permutations of
        (0,+-1,+-h). Demanding ALL 30 EDGES EQUAL gives 2h^2-2h+2 = 4, i.e.
        h^2 = h+1 — the golden ratio's defining equation, so h = phi = 1.6180340.
        Normalising by sqrt(1+phi^2) = 1.9021131 puts every vertex on the UNIT
        SPHERE (so size is the transform's job, not the data's) and makes every edge
        2/1.9021131 = 1.0514622. k_icos_a = 0.5257311, k_icos_b = 0.8506508.
  cross-product: cross(a,b) = (a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y -
        a.y*b.x). Perpendicular to both; |a x b| = |a||b|sin(theta) = the
        PARALLELOGRAM AREA (the sin sibling of dot's cos). Zero for parallel
        inputs (no area, no unique perpendicular). Right-handed: x cross y = z,
        which IS our handedness. ANTICOMMUTES: cross(a,b) = -cross(b,a), so a
        swapped argument order flips an axis (left-handed frame -> mirrored /
        inside-out). INTRODUCED IN 2.9 (camera's right axis); 3.4 revisits it for
        a triangle normal and connects it to signed area + determinant. This
        REVISES vec3.hpp's old "deferred to 3.4" comment — it now lives in 2.9.
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
  assets: GEOMETRY FROM DISK IS DATA, AND DATA GETS CHECKED. Settled in 3.5.
            OBJ INDICES ARE 1-BASED (slot = index - 1) and may be NEGATIVE, meaning
            relative to the count SEEN SO FAR (slot = n + index, no extra -1).
            Index 0 is illegal, which makes it a free "absent" sentinel.
            A VERTEX IS THE TRIPLE (i_v, i_vt, i_vn). Corners share a vertex only when
            they agree about EVERYTHING. Numbered by first appearance, so a load is
            reproducible and diffable.
            ATTRIBUTE ARRAYS ARE INDEX-PARALLEL OR EMPTY. Empty means "has none"; an
            array of zeroes would be the different and more confusing claim that every
            pixel samples one texel.
            FAN TRIANGULATION, (0, k-1, k) — same fan as 3.3's clipper and the same
            condition, stated precisely: correct IFF corner 0 SEES the whole polygon
            (star-shaped about it). Convexity is the sufficient version, sufficient
            because then EVERY corner works. So a concave face may fan fine from one
            corner and grow fins from another — the bug depends on where the exporter
            started listing, which is what makes it intermittent across files.
            uint16 INDICES, ceiling 65536 = SDL_GPU_INDEXELEMENTSIZE_16BIT. Exceeding
            it is an ERROR; a silent wrap builds triangles from unrelated corners.
            NORMALS AND UVS STORED AS WRITTEN — a loader is not a place where data may
            differ from its source. ⚠ THE uv ORIGIN IS STILL OPEN: exporters write v
            bottom-up, SDL_GPU samples top-down; we store the file's numbers and
            settle the flip in 3.9, where a sampler first makes it observable.
            MALFORMED IS FATAL, SILLY IS COUNTED. `f 1/x/2` stops the load with a line
            number; a zero-area face is dropped and reported. Both in the report.
            TOPOLOGY IS MEASURED ON THE WELDED MESH. A uv seam legitimately stores one
            point twice; ask the raw arrays and a watertight model reports a
            seam-shaped hole. Welding is by EXACT bits (with -0.0 normalised) because
            the duplicates came from one computation — a tolerance is Module 5's.
            THE SEAM ONLY WELDS IF YOU ARRANGE IT: computing the wrap angle from
            u = 1.0 gives sin(1.0f*tau) = 1.748e-7 instead of 0, so the two copies
            differ in the last bit and 48 boundary edges appear in a closed torus.
            Compute it from `i % nu`. (Measured, verify_35 §E.)
            EULER IS A DIAGNOSTIC, NOT A VALIDITY CONDITION. V - E + F = 2 - 2g; the
            torus gives 0. Asserting 2 rejects every handle and hole ever modelled.
            "WOUND OUTWARD" = SIGNED VOLUME > 0, by the divergence theorem — assumption
            free, unlike 2.12's centroid test which needs a star-shaped solid and fails
            on the first torus. Accumulate in double: the terms nearly cancel.
  lighting: IN WORLD SPACE, PER VERTEX, IN LINEAR LIGHT. Settled in 3.6.
            LAMBERT IS A FOOTPRINT, NOT A FORMULA. A beam of fixed cross-section on a
            surface tilted by theta covers 1/cos(theta) more area, so power per unit
            area falls by cos(theta). Measured: at 60 deg the footprint is exactly
            2.00 and the brightness exactly 0.50.
            l POINTS TOWARD THE LIGHT. A directional light stores the direction light
            TRAVELS (midday sun = (0,-1,0)); to_light() is the negation and exists so
            the negation has a name. Getting it wrong lights the scene from precisely
            the wrong side and NOTHING LOOKS BROKEN.
            THE CLAMP IS LOAD-BEARING. Past the terminator the dot goes negative and
            SUBTRACTS light: measured -0.740 in red for albedo 0.8. It hides on the
            unlit side (already black) and shows as a hard black rim eating into the
            LIT side near the terminator.
            NORMALS GO THROUGH THE INVERSE TRANSPOSE, never the model matrix. Derived
            from the only thing that defines a normal: perpendicular to every tangent,
            so dot(M*t, X*n) = 0 forces M^T X = I, X = (M^-1)^T.
            AND IT HIDES. Rotation: X == R exactly (measured max diff 5.96e-08).
            Uniform scale: X = (1/s)R, same direction, 0.0000 deg apart. Only a
            NON-UNIFORM scale differs — our slab (1.8,0.35,0.9) tilts a normal by up
            to 67.99 deg, the plinth (1.2,0.25,1.2) by 66.46, the icosahedron by 0.03.
            Rendered on a squashed torus: 97.5% of covered pixels differ, worst
            channel delta 135/255. THE HERO OBJECT LOOKS PERFECT, which is why it ships.
            NORMALISE AT THE POINT OF USE. The inverse transpose does not preserve
            length (the worked example produces one of length 2.16), and neither does
            interpolation. An un-normalised normal scales brightness by its length.
            NO NORMALS -> FACE NORMAL, cross(b-a, c-a), through the SAME normal matrix
            because a face normal is a normal. Zero is the sentinel (normal_at), and it
            survives a matrix multiply as zero.
            LAMBERT IS VIEW-INDEPENDENT. Moving the camera must not change the shading
            (verified: the brightest lit pixel is identical from two camera angles);
            moving the light changed 6,104 px. 3.7's specular is the first view-dependent
            term.
            WHERE THE NORMAL COMES FROM is a SEPARATE axis from WHERE THE EQUATION IS
            EVALUATED. 3.6 does per-vertex evaluation with either normal source; 3.8
            compares flat/Gouraud/per-pixel properly. And cube.obj cannot tell the two
            sources apart — 0 px — because 3.5's split already gave each of its 24
            vertices its own face's normal. A faceted mesh is faceted because of the
            SPLIT, not the shading model. (torus.obj: 5,576 px.)
  specular: THE FIRST VIEW-DEPENDENT TERM IN THE COURSE. Settled in 3.7.
            EVERYTHING BEFORE IT could be evaluated without knowing where the viewer
            was standing, and 3.6 MEASURED that: one fixed point shades 0.83408 from
            two different eyes, the same float. A highlight cannot, and the cost is
            structural, not just arithmetic (see the world_pos note below).
            MIRROR DIRECTION R = 2(n.l)n - l, and it IS -reflect(l,n) exactly (worst
            |sum| over 20000 random pairs: 0.000000). The sign differs because a
            velocity points INTO a surface and a light direction points OUT of it.
            Both names ship; picking the wrong one puts the highlight on the far side.
            R IS UNIT (R.R = 4c^2 - 4c^2 + 1 = 1, so no renormalise) and makes the
            SAME ANGLE with n that l does (n.R = 2c - c = c). Consequence used below:
            R CAN NEVER BE BELOW A SURFACE THE LIGHT IS ABOVE.
            HALFWAY h = normalise(l+v) IS NOT AN APPROXIMATION. Demanding
            mirror(h,l) == v gives 2(h.l)h = l+v, so h must lie along l+v and being
            unit fixes it. Verified: worst |mirror(h,l) - v| = 6e-6 over 20000 pairs.
            THAT is why dot(n,h) asks about the SURFACE ("how much of it faces this
            way") rather than about a ray — the microfacet question, and Module 6's.
            Zero when v == -l, so the term dies rather than peaking.
            beta = alpha/2 EXACTLY (worst 0.000018 deg over a 33x33 sweep), because R
            is fixed by the light while h bisects. Hence q ~ 4p, from
            cos^k x ~ exp(-k x^2/2). FITTED: 4.38x at p=4 -> 4.01x at p=128. It is a
            NEAR-PEAK match and gets WORSE for broad lobes — on the torus, matched
            exponents differ on 85.5% of the object at p=2 and 3.3% at p=64.
            ANY COMPARISON MUST CONVERT THE EXPONENT FIRST or it measures lobe width.
            THE CUT-OFF, AND WHAT IT IS NOT. Phong returns 0 when |a+b| >= 90 deg,
            i.e. when the light and eye are on the SAME SIDE of the normal (on a
            floor: the sun BEHIND you). NOT "the mirror ray dips below the surface" —
            it never does, see above; that folklore was in this lesson's first draft
            and had to be re-derived. What happens is that cos^p only answers over the
            hemisphere AROUND R, which is not the VISIBLE hemisphere, and the visible
            wedge it misses is exactly as wide as the light's angle from n. Measured:
            dot(R,v) <= 0 for 50.4% of above-surface pairs, dot(n,h) <= 0 for 0%.
            Rendered on a plane, sun behind: at 35 deg elevation Phong highlights
            0 of 30806 lit px and Blinn all 30806; at 60 deg, 6168 vs 30806 with
            24638 px Phong paints black that Blinn does not (worst delta 27/255).
            With the sun AHEAD (opposite sides) there is NO cut-off at all and both
            cover the floor — that control is in render_37 on purpose.
            BOTH TERMS CARRY n.l. The cosine law is about ARRIVAL and says nothing
            about what the surface does with the light afterwards. Classic Phong omits
            it: measured 0.608 of full strength on unlit geometry, over 400 of 701
            unlit-but-visible normals. Including it also makes a separate "zero the
            specular past the terminator" guard unnecessary.
            AMBIENT GETS NO SPECULAR — it has no direction, so no mirror direction.
            SPECULAR COLOUR IS NOT THE ALBEDO. A dielectric mirrors off a clear outer
            layer without tinting (white highlight on a coloured body = plastic); a
            metal has no such layer and tints what it reflects. Module 6 = F0.
            NOT ENERGY CONSERVING, and said so with numbers: integral cos^p dw =
            2pi/(p+1), and with the outgoing cosine 2pi/(p+2) (both confirmed by
            quadrature). p 32 -> 64 halves the reflected light (x0.515) while the peak
            stays at exactly 1.0 — backwards, since a smoother surface should
            CONCENTRATE the same light. (m+8)/(8pi) is the usual normalisation:
            0.4775 at m=4, 10.5042 at m=256, so it needs HDR and waits for Module 6.
            THE HIGHLIGHT'S POSITION IS PREDICTABLE on a plane: p = e - R*(e_y/R_y),
            because h == n exactly when v == R and R is the same everywhere on the
            plane. Verified against a 1601^2 argmax, worst error 0.0107 (grid step
            0.02). The peak is often BEHIND the camera, which is why the sun-behind
            case shows only the far tail of the lobe.
            THE CLAMP TO 1 IS REAL: dot of two unit vectors can round above 1 and pow
            amplifies it (measured peak 1.00001). std::min(1.0f, .) makes the
            advertised [0,1] range a fact. GPUs spell it saturate.
            PER-VERTEX IS THE WRONG PLACE, MEASURED THREE WAYS (this is 3.8's case):
              (a) CHORD ERROR. Interpolating the answer vs evaluating at the
                  interpolated normal, over every triangle of the 48x24 torus: worst
                  0.311 at shininess 32, 0.507 at 64, 0.722 at 128 — and 87.8% of the
                  lit area differs at 128. THE ERROR GROWS WITH SHININESS, because a
                  tighter highlight is a sharper feature and a chord approximates a
                  sharp feature badly. Same shape as 2.4's bias and 3.2's affine uv.
              (b) THE PEAK IS MISSED on coarse meshes: an 8x6 torus finds 0.0% of the
                  true peak, 12x8 finds 8.5%, 24x12 73.6%, 48x24 99.6%. 96x48 also
                  finds 99.6% — QUADRUPLING THE VERTICES IMPROVED NOTHING, because
                  whether a vertex lands near the peak is luck.
              (c) SO IT FLICKERS. Over 180 frames of a spin, the brightest pixel is no
                  brighter than the diffuse-only render in 157 of 180 frames for
                  cube.obj, 107 for the icosahedron, 58 for a 12x8 torus, 0 for 48x24.
                  The CUBE is worst, and the reason is the lesson: on a flat face the
                  peak sits in the MIDDLE and per-vertex only samples the corners.
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
        --- 3-D (Lesson 2.6) ---
        EVERY ARGUMENT ABOVE SURVIVES UNCHANGED, because none of them counted the
        axes. Three basis vectors, three columns; the proofs are identical.
        ROTATION NOW NEEDS AN AXIS. Each axis rotation is the 2-D rotation acting
        in the plane of the OTHER TWO, in the order given by the cycle
        x -> y -> z -> x, with its own column left alone.
        Ry's MINUS SIGN IS BELOW THE DIAGONAL, mirrored relative to Rx and Rz,
        because the cycle wraps (z -> x) while the matrix lists x's row above z's.
        THE #1 SIGN ERROR IN GRAPHICS — derive from the cycle, never recall the
        shape. Test: rotation_y(t) * (0,0,1) == (sin t, 0, cos t).
        VERIFIED: rotation_z's top-left 2x2 == mat2's rotation over 121 angles, 0
        mismatches; each rotation fixes its own axis, has det 1, preserves length,
        and transpose == inverse.
        ROTATIONS ABOUT DIFFERENT AXES DO NOT COMMUTE — new in 3-D; in 2-D any two
        rotations always did. Same-axis rotations still add. SHARPEST DEMO: pick a
        point ON one of the axes so one rotation provably does nothing.
        (1,0,0) with Rx(0.6), Ry(0.8): x-first -> (0.6967, 0.0000, -0.7174);
        y-first -> (0.6967, 0.4050, -0.5921). Seed of gimbal lock (Module 7).
        det becomes a VOLUME factor; its sign is HANDEDNESS, so det<0 renders a
        model inside-out once 3.4's culling exists. MEASURED by counting lattice
        points inside the transformed unit cube (120 steps/unit, inside iff
        inverse(M)*p in [0,1)^3): identity 0.00%, rotation_y(0.7) -0.01%,
        scale(1.6,.8,1.3) 0.00%, Rx*Ry*scale -0.00%.
        THE 4x4's FOURTH COLUMN IS INERT while w = 0. A correct translation written
        into c3 moves a point by EXACTLY (0,0,0) — measured, not approximated.
        DIAGNOSIS (2.7's opening): a matrix can only scale each column by a
        component of the input and add them up, so adding a CONSTANT needs a
        component that is always the same number. DO NOT PRE-EMPT THIS.
  shaders: HLSL -> SDL_shadercross (3.0.0-preview) -> SPIR-V/DXIL/MSL  [Module 4+]
  cpp: C++20, no exceptions/RTTI in core, snake_case, private members trailing _,
       .hpp + #pragma once, [[nodiscard]], -Wall -Wextra // /W4, all warnings fixed
  build: CMake >= 3.24, out-of-source (build/), 64-bit; two phases (configure, build);
         Debug build = -DCMAKE_BUILD_TYPE=Debug (adds -g); sources listed explicitly
         (never file(GLOB)); target_include_directories(engine PRIVATE src)
  docs-tooling: the shared CSS and page script are LINKED, not duplicated — ONE copy each,
         at docs/shared/course.css and docs/shared/course.js. Edit those files
         directly; every page picks the change up immediately. (Until the CSS
         extraction they were duplicated into all 36 pages = 18% of docs/; that is
         how the script drifted into 6 versions before 1.2. Cause removed.)
         Still no build step: relative <link>/<script src> resolve off the
         filesystem, so docs/index.html opens by double-clicking, offline —
         verified in Chromium, Firefox AND WebKit including ../shared/ from
         docs/lessons/. Cost: a lesson file is NOT portable alone; the docs/ tree is.
         Two marker regions remain — <!-- SHARED-CSS:BEGIN/END --> and
         <!-- SHARED-SCRIPT:BEGIN/END --> — but they now hold the LINK TAGS, and
         docs/_template/apply-shared.py computes each page's relative prefix
         ("" at docs/, "../" at docs/lessons/) and verifies it. A wrong prefix is
         SILENT: unstyled inert page, no error. It breaks by MOVING a page, not
         editing one, so run the tool after adding/moving pages.
         The KaTeX loader stays INLINE inside SHARED-SCRIPT (SRI + inline onload
         cannot move into course.js; and below the END marker it was never
         propagated — 6 lessons shipped with no maths renderer).
         Page-specific JS/CSS goes OUTSIDE the markers (1.2's key-state widget;
         index.html + math-toolbox.html's own <style>).
         Highlighter word lists: kw is checked before ty, so fundamental types
         (bool/char/int/Uint32/...) belong in CPP_TYPES only. Shell `::` comments
         are anchored to line start (SDL3::SDL3 must not read as a comment).
         `apply-shared.py --check` exits 1 on drift, 2 if a shared file is missing
         or empty, and also lints inline fill= on SVG <text>. Run before committing.
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
  - 2.6  Building mat4 by Hand
  - 2.7  Homogeneous Coordinates and What w Really Means
  - 2.8  The Space Chain: Model to World
  - 2.9  The View Matrix: Deriving Look-At
  - 2.10 Perspective from Similar Triangles
  - 2.11 The Viewport Transform
  - 2.12 MILESTONE: A Spinning Wireframe Mesh  (**Module 2 complete**)
  ===> MODULE 2 COMPLETE <===
  - 3.1  The Painter's Problem and the Z-Buffer
  - 3.2  Perspective-Correct Interpolation
  - 3.3  Near-Plane Clipping
  - 3.4  Back-Face Culling
  - 3.5  A Hand-Rolled OBJ Loader
  - 3.6  Normals and Lambert's Cosine Law

capabilities:
  - gfx 3.6: LIGHT. face_shade's five-step ramp indexed by TRIANGLE NUMBER is retired to
    a key; surfaces now respond to which way they face.
    src/gfx/light.hpp NEW (header-only) — directional_light {direction, colour,
    intensity} where DIRECTION IS THE WAY LIGHT TRAVELS and to_light() is the
    negation, named so it cannot be skipped; `lighting` adding an ambient constant
    that the header itself labels a fudge; lambert(n, l) = max(0, dot) with the clamp
    justified rather than tidied; shade()/shade_encoded() doing every multiply in
    LINEAR light and normalising the normal at the point of use.
    src/math/mat4.hpp — linear_of() and normal_matrix() = transpose(inverse(3x3)),
    with the derivation in the header: a normal is defined by being perpendicular to
    every tangent, so demanding dot(M*t, X*n) = 0 forces X = (M^-1)^T. Needed no new
    machinery — mat3 has had inverse() and transpose() since 2.5.
    src/main.cpp — shading happens PER VERTEX, in WORLD space, in collect_triangles;
    scratch gains world_normal[] and vertex_colour[]. shade_mode {palette, flat,
    smooth} on [G]; correct_normals on [J] with the naive M kept as the wrong thing
    behind a key (8th time, and the FIRST where the wrong thing was the default);
    [A]/[D] swing the light; normal_stats {shaded, fell_back, max_tilt} and a
    normal_wrong pixel count guarded on max_tilt > 0 — no non-uniform scale, nothing
    to compare, which is itself the lesson.
    THE RASTERIZER DID NOT CHANGE. fill_style gained no field and fill_triangle
    gained no branch: lighting produces a vertex colour, and interpolating vertex
    colours is 2.4's job. That is the vertex/fragment split arriving unbidden.
  - gfx 3.5: GEOMETRY FROM DISK. Four files, and the lesson is none of them being
    the parser.
    THE INDEX PROBLEM IS THE CONTENT. OBJ gives every face corner three INDEPENDENT
    indices (`f 1/1/1`); a vertex buffer has ONE index that selects the whole
    vertex. So a vertex IS the triple (i_v, i_vt, i_vn), and a position shared by
    faces that disagree about uv or normal must be stored twice. Measured on
    assets/cube.obj: 8 positions + 4 uvs + 6 normals -> 24 vertices, 16 splits, 0
    reused corners (every one of the 24 corner tokens is a distinct triple). That is
    also why a cube in any engine's vertex buffer has 24 vertices and not 8.
    Numbered by ORDER OF FIRST APPEARANCE, which is deterministic and independent of
    unordered_map's iteration order — so two loads of a file are bit-identical and
    can be diffed.
    src/gfx/mesh.hpp — mesh gains `normals` (nothing reads them until 3.6; they are
    loaded because the file has them and because a normal PARTICIPATES IN DECIDING
    WHAT A VERTEX IS). New owning `mesh_data` {vertices, uvs, normals, indices} with
    .view() -> mesh: the owner/view pair (string/string_view, vector/span), and the
    answer to the ownership strain 3.2's floor_geometry admitted. New `mesh_report`
    + validate(). k_max_mesh_vertices = 65536 — not arbitrary, it is
    SDL_GPU_INDEXELEMENTSIZE_16BIT (verified in SDL_gpu.h). Three factory functions
    switched to designated initialisers (four members now; -Wmissing-field-
    initializers was right to complain).
    src/gfx/mesh.cpp NEW — validate() and make_torus(). Validation runs ONCE at a
    trust boundary, so it reaches for std::map while the loader's hot de-dup loop
    reaches for unordered_map: match the container to how hot the loop actually is.
    src/gfx/obj.hpp/.cpp NEW — obj_status (enum + line number, NOT an exception and
    NOT a general Result<T,E>: that would be inventing a language feature for a
    problem we have once), obj_report (counts, not a bool — split_vertices is the
    index problem measured on this file), parse_obj(string_view) separate from
    load_obj(path) so every awkward case is testable from a string literal,
    save_obj() which COMPACTS each attribute stream as a real exporter does, and
    asset_path() over SDL_GetBasePath.
    src/main.cpp — scene_kind::model + [L] cycling torus/cube/twisted/quirks/
    generated, a live round-trip pixel comparison, and scene_object::closed now
    computed from validate() instead of typed by hand.
    CMakeLists.txt — POST_BUILD copy of assets/ to $<TARGET_FILE_DIR:engine>, a
    generator expression because multi-config generators put the binary in Debug/.
    .gitignore — `*.obj` is MSVC's object extension AND Wavefront's model
    extension; without `!assets/*.obj` every model silently fails to be added.
    assets/ NEW — cube.obj (20 readable lines, quads, the worked example),
    twisted.obj (one face reversed: 3.4's debt made visible), quirks.obj (CRLF,
    negative indices, mixed corner formats, an n-gon, a degenerate face, unknown
    keywords), torus.obj (2,304 tris, written by save_obj from make_torus).
  - gfx 3.4: BACK-FACE CULLING. raster.hpp gains constexpr is_front_facing(a,b,c)
    (= edge_function < 0; a NAMED rule because it now has two readers — the
    rasterizer that acts on it and the demo that counts it, the same argument
    is_top_left got in 2.4), enum class cull_mode {none, front, back} mirroring
    SDL_GPUCullMode's order exactly (verified against SDL_gpu.h), and
    fill_style::cull defaulting to NONE — the ONE field in that struct whose default
    is SAFE rather than CORRECT, because there is no universally correct cull mode.
    raster.cpp: one branch, placed AFTER the area is known and BEFORE the swap that
    reorients to positive area — the swap destroys the sign, so there is exactly one
    window and this is it. Takes SCREEN-SPACE vertices, so the view-space bug cannot
    be written by accident.
    main.cpp: [U] cycles cull_choice {none, back, front, back_by_forward}; the last
    is the classic bug (dot(n, camera_forward), culled in collect_triangles in VIEW
    space, which is exactly where it lives in real codebases) and maps to
    cull_mode::none at the rasterizer. raster_triangle carries front_by_forward so
    the two tests can be compared on identical geometry every frame. cull_stats
    {submitted, front, drawn, disagree}. scene_object gains `closed` — the
    precondition, declared by the geometry, with the HUD warning rather than
    switching modes per object (two cull modes would mean two batches, which is the
    honest lesson and Module 6's job). Second render with cull=none counts the pixels
    culling changed: 100% of them are pixels a BACK FACE had been drawn on.
  - gfx 3.3: NEAR-PLANE CLIPPING. src/gfx/clip.hpp/.cpp (NEW) — struct clip_vertex
    {vec4 position; vec2 uv; Uint32 colour;} (a CLIP-SPACE type, deliberately
    distinct from engine::vertex which is screen-space, so handing the wrong one to
    the clipper is a compile error rather than nonsense); constexpr near_distance(p)
    = p.z; constexpr near_crossing(da, db) = da/(da-db); lerp() (all four position
    components + uv + colour IN LINEAR LIGHT); clip_segment_near(a, b) in place
    (the 1-D case, for the world grid / axes / wireframe); clip_polygon_near(span,
    span) — Sutherland-Hodgman as TWO questions, not four cases, returning 0/3/4
    vertices; k_clip_max_vertices = 4 (an EXACT bound, proved in the header, not a
    margin — six planes would need 9).
    main.cpp: project() SPLIT into to_clip() and screen_from_clip(); screen_point
    LOSES its `visible` flag (the pipeline now has a plan instead of a question);
    new `struct projector {mat4 proj; viewport vp; near_mode near;}` replaces the
    loose (proj, vp) pair everywhere — adding a knob made every call site SHORTER;
    collect_triangles stops at clip space per vertex and moves the divide INSIDE
    the triangle loop, after the clipper, then fans (0, k-1, k); clip_stats reports
    input / in_front / straddling / behind / output as properties of the GEOMETRY,
    not the mode, so the three modes are comparable. [K] cycles clip / drop / none.
    Floor scene dolly min 4 -> 1 so the camera can actually walk past the floor's
    near edge (z=+6, and the eye at radius 7 is at z~6.97).
    NaN guards, all three genuine hardenings: to_pixel() (float->int is UB out of
    range; clamp to +-8000, which also keeps edge_function's products inside int32),
    checker_at() (magenta on a non-finite uv), linear_to_srgb_u8() (`!(linear > 0)`
    — std::clamp CANNOT remove a NaN, since every comparison with one is false).
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
  - gfx 2.12: src/gfx/mesh.hpp (header-only, NEW) — struct mesh (two spans) +
    triangle_count(), plus cube_mesh() (8 verts / 12 tris / 36 idx) and
    icosahedron_mesh() (12 / 20 / 60). main.cpp: draw_cube -> draw_mesh(fb, mesh, m,
    proj, point_w) which transforms each vertex ONCE into view space then walks
    indices 3 at a time drawing each triangle's 3 edges (bounds-checked: its input is
    DATA, which can be wrong in ways code cannot — the same put_pixel-vs-at(row,col)
    distinction from 2.5). New demo-local `struct scene_object { transform xform;
    mesh geometry; const char* name; }` — deliberately NOT bolted onto
    engine::transform (a transform is a placement, not a thing; Module 5's ECS
    attaches mesh and transform as separate components for the same reason).
    Scene is now icosahedron (hero, uniform, spinning) + slab (non-uniform,
    spinning — keeps 2.8's [O] teaching) + plinth (non-uniform, still). The HUD
    probe is now VERTEX 0 OF THE SELECTED MESH and the panel shows verts/tris/idx.
    Dolly min raised 3.0 -> 4.0 (VERIFIED: at 4 the whole scene stays inside the
    viewport at every orbit angle and elevation; the ground grid still runs
    off-screen, which is what a floor should do).
    Verified default frame: icosahedron vertex 0 model(-0.53,+0.85,0) ->
    world(-0.77,+1.29,+0.37) -> view(-0.77,+0.52,-6.42) ->
    clip(-0.83,+1.00,+6.14,w=6.42) -> ndc(-0.130,+0.156,+0.956) -> scr(80.9,82.5,
    d=0.956). 132 wireframe line draws/frame across the 3 objects.
  - gfx 2.11: src/gfx/viewport.hpp (header-only, NEW) — struct viewport {x,y,w,h,
    min_depth,max_depth} mirroring SDL_GPUViewport, with constexpr to_screen(ndc)
    doing the three affine maps and the y flip. main.cpp: k_scene_viewport
    {6, 41.625, 172, 96.75, 0, 1} (centre 92,90; 16:9 to match the projection's
    aspect) REPLACES the loose k_vp_centre/half_w/half_h; project() ends by calling
    to_screen; the HUD's probe now runs the FULL chain model->world->view->clip->
    ndc->SCREEN (the 2.9 R/U/B camera-axis rows were dropped to make room — the
    complete chain is the module's payoff). Depth output computed but unused until
    3.1's z-buffer. Verified default probe ndc(-0.119,0.300,0.959) ->
    screen(81.78, 75.47, d=0.959), inside the viewport, and y ABOVE centre 90
    because ndc.y > 0 — the flip visible in the numbers.
  - maths 2.10: src/math/vec4.hpp gains perspective_divide(v) = v/v.w — a SEPARATE
    named function from xyz() (which still drops w), so drop-vs-divide can never be
    confused. No w guard (behind-camera is clipped upstream, 3.3). src/math/mat4.hpp
    gains perspective(fovy, aspect, near, far); needs <cmath> (added) for std::tan.
    main.cpp: the projection pipeline — project(v_view, proj) does clip = proj*
    point(v) then perspective_divide then viewport (k_vp_centre/half_w/half_h, a
    16:9 rect; -ndc.y is the +y-up->+y-down flip). line3/line3_world/draw_cube/
    draw_axes3/draw_world all take a const mat4& proj now. [P] toggles
    scene_perspective vs scene_orthographic (a demo-local ortho matrix, w=1, kept
    local until 2.11 owns viewport/ortho). Depth cue window retuned to view-z
    [-13,-2]. HUD carries the probe the WHOLE chain model->world->view->clip->ndc
    and shows w=-z_view on the clip line. Verified: default-frame probe
    world(-0.76,1.65,-0.25)->view(-0.76,1.07,-6.87)->clip(-0.82,2.06,6.59,w=6.87)
    ->ndc(-0.119,0.300,0.959); scene fits panel & stays in front (min w 1.74 > near
    0.3) across the whole dolly range.
  - maths 2.9: src/math/vec3.hpp gains cross(a,b) (constexpr; the deferral comment
    is REVISED — cross now belongs to 2.9, not 3.4). src/math/mat4.hpp gains
    look_at(eye, target, up_hint) = the view matrix, built as the inverse of a
    rigid camera placement (transpose the orthonormal basis, -transpose(R)*eye in
    the last column). NO general 4x4 inverse added — mat4 still has none. main.cpp:
    an orbit_camera (target/radius/azimuth/elevation) drives look_at; the scene is
    drawn through view_from_model = view_from_world * world_from_model; to_screen3
    becomes a PLAIN orthographic projection of VIEW space (the 2.8 oblique hack is
    gone — a real camera supplies the 3-D now). Arrows orbit, [-]/[=] dolly (does
    nothing under ortho, on purpose — 2.10 makes it matter). HUD carries one vertex
    model -> world -> view, named in every space, and shows the camera axes read
    from the view matrix's rows. Depth-brightness cue remapped to view-space z
    (window -9..-5, the scene's measured range at r=7).
  - maths 2.8: src/math/transform.hpp (header-only, NEW) — struct transform
    { position; rotation(mat3); scale{1,1,1}; } and parent_from_local(t) building
    M = T*R*S as three scaled rotation columns fed to affine() (9 muls, not 27, and
    it SAYS "column k = axis k times size k" instead of leaving it to be derived).
    NO local_from_parent yet — cheap for this shape (S^-1 * R^T * T^-1) but left for
    2.9's view matrix to derive rather than find written. First "scene": one mesh,
    three transforms, a visible world (ground grid + origin triad). main.cpp:
    to_screen3 gains an OBLIQUE z term (cabinet projection, k_scene_zx/zy) so the
    ground plane no longer collapses to a line — a stopgap, real perspective is 2.10.
    model_matrix(t, order) builds the two WRONG orders on [O] (fifth kept-broken
    demo). Verified: worked vertex (0.5,0.5,-0.5) -> (2.5,1.25,-5.0); T*R*S keeps
    corner 90.000 and |x|=sx at every angle; T*S*R reaches 157.99deg; uniform/still
    controls bit-identical across 360deg.
  - maths 2.7: vec4 gains point(v) [w=1] and direction(v) [w=0] as NAMED
    CONSTRUCTORS — prefer them over to_vec4 everywhere; a literal 1.0f is a magic
    number and magic numbers get changed by whoever is making something compile.
    mat4 gains translation(t), affine(linear, t), translation_of(m).
    ***operator*(mat4, vec4) IS UNCHANGED FROM 2.6, BYTE FOR BYTE.*** The whole
    lesson was a change of MEANING, not machinery — the fix for "my 4x4 does not
    translate" was in what the CALLER said about the data, which is why the code
    you stare at was correct the whole time.
  - maths: src/math/vec3.hpp, vec4.hpp, mat3.hpp, mat4.hpp (header-only, NEW in
    2.6). vec3 = vec2 with a z; every 1.7 idea carries over untouched. NO cross
    product and NO perpendicular() — in 3-D there is a whole PLANE of
    perpendiculars, so the 2-D function has no honest generalisation, and getting
    a specific one needs a second vector, which is exactly what cross takes and
    exactly why 3.4 introduces it when a triangle supplies one.
    vec4 exists so a 4x4 has something to have columns of; its fourth component is
    called w and is NOT yet given a meaning. to_vec4(v, w) is an EXPLICIT named
    function, never an implicit conversion, so w is always something somebody
    chose. xyz(v) DROPS w rather than dividing by it (the divide is 2.10's).
    mat3: three vec3 columns; apply, compose, rotation_x/y/z, scale(sx,sy,sz),
    determinant (cofactor expansion along the top row), transpose, inverse.
    mat4: four vec4 columns, to_mat4(mat3) (VERIFIED faithful AND
    composition-preserving), apply, compose. NO 4x4 determinant or inverse — 2.9
    needs one only for a structured form whose inverse is far cheaper to write
    directly.
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
  - demo 2.7: the cube view becomes a SCENE — TWO cubes built from the same
    rotation and offset composed in OPPOSITE ORDERS. translation(-p)*R spins in
    place; R*translation(p) orbits the origin. That is Exercise 2.5.3 answered on
    screen, and Lesson 2.5's "order matters" with translation finally in play. A
    faint cross marks the world origin so the difference reads in a still frame.
    [W] sets corner w to 0 -> BOTH CUBES COLLAPSE ONTO THE ORIGIN, one exactly on
    top of the other. That is Lesson 2.6 reproduced with one keystroke.
    [N] sets the axis-arrow w to 1 -> the direction arrows get TRANSLATED and skew
    off, while the cubes stay perfectly correct. THAT ASYMMETRY IS THE POINT: it
    is why the normal-as-a-position bug survives code review.
    World centres are read back out of the transformed result, so the HUD reports
    what was drawn rather than what we believe was drawn.
  - demo 2.6: [Tab] now cycles FIVE demos — cube (2.6) / basis (2.5) / triangles /
    lines / Pong. main.cpp is past 1,600 lines. This is no longer merely awkward
    and IT IS THE MODULE 5 ARGUMENT — do not fix it early, but do point at it (the
    lesson does, explicitly).
    Cube view: orthographic wireframe (literally drop z), depth-brightness CUE only
    (no z-buffer, no lighting), the three mat3 columns drawn as red/green/blue
    arrows, cube CENTRED on the origin so rotation spins it in place rather than
    orbiting. [Z] cycles rotation_x / _y / _z / Rx*Ry / Ry*Rx — the last two share
    both ingredients and differ ONLY in order. [,] [.] adjust, [0] reset,
    [Space] spin.
    [T] writes (1.2,0,0) into the 4x4's c3 and the cube MOVES BY 0.00 px. The
    displacement is MEASURED (mat4 result minus mat3 result) so the HUD cannot lie.
    That readout is the whole lesson, and it should START WORKING in 2.7 with no
    change to mat4.hpp — only to what w the caller passes.
    The wireframe is a genuinely ambiguous NECKER CUBE: orthographic projection
    discards the information that would settle it, and perspective (2.10) is what
    puts it back. Worth saying that perspective is not cosmetic.
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
  - depth_buffer subsystem (src/gfx/depth_buffer): width x height floats, clear to
    far, clamped row() fast path, depth_format {f32, unorm24, unorm16} mirroring
    SDL_GPU_TEXTUREFORMAT_D32_FLOAT / D24_UNORM / D16_UNORM, quantise() applied
    BEFORE the compare (the order the hardware uses)
  - fill_triangle takes a NULLABLE depth_buffer*, mirroring
    SDL_BeginGPURenderPass's depth_stencil_target_info ("may be NULL"). Depth is
    interpolated with the SAME unbiased weights as colour; the colour blend runs
    AFTER the test, so a losing pixel is never shaded (early-Z in miniature)
  - vertex carries z (device depth), so fill_triangle's reorientation swap carries
    it automatically — the 2.4 argument for bundling attributes, collected again
  - mesh: quad_mesh() added (4 verts / 2 tris, z=0 plane, CCW from +z)
  - demo: filled, depth-tested 3-D. [F] wireframe / painter's / z-buffer / depth view,
    [C] four scenes (solids, CYCLE, intersecting, near-coplanar), [B] depth format.
    Every frame runs BOTH hidden-surface algorithms on identical geometry and counts
    disagreeing pixels — the lesson's headline number, measured live
  - PERSPECTIVE-CORRECT INTERPOLATION of every vertex attribute; the affine failure
    kept behind interpolation::affine and summonable on [I]
  - vertex now carries x, y, z, inv_w, u, v, colour; mesh carries an optional uvs span
    (empty = this geometry has none) plus uv_at()
  - fill_style: render state as an OBJECT (interpolation + shading + blend_space),
    every field defaulting to the correct value. The shape of a GPU pipeline object;
    adding a knob in 3.6 costs one field, not one parameter at every call site
  - shading::uv_checker — a procedural debug pattern, explicitly a placeholder for a
    fragment shader, which 3.6 will strain and Module 4 replaces
  - the VIEWPORT IS NOW A PARAMETER, threaded through project/line3/draw_world/
    draw_mesh/collect_triangles. The floor scene uses the whole 320x180 framebuffer
    (which is already 16:9, so no new projection); everything else keeps the inset rect
  - demo: a checkered ground plane to the horizon, [I] interpolation, [T] tessellation
    (1..16, rebuilt only on change), and the affine-vs-correct disagreement measured live
  - projection_scratch: reusable per-vertex buffers owned ACROSS frames, replacing the
    fixed 64-vertex stack arrays that the 289-vertex floor would have silently truncated
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
  - point()/direction() named constructors instead of SEPARATE TYPES. The
    type-safe design (position and direction as distinct types) does eliminate the
    bug at compile time and some engines do it. Rejected because it roughly doubles
    the maths library's surface — every operation must state which combinations it
    accepts, and some answers are fiddly (position - position = direction;
    position + position is meaningless) — and because the distinction collapses at
    the GPU boundary anyway, where a shader sees four floats and no types. Named
    constructors buy most of the safety for a twentieth of the machinery.
    Exercise 2.7.5 argues the other side honestly; it is a real trade, not a
    settled question.
  - affine(linear, t) provided ALONGSIDE translation(t) * to_mat4(linear). Same
    matrix; the first says WHAT, the second says HOW, and writing the product out
    is one more chance to get the order backwards.
  - xyz() still DROPS w rather than dividing, deliberately, with a comment saying
    exactly when that stops being correct. The perspective divide must appear in
    2.10 under its own name, not turn out to have been hiding inside an accessor.
  - the demo keeps BOTH ways of getting w wrong on keys ([W] and [N]). Fourth time
    this bargain has been made (draw_line_naive 2.1, pong swept_collision 1.8,
    blend_space::encoded 2.4).
  - identity() moved to a STATIC MEMBER on every matrix type. FORCED: a free
    identity() takes no arguments, so mat2's and mat3's could differ only by return
    type. Everything else survived — transpose/inverse/determinant overload on the
    parameter, rotation vs rotation_x/y/z differ by name, scale differs by arity.
    The function with NOTHING to disambiguate it was the one that broke. General
    rule: a zero-argument function cannot be overloaded at all, so make it a static
    member or give it a distinct name while that is still free.
  - mat2's uniform scale(float) REMOVED. Unused, and it would have become a trap
    the moment someone wanted a uniform 3-D scale: scale(2.0f) silently meaning
    "the 2-D one". Component counts are explicit now.
  - mat3::inverse names its elements in WRITTEN notation (m00..m22) BEFORE doing
    anything. The first draft transcribed the adjugate straight into column members
    and had two cofactors using the wrong component — plausible-looking and wrong.
    The rewrite fixes the CLASS of error, not the instance. M*inverse(M)==I over
    300 matrices is what caught it.
  - the demo cube is CENTRED on the origin. Rotation is always about the origin, so
    a corner-at-origin cube would orbit rather than spin. That is the "choose
    coordinates where the pivot is already the origin" workaround — what asset
    pipelines really do, and the cheap half of Exercise 2.5.3.
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
  - DEPTH BUFFER IS ITS OWN TYPE, not a field of framebuffer. Three reasons, and the
    third settles it: (a) 2-D demos, Pong, the HUD and Module 6's post-process
    intermediates are colour-only and would pay 8 MB each at 1080p for nothing;
    (b) different formats and unrelated clear values; (c) SDL_BeginGPURenderPass
    takes colour targets as an ARRAY and the depth-stencil target as a SEPARATE,
    NULLABLE parameter. Modelling the hardware's split now makes Module 4 a rename.
  - NO depth_buffer::test_and_set(). Tempting — it would name the algorithm — and
    wrong: the comparison is PIPELINE state, not storage. SDL_GPUDepthStencilState
    has compare_op + enable_depth_test + enable_depth_write as three separate knobs
    because different passes set them differently (shadow pass writes depth and no
    colour; transparent pass tests depth and does not write it). Baking
    "less-than, always write" into the buffer would make those unexpressible.
  - fill_triangle takes depth_buffer* rather than gaining a second overload. The
    fill's set-up is subtle (bias, six steps, three starting values) and raster.cpp
    already argues that duplicating something subtle is how one bias ends up wrong
    in one of three places. A nullable NON-OWNING pointer is not a violation of the
    no-raw-owning-pointers rule: that rule is about ownership, and what a reference
    cannot express here is OPTIONALITY.
  - z inserted BEFORE colour in `vertex`, which breaks every brace-init call site —
    deliberately. Uint32 -> float is narrowing in list-initialisation, so the old
    3-argument form is a COMPILE ERROR rather than a colour landing silently in the
    depth field. Aggregate initialisation earning its keep; a constructor taking
    (int,int,Uint32) would have compiled and produced garbage.
  - quantisation is a property of the BUFFER (its format), not of the rasterizer or
    the demo. Storage stays float and we round to the format's grid on write, so the
    BEHAVIOUR (z-fighting) is exact while the memory saving is the part not modelled.
    Said so in the lesson. 2^24-1 and 2^16-1 are exactly representable in float, so
    round(z*codes)/codes lands on a real grid point.
  - the ground grid is drawn FIRST and never depth-tested. Not laziness — it is the
    painter's algorithm surviving as a legitimate special case for a background,
    which is exactly what a skybox is (Module 6). Depth-tested lines are Ex 3.1.2.
  - the demo renders BOTH algorithms every frame into a scratch framebuffer and
    counts differing pixels. A second full pass at 320x180 costs microseconds and
    turns "the painter's algorithm is wrong" from a claim into a live number. Fifth
    time this bargain has been made (draw_line_naive 2.1, pong swept_collision 1.8,
    blend_space::encoded 2.4, the w toggles 2.7, trs_order 2.8).
  - the CYCLE scene's planks are built from make_plank(a, b, width, tilt, OVERHANG).
    The overhang lengthens each quad WITHOUT moving its plane, so the depths at the
    two named corners are still exactly +-tilt. It exists because end-to-end planks
    overlap in a sliver, and a sliver is not a demonstration: swept (R, width),
    the disagreement went 7 px -> 144 px. Measured, not eyeballed.
  - inv_w STORED PRE-DIVIDED rather than w. The inner loop interpolates 1/w, so
    storing w would mean a divide per vertex AND the same divide per pixel; and the
    divide-back becomes a multiply by a reciprocal already computed.
  - AFFINE MODE IS NOT A SECOND CODE PATH. It is the same arithmetic with every 1/w
    forced to 1 — which is what affine interpolation IS: a perspective renderer doing
    orthographic interpolation. One loop, and it says the thing. Costs a redundant
    divide in a mode that exists only to be shown failing; worth it.
  - fill_style replaces the growing tail of trailing enums. Not only tidiness: it is
    the pipeline-object shape (SDL_GPUGraphicsPipelineCreateInfo), and it previews
    Module 4 §2's "why state lives in pipeline objects".
  - shading enum lives in the RASTERIZER and is admitted to be a placeholder for a
    fragment shader. Introduce the crude thing, let it strain (3.6), let the strain
    motivate the right thing — the same arc as main.cpp -> Module 5's demos/ split.
  - mesh uvs are a PARALLEL ARRAY, not interleaved. Parallel lets the cube and
    icosahedron carry none and store none; interleaving is what the GPU wants and what
    Module 4 revisits with the memory-layout diagram the decision deserves.
  - the FLOOR gets the whole framebuffer as its viewport, and that is measured rather
    than chosen: scratch/fit_floor.cpp sweeps every extent worth having and the near
    edge ALWAYS projects outside the inset rect. Not a tuning failure — a surface you
    stand on fills the bottom of your view, which is what makes it a good subject.
  - the uvs on the floor come from the WORLD POSITION, not the grid index, so the
    checker is bit-identical at every tessellation and [T] changes only the
    interpolation error. Without that you are comparing two different pictures.
  - std::floor, not a cast to int, in checker_at. A cast truncates toward zero, so
    -0.5 and +0.5 share cell 0 and the pattern grows a doubled cell at the origin —
    which the floor's uvs (-3..+3) would have shown.
  - the naive collision test is KEPT in the shipped code behind state::swept_collision
    rather than deleted, so the failure can be reproduced on demand (pedagogy §5:
    show the artifact). It is dead weight only if you think a bug you can summon is
    worth less than one you can only describe.
  - CLIPPING GETS ITS OWN VERTEX TYPE. engine::vertex is a SCREEN-space type —
    integer pixels, device depth, a pre-divided inv_w — and every one of those
    fields assumes the divide has happened, which is exactly what makes it the
    wrong type to clip with. One flexible struct with a "have I been divided yet"
    flag would COMPILE and silently produce nonsense. Two types make that call fail
    to compile. Same argument as point()/direction() in 2.7 and drop-vs-divide in
    2.10: when two states must not be confused, make them two things.
  - clip_polygon_near TAKES AND RETURNS SPANS, and the output bound is a PROOF
    written in the header, not a margin: emissions = (vertices inside) + (crossings),
    one plane gives at most one crossing each way around a convex polygon, so 2+2=4.
    No capacity check in the loop. This is 3.2's lesson applied — a limit that
    silently truncates turns a capacity bug into a rendering bug, so the right move
    is to make the bound provable rather than to clamp.
  - SUTHERLAND-HODGMAN WRITTEN AS TWO QUESTIONS, NOT FOUR CASES. "Did the edge
    cross?" (emit the crossing) and "is `cur` inside?" (emit cur). The four-row
    table is real but it is a table, and four branches that are really two is an
    invitation to typo one of them.
  - THE SORT KEY IS COMPUTED BEFORE CLIPPING and shared by every piece. The pieces
    are the same surface; a sort must not be able to tell them apart, or one half of
    a wall sorts in front of the other.
  - THE DIVIDE MOVED INSIDE THE TRIANGLE LOOP, so a shared vertex is divided once
    per triangle using it (60 instead of 12 on the icosahedron). Paid knowingly: the
    two-pass alternative (divide the unclipped, re-divide the clipped) is more code,
    more state, and wrong in ways that are easy to miss. Real hardware pays it too —
    vertex shader, clip, THEN divide, in that order.
  - `projector` GATHERS proj + viewport + near policy. Same argument fill_style made
    in 3.2, one level up. Note the direction of the win: adding this lesson's knob
    made every call site SHORTER, because it replaced two loose parameters with one.
  - THE ARROWHEAD IN draw_axes3 STILL DROPS RATHER THAN CLIPS, and says so. It is
    screen-space DECORATION built by rotating the projected shaft; if either end is
    behind the eye the rotation has nothing meaningful to act on. A decoration with
    no defined position is dropped; GEOMETRY is clipped. Worth stating explicitly so
    it does not read as an oversight.
  - THREE near modes, not a bool. There are two DIFFERENT wrong answers here (drop
    the triangle; divide anyway) and they fail in visibly different ways. A bool
    would only let one of them be seen. Sixth time this bargain has been made
    (draw_line_naive 2.1, pong swept_collision 1.8, blend_space::encoded 2.4, the w
    toggles 2.7, trs_order 2.8, interpolation::affine 3.2).
  - THE NaN GUARDS ARE ENGINE HARDENING, NOT DEMO SCAFFOLDING. float->int is
    UNDEFINED out of range, and `none` mode reaches it. But the NaN is not really
    the demo's: Module 6 pushes an HDR pipeline through linear_to_srgb_u8 and
    Module 7's physics will produce the occasional NaN the way all physics does. A
    cast that is undefined for a value the program can reach is a latent bug
    whichever lesson first reaches it. Note the SHAPE of the test — `!(x < limit)`
    and `!(linear > 0)`, because every comparison with a NaN is false and
    std::clamp therefore hands one straight back.
  - to_pixel CLAMPS TO +-8000 and the constant does two jobs: it keeps the float
    inside int's range, AND it keeps edge_function's products (which multiply
    coordinate DIFFERENCES) inside int32, where signed overflow is also undefined.
  - CULLING READS A SIGN THE RASTERIZER WAS ALREADY COMPUTING, and then throwing
    away. fill_triangle has measured edge_function since 2.2 and immediately
    reoriented to positive area so the top-left rule has meaning — which destroys
    the facing. One window, one branch. No normal, no dot product, no camera: the
    projection already folded the camera into the sign (see the determinant identity
    in the conventions block).
  - is_front_facing IS A NAMED FUNCTION taking SCREEN-SPACE vertices, for two
    reasons. The rule has two readers (rasterizer + HUD counter) and duplicating it
    is how they drift apart — 2.4's is_top_left argument exactly. And the signature
    makes the view-space bug UNWRITABLE: there is no overload that accepts a
    view-space position, so you cannot ask the question in the wrong space by
    accident.
  - THE DEMO COUNTS IN THE CALLER, NOT VIA A RETURN VALUE. fill_triangle could
    report whether it culled, but making every fill return a bool puts a value at
    100% of call sites that 99% ignore. Counting in draw_triangles costs one extra
    edge_function per triangle and keeps the RULE in one place. Instrumentation may
    duplicate the question; it must never duplicate the answer.
  - cull_mode DEFAULTS TO none, breaking 3.2's rule that every fill_style field
    defaults to the correct value. Deliberate: there IS no universally correct cull
    mode, so the default is the SAFE one. SDL_GPU makes the same call (a
    zero-initialised SDL_GPURasterizerState is CULLMODE_NONE).
  - `closed` IS A BOOL ON scene_object AND THE DEMO ONLY WARNS. Restraint on
    purpose: the whole scene is one batch with one fill_style, and per-object cull
    modes would mean splitting the draw. That friction is the lesson — it is the
    first time this engine has wanted two pipeline states in one frame, and the
    answer is a material system (Module 6), not another parameter.
  - THE WRONG TEST IS CULLED IN collect_triangles, IN VIEW SPACE — not because that
    was convenient, but because that is exactly where the bug lives in codebases that
    have it. It looks like a sensible early-out. Seventh keep-the-wrong-thing bargain
    (draw_line_naive 2.1, pong swept_collision 1.8, blend_space::encoded 2.4, the w
    toggles 2.7, trs_order 2.8, interpolation::affine 3.2, near_mode 3.3).

  - to_eye HAS NO DEFAULT on shade(), so every call site written before 3.7 is a
    COMPILE ERROR rather than a silent highlight computed against a viewer who is
    not there. Same bargain 3.1 made inserting z ahead of colour in `vertex`: when
    a change must be noticed, make the compiler notice it. The SURFACE parameters
    do default, because there the safe answer and the correct answer coincide —
    a black highlight reproduces 3.6 BIT FOR BIT (0 of 100000 random configs differ).
  - `specular` IS A MATERIAL AND IS DELIBERATELY NOT CALLED ONE. A material is also
    the albedo, the cull mode, the blend mode, the textures and eventually the
    shader; inventing four fifths of that here would be guessing. This is the
    FOURTH pull in the same direction (3.2's shading enum, 3.4's `closed`, 3.6's
    "fill_style is the wrong home", now this) and Module 6 answers it with
    arguments rather than by accretion.
  - specular_model IS A PARAMETER, NOT A FIELD OF `specular`. Which approximation
    you evaluate is PIPELINE state — a real engine bakes it into a shader at
    compile time and a scene does not mix the two. It is a runtime knob here for
    exactly one reason: so both can be rendered and the difference counted.
  - THE COMPARISON CONVERTS THE EXPONENT (matched_shininess, 4x). Comparing the two
    models at one exponent mostly measures that one lobe is wider, which is true,
    is 3.4's own point, and swamps the effect under test. A comparison is only
    worth running once you have controlled for what you already know differs.
  - THE COMPOSED view_from_model IS GONE, and that is worth naming rather than
    absorbing. It existed BECAUSE the shading was view-independent; a highlight
    needs the world POSITION, so the two hops come apart and every vertex pays a
    second matrix multiply. A fast path was not lost to carelessness, it was BOUGHT
    OUT by a feature. (The model-space dodge — move the eye into model space once
    per object — is Exercise 3.7.4, and it is a win for one light and a loss for
    many, which is why it fell out of fashion.)
  - FLAT SHADING SAMPLES THE CENTROID. Flat used to need no position at all; with a
    view-dependent term it does, because the specular varies across a face even
    when the normal does not. The centre is the only point that privileges no corner.
  - PHONG IS KEPT BEHIND [H] — the EIGHTH keep-the-wrong-thing bargain
    (draw_line_naive 2.1, pong swept_collision 1.8, blend_space::encoded 2.4, the w
    toggles 2.7, trs_order 2.8, interpolation::affine 3.2, near_mode 3.3,
    cull_choice::back_by_forward 3.4). Note this one is not simply "wrong": it is a
    real historical model with a real, provable failure, which is a better example.
  - THE HUD READS THE BRIGHTEST CHANNEL IN THE VIEWPORT. One integer, and it tracks
    the highlight without needing to know WHERE the highlight went — which is the
    thing under investigation. The cheapest honest instrument in the demo.
  - THE MISSING n.l IS AN EXERCISE, NOT A KEY. Three lighting toggles ([G], [J],
    [H]) plus an exponent ([E]) is already at the limit of what one HUD line can
    say, and the artifact is fully characterised numerically in verify_37 §E.
    Restraint, on the same grounds 3.4 used for per-object cull modes.

files:
  /: CLAUDE.md, README.md, ARCHITECTURE.md, LEARNINGS.md, PROMPT.md, LICENSE,
     .gitignore, CMakeLists.txt, STATE.md
  src/: main.cpp
  src/core/: input.hpp, input.cpp, clock.hpp, clock.cpp,
            fixed_step.hpp, fixed_step.cpp
  src/gfx/: clip.hpp, clip.cpp, colour.hpp, colour.cpp,
            depth_buffer.hpp, depth_buffer.cpp,
            framebuffer.hpp, framebuffer.cpp,
            mesh.hpp, mesh.cpp, obj.hpp, obj.cpp,
            raster.hpp, raster.cpp, viewport.hpp
  src/math/: vec2.hpp, vec3.hpp, vec4.hpp, mat2.hpp, mat3.hpp, mat4.hpp, transform.hpp
  src/game/: pong.hpp, pong.cpp
  assets/: cube.obj, twisted.obj, quirks.obj, torus.obj
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
                 02-05-matrices.html, 02-06-mat4.html,
                 02-07-homogeneous.html, 02-08-space-chain.html,
                 02-09-view-matrix.html, 02-10-perspective.html,
                 02-11-viewport.html, 02-12-wireframe-mesh.html,
                 03-01-z-buffer.html, 03-02-perspective-correct.html,
                 03-03-near-plane-clipping.html, 03-04-back-face-culling.html,
                 03-05-obj-loader.html, 03-06-normals-and-lambert.html,
                 03-07-specular-blinn-phong.html
  docs/shared/: course.css, course.js      (THE stylesheet + page script; one copy each)
  docs/_template/: lesson-template.html, README.md, apply-shared.py, check-page.js
  memory/: 2026-07-16.md, 2026-07-18.md, 2026-07-21.md, 2026-07-22.md,
           2026-07-23.md, 2026-07-24.md, 2026-07-25.md, 2026-07-26.md,
           2026-07-27.md, 2026-07-28.md, 2026-07-29.md, 2026-07-30.md,
           2026-07-31.md, 2026-08-01.md, 2026-08-02.md, 2026-08-03.md,
           2026-08-04.md, 2026-08-05.md, 2026-08-06.md, 2026-08-07.md
  (retired: hello.cpp)



next: 3.8 — Flat, Gouraud and Per-Pixel Shading
      (planned filename: docs/lessons/03-08-shading-models.html — 3.7 links to the index
      for now, so BOTH of 3.7's next links and its Recap need repointing when it lands)
      3.7 DID NOT ARGUE FOR THIS LESSON, IT MEASURED IT. Everything in 3.7 §4.7 is a
      property of WHERE the shading equation is evaluated, not of the equation: the
      equation is right and we are sampling it in the wrong places. The three tables are
      the whole motivation and they are already published — reuse the numbers, do not
      re-derive them.
        - THE TWO AXES HAVE BEEN TANGLED SINCE 3.6 AND THIS IS WHERE THEY SEPARATE.
          WHERE THE NORMAL COMES FROM (per-face vs per-vertex, a property of the MESH
          and its splits) is independent of WHERE THE EQUATION IS EVALUATED (per-face,
          per-vertex, per-pixel, a property of the PIPELINE). 3.6's shade_mode enum
          conflates them and says so in its own comment; 3.8 replaces it with two
          knobs and the 2x3 grid is the lesson's spine. Note that some cells are
          degenerate — a face normal evaluated per pixel gives flat shading again —
          and saying WHICH and WHY is the check that the student has the distinction.
        - GOURAUD IS A NAME FOR WHAT WE ALREADY DO. Do not introduce it as new
          machinery: 2.4 built the interpolation, 3.6 fed it lit vertex colours, and
          that combination IS Gouraud shading (1971). Naming something the student
          already built is a good moment; it also sets up the honest question of what
          per-pixel actually costs.
        - THE COST IS THE POINT OF THE LESSON, NOT AN ASIDE. Per-pixel means the
          rasterizer must interpolate a NORMAL and call shade() per fragment — the
          first time raster.cpp has learned that light exists, after THREE lessons of
          it not needing to (3.5, 3.6, 3.7 all left it untouched; 3.7 §4.7 says so
          explicitly and predicts this). That is a real structural change: fill_style
          finally does gain a field, the fill gains a branch, and the vertex/fragment
          split stops being an observation and becomes code. Measure the cost per
          frame; it is the honest reason hardware shades per pixel and 1970s software
          did not.
        - THE INTERPOLATED NORMAL IS NOT UNIT and must be renormalised per fragment.
          2.4's header already warns about this ("returns an interpolated NORMAL that
          is no longer unit length"); this is where the debt is paid. Show what
          skipping it does — brightness scaled by |n|, worst in the middle of a
          triangle where the interpolation is furthest from both ends.
        - PHONG SHADING vs THE PHONG REFLECTION MODEL. Same man, two different things,
          and students conflate them constantly. 3.7 taught the reflection model; 3.8
          teaches the shading (interpolation) scheme. Say it in a callout, once,
          plainly — and note that we use PHONG SHADING with the BLINN-PHONG REFLECTION
          MODEL, which is the usual modern pairing and sounds contradictory until the
          two axes are separated.
        - THE MACH BAND is the artifact Gouraud has and per-pixel does not. Do not
          hand-wave it: it is a perceptual effect (lateral inhibition) that makes a
          piecewise-linear intensity ramp show visible edges at the C1 discontinuities
          — i.e. at the triangle boundaries — even though the numbers are continuous.
          That is a genuinely interesting "the bug is in the eye, not the buffer"
          moment and it is worth a figure.
        - VERIFY: (a) flat vs per-vertex on cube.obj is still 0 px (3.6's result — a
          faceted mesh is faceted because of the SPLIT), and non-zero on the torus;
          (b) per-pixel FIXES 3.7's flicker: the 157-of-180 blank frames for cube.obj
          go to 0, which is the headline; (c) the chord error from 3.7's Table 5 falls
          to the renormalisation error alone; (d) the per-frame cost, measured, of
          moving shade() into the fragment loop; (e) with a CONSTANT normal across a
          triangle, per-vertex and per-pixel must agree bit for bit — the control that
          proves the difference is interpolation and not a second bug.
      Module 3 then finishes: 3.9 texture mapping + bilinear (replacing checker_at with
      a real sampler and settling the uv-origin question 3.5 left open), 3.10 profiling
      and the Module 3 capstone.
```
