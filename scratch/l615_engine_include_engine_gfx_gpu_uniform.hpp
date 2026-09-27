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

#include <engine/gfx/hdr.hpp>        // 6.12: tonemap_settings
#include <engine/gfx/bloom.hpp>      // 6.13: uniforms_of(bloom_settings, ...)
#include <engine/gfx/material.hpp>   // 6.5: uniforms_of
#include <engine/math/mat4.hpp>
#include <engine/math/vec2.hpp>
#include <engine/math/vec3.hpp>
#include <engine/math/vec4.hpp>

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
/// **`key` is colour times IRRADIANCE, multiplied on the CPU.** `directional_light`
/// keeps them apart so a lamp's hue and its brightness can be authored
/// separately (light.hpp says why); nothing downstream of authoring needs them
/// apart, so they arrive here as one product. That is the same reasoning as
/// decoding the tint to linear before it is pushed: work that is constant over a
/// draw belongs on the side that runs once.
///
/// **Lesson 6.2 renamed the scalar and changed nothing here** — which is worth a
/// sentence, because it is the test of whether a uniform block was designed or
/// merely filled in. The field was already the *product* rather than the two
/// factors, so giving one factor a physical unit could not reach it. Had this
/// struct carried `colour` and `intensity` separately, the rename would have
/// crossed the CPU/GPU boundary and the shader would have needed to know what a
/// lamp's authoring model is. A boundary that carries results rather than inputs
/// is one the far side cannot be wrong about.
///
/// **`eye_world` is here because a highlight is view-dependent** — Lesson 3.7's
/// whole point, and the reason the composed `view_from_model` matrix had to be
/// taken apart in that lesson. On the GPU the same fact reappears as: the vertex
/// stage has to output a world position, so every varying costs bandwidth.
struct scene_light_uniforms
{
    vec3  to_light;    ///<  0 — toward the lamp, world space, unit length
    float pad0;        ///< 12
    vec3  key;         ///< 16 — the lamp's linear colour TIMES its irradiance
    float pad1;        ///< 28
    vec3  ambient;     ///< 32 — a uniform hemispherical radiance (Lesson 6.2)

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

    // ---- Lesson 6.8: the shadow map, and everything the lookup needs -------
    //
    // THE BLOCK TRIPLES IN SIZE HERE, 64 bytes to 176, and it is worth saying why
    // that is acceptable when 6.7 made a point of the flag that cost nothing.
    // This is a PER-FRAME push: one 176-byte copy for the whole frame, against
    // 6.7's per-DRAW `material_uniforms` where every byte is multiplied by the
    // draw count. The two blocks are charged at completely different rates, and
    // the design pressure on them is different in exactly that proportion.

    /// World -> the light's clip space. The same matrix `light_camera` holds, and
    /// the reason it goes to the FRAGMENT stage rather than the vertex stage is
    /// §5.3: an orthographic projection leaves `w = 1`, so the light-space
    /// position is AFFINE in the world position — and the fragment already has an
    /// interpolated world position, put there in 3.7 for the specular term.
    /// Recovering it here costs one matrix multiply per fragment and **zero new
    /// varyings**, where the textbook arrangement costs four interpolated floats
    /// on every draw whether it is shadowed or not.
    mat4 light_clip_from_world;   ///< 64

    float shadow_strength;    ///< 128 — 0 disables the lookup entirely
    float shadow_texel;       ///< 132 — world units per shadow texel
    float shadow_depth_range; ///< 136 — far - near, in world units
    float shadow_bias;        ///< 140 — the constant term, in device depth
    float shadow_slope_scale; ///< 144 — a multiplier on the derived slope term
    float shadow_max_slope;   ///< 148 — the clamp on tan(theta)
    float shadow_reach;       ///< 152 — `pcf_reach_texels(radius)`
    float shadow_pcf;         ///< 156 — the kernel radius, as a float
    float shadow_mode;        ///< 160 — 0 none, 1 constant, 2 slope, 3 normal
    float shadow_normal_scale;///< 164 — texels of normal offset at grazing
    float shadow_texel_uv;    ///< 168 — 1/resolution: one texel, in uv

    // ---- Lesson 6.15 --------------------------------------------------------

