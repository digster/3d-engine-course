// engine/include/engine/gfx/hdr.hpp — what to do when a pixel is brighter than white.
//
// Lesson 6.12, and the first thing to establish is that this engine has not been
// getting HDR wrong. It has been **avoiding the question by construction**, which
// is a different thing and is worth seeing clearly before anything is built.
//
// TWO CHOICES HAVE BEEN HOLDING THE LID ON.
//
//   1. `k_reference_irradiance` is pi, and Lesson 6.2 derived exactly why: a white
//      Lambertian surface lit square-on reflects `E_perp / pi`, so `E_perp = pi`
//      makes it render at precisely 1.0. That is a photographer's white card, and
//      it is a *choice* — light.hpp says so, and says this lesson replaces it.
//   2. The demo's roughness is 0.49. At the mirror angle that surface peaks at
//      **0.8676** — just under the lid.
//
// Change the second number and the lid comes off immediately. Measured, at the
// light's actual mirror direction (`scratch/probe_612.cpp`):
//
//     white dielectric, roughness 0.20      11.59      +3.5 stops
//     white dielectric, roughness 0.05    2823.99     +11.5 stops
//     polished metal,   roughness 0.20     230.27      +7.8 stops
//     polished metal,   roughness 0.05   59003.75     +15.8 stops
//
// Every one of those arrives at the screen as **code 255**. Not "a bit too
// bright" — the same code, for all of them, with the highlight's entire shape
// flattened into a white blob. `light.hpp` predicted this in as many words:
// *"a bright highlight arrives at the screen as a flat white blob with its shape
// clipped off, which is exactly the information tonemapping exists to keep."*
//
// AND THE UNITS DEBT, WHICH IS THE SAME PROBLEM WEARING A SUIT. Lesson 6.2
// renamed `intensity` to `irradiance` so that lights would be measured rather
// than tuned, and then could not let anybody author one: real sunlight is about
// 120,000 lux, and that surface under real sunlight peaks at **8,793,937**. You
// cannot author in physical units until something downstream decides what maps to
// white. That something is an exposure control, and it is below.
//
// ---------------------------------------------------------------------------
// THE SHAPE OF THE ANSWER
// ---------------------------------------------------------------------------
//
//     render into a buffer that can hold the values  ->  decide an exposure
//       ->  map the range down  ->  encode  ->  8 bits
//
// Four steps, and the important structural point is that only the FIRST happens
// during rasterization. The other three are a **full-screen pass over a finished
// image**, which is why they live in their own type here rather than in
// `fill_style`, and why the GPU half is a second render pass rather than more
// fragment shader. A tonemapper evaluated inside the shading loop cannot use an
// exposure derived from the frame's own histogram, cannot have a bloom applied
// before it, and cannot be changed without re-rendering. Lesson 6.13 builds the
// stack; this is the first stage in it.
//
// ---------------------------------------------------------------------------
// WHAT THIS MEANS FOR THE REFERENCE RENDER
// ---------------------------------------------------------------------------
//
// It cannot move, and the reason is structural rather than a flag: the fixture
// renders into an 8-bit `framebuffer` through the path it has always used, and
// everything here operates on a DIFFERENT BUFFER TYPE. This is not "the
// tonemapper is optional" — it is "the tonemapper is a stage over a target the
// fixture does not have". Twenty-one lessons at hash E917C06C.

#pragma once

#include <engine/gfx/colour.hpp>

#include <SDL3/SDL.h>

#include <cstddef>
#include <span>
#include <vector>

