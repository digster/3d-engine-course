// src/gfx/texture.hpp — images, and how to ask one what colour is at (u, v).
//
// Lesson 3.9. Everything before this lesson computed a fragment's colour from a
// RULE: a blend of three corner colours (2.4), a checkerboard evaluated from a
// formula (3.2), a lighting equation (3.6-3.8). A texture is the other answer —
// look it up in an array somebody painted — and the whole subject of this file is
// that the lookup is not as simple as an array index.
//
// Three questions have to be settled before `image[y][x]` is a defensible answer,
// and each one has a visible failure if you get it wrong:
//
//   WHERE IS (0, 0)?          Top-left, v downwards. Not a taste question: it is
//                             what SDL_GPU does, quoted below, and Module 4 is a
//                             port rather than a redesign.
//   WHAT IS OUTSIDE [0, 1]?   Three answers, all useful, all different — and the
//                             wrong one shows as a smeared edge or a visible seam.
//   WHAT IS BETWEEN TEXELS?   A texel is a SAMPLE, not a square. Its value belongs
//                             at its CENTRE, and forgetting the half-texel that
//                             follows from that is the single most common texture
//                             bug there is.
//
// The types here mirror `SDL_GPUSampler` deliberately — same names, same
// enumerator order — so that when Module 4 replaces this file with a GPU sampler
// object, the change is a rename and not a re-education.

#pragma once

#include "gfx/colour.hpp"   // linear_rgb: what a sample IS, once decoded
#include "math/vec2.hpp"

#include <SDL3/SDL.h>

#include <cstddef>
#include <span>
#include <vector>

namespace engine {

// ---- The sampler's two enums -----------------------------------------------

/// How to combine texels when the sample point falls between them.
///
/// Enumerator order matches `SDL_GPUFilter` exactly — verified in
/// `SDL3/SDL_gpu.h`, where it is `SDL_GPU_FILTER_NEAREST`, `SDL_GPU_FILTER_LINEAR`
/// in that order — so Module 4's port is a rename.
enum class filter
{
    /// Take the one texel the sample point lands in. Sharp, cheap, exact — and
    /// the reason a magnified texture looks like a mosaic of hard squares.
    nearest,

    /// Blend the four texels whose centres surround the sample point, weighted by
    /// how close each one is. **Bilinear filtering**, and the default, because a
    /// magnified texture is the common case and blocky is worse than soft.
    ///
    /// Note the exact claim: *four texels whose CENTRES surround the point*. Not
    /// "the four texels nearest the point", which is the same thing said loosely
    /// and the sentence that produces the half-texel bug.
    linear
};

/// What a texture coordinate outside `[0, 1]` means.
///
/// Enumerator order matches `SDL_GPUSamplerAddressMode` exactly — `REPEAT`,
/// `MIRRORED_REPEAT`, `CLAMP_TO_EDGE` — verified in `SDL3/SDL_gpu.h`.
///
/// **This is a real choice with three right answers**, not a default plus two
/// curiosities. A tiled floor wants `repeat`; a decal or a skybox face wants
/// `clamp_to_edge`, because repeating it would wrap the far edge into view; a
/// pattern that must not show a seam and was not authored to tile wants
/// `mirrored_repeat`, which makes every boundary a reflection and therefore
/// continuous by construction.
enum class address_mode
{
    /// Wrap around: texel `n` is texel `0`, texel `-1` is texel `n-1`. The tiling
    /// mode. Shows a **seam** if the image's last column and first column are not
    /// continuous with each other — which is a property of the image, not a bug in
    /// the sampler.
    repeat,

    /// Wrap around, reflecting every other copy. Period `2n`, and the edge texel
    /// appears twice at each fold. Never seams, at the price of a pattern that
    /// reads as mirrored rather than repeated.
    mirrored_repeat,

    /// Clamp to the outermost texel. Outside the image, the edge row or column
    /// **smears outward forever** — which is exactly right for a decal and exactly
    /// wrong for a floor, where it turns everything past the first tile into a
    /// stretched streak.
    clamp_to_edge
};

/// Where in a texel its value is deemed to live. **There is only one right
/// answer**; the other is kept so it can be switched on and seen failing.
///
/// The ninth keep-the-wrong-thing bargain in this engine, after `draw_line_naive`
/// (2.1), Pong's naive collision test (1.8), `blend_space::encoded` (2.4), the `w`
/// toggles (2.7), `trs_order` (2.8), `interpolation::affine` (3.2), `near_mode`
/// (3.3), `cull_choice::back_by_forward` (3.4) and Phong (3.7).
enum class texel_origin
{
    /// **Correct.** Texel `i` holds the value of the image at `(i + 0.5) / n`. A
    /// texel is a *sample of a continuous image*, and a sample has a position, not
    /// an extent — the little square you draw when you visualise a texture is a
    /// rendering convenience, not what the number means.
    centre,

