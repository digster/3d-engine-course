# Completed — the roll with its module notes

Moved verbatim from STATE.md's `completed:` block on 2026-09-27, when that file became
a compact resume key (CLAUDE.md §9). Append here; keep STATE.md's headline in step.

```text
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
        (5.10 was SPLIT from the curriculum's "Input Mapping, ImGui, and Debug
         Draw": three engine subsystems, each with its own design argument, and
         §3.8 forbids truncating. ImGui + debug draw became 5.11. Module 5 is now
         11 lessons, inside its stated 9-11, and the course total is 95.)
  - 5.11 Dear ImGui and the Debug Draw System
  - 5.12 Checkpoint: A Small 3D Game on the Public API
        (WRITTEN OUT OF ORDER — authored after 6.18, published between 5.11 and
         6.1. Added to the curriculum in the 2026-09-08 reshape and left unwritten
         while Module 6 ran. Everything it publishes had to compile against the
         5.11 engine, so it was written and run in a checkout of 9be6c96
         (scratch/era511) and ported forward; see conventions:era-split.)
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
        (6.9 and 6.10 were missing from this list until 6.11 added them — the
         list had not been appended to since 6.8 while `capabilities:` below was
         kept current, which is the append-and-merge rule being half-followed.
         Both are published and both are in the index; the omission was here.)
  - 6.13 Bloom and the Post-Processing Stack
  - 6.14 Antialiasing: Geometric and Shading
  - 6.15 Skybox and Image-Based Lighting
        (IT HAPPENED AGAIN, AND THE NOTE ABOVE DID NOT PREVENT IT. 6.13 and 6.14
         were both missing when 6.15 came to append, exactly as 6.9 and 6.10
         were when 6.11 did — same section, same cause, two lessons after the
         warning was written into the file. A NOTE IS NOT A CHECK. The durable
         fix is the one check-curriculum.py already embodies for the index:
         `completed:` should be derived from, or verified against, the
         `published` badges rather than maintained by hand. Filed as work, not
         as another note.)
  - 6.16 Frustum Culling and Instanced Submission
        (APPENDED AT THE TIME, not two lessons later, and the note above is why.
         The durable fix is still unbuilt — but check-curriculum.py's
         `published`-badge count IS now cross-checked against the hero stat, and
         that check failed on this lesson (71 vs 72) until the index was
         corrected. So one half of the drift this section has suffered twice is
         now caught by a tool; deriving `completed:` from the same source is the
         other half and remains work.)
  - 6.17 A Lightweight Frame Graph
  - 6.17b Local Lights: Point and Spot, and Their Shadows   (inserted; written 2026-09-28, after 8.13)
        (Appended at the time, and check-curriculum.py's badge-vs-hero-stat
         cross-check was run BEFORE the commit rather than after: it caught the
         orphan page, then the 4h-vs-5h subtotal drift when the hours moved, then
         the two dead `next` links on 6.16. Three catches on one lesson.)
        (ORDER FIXED BY 6.18: 6.17 was appended ABOVE 6.16 rather than after it,
         so the list read …6.15, 6.17, 6.16. Harmless to a reader and not
         harmless to the tool this section has twice asked for — anything that
         DERIVES this list from the index's badges will also want it ordered,
         and a hand-maintained list drifts in ordering as readily as in
         membership. Same root cause as the two omissions above, third instance.)
  - 6.18 Text and 2D Overlay Rendering
        (Appended at the time. check-curriculum.py caught five things on this
         lesson before the commit: the Module 6 subtotal (92 h vs 93), the hero
         hours (511 vs 512), the prose total, the Module 6 badge still reading
         `in progress` with all 18 lessons published, and a dead prereq link
         (`06-01-linear-srgb.html` — the file is `06-01-linear-and-srgb.html`).
         Five catches on one lesson, which is the most it has ever found.)
  - 6.18b Compute Shaders: GPU Particles   (inserted; written 2026-09-29, after 8.13)
        (Appended at the time, in lesson order — after 6.18, the lesson it follows — not
         at the end of the roll. check-curriculum.py caught the missing manifest entry and
         this line before the commit.)
  ===> MODULE 6 COMPLETE — 18 lessons, ~93 h, the longest module in the course.
       The renderer is modern: linear light, PBR, materials, glTF, normal maps,
       shadows and cascades, mipmaps, transparency, HDR + tonemapping, bloom,
       antialiasing, IBL, culling + instancing, a frame graph, and text. <===
       (2026-09-29: COMPLETE AGAIN at 20 lessons, ~162 h measured, once the review's two
        insertions landed — 6.17b's lamps and 6.18b's compute. Its project tree, which the
        module never had, is on 6.18b's Recap.)
  - 7.1  Euler Angles and Their Pathologies
        (Appended at the time. check-curriculum.py caught 6.18's TWO dead `next`
         links the moment the page existed — exactly what STATE predicted, and
         both were ported into the SOURCES (scratch/l618_body_a.html and
         build_618.py's TAIL) rather than into the HTML, so build_618 still
         reproduces. check-page.js caught three text-on-shape defects, one of
         which appeared at 1280 and NOT at 390: SVG labels scale with the
         viewport, so a collision can open at one width and not the other.)
  - 7.3  Complex Numbers Rotate the Plane
        (7.2's TWO dead `next` links repointed in the SOURCES —
         scratch/l72_body_a.html and build_72.py's TAIL — and build_72 rebuilt,
         so page and generator still agree. Planned at 5 h, shipped at 5; no
         module or course total moved, which is the first time in Module 7.
         FIXED IN PASSING: the Conventions page's own table of contents never
         listed §8c (axis-angle) — 7.2 added the section and not the link, and
         nothing checks a page's internal TOC against its own headings.
         check-curriculum.py verifies hrefs RESOLVE, not that every section is
         listed. Both 8c and 8d are in the TOC now; the missing check is a
         candidate for 9.10's documentation pass.)
  - 7.2  Axis-Angle and Rodrigues' Rotation Formula
        (7.1's TWO dead `next` links were repointed in the SOURCES —
         scratch/l71_body_a.html and build_71.py's TAIL — and build_71 rebuilt,
         so page and generator still agree. Planned at 3 h, shipped at 5, so
         Module 7 went ~38 -> ~40 h and the course ~512 -> ~514 h; index prose,
         hero stat and module subtotal all moved together and
         check-curriculum.py confirms.
         NOTE FOR 7.5: the index still lists 7.5 as "Slerp". 7.2 has now built
         rotation_slerp on mat3 and measured it. 7.5 is NOT thereby redundant —
         it is quaternion slerp, the double cover / shortest-arc sign choice,
         and nlerp-vs-slerp — but its one-line description is stale and its
         scope needs a decision. NOT TAKEN UNILATERALLY; flagged for the user.
         7.3 MAKES THIS MORE PRESSING, NOT LESS: it has now built complex_slerp
         AND complex_nlerp, derived the sec^2(Omega/2) schedule, measured the
         nlerp-vs-slerp gap at seven arcs, and turned the choice into a
         threshold on the arc. The double cover is derived too, in §7.5, from
         mirrors. What is genuinely left for 7.5 is the QUATERNION versions and
         the shortest-arc sign choice that the double cover forces — which is a
         real lesson, and a smaller one than "Slerp" suggests. STILL THE USER'S
         CALL; still not taken.)
  - 7.4  Quaternions, Derived
        (7.3's TWO dead `next` links were repointed in the SOURCES —
         scratch/l73_body_a.html and build_73.py's TAIL — and build_73 rebuilt,
         so page and generator still agree. Planned at 6 h, shipped at 7.
         THE STORAGE SWAP DID NOT HAPPEN, AND THAT IS A FINDING RATHER THAN A
         SLIP — see `decisions:` under `quat-swap-deferred`. transform.hpp now
         carries the measured reason and points at 7.5.
         AND THAT RESOLVES THE 7.5 QUESTION 7.2 AND 7.3 BOTH FLAGGED. Those
         two lessons asked whether "Slerp" was still a full lesson once
         rotation_slerp, complex_slerp and complex_nlerp existed. 7.4 answers
         it by handing 7.5 a second half: quaternion slerp + the shortest-arc
         sign the double cover forces + THE STORAGE SWAP, which is ~25 call
         sites across ten files with a decomposition decision inside one of
         them. 7.5 is now comfortably a lesson. STILL THE USER'S CALL to ratify
         the retitling; the work is scoped either way.)
  - 7.5  Slerp, and the Storage Swap
        (RETITLED from "Slerp" — the question 7.2, 7.3 and 7.4 each flagged,
         and 7.4 scoped. No lesson NUMBER moved, so §9's "approved lessons are
         never renumbered" is untouched; the index row was unpublished.
         Planned at 4 h, shipped at 7. 7.4's TWO dead `next` links were
         repointed in the SOURCES — scratch/l74_body_a.html and build_74.py's
         TAIL — and build_74 rebuilt, so page and generator still agree.
         FIFTEEN LISTINGS, TWELVE OF THEM MODIFIED, and the page is 774 KB —
         the largest in the corpus, past 04-08's 731 KB. That is the honest cost
         of narrowing a type fifteen files touch, and §8 forbids eliding it. If
         a lesson ever needs to list MORE than this, split it per §9 rather
         than trimming a listing.
         THE GOLDEN WAS A REAL INSTRUMENT AND CAME BACK IDENTICAL. Four edited
         files are in demo_scene.cpp's include closure, so the structural
         argument did not apply; hash E917C06C, ninth consecutive byte-identical
         run. The reason is measured rather than hoped: the mat3 -> quat -> mat3
         round trip's worst entry over 20,000 poses is 5.960e-07, which at three
         units out in a 960 px frame is 0.00014 PIXELS.
         verify_74 AND verify_73 WERE RE-RUN BECAUSE angle_between CHANGED
         UNDER THEM: 42/42 and 43/43, unmoved.)
  - 7.6  Skeletal Animation: The Skinning Math
        (7.5's TWO dead `next` links repointed in the SOURCES —
         scratch/l75_body_a.html and build_75.py's TAIL — and build_75 rebuilt,
         so page and generator still agree. Planned at 5 h, shipped at 6.
         THE FIRST NEW PUBLIC DIRECTORY SINCE 5.11: engine/anim/. The argument
         is asset/'s and ui/'s — animation is not a graphics subsystem; nothing
         in skeleton.cpp or skin.cpp mentions a framebuffer, a pipeline or a
         colour, and a character keeps moving when nobody is looking at it.
         THE UMBRELLA LINT DID NOT FIRE, for the first time in the run: both
         includes went in WITH the headers, in the same edit. So it was
         exercised deliberately instead — delete the two lines, reconfigure, and
         it names both and stops the build. Five catches in four lessons, then
         a miss in the check's favour; a check that stays quiet teaches nothing
         unless you make it speak.
         THE GOLDEN RAN AS A REAL INSTRUMENT AGAIN (math/transform.hpp is in
         demo_scene.cpp's closure — 70 files) and came back identical=YES, hash
         E917C06C, the TENTH consecutive byte-identical run.
         FIXED IN PASSING: `engine_use_assets(collector)` had been sitting in
         demos/CMakeLists.txt's GIMBAL block — wrong target named, and wrong for
         that target too, since collector generates every mesh it draws and
         gimbal loads nothing. It copied a directory neither program opens.
         Harmless, and exactly the kind of harmless that survives because
         nothing fails when it is wrong. build/demos/assets/ is still populated
         by sandbox, hello_cube, gltf_view and ecs_swarm, which is what the
         golden harness's search path relies on.)
  - 7.7  Sampling and Blending Animations
        (ADDED RETROSPECTIVELY BY 7.8, and the omission is the finding. 7.7
         shipped its page, wrote its `capabilities:` entry and put its filename
         in the `files:` published-pages block — and never gained a line here, so
         this roll and the module marker under it still said "6 of 8"
         while the index,
         the nav chain and the file manifest all said otherwise. check-curriculum
         passed throughout, because check 8 watches a list of FILENAMES and
         cannot see a missing lesson NUMBER. A new conversation resuming from
         this file would have been told to write 7.7 again.
         FIXED BY A CHECK, not by resolving to remember: `check_state_completed`
         is check 9 in docs/_template/check-curriculum.py, added by 7.8, and it
         caught both this and 7.8's own entry on its first run.
         Planned at 5 h, shipped at 6.)
  - 7.8  SDL3 Audio: Streams, Mixing, and 3D Sound
        (7.7's TWO dead `next` links repointed in the SOURCES —
         scratch/l77_body_a.html and build_77.py's TAIL — and build_77 rebuilt,
         so page and generator still agree. Planned at 5 h, shipped at 6, so
         Module 7 went 46 h -> 47 h and the course total 520 -> 521; the index
         meta, the prose line and BOTH hero stats moved with it, which
         check-curriculum verifies.
         MODULE BOUNDARY, and §7 of CLAUDE.md's reissue rule was honoured
         INCREMENTALLY rather than at the boundary: conventions.html gained §8i
         (audio) plus TEN rows in §11's verified-facts table, and
         math-toolbox.html gained §8d with five cards — each WITH its TOC entry,
         in the same edit, which is 7.3's rule.
         THE FOURTH NEW PUBLIC DIRECTORY SINCE 5.11: engine/audio/, and the
         first whose contents never touch a pixel. NOT under asset/, although
         one of its three files loads a file — 5.5 predicted this directory by
         name and predicted the wrong home for it.
         THE UMBRELLA LINT FIRED AGAIN, on the whole directory, three headers at
         once: SIX CATCHES IN FIVE LESSONS. 86 -> 89 public headers.
         NO GOLDEN RUN, and the graph was asked rather than assumed: nothing
         this lesson touched is in demo_scene.cpp's include closure. log.hpp is
         the only edited file anything else includes, and the edit is one
         enumerator plus comments.)
  ===> MODULE 7 COMPLETE <===
  - 8.1  Integrators: Why One Explodes
  - 8.3  Angular Dynamics: Torque and the Inertia Tensor
        (8.2's dead `next` link repointed in ALL THREE copies — the page,
         scratch/l82_body_a.html and build_82.py's TAIL — and build_82 rebuilt,
         so page and generator still agree; the diff was those four lines and
         nothing else. Planned at 6 h and SHIPPED AT 6, so no module subtotal and
         no course total moved: still 107 lessons, ~523 h. The first lesson since
         7.3 to land on its estimate.
         THE SIXTH NEW PUBLIC DIRECTORY-MEMBER, not a directory: inertia.{hpp,cpp}
         joins phys/, and the split is the dependency direction 8.2 promised —
         inertia.cpp knows about SHAPES and nothing about bodies; rigid_body.cpp
         knows about bodies and asks it what a shape weighs. 90 -> 91 headers.
         THE UMBRELLA LINT DID NOT FIRE, for the SECOND lesson running, which is
         the first time this file has been kept current across consecutive
         lessons since Module 5.
         mat3 GREW ARITHMETIC EIGHTY LESSONS LATE, and the reason is the lesson:
         for 80 lessons a mat3 was "where do the basis vectors land", under which
         reading operator+ is meaningless (the sum of two rotations is not a
         rotation). An inertia tensor is a mat3 under a DIFFERENT reading — a
         QUANTITY — and quantities add. operator+/-/unary-/scalar*, +=, -=, *=,
         outer, skew, trace, diagonal, asymmetry.
         THE DERIVATION'S OWN EXPRESSION IS THE ONE THAT MUST NOT SHIP.
         |r|^2*1 - outer(r,r) has diagonal |r|^2 - x^2 — a sum of three squares
         with one subtracted straight off again. At r = (1000, 0.001, 0) a float
         computes 1000000 - 1000000 and returns EXACTLY ZERO where the answer is
         1e-6: 100% wrong, and the 1e-6 was never IN the sum (ulp at 1e6 is
         0.0625). -[r]x[r]x is the same algebra, never forms the sum, and is
         nine multiplies instead of two matrix products. inertia_of_point ships
         that. The consequence is behavioural: a zero principal moment is a
         singular tensor, so every plank, rail and sword silently refuses to
         spin about its own length.
         AND THE SAME SHAPE OF LESSON A THIRD TIME: a derivation says WHAT to
         compute, not HOW. 6.16 found it in perspective() (A+1 with
         A = -1.003009 discards 8 bits, amplifies one ulp by 332x).
         FOUR GYROSCOPIC MODES, and the fourth reframes the other three.
         explicit = 8.1's determinant in the last place this engine integrates
         explicitly: |L| drift 3.2967e+11 at 30 Hz, 1.6233e+00 at 60.
         implicit (one Newton step in the body frame, Bullet's) = 8.1's OTHER
         determinant, 1/(1+h^2w^2), so it DAMPS: loses 35% at 60 Hz.
         momentum = integrate L, derive omega. The term was never physics — it
         is the price of choosing omega as the state — so it VANISHES. 8.0e-04.
         AND THE MOMENTUM COLUMN GETS WORSE AS h SHRINKS (3.86e-4, 8.05e-4,
         3.01e-3, 1.18e-2 at 30/60/240/960 Hz), which is the ONLY table in this
         course that does. Its error is not truncation — nothing in it
         approximates L; the engine stores omega and rebuilds L every step, so
         the error grows with the NUMBER of steps. Storing L is named as debt.
         CONSERVING THE RIGHT QUANTITY IS NOT ENOUGH. momentum conserves L BY
         CONSTRUCTION whatever it does with the orientation, so |L| cannot tell
         you the body is wrong — the SECOND conserved quantity can. Paired with
         a start-of-step omega, E drifts 97.1% and |omega| reaches 8.0350 where
         §8 DERIVES a bound of 4.0524..4.4880. One half step: 1.198e-04 and
         4.0525..4.4879, INSIDE the bound. A factor of 8,100.
         THE BOUND IS DERIVED, NOT OBSERVED, and that is the strongest check in
         the lesson: write u_i = L_i^2 and BOTH conserved quantities are linear
         in u, so the reachable set is a segment, |omega|^2 is linear on it, and
         a linear function on a segment extremises at the ENDS. Three
         candidates, one infeasible. Predicted 4.0524..4.4880, measured
         4.0527..4.4924.
         THE TENNIS RACKET THEOREM, predicted and measured: sigma = 4.8038 s^-1
         against 4.9058 / 4.8310 / 4.8128 / 4.8084 at 120/480/1920/7680 Hz —
         each refinement roughly quarters the error, which is the midpoint
         pairing's second order showing. Flips every 2.6651 s, forever.
         AND THE FIRST VERSION OF THAT MEASUREMENT MISSED BY 20% BECAUSE OF A
         FRAME: Euler's equations are BODY-frame equations, the engine stores
         omega in WORLD space, and for a spin about y the two perturbation
         components are carried around y at the spin rate. A world-space
         omega.x oscillates at 10 Hz with a growing envelope, and sampling it at
         a threshold crossing samples the oscillation. Fitted 3.8498.
         §12 MEASURED A memcpy FIRST: each arm was `body_world w = seed; ...`,
         putting a 557 KB pool copy inside the timed region. Every arm read
         34-58 ns/body against 8.2's 1-2 and every ratio came out near 1.
         AND THEN THE REAL FINDING, which is not an algorithm: the step built
         mat3_from_quat TWICE from the same quaternion, twenty lines apart, in
         world_inverse_inertia and angular_momentum. Hoisting it plus storing
         the forward tensor took the step 33.793 -> 18.778 ns/body, 1.80x, with
         no number on the page changing. sizeof(rigid_body) 60 -> 172 bytes,
         one cache line -> three, which is the number 8.10's solver will care
         about.
         Ten figures. check-page.js green at 1280 AND 390.)

  - 8.2  Forces, Gravity, and Linear Rigid Bodies
        (OPENS MODULE 8. 7.8's TWO dead `next` links repointed in the SOURCES —
         scratch/l78_body_a.html and build_78.py's TAIL — and build_78 rebuilt,
         so page and generator still agree; the diff was those four lines and
         nothing else. Planned at 5 h, shipped at 6, so Module 8 went 68 -> 69 h
         and the course total 521 -> 522.
         THE FIFTH NEW PUBLIC DIRECTORY SINCE 5.11: engine/phys/, and the second
         (after audio/) whose contents never touch a pixel. NOT under math/: the
         test is what a file KNOWS, and math/ knows about numbers — which is why
         it has no .cpp at all — while integrate.cpp knows that a velocity is
         metres per second and that a step size has a stability limit.
         THE UMBRELLA LINT FIRED AGAIN: SEVEN CATCHES IN SEVEN LESSONS, this
         time from somebody who had just read 7.8's note about the shape of the
         miss and reproduced it exactly. Reading the warning does not prevent it;
         a configure-time check does. 89 -> 90 public headers.
         NO REISSUE PASS AND NO conventions/math-toolbox EDIT: nothing here
         changes a course-wide convention or adds a reusable identity — the
         determinant argument is the lesson's own and lives on its page. The
         `integrator:` block above is the resume-key version.
         NO GOLDEN RUN: the include-closure graph was asked rather than assumed
         and nothing this lesson touched is in demo_scene.cpp's closure.
         TWO NEW CHECKS, both from the same fault line: check 10 (the index's
         <meta name="description">, stale at "94-lesson" since the reshape) and
         check 11 (each page's own Time block against its index row). Both were
         proved by breaking them on purpose.
         Ten figures. check-page.js green at 1280 AND 390 — it caught five
         labels sitting on their own curves' strokes, all moved OUTSIDE the plot
         at the source, which is figs_78.py's standing rule arriving with
         evidence.)
  - 8.4  Collision Primitives: Spheres, AABBs, OBBs, and the SAT
  - 8.5  GJK: Convex Distance from a Support Function
  - 8.6  EPA: Penetration Depth
  - 8.7  Contact Manifolds and Persistence
  - 8.8  Broadphase: A Uniform Grid
  - 8.12 Ragdolls: Joints on a Skeleton
         THE FIRST FILE IN phys/ THAT KNOWS A SKELETON: phys/ragdoll.{hpp,cpp}
         (the arrow points at the more general: anim/skeleton.hpp is maths
         only). constraint.* gains swing-twist and the cone; solver.* one
         stage. Twelve listings, the largest set since 8.10. demos/ragdoll:
         the trip (steered jog into sleeping crates, handoff, realigned
         return; [M] 7.7's walk, [Z] at rest, [A] realign), the hang ([U]
         sub-steps), the pile. Eleven sections, 68 checks, five refusals, two
         engine bugs fixed (see `updated:`).
  - 8.11 Constraints and Joints: Hinge and Ball-Socket
        (8.10's `next` repointed in all three copies — the page,
         scratch/l810_body_a.html and build_810.py's TAIL — and build_810
         rebuilt; the diff is those four lines and nothing else. PLANNED AT 6 h
         AND SHIPPED AT 6, the fourth lesson running on its estimate: still 107
         lessons, ~523 h. Module 8 is 11 of 13, 60 h of 70. check-page.js
         `pass: true` at 1280 AND 390 once figures 3 and 5 moved their
         annotations into legends (four labels, two of them on their own
         shapes). Eleven measured sections, 58 checks, 0.48 s.
         THE FIRST NEW TRANSLATION UNIT SINCE 8.9: phys/constraint.{hpp,cpp},
         BELOW the solver in the dependency graph — solver.hpp includes it and
         it includes nothing from the solver, which is the physical shape of the
         lesson's claim that a contact is a special case of a row.
         `pseudo_velocity` MOVED DOWN into it, as 8.10's own comment predicted.
         engine.hpp and engine/CMakeLists.txt one line each; 102 public
         headers.
         `contact_solver` RENAMED `constraint_solver`, the old name kept as an
         ALIAS so demos/stack and 8.10's harness compile unchanged — which is
         why demos/stack/main.cpp is NOT relisted. The contact half of
         solver.cpp is byte-for-byte 8.10's; `union_edge` was lifted out of the
         contact loop so that joints call the SAME function.
         demos/joints: three pendulums (the parallel axis, heard; a rope going
         slack), two arms at their stops (turnstile dead, gate bouncing — KICKED
         rather than motored, because a motor pushing into the stop swallowed
         the rebound), a weighted chain with [U] for sub-steps, and a hinged
         bridge carrying crates.)
  - 8.10 Sequential Impulses: Warm Starting, Islands, and Sleeping
        (8.9's `next` repointed in all three copies — the page,
         scratch/l89_body_a.html and build_89.py's TAIL — and build_89 rebuilt.
         PLANNED AT 6 h AND SHIPPED AT 6, the third lesson running to land on
         its estimate: still 107 lessons, ~523 h. Module 8 is 10 of 13, 54 h of
         70. check-page.js `pass: true` at 1280 AND 390. Eleven measured
         sections, 64 checks, zero failures; the harness runs in 9 s.
         NO NEW TRANSLATION UNIT AND NO NEW PUBLIC HEADER — the first lesson in
         Module 8 that leaves engine/CMakeLists.txt and engine.hpp alone. Ten
         listings all the same, at 10,231 lines, the largest set in the course,
         because six of them are files earlier lessons already printed and
         CLAUDE.md §8 says a file that changed appears whole.
         THE LARGEST SINGLE EDIT TO A PUBLISHED FILE'S BEHAVIOUR SO FAR:
         `body_world::step` split into `integrate_velocities` and
         `integrate_positions`, checked BIT-FOR-BIT against a private copy of
         the function it replaced — 400 mixed bodies, 120 steps, 400 of 400
         identical, worst position difference 0.000e+00.)
  - 8.9  Impulse Response: Restitution and Friction
        (8.3's dead `next` link repointed in ALL THREE copies — the page,
         scratch/l83_body_a.html and build_83.py's TAIL — and build_83 rebuilt,
         so page and generator still agree. Planned at 6 h and SHIPPED AT 6, the
         second lesson running to land on its estimate: still 107 lessons, ~523 h.
         THE FIRST HEADER THIS COURSE HAS EVER MOVED. engine/gfx/bounds.hpp ->
         engine/math/bounds.hpp, because phys/ wanted `aabb` and a physics header
         including a graphics one is the wrong arrow. The file includes only
         vec3.hpp and mat4.hpp; it was never about graphics; 6.8 simply needed it
         first. Cost: six include lines, one guard, and three gfx headers plus a
         demo touched. The umbrella lint caught the move before the compiler did.
         TWO NEW FILE PAIRS in phys/: shape.{hpp,cpp} (sphere | box, the obb
         placement type, bounds_of, as_obb, support) and collide.{hpp,cpp}
         (separation, interval/project/projected_radius/gap_on_axis, the five
         collide overloads, the boolean-only overlaps, closest_point). 92 -> 94.
         NO `shape` MEMBER ON rigid_body, on purpose: collision is a query over
         GEOMETRY, and the collider that pairs shape with body is 8.6's.
         A SEPARATION IS A CERTIFICATE, and the harness checks proofs rather than
         answers: 173,329/173,329 and 5,727/5,727 re-proved in double from the
         shape data alone, with a 41^3 witness hunt for the overlap direction
         (364/365, the miss a 4.485e-03 m sliver — an instrument out of
         resolution, which is a different thing from a wrong answer).
         Ten figures. check-page.js caught ten spilled figure footers and four
         labels on their own shapes; the footers went into the CAPTIONS, where
         that prose belonged, and the plot panel in fig 9 was drawn with box()
         instead of figs_71's stroke-only frame() — which that helper's own
         docstring warns about, and which only the visual pass caught.
         demos/collide: two panels, the fifteen candidate gaps as bars against a
         zero line, and [F] to throw the nine cross products away.)

  - 8.13 A Character Controller
        (8.12's `next` repointed in all three copies — the page,
         scratch/l812_body_a.html and build_812.py's TAIL — and build_812
         rebuilt byte-identical. PLANNED AT 5 h AND SHIPPED AT 6: M8 71 -> 72 h,
         course ~524 -> ~525 h; the index's M8 badge is now `complete`.
         MODULE BOUNDARY, honoured AT the boundary this time, because 8.5 to
         8.12 had added nothing to either page: conventions.html gains §9f-§9k
         (distance/depth/casts, contacts, the solver, joints, ragdolls, the
         controller) plus the missing TOC entry for §9e and 13 verified facts;
         math-toolbox.html gains §8h-§8l, 21 cards, and a Module 8 `complete`
         entry. The Module 8 project tree is in 8.13 §18, GENERATED from `git
         ls-files` by scratch/gen_l813_tree.py.
         Twelve figures; check-page.js caught 15 spills, 4 overlaps and 12
         labels on shapes in the first build (four scales far too large), all
         fixed at the source; the visual pass caught a curve joined across its
         own gap (fig 3) and two in-plane arrows drawn on top of each other.)
  ===> MODULE 8 COMPLETE — 13 lessons, ~72 h. Physics built, none imported. <===
```
