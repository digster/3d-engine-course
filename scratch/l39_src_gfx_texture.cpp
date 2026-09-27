// src/gfx/texture.cpp — the sampler, in about a hundred lines of arithmetic.
//
// Read `wrap_texel` first, then `to_texel_space`, then the two samplers. Every
// subtlety in this file lives in one of those three places, and none of it is
// subtle once the picture is right.

#include "gfx/texture.hpp"

#include <algorithm>
#include <cmath>

namespace engine {

texture::texture(int w, int h, Uint32 fill)
{
    if (w <= 0 || h <= 0) { return; }   // stays empty; `sample` has an answer for that

    width_ = w;
    height_ = h;
    texels_.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), fill);
}

Uint32 texture::texel(int x, int y) const
{
    if (texels_.empty()) { return pack_argb(0, 0, 0); }

    // The clamp is defence, not addressing — see the header. Addressing already
    // happened in the sampler; by the time an index arrives here it is supposed to
    // be in range, and this makes "supposed to" unnecessary.
    const int cx = std::clamp(x, 0, width_ - 1);
    const int cy = std::clamp(y, 0, height_ - 1);
    return texels_[static_cast<std::size_t>(cy) * static_cast<std::size_t>(width_)
                 + static_cast<std::size_t>(cx)];
}

void texture::set_texel(int x, int y, Uint32 colour)
{
    if (texels_.empty()) { return; }
    if (x < 0 || y < 0 || x >= width_ || y >= height_) { return; }
    texels_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_)
          + static_cast<std::size_t>(x)] = colour;
}

// ---- Addressing --------------------------------------------------------------

int wrap_texel(int i, int n, address_mode mode)
{
    if (n <= 0) { return 0; }

    switch (mode)
    {
    case address_mode::clamp_to_edge:
        return std::clamp(i, 0, n - 1);

    case address_mode::repeat:
    {
        // `%` in C++ truncates toward zero, so `-1 % 8` is `-1` and not `7`. That
        // is the bug this line exists to not have: a texture coordinate goes
        // negative the moment a mesh is offset or a uv is animated, and an index
        // of -1 clamps to the edge texel, which paints a one-texel stripe along
        // exactly the boundary a tiling texture was supposed to cross invisibly.
        //
        // Adding `n` once is enough because `i % n` is already in `(-n, n)`.
        const int m = i % n;
        return (m < 0) ? m + n : m;
    }

    case address_mode::mirrored_repeat:
    {
        // Period 2n: n texels forwards, then the same n backwards. Fold into
        // `[0, 2n)` with the same trick as above, then reflect the upper half.
        const int period = 2 * n;
        int m = i % period;
        if (m < 0) { m += period; }
        return (m < n) ? m : (period - 1 - m);
    }
    }

    return 0;   // unreachable; silences -Wreturn-type on compilers that want it
}

namespace {

/// Where `u` lands in **texel space**: the coordinate in which integers are texel
/// *edges* and `i + 0.5` is the centre of texel `i`.
///
/// This one line is the whole of §4.3. `u = 0` is the left edge of texel 0, `u = 1`
/// is the right edge of texel `n-1`, and the image occupies exactly `n` units.
[[nodiscard]] float to_texel_space(float u, int n)
{
    return u * static_cast<float>(n);
}

/// The debug answer for a sampler with nothing to sample.
[[nodiscard]] linear_rgb unbound_colour()
{
    return to_linear(pack_argb(255, 0, 200));
}

/// A stored texel, decoded. **Every** read of the image goes through here, which
/// is what puts the decode before the filter rather than after it.
[[nodiscard]] linear_rgb fetch(const texture& image, const sampler& samp, int x, int y)
{
    const int tx = wrap_texel(x, image.width(), samp.address_u);
    const int ty = wrap_texel(y, image.height(), samp.address_v);
    return to_linear(image.texel(tx, ty));
}

} // namespace

linear_rgb sample_nearest(const texture& image, const sampler& samp, float u, float v)
{
    if (image.empty()) { return unbound_colour(); }

    // "Which texel does this point land in" — and that question is INDEPENDENT of
    // the half-texel argument, which is why `samp.origin` does not appear in this
    // function at all. Texel `i` covers the interval `[i, i+1)` in texel space no
    // matter where inside it you decide the value lives.
    //
    // `std::floor`, never a cast: a cast truncates toward zero, so u = -0.4 and
    // u = +0.4 would both land in texel 0 and the image would grow a doubled
    // column at the origin. Same argument `checker_at` made in 3.2, and the same
    // reason — texture coordinates go negative routinely.
    const float x = std::floor(to_texel_space(u, image.width()));
    const float y = std::floor(to_texel_space(v, image.height()));

    // A uv that is infinite or NaN reaches here whenever the near plane is not
    // clipped (3.3): the interpolated 1/w passes through zero and its reciprocal
    // is an infinity. Converting that to an int is UNDEFINED BEHAVIOUR, not "a
    // large number", so it is caught rather than clamped. `!(|x| < limit)` and not
    // `|x| >= limit` because every comparison with a NaN is false, and only the
    // negated form catches it.
    constexpr float k_index_limit = 1.0e7f;
    if (!(std::fabs(x) < k_index_limit) || !(std::fabs(y) < k_index_limit))
    {
        return unbound_colour();
    }

    return fetch(image, samp, static_cast<int>(x), static_cast<int>(y));
}

