// engine/include/engine/gfx/bloom.hpp — making a value the display cannot show
// into an area it can.
//
// Lesson 6.13, and the honest way in is to say what Lesson 6.12 did NOT do.
//
// 6.12 gave the engine a float target, so a specular highlight of 55,917 now
// survives rasterization instead of being flattened to code 255 by `to_encoded`.
// That was real. But the value still has to arrive at a display with 256 codes,
// and `scratch/probe_613.cpp` measures exactly where the new lid is — the
// smallest linear value that STILL resolves to code 255, per operator:
//
//     clamp             0.9955      15.78 stops above it, all one code
//     reinhard        223.4789       7.97
//     reinhard_white    3.9556      13.79     (white point 4)
//     aces              6.3774      13.10
//
// So tonemapping MOVED the clipping point; it did not remove it. Under ACES the
// engine's polished metal still spends 13.1 stops of its range on a single code.
// And there is no curve that fixes this, because the constraint is not the
// curve: an 8-bit sRGB display spans code 1 to code 255, which is linear
// 0.00030353 to 1.0, or **11.69 stops, total**. The scene has more range than
// that. Something has to give.
//
// ---------------------------------------------------------------------------
// WHAT GIVES: INTENSITY BECOMES AREA
// ---------------------------------------------------------------------------
//
// A real camera does not clip a bright light to a neat white dot either. It
// BLEEDS — light scatters in the lens elements, off the aperture blades and
// inside the sensor stack, so a point of light lands not as a point but as a
// spike with very wide skirts. Your own eye does it too, in the cornea and the
// vitreous; it is why a streetlight in fog has a halo when there is no fog.
//
// That scattering is a point spread function, and the useful consequence is
// this: the ENERGY in the skirts is proportional to the source's luminance, but
// the skirts are spread over an area, so they land BELOW the lid even when the
// source is far above it. The display cannot encode 55,917 in a pixel's
// intensity. It can encode it in how many pixels are lit.
//
// And that is not a metaphor — it is a prediction with an exponent, and §4 of
// the lesson tests it. If the glow falls off as `r^-2` (which is roughly what
// measured glare does, and what this file's pyramid produces — see below), then
// the radius at which it crosses any fixed visibility threshold goes as sqrt(L),
// so the AREA goes as L. Measured on a delta, over three decades:
//
//     L          glow area      area/L      radius/sqrt(L)
//        100          115        1.150          0.605
//      1,000        1,238        1.238          0.628
//     10,000       11,399        1.140          0.602
//
// Area linear in luminance to within 8%. The display converts an intensity it
// cannot represent into an area it can, and the conversion is close to exact.
//
// ---------------------------------------------------------------------------
// WHY A PYRAMID, WHICH IS THE PART USUALLY WAVED THROUGH
// ---------------------------------------------------------------------------
//
// The standard justification is cost, and the cost argument is real: one
// separable Gaussian wide enough to matter (sigma 64 px) is 385 taps per axis
// and 399 million texel fetches for a 960x540 frame, where the whole pyramid
// below costs **0.666 of a single full-screen pass**.
//
// But cost is the less interesting half. A single Gaussian is also the WRONG
// SHAPE. `exp(-r^2)` has no tails worth the name — over the octave from r = 32
// to r = 64 a sigma-16 Gaussian falls by **403x**, while the pyramid falls by
// **6.0x**, because the pyramid is a sum of Gaussians whose widths DOUBLE, and a
// geometric sum of Gaussians has an approximately power-law envelope. Measured
// log-log slope of this file's output: -1.86, -2.01, -2.19 over successive
// octaves — that is the `r^-2` the prediction above needs.
//
// So the pyramid is not a cheap approximation to the thing we wanted. It is
// cheaper AND closer to the physics. Those rarely coincide and it is worth
// noticing when they do.
//
// ---------------------------------------------------------------------------
// THE NINETY-PERCENT PICTURE
// ---------------------------------------------------------------------------
//
// Three simplifications, named rather than hidden:
//
//   * THE THRESHOLD IS NOT PHYSICAL. Real scatter applies to all light, not just
//     to light above a cutoff; a bright pass exists to stop the whole image from
//     hazing over and to save the bandwidth of blooming pixels that contribute
//     nothing visible. The cost is that bloom appears and disappears at a
//     brightness boundary that has no counterpart in optics. UE4's later
//     "convolution bloom" drops the threshold entirely and pays for it.
//   * THE PSF IS ISOTROPIC. A real lens has streaks from the aperture blades and
//     chromatic separation in the skirts; ours is radially symmetric and grey.
//   * THE KERNEL IS FIXED IN SCREEN SPACE, so the glow is the same size in
//     pixels at any resolution. Shipping engines usually scale `radius` with
//     resolution for this reason. Exercise 4.

