// src/gfx/gpu_uniform.hpp — data that is the same for every vertex of a draw.
//
// Lesson 4.6. Lesson 4.5 gave the engine two rates of change: per-vertex (slot 0)
// and per-instance (slot 1). A camera is neither. Every vertex of every instance
// of a frame uses the same view-projection matrix, and putting it in a vertex
// buffer would mean writing the same sixteen floats 1,225 times per instance.
//
// SDL_GPU'S ANSWER IS NOT A BUFFER. There is no `SDL_GPU_BUFFERUSAGE_UNIFORM` —
// look for it in SDL_gpu.h and it is not there. Instead:
//
//     SDL_PushGPUVertexUniformData(cb, slot, data, bytes);
//
// pushes bytes onto the COMMAND BUFFER, and every draw recorded after it in that
// command buffer sees them. There is no object to create, bind, or release, and
// no lifetime to get wrong. For data too big for that — a hundred object
// transforms, a bone palette — the answer is a STORAGE buffer, which is a
// different resource in a different register space, and Module 6's territory.
//
// ---------------------------------------------------------------------------
// THE PACKING RULE, AND WHY THIS FILE EXISTS
// ---------------------------------------------------------------------------
//
// The bytes you push are read at offsets the shader compiler chose, and C++ and
// HLSL do not choose the same ones. SDL's header says the data "must respect
// std140 layout conventions", which is safe advice and is NOT what our toolchain
// actually does — `verify_46` §B measures HLSL's own constant-buffer packing:
//
//     Registers are 16 bytes. Fields are placed in order, and a VECTOR MAY NOT
//     STRADDLE A REGISTER BOUNDARY: if it would, it moves to the next register.
//     Scalars and the space after a vector are packed tightly.
//
// std140 is stricter — it aligns every `float3` and `float4` to 16 — so a layout
// that satisfies std140 also satisfies HLSL packing. That is why SDL's advice
// works, and why this engine follows it: **pair every `float3` with a `float`**,
// and the two rules agree, and the struct is portable to a GLSL front end you may
// use later.
//
// Where they differ, the difference is silent. `verify_46` §B pushes a struct
// whose last `float3` C++ placed at byte 92 and the shader reads at byte 96: the
// three components come back shifted by one float, with a zero on the end. No
// error, no warning, no validation message — the same temperament as the vertex
// layout in Lesson 4.5.
//
// So every block below states its offsets and asserts them, using `packed_offset`
// rather than a comment, because a rule you can execute is a rule that cannot
// drift away from the code it describes.

#pragma once

#include "math/mat4.hpp"
#include "math/vec2.hpp"
#include "math/vec3.hpp"

#include <SDL3/SDL.h>

#include <cstddef>

namespace engine {

/// Where a field of `components` floats lands, given a cursor in bytes.
///
/// HLSL's constant-buffer packing rule, executable. A vector (2, 3 or 4
/// components) may not straddle a 16-byte register boundary; if placing it at the
/// cursor would, it starts at the next boundary instead. A scalar is never moved.
///
/// This is a `constexpr` function and not a comment on purpose: every block in
/// this file asserts its offsets against it, so a field inserted in the middle
/// breaks the build rather than the picture.
[[nodiscard]] constexpr std::size_t packed_offset(std::size_t cursor, std::size_t components)
{
    if (components > 1)
    {
        const std::size_t start_register = cursor / 16;
        const std::size_t end_register = (cursor + components * 4 - 1) / 16;
        if (start_register != end_register)
        {
            return (start_register + 1) * 16;   // it would straddle: move it on
        }
    }
    return cursor;
}

/// Per-frame data for the vertex stage: the camera, and nothing else.
///
/// Bound at **vertex slot 0**, which the shader declares as
/// `register(b0, space1)` — the space is fixed by SDL per stage (Lesson 4.3) and
/// is not a choice.
///
/// **The matrix crosses untouched, and this is measured rather than hoped.**
/// Lesson 2.6 chose column-major storage for `mat4` and claimed it was what HLSL
/// constant buffers expect. `verify_46` §C finally checks: element (row, col) of
/// our matrix arrives as `m[row][col]` in the shader, all sixteen of them, with no
/// transpose anywhere. A `memcpy` is the whole conversion.
struct camera_uniforms
{
    mat4 clip_from_world;   ///< projection * view — 2.9 and 2.10, multiplied once per frame
};

static_assert(sizeof(camera_uniforms) == 64, "four columns of four floats");
static_assert(offsetof(camera_uniforms, clip_from_world) == 0, "");

/// Per-frame data for the fragment stage: the lighting environment.
///
/// Bound at **fragment slot 0**, which the shader declares as
/// `register(b0, space3)`.
///
/// **Note the shape: every `float3` is followed by a `float`.** That is not
/// decoration — it is what makes the block satisfy std140 as well as HLSL packing,
/// so the two rules agree and nothing depends on which one the compiler used. Two
/// 16-byte registers, exactly filled, no padding to explain to anyone.
struct light_uniforms
{
    vec3  to_light;   ///< toward the lamp, world space, unit length (3.6's convention)
    float ambient;    ///< what a surface facing away still receives

    vec3  sky;        ///< the colour that ambient term is tinted with
    float diffuse;    ///< how much the cosine term contributes

    // ---- Lesson 4.7 ------------------------------------------------------
    //
    // Two texture controls, added to the block that already existed rather than
    // given a block of their own. A second uniform slot would be a second push
    // and a second declaration for two floats and a float2; grouping by RATE OF
    // CHANGE — both of these are per-frame, like everything above — beats
    // grouping by subject.
    vec2  uv_scale;   ///< how many times the image repeats across the surface
    float grid_mix;   ///< 0 = the texture, 1 = Lesson 4.5's diagnostic grid
    float pad0;       ///< keeps the block a whole number of registers

    // Note that `uv_scale` at 32 and `grid_mix` at 40 need no thought: a vec2
    // followed by two floats fills register 2 exactly. The pad is there so the
    // struct's SIZE is a multiple of 16, which nothing here requires but every
    // array of blocks will (Module 6), and which costs four bytes now against
    // an awkward conversation later.
};

static_assert(sizeof(light_uniforms) == 48, "three registers, exactly filled");
static_assert(offsetof(light_uniforms, to_light) == packed_offset(0, 3), "");
static_assert(offsetof(light_uniforms, ambient) == packed_offset(12, 1), "");
static_assert(offsetof(light_uniforms, sky) == packed_offset(16, 3), "");
static_assert(offsetof(light_uniforms, diffuse) == packed_offset(28, 1), "");
static_assert(offsetof(light_uniforms, uv_scale) == packed_offset(32, 2), "");
static_assert(offsetof(light_uniforms, grid_mix) == packed_offset(40, 1), "");

} // namespace engine