linear_rgb sample_bilinear(const texture& image, const sampler& samp, float u, float v)
{
    if (image.empty()) { return unbound_colour(); }

    // ---- The half texel (§4.3) ---------------------------------------------
    //
    // Bilinear asks a different question from nearest: not "which texel is this
    // in" but "which two texel CENTRES does this lie between". Texel i's centre is
    // at i + 0.5 in texel space, so subtracting the half shifts into a coordinate
    // where the centres are the integers — and then `floor` picks the left one and
    // the fraction is the weight, with no special cases at all.
    //
    // `texel_origin::corner` is that subtraction left out. It is exactly the
    // version you write if you think of a texel as a little square, and it is
    // wrong by half a texel everywhere: at a texel centre, where the answer should
    // be that texel exactly, it lands halfway between two and returns their
    // average.
    const float half = (samp.origin == texel_origin::centre) ? 0.5f : 0.0f;

    const float x = to_texel_space(u, image.width()) - half;
    const float y = to_texel_space(v, image.height()) - half;

    constexpr float k_index_limit = 1.0e7f;
    if (!(std::fabs(x) < k_index_limit) || !(std::fabs(y) < k_index_limit))
    {
        return unbound_colour();
    }

    const float fx = std::floor(x);
    const float fy = std::floor(y);

    const int x0 = static_cast<int>(fx);
    const int y0 = static_cast<int>(fy);

    // The weights, each in [0, 1). `x - floor(x)` and not `fmod`, because this is
    // the *distance past* the left centre, which is what the lerp wants.
    const float tu = x - fx;
    const float tv = y - fy;

    // ---- Four fetches, each addressed INDEPENDENTLY -------------------------
    //
    // The order matters and the failure is invisible until it is not. Wrapping the
    // COORDINATE first and then taking neighbours would put both neighbours inside
    // the image, so a repeating texture would blend the last texel with the last
    // texel instead of with the first — a hairline seam along every tile boundary,
    // present only in the filtered mode, and easy to blame on the geometry.
    // Addressing each of the four indices separately is what makes `repeat`
    // actually seamless and `clamp_to_edge` actually smear.
    const linear_rgb c00 = fetch(image, samp, x0,     y0);
    const linear_rgb c10 = fetch(image, samp, x0 + 1, y0);
    const linear_rgb c01 = fetch(image, samp, x0,     y0 + 1);
    const linear_rgb c11 = fetch(image, samp, x0 + 1, y0 + 1);

    // ---- Two lerps along u, one along v -------------------------------------
    //
    // That is all bilinear interpolation is, and the order does not matter: doing
    // v first and u second gives the identical algebraic expression, because both
    // routes multiply out to the same four weights
    // (1-tu)(1-tv), tu(1-tv), (1-tu)tv, tu tv — which sum to 1, which is what makes
    // it an average and not a scaling. Same family as barycentric interpolation
    // (2.3): a weighted average of corner values whose weights sum to one.
    const auto lerp = [](const linear_rgb& a, const linear_rgb& b, float t) {
        return linear_rgb{a.r + (b.r - a.r) * t,
                          a.g + (b.g - a.g) * t,
                          a.b + (b.b - a.b) * t};
    };

    const linear_rgb top = lerp(c00, c10, tu);
    const linear_rgb bottom = lerp(c01, c11, tu);
    return lerp(top, bottom, tv);
}

linear_rgb sample(const texture& image, const sampler& samp, float u, float v)
{
    return (samp.texel_filter == filter::nearest)
         ? sample_nearest(image, samp, u, v)
         : sample_bilinear(image, samp, u, v);
}

// ---- Generated test images ---------------------------------------------------

texture make_checker(int size, int cells, Uint32 a, Uint32 b)
{
    texture t(size, size);
    if (t.empty() || cells <= 0) { return t; }

    // Integer arithmetic throughout, so the squares land on exact texel boundaries
    // whenever `size` divides by `cells` and land *predictably* when it does not.
    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            const int cx = x * cells / size;
            const int cy = y * cells / size;
            t.set_texel(x, y, ((cx + cy) % 2 == 0) ? a : b);
        }
    }
    return t;
}

texture make_uv_grid(int size)
{
    texture t(size, size);
    if (t.empty()) { return t; }

    // Four quadrants, each an unmistakable hue. A checkerboard is symmetric under
    // every flip and rotation and therefore cannot tell you your v is upside down;
    // this can, from across the room.
    constexpr Uint32 k_top_left     = 0xFFD8484Cu;   // red
    constexpr Uint32 k_top_right    = 0xFF5FBF6Au;   // green
    constexpr Uint32 k_bottom_left  = 0xFF4C7FD8u;   // blue
    constexpr Uint32 k_bottom_right = 0xFFE0B24Cu;   // amber

    const int half = size / 2;

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            const bool left = (x < half);
            const bool top = (y < half);
            Uint32 c = top ? (left ? k_top_left : k_top_right)
                           : (left ? k_bottom_left : k_bottom_right);

            // An eighth-scale grid, so magnification and minification both have
            // something with a known spacing to distort. Drawn by darkening rather
            // than by overwriting, so the quadrant colour still reads underneath.
            const int step = std::max(1, size / 8);
            if (x % step == 0 || y % step == 0)
            {
                const linear_rgb light = to_linear(c);
                c = to_encoded({light.r * 0.45f, light.g * 0.45f, light.b * 0.45f});
            }

            t.set_texel(x, y, c);
        }
    }

    // The corner mark: a solid white block in the TOP-LEFT, which is (u, v) =
    // (0, 0) under SDL_GPU's convention. One asymmetric feature is all it takes to
    // read off an orientation, and unlike the quadrants it survives being sampled
    // at a single point.
    const int mark = std::max(2, size / 8);
    for (int y = 0; y < mark; ++y)
    {
        for (int x = 0; x < mark; ++x)
        {
            t.set_texel(x, y, 0xFFF2EFE8u);
        }
    }

    return t;
}

} // namespace engine
