# Files — the manifest with its commentary

Moved verbatim from STATE.md's `files:` block on 2026-09-27, when that file became
a compact resume key (CLAUDE.md §9). Append here; keep STATE.md's headline in step.

```text
files:
  /: CLAUDE.md, README.md, ARCHITECTURE.md, LEARNINGS.md, PROMPT.md, LICENSE,
     .gitignore, CMakeLists.txt, STATE.md
  cmake/: EngineHelpers.cmake, Shaders.cmake
  shaders/: triangle.vert.hlsl, triangle.frag.hlsl,
            textured.vert.hlsl, textured.frag.hlsl,
            mesh.vert.hlsl, mesh.frag.hlsl,
            uniform_probe.vert.hlsl, uniform_probe.frag.hlsl,
            depth_probe.vert.hlsl, depth_probe.frag.hlsl,
            texture_probe.frag.hlsl,
            scene.vert.hlsl, scene.frag.hlsl,
            shadow.vert.hlsl, shadow.frag.hlsl                            [6.8]
            fullscreen.vert.hlsl, tonemap.frag.hlsl                       [6.12]
            bloom_bright.frag.hlsl, bloom_down.frag.hlsl,
            bloom_up.frag.hlsl                                            [6.13]
            skybox.vert.hlsl, skybox.frag.hlsl                            [6.15]
            scene_instanced.vert.hlsl                                     [6.16]
            overlay.vert.hlsl, overlay.frag.hlsl                          [6.18]
            matrix_probe.frag.hlsl
  engine/: CMakeLists.txt
  engine/include/engine/: engine.hpp                      (the umbrella)
  engine/include/engine/asset/: search_path.hpp, asset_store.hpp        [5.5]
  engine/include/engine/core/: assert.hpp, bench.hpp, clock.hpp, fixed_step.hpp,
            handle.hpp, input.hpp, log.hpp, pool.hpp, profile.hpp,
            actions.hpp                                                     [5.10]
  engine/include/engine/ecs/: entity.hpp, pool.hpp, registry.hpp, view.hpp   [5.8]
            hierarchy.hpp, camera.hpp                                       [5.9]
            (header-only — NO entry in engine/CMakeLists.txt. NOTE: ecs/pool.hpp's
             engine::ecs::pool<T> is a DIFFERENT container from core/pool.hpp's
             engine::pool<T>; see conventions:ecs-runtime.)
  engine/include/engine/gfx/: clip.hpp, colour.hpp, debug_draw.hpp,
            cascade.hpp                                                      [6.9]
            mipmap.hpp                                                      [6.10]
            blend.hpp                                                       [6.11]
            draw_order.hpp                                                  [6.11]
            hdr.hpp                                                         [6.12]
            gpu_post.hpp                                                    [6.12]
            bloom.hpp                                                       [6.13]
            antialias.hpp                                                   [6.14]
            cubemap.hpp                                                     [6.15]
            frustum.hpp, instancing.hpp                                     [6.16]
            frame_graph.hpp                                                 [6.17]
            font.hpp, overlay.hpp, gpu_overlay.hpp                          [6.18]
            renderable.hpp                                                  [5.12]
            debug_lines.hpp                                                 [5.11]
            depth_buffer.hpp, framebuffer.hpp, gpu_buffer.hpp, gpu_debug.hpp,
            gpu_device.hpp, gpu_mesh.hpp, gpu_pipeline.hpp, gpu_present.hpp,
            gpu_scene.hpp, gpu_shader.hpp,
            gpu_shadow.hpp                                                   [6.8]
            gpu_texture.hpp, gpu_uniform.hpp,
            cull.hpp                                                         [6.5]
            gltf.hpp                                                         [6.6]
            image.hpp, light.hpp,
            material.hpp                                                     [6.5]
            mesh.hpp,
            microfacet.hpp                                              [6.3, 6.4]
            obj.hpp, projector.hpp, raster.hpp,
            scene.hpp,
            shadow.hpp                                                       [6.8]
            soft_renderer.hpp, texture.hpp, viewport.hpp
  engine/include/engine/math/: mat2.hpp, mat3.hpp, mat4.hpp, transform.hpp,
            vec2.hpp, vec3.hpp, vec4.hpp,
            bounds.hpp                                          [6.8, MOVED 8.4]
              (WAS engine/gfx/bounds.hpp. It includes vec3.hpp and mat4.hpp and
               nothing else, and every line of it is geometry; it lived under
               gfx/ because 6.8's shadow fitting needed it first, which is how
               most headers end up where they are. 8.4 moved it because phys/
               wanted the same type and a physics header including a graphics one
               would have been the engine's FIRST arrow from simulation to
               rendering — entrenched two lessons later by the broadphase and the
               manifold cache. The alternative was a SECOND aabb in phys/, which
               is the duplication 6.8 created this file to end.
               6.8's PAGE STILL PRINTS THE OLD PATH WITH THE OLD CONTENT, and
               that is correct: a page is an archive of its own era. build_68.py
               splices a PIN, so the move did not disturb it — check-builders
               stayed 55/55 across the move.)
            euler.hpp                                                        [7.1]
            rotation.hpp, axis_angle.hpp                                     [7.2]
            complex.hpp                                                      [7.3]
              (THE PLANE'S CORNER, and deliberately outside the layering below.
               It includes mat2.hpp and vec2.hpp and nothing in the rotation
               layer, because it is one dimension down: `angle_between` here is
               SIGNED and `angle_between_rotations` there cannot be, so they are
               not overloads of each other. Written up in ARCHITECTURE.md so the
               split is not tidied away. It is also the TEMPLATE 7.4 writes
               quat.hpp against, function for function.)
              (rotation.hpp is the representation-INDEPENDENT layer — the metric
               moved into it from euler.hpp, which now includes it so no call
               site changed. 7.3 and 7.4 both have reason to add to it.)
  engine/include/engine/anim/: clip.hpp [7.7], skeleton.hpp, skin.hpp        [7.6]
  engine/include/engine/audio/: mixer.hpp, sound.hpp, spatial.hpp          [7.8]
            (THE FOURTH NEW DIRECTORY SINCE THE REFACTOR, and the first whose
             contents never touch a pixel. NOT under asset/, although sound.cpp
             loads a file: 5.5 predicted this directory by name ("it loads
             meshes, images, and in Module 7 sounds") and predicted the wrong
             home for it. The asset system's job is FINDING, CACHING and OWNING;
             decoding a WAV into floats is a subsystem's own business, exactly as
             gfx/image.cpp decodes a PNG and lives under gfx/.
             mixer.hpp includes the other two, so two of the umbrella's three
             new lines are redundant TO THE COMPILER and neither is redundant to
             a reader — engine.hpp is a table of contents.
             mixer.hpp is the ONE public header that includes <SDL3/SDL_audio.h>
             and it has no choice: the callback SDL runs is a member, and its
             declaration needs SDLCALL. Everything else — the mutex, the voice
             pool, the scratch buffer — is inside an incomplete `state`.)
            (A NEW DIRECTORY, and the argument is asset/'s in 5.5 and ui/'s in
             5.11: animation is not a graphics subsystem. Nothing in any of these
             files mentions a framebuffer, a pipeline or a colour, and a character
             keeps moving when nobody is looking at it. 7.7's clip sampling landed
             here beside them, exactly as this note predicted.
             clip.hpp includes skeleton.hpp — a sample needs the bind pose to fall
             back to — and nothing includes clip.hpp. Alphabetical order and
             dependency order agree here BY LUCK, and the CMake list is maintained
             alphabetically, so they will not keep agreeing.)
  engine/include/engine/platform/: platform.hpp, app.hpp,
            main.hpp   (NOT in engine.hpp — it defines the entry point)
  engine/include/engine/phys/: integrate.hpp [8.1], rigid_body.hpp [8.2],
                               inertia.hpp [8.3], shape.hpp [8.4],
                               collide.hpp [8.4], convex.hpp [8.5],
                               gjk.hpp [8.5], epa.hpp [8.6],
                               manifold.hpp [8.7],
                               broadphase.hpp                           [8.8]
                               solver.hpp                               [8.9]
                               constraint.hpp                          [8.11]
                               ragdoll.hpp                             [8.12]
                               cast.hpp, character.hpp                 [8.13]
            (convex.hpp is HEADER ONLY and deliberately so: a function pointer,
             a context pointer, an origin, four inline adapters and four DELETED
             rvalue overloads. It has no .cpp because there is nothing to
             compile — which is also the physical shape of its claim, that an
             algorithm needing no knowledge of its shapes needs no link against
             them.)
            (THE FIFTH NEW DIRECTORY SINCE THE REFACTOR. NOT math/: math/ knows
             about numbers and has no .cpp at all; this knows that a velocity is
             metres per second and that a step size has a stability limit.
             `motion` is deliberately TWO VECTORS — no mass, no orientation, no
             handle — which is what let 8.1 §5 be an argument about a 2x2 matrix
             rather than about an engine, and what makes it reusable for a
             spring-damper camera or a smoothed UI value. 8.2 gives a
             `rigid_body` a `motion`; it does not widen this.
             THERE IS NO implicit_euler ENUMERATOR, and the enum says why: the
             solve is a root find for anything non-linear, and its determinant is
             1/(1 + h^2 w^2), so it is costly AND lossy.
             8.3 SPLIT inertia.hpp OUT rather than widening rigid_body.hpp, and
             the split IS the dependency direction: inertia knows about SHAPES
             and nothing about bodies; rigid_body knows about bodies and asks it
             what a shape weighs. 8.4's collision shapes inherit that, and
             shape.hpp's inertia_of(shape, mass) is the ONE arrow it adds —
             inertia.hpp still knows nothing about collision geometry, because
             plenty of bodies get their tensor from an artist's number or a
             compound assembly instead.
             8.4 SPLIT shape.hpp FROM collide.hpp for the same kind of reason:
             a shape is geometry that may be asked what it weighs, a collide is a
             QUERY over geometry that must not know what anything weighs. There
             is deliberately NO `shape` member on rigid_body — the collider that
             pairs them is 8.6's, because a pair is only useful once something
             walks a list of them looking for candidates, and because a real
             collider also carries an offset, a material and a filter mask.
             phys/ DEPENDS ON math/ AND core/ AND NOTHING ELSE. That was free
             until 8.4 wanted `aabb`; see math/bounds.hpp above for what it cost
             to keep.
             integrate.hpp grew spin_rule/advance_orientation because an
             orientation advance IS an integrator — 8.1's file said 8.3 would
             add it — and it is the one function in the header that needs
             quat.hpp.)
  engine/include/engine/ui/: debug_ui.hpp                                   [5.11]
            (a new directory, same argument asset/ made in 5.5: tooling UI is not a
             graphics subsystem. Does NOT include <imgui.h> — see debug-ui.)
  engine/src/phys/: integrate.cpp [8.1], rigid_body.cpp [8.2],
                    inertia.cpp [8.3], shape.cpp [8.4], collide.cpp [8.4],
                    gjk.cpp [8.5], epa.cpp [8.6], manifold.cpp [8.7],
                    broadphase.cpp                                      [8.8]
                    solver.cpp                                          [8.9]
                    constraint.cpp                                     [8.11]
                    ragdoll.cpp                                        [8.12]
                    cast.cpp, character.cpp                            [8.13]
            (Everything that is NOT a template: the constant-acceleration
             overload, apply_drag/damping_factor, and the four diagnostics.
             Nothing in it is hot — the general stepper stayed in the header
             precisely so it could inline, and I.4 measures what crossing the
             archive boundary costs.)
  engine/src/audio/: mixer.cpp, sound.cpp, spatial.cpp                    [7.8]
            (mixer.cpp is the only file in this library that runs on a thread SDL
             owns, and every strange thing in it comes from that: no allocation,
             no logging, no unbounded wait, one mutex whose compromise is written
             down rather than hidden, and a `voice` struct completed here so that
             handle<voice> can exist without the layout ever being public.
             `mix_into` is PUBLIC so that it can be measured.)
  engine/src/anim/: clip.cpp [7.7], skeleton.cpp, skin.cpp               [7.6]
            (Translation units rather than headers for draw_order.cpp's reason:
             every function in them is a real loop over an array. skeleton.cpp
             runs once per JOINT and skin.cpp once per VERTEX, and 7.6 §10
             measures those at 2.34% and 97.66% of the pair.)
  engine/src/asset/: search_path.cpp, asset_store.cpp                   [5.5]
  engine/src/core/: actions.cpp [5.10], clock.cpp, fixed_step.cpp, input.cpp,
            log.cpp, profile.cpp
  engine/src/gfx/: cascade.cpp [6.9], mipmap.cpp [6.10],
            blend.cpp [6.11], draw_order.cpp [6.11],
            hdr.cpp [6.12], gpu_post.cpp [6.12], bloom.cpp [6.13],
            antialias.cpp [6.14],
            cubemap.cpp [6.15],
            frustum.cpp, instancing.cpp [6.16],
            frame_graph.cpp [6.17],
            renderable.cpp [5.12],
            clip.cpp, colour.cpp,
            debug_draw.cpp,
            debug_lines.cpp [5.11],
            depth_buffer.cpp,
            framebuffer.cpp, gpu_buffer.cpp, gpu_debug.cpp, gpu_device.cpp,
            gpu_mesh.cpp, gpu_pipeline.cpp, gpu_present.cpp, gpu_scene.cpp,
            gpu_shader.cpp,
            gpu_shadow.cpp                                                   [6.8]
            gpu_texture.cpp,
            gltf.cpp                    [6.6 — THE ONLY TU THAT SEES cgltf]
            image.cpp, mesh.cpp, obj.cpp,
            raster.cpp,
            shadow.cpp                                                       [6.8]
            soft_renderer.cpp, texture.cpp
  engine/src/platform/: platform.cpp, app.cpp
  engine/src/ui/: debug_ui.cpp   [5.11 — THE ONLY engine TU that includes <imgui.h>]
  demos/: CMakeLists.txt
  demos/common/: demo_scene.hpp, demo_scene.cpp, pong.hpp, pong.cpp
  demos/sandbox/: main.cpp
  demos/hello_cube/: main.cpp
  demos/pong/: main.cpp
  demos/ecs_swarm/: main.cpp                                              [5.8]
  demos/gltf_view/: main.cpp                                              [6.6]
  demos/collector/: main.cpp                                             [5.12]
  demos/gimbal/: main.cpp                                                 [7.1]
  demos/rig/: main.cpp                                                    [7.6]
           (The first demo that draws something which is NOT a rigid body. One
            generated tube, six joints, software-rasterized, no assets and no
            shaders. Its panel prints the smallest ring radius beside
            r cos(delta/2) and the two agree to five decimals in BOTH modes,
            which is what makes the derivation a claim about the blend rather
            than about twisting. NOTE ITS CLEAR COLOUR: (11, 12, 13), darker
            than gimbal's and plane's (16, 18, 24), because figs_45's quantiser
            treats a pixel as background — and emits nothing for it — only when
            EVERY channel is below 14. The obvious (12, 13, 17) has 17 on blue
            and came back as a mid-grey slab at (79, 84, 98).)
  demos/integrate/: main.cpp                                              [8.1]
           (PHASE SPACE, not a scene: watched as a bouncing dot all three rules
            look like a spring for the first few seconds, which is exactly why
            the bug in one of them ships. Runs `fixed_step` nested inside the
            app's own, so [Up]/[Down] change what physics does without changing
            how often the screen updates. Carries a Cohen-Sutherland clip (2.1's
            exercise, due seventy lessons later) and a CEILING on the view scale
            at three amplitudes — without it a diverging explicit Euler pulls the
            camera back without limit and every correct curve collapses to a dot.
            Its clear colour is (16, 18, 24), and figs_81.py's palette lists the
            demo's own constants because rle_rects SNAPS.)
  demos/spin/: main.cpp                                                   [8.3]
  demos/collide/: main.cpp                                                [8.4]
            (Two panels, and the right one is the lesson: all fifteen candidate
             gaps as bars against a zero line, so the reader watches the verdict
             flip at the instant the FIRST bar crosses it. [F] keeps the six face
             normals and throws the nine cross products away, which turns two
             visibly separated planks red. A negative claim — these fifteen found
             no gap, therefore no direction has one — is the hardest kind to
             believe from a statement.)
  demos/gjk/: main.cpp                                                    [8.5]
            (Two panels again, and the right one is the Minkowski difference
             OUTLINED BY TRACING ITS OWN SUPPORT FUNCTION — a convex set's shadow
             is convex and its support function is the original's composed with
             the transpose of the projection, so one loop draws a box, a capsule,
             a hull or a difference set with no case analysis. Drawing the
             support POINTS instead, which was the first attempt, draws almost
             nothing for a polytope: 900 directions on a box land on 8 corners.
             [S] steps the search one iteration at a time, which is why this demo
             drives the loop itself through `cso_support` and `reduce_simplex`
             rather than calling `gjk_distance` — a visualiser needs the
             iteration, not the answer. Preset 3 is the lesson: two boxes on one
             floor, and the simplex never leaves the plane y = 0.)
           (THE FIRST DEMO IN THE MODULE THAT DRAWS AN OBJECT rather than a graph
            of one, because the claim it settles is about which way something is
            POINTING and a tumbling box is not a trajectory. Wireframe, parallel
            projection, twelve edges sorted by one depth key — 3.1's painter's
            algorithm on twelve primitives — plus the three body axes in the
            course's x/y/z = red/green/blue and ONE AMBER LINE THAT DOES NOT
            MOVE, which is L. Amber deliberately not an axis colour: a reader who
            took it for a fourth body axis would have the picture backwards.
            THE RIGHT PANEL IS IN BODY AXES AND HAS TO BE. The engine stores
            omega in WORLD space, where the two perturbation components of a spin
            about y are carried around y at the spin rate — so a world-space plot
            is an unreadable 10 Hz oscillation with a growing envelope. The flip
            is a SIGN CHANGE only in the frame Euler's equations are written in,
            and getting that wrong is what made §10's first fit miss by 20%.
            NO GRAVITY AND NO FLOOR, which is the experiment rather than a
            missing feature: every claim here is about a body with NOTHING acting
            on it. Gravity would not change the tumble (its lever arm is zero)
            but it would carry the box out of the panel and invite the reader to
            wonder whether the falling was doing it.
            [G] cycles the four gyroscopic modes, which is the whole point: OFF
            is a flat line.)
  demos/epa/: main.cpp                                                    [8.6]
            (Two panels again, and the LEFT one is the new idea: a GHOST of the
             second shape drawn at the position the current answer would push it
             to, so an unconverged lower bound is a crate still buried in a wall
             rather than a number that is 60% of another number. The right panel
             is 8.5's, with two differences that say everything — the origin
             cross is INSIDE the outline, and the pink vector only ever gets
             LONGER. Builds its own polytope from `cso_support` for the same
             reason the gjk demo drove its own loop, and prints the real
             `epa_penetration`'s answer beside its own so the two can be seen to
             agree. [F] drops the flood fill and §7's failure can be watched.)
  demos/manifold/: main.cpp                                               [8.7]
  demos/character/: main.cpp                                             [8.13]
            Three scenes, the ragdoll demo's three-quarter view. [1] a course
            (8 cm curb, five stairs, a 30 cm drop, a 30 deg ramp down, two light
            crates and a heavy one, a turntable, a 1 cm wall dashed into at
            40 m/s; off the path a 55 deg slope, a lift, an obtuse corner),
            walked by [WASD] or the [O] autopilot; [K] clip rule, [N] snap,
            [T] step-up, [C] carry rule, [P] proxy steer/teleport. [2] the same
            script into a rigid capsule and the controller side by side (only
            the controller climbs the ledge; only the rigid one hangs on the
            wall). [3] three 120 deg corners, one clip rule each. `--shot`,
            `--auto`, `--clip`, `--no-snap`, `--no-step`, `--teleport`.
  demos/ragdoll/: main.cpp                                               [8.12]
            Three scenes, orthographic three-quarter view that follows the
            character. [1] trip: the jog steered into four crates asleep
            before it arrives (the kinematic-wake fix is why they move),
            tripped at 1.2 s, three seconds down, realigned slerp return,
            repeat; [M] 7.7's walk, [Z] hand over at rest, [A] realign off.
            [2] hang from one hand ([U] 8 sub-steps). [3] pile of five by
            island. Capsule silhouettes; the skinned skeleton drawn through
            them from the clip or from read_pose. `--shot` works headless.
  demos/joints/: main.cpp                                                [8.11]
            Four scenes. [1] three pendulums from one bar — a ball on a 1 m rod
            and a 2 m plank whose centre is ALSO 1 m down drift apart (the
            parallel axis, heard), and a ball on a rope goes slack a little
            below L/2 and flies. [2] a turnstile and a gate KICKED into their
            limits: the turnstile stops dead, the gate drifts back off its stop
            (the rho^8 rebound), [Right] to 32 sweeps and it stays; the first
            draft used motors, which swallowed the rebound. [3] a ten-link chain
            with a 1/10/100 kg end weight and [U] for eight sub-steps of one
            sweep. [4] a twelve-plank hinged bridge with crates, coloured by
            island. `--weight` and `--substeps` for headless shots.
  demos/stack/: main.cpp                                                 [8.10]
            Four scenes, and each is a measurement made watchable. [1] the
            ten-crate tower that leans and falls at eight iterations and stands
            at twenty; [2] the yard, with the crate's COLOUR being its ISLAND
            and [B] recomputing the labels with fixed bodies bridging (labels
            only — letting the bug into the solve would change what the demo is
            showing); [3] a crate 200 mm inside the floor where [C] cycles the
            correction; [4] four sleeping piles and [X] to throw a crate at
            them. A sleeping crate is drawn in a DIMMED version of its island
            colour rather than a flat grey, because a settled scene otherwise
            loses its whole partition exactly when a reader wants to see it.
            Per-scene camera: one framing for four scenes wastes most of the
            frame on three of them.
  demos/impulse/: main.cpp                                                [8.9]
           (THE FIRST DEMO IN THE COURSE IN WHICH SOMETHING MOVES BECAUSE THE
            PHYSICS SAID SO. Five collision demos before it each answered a
            question and changed nothing. Four scenes, each one measurement made
            watchable: [1] the bounce, with e^(2n)*h0 drawn as ghost lines so
            that [B] shows the ball climbing ABOVE its own prediction; [2] the
            slope, where [O] swaps the solve order and friction stops existing;
            [3] the roll, where the ball's spoke makes the spin visible and the
            panel prints v/v0 against 5/7; [4] the creep, which is the demo of
            why 8.10 exists. The inset at bottom right is the friction clip
            ITSELF, in units of mu*j_n, with this frame's tangential impulse
            plotted in it — press [F] and the square appears around the disc.
            NOTE THE CLIPPED LINE HELPER: this is the first demo whose world is
            unbounded, and without Liang-Barsky the ground ran across the ImGui
            panel and through the inset. It also follows the body along x,
            because a ball landing at 4 m/s crosses a six-metre view in a second
            and a half and the first version simply lost it.)
  demos/broadphase/: main.cpp                                             [8.8]
           (THE FIRST COLLISION DEMO WHOSE SUBJECT IS THE SCENE rather than a
            pair. Seen from directly above, so the lattice is visible; each cell
            shaded by C(n,2) rather than by occupancy, because the picture is
            meant to show where the WORK is and the work is quadratic. [B] runs
            `brute_force_pairs` beside the grid every frame and colours the
            verdict, which is the contract checked live at 60 Hz. [Up]/[Down]
            move entries and bucket tests by orders of magnitude and leave the
            PAIRS line untouched, which is 8.8 §7.3 in one gesture. [T] adds the
            ground plate and [Y] toggles the guard. Preset 4 — mixed sizes — is
            the one to look at: one proxy in seventeen is metres across and each
            draws a visible STAR of candidate pairs, which is §9's argument in
            miniature.)
            (Two panels, and the RIGHT one is the new idea: the contact seen
             FACE ON, in the reference face's own plane, with [S] applying one
             Sutherland-Hodgman side plane per press — the plane dashed, its
             outward normal a stub. The left panel draws what a solver needs and
             8.6 could not give it: the contact POINTS, and the closed loop of
             the support polygon they span, with its AREA printed, because the
             area is the number that decides whether a body can stand up.
             Preset 2 plus [Z] is the lesson in one gesture — tilt the crate and
             four points become two, the polygon collapses to a line and its
             area goes to zero. Preset 4 is the honest one: a ball gets ONE
             point and should. Runs its own clipper for the same reason gjk and
             epa drove their own loops, from `support_face` and
             `collide_manifold`, both public.)
  demos/bodies/: main.cpp                                                 [8.2]
           (WORLD-SPACE TRAJECTORIES, not phase space, because the claim it
            settles is one a player could see. Three masses (0.1, 1, 10 kg) on
            identical launches, and [M] cycles none / damping / drag: the first
            TWO produce the same picture and the third does not. EACH BODY DRAWS
            ONE DASH IN THREE — three solid curves on top of one another are
            indistinguishable from one, so the picture proving they agree would
            look exactly like a bug that lost two bodies. figs_82.py's `opoly`
            does the same thing in SVG with the same phases.
            The right panel is the frame half: four bodies dropped from the SAME
            WORLD POINT — via `place_in_parent`, because giving them all the same
            LOCAL position would compare four different drops — in four parent
            frames, of which only one is falling. It is the only place in this
            repository that integrates outside world space, on purpose, so the
            result can be looked at. Clear colour (16, 18, 24), and figs_82.py's
            palette lists the demo's own constants because rle_rects SNAPS.)
  demos/audio/: main.cpp                                                  [7.8]
           (The first demo here whose OUTPUT IS NOT THE PICTURE. The map is an
            EXPLANATION of the result — where each source is and what two gains
            fell out — because the result itself cannot be looked at. Faint rings
            at 2/4/8/16/32 m from the LISTENER, so each ring is one halving.
            Its grid is every FOUR metres and not every one: at 12 px/m a
            one-metre grid survived the figure pipeline's 3:1 peak downsample as
            a mesh with the content buried in it.
            No assets and no shaders: every sound is synthesised with make_tone,
            so the repository still carries no .wav and `--shot` is byte-for-byte
            reproducible. `--shot` uses open_offline, so a headless run opens NO
            DEVICE — silent, deterministic, and runnable with no sound card.)
  demos/plane/: main.cpp                                                  [7.3]
           (2-D, framebuffer only, no assets and no shaders. Four modes on
            [1..4]; mode 3 is the two-mirror construction and is the one worth
            running. It draws its grid at (24, 26, 32) ON PURPOSE — figs_511's
            peak sampler floors below luminance 30,000, so graph paper any
            brighter survives the 3:1 downsample and buries the construction
            inside it in the published figure.)
           (THE GAME. Links engine::engine directly and not demo_common, for the
            reason hello_cube and ecs_swarm give. No engine_use_shaders: it renders
            on the CPU, which is finding (b) in conventions:checkpoint-findings.)
  assets/: cube.obj, twisted.obj, quirks.obj, torus.obj, uv_grid.png,
           cube.gltf, cube.bin, shapes.glb                              [6.6]
  assets/fonts/: Karla-Regular.ttf, OFL.txt                             [6.18]
           (THE FIRST BINARY ASSET THE COURSE DID NOT GENERATE. 16,848 bytes,
            SIL Open Font License 1.1, copied byte for byte from Dear ImGui's
            misc/fonts/ — a dependency this repository already vendors. Chosen
            for size (Roboto is 162 kB, DroidSans 190 kB), for being a real
            OUTLINE font with a `glyf` table so stb_truetype actually rasterizes
            curves, for being proportional so 6.18 §4's advance arithmetic is
            not trivially uniform, and for carrying GPOS with no `kern` table —
            which is what makes 6.18 §3.2's kerning finding real rather than
            hypothetical. `.ttf` is not in .gitignore and does not need an
            exception, unlike `.obj` and `.bin`.)
           (GENERATED by scratch/make_gltf_assets.py — the course ships no
            third-party geometry, 3.5's rule, and the cube's positions are
            transcribed from k_cube_vertices so verify_66 §A is a real
            round trip. `.bin` is excepted in .gitignore for the same
            reason `.obj` is: the failure would be silent.)
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
                 03-07-specular-blinn-phong.html, 03-08-shading-models.html,
                 03-09-textures.html, 03-10-profiling-capstone.html,
                 04-01-how-gpus-work.html, 04-02-sdl-gpu-model.html,
                 04-03-shader-toolchain.html, 04-04-first-triangle.html,
                 04-05-vertex-buffers.html, 04-06-uniforms.html,
                 04-07-textures-and-depth.html,
                 04-08-porting-the-scene.html,
                 04-09-renderdoc.html,
                 05-01-the-refactor.html,
                 05-02-platform-layer.html,
                 05-03-logging-and-errors.html,
                 05-04-handles.html, 05-05-asset-system.html,
                 05-06-data-oriented-design.html,
                 05-07-ecs-storage.html,
                 05-08-ecs-runtime.html,
                 05-09-transform-hierarchy.html,
                 05-10-input-mapping.html,
                 05-11-imgui-debug-draw.html,
                 06-01-linear-and-srgb.html,
                 06-02-what-a-brdf-is.html,
                 06-03-microfacet-theory.html,
                 06-04-cook-torrance.html,
                 06-05-material-system.html,
                 06-06-gltf.html,
                 06-07-normal-mapping.html,
                 06-08-shadow-mapping.html,
                 06-09-cascaded-shadows.html,
                 06-10-mipmaps.html,
                 06-11-transparency.html,
                 06-12-hdr-tonemapping.html,
                 06-13-bloom-post-stack.html,
                 06-14-antialiasing.html,
                 06-15-skybox-ibl.html,
                 06-16-frustum-culling.html,
                 06-17-frame-graph.html,
                 06-18-text-overlay.html,
                 05-12-checkpoint-game.html,
                 07-01-euler-angles.html,
                 07-02-axis-angle.html,
                 07-03-complex-numbers.html,
                 07-04-quaternions.html,
                 07-05-slerp.html,
                 07-06-skeletal-animation.html,
                 07-07-sampling-blending.html
                 07-08-audio.html, 08-01-integrators.html,
                 08-02-forces-and-bodies.html, 08-03-angular-dynamics.html,
                 08-04-collision-primitives.html,
                 08-05-gjk.html, 08-06-epa.html,
                 08-07-contact-manifolds.html,
                 08-08-broadphase.html
                 08-09-impulse-response.html
                 08-10-sequential-impulses.html
                 08-11-constraints-and-joints.html
                 08-12-ragdolls.html
                 08-13-character-controller.html
                 (5.12 IS OUT OF SEQUENCE ON PURPOSE — Module 5 closed eleven
                  lessons after 5.11 and one after 6.18, and the list is
                  append-ordered rather than sorted so that the history is
                  legible. check-curriculum.py compares SETS, not order.
                  6.9 THROUGH 6.14 WERE ALL MISSING when 6.15 came to append —
                  the same half-followed append-and-merge the `completed:` list
                  above records twice. check-curriculum.py verifies the INDEX
                  against the filesystem; nothing verifies this list, which is
                  why it is the one that rots. 6.16 AND 6.17 WERE MISSING TOO
                  when 6.18 came to append, which is the FOURTH instance across
                  three sections of this file. The lists are not the problem;
                  hand-maintaining three parallel copies of the same fact is.
                  `check-curriculum.py` already knows the true set — it walks
                  docs/lessons/ — so the durable fix is to have it verify these
                  three sections too, and that is now the highest-value piece of
                  bookkeeping work outstanding.
                  *** DONE, 7.4. *** check-curriculum.py check 8 now compares
                  this block against docs/lessons/ and fails listing anything
                  missing. It found FIVE on its first run — 5.12, 7.1, 7.2, 7.3
                  and 7.4 itself — which is a fifth instance and the last one,
                  because it can no longer happen silently. The check is
                  ONE-WAY: it reports pages the manifest lacks and says nothing
                  about entries with no file, because that case is a DELETED
                  lesson, which has never happened and deserves a human rather
                  than a green tick. The `completed:` and `capabilities:` lists
                  are still unchecked — they are prose keyed on lesson id
                  rather than filename, so the same trick does not apply
                  directly. Candidate for 9.10.)
  docs/shared/: course.css, course.js      (THE stylesheet + page script; one copy each)
  docs/_template/: lesson-template.html, README.md, apply-shared.py, check-page.js
  scratch/ (REPAIRED HARNESSES, tracked since 2026-09-28): verify_65.cpp, verify_68.cpp,
           verify_69.cpp, verify_616.cpp and their build_verify_*.sh. Until then every lesson's working
           harness was gitignored and its only tracked record was the PIN printed on its
           page (l68_verify_68.cpp …) — which must stay as it shipped, because it compiles
           against the engine AS IT WAS at that lesson. A harness repaired to build at HEAD
           is a different file with a different job, so it is tracked under its own name.
           (8.4 moved bounds.hpp to engine/math/; 7.4 made transform::rotation a quat.)
  scratch/ (6.17b, not shipped with the engine — an INSERTED lesson): verify_617b.cpp,
           build_verify_617b.sh, figs_617b.py (13 figures; inputs l617b_{gpu,cpu,
           mask,gpu_nobias,floor_68,floor_snap,demo,demo_noshadow}.ppm from
           VERIFY617B_DUMP and gltf_view --lights --shot), build_617b.py,
           l617b_body_{a..d}.html, l617b_fig{1..13}.svg, and the LISTING PINS
           l617b_<path> (22 engine/shader/demo files + the harness).
           THE INSERTION TOOLING, reusable for 6.18b, 7.7b and 8.14:
           replay_tree.py (the student's tree at any lesson, from pins; --check),
           make_t617b.py (`tree`: listings = tree at 6.17 + 6.17b's hunks;
           `carry`: the hunks onto every LATER pin; each result PROVED by delta, with
           an undo-later-lines fallback when a later lesson rewrote a context line),
           _base617b/ (run_all.sh, classify.py: the additivity check across 26
           harnesses; run_shim.sh and its include shim were removed 2026-09-28 once
           6.8's, 6.9's and 6.16's harnesses were repaired to build at HEAD. Its before/ and after_final/ transcripts were
           local evidence and are NOT tracked; the .ppm figure inputs are not either,
           as for every lesson — the SVGs are the tracked product).
           (44 checks in eleven sections — §K, the sun's reach, added by the fix after
           the lesson; §D is the golden; §J's timing needs a
           release libengine.a.)
  scratch/ (8.13, not shipped with the engine): verify_813.cpp,
           build_verify_813.sh, figs_813.py, build_813.py, gen_l813_body_e.py
           (one-off: §13's snippets into the STATIC l813_body_e.html),
           gen_l813_tree.py (one-off: §18's project tree into l813_tree.html,
           already inside l813_body_f.html), pin_813.py (freezes the eleven
           listings from the working tree), l813_body_{a..f}.html,
           l813_fig{1..12}.svg, verify_813.log, and the LISTING PINS
           l813_<path> (eleven).
           (Eleven sections, 90 checks, 2.49 s; two consecutive runs identical
            but §K's timings. §B keeps a PRIVATE COPY of the cast stepped from
            GJK's upper bound, as its control.)
  scratch/ (8.12, not shipped with the engine): verify_812.cpp,
           build_verify_812.sh, figs_812.py, build_812.py, gen_l812_body_e.py
           (one-off: cut §14's snippets from the sources into the STATIC
           fragment l812_body_e.html), l812_body_{a..f}.html,
           l812_fig{1..10}.svg, verify_812.log, and the LISTING PINS
           l812_<path> (twelve).
           (Eleven sections, 68 checks, 1.84 s; two consecutive runs identical
            but §K's timings. §E keeps a PRIVATE COPY of 8.11's position pass.)
  scratch/ (8.11, not shipped with the engine): verify_811.cpp,
           build_verify_811.sh, figs_811.py, build_811.py,
           l811_body_{a..f}.html, l811_fig{1..10}.svg, verify_811.log, and the
           LISTING PINS l811_<path> (ten).
           (Eleven measured sections, 58 checks, 0.48 s. THE LOG IS THE
            CANONICAL RUN for every number on the page; two consecutive runs
            differ only in timings. §13's snippets were verified line by line
            against the sources they name. check-page.js found four labels in
            figures 3 and 5; the visual pass found four more defects it cannot
            see — ragged monospace columns (SVG collapses spaces), a curve
            leaving its frame, a prediction hidden exactly under the
            measurement, and a value sitting on its axis floor.)
  scratch/ (8.10, not shipped with the engine): verify_810.cpp,
           build_verify_810.sh, figs_810.py, build_810.py,
           l810_body_{a..f}.html, l810_fig{1..8}.svg, verify_810.log, and the
           LISTING PINS l810_<path> (TEN of them, the largest set in the
           course, because six are files earlier lessons already printed).
           (Eleven measured sections, 64 checks, 9 s. THE LOG IS THE CANONICAL
            RUN for every number on the page. Two consecutive runs were diffed:
            everything except the §J timings is byte-identical, which is what
            lets a figure transcribe a number rather than estimate one.
            check-page.js caught exactly ONE defect this time — figure 4's
            legend overlapping the table below it — which is the first lesson
            since 6.13 where it found fewer than four.
            THE SNAPSHOT INCLUDES THE MANIFOLD CACHE and that is not optional:
            the first version held only the bodies, and §B's sweep came out
            non-monotone because each trial inherited the impulses the PREVIOUS
            trial had written. Warm starting is exactly the feature that makes
            a physics step depend on more than its bodies.)
  scratch/ (8.9, not shipped with the engine): verify_89.cpp, build_verify_89.sh,
           figs_89.py, build_89.py, l89_body_{a..f}.html, l89_fig{1..8}.svg,
           verify_89.log, and the LISTING PINS l89_<path>.
           (Eleven measured sections, 82 checks. THE LOG IS THE CANONICAL RUN
            for every number on the page, and §K's timings are MINIMA over sixty
            repetitions with FIVE WARM-UPS DISCARDED — the first repetition
            after process start measures 173 ns where every later one measures
            139.5, so without the warm-up the published number depended on which
            sections the reader asked for. The deterministic counts are what
            every other conclusion rests on.
            check-page.js caught, in three rounds: four spilled labels, eight
            overlapping pairs, and nine labels sitting on their own shapes —
            including TWO LEGENDS DRAWN INSIDE THEIR PLOTS, which figs_81's
            legend() docstring already warns about. It did NOT catch the
            `manifest` table overflowing at 390px (that surfaced as
            pageScrollsX, not as a table finding) or figure 5's polar plot
            drawn with a SUPPRESSED ZERO, which made the lobe dramatic and the
            ratio — the one thing the plot exists to show — a lie. Only a
            rendered frame showed the second.)
  scratch/ (8.4, not shipped with the engine): verify_84.cpp, build_verify_84.sh,
           figs_84.py, build_84.py, check_84.mjs, shots_84.mjs,
           l84_body_{a..e}.html, l84_fig{1..10}.svg, _b84_p{0,1,1f,2}.ppm,
           verify_84.log, and the LISTING PINS l84_<path>.
           (check_84.mjs AND shots_84.mjs ARE NEW AND WORTH KEEPING. The first
            drives docs/_template/check-page.js at 1280 and 390 through the
            Playwright NODE LIBRARY rather than the MCP server, which matters:
            the MCP browser holds a single profile lock and refuses a second
            client, and a scripted two-viewport run has no business needing one.
            The second screenshots every figure.pdia into /tmp for the visual
            pass — and it earned its place immediately, because check-page.js was
            GREEN on a figure whose plot panel was drawn with box() instead of
            figs_71's stroke-only frame(). A filled panel is not a geometry error;
            it is just wrong, and only eyes catch it.
            THE HARNESS CARRIES THREE FUNCTIONS THE ENGINE DOES NOT SHIP, and the
            lesson measures all three: overlaps_faces_only (six axes),
            collide_unguarded (fifteen, no degeneracy guard) and overlaps_eps
            (the absR form with a settable epsilon). Keeping them here rather
            than behind a flag in the engine is the rule 8.3's four gyroscopic
            modes did NOT follow, and the difference is that those four are
            choices a caller might legitimately want.
            THE GROUND TRUTH IS overlaps_double — the same fifteen axes in
            double — and it is a legitimate reference ONLY because §F's question
            is about precision: a cross product of nearly parallel unit vectors
            has direction error eps/sin(theta), which is 1e-16/sin in double and
            1e-7/sin in float.)
  scratch/ (8.1, not shipped with the engine): verify_81.cpp, build_verify_81.sh,
           figs_81.py, build_81.py, l81_body_{a..e}.html, l81_fig{1..10}.svg,
           l81_demo.ppm, verify_81.log, and the LISTING PINS l81_<path>.
           (tools/figview81.sh is figview78.sh renamed. build_verify_81.sh still
            REFUSES an unoptimised libengine.a, and it matters more here than
            anywhere: section I's claim is that two rules cost the SAME, and a
            debug build's per-call overhead swamps the difference the claim is
            about.)
  scratch/ (7.7, not shipped with the engine): verify_77.cpp, build_verify_77.sh,
           golden_77.cpp, closure_77.py, figs_77.py, build_77.py,
           l77_body_{a,b,c,d,e}.html, l77_fig{1..10}.svg,
           l77_fade{0,5,5m,1}.ppm (the cross-fade renders figs_77 encodes for
           figure 6), tools/figshot77.py.
           TEN PINS, taken the moment the page first built and before anything
           else could touch those files. `demos/rig/main.cpp` is listed WHOLE and
           7.7 is the second lesson to edit it; `math/transform.hpp` is the
           most-edited file in the repository. build_77.py GENERATES its pin paths
           from LISTING_META via `_pin`, so a path added without a pin file beside
           it fails loudly at build time — which is the opposite of build_66.py's
           failure, where three inherited pins sat inert for a whole lesson and
           made the discipline LOOK satisfied.
           NO PROBE. Every measurement had an answer derivable on paper first, so
           they went into verify_77 as assertions. Two throwaway experiments DID
           get written and then folded back in rather than kept: one that measured
           std::fmod's quotient-dependence in isolation (now §I), and one that
           timed three bracket strategies (now §A.4, with the RETIRED two-loop
           walk kept beside them for the same reason quat.hpp keeps
           angle_between_by_cosine).
  scratch/ (5.7, not shipped with the engine): ecs_probe.hpp, bench_57.cpp,
           verify_57.cpp, measure_57.py, build_bench_57.sh, build_verify_57.sh,
           figs_57.py, build_57.py, l57_body_{a,b}.html, l57_fig{1..6}.svg
  scratch/ (5.8, not shipped with the engine): verify_58.cpp, build_verify_58.sh,
           figs_58.py, build_58.py, l58_body_{a,b}.html, l58_fig{1..6}.svg,
           l58_ecs_swarm.cpp  (A PINNED LISTING — see the note in build_58.py:
           5.9 rewrote demos/ecs_swarm/main.cpp, and re-running build_58.py to
           repoint one nav link spliced 5.9's demo into 5.8's page. The snapshot is
           byte-identical to that file at commit dfbdb0f. ANY FILE A LATER LESSON
           MODIFIES MUST BE PINNED THE SAME WAY.)
  scratch/ (5.10, not shipped with the engine): verify_510.cpp,
           build_verify_510.sh, figs_510.py, build_510.py,
           l510_body_{a,b,c}.html, l510_fig{1..5}.svg,
           l510_ecs_swarm.cpp AND l510_actions.hpp — TWO PINNED LISTINGS, both
           byte-identical to commit c697e14. The demo was expected. actions.hpp was
           NOT, and that is the lesson: it was 5.10's OWN NEW HEADER, so it read as
           finished, and 5.11 added masked_input to it. Rebuilding 5.10 spliced a
           class that mentions Lesson 5.11 into Lesson 5.10's listings (+173 lines,
           caught by `git diff` on the page).
           THE RULE IS NOT "PIN THE DEMO" — IT IS: PIN EVERY FILE THE PAGE LISTS
           THAT A LATER LESSON TOUCHES, and the cheap way to find out which is to
           re-run the builder and diff the page BEFORE shipping.
  scratch/ (5.9, not shipped with the engine): hier_probe.hpp, bench_59.cpp,
           bench_59.log, verify_59.cpp, build_bench_59.sh, build_verify_59.sh,
           figs_59.py, build_59.py, l59_body_{a,b,c}.html, l59_fig{1..6}.svg,
           l59_ecs_swarm.cpp  (A PINNED LISTING, byte-identical to that file at
           commit c1c5ea7 — 5.10 rewrote the demo, so build_59.py must not read the
           live one. SAME TRAP AS build_58.py; see its comment.)
           (NOTE: build_57.py was amended in 5.8 — it no longer stamps a STATE
            block, and it now byte-reproduces the shipped 05-07 page. Any future
            build_NN.py copied from it inherits the correct form.)
  scratch/ (6.1, not shipped with the engine): verify_61.cpp, build_verify_61.sh,
           figs_61.py, build_61.py, l61_body_{a,b,c}.html, l61_fig{1..6}.svg,
           probe_61.cpp (a THROWAWAY that established the facts before a line of
           the lesson was written: it printed the default swapchain format, asked
           which compositions this window supports, and tabulated what raw linear
           values look like when read as codes. Kept because "write the probe
           first" is the habit, not the file.)
           PINNED BY 6.2, BEFORE A LINE OF 6.2 WAS WRITTEN — and this is the first
           time the rule was applied on time rather than after a diff caught it.
           ALL SIX repository listings frozen at 373dd4b and verified byte-identical:
           l61_gpu_device.{hpp,cpp}, l61_gpu_uniform.hpp, l61_scene.frag.hlsl,
           l61_colour.{hpp,cpp}. Two were certain to be touched (gpu_uniform.hpp,
           scene.frag.hlsl); the other four were pinned anyway, because the file
           that bites you is the one you were sure was finished. Re-running
           build_61.py then produced a diff of exactly the four nav lines that were
           MEANT to change, which is what "pinned correctly" looks like.
  scratch/ (6.2, not shipped with the engine): verify_62.cpp, build_verify_62.sh,
           figs_62.py, build_62.py, l62_body_{a,b,c}.html, l62_fig{1..6}.svg,
           probe_62.cpp and probe_62b.cpp (THROWAWAYS, kept: the first established
           that pi*inv_pi is exactly 1.0f and that no 8-bit code moves; the second
           was written BECAUSE THE FIRST OVERTURNED A GUESS — I expected the raw
           Blinn lobe to break energy conservation at the shininess values the
           engine uses, and it does not, so 62b went looking for the failure that
           does survive and found it in the un-coupled SUM. Write the probe first,
           and write a second one when it tells you something you did not expect.)
           figview/ — one throwaway HTML page per figure, because scrolling a
           220 KB page to look at figure 5 lands unpredictably (5.11's note).
           PINNED BY 6.3, before a line of it was written. All four listings frozen:
           l62_light.hpp, l62_scene.frag.hlsl, l62_gpu_uniform.hpp and
           l62_verify_62.cpp. The three TRACKED files were verified against commit
           3507837 with `git show ... | diff - <pin>`; verify_62.cpp is GITIGNORED,
           so its pin is only a working-tree copy taken before the first edit —
           weaker provenance, worth knowing, and the reason to take it early.
           Rebuilding then gave a diff of exactly the two nav lines meant to move.
  scratch/ (6.3, not shipped with the engine): verify_63.cpp, build_verify_63.sh,
           figs_63.py, build_63.py, l63_body_{a,b,c}.html, l63_fig{1..6}.svg,
           probe_63.cpp and probe_63b.cpp (THROWAWAYS, kept. The first asked six
           questions; the second existed because the first's answers raised two
           DESIGN questions the lesson could not settle by taste — is the
           height-correlated Smith form worth shipping beside the separable one
           (yes: 1.715x), and what does the specular BRDF's own energy budget look
           like (0.3069 at full roughness). Same habit as 6.2: write another probe
           the moment one tells you something you did not expect.)
           figview/ — one throwaway page per figure; g1..g6.html this lesson.
           PINNED BY 6.4, before a line of it was written: l63_microfacet.hpp
           (verified against commit 8735ba5 with `git show ... | diff - <pin>`) and
           l63_verify_63.cpp, a working-tree copy taken FIRST because it is
           gitignored and that provenance cannot be recovered later. Rebuilding gave a diff of exactly the two nav
           lines meant to move — THIRD LESSON RUNNING, so it is the standard now.
  scratch/ (6.4, not shipped with the engine): verify_64.cpp, build_verify_64.sh,
           figs_64.py, build_64.py, l64_body_{a,b,c}.html, l64_fig{1..6}.svg,
           l64_golden_905BF27E.ppm (THE RETIRED GOLDEN, kept as history — the
           picture the engine made for fourteen lessons), worked_64.cpp (a
           throwaway that checks every worked example the page prints against the
           engine itself; CLAUDE.md §10 requires the arithmetic verified, and
           doing it by hand twice is not verification),
           probe_64.cpp, probe_64b.cpp and probe_64c.cpp (THROWAWAYS, kept, and the
           chain is the point. 64 measured the Jacobian, Schlick and the energy,
           and OVERTURNED THE PLAN: I expected Fresnel coupling to close the energy
           door outright, and it leaves 1.2031 at grazing. 64b asked whether that
           was the instrument (refine the grid: it CONVERGES, so no) and where it
           sat (the specular takes 0.40, the diffuse gives up 0.05). 64c tested the
           resulting DIAGNOSIS — that the missing factor is the light which fails
           to get OUT — by predicting an exit factor would kill it. It did, and
           then failed reciprocity, which is what chose the shipped form. THREE
           PROBES, EACH BECAUSE THE LAST ONE DISAGREED WITH ME.)
           check-pages.mjs is shared, not per-lesson; shot_figs.mjs likewise.
           PINNED BY 6.5, before a line of it was written: l64_microfacet.hpp,
           l64_light.hpp and l64_scene.frag.hlsl, all verified against commit
           e394559 with `git show ... | diff - <pin>`; l64_verify_64.cpp was taken
           during 6.4 itself, since it is gitignored.
           AND THE REBUILD CAUGHT SOMETHING THE RULE WAS NOT AIMED AT: three stale
           "Lesson 6.7" references (glTF is 6.6) that had been fixed in the SOURCES
           after the last build and never rebuilt into the page. The generalisation
           is wider than pinning — ANY GENERATED ARTIFACT NEEDS A
           REGENERATE-AND-DIFF AFTER THE LAST EDIT TO ITS INPUTS, not only when you
           remember to pin.
  scratch/ (6.15, not shipped with the engine): verify_615.cpp,
           build_verify_615.sh, figs_615.py, build_615.py,
           l615_body_{a,b,c}.html, l615_fig{1..7}.svg,
           probe_615.cpp and probe_615b.cpp (THROWAWAYS, kept — and the second
           one EARNED ITS KEEP twice over. The first asked six questions and got
           two of them WRONG: it reported the split sum 20% dark at roughness
           0.25 (Monte Carlo noise at 8,192 samples) and it found only the
           nearest INTEGER mip level, which cannot say whether the folklore
           mapping is good or lucky. probe_615b re-ran both at a converged
           sample count and continuously, and overturned the first. 6.2's rule —
           write another probe the moment one tells you something you did not
           expect — held for the fourth time.),
           sanity_615.cpp, golden_615.cpp, dbg_615.cpp, dbg_615b.cpp,
           dbg_seam.cpp and dbg_ray.cpp (SIX SMALL DIAGNOSTICS, all throwaways.
           dbg_615b is the one that found the ARGB/RGBA packing bug by printing
           the LUT's two channels beside the value they should have; dbg_seam is
           the one that proved the seam test was measuring nothing; dbg_ray is
           the one that explained why the demo's background is entirely ground.
           A four-line program that prints the intermediate is faster than any
           amount of reasoning about which of five things is wrong.),
           shot_615_env.ppm and shot_615_noenv.ppm (the with/without comparison),
           verify615.ppm (the golden this run produced).
           PINNED BY 6.15, before a line of it was written: l614_antialias.hpp,
           l614_antialias.cpp, l614_gpu_post.hpp and l614_gpu_post.cpp, all four
           verified against commit 9830dd3, plus l614_verify_614.cpp as a
           working-tree copy since it is gitignored. ALL FIVE WERE THEN CHECKED
           BY SUBSTRING AGAINST THE SHIPPED PAGE — which matters most for the
           gitignored one, whose only provenance is the copy, and the page is
           the only thing that can confirm the copy is still lesson-era text.
           Rebuilding gave a diff of exactly the two nav lines meant to move,
           FOURTEENTH LESSON RUNNING.
           (build_615.py PINS NOTHING YET and lists FIVE files whole. Lesson
            6.16 is FRUSTUM CULLING, which touches bounds.hpp, cull.hpp,
            gpu_mesh and the draw list — none of which appear in 6.15's
            listings. PIN ANYWAY, for two reasons: verify_615.cpp is gitignored
            so a working-tree copy now is the only cheap moment, and "nothing
            here is at risk" is exactly what was said about gpu_post.hpp before
            6.14 rewrote it.)
  scratch/ (6.14, not shipped with the engine): verify_614.cpp,
           build_verify_614.sh, figs_614.py, build_614.py,
           l614_body_{a,b,c}.html, l614_fig{1..7}.svg,
           probe_614.cpp (a THROWAWAY that decided the framing, third lesson
           running — and it found the CLOSED FORM by noticing that a bisected
           coefficient came out at 0.6436 for every roughness, which is not what
           an empirical constant does. A number that refuses to vary is a
           derivation you have not done yet.)
           TWO MEASUREMENTS WERE WRONG BEFORE THEY WERE RIGHT, both in the same
           way — the statistic was blind to the thing it was pointed at:
             (1) §D took the MEAN over the whole image and found it correct at 1x,
                 because errors of opposite sign cancel across pixels whose phases
                 differ. Aliasing is PER PIXEL, so the statistic must be.
             (2) §E averaged each method over 64 sub-pixel phases and compared the
                 means — but AVERAGING OVER PHASES IS ANTIALIASING, which flattered
                 the single sample to a 4.6% "error" at roughness 0.05. Fixed by
                 comparing per phase against that phase's own ground truth.
           BEFORE BELIEVING A NULL RESULT, CHECK THE MEASUREMENT CAN PRODUCE A
           NON-NULL ONE.
           check-page.js GAINED A SHAPE-SPILL CHECK, and it caught a real defect
           in this lesson's own figure 7 within a minute of being written. 6.13
           found that the spill test was TEXT-ONLY (its figure 3's legend box ran
           6 px off the viewBox and passed); 6.14 shipped the same defect again,
           which is what turned a note in LEARNINGS.md into a check. Strokes are
           excluded deliberately — a polyline reaching the edge of a plot box is
           correct — so only rect/circle/ellipse are tested. Whole site re-swept
           with it: 70 pages x 2 widths, 0 failures.
           FIGURE 7'S LAYOUT IS NOW COMPUTED FROM THE CANVAS WIDTH rather than
           hand-placed, which makes the overflow impossible rather than merely
           detected. That is the better fix when it is available.
           PINNED BY 6.14, before a line of it was written: l613_bloom.hpp,
           l613_bloom.cpp, l613_bloom_{bright,down,up}.frag.hlsl,
           l613_gpu_post.{hpp,cpp} and l613_tonemap.frag.hlsl against commit
           4637254; l613_verify_613.cpp as a working-tree copy. THE REBUILD DIFF
           WAS EXACTLY THE TWO NAV LINES, THIRTEENTH LESSON RUNNING.
           AND THE PREDICTION IN build_613.py's NOTE WAS RIGHT: gpu_post.{hpp,cpp}
           moved (MSAA needed a second colour target) and bloom.{hpp,cpp} did NOT,
           which is what made them the useful control.
           (build_614.py PINS NOTHING YET and lists FIVE files whole. 6.15 is
            SKYBOX AND IBL: a prefiltered environment map is a mip chain indexed by
            roughness, so microfacet.hpp — which now owns ggx_lobe_half_angle, and
            6.15 needs it to decide how much sky a level covers — and mipmap.hpp
            are both likely; gpu_texture.{hpp,cpp} is near certain, because a cube
            map is a texture TYPE this engine has never created. antialias.{hpp,cpp}
            should NOT move, which makes them the control. Pin first, and take
            verify_614.cpp early since it is gitignored.)
  scratch/ (6.13, not shipped with the engine): verify_613.cpp,
           build_verify_613.sh, figs_613.py, build_613.py,
           l613_body_{a,b,c}.html, l613_fig{1..7}.svg,
           probe_613.cpp (a THROWAWAY that decided the lesson's framing, exactly
           as probe_612 did — and it too was WRONG first, in two ways worth
           keeping. (1) Its firefly test compared a bright pixel at x and at x+1,
           which are in the SAME 2x2 block, so the box downsample returned
           identical results and the test was a structural null reporting 0.000%.
           A null result on a test that CANNOT vary is not evidence. (2) Its tail
           measurement sampled full-res radii 1 and 2, both of which land on
           half-res texel 0 or 1 — i.e. on the CORE, not the tail — and reported a
           slope of -4.39 that was an artefact of measuring the delta itself.
           Measure the tail in the units the data is stored in.)
           SEVEN figures. Figure 2 (the chain) was REDESIGNED after a visual pass:
           check-page.js passed it, but the labels were crowded, the arrows did
           not connect to the boxes, and "bright" sat on level 0's corner. THE
           AUTOMATED CHECKS DO NOT SEE CROWDING — look at every figure.
           AND check-page.js's SPILL CHECK TESTS TEXT ONLY: figure 3's legend BOX
           ran 6 px off the right edge of its viewBox and passed. A shape can
           spill where a label cannot.
           FIGURE NUMBERS DRIFTED AGAIN (positions 3, 5 and 7 disagreed with their
           captions) and `figOrder` caught it. Fixed by RENUMBERING plus ONE move:
           the chain diagram genuinely belonged in §3.2 beside the arithmetic it
           illustrates rather than in §5.4. One caption then referred to "figure
           2's tail" while BEING figure 2 — a self-reference the checker cannot
           see, because it only compares numbers to positions.
           THE TAG CLASS IS `new` / `modified`, NOT `mod`: check-page.js's
           `unknownTagClasses` rejects anything else. Note in passing that
           build_611.py and build_612.py pass ("new", "cpp"), which renders the
           LANGUAGE in the status pill — a small cosmetic defect in two shipped
           pages, left alone here because both are pinned and out of scope.
           HOURS MUST BE AN INTEGER in the index's `hrs` cell: check-curriculum's
           ROW_RE is `[0-9]+\s*h`, so "4.5 h" made the row invisible and the page
           read as an orphan with the module one lesson short. The lesson header
           and the index row were then aligned at 5 h, since nothing checks those
           two against each other.
           PINNED BY 6.13, before a line of it was written: l612_hdr.hpp,
           l612_hdr.cpp, l612_gpu_post.hpp, l612_gpu_post.cpp,
           l612_fullscreen.vert.hlsl and l612_tonemap.frag.hlsl against commit
           42f91ac with `git show ... | diff - <pin>`; l612_verify_612.cpp taken
           as a working-tree copy since it is gitignored. THE REBUILD DIFF WAS
           EXACTLY THE TWO NAV LINES, twelfth lesson running.
           AND THE PREDICTION IN build_612.py's NOTE WAS RIGHT ON ALL FOUR COUNTS:
           gpu_post.{hpp,cpp} moved (they had to — the header promised it),
           hdr.{hpp,cpp} moved (`resolve` grew the bloom), and
           fullscreen.vert.hlsl did NOT, which is what made it the useful control.
           (build_613.py PINS NOTHING YET and lists NINE files whole. 6.14 is
            ANTIALIASING: MSAA changes `sample_count` on every pipeline and every
            target, so gpu_post.{hpp,cpp} and the resolve are plausible; a
            post-process AA would instead ADD A STAGE, which is the first real
            test of §6's claim that gpu_post_stack stops scaling at four or five.
            bloom.{hpp,cpp} should NOT move, which makes them the control. Pin all
            eight repository files first, and take verify_613.cpp early since it
            is gitignored.)
  scratch/ (6.12, not shipped with the engine): verify_612.cpp,
           build_verify_612.sh, figs_612.py, build_612.py,
           l612_body_{a,b,c}.html, l612_fig{1..6}.svg,
           probe_612.cpp (a THROWAWAY, and it earned its keep twice: it
           established the peaks the lesson opens with, and its FIRST version was
           WRONG in an instructive way — it swept the eye through a plane that did
           not contain the mirror direction, so SHARPER surfaces reported LOWER
           peaks. A monotonic relationship coming out backwards is the diagnostic
           that a measurement is missing the thing it measures).
           PINNED BY 6.12, before a line of it was written: l611_blend.hpp,
           l611_blend.cpp, l611_draw_order.hpp, l611_draw_order.cpp against
           commit adec68d, plus l611_verify_611.cpp as a working-tree copy. THE
           REBUILD DIFF WAS EXACTLY THE TWO NAV LINES, eleventh lesson running —
           and the prediction was RIGHT: 6.12 did move blend.hpp (a doc note about
           where 6.11's rule applies), so an unpinned build_611.py would have
           spliced it into 6.11's page.
           NEW TOOL, and it found three SHIPPED defects on its first run:
           `figOrder` in docs/_template/check-page.js, which verifies that figure
           captions read 1, 2, 3… in DOM order. 6.12's own first build had the
           resolve diagram at 4 and the per-channel one at 5 while the body showed
           them the other way round — the SECOND time this drift has shipped
           (6.10 rendered fig4.png captioned "Figure 5") — and it happens because
           the NUMBERS live in build_NN.py and the ORDER lives in the body
           fragments. Sweeping every lesson page then found 02-05-matrices (3/4
           swapped), 03-10-profiling-capstone (4,5,6 as 6,4,5) and
           04-01-how-gpus-work (1,2,3,4 as 4,1,3,2). NOT FIXED IN THIS SESSION —
           out of scope, and the fix must edit the BODY FRAGMENTS and the prose's
           numbered references, not the rendered pages. Flagged as a task.
           (build_612.py PINS NOTHING YET and lists SEVEN files whole. 6.13 is
            bloom and the post-processing STACK, which is the second user of
            gpu_post.{hpp,cpp} — and that header says outright it is "deliberately
            not a stack" and that 6.13 asks the ownership question. Those two are
            near certain to move and hdr.{hpp,cpp} are likely (a bright-pass
            threshold and a downsample chain are both HDR operations wanting a
            home). fullscreen.vert.hlsl should NOT move, which makes it the
            control.)
  scratch/ (6.11, not shipped with the engine): verify_611.cpp,
           build_verify_611.sh, figs_611.py, build_611.py,
           l611_body_{a,b,c}.html, l611_fig{1..6}.svg, shot_figs.mjs (NEW, and
           SHARED: screenshots every `figure.dia` of a served page at 1280, for
           the visual pass check-page.js cannot do — 6.10 shipped two invisible
           markers for three build cycles because a line that was never drawn
           cannot overlap anything).
           PINNED BY 6.11, before a line of it was written: l610_mipmap.hpp,
           l610_mipmap.cpp against commit cf2d208, plus l610_verify_610.cpp as a
           working-tree copy. The prediction was RIGHT this time — 6.11 grew
           `average_2x2` a weight per texel and added `mip_options`, so an
           unpinned build_610.py would have spliced 6.11's code into 6.10's page.
           THE REBUILD DIFF WAS EXACTLY THE TWO NAV LINES, tenth lesson running.
           TWO FIGURES WERE REDRAWN AFTER check-page.js REJECTED THEM: figures 3
           and 5 had annotations placed in what looked like empty regions of a
           plot, and six of them landed on a polyline. Both now put their legend
           OUTSIDE the plot box. "Empty" judged by eye on a diagram whose curves
           cross most of the box is not a measurement.
           (build_611.py PINS NOTHING YET and lists FIVE files whole — all four
            new engine files plus the harness. 6.12 is HDR and tonemapping, which
            changes what a colour target IS, and `blend_over` clamps into [0,1]
            through `to_encoded` — so blend.{hpp,cpp} are near certain to move.
            Pin all four before writing a line of 6.12, and take verify_611.cpp
            early since it is gitignored.)
  scratch/ (6.8, not shipped with the engine): verify_68.cpp, build_verify_68.sh,
           figs_68.py, build_68.py, l68_body_{a,b,c}.html, l68_fig{1..7}.svg.
           SEVEN figures, not six, because §3.5 wants the ARTEFACT shown before
           the derivation and acne deserves its own picture. Figure 4 is a mock
           whose fringes are COMPUTED: acne on a ground plane is the level sets of
           a projective coordinate, which is why it swirls rather than striping,
           and drawing them from that formula is the difference between a diagram
           and a doodle.
           NO PROBE, the fourth lesson running. verify_68 needs a headless
           gpu_device (SDL_Init(VIDEO) first — without it the device silently does
           not come back and §G skips rather than fails, which is the most
           misleading kind of silence a harness can produce).
           PINNED BY 6.8, before a line of it was written: l67_texture.hpp,
           l67_mesh.cpp, l67_material.hpp and l67_scene.frag.hlsl against commit
           b5cbda8, plus l67_verify_67.cpp as a working-tree copy. THE REBUILD
           DIFF WAS EXACTLY THE TWO NAV LINES, seventh lesson running.
           AND ONE OF THE PREDICTIONS WAS WRONG, which is worth recording:
           material.hpp did NOT need a shadow-casting flag (whether an object
           casts is a property of the DRAW LIST, not of the surface — the demo
           expresses it with a subspan), and texel_space did NOT need a third
           value (a CPU shadow map is a depth_buffer of floats, not a texture of
           Uint32, so the question never arises). Only scene.frag.hlsl was
           certain, and it was.
           (build_68.py PINS NOTHING YET and lists EIGHT files whole. 6.9 is
            cascades and will edit shadow.{hpp,cpp}, gpu_shadow.{hpp,cpp},
            scene.frag.hlsl and probably bounds.hpp. Pin first, and take
            verify_68.cpp early.)
  scratch/ (6.7, not shipped with the engine): verify_67.cpp, build_verify_67.sh,
           figs_67.py, build_67.py, l67_body_{a,b,c}.html, l67_fig{1..6}.svg.
           NO PROBE, the third lesson running — every measurement had an answer
           derivable on paper first, so they went into verify_67 as assertions
           rather than into a probe as questions. NO NEW ASSET EITHER: the normal
           map is GENERATED by make_normal_bumps and inserted through
           insert_texture, which is 5.5's "an asset system that can only load is
           missing half its job" applied to the newest asset type.
           PINNED BY 6.7, before a line of it was written: l66_gltf.hpp,
           l66_gltf.cpp, l66_asset_store.hpp and l66_asset_store.cpp, all verified
           against commit 2c787a2 with `git show ... | diff - <pin>`;
           l66_verify_66.cpp and l66_make_gltf_assets.py taken as working-tree
           copies since both are gitignored. THE REBUILD DIFF WAS EXACTLY THE TWO
           NAV LINES, sixth lesson running.
           AND THE PINNING CAUGHT SOMETHING ELSE: build_66.py had inherited 6.5's
           THREE PINS verbatim when it was copied from build_65.py, and they sat
           inert for a whole lesson because none of those paths appeared in its
           LISTING_META. Harmless — and it made the discipline LOOK satisfied.
           WHEN COPYING build_NN.py, EMPTY LISTING_SOURCE AS WELL AS FIGURES.
           (build_67.py PINS NOTHING YET and lists texture.hpp, mesh.cpp,
            material.hpp, scene.frag.hlsl and verify_67.cpp WHOLE. LESSON 6.8 IS
            SHADOW MAPPING: scene.frag.hlsl is certain (the depth comparison is a
            fragment), material.hpp is likely (a shadow-casting flag is per-object
            state and 6.5's rule has to sort it), and texture.hpp is plausible — a
            depth map is neither colour nor ordinary data, so texel_space may need
            a third value. Pin first, and take verify_67.cpp early.)
  scratch/ (6.6, not shipped with the engine): verify_66.cpp, build_verify_66.sh,
           figs_66.py, build_66.py, l66_body_{a,b,c}.html, l66_fig{1..6}.svg,
           make_gltf_assets.py (NOT a throwaway — it is how assets/cube.gltf and
           assets/shapes.glb are produced, and re-running it must reproduce them
           byte-for-byte; it is gitignored with the rest of scratch/, which is a
           debt worth naming, because the assets it generates ARE committed).
           NO PROBE THIS LESSON, the second after 6.5. The measurements that
           mattered (the f0-lerp identity, the coupling ratio, the metal peak
           radiance) all had answers derivable on paper first, so they went
           straight into verify_66 §E as assertions rather than into a probe as
           questions. That is the right shape when you can predict the answer —
           6.2's rule was to write a probe the moment the first one disagrees with
           you, and none did.
           PINNED BY 6.6, before a line of it was written: l65_material.hpp and
           l65_scene.hpp, both verified against commit ebb3199 with
           `git show ... | diff - <pin>`; l65_verify_65.cpp taken as a working-tree
           copy since it is gitignored. THE REBUILD DIFF WAS EXACTLY THE TWO NAV
           LINES, fifth lesson running.
           (build_66.py PINS NOTHING YET and lists gltf.hpp, gltf.cpp,
            asset_store.hpp and asset_store.cpp WHOLE. LESSON 6.7 IS NORMAL
            MAPPING and needs the thing 6.6 deferred — a texture that knows whether
            it holds colour or linear data — so it will edit ALL FOUR: the
            wants_normal_texture flag becomes a real load, and load_texture grows a
            colour-space argument. Pin before writing a line of 6.7, and take
            verify_66.cpp AND make_gltf_assets.py early since both are gitignored.)
  scratch/ (6.5, not shipped with the engine): verify_65.cpp, build_verify_65.sh,
           figs_65.py, build_65.py, l65_body_{a,b,c}.html, l65_fig{1..5}.svg,
           check-tags.py (SHARED, and new: see below),
           shot_65_*.ppm and swarm_65_*.ppm (the three-stage and clean-build
           comparisons; throwaways, kept as the evidence for the refactor claim).
           NO PROBE THIS LESSON — the first since 6.1. A refactor's question is
           "did the output move?", which the golden answers directly; there was no
           measurement whose answer could surprise. The one number that DID need
           taking was the header compile times, and taking it overturned the
           argument it was meant to support (see conventions:material).
           (build_65.py PINS NOTHING YET and lists material.hpp and scene.hpp
            WHOLE. LESSON 6.6 LOADS glTF and will most likely grow material.hpp —
            a material from a file needs a name, a cache and an identity that
            survives a reload. Pin before writing a line of 6.6, and take
            verify_65.cpp's copy EARLY since it is gitignored.)
  scratch/ (5.12, not shipped with the engine): verify_512.cpp,
           build_verify_512.sh, golden_512.cpp, figs_512.py, build_512.py,
           l512_body_{a,b,c,d}.html, l512_fig{1..8}.svg,
           l512_shot.ppm and l512_shot_giz.ppm (THE ERA RENDERS figs_512.py
           run-length encodes for figure 8 — taken from the era511 build, NOT
           the live one, because the two differ on 91.9% of pixels),
           port_512.py (THE ERA->LIVE PORT, and the record of what eleven
           lessons of drift cost: 26 real lines of 1,485. Its line-counting was
           WRONG the first time — it compared the files position by position, so
           an inserted line scored every line after it as changed and it reported
           1,107 of 1,196 for a file whose real delta is nineteen. difflib
           aligns first; and a second pass separates a genuine change from a
           RE-INDENT, because folding two fields into a brace moves a twenty-line
           comment sideways and crediting Module 6 with that would be dishonest),
           era511/ (A FULL 9be6c96 CHECKOUT, configured against the main tree's
           fetched SDL3/stb/imgui via FETCHCONTENT_SOURCE_DIR_*. This is where
           the lesson was written, compiled and run; see conventions:era-split.
           Rebuild with:
             mkdir -p scratch/era511 && git archive 9be6c96 | tar -x -C scratch/era511
             cmake -S scratch/era511 -B scratch/era511/build \
               -DFETCHCONTENT_SOURCE_DIR_SDL3=$PWD/build/_deps/sdl3-src \
               -DFETCHCONTENT_SOURCE_DIR_STB=$PWD/build/_deps/stb-src \
               -DFETCHCONTENT_SOURCE_DIR_IMGUI=$PWD/build/_deps/imgui-src
           then re-apply 5.12's six files from the scratch/l512_* pins.)
           PINNED BY 5.12 ITSELF, before the page was first built, which is
           earlier than the usual discipline and necessary here: the pins are
           ERA COPIES, so reading the repository paths live would have published
           Module 6 spellings on day one. Nine pins, six from era511 and three
           (the harness) from the working tree, since scratch/ is gitignored.
  scratch/ (5.11, not shipped with the engine): verify_511.cpp,
           build_verify_511.sh, figs_511.py, build_511.py,
           l511_body_{a,b,c}.html, l511_fig{1..7}.svg
           (figs_511.py adds peak_sample() — a MAX downsampler, because 5.7's
            box_sample AVERAGES and a 1-pixel debug line inside a 3x3 block
            contributes one ninth of its brightness, so 152 crisp lines average
            into a haze. It also floors dim cells to true black, because
            figs_45.quantise() calls a cell background only when ALL THREE channels
            are under 14 and this demo's background is (12, 14, 20) — blue is 20,
            so every empty cell snapped to a tint and the panel came out solid
            slate with the lines invisible inside it.
            build_511.py PINS NOTHING YET. It lists BOTH CMakeLists.txt files, the
            umbrella header and actions.hpp, and Module 6 will edit at least the
            first three — so it will need pins, and the warning in it says so.)
  memory/: 2026-07-16.md … 2026-08-25.md, 2026-08-25-b.md, 2026-08-25-c.md,
           2026-08-26.md, 2026-08-26-b.md, 2026-08-29.md, 2026-09-02.md, 2026-09-02-b.md,
           2026-09-02-c.md, 2026-09-04.md, 2026-09-05.md, 2026-09-06.md, 2026-09-07.md,
           2026-09-08.md
           (ONE FILE PER DATE. 6.2, 6.3 and 6.4 all landed on 2026-09-06 and all
            three are sections of that one file — never a -b suffix for a same-day
            session. 6.7 and 6.8 both landed on 2026-09-08 and share that file the
            same way.)
  (retired: src/ — the whole directory. hello.cpp.)
```
