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