namespace engine {

class framebuffer;

// ---- Luminance ---------------------------------------------------------------

/// How bright a colour is, as one number — Rec. 709 / sRGB primaries.
///
/// **The weights are not a taste**: they are the Y row of the sRGB-to-XYZ matrix,
/// which is to say they are how much each primary contributes to the perception
/// of brightness. Green dominates because the eye's response peaks there, and
/// blue is nearly negligible — which is why a pure blue at full intensity looks
/// dark and a pure green looks glaring.
///
/// It appears in three places below, all of them consequences of the same fact:
/// deciding an exposure, tonemapping without shifting hue, and measuring how much
/// of a frame is over the lid.
///
/// **Input must be linear light.** Applying these weights to sRGB codes is the
/// 1.6 mistake in yet another costume, and it is a popular one — a great deal of
/// image-processing code computes "luminance" from stored bytes.
[[nodiscard]] constexpr float luminance(linear_rgb c)
{
    return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;
}

// ---- Exposure ----------------------------------------------------------------

/// Convert a photographic exposure value at ISO 100 into the multiplier a
/// renderer applies before tonemapping.
///
/// **EV is a logarithmic scale of light, and one EV is one stop** — a factor of
/// two. EV 0 is a very dim room; EV 15 is a sunny day; each step up halves the
/// amount of light that reaches white. That is the whole intuition, and it is why
/// artists and photographers can talk to each other about it.
///
/// The constant is standardised rather than derived, and it is worth being honest
/// about which:
///
///     max_luminance = 1.2 * 2^EV100          (the saturation-based standard)
///     exposure      = 1 / max_luminance
///
/// The 1.2 is `78 / (q * 100)` with `q = 0.65`, from ISO 12232's saturation-based
/// speed definition — a calibration of real camera hardware, not a fact about
/// light. Lagarde and de Rousiers, *Moving Frostbite to PBR* (2014) §5.1 works it
/// through properly; this course takes it as given and says so.
///
/// **What it buys is the units debt.** With this in place a light can be authored
/// at 120,000 lux because it *is* 120,000 lux, and the exposure decides what maps
/// to white — exactly as a camera does, and exactly as `k_reference_irradiance`'s
/// doc comment promised in Lesson 6.2.
[[nodiscard]] float exposure_from_ev100(float ev100);

/// The inverse: which EV makes this luminance the one that maps to white.
///
/// Useful for the other direction of the same conversation — "the scene is this
/// bright; what exposure is correct for it?" — and it is how an auto-exposure
/// system turns a measured average into a setting.
[[nodiscard]] float ev100_from_luminance(float luminance);

/// The EV that reproduces this engine's behaviour before Lesson 6.12.
///
/// `k_reference_irradiance = pi` was chosen so a white surface renders at exactly
/// 1.0, which means "1.0 maps to white", which means an exposure of exactly 1 —
/// and `exposure_from_ev100` returns 1 at **EV 100 = log2(1/1.2) = -0.263**.
/// Naming it makes the old behaviour a *point on the new scale* rather than a
/// special case, which is what turns a replacement into a generalisation.
inline constexpr float k_reference_ev100 = -0.2630344f;

// ---- Tonemapping -------------------------------------------------------------

/// How to bring an unbounded range down to [0, 1].
///
/// Every one of these is a function from `[0, inf)` to `[0, 1]`, and the three
/// properties that matter are the same for all of them: it must be **monotonic**
/// (brighter input, brighter output — or the image is not a photograph of
/// anything), it must be **close to the identity near zero** (or the dark end of
/// every image is wrong), and it must **not exceed 1** (or it has not done its
/// job). What separates them is what they do in between.
enum class tonemap
{
    /// `min(x, 1)`. What this engine has always done, and what `to_encoded` does.
    ///
    /// **Kept as a named option rather than as an absence**, for the same reason
    /// `blend_space::encoded` and `interpolation::affine` are kept: it is the
    /// failure this lesson exists to fix, and a failure you can select is worth
    /// more than one you have to describe. It is also genuinely correct for
    /// content that was authored in [0, 1] — a UI, a 2-D overlay — which is why
    /// it is the default.
    clamp,

