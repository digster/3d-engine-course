// engine/include/engine/gfx/mipmap.hpp — the texture, at every size at once.
//
// Lesson 6.10, and this file is a debt being paid. Lesson 3.9 measured the
// problem precisely and then deferred it, in as many words: one screen pixel
// covers **0.60 texels** at the bottom of the frame and **62.46** two rows below
// the horizon, the sampler reads four texels whatever the pixel covers, and past
// the crossover it is looking at a shrinking fraction of the truth — about 6.4%
// of it at the horizon. That gap is the sparkle on the floor, and 3.9 named the
// fix and moved on. So did 4.7, which created every texture with
// `num_levels = 1` and a comment saying Module 6 would change it. So did
// `texture.hpp`'s own sampler struct, whose doc comment still reads "Module 6's
// mipmaps are what fills the gap".
//
// THE IDEA IS ONE SENTENCE. If a pixel covers sixty texels, do not read four of
// them and hope — read from a copy of the image in which those sixty have
// ALREADY been averaged. Precompute the image at half size, quarter size, and so
// on down to one texel, and choose the level whose texels are about the size of
// the pixel's footprint.
//
// WHAT MAKES IT A LESSON IS THE THREE QUESTIONS THAT SENTENCE HIDES:
//
//   1 WHICH LEVEL? The footprint is a derivative — how fast uv changes per
//     screen pixel — and on the GPU `ddx`/`ddy` hand it to you because fragments
//     are shaded in 2x2 quads. THE CPU HAS NO NEIGHBOURING FRAGMENT. It has to
//     compute the derivative analytically from the triangle's own gradients,
//     which is `uv_gradients` below and is genuinely the most interesting
//     arithmetic in the file. Lesson 3.9's Exercise 8.4 asked for exactly this
//     and said it was "Module 6's job properly".
//
//   2 WHAT ABOUT BETWEEN LEVELS? Level 3 and level 4 differ by a factor of two,
//     so switching between them abruptly draws a visible line across the floor
//     where the level changes — the same shape of artefact Lesson 6.9 met as the
//     cascade seam, and cured the same way: blend across the boundary.
//     `filter::linear` on `mip_filter` is trilinear.
//
//   3 WHAT IF THE FOOTPRINT IS NOT SQUARE? A floor at a grazing angle has a
//     footprint that is long in one direction and short in the other. One
//     square average cannot represent that, so choosing a level by the LONG axis
//     over-blurs and by the SHORT axis still aliases. That is anisotropy, and
//     `max_anisotropy` takes several samples along the long axis instead.
//
// THE TRAP, AND IT IS FAMOUS: A CHAIN BUILT BY AVERAGING sRGB BYTES GETS DARKER
// AT EVERY LEVEL. Averaging is a linear operation and sRGB is not a linear
// encoding, so the mean of two codes is not the code of the mean light.
// `build_mips` decodes, averages in linear light, and re-encodes — which is what
// Lesson 6.1 built the whole vocabulary for, and 3.9's Exercise 8.5 hint already
// warned about. §3.4 of the lesson measures the error: 21.7% too dark at level 1
// on a black-and-white checker, compounding down the chain.
//
// WHAT THIS FILE DOES NOT DO:
//
//   - **No non-power-of-two smarts.** Odd sizes round down, which loses a row.
//     Real engines box-filter with weights; the difference is invisible and the
//     code is twice as long.
//   - **No mip generation on the GPU.** SDL does that
//     (`SDL_GenerateMipmapsForGPUTexture`), and §5 says what it costs and what
//     it demands of the texture's usage flags.
//   - **The chain is OPT-IN.** `texture` is untouched and a texture without a
//     chain samples exactly as it did in 3.9. That is deliberate: a chain costs
//     33% more memory, a 1x1 fallback has nothing to average, and — the honest
//     reason — building one for every texture would move the reference render,
//     which has been byte-identical for eighteen lessons.

#ifndef ENGINE_GFX_MIPMAP_HPP
#define ENGINE_GFX_MIPMAP_HPP

#include <span>
#include <vector>

#include <engine/gfx/colour.hpp>
#include <engine/gfx/texture.hpp>
#include <engine/math/vec2.hpp>

