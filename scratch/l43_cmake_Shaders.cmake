# cmake/Shaders.cmake — compiling HLSL to every backend's shader format.
#
# Lesson 4.3. One source language, three binary formats, produced at build time so
# that a shader edit is a build step rather than a ritual.
#
#     shaders/triangle.vert.hlsl                        (you write this)
#         -> shaders/triangle.vert.spv    SPIR-V, for Vulkan
#         -> shaders/triangle.vert.msl    MSL, for Metal
#         -> shaders/triangle.vert.dxil   DXIL, for D3D12
#         -> shaders/triangle.vert.json   the resource counts, for SDL
#
# THE JSON IS NOT OPTIONAL BOOKKEEPING. `SDL_GPUShaderCreateInfo` demands four
# numbers — samplers, storage textures, storage buffers, uniform buffers — and
# SDL_gpu.h's own FAQ names getting them wrong as the commonest cause of a shader
# that does not work. shadercross computes them from the compiled code, so we ask
# it rather than counting by hand and being right until somebody edits the shader.
#
# WHY THERE IS A CAPABILITY PROBE BELOW. SDL_shadercross does two separate jobs:
# it compiles HLSL to SPIR-V (using DirectXShaderCompiler) and it translates
# SPIR-V to MSL, DXIL or JSON (using SPIRV-Cross). **A shadercross built without
# DXC can only do the second**, and it is entirely possible to have such a build
# installed — the machine this lesson was written on does. So we do not assume;
# we run the tool once at configure time and find out.

# ---- Finding the tools -------------------------------------------------------

find_program(SHADERCROSS_EXE
    NAMES shadercross
    DOC "SDL_shadercross CLI — SPIR-V/HLSL to SPIR-V, DXIL, MSL and JSON")

# The fallback for the first hop only. shaderc's glslc compiles HLSL to SPIR-V
# with -x hlsl, which is the same job DXC does inside shadercross.
find_program(GLSLC_EXE
    NAMES glslc
    DOC "shaderc's glslc — used only to compile HLSL to SPIR-V when shadercross cannot")

# Some shadercross installs ship without an rpath, so the CLI cannot find its own
# shared library. Adding the sibling lib directory to the loader's search path
# costs nothing when it is not needed, and is the difference between "works" and
# "dyld: Library not loaded" when it is.
if(SHADERCROSS_EXE)
    get_filename_component(_sc_bin_dir "${SHADERCROSS_EXE}" DIRECTORY)
    get_filename_component(_sc_prefix "${_sc_bin_dir}" DIRECTORY)
    if(APPLE)
        set(_sc_env "DYLD_LIBRARY_PATH=${_sc_prefix}/lib:$ENV{DYLD_LIBRARY_PATH}")
    elseif(UNIX)
        set(_sc_env "LD_LIBRARY_PATH=${_sc_prefix}/lib:$ENV{LD_LIBRARY_PATH}")
    else()
        set(_sc_env "PATH=${_sc_prefix}/bin;$ENV{PATH}")
    endif()
    set(SHADERCROSS_RUN ${CMAKE_COMMAND} -E env "${_sc_env}" "${SHADERCROSS_EXE}")
endif()

# ---- The capability probe ----------------------------------------------------
#
# Ask, once, at configure time: can this shadercross read HLSL? The answer decides
# which pipeline every shader below is built with, and it is printed, because a
# silent fallback is a thing you discover months later when the output differs.

set(SHADERCROSS_READS_HLSL OFF)
if(SHADERCROSS_EXE)
    execute_process(
        COMMAND ${SHADERCROSS_RUN}
                "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert.hlsl"
                -s HLSL -t vertex -d SPIRV
                -o "${CMAKE_CURRENT_BINARY_DIR}/shadercross_probe.spv"
        RESULT_VARIABLE _probe_result
        OUTPUT_QUIET ERROR_QUIET)
    if(_probe_result EQUAL 0)
        set(SHADERCROSS_READS_HLSL ON)
    endif()
    file(REMOVE "${CMAKE_CURRENT_BINARY_DIR}/shadercross_probe.spv")
endif()

# Which formats we can actually produce. SPIR-V and MSL need only SPIRV-Cross;
# DXIL needs DXC, so it rides on the same capability as reading HLSL.
set(SHADER_FORMATS spv msl)
if(SHADERCROSS_READS_HLSL)
    list(APPEND SHADER_FORMATS dxil)
endif()

# ---- Report, at configure time ----------------------------------------------