    /// `x / (1 + x)`. Reinhard (2002), and the one to understand properly,
    /// because the derivation is two sentences and everything else here is a
    /// variation on it. See §3.2 of the lesson.
    ///
    /// **It never reaches 1.** The consequence is concrete and famous: code 255
    /// needs an encoded value of at least 254.5/255, which is a linear 0.99554,
    /// which needs an input of **224** — so an image tonemapped this way has no
    /// pure white in it at any brightness you would plausibly render, and looks
    /// washed out for exactly that reason.
    ///
    /// (That number was 768 in this comment's first draft, from an arithmetic
    /// slip — the 254.5/255 threshold applies to the ENCODED value, not the
    /// linear one, and confusing the two is this whole module's recurring
    /// mistake. `verify_612` §C measures it rather than repeating it.)
    reinhard,

    /// Reinhard with a **white point**: `x(1 + x/W^2) / (1 + x)`.
    ///
    /// Choose the value you want to render as white, and the formula follows from
    /// requiring `f(W) = 1` while keeping the shape. Solves the "no pure white"
    /// problem exactly, and gives an artist one meaningful dial instead of none.
    reinhard_white,

    /// A curve **fitted** to the ACES reference rendering transform — Krzysztof
    /// Narkowicz's 2015 approximation, five constants and no derivation.
    ///
    /// It is here because it is what the industry actually ships and because its
    /// toe and shoulder do something the Reinhard family does not: they mimic
    /// film, darkening the very bottom and rolling off the very top, which reads
    /// as "photographic" rather than "mathematically correct". **It is a fit, not
    /// a model**, and pretending otherwise would be exactly the hand-waving this
    /// course refuses. The real ACES transform is a much larger piece of
    /// machinery in a different colour space.
    aces
};

[[nodiscard]] const char* name_of(tonemap op);

/// Everything the resolve needs, gathered — the same bargain `fill_style` made.
struct tonemap_settings
{
    tonemap op = tonemap::clamp;

    /// Multiplied into the colour **before** the curve. `exposure_from_ev100` is
    /// where this normally comes from; 1.0 reproduces the pre-6.12 engine.
    float exposure = 1.0f;

    /// The input value that `reinhard_white` should map to exactly 1. Ignored by
    /// every other operator.
    ///
    /// Defaults to 4 — two stops above the old lid, which is a sensible starting
    /// point rather than a derived one, and is the sort of number an artist
    /// should be changing.
    float white = 4.0f;

    /// Apply the curve to each channel independently, or to the **luminance**
    /// alone?
    ///
    /// This is a real choice with a visible difference, not a tuning knob.
    /// Per-channel compresses each primary separately, so a bright saturated
    /// colour **desaturates toward white** as it brightens — which is what film
    /// does and what most people expect a highlight to look like. Luminance-only
    /// preserves the hue exactly and can therefore leave individual channels
    /// **above 1 after tonemapping**, which then clip and shift the hue anyway,
    /// at a different threshold. §D of the harness measures both.
    bool per_channel = true;
};

/// Apply one operator to one value. The curve, and nothing else.
///
/// Separate from the settings so the shape can be tabulated, plotted and tested
/// on its own — the same argument `wrap_texel` (3.9) and `is_top_left` (2.4)
/// made. Exposure is *not* applied here; it is a separate multiply, because it
/// belongs to the frame and the curve belongs to the display.
[[nodiscard]] float apply_tonemap(float x, tonemap op, float white = 4.0f);

/// Apply an operator to a colour, honouring `per_channel`.
[[nodiscard]] linear_rgb apply_tonemap(linear_rgb c, const tonemap_settings& s);

// ---- The buffer --------------------------------------------------------------

/// A CPU render target that can hold values above 1.
///
/// **Three floats per pixel, and the memory is the point of the type.** At
/// 960x540 that is 6.22 MB against `framebuffer`'s 2.07 MB — **3x**, and the GPU
/// half pays 2x rather than 3x because it uses half floats. That multiplier is
/// the honest price of the whole lesson, and it is why HDR arrived in games at
/// the same time as the memory bandwidth to afford it.
///
/// **No alpha**, deliberately. Lesson 6.11 established that a colour target's
/// alpha is read by nothing in this engine, and a float alpha would add 33% to
/// the above in exchange for a channel with no consumer. `linear_rgb` is
/// therefore the pixel type, which also means the buffer's contents are *the same
/// type the shading equation returns* — no conversion at the write at all, which
/// is the second thing this type buys.
class hdr_buffer
{
public:
    /// `width x height` pixels, zero-initialised (black). Dimensions below 1 are
    /// clamped, exactly as `framebuffer` does, so that every index calculation
    /// stays well-defined rather than failing.
    hdr_buffer(int width, int height);

