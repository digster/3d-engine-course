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
