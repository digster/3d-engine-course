// engine/include/engine/gfx/blend.hpp — what to do when a surface does not stop the light.
//
// Lesson 6.11, and it is worth being precise about what kind of lesson this is,
// because it is not "here is a new feature". Three facts were all already true
// before a line of this file existed:
//
//   1. `gpu_pipeline.hpp` has said **"no blending"** since Lesson 4.4. There was
//      no blend state anywhere in this engine, on either renderer.
//   2. Lesson 6.6's glTF importer reads `baseColorFactor[3]` into
//      `gltf_material_desc::alpha` and **ignores `alphaMode` and `alphaCutoff`
//      entirely**, so every material an artist marked `MASK` or `BLEND` imported
//      as fully opaque.
//   3. That was the one gap in that importer which **fired no status**, and
//      Lesson 6.6 §10 says so in as many words. The reason was structural rather
//      than sloppy: a status code says *"this file wants something the engine
//      cannot do"*, and you cannot report a conflict with a concept that does
//      not exist. There was no blend state to conflict with.
//
// So cutout foliage and window glass have been importing as opaque cardboard,
// silently, for five lessons. This file is what closes that loop — and closing
// it removes the gap rather than reporting it, which is the better of the two
// honest outcomes.
//
// ---------------------------------------------------------------------------
// THE ONE SENTENCE TO KEEP
// ---------------------------------------------------------------------------
//
//   **Alpha is a coverage fraction, not a quantity of light.**
//
// `colour.hpp` has said this since Lesson 1.6 — "alpha is a coverage fraction
// rather than a quantity of light, so it does not belong in a type whose whole
// purpose is to be linear in light. Carry it separately if you need it." This
// file is what "carry it separately" turned out to mean, and every design
// decision below falls out of that sentence:
//
//   * `texel_sample` is a `linear_rgb` **and** a float, not a four-channel
//     colour, because three of those numbers are light and the fourth is not.
//   * a mip chain averages alpha **without** a transfer function, while it
//     averages colour with one (6.10 already got this right).
//   * the *colours* being composited must be decoded first and the *weights*
//     must not, which is the whole of `blend_over` below.
//
// ---------------------------------------------------------------------------
// AND ONE THING THIS FILE IS NOT
// ---------------------------------------------------------------------------
//
// It is not order-independent transparency. Blending is not commutative — §6 of
// the lesson shows two intersecting quads for which no per-object order is
// correct — and the honest answer at this stage of the course is a per-frame
// back-to-front sort (`draw_order.hpp`) plus a named limitation. Weighted
// blended OIT, depth peeling and per-pixel linked lists all exist and none of
// them is a small addition; see the lesson's Further Reading.

#pragma once

#include <engine/gfx/colour.hpp>

#include <SDL3/SDL.h>

namespace engine {

// ---- What kind of transparency a surface has --------------------------------

/// glTF 2.0's three alpha modes, which are also everybody else's three.
///
/// **These three values are not three settings of one dial.** They land on
/// opposite sides of the line Lesson 6.5 drew through `material` — *which half
/// can be a number in a buffer?* — and that is the most useful thing on this
/// page:
///
///   `opaque`  no state at all. The z-buffer is the sort (3.1) and always was.
///   `mask`    **a number in a buffer.** The fragment compares its alpha against
///             a cutoff and discards itself. Nothing about the pipeline changes;
///             `alpha_cutoff` is a uniform, like `roughness`.
///   `blend`   **pipeline state.** Blend factors, a blend op, and depth-write
///             turned off. Two objects that differ here cannot be drawn back to
///             back without a pipeline change between them, which is exactly
///             Lesson 4.8's definition of the other side of that line.
///
/// So one glTF enum splits across the hardware's own boundary, and the split is
/// visible in the pipeline count: **`mask` adds zero pipelines and `blend` adds
/// six** (three surface styles times two blend styles — see `gpu_scene.cpp`).
///
/// (The ninety-percent picture. Real engines often *do* give masked geometry its
/// own pipeline, because `discard` defeats early-Z on most hardware: the depth
/// test can no longer run before the fragment when the fragment decides whether
/// there is a fragment. Ours has no early-Z to lose — the software rasterizer
/// pays the same reordering cost explicitly, and `raster.cpp` measures it — so
/// the two-pipeline answer buys nothing here. It would in Module 9.)
enum class alpha_mode
{
    /// Alpha is ignored entirely. The default, and it must stay the default:
    /// every material written before this lesson means exactly what it meant.
    opaque,

    /// Alpha is a **binary** test. Below the cutoff the fragment does not exist;
    /// at or above it, the fragment is fully opaque. Cutout foliage, chain-link
    /// fence, decals with hard edges.
    ///
    /// **The depth buffer stays honest**, which is the property worth the whole
    /// mode: a discarded fragment writes no depth and no colour, so it occludes
    /// nothing, so draw order does not matter. Masked geometry can be drawn
    /// with the opaque geometry, in any order, and comes out right.
    mask,

