// engine/include/engine/gfx/gpu_uniform.hpp — data that is the same for every vertex of a draw.
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

#include <engine/math/mat4.hpp>
#include <engine/math/vec2.hpp>
#include <engine/math/vec3.hpp>

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


// ---------------------------------------------------------------------------
// Lesson 4.8 — the blocks the ported scene needs
// ---------------------------------------------------------------------------
//
// Lesson 4.6 established ONE rate that is neither per-vertex nor per-instance:
// per FRAME. The port needs a second one, and noticing that it is a different
// rate is most of what this lesson does to the renderer's shape.
//
//     per frame    the camera, the lamp            pushed once, before the pass
//     per DRAW     this object's matrices, this    pushed between draws
//                  object's material
//     per vertex   position, normal, uv            a vertex buffer (4.5)
//
// The middle row did not exist while the demo drew one instanced torus, because
// every instance of a torus shares a material and its placement fits in 28 bytes
// of vertex buffer. Module 3's scene has objects with DIFFERENT SHAPES, different
// materials and a general `transform` each, and none of that fits the instance
// trick — so it arrives the way the camera does, one push per draw.
//
// SDL's header is explicit that this works: "Subsequent draw calls in this
// command buffer will use this uniform data." A push is not bound to a pass and
// not bound to a pipeline; it is a value written into the command stream at the
// point you write it.

/// Per-DRAW data for the vertex stage: where this object is, and how its normals
/// have to be carried.
///
/// Bound at **vertex slot 1** — `register(b1, space1)` — beside 4.6's camera at
/// slot 0, which does not move and is not re-pushed.
///
/// **Why the normal matrix is three `vec4` and not a `mat3`.** HLSL will happily
/// accept `float3x3` in a constant buffer, and it costs exactly what this does:
/// each column is padded out to a 16-byte register, so a 3x3 occupies 48 bytes
/// and wastes 12 of them. Writing the padding out is not a saving, it is a
/// removal of one thing to be sure about — the packing of a matrix type in a
/// constant buffer depends on a compiler default (`column_major` for DXC, and
/// `row_major` if a `#pragma pack_matrix` upstream says so), and a silently
/// transposed normal matrix is a lighting bug that looks like a modelling bug.
/// `verify_48` §C measures both spellings and reports whether they agree.
///
/// The columns, not the rows, because `mat3` stores columns and the shader
/// rebuilds `M*v` as `c0*v.x + c1*v.y + c2*v.z` — the definition of a matrix
/// times a vector, and Lesson 2.5's "a matrix is where the basis vectors land"
/// written out one last time.
struct object_uniforms
{
    mat4 world_from_model;      ///< T*R*S — Lesson 2.8's composition, per object
    vec4 normal_from_model[3];  ///< the inverse transpose's three columns, w unused
};

static_assert(sizeof(object_uniforms) == 112, "seven registers: four and three");
static_assert(offsetof(object_uniforms, world_from_model) == 0, "");
static_assert(offsetof(object_uniforms, normal_from_model) == 64, "");

/// Per-FRAME data for the fragment stage, once the shading is Module 3's.
///
/// Bound at **fragment slot 0** — `register(b0, space3)`. It replaces
/// `light_uniforms` for the scene pipelines rather than extending it, because
/// the two describe different shading models and a block that serves both would
/// carry fields one of them ignores. The probe's block stays exactly as 4.7 left
/// it; nothing that worked stops working.
///
/// **`key` is colour times intensity, multiplied on the CPU.** `directional_light`
/// keeps them apart so a lamp's hue and its brightness can be authored
/// separately (light.hpp says why); nothing downstream of authoring needs them
/// apart, so they arrive here as one product. That is the same reasoning as
/// decoding the tint to linear before it is pushed: work that is constant over a
/// draw belongs on the side that runs once.
///
/// **`eye_world` is here because a highlight is view-dependent** — Lesson 3.7's
/// whole point, and the reason the composed `view_from_model` matrix had to be
/// taken apart in that lesson. On the GPU the same fact reappears as: the vertex
/// stage has to output a world position, so every varying costs bandwidth.
struct scene_light_uniforms
{
    vec3  to_light;    ///<  0 — toward the lamp, world space, unit length
    float pad0;        ///< 12
    vec3  key;         ///< 16 — the lamp's linear colour TIMES its intensity
    float pad1;        ///< 28
    vec3  ambient;     ///< 32 — light.hpp's honest fudge, in linear light