    /// 172 — **the third free ride**, and the last one this block has to give.
    ///
    /// It was `pad2`, the slot 6.8 added to fill the eleventh register, so the
    /// environment's master switch cost zero bytes — the same gift 6.7 got from
    /// 6.4's padding and 6.14 got from 6.11's.
    ///
    /// **0 means "no environment": the shader falls back to `ambient`**, which
    /// is what keeps every picture made before this lesson byte-identical. 1
    /// means the environment at the radiance it was baked with; other values are
    /// an artistic scalar and are honest about being one.
    float ibl_intensity;

    /// 176 — `prefiltered.levels() - 1`. **This one costs a whole register**,
    /// taking the block from eleven to twelve, and the cost is named rather than
    /// absorbed because three lessons running have been free and a reader could
    /// reasonably have started to expect it.
    ///
    /// It cannot be a constant in the shader. The chain's depth is chosen by
    /// `bake_environment` at run time, and the roughness-to-level mapping is a
    /// property of how the chain was built — so a hard-coded 5 in HLSL would be
    /// a number that must agree with a number in C++, in another language, with
    /// nothing checking. That is precisely the failure mode the `static_assert`s
    /// in this file exist to make impossible, so importing it back in through
    /// the shader would be a poor trade for 16 bytes a frame.
    float ibl_max_level;

    float ibl_pad0;           ///< 180
    float ibl_pad1;           ///< 184
    float ibl_pad2;           ///< 188 — fills the twelfth register
};

static_assert(sizeof(scene_light_uniforms) == 192, "twelve registers, exactly filled");
static_assert(offsetof(scene_light_uniforms, ibl_intensity) == 172,
              "Lesson 6.15 must land in 6.8's pad, or it is not free");
static_assert(offsetof(scene_light_uniforms, ibl_max_level) == 176, "");

/// The per-cascade half of the shadow contract. Lesson 6.9, **fragment slot 2**.
///
/// A SEPARATE BLOCK RATHER THAN A BIGGER `scene_light_uniforms`, for the reason
/// 6.7 gave when it refused to grow the per-draw block: pushes are billed by how
/// often they happen. This is per-FRAME data pushed once, beside a per-frame
/// light block and a per-draw material block, and keeping the three separate is
/// what lets the biggest one be pushed the fewest times.
///
/// **Everything here is `float4`-aligned on purpose.** Four floats that logically
/// form an array of four scalars are written as one `float4` because HLSL packs a
/// `float[4]` into four SEPARATE registers — sixteen bytes each, twelve of them
/// padding. Lesson 4.6 met the same rule from the other side; here it is the
/// difference between 320 bytes and 704.
struct cascade_uniforms
{
    /// One `clip_from_world` per cascade. Cascade i's matrix at index i.
    mat4 light_clip_from_world[4];   ///< 0

    /// The far distance of each cascade, in VIEW space — the same number the
    /// splits were computed in, so selection cannot disagree with the fit.
    vec4 splits;                     ///< 256

    /// `world_per_texel` per cascade. The only per-cascade term in 6.8's bias,
    /// which is why nothing else in the bias had to change.
    vec4 world_per_texel;            ///< 272

    /// `depth_range` per cascade.
    vec4 depth_range;                ///< 288

    float cascade_count;             ///< 304 — as a float; the shader compares it
    float blend_fraction;            ///< 308 — 0 shows the seam, which is the point
    float cpad0;                     ///< 312
    float cpad1;                     ///< 316

    /// The camera's forward axis, world space, unit length.
    ///
    /// **Because the splits are in AXIAL depth and the fragment only knows a
    /// position.** `length(eye - world)` is the RADIAL distance, and the two
    /// differ by 1/cos(angle off axis) — at a 60-degree vertical field of view
    /// the screen corner is about 35 degrees off axis, so radial distance
    /// overstates depth there by roughly 22%. Selecting on it would put the
    /// corners of the screen in the wrong cascade and bend every seam into a
    /// curve. `dot(world - eye, forward)` is the number the splits were computed
    /// in. Sixteen bytes to keep selection and fitting talking about the same
    /// quantity.
    vec4 view_forward;               ///< 320 — xyz used, w spare
};

static_assert(sizeof(cascade_uniforms) == 336, "twenty-one registers, exactly filled");
static_assert(offsetof(cascade_uniforms, view_forward) == 320, "");
static_assert(offsetof(cascade_uniforms, splits) == 256, "four matrices first");
static_assert(offsetof(cascade_uniforms, cascade_count) == 304, "");
static_assert(offsetof(scene_light_uniforms, light_clip_from_world) == 64,
              "a float4x4 must start on a register boundary");