    void clear(linear_rgb colour = {});

    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }

    /// A whole row, for a fill loop that has already hoisted its bounds check —
    /// the same fast path `framebuffer::row` offers, and for the same reason.
    [[nodiscard]] linear_rgb* row(int y) { return &pixels_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_)]; }
    [[nodiscard]] const linear_rgb* row(int y) const { return &pixels_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_)]; }

    [[nodiscard]] linear_rgb pixel_at(int x, int y) const;
    void put_pixel(int x, int y, linear_rgb c);

    [[nodiscard]] std::span<const linear_rgb> pixels() const { return pixels_; }

private:
    std::vector<linear_rgb> pixels_;
    int width_ = 0;
    int height_ = 0;
};

/// What is actually in an HDR buffer. Numbers, so a decision can be made from
/// them rather than from a look at the screen.
struct hdr_stats
{
    float max_channel = 0.0f;      ///< the largest single component anywhere
    float max_luminance = 0.0f;    ///< the largest `luminance()`
    float mean_luminance = 0.0f;

    /// The **geometric** mean of luminance, which is what an auto-exposure wants
    /// and is not the same as the arithmetic mean. A single specular pixel at
    /// 59,000 drags an arithmetic mean anywhere it likes; a log-average barely
    /// notices it, because the log of a large number is a small number. That
    /// robustness is the entire reason exposure metering is done in log space.
    float log_mean_luminance = 0.0f;

    /// Pixels with any channel above 1 — the ones a clamp would flatten.
    int over_one = 0;
    int pixels = 0;

    [[nodiscard]] float over_fraction() const
    {
        return (pixels > 0) ? static_cast<float>(over_one) / static_cast<float>(pixels) : 0.0f;
    }
};

[[nodiscard]] hdr_stats measure(const hdr_buffer& src);

/// **The resolve**: exposure, curve, encode — one full-screen pass, into 8 bits.
///
/// This is the function that makes an HDR buffer into a picture, and the order of
/// its three steps is not negotiable:
///
///   1. **Exposure** multiplies, in linear light, because it models how long the
///      shutter was open and light accumulates linearly in time.
///   2. **The curve** compresses, in linear light, because it is a statement
///      about quantities of light and not about codes.
///   3. **The encode** happens last and exactly once, because that is what the
///      display's transfer function is for (Lesson 6.1's rule, unchanged).
///
/// Get the order wrong and each mistake has its own signature: exposure after the
/// curve does nothing useful at the bright end (everything is already compressed
/// into the same place), and tonemapping after the encode compresses *codes*,
/// which crushes the shadows the encode had carefully spread out.
///
/// **Lesson 6.13 added a fourth step, and put it FIRST.** A bloom is light, so it
/// is composited into the scene before the curve runs:
///
///     (scene * exposure  +  intensity * bloom)  ->  curve  ->  encode
///
/// Composite it AFTER the curve instead and the arithmetic tells you why not: the
/// curve's output is already in [0, 1], so adding anything to it lands above 1
/// and is clipped by the encode — every glow grows a flat white core, and the
/// brighter the source the bigger that core. Which is precisely the artefact
/// bloom was introduced to remove, reintroduced one pass later.
///
/// The bloom carries its OWN exposure, applied during the bright pass, which is
/// why `exposure` is not multiplied into it here. `bloom.hpp`'s `threshold`
/// comment explains why the bright pass has to be the place that applies it.
///
/// @param dst must be the same dimensions as `src`; a mismatch is a no-op rather
///        than a partial write, because half a resolved frame is worse than none.
/// @param bloom half-resolution, from `compute_bloom`; null for no bloom. It is
///        upsampled with a bilinear fetch per pixel — the same tent the pyramid
///        used, applied one last time.
/// @param bloom_intensity see `bloom_settings::intensity`, and note what its doc
///        comment says about the chain's gain: this is a tuned scalar, not a
///        percentage.
///
/// **Two inputs is the point at which this signature stops scaling**, and that is
/// worth naming rather than absorbing. A third would need a struct; a fourth
/// would need something that knows the order. On the GPU side, where the passes
/// are real and the intermediates are real memory, that something is
/// `gpu_post_stack` — and the general form, for intermediates whose lifetimes
/// cross passes, is Lesson 6.17's frame graph.
void resolve(const hdr_buffer& src, framebuffer& dst, const tonemap_settings& s,
             encode_mode mode = encode_mode::exact,
             const hdr_buffer* bloom = nullptr, float bloom_intensity = 0.0f);


