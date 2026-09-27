# cmake/EngineHelpers.cmake — the three things every target in this project needs
# to say, said once.
#
# Lesson 5.1. Before the split there was one target, so "set the warning flags"
# and "copy the assets" were two lines in the middle of the one CMakeLists.txt.
# With four targets in three directories they would be twelve lines in three
# files, drifting apart at the speed of whoever edits one and not the others.
#
# A function is the fix, and it is the same fix as everywhere else in this
# course: one rule, one place, many callers.

# ---- engine_set_warnings ----------------------------------------------------
#
# Our warning discipline (cpp-style §9), applied to OUR targets only and never to
# SDL or stb. Third-party code is compiled with its own flags; holding somebody
# else's library to /W4 produces noise we cannot act on and would eventually
# train us to ignore the ones we can.
function(engine_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /Zc:__cplusplus)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
endfunction()

# ---- engine_use_assets ------------------------------------------------------
#
# Put assets/ next to the executable. Lesson 3.5's rule is that the program finds
# its data with SDL_GetBasePath() — the directory the binary lives in — and never
# relative to the working directory, which is wherever the user happened to be
# standing. So the data has to BE there, and putting it there is the build's job.
#
# $<TARGET_FILE_DIR:...> is a GENERATOR EXPRESSION: a value CMake cannot know at
# configure time and resolves when it writes the build rules. It matters because
# multi-config generators (Visual Studio, Xcode) put the binary in Debug/ or
# Release/, and a hard-coded path would be right in at most one of them.
function(engine_use_assets target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:${target}>/assets"
        COMMENT "Copying assets/ next to ${target}"
        VERBATIM)
endfunction()

# ---- engine_use_shaders -----------------------------------------------------
#
# Depend on every compiled shader, and copy them next to the executable for the
# same reason assets go there.
#
# The dependency half is not optional. The custom commands in Shaders.cmake
# produce files nobody asked for; a custom command with no consumer is never run,
# and the result is a build that succeeds and produces nothing — the classic
# CMake shader mistake. `add_dependencies` is what gives them a consumer.
function(engine_use_shaders target)
    get_property(shader_targets GLOBAL PROPERTY ENGINE_SHADER_TARGETS)
    if(shader_targets)
        add_dependencies(${target} ${shader_targets})
    endif()

    # Must not fail when the toolchain was missing and nothing was compiled. It
    # does not, because Shaders.cmake creates the output directory at configure
    # time, BEFORE it decides whether it can compile anything — so the worst case
    # here is copying an empty directory, and the program then reports the
    # absence at startup. A better error, in a course that teaches acquiring the
    # tool, than a build that stops.
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${ENGINE_SHADER_OUTPUT_DIR}" "$<TARGET_FILE_DIR:${target}>/shaders"
        COMMENT "Copying compiled shaders next to ${target}"
        VERBATIM)
endfunction()
