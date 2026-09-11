// engine/src/gfx/mipmap.cpp — Lesson 6.10.
//
// The header carries the argument; this file is the arithmetic.

#include <engine/gfx/mipmap.hpp>

#include <algorithm>
#include <cmath>

namespace engine
{
namespace
{

/// Average a 2x2 block of the level above, in the right space.
///
/// THE WHOLE CORRECTNESS STORY OF THIS FILE IS THE `decode` FLAG. Averaging is a
/// linear operation; sRGB is not a linear encoding. The mean of two sRGB codes
/// is not the code of the mean light, and it is always DARKER, because the
/// curve is convex. Skip the decode and every level comes out darker than the
/// one above it — so a textured surface dims as it recedes, and the usual
/// diagnosis is "the lighting falls off too fast", which sends you to look in
/// entirely the wrong file.
[[nodiscard]] Uint32 average_2x2(const texture& src, int x, int y, bool decode, bool weighted)
{
    const int x0 = std::min(2 * x, src.width() - 1);
    const int y0 = std::min(2 * y, src.height() - 1);
    const int x1 = std::min(x0 + 1, src.width() - 1);
    const int y1 = std::min(y0 + 1, src.height() - 1);

    const Uint32 c[4] = { src.texel(x0, y0), src.texel(x1, y0),
                          src.texel(x0, y1), src.texel(x1, y1) };

    // Alpha is averaged as-is in BOTH paths: it is a coverage fraction, not a
    // colour, so it was never sRGB-encoded and decoding it would be a second
    // bug hiding behind the fix for the first.
    float a = 0.0f;
    for (Uint32 t : c) { a += static_cast<float>((t >> 24) & 0xFFu); }
    a *= 0.25f;

    // LESSON 6.11. THE WEIGHTS, and they are the only thing this lesson adds to
    // this function. `weighted` makes each texel contribute in proportion to how
    // much of it there is, which is what "average four texels, three of which are
    // not there" has to mean — and it is arithmetically identical to
    // premultiplying, averaging and dividing back out. That equivalence is why a
    // `premultiplied` texture gets the behaviour unconditionally: its colours are
    // ALREADY scaled by their coverage, so a plain average of them is a weighted
    // average of the colours underneath.
    //
    // The denominator is the sum of the weights, not four, or the result would be
    // darkened by the transparent texels rather than merely uninfluenced by them
    // — which is the same bug in the opposite direction and much easier to write.
    float w[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float wsum = 4.0f;
    if (weighted)
    {
        wsum = 0.0f;
        for (int i = 0; i < 4; ++i)
        {
            w[i] = static_cast<float>((c[i] >> 24) & 0xFFu) * (1.0f / 255.0f);
            wsum += w[i];
        }

        // Four fully transparent texels have nothing to say about colour, and
        // dividing by their total weight would say it with a NaN. Fall back to
        // the unweighted mean, which is the least wrong thing available and is
        // invisible by construction: whatever colour comes out, its alpha is 0.
        if (wsum <= 0.0f)
        {
            w[0] = w[1] = w[2] = w[3] = 1.0f;
            wsum = 4.0f;
        }
    }
    const float inv_w = 1.0f / wsum;

    if (decode)
    {
        linear_rgb sum{0.0f, 0.0f, 0.0f};
        for (int i = 0; i < 4; ++i)
        {
            const linear_rgb l = to_linear(c[i]);
            sum.r += l.r * w[i];
            sum.g += l.g * w[i];
            sum.b += l.b * w[i];
        }
        const linear_rgb mean{sum.r * inv_w, sum.g * inv_w, sum.b * inv_w};
        const Uint32 rgb = to_encoded(mean);
        return (static_cast<Uint32>(a + 0.5f) << 24) | (rgb & 0x00FFFFFFu);
    }

    // Data, not colour — a normal map, a roughness map. These really are linear
    // already (6.7 gave `texel_space` its meaning), so averaging the bytes is
    // the correct operation rather than the lazy one.
    float ch[3] = {0.0f, 0.0f, 0.0f};
    for (int i = 0; i < 4; ++i)
    {
        ch[0] += static_cast<float>((c[i] >> 16) & 0xFFu) * w[i];
        ch[1] += static_cast<float>((c[i] >> 8) & 0xFFu) * w[i];
        ch[2] += static_cast<float>(c[i] & 0xFFu) * w[i];
    }
    return (static_cast<Uint32>(a + 0.5f) << 24)
         | (static_cast<Uint32>(ch[0] * inv_w + 0.5f) << 16)
         | (static_cast<Uint32>(ch[1] * inv_w + 0.5f) << 8)
         | static_cast<Uint32>(ch[2] * inv_w + 0.5f);
}

} // namespace

const texture& mip_chain::level(int i) const
{
    const int n = static_cast<int>(levels_.size());
    const int k = (i < 0) ? 0 : (i >= n ? n - 1 : i);
    return levels_[static_cast<std::size_t>(k)];
}

std::size_t mip_chain::texels() const
{
    std::size_t total = 0;
    for (const texture& t : levels_)
    {
        total += static_cast<std::size_t>(t.width()) * static_cast<std::size_t>(t.height());
    }
    return total;
}

mip_chain build_mips(const texture& base)
{
    return build_mips(base, mip_options{});
}

float coverage_of(const texture& image, float cutoff)
{
    if (image.empty()) { return 0.0f; }

    const std::span<const Uint32> texels = image.texels();
    std::size_t passing = 0;
    for (Uint32 t : texels)
    {
        // `>=`, matching the renderer's own test to the boundary case. A cutoff
        // is a comparison and a comparison has an edge; measuring coverage with
        // `>` while the fill discards with `<` would make this number describe an
        // image the renderer never draws.
        if (static_cast<float>((t >> 24) & 0xFFu) * (1.0f / 255.0f) >= cutoff) { ++passing; }
    }
    return static_cast<float>(passing) / static_cast<float>(texels.size());
}

void rescale_alpha_to_coverage(texture& image, float target, float cutoff)
{
    if (image.empty() || cutoff <= 0.0f) { return; }

    // BISECT ON THE SCALE FACTOR. Coverage is a step function of the scale — it
    // moves only when some texel's scaled alpha crosses the cutoff — so it is
    // monotonically non-decreasing in the scale and has no derivative worth
    // having. That rules out Newton and rules IN bisection, which needs nothing
    // but monotonicity. Ten steps take the bracket [0, 4] down to about 0.004,
    // and the alpha channel has 1/255 of resolution anyway, so more would be
    // measuring the quantisation.
    float lo = 0.0f;
    float hi = 4.0f;
    float best = 1.0f;
    float best_err = 1.0e30f;

    for (int step = 0; step < 10; ++step)
    {
        const float mid = 0.5f * (lo + hi);

        std::size_t passing = 0;
        for (Uint32 t : image.texels())
        {
            const float a = static_cast<float>((t >> 24) & 0xFFu) * (1.0f / 255.0f);
            if (a * mid >= cutoff) { ++passing; }
        }
        const float cov = static_cast<float>(passing)
                        / static_cast<float>(image.texels().size());

        const float err = std::fabs(cov - target);
        if (err < best_err) { best_err = err; best = mid; }

        // Too little coverage means the alphas need scaling UP, which is the
        // direction that is easy to get backwards: a larger scale pushes more
        // texels above the line, so `cov < target` moves the LOW end up.
        if (cov < target) { lo = mid; } else { hi = mid; }
    }

    // Apply the best scale found, not the last one probed. Bisection converges on
    // the crossing, and on a step function the crossing itself may be worse than
    // a point either side of it.
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            const Uint32 t = image.texel(x, y);
            const float a = static_cast<float>((t >> 24) & 0xFFu) * best;
            const Uint32 a8 = static_cast<Uint32>(std::clamp(a + 0.5f, 0.0f, 255.0f));
            image.set_texel(x, y, (a8 << 24) | (t & 0x00FFFFFFu));
        }
    }
}