// ---------------------------------------------------------------------------
// Lesson 6.15 — half floats, because HDR data has to reach the device somehow
// ---------------------------------------------------------------------------

/// Encode a `float` as an IEEE 754 **binary16** bit pattern.
///
/// **Why this function did not exist until Lesson 6.15.** Everything HDR in this
/// engine so far has been PRODUCED on the GPU — `gpu_post_stack` renders into a
/// `k_hdr_format` target and reads it back through a sampler, and no half float
/// ever crosses the CPU boundary. An environment map is the first HDR data that
/// is computed on the CPU and has to be uploaded, and `SDL_UploadToGPUTexture`
/// takes bytes in the texture's own format. There is no half type in C++20 and
/// no SDL helper, so this is ours to write.
///
/// **The format, which is worth knowing once**: 1 sign bit, 5 exponent bits with
/// a bias of 15, 10 mantissa bits. That gives a largest finite value of 65,504
/// and about 3 decimal digits of precision — and the precision is RELATIVE,
/// which is exactly the property `gpu_post.hpp` argues makes half floats right
/// for light. A sun disc at 6,000 stores with a relative error of about 0.05%;
/// the sky at 0.4 stores with the same 0.05%.
///
/// **The three cases that are not the common one**, each of which produces a
/// visible artefact if skipped:
///
///   - **Overflow.** Anything above 65,504 becomes `inf`, and an `inf` in an
///     environment map propagates through the prefilter into a whole mip level
///     of NaN. We CLAMP to the largest finite half instead, and say so here
///     rather than leaving a caller to discover it: a sun stored at 65,504
///     instead of 100,000 is wrong by a stop and a half, and an `inf` is wrong
///     by everything.
///   - **Underflow to subnormal.** Below 2^-14 the exponent cannot go lower and
///     the mantissa has to absorb the shift. Getting this wrong rounds small
///     values to zero, which on a radiance map is invisible until you tonemap
///     with a low exposure and find the shadows have quantised.
///   - **Round to nearest even**, not truncate. Truncation biases every value
///     downward by half a step on average, which over a whole environment map is
///     a systematic darkening rather than noise.
///
/// @return the 16 bits, in the low half of a `Uint16`. Negative inputs are
///         encoded faithfully even though radiance cannot be negative, because a
///         function that silently clamps its input is a function whose test
///         cannot tell you it is working.
[[nodiscard]] Uint16 float_to_half(float value);

/// Decode a binary16 bit pattern back to `float`. **Exact** — every half is
/// representable as a float — which is what makes the round trip
/// `half_to_float(float_to_half(x))` a measurable quantity rather than a hope.
/// `verify_615` §I measures its worst relative error across five decades.
[[nodiscard]] float half_to_float(Uint16 bits);

} // namespace engine
