// engine/src/gfx/hdr.cpp — exposure, four curves, and one full-screen pass.
//
// Lesson 6.12. Every function here is short; the arguments are in hdr.hpp, and
// the code should be short enough to check against them by eye.

#include <engine/gfx/hdr.hpp>

// Lesson 6.13: `resolve` composites the bloom, so it needs the bloom's sampler.
#include <engine/gfx/bloom.hpp>

#include <engine/gfx/framebuffer.hpp>

#include <algorithm>
#include <cmath>

namespace engine {

const char* name_of(tonemap op)
{
    switch (op)
    {
    case tonemap::clamp:          return "clamp";
    case tonemap::reinhard:       return "reinhard";
    case tonemap::reinhard_white: return "reinhard-white";
    case tonemap::aces:           return "aces";
    }
    return "?";
}

// ---- Exposure ---------------------------------------------------------------

float exposure_from_ev100(float ev100)
{
    // max_luminance = 1.2 * 2^EV100, and the exposure is its reciprocal. See the
    // header for where the 1.2 comes from and why it is quoted rather than
    // derived.
    //
    // `exp2` and not `pow(2, x)`: the same value, computed by a dedicated
    // instruction on every platform this course targets, and exact for integer
    // arguments — which matters, because EV values are very often integers and a
    // "stop" that is not exactly a factor of two is a stop nobody can reason
    // about.
    const float max_luminance = 1.2f * std::exp2(ev100);
    return 1.0f / max_luminance;
}

float ev100_from_luminance(float lum)
{
    // The inverse of the above, with the same calibration constant. A
    // non-positive luminance has no EV — a black frame does not imply an
    // exposure — so it returns the reference rather than a negative infinity that
    // would propagate into every subsequent multiply.
    if (!(lum > 0.0f)) { return k_reference_ev100; }
    return std::log2(lum / 1.2f);
}

// ---- The curves -------------------------------------------------------------

namespace {

/// Narkowicz's 2015 fit to the ACES RRT + ODT. Five constants, no derivation —
/// see the header. Written with the multiplies factored the way he published
/// them, because a rearrangement that is algebraically identical is not
/// numerically identical and this is a *fit*: its error budget was measured
/// against this arrangement.
[[nodiscard]] float aces_fit(float x)
{
    constexpr float a = 2.51f;
    constexpr float b = 0.03f;
    constexpr float c = 2.43f;
    constexpr float d = 0.59f;
    constexpr float e = 0.14f;
    return std::clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0f, 1.0f);
}

} // namespace

float apply_tonemap(float x, tonemap op, float white)
{
    // NEGATIVE INPUT IS NOT A BRIGHTNESS. It reaches here from an interpolated
    // value that overshot, or from a light with a negative component somebody
    // typed. Clamped at zero for every operator, because `x/(1+x)` at x = -2 is
    // +2, which is not merely wrong but wrong in the *bright* direction.
    if (!(x > 0.0f)) { return 0.0f; }

    switch (op)
    {
    case tonemap::clamp:
        return std::min(x, 1.0f);

    case tonemap::reinhard:
        // THE WHOLE DERIVATION, and it is worth having in one line: we want to
        // divide by something that is about 1 for small x (so the dark end is
        // untouched) and about x for large x (so the bright end flattens). The
        // simplest function that is both is `1 + x`.
        return x / (1.0f + x);

    case tonemap::reinhard_white:
    {
        // The same shape, with the extra factor chosen so that f(W) = 1 exactly:
        //   f(W) = W(1 + W/W^2)/(1+W) = W(1 + 1/W)/(1+W) = (W+1)/(1+W) = 1.
        // A white point at or below zero is meaningless, so fall back to the
        // plain operator rather than dividing by it.
        if (!(white > 0.0f)) { return x / (1.0f + x); }
        const float w2 = white * white;
        return std::min(x * (1.0f + x / w2) / (1.0f + x), 1.0f);
    }

    case tonemap::aces:
        return aces_fit(x);
    }
    return std::min(x, 1.0f);
}

linear_rgb apply_tonemap(linear_rgb c, const tonemap_settings& s)
{
    if (s.per_channel)
    {
        return {apply_tonemap(c.r, s.op, s.white),
                apply_tonemap(c.g, s.op, s.white),
                apply_tonemap(c.b, s.op, s.white)};
    }

    // LUMINANCE-ONLY: compute the curve once, on the one number that says how
    // bright the colour is, and scale all three channels by the RATIO. The hue
    // and the saturation are preserved exactly, because scaling a colour by a
    // scalar is a move along the ray from black through it.
    //
    // AND THAT IS ALSO ITS FAILURE: a saturated colour whose luminance lands at
    // 1 can easily have a channel above 1 — a pure blue has luminance 0.0722, so
    // a blue of (0, 0, 8) has luminance 0.578, sails through the curve, and
    // arrives with b = 6.9 for the encode to clamp. The clamp then shifts the hue
    // anyway, just at a threshold nobody chose. §D measures it.
    const float lum = luminance(c);
    if (!(lum > 0.0f)) { return {}; }
    const float scale = apply_tonemap(lum, s.op, s.white) / lum;
    return {c.r * scale, c.g * scale, c.b * scale};
}

