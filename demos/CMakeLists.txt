# demos/CMakeLists.txt — programs built ON the engine.
#
# Lesson 5.1. Everything here links `engine::engine` and may include only its
# public headers; none of it is compiled into the library. That rule is not
# advice, it is the include path (see engine/CMakeLists.txt), and it is what
# makes "does the public API suffice?" a question the build answers rather than a
# question somebody remembers to ask.

# ---- demo_common: the content, so it can be shared ------------------------
#
# The scene the demos draw, and Pong. A LIBRARY rather than a header in each
# demo, for a reason with four receipts: Lessons 4.6 through 4.9 each had a
# verification harness that transcribed `build_scene` by hand, because it was
# trapped in an anonymous namespace inside main.cpp. Copy number four could have
# drifted from the original and nothing in the build would have noticed.
#
# It is deliberately NOT part of the engine. Nothing in it would survive contact
# with a real game: it exists so that several programs can draw the same picture.
add_library(demo_common STATIC
    common/demo_scene.cpp
    common/pong.cpp)

# PUBLIC, so anything linking demo_common can include "demo_scene.hpp" by that
# name. The directory is `common/` and not the demos/ root, which keeps the
# spelling short and — more usefully — makes it impossible for one demo to
# include another demo's private headers by accident.
target_include_directories(demo_common PUBLIC common)
target_link_libraries(demo_common PUBLIC engine::engine)
engine_set_warnings(demo_common)

# ---- pong: Lesson 1.8's game, as its own program --------------------------
#
# Lesson 5.2. Until this lesson Pong was a branch inside sandbox's five-way
# switch, reachable only by pressing [Tab] four times, and it was there for one
# reason: a demo needs a loop and there was exactly one loop in the repository.
# `engine::app` is a loop you do not have to write, so the branch came out.
#
# It links demo_common for the simulation (which was always shared) and gets the
# engine transitively — demo_common links it PUBLIC. No assets and no shaders:
# Pong draws with nothing but a framebuffer, which is the whole of what Module 1
# had and is still enough for a complete game.
add_executable(pong pong/main.cpp)
target_link_libraries(pong PRIVATE demo_common)
engine_set_warnings(pong)

# ---- sandbox: every demo the course has built so far ----------------------
#
# Lessons 2.1 through 4.9, on [Tab] and a handful of flags. Lesson 5.1 said the
# splitting needed an application layer to split it INTO; Lesson 5.2 built one
# and took the first demo out (see `pong` above).
#
# What sandbox KEPT is a hand-written main() and its own `while` loop, and that
# is a decision rather than a leftover. It chooses between three incompatible
# loops at runtime — software, GPU probe, GPU scene — and it is the instrument
# every measurement in Modules 2 to 4 was taken with. A program with that job
# wants the library arrangement, and Lesson 5.2 §6 argues it out. It still uses
# `engine::platform`, so the SDL lifecycle is no longer its problem; what it
# owns is the loop.
# NOTE THE NAME. Through Lesson 4.9 this executable was called `engine`, and
# that name is now taken by the library — by the thing it links, which is
# precisely the confusion the old name was papering over. It is `sandbox` from
# here on, and it lives at build/demos/sandbox.
add_executable(sandbox sandbox/main.cpp)
target_link_libraries(sandbox PRIVATE demo_common)
engine_set_warnings(sandbox)
engine_use_assets(sandbox)
engine_use_shaders(sandbox)

# ---- hello_cube: the smallest program that proves the boundary -------------
#
# A hundred lines against the public API and nothing else. It is the acceptance
# test for this entire lesson: if a spinning, lit, textured cube cannot be
# written from outside the library, then the library does not have an API — it
# has an inside that some programs happen to be able to reach.
add_executable(hello_cube hello_cube/main.cpp)
target_link_libraries(hello_cube PRIVATE engine::engine)
engine_set_warnings(hello_cube)
engine_use_assets(hello_cube)