    /// Alpha is a **fraction**, and the fragment is composited over what is
    /// already there.
    ///
    /// This is the expensive one, and the cost is not the arithmetic. It is that
    /// the operation is **not commutative**, so the draw order becomes part of
    /// the answer — and a per-frame sort is the first thing in this engine that
    /// has to happen *before* the frame can be drawn rather than during it.
    blend
};

[[nodiscard]] const char* name_of(alpha_mode m);

/// The cutoff glTF specifies when a material says `MASK` and nothing else.
///
/// glTF 2.0 §5.19.2: `alphaCutoff` defaults to 0.5 and is ignored unless
/// `alphaMode` is `MASK`. Named rather than typed inline because it appears in
/// three places — the importer's default, `material`'s default, and the
/// coverage-preserving mip rescale, which has to test against the same number
/// the renderer will later test against or the whole exercise is pointless.
inline constexpr float k_default_alpha_cutoff = 0.5f;

// ---- How an image stores its colour beside its alpha ------------------------

/// Does a texel's RGB already have its own alpha multiplied into it?
///
/// **This is a property of the DATA, not of how you read it**, which is why it
/// lives on `texture` beside `texel_space` rather than on `sampler` beside the
/// filters. Lesson 6.7 made exactly this call about colour space and
/// `mipmap.cpp` states the rule it produced: *read the space from the data, not
/// from a parameter — a caller who has to remember which of their textures is a
/// normal map is a caller who will get the third one wrong.* A caller who has to
/// remember which of their textures is premultiplied is the same caller.
///
/// The difference only becomes visible when a texel is **averaged with its
/// neighbours** — by a bilinear filter, or by a mip chain's downsample — and
/// then it is glaring. See `blend_over_premultiplied` for the arithmetic and the
/// lesson's §5 for the picture.
enum class alpha_storage
{
    /// `(r, g, b)` is the surface's colour and `a` is its coverage, independent.
    /// What a PNG holds, what an artist paints, and the default so that every
    /// image loaded before this lesson still means what it meant.
    straight,

    /// `(r, g, b)` has already been multiplied by `a` — so a fully transparent
    /// texel is `(0, 0, 0, 0)` and carries no colour to bleed.
    ///
    /// **This is the form that survives filtering**, and it is not a matter of
    /// taste: `lerp` and `over` compose associatively in this form and do not in
    /// the other. The classic symptom of getting it wrong is a dark halo around
    /// every cutout edge, because the fully-transparent texels in a PNG are
    /// usually black, and a straight-alpha bilinear filter averages that black
    /// into the visible edge with weight proportional to how transparent its
    /// neighbours are.
    premultiplied
};

[[nodiscard]] const char* name_of(alpha_storage s);

// ---- A sampled texel, colour and coverage kept apart ------------------------

/// What `sample_rgba` returns: three quantities of light, and one fraction.
///
/// **Deliberately not a `linear_rgba`**, and this is the type that makes the
/// file's one sentence structural rather than a comment. A four-float colour
/// type invites `a.r + b.r`, `a.a + b.a` and every other component-wise loop —
/// and three of those four operations are arithmetic on light while the fourth
/// is arithmetic on coverage. They are only accidentally the same instruction.
///
/// Keeping them in separate members costs nothing at runtime (the struct is four
/// floats either way) and makes the wrong operation something you have to
/// *write out* rather than something you get for free from a loop bound.
struct texel_sample
{
    /// The colour, decoded to linear light exactly as `sample` returns it.
    ///
    /// **Whether this has been divided by `alpha` depends on the image's
    /// `alpha_storage`** — the sampler does not un-premultiply, because the
    /// division is lossy at low alpha (at `a = 1/255` it multiplies every
    /// rounding error by 255) and because the compositing arithmetic below wants
    /// the premultiplied form anyway. See `blend_over_premultiplied`.
    linear_rgb colour{};