    /// 44 — **Lesson 6.1**, and it cost nothing to add, which is the point.
    ///
    /// 1 means "the shader must apply the sRGB transfer function before
    /// returning"; 0 means the swapchain is an `_SRGB` format and the hardware
    /// does it on write. Fill it from
    /// `!gpu_device::report().output_encodes_in_hardware` and never guess: one
    /// wrong answer gives a picture 2.3x too dark at mid grey, the other gives a
    /// washed-out one.
    ///
    /// THIS SLOT WAS ALREADY HERE. It was `pad2`, and it existed because HLSL
    /// packs a `float3` and a `float` into one 16-byte register (Lesson 4.6) —
    /// so the padding was never wasted space, it was an unused field. The struct
    /// is still 64 bytes, every offset is unchanged, and the `static_assert`s
    /// below did not move. A flag that costs zero bytes and zero repacking is a
    /// flag you can afford to read on every fragment.
    float encode_output;
    vec3  eye_world;   ///< 48 — where the camera is; a highlight needs it
    float spec_model;  ///< 60 — 0 = none, 1 = Phong, 2 = Blinn (3.7's enum)
};

static_assert(sizeof(scene_light_uniforms) == 64, "four registers, exactly filled");
static_assert(offsetof(scene_light_uniforms, to_light) == packed_offset(0, 3), "");
static_assert(offsetof(scene_light_uniforms, key) == packed_offset(16, 3), "");
static_assert(offsetof(scene_light_uniforms, ambient) == packed_offset(32, 3), "");
static_assert(offsetof(scene_light_uniforms, eye_world) == packed_offset(48, 3), "");
static_assert(offsetof(scene_light_uniforms, spec_model) == packed_offset(60, 1), "");

/// Per-DRAW data for the fragment stage: the surface.
///
/// Bound at **fragment slot 1** — `register(b1, space3)`.
///
/// This struct is the material that Lessons 3.4, 3.7 and 3.8 each said was the
/// missing idea, and it is worth seeing how small the first honest version is:
/// an albedo, a highlight colour, an exponent, and a flag saying where the albedo
/// comes from. What it is NOT is the cull mode or the fill mode — those are
/// pipeline state, they cannot be a number in a buffer, and that is exactly why
/// this lesson ends up with three pipelines and a sort. Module 6 builds the
/// material that owns both halves.
///
/// **`albedo` is LINEAR**, decoded from the demo's `Uint32` tint before the push.
/// It multiplies a quantity of light two lines later, and Lesson 1.6's rule has
/// not softened.
struct material_uniforms
{
    vec3  albedo;      ///<  0 — the surface's own colour, linear, in [0,1]
    float shininess;   ///< 12 — the specular exponent (3.7); not comparable across models
    vec3  specular;    ///< 16 — the highlight's reflectance; black = matte
    float textured;    ///< 28 — 0 = use `albedo`, 1 = sample the bound texture
};

static_assert(sizeof(material_uniforms) == 32, "two registers, exactly filled");
static_assert(offsetof(material_uniforms, albedo) == packed_offset(0, 3), "");
static_assert(offsetof(material_uniforms, shininess) == packed_offset(12, 1), "");
static_assert(offsetof(material_uniforms, specular) == packed_offset(16, 3), "");
static_assert(offsetof(material_uniforms, textured) == packed_offset(28, 1), "");

} // namespace engine