# ---- gltf_view: Lesson 6.6's acceptance test for the ASSET SYSTEM ---------
#
# `hello_cube` proved a program outside the library could draw. This one proves
# a program outside the library can LOAD — one call to `asset_store::load_model`
# and a loop that turns handles into `scene_object`s, with no path assembly, no
# parser, no cache and no lifetime rule anywhere in it.
#
# It links `engine::engine` directly rather than `demo_common`, on purpose: it
# shares no content with the other demos, and a demo that reaches for the shared
# scene when it does not need one is how demo_common stops being optional.
add_executable(gltf_view gltf_view/main.cpp)
target_link_libraries(gltf_view PRIVATE engine::engine)
engine_set_warnings(gltf_view)
engine_use_assets(gltf_view)

# ---- ecs_swarm: Lesson 5.8's world, built out of components ---------------
#
# The acceptance test for the ECS, and the answer to "what is this FOR" at a
# scale where the performance argument does not apply. It links `engine::engine`
# directly rather than `demo_common`, for the same reason `hello_cube` does: a
# program that reaches for the shared demo scene is not proving anything about
# the public API. Everything it draws it builds itself, out of five component
# types the engine has never heard of.
#
# `engine_use_assets` because `asset_store` brings a search path with it, not
# because this demo loads a file — every mesh in it is generated.
add_executable(ecs_swarm ecs_swarm/main.cpp)
target_link_libraries(ecs_swarm PRIVATE engine::engine)
engine_set_warnings(ecs_swarm)
engine_use_assets(ecs_swarm)

# ---- collector: Lesson 5.12's checkpoint game -----------------------------
#
# The first program in this repository written to be a GAME rather than a
# demonstration, and therefore the first real test of the boundary Lesson 5.1
# drew. Every other demo was written by the person who had just built the
# subsystem it shows off; this one was written against the published headers and
# kept a list of everything it could not find.
#
# `engine::engine` directly and NOT `demo_common`, for the reason hello_cube and
# ecs_swarm give: a program that reaches for the shared demo scene is not proving
# anything about the public API. Every mesh in it is generated, every component
# but six is the engine's, and it includes nothing else.
#
# No `engine_use_shaders`: it renders on the CPU. That is itself one of the
# lesson's findings — the ECS produces components, the GPU renderer consumes
# `gpu_draw_item`, and there is no public path between them (§8).
add_executable(collector collector/main.cpp)
target_link_libraries(collector PRIVATE engine::engine)
engine_set_warnings(collector)

# ---- gimbal: three rings and a number -------------------------------------
#
# Lesson 7.1. The smallest demo since `pong`, and deliberately so: it draws one
# object and three circles, and everything else on screen is a measurement of the
# pose that object is in. A bigger scene would have made the picture prettier and
# the claim harder to check.
#
# It links `engine::engine` and NOT `demo_common`, which is the same statement
# `collector` makes — a program built to test the public API does not get to
# reach for the shared demo scene. What it needs is a cube, and `mesh.hpp`
# generates one.
add_executable(gimbal gimbal/main.cpp)
target_link_libraries(gimbal PRIVATE engine::engine)
engine_set_warnings(gimbal)
# FIXED IN PASSING, Lesson 7.6: an `engine_use_assets(collector)` stood here —
# in the gimbal block, naming the wrong target, and wrong for that target too.
# `collector` builds every mesh it draws (see its own section above, which says
# so) and `gimbal` loads nothing either, so the line copied a directory that
# neither program opens. Harmless, and exactly the kind of harmless that survives
# because nothing fails when it is wrong. `build/demos/assets/` is still
# populated by sandbox, hello_cube, gltf_view and ecs_swarm, which is what the
# lesson harnesses' search path relies on.