    /// Coverage, in [0, 1]. **Not** put through any transfer function on its way
    /// here, and 1 for every image that has no meaningful alpha channel.
    float alpha = 1.0f;
};

// ---- Compositing ------------------------------------------------------------

/// The **over** operator, in linear light, straight alpha.
///
///     result = src * a + dst * (1 - a)
///
/// Porter and Duff (1984) named it, and the name is worth keeping in mind
/// because it is a statement about *coverage*: `a` is the fraction of the pixel
/// the source covers, `1 - a` is the fraction it leaves showing, and the result
/// is the area-weighted average of what is in each. It is a lerp — and knowing
/// that it is a lerp is what tells you it must happen in linear light, because
/// Lesson 2.4 already established that lerping encoded values does not lerp the
/// thing they encode.
///
/// **A worked number, because this is the one to have at your fingertips.** Half
/// coverage of white over black is `1.0 * 0.5 + 0.0 * 0.5 = 0.5` of the light,
/// which stores as **code 188** (6.1's second integer). Do the same lerp on the
/// stored bytes and you get code 128, which emits 0.2159 — **43% of the light
/// the answer should have.** That is `blend_over_encoded` below, and it is the
/// single most common transparency bug there is.
[[nodiscard]] constexpr linear_rgb over(linear_rgb src, float a, linear_rgb dst)
{
    const float inv = 1.0f - a;
    return {src.r * a + dst.r * inv,
            src.g * a + dst.g * inv,
            src.b * a + dst.b * inv};
}

/// The same operator when the source is **premultiplied**:
///
///     result = src + dst * (1 - a)
///
/// One multiply fewer, and that is not why anybody uses it. The reason is
/// **associativity**: `over` in this form composes — compositing A over B and
/// then over C gives the same answer as compositing A over (B over C) — which
/// is what makes it safe to *average premultiplied texels*. A bilinear filter, a
/// mip downsample and an antialiasing resolve are all averages of already-
/// composited fragments, and all three are correct on premultiplied data and
/// wrong on straight data.
///
/// The failure in the other direction is the halo: a straight-alpha PNG usually
/// stores `(0, 0, 0, 0)` for "nothing here", and a bilinear filter that averages
/// the colour channels independently drags that black into every edge texel.
/// Premultiplied storage makes that same texel contribute *nothing* rather than
/// contributing black, which is what "nothing here" was supposed to mean.
[[nodiscard]] constexpr linear_rgb over_premultiplied(linear_rgb src, float a, linear_rgb dst)
{
    const float inv = 1.0f - a;
    return {src.r + dst.r * inv,
            src.g + dst.g * inv,
            src.b + dst.b * inv};
}

/// Composite one stored pixel over another, **correctly**: decode both, blend in
/// linear light, re-encode.
///
/// This is the software rasterizer's fixed-function blender, and writing it out
/// makes visible something the GPU hides: **compositing is a read-modify-write,
/// and on the CPU the round trip through the transfer function is most of its
/// cost.** A GPU with an `_SRGB` colour target does exactly this — decode,
/// blend, encode — in the ROP hardware, for free, which is the real reason
/// Lesson 6.1 preferred an `_SRGB` swapchain and said so at the time: *"hardware
/// blending happens after the shader, and if we encode there, the blender adds
/// codes."* This lesson is where that sentence gets its number (§7).
///
/// @param dst   the pixel already in the framebuffer, sRGB-encoded
/// @param src   the fragment's colour, sRGB-encoded
/// @param a     the fragment's coverage, in [0, 1]
/// @param mode  which encode to use on the way back out (Lesson 3.10)
/// @param how   whether `src` is straight or premultiplied
///
/// The destination's own alpha is preserved rather than blended, because the
/// framebuffer is opaque: nothing in this engine reads a colour target's alpha,
/// and writing a blended coverage there would produce a window that is
/// gradually less opaque than the window it is drawn in.
[[nodiscard]] Uint32 blend_over(Uint32 dst, Uint32 src, float a,
                                encode_mode mode = encode_mode::exact,
                                alpha_storage how = alpha_storage::straight);

/// The same composite performed on the **stored bytes**, without decoding.
///
/// **Wrong**, and kept for exactly the reason `draw_line_naive` (1.4),
/// `blend_space::encoded` (2.4), `interpolation::affine` (3.2) and
/// `mix_encoded` (1.6) are kept: a failure you can summon with one keystroke
/// teaches more than a paragraph describing it. This is `mix_encoded`'s error
/// arriving for the fourth time in this course and the first time it is
/// somebody's *whole feature* — half-transparent white over black comes out at
/// 43% of the light it should have, so every piece of glass in the scene is
/// too dark and every fade-to-black finishes early.
///
/// It is also, for the record, what a great many shipped renderers do, because
/// blending on a non-sRGB target with a shader that encodes its own output is
/// precisely this operation performed by the hardware. See §7.
[[nodiscard]] Uint32 blend_over_encoded(Uint32 dst, Uint32 src, float a);

/// Multiply a stored pixel's colour channels by its own alpha.
///
/// **In LINEAR light**, which is the part that is easy to get wrong: alpha is a
/// coverage fraction, so multiplying by it is a physical scaling of the light
/// leaving that texel, and scaling an sRGB byte scales nothing meaningful. So
/// the operation is decode, multiply, re-encode — and it is lossy at the dark
/// end, which is why premultiplication is properly done once at load time on
/// data with more than eight bits, or not at all.
[[nodiscard]] Uint32 premultiply(Uint32 encoded);

/// The inverse, for the rare caller that must recover an artist's colour.
///
/// **Lossy, and unavoidably so.** At `a = 1/255` the division multiplies every
/// rounding error by 255, so a nearly-transparent texel comes back as noise.
/// Returns the texel unchanged when `a == 0`, because there is no colour in
/// there to recover and inventing one would be worse than admitting it.
[[nodiscard]] Uint32 unpremultiply(Uint32 encoded);

} // namespace engine