    /// **Wrong**, and wrong in the most plausible possible way: treat texel `i` as
    /// occupying the square starting at `i / n`. Nearest-neighbour sampling cannot
    /// tell the difference — §5.3 measures exactly zero pixels changed — and
    /// bilinear sampling shifts the whole image by half a texel *and* blurs it,
    /// because the sample point that should have landed on a texel centre now
    /// lands exactly between two.
    corner
};

/// Everything about *how* to read a texture, gathered into one object.
///
/// The shape is `SDL_GPUSamplerCreateInfo` with the fields we can honestly
/// implement. That struct has thirteen members; three of them are ours today, and
/// naming the gap is more useful than pretending there is none:
///
///   - `min_filter` / `mag_filter` / `mipmap_mode` are **one** field here, because
///     without mipmaps there is nothing different for a minification filter to do.
///     That is not a simplification being hidden — it is §7's entire subject. The
///     sparkle on a floor running to the horizon *is* the missing minification
///     filter, and Module 6's mipmaps are what fills the gap.
///   - `address_mode_w` is absent because we have no 3-D textures.
///   - anisotropy, LOD clamping and comparison sampling are all Module 6.
struct sampler
{
    /// How to combine texels. One field standing in for SDL's three; see above.
    filter texel_filter = filter::linear;

    /// Addressing per axis, and **per axis for a reason**: a strip of road wants to
    /// repeat along its length and clamp across its width, and one mode for both
    /// cannot say that.
    address_mode address_u = address_mode::repeat;
    address_mode address_v = address_mode::repeat;

    /// Always `centre` in anything that is not a demonstration.
    texel_origin origin = texel_origin::centre;
};

// ---- The image itself -------------------------------------------------------

/// A 2-D array of texels, owned.
///
/// **Row 0 is the TOP row**, and that is the load-bearing sentence of this whole
/// file. SDL3's `SDL_gpu.h` states the convention in its "Coordinate System"
/// section, verbatim:
///
///   > **Texture Coordinates:** The top-left corner has an x,y coordinate of
///   > `(0, 0)` and extends to the bottom-right corner at `(1.0, 1.0)`. +Y is down.
///
/// So `v = 0` is the top of the image and `v` increases downwards, which lines up
/// with the framebuffer's own +y-down convention (Lesson 1.5) and with the
/// viewport transform's y-flip (2.11). Every space this engine touches on the
/// screen side of the projection now agrees.
///
/// **It does not agree with an OBJ file.** Wavefront writes `vt` with the origin
/// at the *bottom* left, which is the older, mathematician's convention, and the
/// disagreement is a straight vertical flip — see `flip_uv_v` in `mesh.hpp`.
///
/// Texels are `Uint32` ARGB8888, byte-identical to a framebuffer pixel. That is
/// not laziness: it means a texture drawn at 1:1 is a copy rather than a
/// conversion, and §5.1 uses exactly that to check the sampler against the image
/// it is sampling.
///
/// **Texels are sRGB-ENCODED**, like every other stored colour in this engine and
/// like every PNG an artist will ever hand you. Decoding is the *sampler's* job
/// (`sample` returns `linear_rgb`), which is also what hardware does: an
/// `SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB` texture is decoded in the sampler,
/// before filtering, for free. Lesson 1.6's argument, arriving for the third time.
class texture
{
public:
    texture() = default;

    /// An image of `w` x `h` texels, every one set to `fill`.
    ///
    /// A non-positive dimension gives an **empty** texture rather than a thrown
    /// exception or an undefined one — the engine core has no exceptions (§4 of the
    /// course conventions) and `sample` has a defined answer for an empty image.
    texture(int w, int h, Uint32 fill = 0xFF000000u);

    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }
    [[nodiscard]] bool empty() const { return texels_.empty(); }

    /// The texel at integer coordinates, **clamped** into range.
    ///
    /// The clamp is not the addressing mode — addressing happens in the sampler,
    /// which decides what `x = -1` *means* before it ever gets here. This clamp is
    /// the last line of defence: it guarantees this function cannot read memory the
    /// texture does not own, whatever a caller does. An empty texture returns
    /// opaque black, because there is no texel to return and a debug magenta here
    /// would fire on every pixel of a legitimately-unbound sampler.
    [[nodiscard]] Uint32 texel(int x, int y) const;

    void set_texel(int x, int y, Uint32 colour);