# ---- rig: one surface, six joints ------------------------------------------
#
# Lesson 7.6. The first demo in this repository that draws something which is
# NOT a rigid body: every other program here places whole meshes with whole
# transforms, which is what `ecs::hierarchy` is for and what it can do. A skinned
# tube is the case that needs `engine::anim`, and it exists to be looked at while
# the numbers on its panel are read.
#
# `engine::engine` directly and NOT `demo_common`, the same statement `gimbal`,
# `collector` and `ecs_swarm` make: a program written to exercise the public API
# does not get to reach for the shared demo scene. Every vertex in it is
# generated, and the ImGui panel comes with the engine.
#
# No `engine_use_assets` and no `engine_use_shaders`. It loads nothing and
# compiles nothing: the mesh is built in `build_tube()` and the renderer is the
# software one, which is deliberate — the argument of this lesson is arithmetic
# that happens before anything is drawn, and a CPU path lets a headless `--shot`
# reproduce it byte for byte on any machine.
add_executable(rig rig/main.cpp)
target_link_libraries(rig PRIVATE engine::engine)
engine_set_warnings(rig)

# ---- plane: the complex plane, drawn ---------------------------------------
#
# Lesson 7.3, and the smallest demo in the repository — smaller than `pong`,
# which held that title from 5.2. It draws in two dimensions with nothing but a
# `framebuffer`, `draw_line` and `math/complex.hpp`, and that is the argument
# rather than an economy: the whole of a plane rotation is two floats and a
# multiplication rule, and a target with no assets, no shaders and no
# `demo_common` says so more plainly than the lesson's prose can.
#
# No `engine_use_assets` and no `engine_use_shaders`. It loads nothing and
# compiles nothing; everything on screen it computes.
add_executable(plane plane/main.cpp)
target_link_libraries(plane PRIVATE engine::engine)
engine_set_warnings(plane)

# ---- audio: a listener, three emitters, and a map --------------------------
#
# Lesson 7.8, and the first demo in this repository whose OUTPUT IS NOT THE
# PICTURE. The map is an explanation: it shows where the sources are and what
# gains fell out of that, so the numbers on the panel can be checked against
# something. A demo for a subsystem you cannot look at has to be built the other
# way round from every other one here.
#
# `engine::engine` directly and NOT `demo_common`, the same statement `gimbal`,
# `rig`, `plane`, `collector` and `ecs_swarm` make. Every sound it plays it
# synthesises with `make_tone`, so this target needs no assets and the repository
# carries no .wav — which also means a `--shot` run is byte-for-byte reproducible
# on any machine.
#
# No `engine_use_assets` and no `engine_use_shaders`.
add_executable(audio audio/main.cpp)
target_link_libraries(audio PRIVATE engine::engine)
engine_set_warnings(audio)

# ---- integrate: three rules on one spring, in phase space ------------------
#
# Lesson 8.1. The picture is a PHASE PLOT rather than a scene, for the reason
# the file's own header gives: watched as a bouncing dot, all three integrators
# look like a spring for the first few seconds, which is exactly why the bug in
# one of them ships. Plotted as position against velocity the difference is the
# first thing you see — one curve spirals outward and two close on themselves.
#
# `engine::engine` directly and NOT `demo_common`, the same statement `gimbal`,
# `rig`, `plane`, `collector`, `ecs_swarm` and `audio` make. It loads nothing
# and computes everything on screen, so a `--shot` run is byte-for-byte
# reproducible on any machine.
#
# No `engine_use_assets` and no `engine_use_shaders`.
add_executable(integrate integrate/main.cpp)
target_link_libraries(integrate PRIVATE engine::engine)
engine_set_warnings(integrate)

# ---- bodies: three masses, three ways to fall ------------------------------
#
# Lesson 8.2, and the picture is a WORLD-SPACE TRAJECTORY rather than a phase
# plot, because the claim it settles is one a player could see: two of the three
# resistance models produce identical trails for three very different masses and
# the third does not. The right-hand panel is the other half of the lesson —
# four bodies dropped in four different parent frames, drawn where they actually
# end up — and it is the only place in this repository where something is
# integrated in a space that is not world space, on purpose, so that the result
# can be looked at.
#
# `engine::engine` directly and NOT `demo_common`, the same statement `gimbal`,
# `rig`, `plane`, `collector`, `ecs_swarm`, `audio` and `integrate` make. It
# loads nothing and computes everything on screen, so a `--shot` run is
# byte-for-byte reproducible on any machine.
#
# No `engine_use_assets` and no `engine_use_shaders`.
add_executable(bodies bodies/main.cpp)
target_link_libraries(bodies PRIVATE engine::engine)
engine_set_warnings(bodies)