namespace engine
{

/// The most levels a chain will ever hold: 2^16 is 65536, wider than any texture
/// this engine loads, and the array is small enough to make a cap cheaper than a
/// growth policy.
inline constexpr int k_max_mip_levels = 17;

/// How fast uv moves per screen pixel, at one fragment.
///
/// **This is the whole input to level selection**, and its two vectors are not
/// interchangeable: `du_dx` is how far the sample point travels across the
/// texture when the pixel moves one step in screen *x*, and `du_dy` the same for
/// screen *y*. A footprint that is long in one and short in the other is exactly
/// the anisotropic case.
///
/// Both are in TEXTURE units (0..1), not texels — multiplying by the level-0
/// size happens in `mip_level_for`, once, where the size is known.
struct uv_footprint
{
    vec2 d_dx{0.0f, 0.0f};
    vec2 d_dy{0.0f, 0.0f};
};

/// The analytic screen-space uv gradients of a perspective-correct triangle.
///
/// **The CPU's replacement for `ddx`/`ddy`, and it is exact rather than a finite
/// difference.** A GPU shades fragments in 2x2 quads specifically so that a
/// neighbour is always available to subtract; a scanline rasterizer has no such
/// neighbour, so the derivative has to come from the triangle itself.
///
/// The derivation, which the lesson does properly in §3.2. Perspective-correct
/// interpolation computes
///
///     u(x, y) = U(x, y) / W(x, y)
///
/// where `U = sum(f_i * u_i/w_i)` and `W = sum(f_i / w_i)` are both **affine** in
/// screen space, because the normalised barycentrics `f_i` are. So the quotient
/// rule applies and gives, per axis,
///
///     du/dx = (dU/dx - u * dW/dx) / W
///
/// with `dU/dx = sum(df_i/dx * u_i/w_i)` and `df_i/dx = step_x_i / area`, both of
/// which the rasterizer already computes for its edge functions. Nothing new is
/// evaluated per pixel except two multiplies and a subtract.
///
/// @param du_dx_num  `dU/dx`, `dV/dx` — the numerator's screen-x gradient.
/// @param du_dy_num  the same for screen y.
/// @param dw_dx      `dW/dx`, the interpolated 1/w's screen-x gradient.
/// @param dw_dy      the same for screen y.
/// @param uv         the uv at this fragment, already divided.
/// @param w_recip    `1/W` at this fragment — the reciprocal the fill loop
///                   already computed, reused rather than recomputed.
[[nodiscard]] inline uv_footprint uv_gradients(vec2 du_dx_num, vec2 du_dy_num,
                                               float dw_dx, float dw_dy,
                                               vec2 uv, float w_recip)
{
    uv_footprint out;
    out.d_dx = vec2{(du_dx_num.x - uv.x * dw_dx) * w_recip,
                    (du_dx_num.y - uv.y * dw_dx) * w_recip};
    out.d_dy = vec2{(du_dy_num.x - uv.x * dw_dy) * w_recip,
                    (du_dy_num.y - uv.y * dw_dy) * w_recip};
    return out;
}

/// A texture and every halving of it, down to 1x1.
///
/// Level 0 is a full copy of the source rather than a reference, so a chain owns
/// its pixels and cannot dangle. That costs one image; the chain costs 1/3 of one
/// more (1/4 + 1/16 + 1/64 + ... = 1/3), which is the famous **33%**.
class mip_chain
{
public:
    mip_chain() = default;

    [[nodiscard]] bool empty() const { return levels_.empty(); }

    /// How many levels. `1 + floor(log2(max(w, h)))` for a built chain.
    [[nodiscard]] int levels() const { return static_cast<int>(levels_.size()); }

    /// Level `i`, clamped into range. Level 0 is the original size.
    [[nodiscard]] const texture& level(int i) const;

    [[nodiscard]] int width() const { return levels_.empty() ? 0 : levels_.front().width(); }
    [[nodiscard]] int height() const { return levels_.empty() ? 0 : levels_.front().height(); }

    /// Total texels across every level, for the memory claim in §5.
    [[nodiscard]] std::size_t texels() const;

    friend mip_chain build_mips(const texture& base);

private:
    std::vector<texture> levels_;
};

/// Build the chain by repeated 2x2 averaging.
///
/// **Averaged in LINEAR LIGHT when the source is sRGB-encoded**, which is the
/// whole correctness story of this function. `texel_space` already records which
/// a texture is (Lesson 6.7 gave it that field so a normal map would stop being
/// decoded as colour), so the decision is read from the data rather than passed
/// in — a caller who has to remember is a caller who will forget on the third
/// texture.
///
/// The failure mode if you skip it is not subtle once you know to look: every
/// level is darker than the one above, so a surface *gets darker as it recedes*,
/// and the usual diagnosis is "the lighting falls off too fast".
[[nodiscard]] mip_chain build_mips(const texture& base);

/// Which level has texels about the size of this footprint, as a CONTINUOUS
/// value — 2.4 means "level 2, four tenths of the way to level 3".
///
/// The derivation, in one line: a level halves the texture's size, so level `L`
/// has texels `2^L` times as wide. We want the level whose texel matches the
/// footprint, so
///
///     2^L = footprint_in_texels   =>   L = log2(footprint_in_texels)
///
/// **The footprint is the LONGER of the two axes**, and that choice is the whole
/// of why isotropic mipmapping over-blurs: taking the longer axis guarantees no
/// aliasing along either, at the cost of blurring the short one. Taking the
/// shorter would keep the detail and alias. Anisotropic filtering is what
/// refuses the choice.
[[nodiscard]] float mip_level_for(const uv_footprint& fp, int width, int height);

/// Sample a chain, choosing and blending levels per `samp`.
///
/// - `mip_filter == nearest` picks the rounded level: cheap, and it draws a
///   visible line on the floor where the level changes.
/// - `mip_filter == linear` blends the two straddling levels — trilinear — for
///   the cost of a second bilinear fetch.
/// - `max_anisotropy > 1` takes up to that many samples along the footprint's
///   long axis, at the level chosen by the SHORT axis.
///
/// With an empty chain this returns black rather than guessing; callers hold a
/// chain only when they built one.
[[nodiscard]] linear_rgb sample_mipped(const mip_chain& chain, const sampler& samp,
                                       vec2 uv, const uv_footprint& fp);

} // namespace engine

#endif // ENGINE_GFX_MIPMAP_HPP