mip_chain build_mips(const texture& base, const mip_options& opts)
{
    mip_chain chain;
    if (base.empty()) { return chain; }

    chain.levels_.push_back(base);

    // Read the space from the DATA, not from a parameter. A caller who has to
    // remember which of their textures is colour is a caller who will get the
    // third one wrong, and the symptom — a normal map quietly gamma-decoded —
    // is 6.7's bug arriving by a different door.
    const bool decode = (base.space() == texel_space::srgb);

    // LESSON 6.11, and note that the SAME RULE applies to the second property:
    // read it from the data. A premultiplied image's colours are already scaled
    // by their own coverage, so a plain average of them IS the alpha-weighted
    // average of the colours underneath — the weighting is free, and it is free
    // because of how the bytes are stored rather than because of a flag anybody
    // had to remember. `mip_options::alpha_weighted` is what a straight-alpha
    // image needs to catch up.
    const bool weighted = opts.alpha_weighted
                          || (base.storage() == alpha_storage::premultiplied);

    // The coverage every level is rescaled to match. Taken from level 0 ONCE,
    // before any downsampling, because each level must match the ORIGINAL — chain
    // them and the target drifts down the chain exactly as the bug being fixed
    // does, only more slowly.
    const bool preserve = (opts.coverage_cutoff > 0.0f);
    const float target = preserve ? coverage_of(base, opts.coverage_cutoff) : 0.0f;

    while (chain.levels() < k_max_mip_levels)
    {
        const texture& src = chain.levels_.back();
        if (src.width() <= 1 && src.height() <= 1) { break; }

        // Halve, never below 1. A 64x4 texture ends 8x1, 4x1, 2x1, 1x1 — the
        // chain stops when BOTH axes reach 1, not when either does, or a wide
        // thin texture would lose its remaining width.
        const int w = std::max(1, src.width() / 2);
        const int h = std::max(1, src.height() / 2);

        texture dst(w, h, 0xFF000000u, base.space(), base.storage());
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                dst.set_texel(x, y, average_2x2(src, x, y, decode, weighted));
            }
        }

        // AFTER the whole level exists, never per texel: coverage is a property
        // of the level as a set, and the scale that fixes it cannot be known from
        // one 2x2 block. Note also that the rescale reads `dst` and writes `dst`,
        // so the NEXT level averages the RESCALED alphas — which is right, since
        // that is what the renderer will sample.
        if (preserve) { rescale_alpha_to_coverage(dst, target, opts.coverage_cutoff); }

        chain.levels_.push_back(std::move(dst));
    }
    return chain;
}