# ---- spin: a box that flips with nothing touching it -----------------------
#
# Lesson 8.3, and the picture is a WIREFRAME BODY beside a body-frame plot —
# the first demo in this module that draws an object rather than a graph of one,
# because the claim it settles is about which way something is POINTING. A
# tumbling box is not a trajectory.
#
# The right-hand panel is the other half, and it has to be in BODY axes: the
# engine stores `omega` in world space, where the two perturbation components of
# a spin about y are carried around y at the spin rate, so a world-space plot of
# them is an unreadable 10 Hz oscillation with a slowly growing envelope. The
# flip is a sign change only in the frame Euler's equations are written in.
#
# `engine::engine` directly and NOT `demo_common`, the same statement `gimbal`,
# `rig`, `plane`, `collector`, `ecs_swarm`, `audio`, `integrate` and `bodies`
# make. It loads nothing and computes everything on screen, so a `--shot` run is
# byte-for-byte reproducible on any machine.
#
# No `engine_use_assets` and no `engine_use_shaders`.
add_executable(spin spin/main.cpp)
target_link_libraries(spin PRIVATE engine::engine)
engine_set_warnings(spin)

# ---- collide: fifteen candidate axes, live ---------------------------------
#
# Lesson 8.4. The Separating Axis Theorem makes a NEGATIVE claim — these fifteen
# directions found no gap, therefore no direction has one — and a negative claim
# is the hardest kind to believe from a statement. So the right-hand panel draws
# all fifteen candidates as bars against a zero line, and the reader watches the
# answer flip at the instant the first bar crosses it.
#
# The [F] key is the demo's argument: it throws away the nine edge-edge
# candidates, keeps the six face normals, and in the crossed-planks preset
# reports two visibly separated boxes as touching. That is the whole reason the
# candidate list is fifteen rather than six, and it is far more convincing seen
# than derived.
#
# `engine::engine` directly and NOT `demo_common`, the same statement `gimbal`,
# `rig`, `plane`, `collector`, `ecs_swarm`, `audio`, `integrate`, `bodies` and
# `spin` make. It loads nothing and computes everything on screen, so a `--shot`
# run is byte-for-byte reproducible on any machine.
#
# No `engine_use_assets` and no `engine_use_shaders`.
add_executable(collide collide/main.cpp)
target_link_libraries(collide PRIVATE engine::engine)
engine_set_warnings(collide)

# ---- gjk: the Minkowski difference, searched --------------------------------
#
# Lesson 8.5. GJK's correctness argument is short; the SUBSTITUTION it rests on
# is not easy to believe from a statement. Two shapes are replaced by their
# Minkowski difference, and "are these touching?" becomes "does that set contain
# the origin?". The right-hand panel draws the difference as a cloud of support
# points with the origin as a cross, and [S] steps the search one iteration at a
# time so the simplex can be watched growing around it.
#
# Preset 3 is the one to sit with: two boxes standing on the same floor produce a
# difference that is completely FLAT, because a shared up axis puts y = 0 on
# every vertex of it. That is why the same preset can never produce a
# tetrahedron, which is 8.6's problem and named here.
#
# `engine::engine` directly and NOT `demo_common`, the same statement `gimbal`,
# `rig`, `plane`, `collector`, `ecs_swarm`, `audio`, `integrate`, `bodies`,
# `spin` and `collide` make. It loads nothing and computes everything on screen,
# so a `--shot` run is byte-for-byte reproducible on any machine.
#
# No `engine_use_assets` and no `engine_use_shaders`.
add_executable(gjk gjk/main.cpp)
target_link_libraries(gjk PRIVATE engine::engine)
engine_set_warnings(gjk)