#pragma once

#include <engine/gfx/colour.hpp>
#include <engine/gfx/hdr.hpp>

#include <cstddef>
#include <vector>

namespace engine {

/// The most pyramid levels anything here will build.
///
/// Six levels from half resolution reaches 2^6 = 64 half-res texels, which is
/// 128 full-resolution pixels of glow radius — about a quarter of a 540-line
/// frame, and past the point where more levels buy anything but a slower frame.
/// It is also where the measured power law ENDS: beyond the pyramid's reach
/// there is no level left to contribute, so the tail does not decay, it stops.
inline constexpr int k_max_bloom_levels = 8;

/// Everything the bloom needs, gathered — the bargain `tonemap_settings` made.
struct bloom_settings
{
    /// Off by default, and that default is load-bearing: it is what keeps the
    /// twenty-first-lesson golden and every measurement before it intact. The
    /// same call `mip_options` made in 6.11 and `texel_space` in 6.7.
    bool enabled = false;

    /// Luminance above which a pixel contributes, **measured after exposure**.
    ///
    /// After, and not before, and the reason is that the threshold's job is to
    /// separate "will look bright on the display" from "will not" — which is a
    /// question about the final image, and the exposure is what decides it. Put
    /// the threshold in scene-referred units instead and a dim scene viewed at a
    /// high exposure looks bright and blooms nothing, while the same geometry
    /// under a bright sun blooms everywhere. The bug reads as "the bloom turns
    /// itself off indoors".
    float threshold = 1.0f;

    /// Half-width of the soft transition around `threshold`. Zero is a hard cut.
    ///
    /// **A hard threshold makes a pixel oscillating around it flicker**, because
    /// the bright pass's derivative jumps from 0 to 1 at the cut: a pixel
    /// crossing from 0.999 to 1.001 goes from contributing nothing to
    /// contributing its full excess, and the boundary it crosses is a contour in
    /// the image that moves with the camera. The result is a crawling edge along
    /// every gradient that happens to pass through the threshold.
    ///
    /// The knee replaces the corner with a quadratic that matches both the value
    /// AND the slope at each join — derived in §3.6, checked numerically in
    /// `verify_613` §B. See `bright_pass_weight`.
    float knee = 0.5f;

    /// How much of the blurred result is added back. See `compute_bloom` for why
    /// this number is not "the fraction of the image that is bloom".
    float intensity = 0.04f;

    /// Ceiling applied to the bright pass, in exposure-corrected linear light.
    ///
    /// **The firefly control, and the measurement that justifies it is stark.**
    /// Drop one pixel of 6.12's polished metal (55,917) into an otherwise
    /// ordinary 256x256 frame over the threshold — 0.00153% of the pixels — and
    /// it contributes **62.8%** of the finished bloom's entire energy. It is one
    /// sub-pixel highlight, so it appears and vanishes as the camera moves by a
    /// pixel, and nearly two thirds of the glow in the frame blinks with it.
    ///
    /// A clamp at 100 removes **99.3%** of that contribution and leaves the honest
    /// bloom within **1.19%** of where it was. That is an unusually cheap fix, and
    /// it is cheap precisely because the firefly is so far out of family: a
    /// ceiling sixty times above every legitimate value in the frame still catches
    /// it.
    ///
    /// **APPLIED AFTER THE 2x2 AVERAGE, AND NOT BY CHOICE.** The average is
    /// performed by the texture unit inside a single bilinear fetch, so by the
    /// time the shader sees a number the four source texels no longer exist
    /// separately. `verify_613` §G measures what that costs: clamping the texels
    /// themselves would leave **0.30%** rather than 1.19%, a factor of 4. That gap
    /// is the price of the one-tap downsample, and it is the real reason shipping
    /// engines use Karis's 13-tap kernel — not because it is wider, but because it
    /// is the only shape that can see what it is averaging.
    ///
    /// **It is a lie either way, and a deliberate one**: energy is being
    /// discarded. The alternative is Karis's partial average — weight each texel
    /// by `1/(1 + luma)` so bright outliers are down-weighted rather than
    /// truncated — which is better behaved and harder to reason about. Exercise 3.
    float clamp_max = 100.0f;

