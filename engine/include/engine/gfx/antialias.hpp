// engine/include/engine/gfx/antialias.hpp — two problems that share a name.
//
// Lesson 6.14, and the first job is to separate them, because they have different
// causes, different cures, and the most popular cure fixes exactly one of them.
//
//   GEOMETRIC ALIASING is the staircase on a silhouette. What is undersampled is
//       COVERAGE — the question "how much of this pixel does the triangle cover?"
//       answered with a yes or a no.
//   SHADING ALIASING is the sparkle on a shiny curved surface. What is
//       undersampled is the LIGHTING — a specular lobe narrower than the pixel
//       that is looking for it.
//
// MSAA fixes the first and does **nothing at all** for the second, by
// construction: it multisamples coverage and shades ONCE per primitive per pixel.
// That is not a defect in MSAA, it is the optimisation that makes MSAA affordable,
// and knowing it is the difference between choosing a technique and copying one.
//
// ---------------------------------------------------------------------------
// WHAT ANTIALIASING ACTUALLY IS, WHICH IS NOT WHAT IT SOUNDS LIKE
// ---------------------------------------------------------------------------
//
// It is tempting to think of antialiasing as "getting a more accurate answer".
// It is not, and the measurement in `verify_614` §E is unusually blunt about it.
// Filtering the NDF at roughness 0.05 improves the per-pixel RMS error against
// brute-force ground truth by only **2.5x** — and at roughness 0.20 it makes that
// error slightly *worse*. Meanwhile it reduces the frame-to-frame swing from
// **3,996x to 3.2x**.
//
// So:
//
//     ANTIALIASING DOES NOT MAKE A PIXEL CORRECT. IT MAKES IT STABLE.
//
// The artefact was never "this pixel has the wrong value". It was "this pixel
// changes violently when nothing in the scene did". Aliasing is high-frequency
// content masquerading as low-frequency content, and the only cure is to REMOVE
// frequencies you cannot represent — which is a loss of information you accept,
// because the alternative is a lie that moves.
//
// Both halves of this file are that same operation:
//
//   * Supersampling prefilters COVERAGE by integrating over the pixel's area.
//   * `filtered_roughness` prefilters the NDF by widening it until the lobe is no
//     narrower than the normal variation the pixel has to integrate over.
//
// Both throw detail away. Both are correct.
//
// ---------------------------------------------------------------------------
// AND ITS RELATIONSHIP TO LESSON 6.13, WHICH IS EXACT
// ---------------------------------------------------------------------------
//
// 6.13 had more RANGE than the display could carry, and spent AREA to encode it.
// This lesson has more DETAIL than the sample grid can carry, and spends
// SHARPNESS to buy stability. In both, the pixel grid is the thing that cannot be
// enlarged, and in both the fix is to convert the surplus into a currency the
// grid does have.

#pragma once

#include <engine/gfx/colour.hpp>
#include <engine/gfx/microfacet.hpp>
#include <engine/math/vec3.hpp>

namespace engine {

class framebuffer;

// ---- Geometric: supersampling -----------------------------------------------

/// Everything the antialiasing needs, gathered — the bargain `bloom_settings` and
/// `tonemap_settings` both made.
struct aa_settings
{
    /// Linear supersampling factor. 1 is off; 2 renders at 2x2 = 4 samples per
    /// pixel, 4 at 4x4 = 16.
    ///
    /// **Off by default**, the fifth time this course has made that call (6.7's
    /// `texel_space`, 6.10's opt-in chain, 6.11's `mip_options`, 6.13's bloom,
    /// this): a new capability is a new path, so every prior measurement and the
    /// reference render survive it untouched.
    int factor = 1;