static_assert(offsetof(scene_light_uniforms, shadow_strength) == packed_offset(128, 1), "");
static_assert(offsetof(scene_light_uniforms, shadow_mode) == packed_offset(160, 1), "");
static_assert(offsetof(scene_light_uniforms, shadow_texel_uv) == packed_offset(168, 1), "");
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
/// missing idea. What it is NOT is the cull mode or the fill mode — those are
/// pipeline state, they cannot be a number in a buffer, and that is exactly why
/// Lesson 4.8 ends up with three pipelines and a sort. Lesson 6.5 builds the
/// material that owns both halves.
///
/// **`albedo` is LINEAR**, decoded from the demo's `Uint32` tint before the push.
/// It multiplies a quantity of light two lines later, and Lesson 1.6's rule has
/// not softened.
///
/// **LESSON 6.4 REPLACED THE SURFACE HALF AND THE STRUCT DID NOT CHANGE SIZE.**
/// It went from `{albedo, shininess, specular, textured}` to
/// `{albedo, roughness, metallic, f0, textured}` — four floats of description
/// where there were four before, still exactly two 16-byte registers, so not one
/// line of the binding code moved. That is a coincidence worth noticing rather
/// than relying on: the packing rules (Lesson 4.5) put a `vec3` and a following
/// `float` in one register, and the second register happens to hold four scalars
/// as comfortably as it held a `vec3` and a scalar.
///
/// The `static_assert`s below are the reason a change this large was safe to make
/// in one commit. A uniform layout that disagrees with its shader does not fail
/// loudly; it renders something *plausible* with the fields shifted, which is the
/// worst kind of bug there is. Every offset is pinned.
struct material_uniforms
{
    vec3  albedo;      ///<  0 — the surface's own colour, linear, in [0,1]
    float roughness;   ///< 12 — perceptual, [0,1]; `alpha` is its square (6.3)
    float metallic;    ///< 16 — 0 = dielectric, 1 = conductor (6.4)
    float f0;          ///< 20 — dielectric normal-incidence reflectance, ~0.04
    float textured;    ///< 24 — 0 = use `albedo`, 1 = sample the bound texture

    /// 28 — 0 = shade with the geometric normal, 1 = perturb it from
    /// `normal_map`. **Lesson 6.7, and it costs zero bytes**: this slot was
    /// `pad0`, added in 6.4 purely to keep the block at 32, and the block is
    /// still exactly 32. No binding code moved and no shader's register
    /// allocation changed.
    ///
    /// Like `textured`, it is DERIVED at the push from `material::normal_mapped()`
    /// rather than stored on the material, so it cannot contradict the handle it
    /// describes (6.5 §5).
    ///
    /// A float and not a bool for the reason 4.7 gave about `textured`: a
    /// constant buffer has no 1-byte type, HLSL's `bool` is four bytes with
    /// packing rules of its own, and the shader wants it as a `lerp` weight
    /// anyway — which is a multiply instead of a branch, and every fragment in
    /// the draw takes the same path regardless.
    float normal_mapped;

    // ---- Lesson 6.11, and this one is NOT free -----------------------------
    //
    // 6.7 got its flag for nothing, because 6.4 had left a `pad0` behind and a
    // flag is exactly the size of a pad. There is no pad left. Two more floats
    // take the block from 32 bytes to 40, and a constant buffer is allocated in
    // 16-byte registers, so 40 becomes 48: **a whole third register for eight
    // bytes of payload.** That is worth stating plainly rather than hiding
    // behind a `static_assert`, because it is the moment a uniform block starts
    // costing what a uniform block costs, and the answer at scale is not "add
    // fewer fields" — it is to stop pushing per-draw constants and index into a
    // storage buffer instead, which is Module 9's territory.
    //
    // WHY NOT PACK THE FOUR 0-OR-1 FLOATS INTO ONE BITFIELD? Because `textured`,
    // `normal_mapped` and `alpha` are each read as a `lerp` weight or a multiply
    // in the shader, and unpacking a bitfield costs an `and` and a compare per
    // fragment to save eight bytes per DRAW. The arithmetic is not close.

