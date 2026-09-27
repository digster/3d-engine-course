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
#     tools/    (Module 9) the editor and the asset pipeline.
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

# Dear ImGui — Lesson 5.11. The course's first third-party USER INTERFACE, and
# the first third-party library whose types reach our PUBLIC headers.
#
# WHY WE DO NOT HAND-ROLL THIS. The test this course applies to every library is
# not "is it hard?" — the rasterizer was hard and we wrote it — it is **whether
# the hard part is the subject**. A debug UI is four subjects at once: a font
# rasterizer and text shaper, a layout engine, an input router with focus and
# capture rules, and a widget state machine that has to remember what you were
# dragging across frames. Not one of those is graphics or engine architecture,
# and a half-built version of all four is precisely the tool you stop trusting
# the first time it misreports a number.
#
# What we get in exchange is worth naming too, because it is not just "saved
# time": ImGui is IMMEDIATE MODE, so a debug panel is a function that runs every
# frame and owns no state. There is no widget tree to keep in sync with the
# world, which is the failure every retained-mode debug UI eventually has.
#
# TOOLING ONLY, NEVER GAMEPLAY UI. That is a binding rule of this course and it
# is a real engineering line, not a stylistic one: ImGui rebuilds its geometry
# from scratch every frame and knows nothing about controllers, localisation,
# safe areas or artists. It is the right tool for the people building the game
# and the wrong one for the people playing it.
#
# IMGUI SHIPS SOURCES, NOT A BUILD. There is no CMakeLists.txt in that
# repository, which is deliberate on its author's part — the library is nine
# .cpp files and a backend pair, and every project wants to compile them its own
# way. FetchContent_MakeAvailable notices the absence and only POPULATES; the
# target below is ours to declare, and being forced to declare it is how you
# find out exactly which files you are compiling.
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.9b   # pinned — never a branch name
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

# The core, plus exactly ONE backend pair. ImGui separates "what a window looks
# like" (the core) from "how events get in and triangles get out" (a platform
# backend and a renderer backend), and you pick one of each.
#
#   imgui_impl_sdl3         the PLATFORM backend: SDL_Event in, mouse/keyboard/
#                           text/focus out. Independent of how we draw.
#   imgui_impl_sdlrenderer3 the RENDERER backend: ImGui's vertex+index draw
#                           lists out through an SDL_Renderer.
#
# Lesson 5.11 §5 explains why the SDL_Renderer pair goes first and what the
# SDL_GPU one (imgui_impl_sdlgpu3) would need instead. The platform half is
# shared between them; only the renderer half would change.
#
# imgui_demo.cpp is 8,000 lines of showcase and it is included ON PURPOSE:
# ImGui::ShowDemoWindow() is the fastest widget reference there is, and its
# source is the documentation. Drop it from this list when a real game ships.
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)
add_library(imgui::imgui ALIAS imgui)

# PUBLIC, because anything that includes <imgui.h> needs these two directories
# and ImGui's headers are reached BY NAME rather than through a prefix — the
# spelling is <imgui.h>, not <imgui/imgui.h>. That is a real cost of using it and
# it is worth seeing: two directories go onto the include path of everything that
# links this target.
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends)

# Both backends include SDL3 headers, and imgui_impl_sdlrenderer3 calls into it.
target_link_libraries(imgui PUBLIC SDL3::SDL3)

# NOT engine_set_warnings(imgui). Third-party code is compiled with its own
# flags, for the reason cmake/EngineHelpers.cmake gives: warnings we cannot act
# on train us to ignore the ones we can.

# cgltf — Lesson 6.6. The third approved third-party library, and the one whose
# "why we don't hand-roll this" argument is the least obvious of the three,
# because on the face of it glTF is JSON and we have already written a parser.
#
# WHY WE DO NOT HAND-ROLL THIS. Lesson 3.5 wrote an OBJ loader in three hundred
# lines, and it was the right call: OBJ is a line-oriented text format with six
# keywords that matter, and writing it taught the index problem, fan
# triangulation and vertex unification — all of which are ENGINE subjects.
#
# glTF is a different kind of thing, and the difference is not "harder". It is
# that glTF is a CONTAINER: JSON describing accessors, which describe strides
# into buffer views, which describe windows into buffers, which may be external
# files, base64 data URIs, or a binary chunk glued to the JSON in a .glb. An
# accessor may be any of five component types, normalized or not, interleaved
# with its neighbours or not, sparse (a base array plus a patch list) or not.
# That is a combinatorial matrix with roughly a hundred cells, every one of them
# legal, and none of them about graphics. `cgltf_accessor_read_float` is the
# function that collapses the matrix — and writing our own would teach byte
# offsets, not rendering.
#
# The test this course applies is never "is it hard?" but WHETHER THE HARD PART
# IS THE SUBJECT. For stb_image the hard part was DEFLATE; for ImGui it was text
# layout and input routing; here it is a strided, typed, optionally-sparse view
# of somebody else's bytes. In all three cases the answer is no.
#
# WHY cgltf RATHER THAN tinygltf. Both are approved (CLAUDE.md §4). cgltf is one
# C header with no dependencies; tinygltf is C++ and pulls in a JSON library and
# stb (which we already have, at a different pin — two copies of stb_image in one
# binary is a diagnosable problem we would rather not have). cgltf also matches
# the shape we already use: header-only, PRIVATE to one translation unit, wrapped
# behind our own types so the dependency stops at the library boundary.
#
# Pinned to a TAG, because cgltf has releases — unlike stb, which forced a commit.
FetchContent_Declare(
    cgltf
    GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
    GIT_TAG        v1.15   # pinned — never a branch name
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(cgltf)

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
add_hlsl_shader(shadow.vert vertex)          # Lesson 6.8 — the depth pass
add_hlsl_shader(shadow.frag fragment)
add_hlsl_shader(fullscreen.vert vertex)      # Lesson 6.12 — no vertex buffer at all
add_hlsl_shader(tonemap.frag fragment)       # Lesson 6.12 — the resolve
# Lesson 6.13 — the bloom's three stages. Three fragment shaders and no new vertex
# shader, which is the clearest evidence that `fullscreen.vert` was the right
# abstraction: every post pass wants the same three corners, and only the question
# asked of each pixel differs.
add_hlsl_shader(bloom_bright.frag fragment)
add_hlsl_shader(bloom_down.frag fragment)
add_hlsl_shader(bloom_up.frag fragment)

# ---- The project -----------------------------------------------------------
# ORDER MATTERS, and it is the dependency direction made visible: engine/ knows
# nothing of demos/ and is added first; demos/ links the engine and is added
# second. Swap these two lines and the configure fails, because `engine::engine`
# would not exist yet — which is a build system enforcing an architecture rule,
# and is the cheapest such enforcement you will ever get.
add_subdirectory(engine)
add_subdirectory(demos)