    /// How many pyramid levels. Clamped to what the frame can actually halve.
    int levels = 6;

    /// The tent's sampling radius during upsample, in **source** texels.
    ///
    /// 1.0 is the plain 1-2-1 tent derived in §3.4. Larger values widen each
    /// level's contribution, which smooths the result and eventually makes the
    /// pyramid's own block structure visible as soft square edges.
    float radius = 1.0f;
};

/// How much of a pixel survives the bright pass — the soft knee, as a scalar.
///
/// Derived rather than quoted (§3.6). We want a function that is 0 below
/// `T - k`, is exactly `x - T` above `T + k`, and joins both smoothly. A
/// quadratic does it, and there is only one that fits:
///
///     f(x) = (x - T + k)^2 / (4k)      for x in [T-k, T+k]
///
/// Check it: at `x = T-k` the numerator is 0, so `f = 0` and `f' = 0`, matching
/// the flat branch. At `x = T+k` it is `(2k)^2/(4k) = k`, matching `x - T = k`,
/// and `f' = 2(2k)/(4k) = 1`, matching the linear branch's slope. Both joins
/// agree in value and in slope, which is what C1 means and what keeps a pixel
/// drifting across the threshold from popping.
///
/// Returned as a WEIGHT (the surviving luminance), not as a colour, so the caller
/// can scale the original colour by `f(luma)/luma` and keep the hue exactly. A
/// bright pass that operates per channel desaturates everything it touches,
/// which is a strange thing for a filter whose job is to select.
[[nodiscard]] float bright_pass_weight(float luma, float threshold, float knee);

/// A bilinear fetch from an `hdr_buffer`, in **texel** coordinates, edge-clamped.
///
/// `(0.5, 0.5)` is the centre of texel (0, 0) — the half-texel convention from
/// Lesson 3.9, which matters more here than anywhere yet, because every filter
/// below is defined by where its taps land relative to texel centres. Getting it
/// wrong shifts the entire bloom by half a pixel per level, which compounds down
/// the chain into a visible diagonal drift.
[[nodiscard]] linear_rgb sample_bilinear(const hdr_buffer& src, float x, float y);

/// Every level of the bloom, owned together.
///
/// **This type exists to answer "who owns the intermediate targets?"** — the
/// question `gpu_post.hpp` deferred to this lesson. The answer it gives is the
/// narrow one, and the narrowness is the point: a pass that knows its own
/// intermediates owns them. Nothing outside the bloom needs level 3, nothing
/// outside the bloom knows how many levels there are, and the lifetimes are
/// entirely internal — so a general resource pool would be solving a problem the
/// bloom does not have. The general version, for intermediates that cross
/// between passes, is the frame graph in Lesson 6.17.
///
/// The pyramid is **allocated once and reused every frame**. That is not an
/// optimisation, it is the whole reason the type is a class: six buffers
/// allocated and freed per frame at 60 Hz is 360 allocations a second of up to
/// 1.32 MB, which is the kind of cost that does not show up in a profile as a
/// single hot line.
class bloom_pyramid
{
public:
    /// Size the pyramid for a `full_width x full_height` scene buffer.
    ///
    /// **Level 0 is HALF the scene's size**, and that halving is free quality as
    /// well as free speed: the bright pass is the first filter in the chain, so
    /// starting it at half resolution costs a blur we were about to apply anyway.
    ///
    /// Idempotent — a call with the dimensions it already has does nothing, so
    /// the caller may invoke it every frame and pay only on a resize.
    void resize(int full_width, int full_height, int levels);