float mip_level_for(const uv_footprint& fp, int width, int height)
{
    if (width <= 0 || height <= 0) { return 0.0f; }

    const float w = static_cast<float>(width);
    const float h = static_cast<float>(height);

    // Into TEXELS. The footprint arrives in texture units because that is what
    // the interpolator produces; a level is a statement about texels, so the
    // conversion has to happen before the logarithm and not after.
    const float dx = std::sqrt((fp.d_dx.x * w) * (fp.d_dx.x * w)
                             + (fp.d_dx.y * h) * (fp.d_dx.y * h));
    const float dy = std::sqrt((fp.d_dy.x * w) * (fp.d_dy.x * w)
                             + (fp.d_dy.y * h) * (fp.d_dy.y * h));

    // THE LONGER AXIS, and this line is the whole of why isotropic mipmapping
    // over-blurs. Taking the longer guarantees no aliasing along either axis, at
    // the cost of blurring the shorter. Taking the shorter keeps the detail and
    // aliases. Anisotropic filtering is what refuses the choice.
    const float rho = std::max(dx, dy);
    if (rho <= 1.0f) { return 0.0f; }   // magnification: level 0, and no log

    return std::log2(rho);
}

texel_sample sample_mipped_rgba(const mip_chain& chain, const sampler& samp,
                               vec2 uv, const uv_footprint& fp)
{
    if (chain.empty()) { return texel_sample{{0.0f, 0.0f, 0.0f}, 1.0f}; }

    const int w = chain.width();
    const int h = chain.height();
    const float top = static_cast<float>(chain.levels() - 1);

    // ---- Anisotropy ---------------------------------------------------------
    //
    // Take the level the SHORT axis asked for, then walk the LONG axis taking
    // that many samples. The short axis is the one that would have been blurred
    // away by an isotropic choice, so this is the detail coming back.
    const float aniso_max = static_cast<float>(std::clamp(samp.max_anisotropy, 1, 16));
    int taps = 1;
    vec2 step{0.0f, 0.0f};
    float level = mip_level_for(fp, w, h) + samp.mip_bias;

    if (aniso_max > 1.0f)
    {
        const float fw = static_cast<float>(w);
        const float fh = static_cast<float>(h);
        const float len_x = std::sqrt((fp.d_dx.x * fw) * (fp.d_dx.x * fw)
                                    + (fp.d_dx.y * fh) * (fp.d_dx.y * fh));
        const float len_y = std::sqrt((fp.d_dy.x * fw) * (fp.d_dy.x * fw)
                                    + (fp.d_dy.y * fh) * (fp.d_dy.y * fh));

        const float major = std::max(len_x, len_y);
        const float minor = std::min(len_x, len_y);
        if (minor > 1e-6f && major > minor)
        {
            const float ratio = std::min(major / minor, aniso_max);
            taps = std::max(1, static_cast<int>(ratio + 0.5f));

            // The level the MINOR axis wanted — which is exactly `log2(minor)`,
            // the same formula as `mip_level_for` with the other max.
            level = ((minor <= 1.0f) ? 0.0f : std::log2(minor)) + samp.mip_bias;

            // Walk along the major axis, centred on the sample point.
            const vec2 dir = (len_x >= len_y) ? fp.d_dx : fp.d_dy;
            step = vec2{dir.x / ratio, dir.y / ratio};
        }
    }

    level = std::clamp(level, 0.0f, top);

    // One sampler for the level fetches: the mip fields mean nothing to the
    // bilinear fetch inside a level, and passing them through would invite a
    // reader to think they did.
    sampler inner = samp;
    inner.max_anisotropy = 1;

    // LESSON 6.11. Every fetch below carries four numbers where 6.10's carried
    // three, and the three colour channels are combined by the identical
    // expressions in the identical order — so the chain a call site sampled
    // yesterday returns the same bits today. The alpha rides along through the
    // trilinear lerp and the anisotropic sum, which is exactly what makes a
    // mipped cutout dissolve and exactly why `mip_options::coverage_cutoff`
    // exists: nothing here is wrong, and the fraction above the cutoff is not
    // preserved by any of it.
    auto fetch = [&](int lvl, vec2 at) -> texel_sample {
        return sample_rgba(chain.level(lvl), inner, at.x, at.y);
    };

    auto fetch_trilinear = [&](vec2 at) -> texel_sample {
        if (samp.mip_filter == filter::nearest)
        {
            return fetch(static_cast<int>(level + 0.5f), at);
        }
        // TRILINEAR: two bilinear fetches and a lerp. Without it the boundary
        // between two levels is a visible line across the floor — the same
        // artefact Lesson 6.9 met as the cascade seam, cured the same way.
        const int lo = static_cast<int>(level);
        const int hi = std::min(lo + 1, chain.levels() - 1);
        const float f = level - static_cast<float>(lo);
        const texel_sample a = fetch(lo, at);
        if (f <= 0.0f || hi == lo) { return a; }
        const texel_sample b = fetch(hi, at);
        return texel_sample{{a.colour.r + (b.colour.r - a.colour.r) * f,
                             a.colour.g + (b.colour.g - a.colour.g) * f,
                             a.colour.b + (b.colour.b - a.colour.b) * f},
                            a.alpha + (b.alpha - a.alpha) * f};
    };

    if (taps <= 1) { return fetch_trilinear(uv); }

    linear_rgb sum{0.0f, 0.0f, 0.0f};
    float asum = 0.0f;
    const float half = 0.5f * static_cast<float>(taps - 1);
    for (int i = 0; i < taps; ++i)
    {
        const float t = static_cast<float>(i) - half;
        const texel_sample s = fetch_trilinear(vec2{uv.x + step.x * t, uv.y + step.y * t});
        sum.r += s.colour.r;
        sum.g += s.colour.g;
        sum.b += s.colour.b;
        asum += s.alpha;
    }
    const float inv = 1.0f / static_cast<float>(taps);
    return texel_sample{{sum.r * inv, sum.g * inv, sum.b * inv}, asum * inv};
}

linear_rgb sample_mipped(const mip_chain& chain, const sampler& samp,
                         vec2 uv, const uv_footprint& fp)
{
    return sample_mipped_rgba(chain, samp, uv, fp).colour;
}

} // namespace engine
