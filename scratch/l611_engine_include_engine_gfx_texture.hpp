// engine/include/engine/gfx/texture.hpp — images, and how to ask one what colour is at (u, v).
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

#include <engine/core/handle.hpp>   // 6.5: a texture is referenced, not pointed at
#include <engine/core/pool.hpp>
#include <engine/gfx/blend.hpp>    // 6.11: alpha_storage, texel_sample
#include <engine/gfx/colour.hpp>   // linear_rgb: what a sample IS, once decoded
#include <engine/gfx/image.hpp>    // 6.6: image_data, the thing a file decodes to
#include <engine/math/vec2.hpp>

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

// ---- Lesson 6.7: what the numbers in a texture MEAN --------------------------

/// Is this image **colour**, or is it **data**?
///
/// Lesson 6.7, and this is the field whose absence Lesson 6.6 had to name and
/// walk away from. Every texture in this engine until now was an albedo — an
/// authored colour, sRGB-encoded like every stored colour since Lesson 1.6 — so
/// `sample` could decode unconditionally and be right every time. A normal map
/// is not a colour. Its three channels are a **direction**, packed into bytes,
/// and running them through the sRGB curve is arithmetic on numbers that were
/// never a colour in the first place.
///
/// **THE GPU HAS HAD THIS SINCE LESSON 4.7 AND THE CPU HAS NEVER HAD IT.**
/// `gpu_texture::create_sampled` takes an `srgb` flag and asks for an
/// `SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB` format when it is set. That is
/// the same gap 6.6 found between `load_image` and `sample` — two halves of a
/// thing, one of them complete, and no code path crossing between them until a
/// format arrived that needed both.
///
/// **It belongs on the TEXTURE, not on the sampler, and the hardware settles
/// it.** In SDL_GPU the decode is declared by the texture's FORMAT and performed
/// by the sampler; the same image bound twice cannot be sRGB in one binding and
/// linear in the other, because the format is a property of the memory. Lesson
/// 3.9 built these types to mirror SDL_GPU's "so that when Module 4 replaces
/// this file with a GPU sampler object, the change is a rename and not a
/// re-education", and mirroring includes mirroring where a decision lives.
enum class texel_space
{
    /// **Colour.** Authored, sRGB-encoded, decoded through the transfer function
    /// on every read — which is what `sample` has always done and what makes an
    /// albedo a reflectance that can multiply a quantity of light (1.6, 3.9).
    ///
    /// The default, because it is the only kind of texture that existed before
    /// this lesson and because getting it wrong on a colour is the *visible*
    /// mistake — a washed-out albedo announces itself.
    srgb,

    /// **Data.** A normal, a roughness, a metalness, a mask, an occlusion factor.
    /// The byte is a number, `value / 255`, and no curve is applied.
    ///
    /// Getting THIS one wrong is the invisible mistake, which is why the two are
    /// worth separating rather than documenting. A normal map read through an
    /// sRGB decode is wrong by the gamma curve everywhere except 0 and 1 — the
    /// flat value 0.5 comes back as **0.2140** — so every surface tilts toward
    /// its own steepest reading and the result looks like a normal map that was
    /// authored too strong. Lesson 6.7 §3 measures it.
    linear
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
    /// How to combine texels WITHIN a level. One of SDL's three; the other two
    /// arrived in Lesson 6.10, below.
    filter texel_filter = filter::linear;

    /// **How to filter BETWEEN mip levels.** Lesson 6.10.
    ///
    /// The second of the three fields 3.9 collapsed into one, and 3.9's comment
    /// said exactly why it could: "without mipmaps there is nothing different
    /// for a minification filter to do". There is now. `nearest` snaps to the
    /// closer level and draws a visible line across a floor where the level
    /// changes; `linear` blends the two straddling levels — trilinear — for the
    /// cost of a second bilinear fetch.
    ///
    /// Ignored when sampling a bare `texture`, which is every call site written
    /// before 6.10.
    filter mip_filter = filter::linear;

    /// Nudge the chosen level. Negative sharpens (and re-introduces the
    /// aliasing), positive blurs. It is the one knob artists actually reach for,
    /// and `SDL_GPUSamplerCreateInfo::mip_lod_bias` has it — a sampler that
    /// could not express it would not survive the port.
    float mip_bias = 0.0f;

    /// **Refuse to choose between blurring and aliasing.** 1 is isotropic.
    ///
    /// A grazing floor has a footprint long in one direction and short in the
    /// other, and one square average cannot represent that: choose the level by
    /// the long axis and the short one is blurred away; choose it by the short
    /// axis and the long one aliases. Above 1 this takes several samples ALONG
    /// the long axis at the level the SHORT axis asked for, which buys the
    /// detail back at a linear cost. Clamped to 16, as hardware is.
    int max_anisotropy = 1;

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
    ///
    /// **`space` defaults to `srgb`** (Lesson 6.7), which keeps every texture
    /// written before that lesson meaning exactly what it meant. A default that
    /// changed the answer would have made the whole lesson a re-baseline.
    ///
    /// **`storage` defaults to `straight`** (Lesson 6.11), for exactly the reason
    /// `space` defaults to `srgb`: a default that changed the answer would make
    /// the lesson a re-baseline of every picture in the course rather than an
    /// addition to it.
    texture(int w, int h, Uint32 fill = 0xFF000000u, texel_space space = texel_space::srgb,
            alpha_storage storage = alpha_storage::straight);