    [[nodiscard]] int levels() const { return static_cast<int>(levels_.size()); }
    [[nodiscard]] bool empty() const { return levels_.empty(); }

    /// Level `i`, clamped into range. Level 0 is half the scene's size.
    [[nodiscard]] hdr_buffer& level(int i);
    [[nodiscard]] const hdr_buffer& level(int i) const;

    /// The finished bloom — level 0, after the upsample chain has run.
    [[nodiscard]] const hdr_buffer& result() const { return level(0); }

    /// Total texels across every level, for the memory claim in §5.
    [[nodiscard]] std::size_t texels() const;

private:
    std::vector<hdr_buffer> levels_;
    int full_width_ = 0;
    int full_height_ = 0;
};

/// Threshold, clamp and halve — the first stage, and the only one that selects.
///
/// `dst` is half `src`'s size (pyramid level 0). Each destination texel averages
/// the 2x2 source block beneath it, and the selection happens **after** the
/// average, so a single bright texel is diluted by its three neighbours before it
/// is tested. That ordering is deliberate and it is half of the firefly story;
/// `clamp_max` is the other half.
void bright_pass(const hdr_buffer& src, hdr_buffer& dst,
                 const bloom_settings& s, float exposure);

/// Halve, by averaging each 2x2 block.
///
/// **On the GPU this is ONE bilinear tap, not four fetches.** A bilinear sample
/// placed exactly at the corner shared by four texels returns their unweighted
/// mean, because all four weights are 1/2 x 1/2. `verify_613` §C checks the
/// identity on real numbers rather than asserting it. That is why
/// `gpu_bloom`'s sampler is LINEAR where the 6.12 resolve's is deliberately
/// NEAREST — the filter mode is carrying arithmetic here, not smoothing.
void downsample(const hdr_buffer& src, hdr_buffer& dst);

/// Double, with a 1-2-1 tent, and ADD into `dst`.
///
/// The tent is derived, not chosen: a bilinear upsample IS a tent filter, because
/// a box convolved with a box is a triangle — discretely, `[1 1] * [1 1] =
/// [1 2 1]`. Applying it once per level means the composite kernel is a repeated
/// convolution of tents, which by the central limit theorem approaches a
/// Gaussian; each level's Gaussian is twice as wide as the one below it, and
/// their sum is the power-law envelope this file's header measured.
///
/// **Added, not assigned**, which is what makes the result a SUM over levels
/// rather than just the widest one. On the GPU this is the hardware's additive
/// blend — Lesson 6.11's machinery in a pass with no geometry in it.
void upsample_add(const hdr_buffer& small, hdr_buffer& dst, float radius);

/// Run the whole chain: bright pass, down, up. `py.result()` holds the bloom.
///
/// **The chain's gain is about `levels`, and `intensity` absorbs it.** Every
/// level is added at full weight, so the finished level 0 carries roughly N
/// copies of the bright pass's energy for N levels — `verify_613` §D measures the
/// ratio. This is what shipping implementations do (each level is a Gaussian of a
/// different width and you want all of them), but it means `intensity` is a tuned
/// scalar and NOT "the fraction of the image that is bloom". Saying so is
/// cheaper than leaving a reader to discover that 0.04 is not four percent.
///
/// @param exposure the same value the resolve will apply to the scene. It must be
///        the same number in both places or the two halves of the composite are
///        photographed at different shutter speeds.
void compute_bloom(const hdr_buffer& src, bloom_pyramid& py,
                   const bloom_settings& s, float exposure);

/// Total energy in a buffer's red channel — the measurement `verify_613` leans on.
///
/// Public because "did this filter conserve what it was supposed to?" is the
/// question every stage in the chain has to answer, and a test that cannot ask it
/// can only compare pictures.
[[nodiscard]] double total_energy(const hdr_buffer& src);

} // namespace engine
