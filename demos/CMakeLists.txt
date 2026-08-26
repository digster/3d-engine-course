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

# ---- sandbox: every demo the course has built so far ----------------------
#
# Lessons 1.8 through 4.9, on [Tab] and a handful of flags. It is still far too
# big, and Lesson 5.1 did not fix that on purpose: splitting it needs an
# application layer to split it INTO, and that is Lesson 5.2. What this lesson
# took out of it was everything that was never the demo's to begin with.
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