if(NOT SHADERCROSS_EXE)
    message(WARNING
        "SDL_shadercross not found — shaders will NOT be compiled and the GPU demo "
        "will report that it has none. Build it from "
        "https://github.com/libsdl-org/SDL_shadercross and put `shadercross` on PATH.")
elseif(SHADERCROSS_READS_HLSL)
    message(STATUS "Shaders: shadercross reads HLSL directly (built with DXC) -> ${SHADER_FORMATS}")
elseif(GLSLC_EXE)
    message(STATUS "Shaders: shadercross has no DXC; using glslc for HLSL->SPIR-V -> ${SHADER_FORMATS}")
else()
    message(WARNING
        "shadercross was built without DXC and glslc was not found, so HLSL cannot be "
        "compiled to SPIR-V on this machine. Install shaderc (glslc) or rebuild "
        "shadercross with DXC. Shaders will NOT be compiled.")
endif()

# ---- add_hlsl_shader ---------------------------------------------------------
#
# Compile one HLSL file into every format this machine can produce, plus its
# reflection JSON, and hang the outputs off a target so the build orders itself.
#
#   add_hlsl_shader(<target> <name> <stage>)
#     target : the target whose build these outputs are part of
#     name   : "triangle.vert" — the file is shaders/<name>.hlsl
#     stage  : vertex | fragment | compute
#
# The stage is passed explicitly rather than inferred from the filename. The CLI
# can infer it, but an inference that silently picks `vertex` for a misnamed file
# is a bug that shows up as a blank screen.
function(add_hlsl_shader target name stage)
    set(src "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${name}.hlsl")
    set(out_dir "${CMAKE_CURRENT_BINARY_DIR}/shaders")
    file(MAKE_DIRECTORY "${out_dir}")

    if(NOT SHADERCROSS_EXE)
        return()
    endif()
    if(NOT SHADERCROSS_READS_HLSL AND NOT GLSLC_EXE)
        return()
    endif()

    set(outputs "")

    # ---- Hop 1: HLSL -> SPIR-V ----------------------------------------------
    # SPIR-V is the hub. Every other format is translated FROM it, which is why a
    # missing DXC costs you the whole toolchain and not just the D3D12 output.
    set(spv "${out_dir}/${name}.spv")
    if(SHADERCROSS_READS_HLSL)
        add_custom_command(
            OUTPUT "${spv}"
            COMMAND ${SHADERCROSS_RUN} "${src}" -s HLSL -t ${stage} -d SPIRV -o "${spv}"
            DEPENDS "${src}"
            COMMENT "HLSL -> SPIR-V  ${name}"
            VERBATIM)
    else()
        add_custom_command(
            OUTPUT "${spv}"
            COMMAND "${GLSLC_EXE}" -x hlsl -fshader-stage=${stage} -fentry-point=main
                    -o "${spv}" "${src}"
            DEPENDS "${src}"
            COMMENT "HLSL -> SPIR-V (glslc)  ${name}"
            VERBATIM)
    endif()
    list(APPEND outputs "${spv}")

    # ---- Hop 2: SPIR-V -> everything else ------------------------------------
    foreach(fmt IN LISTS SHADER_FORMATS)
        if(fmt STREQUAL "spv")
            continue()
        endif()
        string(TOUPPER "${fmt}" fmt_upper)
        set(dst "${out_dir}/${name}.${fmt}")
        add_custom_command(
            OUTPUT "${dst}"
            COMMAND ${SHADERCROSS_RUN} "${spv}" -s SPIRV -t ${stage} -d ${fmt_upper} -o "${dst}"
            DEPENDS "${spv}"
            COMMENT "SPIR-V -> ${fmt_upper}  ${name}"
            VERBATIM)
        list(APPEND outputs "${dst}")
    endforeach()

    # ---- The reflection ------------------------------------------------------
    set(json "${out_dir}/${name}.json")
    add_custom_command(
        OUTPUT "${json}"
        COMMAND ${SHADERCROSS_RUN} "${spv}" -s SPIRV -t ${stage} -d JSON -o "${json}"
        DEPENDS "${spv}"
        COMMENT "SPIR-V -> reflection  ${name}"
        VERBATIM)
    list(APPEND outputs "${json}")

    # One custom target per shader, which the executable depends on. Without this
    # the custom commands have no consumer and are never run — a build that
    # silently produces nothing, which is the classic CMake shader mistake.
    string(REPLACE "." "_" safe_name "${name}")
    add_custom_target(shader_${safe_name} DEPENDS ${outputs})
    add_dependencies(${target} shader_${safe_name})
endfunction()