    /// The material's opacity — `material::alpha`, multiplied into the albedo
    /// texture's own alpha in the shader. 1 for every opaque material, which is
    /// every material written before this lesson.
    float alpha;

    /// The coverage below which a masked fragment is discarded.
    ///
    /// **ZERO MEANS "DO NOT TEST", and that is a deliberate encoding rather than
    /// a sentinel to be embarrassed about.** The shader's test is
    /// `clip(a - alpha_cutoff)`, which discards when the argument is negative; at
    /// a cutoff of 0 nothing with non-negative alpha can be discarded, so the
    /// test costs one subtract and disables itself. That is why `alpha_mode`
    /// needs no third pipeline and no `#ifdef`: **masking is a number in a
    /// buffer**, exactly as `blend.hpp` claims, and this field is the number.
    ///
    /// `uniforms_of` derives it — a `mask` material pushes its cutoff and every
    /// other material pushes 0 — so a `blend` material cannot accidentally clip.
    float alpha_cutoff;

    /// 40 -> 48. Named `alpha_pad` and not `pad1`, because HLSL cbuffer members
    /// share ONE global namespace across every buffer in a shader (6.4 found
    /// that out the hard way), so every pad in this file needs its own name.
    /// 40 — **Lesson 6.14, and it cost zero bytes.** This was `alpha_pad0`, the
    /// slot 6.11 had to add to fill a register — the same free ride 6.7 got from
    /// 6.4's padding, three lessons later. Non-zero widens the NDF to cover the
    /// normal's variation across the pixel; see `antialias.hpp`.
    float specular_aa;
    float alpha_pad1;
};

static_assert(sizeof(material_uniforms) == 48, "three registers — 6.11 bought the third");
static_assert(offsetof(material_uniforms, albedo) == packed_offset(0, 3), "");
static_assert(offsetof(material_uniforms, roughness) == packed_offset(12, 1), "");
static_assert(offsetof(material_uniforms, metallic) == packed_offset(16, 1), "");
static_assert(offsetof(material_uniforms, f0) == packed_offset(20, 1), "");
static_assert(offsetof(material_uniforms, textured) == packed_offset(24, 1), "");
static_assert(offsetof(material_uniforms, normal_mapped) == packed_offset(28, 1), "");
static_assert(offsetof(material_uniforms, alpha) == packed_offset(32, 1), "");
static_assert(offsetof(material_uniforms, alpha_cutoff) == packed_offset(36, 1), "");
static_assert(offsetof(material_uniforms, specular_aa) == packed_offset(40, 1), "");

/// What the resolve pass reads — Lesson 6.12.
///
/// **One register of payload and one of padding**, and the padding is honest: a
/// constant buffer is allocated in 16-byte registers, and five floats need two of
/// them whatever order they go in. Naming the pads rather than leaving the tail
/// of the struct undefined is the rule this file has followed since 6.4 — and the
/// names are unique across the whole shader, because HLSL cbuffer members share
/// ONE namespace and a second `pad0` is a redefinition.
struct tonemap_uniforms
{
    float exposure = 1.0f;      ///<  0 — multiplied BEFORE the curve
    float white = 4.0f;         ///<  4 — what `reinhard_white` maps to exactly 1
    float op = 0.0f;            ///<  8 — 0 clamp, 1 reinhard, 2 reinhard-white, 3 aces
    float per_channel = 1.0f;   ///< 12 — 1 = per channel, 0 = luminance only

    /// 16 — **`encode_output`'s third situation, moved where it can be answered.**
    ///
    /// In `scene.frag.hlsl` that flag means "does the target I write to apply the
    /// transfer function?", and once the scene renders to a FLOAT target the
    /// answer there is permanently "no". The question did not go away; it moved
    /// to the pass that actually writes the display, which is the only place it
    /// was ever answerable. 1 when the swapchain is a plain UNORM, 0 when it is
    /// `_SRGB` and the hardware encodes on the write.
    float encode = 0.0f;

    /// 20 — **Lesson 6.13.** How much of the bloom target is added before the
    /// curve. Zero means the resolve reads the bloom texture and multiplies it by
    /// nothing, which is deliberate: a branch would cost a divergent fetch on
    /// every fragment to save a multiply on some of them, and the binding has to
    /// be valid either way (SDL_GPU has no "unbind a sampler" — see
    /// `gpu_post_stack`'s note on the dummy texture).
    ///
    /// NOT a percentage. `bloom_settings::intensity` explains why: the upsample
    /// chain adds every level at full weight, so the pyramid's gain is about
    /// `levels`, and this scalar absorbs it.
    float bloom_intensity = 0.0f;