# ---- epa: the balloon in the dark room --------------------------------------
#
# Lesson 8.6. "Expand the polytope toward its closest face" is a sentence; the
# thing it describes is a shape growing until it presses against a wall you
# cannot see, and that is worth watching rather than reading. The right panel
# draws the Minkowski difference with the origin INSIDE it, the expanding
# polytope in gold, and the vector to its closest face in pink — which, unlike
# GJK's `v`, only ever gets longer.
#
# The left panel earns its place too: it draws a GHOST of the second shape at
# the position the current answer would push it to. Step the search and watch the
# ghost stop overlapping. An unconverged lower bound is a shape that has not
# quite come free.
#
# Preset 3 is the one to sit with, and it is 8.5's preset 3 one lesson on: two
# crates on a floor, where GJK hands over a flat triangle and the seed has to
# find two more vertices before EPA can start. [F] drops the flood fill from the
# visibility test — the textbook formulation — and the depth goes visibly wrong.
#
# `engine::engine` directly and NOT `demo_common`, the same statement `gimbal`,
# `rig`, `plane`, `collector`, `ecs_swarm`, `audio`, `integrate`, `bodies`,
# `spin`, `collide` and `gjk` make. It loads nothing and computes everything on
# screen, so a `--shot` run is byte-for-byte reproducible on any machine.
#
# No `engine_use_assets` and no `engine_use_shaders`.
add_executable(epa epa/main.cpp)
target_link_libraries(epa PRIVATE engine::engine)
engine_set_warnings(epa)

# ---- manifold: one contact is not a contact ---------------------------------
#
# Lesson 8.7. 8.6's demo ended on a single vector — how deep, and which way — and
# a solver cannot rest a crate on a vector. This one draws the SET: the contact
# points, and the support polygon they span, whose AREA is the number that
# decides whether a body can stand up at all.
#
# The right panel is the one to sit with. It shows the contact face on, in the
# reference face's own plane, and [S] applies one Sutherland-Hodgman side plane
# at a time — the plane drawn dashed with its outward normal, so the algorithm
# 3.3 wrote for the near plane can be watched doing a different job.
#
# Preset 2 and the [Z] key are the lesson in one gesture: tilt the crate and the
# manifold drops from four points to two, the support polygon collapses to a
# line, and its area goes to zero. Preset 4 is the honest one: a ball has no flat
# feature, gets one contact point, and should.
#
# `engine::engine` directly and NOT `demo_common`, the same statement `gimbal`,
# `rig`, `plane`, `collector`, `ecs_swarm`, `audio`, `integrate`, `bodies`,
# `spin`, `collide`, `gjk` and `epa` make. It loads nothing and computes
# everything on screen, so a `--shot` run is byte-for-byte reproducible on any
# machine.
#
# No `engine_use_assets` and no `engine_use_shaders`.
add_executable(manifold manifold/main.cpp)
target_link_libraries(manifold PRIVATE engine::engine)
engine_set_warnings(manifold)

# ---- broadphase: the cheap question, asked of everything ---------------------
#
# Lesson 8.8. The first collision demo in the course whose subject is the SCENE
# rather than a pair. It draws the grid from directly above, shades each cell by
# how many proxies are in it, and reports the counts the stats struct keeps —
# entries, bucket tests, the three reasons a candidate is thrown away, and the
# pairs that survive.
#
# [B] is the one to leave on. It runs `brute_force_pairs` beside the grid every
# frame and colours the verdict, so the contract — a superset, never a subset —
# is checked live rather than asserted in a comment.
#
# Then hold [Up] or [Down]. Entries move by orders of magnitude, bucket tests
# move the other way, the milliseconds trace the cost curve of Lesson 8.8 §5 —
# and the PAIRS line does not move at all, which is the whole point.
#
# `engine::engine` directly and NOT `demo_common`, the same statement `gimbal`,
# `rig`, `plane`, `collector`, `ecs_swarm`, `audio`, `integrate`, `bodies`,
# `spin`, `collide`, `gjk`, `epa` and `manifold` make. It loads nothing and
# computes everything on screen, so a `--shot` run is byte-for-byte reproducible
# on any machine.
#
# No `engine_use_assets` and no `engine_use_shaders`.
add_executable(broadphase broadphase/main.cpp)
target_link_libraries(broadphase PRIVATE engine::engine)
engine_set_warnings(broadphase)
