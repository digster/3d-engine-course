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
[[nodiscard]] Uint32 average_2x2(const texture& src, int x, int y, bool decode)
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

    if (decode)
    {
        linear_rgb sum{0.0f, 0.0f, 0.0f};
        for (Uint32 t : c)
        {
            const linear_rgb l = to_linear(t);
            sum.r += l.r;
            sum.g += l.g;
            sum.b += l.b;
        }
        const linear_rgb mean{sum.r * 0.25f, sum.g * 0.25f, sum.b * 0.25f};
        const Uint32 rgb = to_encoded(mean);
        return (static_cast<Uint32>(a + 0.5f) << 24) | (rgb & 0x00FFFFFFu);
    }

    // Data, not colour — a normal map, a roughness map. These really are linear
    // already (6.7 gave `texel_space` its meaning), so averaging the bytes is
    // the correct operation rather than the lazy one.
    float ch[3] = {0.0f, 0.0f, 0.0f};
    for (Uint32 t : c)
    {
        ch[0] += static_cast<float>((t >> 16) & 0xFFu);
        ch[1] += static_cast<float>((t >> 8) & 0xFFu);
        ch[2] += static_cast<float>(t & 0xFFu);
    }
    return (static_cast<Uint32>(a + 0.5f) << 24)
         | (static_cast<Uint32>(ch[0] * 0.25f + 0.5f) << 16)
         | (static_cast<Uint32>(ch[1] * 0.25f + 0.5f) << 8)
         | static_cast<Uint32>(ch[2] * 0.25f + 0.5f);
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
    mip_chain chain;
    if (base.empty()) { return chain; }

    chain.levels_.push_back(base);

    // Read the space from the DATA, not from a parameter. A caller who has to
    // remember which of their textures is colour is a caller who will get the
    // third one wrong, and the symptom — a normal map quietly gamma-decoded —
    // is 6.7's bug arriving by a different door.
    const bool decode = (base.space() == texel_space::srgb);

    while (chain.levels() < k_max_mip_levels)
    {
        const texture& src = chain.levels_.back();
        if (src.width() <= 1 && src.height() <= 1) { break; }

        // Halve, never below 1. A 64x4 texture ends 8x1, 4x1, 2x1, 1x1 — the
        // chain stops when BOTH axes reach 1, not when either does, or a wide
        // thin texture would lose its remaining width.
        const int w = std::max(1, src.width() / 2);
        const int h = std::max(1, src.height() / 2);

        texture dst(w, h, 0xFF000000u, base.space());
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                dst.set_texel(x, y, average_2x2(src, x, y, decode));
            }
        }
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

linear_rgb sample_mipped(const mip_chain& chain, const sampler& samp,
                         vec2 uv, const uv_footprint& fp)
{
    if (chain.empty()) { return linear_rgb{0.0f, 0.0f, 0.0f}; }

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

    auto fetch = [&](int lvl, vec2 at) -> linear_rgb {
        return sample(chain.level(lvl), inner, at.x, at.y);
    };

    auto fetch_trilinear = [&](vec2 at) -> linear_rgb {
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
        const linear_rgb a = fetch(lo, at);
        if (f <= 0.0f || hi == lo) { return a; }
        const linear_rgb b = fetch(hi, at);
        return linear_rgb{a.r + (b.r - a.r) * f,
                          a.g + (b.g - a.g) * f,
                          a.b + (b.b - a.b) * f};
    };

    if (taps <= 1) { return fetch_trilinear(uv); }

    linear_rgb sum{0.0f, 0.0f, 0.0f};
    const float half = 0.5f * static_cast<float>(taps - 1);
    for (int i = 0; i < taps; ++i)
    {
        const float t = static_cast<float>(i) - half;
        const linear_rgb s = fetch_trilinear(vec2{uv.x + step.x * t, uv.y + step.y * t});
        sum.r += s.r;
        sum.g += s.g;
        sum.b += s.b;
    }
    const float inv = 1.0f / static_cast<float>(taps);
    return linear_rgb{sum.r * inv, sum.g * inv, sum.b * inv};
}

} // namespace engine