    float pad_t1 = 0.0f;        ///< 24
    float pad_t2 = 0.0f;        ///< 28
};

static_assert(sizeof(tonemap_uniforms) == 32, "two registers, exactly filled");
static_assert(offsetof(tonemap_uniforms, exposure) == packed_offset(0, 1), "");
static_assert(offsetof(tonemap_uniforms, white) == packed_offset(4, 1), "");
static_assert(offsetof(tonemap_uniforms, op) == packed_offset(8, 1), "");
static_assert(offsetof(tonemap_uniforms, per_channel) == packed_offset(12, 1), "");
static_assert(offsetof(tonemap_uniforms, encode) == packed_offset(16, 1), "");
static_assert(offsetof(tonemap_uniforms, bloom_intensity) == packed_offset(20, 1), "");

/// Pack `tonemap_settings` into the block the resolve shader reads.
///
/// **The operator crosses as a NUMBER, spelled out rather than cast**, which is
/// 6.4's rule about enums crossing a language boundary: `engine::tonemap` and the
/// shader's `op` comparisons happen to agree today, and a `static_cast` would
/// turn the day somebody reorders the enum into a silent change of curve. The
/// switch stops compiling instead.
[[nodiscard]] inline tonemap_uniforms uniforms_of(const tonemap_settings& s, bool shader_encodes,
                                                  float bloom_intensity = 0.0f)
{
    float op = 0.0f;
    switch (s.op)
    {
    case tonemap::clamp:          op = 0.0f; break;
    case tonemap::reinhard:       op = 1.0f; break;
    case tonemap::reinhard_white: op = 2.0f; break;
    case tonemap::aces:           op = 3.0f; break;
    }
    return {.exposure = s.exposure,
            .white = s.white,
            .op = op,
            .per_channel = s.per_channel ? 1.0f : 0.0f,
            .encode = shader_encodes ? 1.0f : 0.0f,
            .bloom_intensity = bloom_intensity};
}

// ---- Lesson 6.13: the bloom's two blocks ------------------------------------

/// What `bloom_bright.frag.hlsl` reads.
///
/// **`texel_size` is the SOURCE's, not the destination's**, at every stage of the
/// chain, and it is the single most common way to get a pyramid subtly wrong. The
/// offsets these shaders compute are positions in the texture being READ; using
/// the destination's texel size makes every tap land at twice (or half) the
/// intended spacing, and because the error scales with the level the symptom is a
/// bloom that is too tight at one end of the pyramid and too loose at the other —
/// which reads as "the blur radius is wrong" rather than as an addressing bug.
struct bloom_bright_uniforms
{
    float texel_w = 0.0f;       ///<  0 — 1 / source width
    float texel_h = 0.0f;       ///<  4 — 1 / source height
    float exposure = 1.0f;      ///<  8 — the SAME number the resolve will use
    float threshold = 1.0f;     ///< 12 — in exposure-corrected linear light

    float knee = 0.5f;          ///< 16 — half-width of the soft transition
    float clamp_max = 0.0f;     ///< 20 — the firefly ceiling; <= 0 disables
    float pad_b0 = 0.0f;        ///< 24
    float pad_b1 = 0.0f;        ///< 28
};

static_assert(sizeof(bloom_bright_uniforms) == 32, "two registers, exactly filled");
static_assert(offsetof(bloom_bright_uniforms, exposure) == packed_offset(8, 1), "");
static_assert(offsetof(bloom_bright_uniforms, knee) == packed_offset(16, 1), "");

/// What `bloom_down.frag.hlsl` and `bloom_up.frag.hlsl` read.
///
/// One block for two shaders, because the downsample's needs are a strict subset
/// of the upsample's — and `radius` being present-but-unread by the downsample is
/// cheaper than a second sixteen-byte block and a second `uniforms_of`. Sixteen
/// bytes is the minimum a uniform push costs anyway.
struct bloom_filter_uniforms
{
    float texel_w = 0.0f;       ///<  0 — 1 / SOURCE width
    float texel_h = 0.0f;       ///<  4 — 1 / SOURCE height
    float radius = 1.0f;        ///<  8 — tent spacing in source texels; upsample only
    float pad_f0 = 0.0f;        ///< 12
};

