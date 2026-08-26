# CMakeLists.txt — the top of the build.
#
# Lesson 5.1 turned this file inside out. Through Module 4 it described one
# executable and everything that went into it, which is exactly right for a
# program. This project is no longer a program: it is a LIBRARY and the programs
# built on it, and this file's job is now to acquire the dependencies, declare the
# rules everybody shares, and then get out of the way.
#
#     engine/   the library. Public headers under engine/include/engine/.
#     demos/    executables that link it and may include only those headers.
#     tools/    (Module 8) the editor and the asset pipeline.
#
# Read this file, then engine/CMakeLists.txt, then demos/CMakeLists.txt. Together
# they are about a hundred lines of actual instruction, and the boundary the whole
# lesson is about is drawn by three of them.

# 3.24 is our floor: new enough for modern FetchContent behaviour and the
# target/property model we rely on. Older CMake will refuse to configure, which
# is what we want — a clear failure, not a subtle one.
cmake_minimum_required(VERSION 3.24)

project(engine LANGUAGES CXX)

# The C++ standard, set once for every target in the project. STANDARD_REQUIRED
# makes a compiler that cannot do C++20 an error rather than a silent downgrade;
# EXTENSIONS OFF asks for -std=c++20, not -std=gnu++20, so we stay portable.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Dependencies ----------------------------------------------------------
# FetchContent acquires SDL3 at configure time and adds it to our build. We pin
# an exact release tag: a moving branch would mean a fresh clone could build
# different source than yours, which is how "works on my machine" is born.
include(FetchContent)

# SDL build options must be set BEFORE SDL is configured. We only need the
# library itself, so skip SDL's own test helpers — it shaves real time off a
# from-source build.
set(SDL_TEST_LIBRARY OFF)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.4.12   # pinned — never a branch name
    GIT_SHALLOW    TRUE             # only this tag's snapshot, not all of history
)
FetchContent_MakeAvailable(SDL3)

# stb_image — Lesson 4.7. A PNG/JPEG/BMP decoder in one header, and the first
# third-party code in this project that is not SDL.
#
# WHY WE DO NOT HAND-ROLL THIS. Every other "we could write that" in this course
# has been written: the maths, the rasterizer, the OBJ parser, the ECS to come.
# An image decoder is different in kind, and the difference is not difficulty —
# a baseline PNG decoder is an afternoon — it is that PNG is DEFLATE plus five
# filter modes plus interlacing plus sixteen-bit channels plus palettes plus
# transparency chunks, and a decoder that handles only the files you happened to
# test is worse than no decoder at all. There is nothing about the ENGINE to be
# learned from the fifth filter mode.
#
# stb has no releases and no tags, so we pin a COMMIT — the same rule Lesson 4.3
# had to apply to SDL_shadercross for the same reason. A branch name would mean a
# fresh clone could decode differently from yours.
#
# Header-only, so there is nothing to build. Note where it is USED: only
# engine/src/gfx/image.cpp, and only PRIVATELY (engine/CMakeLists.txt), so the
# dependency stops at the library boundary and no demo can see it.
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        2c980bb59875b0d32144a71867fbdebb2f77cd20   # pinned — no tags exist
    GIT_SHALLOW    FALSE   # a SHA cannot be shallow-fetched by name
)
FetchContent_MakeAvailable(stb)

# ---- Shared build rules ----------------------------------------------------
# Two modules, both of ours. EngineHelpers defines the three functions every
# target calls; Shaders defines the HLSL toolchain and probes it once, here, at
# configure time.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(EngineHelpers)
include(Shaders)

# ---- Shaders ---------------------------------------------------------------
# Lesson 4.3. One HLSL source per stage, compiled to every format this machine's
# toolchain can produce, plus a JSON reflection file per shader.
#
# Declared HERE rather than in engine/ or demos/, and the reason is worth a
# sentence. A compiled shader is not the property of a library (the engine never
# loads one by name) nor of any single program (three of them want the same
# scene.frag). It is a build artifact of the repository. Each executable then
# asks for a copy beside itself with engine_use_shaders().
add_hlsl_shader(triangle.vert vertex)
add_hlsl_shader(triangle.frag fragment)
add_hlsl_shader(textured.vert vertex)
add_hlsl_shader(textured.frag fragment)
add_hlsl_shader(mesh.vert vertex)             # Lesson 4.5
add_hlsl_shader(mesh.frag fragment)           # Lesson 4.5
add_hlsl_shader(uniform_probe.vert vertex)    # Lesson 4.6 — the instrument
add_hlsl_shader(uniform_probe.frag fragment)
add_hlsl_shader(depth_probe.vert vertex)      # Lesson 4.7 — the instrument
add_hlsl_shader(depth_probe.frag fragment)
add_hlsl_shader(texture_probe.frag fragment)
add_hlsl_shader(scene.vert vertex)            # Lesson 4.8 — the ported scene
add_hlsl_shader(scene.frag fragment)
add_hlsl_shader(matrix_probe.frag fragment)   # Lesson 4.8 — the instrument

# ---- The project -----------------------------------------------------------
# ORDER MATTERS, and it is the dependency direction made visible: engine/ knows
# nothing of demos/ and is added first; demos/ links the engine and is added
# second. Swap these two lines and the configure fails, because `engine::engine`
# would not exist yet — which is a build system enforcing an architecture rule,
# and is the cheapest such enforcement you will ever get.
add_subdirectory(engine)
add_subdirectory(demos)
