// engine/src/gfx/bloom.cpp — four filters, and the chain that orders them.
//
// Lesson 6.13. The argument is in bloom.hpp. What is here is deliberately plain:
// every one of these functions is a loop over pixels doing arithmetic that the
// corresponding HLSL does identically, because §G of the harness renders the same
// values through both and compares them code for code. Two implementations of one
// filter that disagree are a bug that only shows up in a diff — the same argument
// `tonemap.frag.hlsl` makes about the curve.

#include <engine/gfx/bloom.hpp>

#include <cmath>

namespace engine {

// ---- The knee ---------------------------------------------------------------

float bright_pass_weight(float luma, float threshold, float knee)
{
    // A negative luminance is not a brightness. It can arise from a filter with
    // negative lobes (ours has none) or from a NaN propagating through a shading
    // term, and letting it through would subtract energy from the bloom.
    if (!(luma > 0.0f)) { return 0.0f; }

    // Zero knee is a hard cut, and it is reachable on purpose: §3.6 shows the
    // flicker it causes, and a failure you can select is worth more than one you
    // have to describe. (The same argument `tonemap::clamp` makes.)
    if (knee <= 0.0f) { return (luma > threshold) ? luma - threshold : 0.0f; }

    if (luma <= threshold - knee) { return 0.0f; }
    if (luma >= threshold + knee) { return luma - threshold; }

    // The quadratic, derived in bloom.hpp's doc comment. At the lower join it is
    // 0 with slope 0; at the upper it is k with slope 1. Both branches it joins
    // agree there, which is what stops a pixel drifting across the threshold from
    // popping into existence.
    const float t = luma - threshold + knee;
    return t * t / (4.0f * knee);
}

// ---- Sampling ---------------------------------------------------------------

linear_rgb sample_bilinear(const hdr_buffer& src, float x, float y)
{
    if (src.width() <= 0 || src.height() <= 0) { return {}; }

    // THE HALF-TEXEL SHIFT, and it is the single easiest thing to get wrong in
    // this file. `x` is in texel coordinates where 0.5 is the CENTRE of texel 0,
    // so subtracting 0.5 converts to "distance from texel 0's centre", which is
    // the space the floor-and-lerp below actually works in. Omit it and every
    // filter here is offset by half a texel — which compounds down the pyramid
    // into a bloom that drifts diagonally away from what produced it.
    const float fx = x - 0.5f;
    const float fy = y - 0.5f;

    const int x0 = static_cast<int>(std::floor(fx));
    const int y0 = static_cast<int>(std::floor(fy));
    const float tx = fx - static_cast<float>(x0);
    const float ty = fy - static_cast<float>(y0);

    // CLAMP TO EDGE, matching `address_mode::clamp_to_edge` on the GPU side. The
    // alternative — treating outside as black — darkens the bloom along every
    // border of the screen, which reads as a vignette nobody asked for.
    auto at = [&](int px, int py) {
        px = (px < 0) ? 0 : (px >= src.width()  ? src.width()  - 1 : px);
        py = (py < 0) ? 0 : (py >= src.height() ? src.height() - 1 : py);
        return src.pixel_at(px, py);
    };

    const linear_rgb a = at(x0,     y0);
    const linear_rgb b = at(x0 + 1, y0);
    const linear_rgb c = at(x0,     y0 + 1);
    const linear_rgb d = at(x0 + 1, y0 + 1);

    const auto mix = [](float u, float v, float t) { return u + (v - u) * t; };
    return {mix(mix(a.r, b.r, tx), mix(c.r, d.r, tx), ty),
            mix(mix(a.g, b.g, tx), mix(c.g, d.g, tx), ty),
            mix(mix(a.b, b.b, tx), mix(c.b, d.b, tx), ty)};
}

// ---- The pyramid ------------------------------------------------------------

void bloom_pyramid::resize(int full_width, int full_height, int levels)
{
    if (levels < 1) { levels = 1; }
    if (levels > k_max_bloom_levels) { levels = k_max_bloom_levels; }
    if (full_width < 2) { full_width = 2; }
    if (full_height < 2) { full_height = 2; }

    // IDEMPOTENT, so the caller can call it every frame. The dimension check has
    // to include the level count, because asking for fewer levels at the same
    // size is still a change.
    if (full_width == full_width_ && full_height == full_height_
        && static_cast<int>(levels_.size()) == levels)
    {
        return;
    }

    levels_.clear();
    full_width_ = full_width;
    full_height_ = full_height;

    int w = full_width / 2;
    int h = full_height / 2;
    for (int i = 0; i < levels; ++i)
    {
        // INTEGER HALVING, and the odd case is worth a sentence: 135/2 is 67, so
        // two levels of a 540-line frame are 135 and 67, and 67*2 = 134 != 135.
        // The half-texel-correct sampling below absorbs that — `sample_bilinear`
        // is defined in continuous texel coordinates, so a level that is not
        // exactly half its parent simply samples slightly differently, rather
        // than reading off the end of a row. Filters written with integer index
        // arithmetic instead are where the "one-pixel bright line down the right
        // edge at certain window sizes" bug comes from.
        levels_.emplace_back(w, h);
        if (w <= 1 && h <= 1) { break; }
        w = (w > 1) ? w / 2 : 1;
        h = (h > 1) ? h / 2 : 1;
    }
}

hdr_buffer& bloom_pyramid::level(int i)
{
    if (i < 0) { i = 0; }
    if (i >= static_cast<int>(levels_.size())) { i = static_cast<int>(levels_.size()) - 1; }
    return levels_[static_cast<std::size_t>(i)];
}

const hdr_buffer& bloom_pyramid::level(int i) const
{
    if (i < 0) { i = 0; }
    if (i >= static_cast<int>(levels_.size())) { i = static_cast<int>(levels_.size()) - 1; }
    return levels_[static_cast<std::size_t>(i)];
}

std::size_t bloom_pyramid::texels() const
{
    std::size_t n = 0;
    for (const hdr_buffer& lv : levels_)
    {
        n += static_cast<std::size_t>(lv.width()) * static_cast<std::size_t>(lv.height());
    }
    return n;
}

// ---- The stages -------------------------------------------------------------

void bright_pass(const hdr_buffer& src, hdr_buffer& dst,
                 const bloom_settings& s, float exposure)
{
    for (int y = 0; y < dst.height(); ++y)
    {
        linear_rgb* out = dst.row(y);
        for (int x = 0; x < dst.width(); ++x)
        {
            // The centre of the 2x2 source block. `x * 2 + 1` in texel
            // coordinates is exactly the corner shared by texels 2x and 2x+1,
            // which is where a bilinear tap returns their mean — the identity the
            // GPU downsample is built on, used here so the two agree.
            linear_rgb c = sample_bilinear(src, static_cast<float>(x * 2 + 1),
                                                static_cast<float>(y * 2 + 1));

            // EXPOSURE FIRST. The threshold is a statement about the finished
            // image, so it has to be applied in the same space the resolve will
            // use. bloom.hpp's `threshold` comment has the failure mode.
            c.r *= exposure;
            c.g *= exposure;
            c.b *= exposure;

            // THE FIREFLY CLAMP, applied per channel and before the threshold.
            // Per channel rather than on the luminance because a single blown-out
            // channel is exactly what a specular highlight on a coloured metal
            // produces, and clamping the luminance would leave that channel
            // untouched.
            if (s.clamp_max > 0.0f)
            {
                c.r = (c.r > s.clamp_max) ? s.clamp_max : c.r;
                c.g = (c.g > s.clamp_max) ? s.clamp_max : c.g;
                c.b = (c.b > s.clamp_max) ? s.clamp_max : c.b;
            }

            // SELECT ON LUMINANCE, SCALE THE COLOUR. Keeping the ratio between
            // the channels keeps the hue: a bright orange highlight blooms
            // orange. A per-channel threshold instead subtracts a constant from
            // each channel, which moves a saturated colour toward white — the
            // filter would be desaturating what it selects, which is a strange
            // job for a filter whose only job is to select.
            const float lum = luminance(c);
            const float kept = bright_pass_weight(lum, s.threshold, s.knee);
            const float scale = (lum > 0.0f) ? kept / lum : 0.0f;

            out[x] = {c.r * scale, c.g * scale, c.b * scale};
        }
    }
}

void downsample(const hdr_buffer& src, hdr_buffer& dst)
{
    for (int y = 0; y < dst.height(); ++y)
    {
        linear_rgb* out = dst.row(y);
        for (int x = 0; x < dst.width(); ++x)
        {
            out[x] = sample_bilinear(src, static_cast<float>(x * 2 + 1),
                                          static_cast<float>(y * 2 + 1));
        }
    }
}

void upsample_add(const hdr_buffer& small, hdr_buffer& dst, float radius)
{
    // The 1-2-1 tent, as an outer product. `[1 2 1] x [1 2 1] / 16` is the 3x3
    // kernel, and it sums to 1 — so the filter neither brightens nor darkens what
    // it widens, which is the property `verify_613` §D asserts before anything
    // else about this function.
    static constexpr float k_tent[3] = {1.0f, 2.0f, 1.0f};

    for (int y = 0; y < dst.height(); ++y)
    {
        linear_rgb* out = dst.row(y);
        for (int x = 0; x < dst.width(); ++x)
        {
            // Destination texel centre, expressed in SOURCE texel coordinates.
            // `(x + 0.5) * 0.5` maps destination centres onto the source's
            // continuous coordinate, which is the inverse of the `2x + 1` the
            // downsample used — the two are the same mapping read in opposite
            // directions, which is why the pyramid does not drift.
            const float sx = (static_cast<float>(x) + 0.5f) * 0.5f;
            const float sy = (static_cast<float>(y) + 0.5f) * 0.5f;

            linear_rgb acc{};
            for (int j = -1; j <= 1; ++j)
            {
                for (int i = -1; i <= 1; ++i)
                {
                    const float w = k_tent[i + 1] * k_tent[j + 1] * (1.0f / 16.0f);
                    const linear_rgb t = sample_bilinear(small,
                                                         sx + static_cast<float>(i) * radius,
                                                         sy + static_cast<float>(j) * radius);
                    acc.r += t.r * w;
                    acc.g += t.g * w;
                    acc.b += t.b * w;
                }
            }

            // ADDED, not assigned. This is the line that makes the finished
            // pyramid a SUM of every level's Gaussian rather than just the widest
            // one, and therefore the line that produces the power-law tail.
            out[x] = {out[x].r + acc.r, out[x].g + acc.g, out[x].b + acc.b};
        }
    }
}

void compute_bloom(const hdr_buffer& src, bloom_pyramid& py,
                   const bloom_settings& s, float exposure)
{
    py.resize(src.width(), src.height(), s.levels);
    if (py.empty()) { return; }

    const int n = py.levels();

    // DOWN. The bright pass writes level 0 while halving, so it replaces the
    // first downsample rather than preceding it — one full-resolution read of the
    // scene buffer for the whole chain.
    bright_pass(src, py.level(0), s, exposure);
    for (int i = 1; i < n; ++i)
    {
        downsample(py.level(i - 1), py.level(i));
    }

    // UP. From the smallest level back to level 0, each one added into the level
    // below it. Note that this MUTATES levels 0..n-2, so the pyramid after this
    // call no longer holds a mip chain of the bright pass — level i holds the sum
    // of levels i..n-1. That is exactly what makes level 0 the answer, and it is
    // why the chain cannot be run twice without a fresh bright pass.
    for (int i = n - 1; i > 0; --i)
    {
        upsample_add(py.level(i), py.level(i - 1), s.radius);
    }
}

double total_energy(const hdr_buffer& src)
{
    double sum = 0.0;
    for (int y = 0; y < src.height(); ++y)
    {
        const linear_rgb* row = src.row(y);
        for (int x = 0; x < src.width(); ++x) { sum += static_cast<double>(row[x].r); }
    }
    return sum;
}

} // namespace engine
