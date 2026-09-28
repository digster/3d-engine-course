# STATE — resume key

The resume key of CLAUDE.md §9: read CLAUDE.md, then this file, then continue from
`next`. It holds the headlines; **the full text of every section lives verbatim in
`state/`** (see `state/README.md`), and each section here says which file. Merge in
place and never regenerate: a lesson updates `updated:`, `completed:`, `capabilities:`,
`next:` and any headline it changes here, and appends its detail to the matching
`state/` archive.

```STATE
course: Build a Professional 3D Game Engine (SDL3 + C++20)
version: 1.0
updated: 2026-09-28 (after Lesson 6.17b, written after 8.13 — 97 of 113 lessons published;
         the first of the four lessons inserted by the post-Module 8 review, placed between
         6.17 and 6.18. Local lights, their shadows, a Module 3 guard-band fix; ~774 h.
         Per-lesson history: state/updated.md.)

conventions:   (headline of each key, verbatim; full text: state/conventions.md;
               the reader's version: docs/conventions.html)
  quat: w FIRST, w = cos(theta/2), SANDWICH q v conj(q), q*p MEANS "DO p THEN q".
  clip: A CLIP IS A FUNCTION FROM TIME TO POSE, AND IT HAS NO STATE.
  contact: *** THE ARITHMETIC IS IN VELOCITY, AND A COLLISION IS AN EVENT RATHER THAN AN INTERVAL.
  solver: *** THE LOOP IS SEVEN STAGES AND THREE OF THE ORDERINGS ARE LAW.
  joint: *** A CONSTRAINT IS A ROW AND TWO BOUNDS, AND C >= 0 IS THE SATISFIED SIDE.
  swing_twist: *** q = swing * twist, TWIST FIRST, ABOUT THE BONE.
  ragdoll: *** EVERY BODY HAS EXACTLY ONE OWNER.
  cast: *** A CAST IS NEWTON'S METHOD ON A CONVEX DISTANCE, STEPPED FROM GJK'S LOWER BOUND.
  character: *** A CHARACTER IS A QUESTION, NOT A BODY.
  integrator: *** SEMI-IMPLICIT (SYMPLECTIC) EULER IS THE DEFAULT, AND EXPLICIT EULER IS NEVER CORRECT.
  audio: *** ENERGY ADDS, AMPLITUDES DO NOT.
  skinning: A SKINNING MATRIX IS `model_from_model`, AND EVERYTHING FOLLOWS FROM THAT.
  slerp: THE FORMULA IS a (a^-1 b)^t AND IT IS NOT ABOUT QUATERNIONS.
  euler: INTRINSIC Y-X-Z, ACTIVE, RIGHT-HANDED, RADIANS — and the whole point is that this line exists.
  plane: COMPLEX NUMBERS ARE THE PLANE'S ROTATIONS, AND THE ROTOR IS NOT ONE.
  renderable: THE ENGINE DEFINES A COMPONENT WHEN, AND ONLY WHEN, AN ENGINE SYSTEM READS IT.
  boom: A PARENTED CAMERA IS NOT A FOLLOW CAMERA, AND THE ALGEBRA HAS ONE TRAP.
  local_light: INTENSITY IS THE IRRADIANCE AT ONE METRE, SO A LAMP OF INTENSITY pi AT 1 m IS THE ENGINE'S SUN.
  perspective_bias: UNDER A CAMERA WITH A POSITION, 6.8's BIAS GOES THROUGH THE DEPTH CURVE, WITH THE AXIAL SLOPE, OVER A ROUNDING FLOOR.
  cube_face: A CUBE FACE'S CAMERA IS A MIRROR — rows (u, -v, -major), determinant -1 — NEVER look_at.
  guard_band: NEAR-CLIPPED IS FINITE, NOT SMALL: CLIP AT +-7936 px WHEN A POLYGON LEAVES THE BAND, NEVER CLAMP.
  storage: A LIST SIZED BY THE SCENE IS A STORAGE BUFFER, AND ITS COUNT IS SET BY WHOEVER BINDS IT.
  frustum: SIX PLANES, FROM THE ROWS OF clip_from_world, NEVER FROM A CAMERA.
  conservatism: CONSERVATIVE IN TWO DIRECTIONS, AND ONLY ONE IS ALLOWED TO BE WRONG.
  antialias: LESSON 6.14. TWO PROBLEMS SHARE THE NAME AND THE POPULAR CURE FIXES ONE.
  msaa: LESSON 6.14, AND EVERY FIELD WAS CHECKED AGAINST SDL_gpu.h RATHER THAN ASSUMED.
  bloom: LESSON 6.13. THE THRESHOLD IS IN EXPOSURE-CORRECTED LIGHT AND THE COMPOSITE IS BEFORE THE CURVE — both are physics rather than taste, and both are measured (324 vs 32 clipped pixels; 4,064 of 4,096 changed by an exposure mismatch…
  hdr: LESSON 6.12. THE ENGINE HAD NO HDR BUG — IT WAS AVOIDING THE QUESTION BY CONSTRUCTION, and the two choices that held the lid on are both on record: `k_reference_irradiance` = pi (6.2, so a white surface renders at exactly 1.0) an…
  transparency: LESSON 6.11. ALPHA IS COVERAGE — THE FRACTION OF THE PIXEL'S AREA A SURFACE OCCUPIES — NOT OPACITY, AND NEVER A QUANTITY OF LIGHT.
  actions: AN ACTION IS A NAME; A BINDING MAPS A SIGNAL ONTO IT; A FRAME PUBLISHES THE VALUE.
  concepts: DEPEND ON A SHAPE, NOT A TYPE — first use of a C++20 concept in the course, 5.10, and it was forced by a test that could not otherwise exist.
  hierarchy: THE MATHS WAS FREE; THE ORDER WAS THE LESSON.
  camera: A CAMERA IS AN ENTITY, NOT A KIND OF THING.
  measurement: A/B TIMING HAS FOUR RULES AND engine/core/bench.hpp IS THEM.
  assets: AN ASSET IS FOUND BY NAME, LOADED ONCE, AND EXPLICITLY UNLOADED.
  handles: A REFERENCE INTO THE ENGINE IS AN INDEX PLUS A GENERATION, NEVER A POINTER.
  logging: TWO AXES, NEVER ONE. A CATEGORY says WHO is speaking (a noun, a part of the program); a PRIORITY says HOW MUCH IT MATTERS.
  assertions: AN ASSERTION IS NOT AN ERROR, and one question separates them: COULD A CORRECT PROGRAM, ON A WORKING MACHINE, ENCOUNTER THIS?
  errors: THE ENGINE HAD ALREADY CONVERGED ON THE ANSWER TWICE.
  surfaces: THE SURFACE IS CHOSEN BEFORE ANYTHING EXISTS, in app_config, because by the time you could regret it the window already belongs to somebody.
  samplers: A SAMPLER IS AN OBJECT, WHICH IS THE WHOLE DIFFERENCE FROM 3.9.
  interleave: INTERLEAVED BY DEFAULT, HYBRID IN PRODUCTION, AND BOTH NUMBERS ARE MEASURED on our 1,225-vertex torus with 64-byte lines.
  instancing: ONE ENUM VALUE. input_rate = _INSTANCE instead of _VERTEX; same buffer type, same usage bit, same SDL_BindGPUVertexBuffers, same attributes at ordinary locations.
  pipelines: EVERY PIECE OF RENDER STATE, IN ONE IMMUTABLE OBJECT — 9 top-level fields, 53 expanded, which is exactly the count 4.1 predicted from fill_style.
  shaders: ONE SOURCE, THREE BINARIES, AND SPIR-V IN THE MIDDLE.
  world: right-handed, Y-up, -Z forward
  clip: left-handed, +Y up, z in [0,1] (SDL_GPU-fixed; projection absorbs the flip) sw-rasterizer: targets SDL_GPU's exact NDC (Module 4 port = API change, not maths change)
  matrices: column vectors, v' = M*v, COLUMN-MAJOR storage.
  depth: DEVICE DEPTH, in [0,1], 0 = NEAR, 1 = FAR.
  clipping: NEAR PLANE ONLY, IN CLIP SPACE, BEFORE THE DIVIDE.
  culling: FROM THE SIGN OF THE SCREEN-SPACE SIGNED AREA, in the rasterizer, at the front of fill_triangle.
  interpolation: BARYCENTRIC INTERPOLATION PROMISES ONE THING — the unique AFFINE function of the PIXEL POSITION agreeing with three corner values.
  homogeneous: w SAYS WHAT KIND OF THING THIS IS.
  spaces: A COORDINATE IS THREE FLOATS AND A ROOM.
  transform: struct { vec3 position; mat3 rotation; vec3 scale{1,1,1}; }.
  projection: perspective(fovy, aspect, near, far) -> CLIP space.
  viewport: NDC -> framebuffer pixels + depth, the LAST hop of the chain (2.11).
  meshes: INDEXED GEOMETRY (2.12).
  winding: CCW = front, cull back (per-pipeline state; set explicitly every time)
  units: 1 unit = 1 metre; radians internally; linear colour in the renderer axis colours: x/y/z = red/green/blue (every diagram, no exceptions)
  sdl3: FetchContent, pinned GIT_TAG release-3.4.12 (main is 3.5.0 but unreleased); target SDL3::SDL3; SDL_TEST_LIBRARY OFF sdl3-api: bool SDL_Init / bool SDL_PollEvent; SDL_CreateWindow(title,w,h,flags) — no x/y; SDL_CreateRenderer(wind…
  framebuffer: row-major, index = y*width + x.
  colour: stored channel values are sRGB-ENCODED, not light.
  vec2: an ARROW — direction and length, NO position.
  collision: a discrete overlap test answers about an INSTANT; collision is a fact about an INTERVAL.
  reflect: reflect(v,n) = v - 2*dot(v,n)*n, derived as "subtract the shadow twice".
  determinism: same binary + machine + seed + inputs + STEP SIZE.
  lines: endpoint-INCLUSIVE at BOTH ends (so a rectangle's corners close).
  triangles: edge_function(a,b,p) = (bx-ax)(py-ay) - (by-ay)(px-ax) = z of the 2-D cross product = dot(P-A, perpendicular(B-A)).
  assets: GEOMETRY FROM DISK IS DATA, AND DATA GETS CHECKED.
  lighting: IN WORLD SPACE, PER VERTEX, IN LINEAR LIGHT.
  specular: THE FIRST VIEW-DEPENDENT TERM IN THE COURSE.
  shading: TWO INDEPENDENT AXES, and 3.8 exists because 3.6 shipped them as one enum.
  varyings: vertex = position + VARYINGS as of 3.8.
  barycentric: w_i = area of the sub-triangle OPPOSITE v_i, over the total.
  interpolation: a(P) = w0*a0 + w1*a1 + w2*a2.
  shaders: HLSL -> SDL_shadercross (3.0.0-preview) -> SPIR-V/DXIL/MSL [Module 4+]
  cpp: C++20, no exceptions/RTTI in core, snake_case, private members trailing _, .hpp + #pragma once, [[nodiscard]], -Wall -Wextra // /W4, all warnings fixed
  build: CMake >= 3.24, out-of-source (build/), 64-bit; two phases (configure, build); Debug build = -DCMAKE_BUILD_TYPE=Debug (adds -g); sources listed explicitly (never file(GLOB)); target_include_directories(engine PRIVATE src) state-bl…
  textures: A TEXEL IS A SAMPLE, NOT A SQUARE — its value lives at (i + 0.5)/N.
  measurement: THREE INSTRUMENTS, THREE QUESTIONS (3.10).
  amdahl: speedup = 1/((1-p) + p/s); s -> infinity gives 1/(1-p) and no more.
  cache: MEASURE THE CLIFF, DO NOT ASSERT IT.
  divergence: A WARP RUNS BOTH SIDES OF A BRANCH ITS LANES DISAGREE ABOUT, with the inactive lanes MASKED.
  quads: FRAGMENTS ARE SHADED IN ALIGNED 2x2 BLOCKS, ALWAYS.
  traversal: fill_style::traverse {scanline, quad, quad_debug}, defaulting to SCANLINE — what a CPU rasterizer should do, and what every measurement before 4.1 was taken against.
  measurement: A REFACTOR'S PERFORMANCE CLAIM NEEDS THE SAME CONTROL AS A FEATURE'S.

curriculum: 113 lessons, ~774 h (measured; was ~510), 10 modules   (reshaped 2026-09-08 — see `roadmap:`)
  M0:6  M1:8  M2:12  M3:10  M4:9  M5:12  M6:18  M7:8  M8:13  M9:11
  (reshapes and their reasons: state/curriculum.md)

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
  - 3.7  Specular and Blinn-Phong
  - 3.8  Flat, Gouraud and Per-Pixel Shading
  - 3.9  Texture Mapping and Bilinear Filtering
  - 3.10 Profiling, and the Module 3 Capstone
  ===> MODULE 3 COMPLETE — Stage A (the CPU software rasterizer) is DONE <===
  - 4.1  How GPUs Actually Work
  - 4.2  The SDL_GPU Mental Model
  - 4.3  The Shader Toolchain
  - 4.4  The First Triangle
  - 4.5  Vertex Buffers and Layouts
  - 4.6  Uniform Data and the Matrix Upload
  - 4.7  Textures, Samplers, and Depth
  - 4.8  Porting the Module-3 Scene
  - 4.9  Debugging a Frame with RenderDoc
  ===> MODULE 4 COMPLETE — Stage B (the SDL_GPU renderer) draws the scene <===
  - 5.1  The Refactor: Engine, Demos, and the Public API
  - 5.2  The Platform and Application Layer
  - 5.3  Logging, Assertions, and Errors Without Exceptions
  - 5.4  Handles: Generational Indices
  - 5.5  The Asset System v1
  - 5.6  Data-Oriented Design: Why Scene Trees Creak
  - 5.7  An ECS from Scratch: Storage Design
  - 5.8  The ECS Runtime
  - 5.9  Transform Hierarchy and the Camera System
  - 5.10 Input Mapping: Actions, Not Keycodes
  - 5.11 Dear ImGui and the Debug Draw System
  - 5.12 Checkpoint: A Small 3D Game on the Public API
  ===> MODULE 5 COMPLETE — 12 lessons, ~62 h <===
  - 6.1  Linear and sRGB: The Gamma Lesson
  - 6.2  Radiometry-Lite: What a BRDF Is
  - 6.3  Microfacet Theory
  - 6.4  Cook–Torrance PBR, Derived
  - 6.5  A Material System
  - 6.6  glTF 2.0 Loading
  - 6.7  Normal Mapping and the TBN Derivation
  - 6.8  Shadow Mapping: Bias, Acne, and PCF
  - 6.9  Cascaded Shadow Maps
  - 6.10 Mipmaps, LOD, and Anisotropic Filtering
  - 6.11 Transparency: Alpha Modes, Blending, and Draw Order
  - 6.12 HDR and Tonemapping
  - 6.13 Bloom and the Post-Processing Stack
  - 6.14 Antialiasing: Geometric and Shading
  - 6.15 Skybox and Image-Based Lighting
  - 6.16 Frustum Culling and Instanced Submission
  - 6.17 A Lightweight Frame Graph
  - 6.17b Local Lights: Point and Spot, and Their Shadows   (inserted; written 2026-09-28, after 8.13)
  - 6.18 Text and 2D Overlay Rendering
  ===> MODULE 6 COMPLETE — 18 lessons, ~93 h, the longest module in the course.
  - 7.1  Euler Angles and Their Pathologies
  - 7.3  Complex Numbers Rotate the Plane
  - 7.2  Axis-Angle and Rodrigues' Rotation Formula
  - 7.4  Quaternions, Derived
  - 7.5  Slerp, and the Storage Swap
  - 7.6  Skeletal Animation: The Skinning Math
  - 7.7  Sampling and Blending Animations
  - 7.8  SDL3 Audio: Streams, Mixing, and 3D Sound
  ===> MODULE 7 COMPLETE <===
  - 8.1  Integrators: Why One Explodes
  - 8.3  Angular Dynamics: Torque and the Inertia Tensor
  - 8.2  Forces, Gravity, and Linear Rigid Bodies
  - 8.4  Collision Primitives: Spheres, AABBs, OBBs, and the SAT
  - 8.5  GJK: Convex Distance from a Support Function
  - 8.6  EPA: Penetration Depth
  - 8.7  Contact Manifolds and Persistence
  - 8.8  Broadphase: A Uniform Grid
  - 8.12 Ragdolls: Joints on a Skeleton
  - 8.11 Constraints and Joints: Hinge and Ball-Socket
  - 8.10 Sequential Impulses: Warm Starting, Islands, and Sleeping
  - 8.9  Impulse Response: Restitution and Friction
  - 8.13 A Character Controller
  ===> MODULE 8 COMPLETE — 13 lessons, ~72 h. Physics built, none imported. <===
  (2026-09-27: 6.17b, 6.18b, 7.7b and 8.14 were inserted into Modules 6–8. 6.17b landed
   2026-09-28; the other three are not written yet, so those modules read "in progress" in
   the index until they land.)

capabilities:   (the first headline of each lesson, verbatim; every entry in full:
                 state/capabilities.md)
  - 8.13 THE ENGINE HAS A CHARACTER CONTROLLER, AND A SHAPE CAST ANY GAMEPLAY CODE CAN USE.
  - 8.12 THE ENGINE HAS RAGDOLLS, AND HANDS A CHARACTER TO THE SOLVER AND BACK.
  - 8.11 THE ENGINE HAS JOINTS, IN THE SAME LOOP AS CONTACTS.
  - 8.10 THE ENGINE CAN HOLD A STACK UP, PARTITION A LEVEL, AND STOP SIMULATING THE PARTS OF IT THAT ARE NOT DOING ANYTHI…
  - 8.9 THE ENGINE CAN RESOLVE A CONTACT, which is the first thing in phys/ that changes the world rather than describing…
  - 8.8 THE ENGINE CAN FIND CANDIDATE PAIRS IN A SCENE OF THOUSANDS OF BODIES WITHOUT TESTING EVERY PAIR, and can prove i…
  - 8.5 THE ENGINE CAN MEASURE THE DISTANCE BETWEEN ANY TWO CONVEX SHAPES and hand back the two surface points that reali…
  - 8.4 THE ENGINE KNOWS HOW BIG THINGS ARE, and can answer "are these two touching, and if so how do I fix it?" for ever…
  - 8.3 THE ENGINE CAN TURN.
  - 8.2 THE ENGINE HAS BODIES, and can be told what is pushing on what.
  - 8.1 THE ENGINE CAN ADVANCE A STATE THROUGH TIME, and can say whether the step size you chose is one that works.
  - 7.8 THE ENGINE CAN HEAR.
  - 7.7 THE ENGINE CAN PLAY RECORDED MOTION.
  - 7.6 THE ENGINE CAN DEFORM A SURFACE, which is categorically not what 5.9's hierarchy does: that PLACES objects, and t…
  - 7.5 THE ENGINE CAN BLEND TWO ORIENTATIONS, AND IT STORES THEM AS FOUR FLOATS.
  - 7.4 THE ENGINE CAN WRITE DOWN A ROTATION OF SPACE IN FOUR FLOATS.
  - 7.3 THE ENGINE CAN COMPOSE A ROTATION BY MULTIPLYING, AND INTERPOLATE ONE.
  - 7.2 THE ENGINE CAN NAME THE SINGLE TURN A ROTATION IS, AND TRAVEL IT.
  - 7.1 THE ENGINE CAN BE TOLD AN ORIENTATION IN THREE NUMBERS.
  - 6.18 THE ENGINE CAN SAY SOMETHING.
  - 6.17b THE ENGINE HAS LAMPS, AND EACH ONE CAN CAST A SHADOW.
  - 6.17 THE FRAME IS A DECLARATION, AND FOUR FACTS STOPPED BEING MAINTAINED.
  - 6.16 THE ENGINE DECIDES WHAT NOT TO DRAW, AND DRAWS THE REST IN FEWER CALLS.
  - 6.15 AN ENVIRONMENT LIGHTS THE SCENE AND IS THE SKY BEHIND IT, ON BOTH RENDERERS.
  - 6.14 ANTIALIASING, BOTH KINDS, AND THE HONEST ACCOUNT OF WHAT EACH REACHES.
  - 6.13 A BLOOM IN BOTH RENDERERS, AND THE TWO QUESTIONS 6.12 DEFERRED, ANSWERED.
  - 6.12 AN HDR PIPELINE IN BOTH RENDERERS, AND THE LIGHT-UNITS DEBT PAID.
  - 6.11 TRANSPARENCY IN BOTH RENDERERS, AND THE LAST SILENT GAP IN THE glTF IMPORTER CLOSED.
  - 6.10 MIPMAPPING IN BOTH RENDERERS, AND A DEBT OF SIX PROMISES CLEARED.
  - 6.9 CASCADED SHADOW MAPS IN BOTH RENDERERS, AND AN AUDIT 6.8 PASSED.
  - 6.8 SHADOWS IN BOTH RENDERERS, FROM A BIAS THAT IS DERIVED RATHER THAN TUNED.
  - 6.7 PER-PIXEL NORMALS IN BOTH RENDERERS, AND THE GOLDEN STILL DID NOT MOVE.
  - 6.6 THE ENGINE READS glTF 2.0, AND THE GOLDEN STILL DID NOT MOVE.
  - 6.5 THE ENGINE HAS A MATERIAL, AND THE GOLDEN DID NOT MOVE.
  - 6.4 THE ENGINE HAS A PHYSICALLY-BASED BRDF, LIVE IN BOTH RENDERERS.
  - 6.3 THE ENGINE HAS A STORY ABOUT WHAT A SURFACE IS, AND IT CAN BE CHECKED.
  - 6.2 THE SHADING EQUATION HAS UNITS, AND THEY ARE SEPARABLE.
  - 6.1 THE ENGINE'S OUTPUT STAGE IS CORRECT ON BOTH SURFACES.
  - 5.12 THE ENGINE HAS A GAME BUILT ON IT, BY SOMEBODY STANDING OUTSIDE.
  - 5.11 THE ENGINE CAN DRAW WHAT IT IS THINKING, AND BE ASKED QUESTIONS.
  - 5.10 THE ENGINE CAN BE TOLD WHAT THE PLAYER MEANT.
  - 5.9 THE ENGINE HAS A TRANSFORM HIERARCHY AND A CAMERA THAT IS AN ENTITY.
  - 5.8 THE ENGINE HAS AN ECS, AND IT IS ABOUT 400 LINES OF LOGIC.
  - DESIGN 5.7: THE ECS STORAGE QUESTION IS ANSWERED, WITH EVIDENCE ATTACHED, AND NO ENGINE CODE CHANGED.
  - PERF 5.6: THE ENGINE CAN TIME TWO THINGS FAIRLY.
  - ASSET 5.5: THE ENGINE CAN FIND, SHARE AND FREE THE THINGS IT DRAWS.
  - ARCH 5.4: NOTHING IN THE ENGINE HOLDS A BORROWED POINTER TO GEOMETRY ANY MORE.
  - DIAG 5.3: THE ENGINE CAN BE TURNED DOWN.
  - ARCH 5.2: A PROGRAM NO LONGER HAS TO SAY HOW TO START.
  - ARCH 5.1: THE ENGINE HAS AN OUTSIDE.
  - gfx 4.9: THE ENGINE IS DEBUGGABLE.
  - gfx 4.8: THE ENGINE DRAWS A SCENE, not a thing.
  - gfx 4.7: THE ENGINE CAN HIDE SURFACES AND PAINT THEM.
  - gfx 4.6: THE CAMERA CAN MOVE.
  - gfx 4.5: THE ENGINE CAN DRAW A REAL MESH, MANY TIMES.
  - gfx 4.4: THE ENGINE CAN DRAW.
  - gfx 4.3: THE ENGINE CAN LOAD A SHADER.
  - gfx 4.2: THE ENGINE CAN TALK TO A GPU.
  - gfx 4.1: THE RASTERIZER CAN IMITATE THE HARDWARE, AND COUNT WHAT THAT COSTS.
  - core 3.10: THE ENGINE CAN MEASURE ITSELF.
  - gfx 3.9: COLOUR FROM DATA.
  - gfx 3.6: LIGHT. face_shade's five-step ramp indexed by TRIANGLE NUMBER is retired to a key; surfaces now respond to w…
  - gfx 3.5: GEOMETRY FROM DISK.
  - gfx 3.4: BACK-FACE CULLING.
  - gfx 3.3: NEAR-PLANE CLIPPING.
  - gfx 2.12: src/gfx/mesh.hpp (header-only, NEW) — struct mesh (two spans) + triangle_count(), plus cube_mesh() (8 verts…
  - gfx 2.11: src/gfx/viewport.hpp (header-only, NEW) — struct viewport {x,y,w,h, min_depth,max_depth} mirroring SDL_GPUV…
  - maths 2.10: src/math/vec4.hpp gains perspective_divide(v) = v/v.w — a SEPARATE named function from xyz() (which still…
  - maths 2.9: src/math/vec3.hpp gains cross(a,b) (constexpr; the deferral comment is REVISED — cross now belongs to 2.9,…
  - maths 2.8: src/math/transform.hpp (header-only, NEW) — struct transform { position; rotation(mat3); scale{1,1,1}; } a…
  - maths 2.7: vec4 gains point(v) [w=1] and direction(v) [w=0] as NAMED CONSTRUCTORS — prefer them over to_vec4 everywhe…
  - raster 2.4: is_top_left is now PUBLIC (interpolation has to UNDO the bias, so the rule producing it must be inspectab…
  - demo 2.6: [Tab] now cycles FIVE demos — cube (2.6) / basis (2.5) / triangles / lines / Pong.
  - demo 2.5: [Tab] now cycles FOUR demos — basis (2.5) / triangles (2.2-2.4) / lines (2.1) / Pong (1.8).
  - (39 earlier entries, Modules 0-3, predate lesson tags; they close state/capabilities.md)

decisions:   (headlines; full: state/decisions.md)
  pending-code-corrections: ONE ENGINE COMMENT IS STILL WRONG (IN frustum.cpp), AND THE NEXT LESSON THAT LISTS IT WHOLE CORRECTS IT. (gpu_scene.cpp's two: corrected by 6.17b.)
  found-by-617b: 6.17b FOUND FOUR DEFECTS OUTSIDE ITS SCOPE AND FIXED NONE OF THEM, BECAUSE AN INSERTION MUST NOT MOVE ANOTHER LESSON'S NUMBERS.
  shipped-lesson-fixes: *** A LATER LESSON THAT FIXES AN EARLIER LESSON'S ENGINE CODE SHIPS THE FIX IN ITS OWN LISTINGS AND TELLS THE STORY; THE EARLIER PAGE STAYS AN ARCHIVE OF WHAT SHIPPED.
  quat-swap-deferred: THE FIELD IS CALLED `rotation` AND ONE CALLER DOES NOT PUT A ROTATION IN IT.
  fill-shot: A RENDER FIGURE KEEPS THE PROGRAM'S OWN BACKGROUND.
  builder-byte-count: 46 OF 47 BUILDERS PRINT A CHARACTER COUNT AND CALL IT BYTES.

files:   (paths only, from `git ls-files`; commentary: state/files.md)
  /: .gitignore, ARCHITECTURE.md, CHANGELOG.md, CLAUDE.md, CMakeLists.txt, LEARNINGS.md, LICENSE,
     PROMPT.md, README.md, STATE.md
  .github/workflows/: ci.yml
  assets/: cube.bin, cube.gltf, cube.obj, quirks.obj, shapes.glb, torus.obj, twisted.obj,
     uv_grid.png
  assets/fonts/: Karla-Regular.ttf, OFL.txt
  cmake/: EngineHelpers.cmake, Shaders.cmake
  demos/: CMakeLists.txt
  demos/<name>/main.cpp: audio, bodies, broadphase, character, collector, collide, ecs_swarm,
     epa, gimbal, gjk, gltf_view, hello_cube, impulse, integrate, joints, manifold, plane, pong,
     ragdoll, rig, sandbox, spin, stack
  demos/common/: demo_scene.cpp, demo_scene.hpp, pong.cpp, pong.hpp
  docs/: conventions.html, cpp-style.html, index.html, math-toolbox.html
  docs/_template/: README.md, apply-shared.py, check-builders.py, check-continuity.py,
     check-curriculum.py, check-page.js, continuity-known.txt, estimate-hours.py,
     lesson-template.html
  docs/_template/tests/: test_checkers.py
  docs/lessons/: 00-01-what-is-an-engine.html, 00-02-how-this-course-works.html,
     00-03-toolchain.html, 00-04-cmake-from-zero.html, 00-05-first-window.html,
     00-06-headers-and-debugger.html, 01-01-events-properly.html,
     01-02-input-state-vs-events.html, 01-03-delta-time.html, 01-04-fixed-timestep.html,
     01-05-framebuffer.html, 01-06-colour.html, 01-07-vectors-2d.html, 01-08-pong.html,
     02-01-lines.html, 02-02-triangle-edge-functions.html, 02-03-barycentric.html,
     02-04-attribute-interpolation.html, 02-05-matrices.html, 02-06-mat4.html,
     02-07-homogeneous.html, 02-08-space-chain.html, 02-09-view-matrix.html,
     02-10-perspective.html, 02-11-viewport.html, 02-12-wireframe-mesh.html,
     03-01-z-buffer.html, 03-02-perspective-correct.html, 03-03-near-plane-clipping.html,
     03-04-back-face-culling.html, 03-05-obj-loader.html, 03-06-normals-and-lambert.html,
     03-07-specular-blinn-phong.html, 03-08-shading-models.html, 03-09-textures.html,
     03-10-profiling-capstone.html, 04-01-how-gpus-work.html, 04-02-sdl-gpu-model.html,
     04-03-shader-toolchain.html, 04-04-first-triangle.html, 04-05-vertex-buffers.html,
     04-06-uniforms.html, 04-07-textures-and-depth.html, 04-08-porting-the-scene.html,
     04-09-renderdoc.html, 05-01-the-refactor.html, 05-02-platform-layer.html,
     05-03-logging-and-errors.html, 05-04-handles.html, 05-05-asset-system.html,
     05-06-data-oriented-design.html, 05-07-ecs-storage.html, 05-08-ecs-runtime.html,
     05-09-transform-hierarchy.html, 05-10-input-mapping.html, 05-11-imgui-debug-draw.html,
     05-12-checkpoint-game.html, 06-01-linear-and-srgb.html, 06-02-what-a-brdf-is.html,
     06-03-microfacet-theory.html, 06-04-cook-torrance.html, 06-05-material-system.html,
     06-06-gltf.html, 06-07-normal-mapping.html, 06-08-shadow-mapping.html,
     06-09-cascaded-shadows.html, 06-10-mipmaps.html, 06-11-transparency.html,
     06-12-hdr-tonemapping.html, 06-13-bloom-post-stack.html, 06-14-antialiasing.html,
     06-15-skybox-ibl.html, 06-16-frustum-culling.html, 06-17-frame-graph.html,
     06-17b-local-lights.html, 06-18-text-overlay.html, 07-01-euler-angles.html, 07-02-axis-angle.html,
     07-03-complex-numbers.html, 07-04-quaternions.html, 07-05-slerp.html,
     07-06-skeletal-animation.html, 07-07-sampling-blending.html, 07-08-audio.html,
     08-01-integrators.html, 08-02-forces-and-bodies.html, 08-03-angular-dynamics.html,
     08-04-collision-primitives.html, 08-05-gjk.html, 08-06-epa.html,
     08-07-contact-manifolds.html, 08-08-broadphase.html, 08-09-impulse-response.html,
     08-10-sequential-impulses.html, 08-11-constraints-and-joints.html, 08-12-ragdolls.html,
     08-13-character-controller.html
  docs/shared/: course.css, course.js
  engine/: CMakeLists.txt
  engine/include/engine/: engine.hpp
  engine/include/engine/anim/: clip.hpp, skeleton.hpp, skin.hpp
  engine/include/engine/asset/: asset_store.hpp, search_path.hpp
  engine/include/engine/audio/: mixer.hpp, sound.hpp, spatial.hpp
  engine/include/engine/core/: actions.hpp, assert.hpp, bench.hpp, clock.hpp, fixed_step.hpp,
     handle.hpp, input.hpp, log.hpp, pool.hpp, profile.hpp
  engine/include/engine/ecs/: camera.hpp, entity.hpp, hierarchy.hpp, pool.hpp, registry.hpp,
     view.hpp
  engine/include/engine/gfx/: antialias.hpp, blend.hpp, bloom.hpp, cascade.hpp, clip.hpp,
     colour.hpp, cubemap.hpp, cull.hpp, debug_draw.hpp, debug_lines.hpp, depth_buffer.hpp,
     draw_order.hpp, font.hpp, frame_graph.hpp, framebuffer.hpp, frustum.hpp, gltf.hpp,
     gpu_buffer.hpp, gpu_debug.hpp, gpu_device.hpp, gpu_mesh.hpp, gpu_overlay.hpp,
     gpu_pipeline.hpp, gpu_post.hpp, gpu_present.hpp, gpu_scene.hpp, gpu_shader.hpp,
     gpu_shadow.hpp, gpu_texture.hpp, gpu_uniform.hpp, hdr.hpp, image.hpp, instancing.hpp,
     light.hpp, material.hpp, mesh.hpp, microfacet.hpp, mipmap.hpp, obj.hpp, overlay.hpp,
     projector.hpp, raster.hpp, renderable.hpp, scene.hpp, shadow.hpp, soft_renderer.hpp,
     texture.hpp, viewport.hpp
  engine/include/engine/math/: axis_angle.hpp, bounds.hpp, complex.hpp, euler.hpp, mat2.hpp,
     mat3.hpp, mat4.hpp, quat.hpp, rotation.hpp, transform.hpp, vec2.hpp, vec3.hpp, vec4.hpp
  engine/include/engine/phys/: broadphase.hpp, cast.hpp, character.hpp, collide.hpp,
     constraint.hpp, convex.hpp, epa.hpp, gjk.hpp, inertia.hpp, integrate.hpp, manifold.hpp,
     ragdoll.hpp, rigid_body.hpp, shape.hpp, solver.hpp
  engine/include/engine/platform/: app.hpp, main.hpp, platform.hpp
  engine/include/engine/ui/: debug_ui.hpp
  engine/src/anim/: clip.cpp, skeleton.cpp, skin.cpp
  engine/src/asset/: asset_store.cpp, search_path.cpp
  engine/src/audio/: mixer.cpp, sound.cpp, spatial.cpp
  engine/src/core/: actions.cpp, clock.cpp, fixed_step.cpp, input.cpp, log.cpp, profile.cpp
  engine/src/gfx/: antialias.cpp, blend.cpp, bloom.cpp, cascade.cpp, clip.cpp, colour.cpp,
     cubemap.cpp, debug_draw.cpp, debug_lines.cpp, depth_buffer.cpp, draw_order.cpp, font.cpp,
     frame_graph.cpp, framebuffer.cpp, frustum.cpp, gltf.cpp, gpu_buffer.cpp, gpu_debug.cpp,
     gpu_device.cpp, gpu_mesh.cpp, gpu_overlay.cpp, gpu_pipeline.cpp, gpu_post.cpp,
     gpu_present.cpp, gpu_scene.cpp, gpu_shader.cpp, gpu_shadow.cpp, gpu_texture.cpp, hdr.cpp,
     image.cpp, instancing.cpp, mesh.cpp, mipmap.cpp, obj.cpp, overlay.cpp, raster.cpp,
     renderable.cpp, shadow.cpp, soft_renderer.cpp, texture.cpp
  engine/src/phys/: broadphase.cpp, cast.cpp, character.cpp, collide.cpp, constraint.cpp,
     epa.cpp, gjk.cpp, inertia.cpp, integrate.cpp, manifold.cpp, ragdoll.cpp, rigid_body.cpp,
     shape.cpp, solver.cpp
  engine/src/platform/: app.cpp, platform.cpp
  engine/src/ui/: debug_ui.cpp
  shaders/: bloom_bright.frag.hlsl, bloom_down.frag.hlsl, bloom_up.frag.hlsl,
     depth_probe.frag.hlsl, depth_probe.vert.hlsl, fullscreen.vert.hlsl, matrix_probe.frag.hlsl,
     mesh.frag.hlsl, mesh.vert.hlsl, overlay.frag.hlsl, overlay.vert.hlsl, scene.frag.hlsl,
     scene.vert.hlsl, scene_instanced.vert.hlsl, shadow.frag.hlsl, shadow.vert.hlsl,
     skybox.frag.hlsl, skybox.vert.hlsl, texture_probe.frag.hlsl, textured.frag.hlsl,
     textured.vert.hlsl, tonemap.frag.hlsl, triangle.frag.hlsl, triangle.vert.hlsl,
     uniform_probe.frag.hlsl, uniform_probe.vert.hlsl
  memory/: 59 dated session logs (memory/YYYY-MM-DD.md)
  scratch/: 1579 authoring sources, force-added (builders, fragments, figures, pins, harnesses)
  learnings/: foundations.md, module-2.md … module-8.md, tooling.md
  state/: updated.md, conventions.md, curriculum.md, completed.md, capabilities.md, decisions.md, files.md, roadmap.md, README.md

roadmap: RESHAPED 2026-09-08, AFTER TWO EXTERNAL REVIEWS OF THE PUBLISHED OUTLINE.
         (the whole of it: state/roadmap.md)

next: 6.18b — Compute Shaders: GPU Particles

      THE SECOND OF FOUR INSERTED LESSONS (post-Module 8 review, 2026-09-27); 6.17b
      landed 2026-09-28. Then 7.7b (glTF skins and clips), 8.14 (scene queries and a
      static mesh collider), then Module 9 from 9.1 Hardening the Build. Modules 6–8
      read "in progress" until all four land.

      AN INSERTION HAS A PROTOCOL — docs/_template/README.md §17 — and 6.17b is now
      its worked example (state/decisions.md, `found-by-617b`, and state/files.md):
        - scratch/replay_tree.py N.M     the student's tree at any lesson, from pins.
        - scratch/make_t617b.py          `tree` (the listings: tree at the previous
          lesson + the insertion's hunks) and `carry` (the hunks onto every LATER pin),
          each result PROVED by delta against the working tree. Copy it as
          make_t618b.py and change CHANGED/LATER_PINS; LATER_PINS for 6.18b are every
          lNN_ pin, after 6.18b in course order, of a path it changes.
        - scratch/_base617b/run_all.sh + classify.py   the additivity check: 26
          harnesses before/after; only timing lines may differ. Take the BEFORE run
          before the first edit. All 26 build at HEAD since 2026-09-28 (6.8's,
          6.9's and 6.16's were repaired and are now TRACKED — found-by-617b).
        - 6.18b sits between 6.18 and 7.1, so re-point build_618.py's next and
          build_71.py's prev. Update 6.18's module tree if a file is added.

      WHAT 6.18b INHERITS, AND MUST NOT RE-DERIVE:
        - 6.17b's first fragment STORAGE BUFFER (gpu_scene.cpp bind_local_lights):
          SDL numbers each resource kind from 0 on the C++ side while HLSL puts
          sampled textures, storage textures, storage buffers in ONE t sequence in
          space2 (SDL_gpu.h's layout comment). A COMPUTE shader's layout is different
          again — read SDL_gpu.h's SDL_CreateGPUComputePipeline comment, do not assume.
        - 6.17b's rule that the renderer, not the caller, sets a list's count; and
          the identity-element fallbacks (a zeroed one-record buffer) for a declared
          resource that must be bound even when empty.
        - 6.17's frame graph: a compute pass writing a buffer the scene pass reads is
          the graph's first non-texture resource. ⚠ VERIFY whether frame_graph.hpp can
          express a buffer at all before designing around it.
        - 4.6 (corrected): pushes are 32 KiB blocks, 4 KiB bound on Vulkan.

      PENDING CODE CORRECTIONS (decisions: pending-code-corrections): frustum.cpp's
      near-plane comment waits for whichever lesson lists it whole.

      The standing defect list and the notes for Module 9's multithreading
      lesson (now 9.3) are in state/roadmap.md, under "CARRIED FROM STATE.md".

```