    /// Colour or data? See `texel_space`. Set at construction and never after:
    /// an image does not stop being a normal map halfway through a frame, and
    /// making it settable would put the decision back at the call site, which is
    /// exactly where this lesson took it from.
    [[nodiscard]] texel_space space() const { return space_; }

    /// Is the colour already multiplied by the alpha? See `alpha_storage`.
    /// Lesson 6.11.
    ///
    /// **The second property of this kind, and the pair is now a pattern worth
    /// naming**: both `space()` and `storage()` describe what the BYTES MEAN, both
    /// are fixed at construction, and both exist so that the one function every
    /// read goes through — `fetch` — can ask instead of every call site
    /// remembering. `build_mips` reads them for the same reason and says so.
    [[nodiscard]] alpha_storage storage() const { return storage_; }

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
    texel_space space_ = texel_space::srgb;   ///< 6.7
    alpha_storage storage_ = alpha_storage::straight;   ///< 6.11
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

// ---- Sampling, with the alpha kept (Lesson 6.11) -----------------------------

/// The same sample, **carrying the coverage out with the colour**.
///
/// `sample` above has been throwing the alpha byte away since Lesson 3.9, and
/// it was right to: `linear_rgb` has three channels, alpha is not one of them
/// (`colour.hpp`: *"alpha is a coverage fraction rather than a quantity of
/// light... carry it separately if you need it"*), and until this lesson nothing
/// in the engine had a use for it. `fetch`'s own comment said so in as many
/// words. Transparency is the use.
///
/// **`sample` is now this function with the fourth number dropped**, rather than
/// a second implementation of the same arithmetic — two copies of a filtering
/// rule are two rules, and the day they disagree is the day a textured surface
/// changes colour when somebody makes it transparent. The colour channels are
/// bit-identical to what 3.9 returned, which is not a hope: `verify_611` §A
/// asserts it texel by texel, and the reference render is byte-identical for the
/// twentieth lesson running.
///
/// **The alpha is filtered, not point-sampled**, and with the same weights as
/// the colour. A bilinear alpha is what makes a cutout edge land between texels
/// instead of on one, and it is why `alpha_storage::premultiplied` matters: with
/// straight alpha, filtering the colour and the coverage independently drags the
/// colour of invisible texels into the visible ones.
///
/// **No transfer function is applied to it, ever**, in either `texel_space`. A
/// coverage fraction is already linear in the only thing it measures.
[[nodiscard]] texel_sample sample_rgba(const texture& image, const sampler& samp,
                                       float u, float v);

/// `sample_rgba` with the `vec2` a mesh actually stores.
[[nodiscard]] inline texel_sample sample_rgba(const texture& image, const sampler& samp,
                                              vec2 uv)
{
    return sample_rgba(image, samp, uv.x, uv.y);
}

[[nodiscard]] texel_sample sample_nearest_rgba(const texture& image, const sampler& samp,
                                               float u, float v);
[[nodiscard]] texel_sample sample_bilinear_rgba(const texture& image, const sampler& samp,
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
/// Lesson 6.10. Defined in `mipmap.hpp`, which includes this file.
class mip_chain;

struct texture_binding
{
    const texture* image = nullptr;
    sampler samp{};

    /// An optional mip chain for `image`. Lesson 6.10.
    ///
    /// **Nullable, and null is the whole of "no mipmapping"** — the same bargain
    /// `lights`, `albedo` and 6.9's `cascades` already make. When it is null the
    /// fill samples `image` exactly as it did in 3.9, which is why nineteen
    /// lessons of reference renders are still byte-identical.
    ///
    /// A forward declaration rather than an include: `mipmap.hpp` includes THIS
    /// header, so the dependency has to point one way.
    const mip_chain* mips = nullptr;

    /// Is a chain bound AND wanted? A chain plus `max_anisotropy` and the mip
    /// filters live in `samp`, so this asks only about the data.
    [[nodiscard]] bool mipped() const { return mips != nullptr; }

    /// Is there anything to sample? A binding with no image is not an error — it
    /// is a pipeline that was never given one, and the fill falls back to vertex
    /// colours.
    [[nodiscard]] bool bound() const { return image != nullptr && !image->empty(); }
};

// ---- Lesson 6.5: storing textures, as opposed to pointing at them -----------

/// A reference to a texture held in a `texture_pool`.
///
/// **The same argument `image_handle` made in Lesson 5.5, arriving one layer
/// further down.** `image_data` is what was *loaded*; `texture` is what was
/// *derived* from it and is ready to sample. Both want stable, checkable
/// references, and for the same reason: a raw `const texture*` stored in a
/// material outlives nothing in particular, and the day the pool reallocates is
/// the day every material in the scene points at freed memory.
using texture_handle = handle<texture>;

/// Storage for the textures a scene samples.
///
/// **Lesson 6.5 kept this out of `asset_store` and Lesson 6.6 put it in.** The
/// reasoning 6.5 gave was that the store is about *files* — a search path, a
/// name, a cache, a load count (5.5) — and every texture in the engine at that
/// point was generated in memory. glTF is what expired that: a material now
/// arrives naming an image on disk, two materials routinely name the same one,
/// and "load it once and hand out the same reference" is precisely the store's
/// job description.
///
/// The pool itself did not move or change. What the store adds around it is the
/// three things 5.5 named: a name -> handle map, an unload, and — because a
/// texture is MADE FROM an image rather than loaded directly — a derivation
/// edge, so that unloading the image unloads the textures built from it. See
/// `asset_store::load_texture`.
using texture_pool = pool<texture>;

// ---- Lesson 6.6: the bridge that had never been built -----------------------

/// Turn decoded file pixels into something `sample` can read.
///
/// **This function should have existed since Lesson 5.3 and did not**, and the
/// reason it did not is worth more than the twelve lines it takes. The engine
/// has been able to decode a PNG since 5.3 (`load_image` -> `image_data`) and to
/// sample a texture since 3.9 (`texture` -> `sample`), and nothing ever joined
/// them — because every CPU texture was *generated* (`make_checker`,
/// `make_uv_grid`) and every *loaded* image went straight to the GPU as bytes
/// (`gpu_texture::create_sampled`). Two complete halves with no middle, and no
/// test could see the gap because no code path crossed it.
///
/// glTF is what forces the join: a material names an image file, and the
/// software renderer has to be able to sample it. Lesson 6.6 §5.
///
/// **It is a channel shuffle, not a memcpy**, and that is the part to get right.
/// `image_data::pixels` is R, G, B, A in that byte order, as stb hands it over.
/// `texture` stores `Uint32` in ARGB8888 — the framebuffer's own format (1.5) —
/// which on a little-endian machine is the bytes B, G, R, A. Copying the buffer
/// wholesale therefore swaps red and blue, which is the single most recognisable
/// wrong-looking texture there is and the reason `pack_argb` exists rather than
/// a cast.
///
/// An invalid image gives an empty texture, which `sample` already answers with
/// debug magenta — a visible failure rather than a silent black one.
///
/// **`space` is not optional in spirit even though it has a default** (Lesson
/// 6.7). An importer that does not say what an image holds is guessing, and the
/// guess is right for albedos and silently wrong for everything else — which is
/// why `asset_store::load_texture` takes the space as a parameter and glTF
/// supplies it from which slot the texture was bound to.
[[nodiscard]] texture to_texture(const image_data& src,
                                 texel_space space = texel_space::srgb);

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

/// An **egg-carton normal map**: a grid of `cells` x `cells` smooth bumps.
///
/// Lesson 6.7's test image, and it is generated rather than painted for the
/// reason Lesson 3.9 generated its checkerboard and 6.6 generated its glTF —
/// the course ships no third-party assets, and every number a lesson quotes has
/// to be reproducible from the repository alone.
///
/// **The height field is analytic**, which is what makes it a test rather than a
/// picture:
///
///     k = 2*pi*cells,   h(u, v) = (strength/k) * cos(k*u) * cos(k*v)
///
/// so the surface normal at any point is the normalised
/// `(-dh/du, -dh/dv, 1)`, and `verify_67` computes that by hand and compares.
/// A painted map can only be checked by looking at it.
///
/// **`strength` is the maximum SLOPE, not the amplitude**, which is why the
/// `1/k` is in there. `A cos(ku)cos(kv)` has a peak gradient of `A k`, so a
/// fixed amplitude means the steepness changes when the cell count does — at six
/// cells an amplitude of 1 is a surface tilted 88 degrees everywhere, and the
/// picture is a dark mess that reads as a shading bug. Dividing it out makes
/// **1.0 a maximum tilt of 45 degrees** at any cell count, which is a number
/// somebody can author against.
///
/// **`strength = 0` gives the flat map** — every texel `(0.5, 0.5, 1.0)`, the
/// lavender that means "no change" — which is the identity input for the round
/// trip in §D: perturb by a flat map and the geometric normal must come back
/// unchanged to float precision.
///
/// The result carries `texel_space::linear`, because it is a direction and not a
/// colour. That is not a detail this function could sensibly leave to the
/// caller: an image whose meaning is fixed by the function that made it should
/// arrive knowing what it is.
///
/// **Green points along +v, which is DOWN the image** (Lesson 3.9's origin, and
/// glTF's). A map baked for the opposite convention has its green channel
/// inverted, and the symptom is that every bump reads as a dent — see §7.
[[nodiscard]] texture make_normal_bumps(int size, int cells, float strength = 1.0f);

} // namespace engine
