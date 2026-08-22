# CMakeLists.txt — the engine's build description.
#
# This one file tells CMake how to build the project on Windows, Linux, and
# macOS, with MSVC, GCC, or Clang, and how to acquire SDL3. You describe the
# build once here; CMake generates the right native build for whatever machine
# runs it. This is what Lesson 0.4 builds up, line by line.

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

# ---- Our program -----------------------------------------------------------
# Every source file is listed by hand. CMake can glob them for you, and you
# should not let it: a glob is evaluated at configure time, so adding a file
# leaves the build silently ignoring it until someone reconfigures. An explicit
# list is one extra line per file and never lies.
add_executable(engine
    src/main.cpp
    src/core/clock.cpp
    src/core/fixed_step.cpp
    src/core/input.cpp
    src/core/profile.cpp
    src/game/pong.cpp
    src/gfx/clip.cpp
    src/gfx/colour.cpp
    src/gfx/depth_buffer.cpp
    src/gfx/framebuffer.cpp
    src/gfx/gpu_buffer.cpp
    src/gfx/gpu_device.cpp
    src/gfx/gpu_mesh.cpp
    src/gfx/gpu_pipeline.cpp
    src/gfx/gpu_present.cpp
    src/gfx/gpu_shader.cpp
    src/gfx/mesh.cpp
    src/gfx/obj.cpp
    src/gfx/raster.cpp
    src/gfx/texture.cpp
)

# Make `src/` a root for our own includes, so a file says
#     #include "core/input.hpp"
# from anywhere in the tree — the same spelling regardless of which directory
# the including file lives in. Relative paths like "../core/input.hpp" break the
# moment a file moves; a stable include root does not. PRIVATE because nothing
# links against us: this is a leaf executable, and the setting stops here.
target_include_directories(engine PRIVATE src)

# Link SDL. SDL3::SDL3 is the alias SDL exports for "the SDL library, however it
# was built"; linking it also pulls in SDL's headers and platform frameworks
# transitively, so we never name Cocoa or user32 ourselves.
target_link_libraries(engine PRIVATE SDL3::SDL3)

# ---- Assets ----------------------------------------------------------------
# Lesson 3.5. The program loads models at runtime, and it finds them with
# SDL_GetBasePath() — the directory the executable lives in — never with a path
# relative to the current working directory, which is wherever the user happened
# to be standing when they launched it. So the assets have to BE there, and
# putting them there is the build's job.
#
# POST_BUILD runs after every successful link, so editing a model and rebuilding
# is enough; there is no separate "install assets" step to forget.
#
# $<TARGET_FILE_DIR:engine> is a GENERATOR EXPRESSION — a value CMake cannot know
# at configure time and resolves when it writes the build rules. It matters here
# because multi-config generators (Visual Studio, Xcode) put the binary in
# Debug/ or Release/ and a hard-coded path would be right in at most one of them.
add_custom_command(TARGET engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:engine>/assets"
    COMMENT "Copying assets/ next to the executable"
    VERBATIM)

# ---- Shaders ---------------------------------------------------------------
# Lesson 4.3. One HLSL source per stage, compiled to every format this machine's
# toolchain can produce — plus a JSON reflection file per shader, because
# SDL_GPUShaderCreateInfo wants four resource counts and guessing them is the
# first thing SDL_gpu.h's FAQ tells you not to do.
#
# cmake/Shaders.cmake probes the toolchain at configure time rather than assuming
# it: SDL_shadercross can be built WITHOUT DirectXShaderCompiler, and such a build
# cannot read HLSL at all. Read its configure-time message; it says which path
# you got.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Shaders)

add_hlsl_shader(engine triangle.vert vertex)
add_hlsl_shader(engine triangle.frag fragment)
add_hlsl_shader(engine textured.vert vertex)
add_hlsl_shader(engine textured.frag fragment)
add_hlsl_shader(engine mesh.vert vertex)      # Lesson 4.5
add_hlsl_shader(engine mesh.frag fragment)    # Lesson 4.5
add_hlsl_shader(engine uniform_probe.vert vertex)    # Lesson 4.6 — the instrument
add_hlsl_shader(engine uniform_probe.frag fragment)

# Compiled shaders live beside the executable for the same reason assets do: the
# program finds them with SDL_GetBasePath(), never relative to the working
# directory.
#
# This copy must not fail when the toolchain was missing and no shader was built.
# It does not, and the reason is one line in cmake/Shaders.cmake: the output
# directory is created at configure time, BEFORE the function decides whether it
# can compile anything, so the worst case here is copying an empty directory. The
# program then reports the absence at startup — a better error, in a lesson about
# acquiring a tool, than a build that stops.
add_custom_command(TARGET engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:engine>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_BINARY_DIR}/shaders"
            "$<TARGET_FILE_DIR:engine>/shaders"
    COMMENT "Copying compiled shaders next to the executable"
    VERBATIM)

# Our warning discipline (see cpp-style §9) — applied to OUR target only, never
# to SDL. Third-party code is compiled with its own flags; holding someone
# else's library to /W4 just produces noise we cannot act on.
if(MSVC)
    target_compile_options(engine PRIVATE /W4 /Zc:__cplusplus)
else()
    target_compile_options(engine PRIVATE -Wall -Wextra)
endif()