    /// Widen the NDF to match the normal variation across a pixel — the SHADING
    /// half, which supersampling addresses only by brute force and MSAA not at
    /// all. See `filtered_roughness`.
    bool specular = false;
};

/// Samples per pixel for a given linear factor. `factor` 2 means 4 samples.
[[nodiscard]] constexpr int aa_samples(int factor)
{
    return (factor < 1) ? 1 : factor * factor;
}

/// Box-filter a supersampled framebuffer down to its final size.
///
/// **In LINEAR LIGHT, and this is the whole correctness story of the function.**
/// A `framebuffer` holds sRGB-encoded bytes, and averaging those bytes is Lesson
/// 6.1's mistake wearing its fourth costume — after 6.10's mip chains and 6.11's
/// compositing. The measurement is the same shape every time: averaging an edge
/// between black and white in encoded bytes gives code 128, and in linear light
/// gives code **188**, which is 42.9% of the light delivered instead of 100%.
///
/// The symptom is specific and often misdiagnosed: antialiased edges come out
/// **too dark**, so the image looks like it has a thin dark outline around every
/// silhouette, and the usual guess is that the edges are being blended with the
/// background twice.
///
/// @param hi  the supersampled source; must be `factor` times `lo` in each axis.
/// @param lo  the destination. A size mismatch is a no-op rather than a partial
///            write, for the reason `resolve` gives: half a resolved frame is
///            worse than none.
/// @param encoded_average  average the stored BYTES instead — the bug, kept
///            selectable because a failure you can turn on is worth more than one
///            you have to describe. The same argument `tonemap::clamp` and
///            `blend_space::encoded` make.
void resolve_supersampled(const framebuffer& hi, framebuffer& lo, int factor,
                          bool encoded_average = false);

// ---- Shading: filtering the normal distribution ------------------------------

/// The screen-space filter width, in the convention of Tokuyoshi & Kaplanyan
/// (2019). **Quoted, not derived** — it is a choice of how wide a pixel's
/// reconstruction filter is taken to be, and 1/(2*pi) is theirs.
inline constexpr float k_specular_aa_sigma2 = 0.15915494f;

/// The ceiling on how much roughness may be added. **Also quoted, also a fit.**
///
/// Without it, a surface seen edge-on — where the normal swings through most of a
/// hemisphere inside one pixel — would be filtered to fully rough, and a chrome
/// bumper would go matte at its own silhouette. 0.18 is the paper's value and is
/// the sort of number an artist should be allowed to change.
inline constexpr float k_specular_aa_kappa = 0.18f;

/// Widen `alpha^2` to cover the normal variation across one pixel.
///
/// **THE DERIVABLE CORE IS THAT VARIANCES ADD.** A pixel does not see one normal;
/// it sees a distribution of them, spread across its footprint. What the pixel
/// therefore integrates is the surface's NDF **convolved** with that spread — and
/// convolution adds variances. GGX's `alpha` behaves (approximately) as a standard
/// deviation in the half-vector domain, so the filtered surface is
///
///     alpha'^2 = alpha^2 + 2 * sigma^2
///
/// with `sigma^2` the variance of the normal within the pixel, estimated from its
/// screen-space derivatives. That is the whole idea, and it is the same idea as a
/// mip chain: prefilter the thing being undersampled, at the frequency you can
/// actually represent.
///
/// **WHAT IS DERIVED AND WHAT IS NOT, stated plainly.** The convolution argument
/// is real. The factor of 2, the `sigma^2` above, and the `kappa` clamp are from
/// Tokuyoshi & Kaplanyan's paper and are fits — GGX's variance is in fact
/// *infinite* (its tails are heavy enough that the second moment diverges), so
/// "alpha is a standard deviation" is an approximation doing real work here. This
/// course marks that rather than hiding it, and then measures whether it helps.
///
/// **IT DOES, BUT NOT IN THE WAY YOU WOULD EXPECT.** See this file's header: the
/// per-pixel accuracy barely moves, and the stability improves by three orders of
/// magnitude. That is the correct outcome and it is what antialiasing is for.
///
/// @param dndx  d(normal)/d(screen x), a world-space vector. On the GPU this is
///              `ddx(n)`; on the CPU it comes from the triangle, the same way
///              Lesson 6.10's `uv_gradients` does, because a scanline rasterizer
///              has no neighbouring fragment to subtract.
[[nodiscard]] float filtered_alpha2(float alpha2, vec3 dndx, vec3 dndy);

/// `filtered_alpha2`, expressed in the perceptual roughness the rest of the
/// engine speaks.
///
/// Note the double round trip — roughness to alpha, filter, alpha back to
/// roughness — which is deliberate: the filtering is a statement about the NDF's
/// actual width, and `roughness` is a remapping chosen to make a slider feel
/// linear (6.3). Filtering the slider instead of the quantity would be filtering
/// the wrong thing, and the error would grow with how nonlinear the remap is.
[[nodiscard]] float filtered_roughness(float roughness, vec3 dndx, vec3 dndy);

/// How much of a pixel's normal variation a lobe can absorb, as a ratio.
///
/// Above 1 the lobe is wider than the variation and the highlight is resolved;
/// below 1 it can fall between sample points entirely, which is a **sparkle**
/// rather than a staircase and is why no amount of coverage sampling helps.
///
/// `verify_614` §B bisects the crossover on a sphere 40 px in radius and gets
/// **roughness 0.2468** — which places the demo's own 0.49 safely above it, and
/// places every polished material Lesson 6.12 introduced safely below.
[[nodiscard]] float lobe_to_variation_ratio(float roughness, float normal_turn_per_pixel);

} // namespace engine