    /// The raw texels, row-major, top row first. For blits and for tests that
    /// compare an image against what was drawn from it.
    [[nodiscard]] std::span<const Uint32> texels() const { return texels_; }

private:
    std::vector<Uint32> texels_;
    int width_ = 0;
    int height_ = 0;
};

// ---- Addressing --------------------------------------------------------------

/// Fold an out-of-range texel index into `[0, n)` according to `mode`.
///
/// Exposed rather than hidden inside the sampler because it is the piece with the
/// most arithmetic per line and the least visible failure, so it deserves to be
/// testable on its own — the same argument `is_top_left` (2.4) and
/// `is_front_facing` (3.4) made. §5.2 tabulates its output.
///
/// `n <= 0` returns 0, so an empty texture cannot produce an index.
[[nodiscard]] int wrap_texel(int i, int n, address_mode mode);

// ---- Sampling ----------------------------------------------------------------

/// **The one to call.** What colour is the image at texture coordinate `(u, v)`?
///
/// Returns **linear light**, not a stored pixel, because a texture is a material
/// input and material inputs get multiplied by quantities of light. Decoding here
/// rather than at the call site also puts the decode *before* the filter, which is
/// the correct order and not the obvious one: blending four encoded bytes and
/// decoding the result is a different (and wrong) answer, for exactly the reason
/// Lesson 2.4 gave about blending vertex colours. §4.4 measures the difference.
///
/// An empty texture samples as opaque **magenta** — the debug convention this
/// engine already uses for "this value is invalid" (`checker_at`, 3.2) — because a
/// fill configured to sample a texture that is not there is a mistake worth seeing
/// rather than a black surface worth misreading as unlit.
[[nodiscard]] linear_rgb sample(const texture& image, const sampler& samp, float u, float v);

/// `sample` with the `vec2` a mesh actually stores.
[[nodiscard]] inline linear_rgb sample(const texture& image, const sampler& samp, vec2 uv)
{
    return sample(image, samp, uv.x, uv.y);
}

/// The two halves of `sample`, separated so each can be measured against the other.
///
/// Both take the sampler for its addressing and origin fields and ignore
/// `texel_filter`, which is what selects between them.
[[nodiscard]] linear_rgb sample_nearest(const texture& image, const sampler& samp,
                                        float u, float v);
[[nodiscard]] linear_rgb sample_bilinear(const texture& image, const sampler& samp,
                                         float u, float v);

// ---- Binding -----------------------------------------------------------------

/// An image and the rules for reading it, travelling together.
///
/// `SDL_GPUTextureSamplerBinding` is exactly this pair — `{ SDL_GPUTexture*,
/// SDL_GPUSampler* }` — and it is a pair for a good reason: the same image is
/// legitimately read two ways in one frame (clamped for a decal, repeated for a
/// floor), and the same rules are legitimately applied to a hundred images. They
/// are independent objects joined at the point of use.
///
/// **Non-owning.** The texture outlives the binding, exactly as `fill_style`'s
/// `lighting*` does (3.8).
struct texture_binding
{
    const texture* image = nullptr;
    sampler samp{};

    /// Is there anything to sample? A binding with no image is not an error — it
    /// is a pipeline that was never given one, and the fill falls back to vertex
    /// colours.
    [[nodiscard]] bool bound() const { return image != nullptr && !image->empty(); }
};

// ---- Generated test images ---------------------------------------------------
//
// No image DECODER this lesson, and that is a deliberate boundary rather than an
// omission. Decoding PNG or JPEG is a compression problem, not a graphics one:
// stb_image is the approved answer (course conventions §4) and it arrives with the
// asset pipeline. Everything in this file takes an array of texels and does not
// care where the array came from — which is precisely why generating it in memory
// costs the lesson nothing.

/// A checkerboard of `cells` x `cells` squares over a `size` x `size` image.
///
/// The classic texture-debugging image, now as actual texels rather than 3.2's
/// procedural rule. `size` should be a multiple of `cells` or the squares come out
/// uneven — which is itself worth seeing once, so it is not forbidden.
[[nodiscard]] texture make_checker(int size, int cells, Uint32 a, Uint32 b);

/// An orientation chart: four differently-coloured quadrants, a grid, and a mark
/// in the top-left corner.
///
/// Its whole job is to make **which way up** unmistakable. A checkerboard is
/// symmetric under every flip and rotation, so it cannot show you that your `v` is
/// upside down; this can, at a glance, and §3.2 uses it to settle the OBJ-versus-
/// SDL_GPU disagreement by looking at it rather than by arguing about it.
///
/// Quadrant colours, which §5.4 asserts by sampling:
///   top-left RED, top-right GREEN, bottom-left BLUE, bottom-right AMBER.
[[nodiscard]] texture make_uv_grid(int size);

} // namespace engine