static_assert(sizeof(bloom_filter_uniforms) == 16, "one register, exactly filled");

/// Pack the bright pass's block. `src_w/src_h` are the SCENE's dimensions.
[[nodiscard]] inline bloom_bright_uniforms uniforms_of(const bloom_settings& s,
                                                       float exposure,
                                                       int src_w, int src_h)
{
    return {.texel_w = (src_w > 0) ? 1.0f / static_cast<float>(src_w) : 0.0f,
            .texel_h = (src_h > 0) ? 1.0f / static_cast<float>(src_h) : 0.0f,
            .exposure = exposure,
            .threshold = s.threshold,
            .knee = s.knee,
            .clamp_max = s.clamp_max};
}

/// Pack a filter block. `src_w/src_h` are the dimensions of the level being READ.
[[nodiscard]] inline bloom_filter_uniforms filter_uniforms_of(int src_w, int src_h,
                                                              float radius)
{
    return {.texel_w = (src_w > 0) ? 1.0f / static_cast<float>(src_w) : 0.0f,
            .texel_h = (src_h > 0) ? 1.0f / static_cast<float>(src_h) : 0.0f,
            .radius = radius};
}

/// Pack a `material` into the block the fragment shader reads — Lesson 6.5.
///
/// **The one place the CPU-side material becomes GPU-side numbers**, and having
/// exactly one of them is the point. Before this lesson every call site assembled
/// the block by hand: five assignments, repeated in the sandbox, in `verify_48`
/// and in `verify_49`, each free to get the fifth one wrong.
///
/// That fifth one is `textured`, and it is the reason this function exists rather
/// than being a constructor. It is **derived**, from the only field that knows —
/// so a material with an albedo map always pushes 1, and one without always
/// pushes 0, and the two can no longer disagree. A hand-assembled block with
/// `textured = 1` and no texture bound draws the sampler's debug magenta; with
/// `textured = 0` and a texture bound it silently ignores the image. Neither
/// fails at the point of the mistake.
///
/// `albedo` is decoded here, which is Lesson 6.1's rule about edges: the material
/// stores the artist's sRGB `Uint32`, and this is the input edge where it meets
/// arithmetic. The software path decodes at the same conceptual point, inside
/// `shade_encoded`.
[[nodiscard]] inline material_uniforms uniforms_of(const material& m,
                                                   bool specular_aa = false)
{
    const linear_rgb albedo = to_linear(m.tint);
    return {.albedo = vec3{albedo.r, albedo.g, albedo.b},
            .roughness = m.surface.roughness,
            .metallic = m.surface.metallic,
            .f0 = m.surface.f0,
            .textured = m.textured() ? 1.0f : 0.0f,
            // 6.7: derived, like `textured`, from the only field that knows.
            .normal_mapped = m.normal_mapped() ? 1.0f : 0.0f,
            // 6.11: DERIVED for exactly the reason 6.5 gave — one field cannot
            // contradict itself. A `blend` material pushing a live cutoff would
            // clip its own soft edges away; an `opaque` one pushing its texture's
            // alpha would make every leaf-shaped hole in every atlas suddenly
            // real. The mode decides both numbers, here, in one place.
            //
            // (DESIGNATORS MUST BE IN DECLARATION ORDER — `-Wreorder-init-list`
            // is an error waiting to happen rather than a style note, because
            // C++20 evaluates them in declaration order regardless of how they
            // are written, so a reordered list means the initialisers run in an
            // order the reader did not choose.)
            .alpha = (m.mode == alpha_mode::opaque) ? 1.0f : m.alpha,
            .alpha_cutoff = (m.mode == alpha_mode::mask) ? m.alpha_cutoff : 0.0f,
            // 6.14: NOT derived from the material, and that is the interesting
            // part. Whether to filter the NDF is a property of how the surface is
            // being VIEWED — a sphere 400 px across needs it and the same sphere
            // at 4 px needs it far more — so it cannot be a field of `material`
            // the way `roughness` is. It is a render setting that happens to
            // arrive through the material's block.
            .specular_aa = specular_aa ? 1.0f : 0.0f,
            .alpha_pad1 = 0.0f};
}

} // namespace engine
