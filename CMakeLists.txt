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
    src/game/pong.cpp
    src/gfx/colour.cpp
    src/gfx/framebuffer.cpp
    src/gfx/raster.cpp
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

# Our warning discipline (see cpp-style §9) — applied to OUR target only, never
# to SDL. Third-party code is compiled with its own flags; holding someone
# else's library to /W4 just produces noise we cannot act on.
if(MSVC)
    target_compile_options(engine PRIVATE /W4 /Zc:__cplusplus)
else()
    target_compile_options(engine PRIVATE -Wall -Wextra)
endif()