// ---- The buffer -------------------------------------------------------------

hdr_buffer::hdr_buffer(int width, int height)
    : width_(std::max(1, width)), height_(std::max(1, height))
{
    pixels_.assign(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_),
                   linear_rgb{});
}

void hdr_buffer::clear(linear_rgb colour)
{
    std::fill(pixels_.begin(), pixels_.end(), colour);
}

linear_rgb hdr_buffer::pixel_at(int x, int y) const
{
    if (x < 0 || y < 0 || x >= width_ || y >= height_) { return {}; }
    return pixels_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_)
                 + static_cast<std::size_t>(x)];
}

void hdr_buffer::put_pixel(int x, int y, linear_rgb c)
{
    if (x < 0 || y < 0 || x >= width_ || y >= height_) { return; }
    pixels_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_)
          + static_cast<std::size_t>(x)] = c;
}

hdr_stats measure(const hdr_buffer& src)
{
    hdr_stats st{};
    st.pixels = src.width() * src.height();
    if (st.pixels == 0) { return st; }

    double sum = 0.0;
    double log_sum = 0.0;

    for (const linear_rgb& p : src.pixels())
    {
        const float m = std::max(p.r, std::max(p.g, p.b));
        st.max_channel = std::max(st.max_channel, m);
        if (m > 1.0f) { ++st.over_one; }

        const float lum = luminance(p);
        st.max_luminance = std::max(st.max_luminance, lum);
        sum += static_cast<double>(lum);

        // THE EPSILON IS NOT A FUDGE. A frame's background is exactly zero and
        // log(0) is negative infinity, which would make the log-average of every
        // frame with one black pixel in it negative infinity. The standard
        // treatment (Reinhard 2002 §3.1) adds a small delta for precisely this
        // reason, and the value only has to be well below anything a display can
        // show.
        log_sum += std::log(static_cast<double>(lum) + 1.0e-4);
    }

    const double n = static_cast<double>(st.pixels);
    st.mean_luminance = static_cast<float>(sum / n);
    st.log_mean_luminance = static_cast<float>(std::exp(log_sum / n));
    return st;
}

void resolve(const hdr_buffer& src, framebuffer& dst, const tonemap_settings& s,
             encode_mode mode, const hdr_buffer* bloom, float bloom_intensity)
{
    // Dimensions must agree. A partial resolve is worse than none: it leaves a
    // frame that is half tonemapped and half whatever was in the target, which
    // looks like a rendering bug in whichever half you notice first.
    if (src.width() != dst.width() || src.height() != dst.height()) { return; }

    for (int y = 0; y < src.height(); ++y)
    {
        const linear_rgb* in = src.row(y);
        Uint32* out = dst.row(y);
        for (int x = 0; x < src.width(); ++x)
        {
            // ONE: exposure, in linear light, because light accumulates linearly
            // in time and that is what an exposure models.
            linear_rgb lit{in[x].r * s.exposure,
                           in[x].g * s.exposure,
                           in[x].b * s.exposure};

            // ONE AND A HALF (Lesson 6.13): the bloom, added while everything is
            // still linear light and BEFORE the curve.
            //
            // The bloom is half resolution, so this is a bilinear fetch at the
            // destination pixel's position in bloom-texel space: `(x + 0.5) * 0.5`
            // is the same destination-to-source mapping `upsample_add` uses, which
            // is what keeps this last upsample aligned with all the ones before it.
            //
            // NO EXPOSURE MULTIPLY HERE. `bright_pass` already applied it, and
            // applying it twice would photograph the glow at a different shutter
            // speed from the scene it belongs to — visible as a bloom that grows
            // quadratically while the image it sits on grows linearly.
            if (bloom != nullptr && bloom_intensity > 0.0f)
            {
                const linear_rgb b = sample_bilinear(
                    *bloom,
                    (static_cast<float>(x) + 0.5f) * 0.5f,
                    (static_cast<float>(y) + 0.5f) * 0.5f);
                lit.r += b.r * bloom_intensity;
                lit.g += b.g * bloom_intensity;
                lit.b += b.b * bloom_intensity;
            }

            // TWO: the curve, still in linear light.
            const linear_rgb mapped = apply_tonemap(lit, s);

            // THREE: the encode, last and exactly once. `to_encoded` also clamps,
            // which after a tonemap is a no-op for every operator except
            // luminance-only — see `apply_tonemap`'s note about saturated colours
            // arriving with a channel above 1.
            out[x] = to_encoded(mapped, mode);
        }
    }
}

} // namespace engine
