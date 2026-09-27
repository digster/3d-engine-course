// engine/src/gfx/texture.cpp — the sampler, in about a hundred lines of arithmetic.
//
// Read `wrap_texel` first, then `to_texel_space`, then the two samplers. Every
// subtlety in this file lives in one of those three places, and none of it is
// subtle once the picture is right.

#include <engine/gfx/texture.hpp>
#include <engine/math/vec3.hpp>   // 6.7: make_normal_bumps works in 3-D

#include <algorithm>
#include <cmath>

namespace engine {

texture::texture(int w, int h, Uint32 fill, texel_space space, alpha_storage storage)
{
    space_ = space;
    storage_ = storage;

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

/// The same debug answer, **fully opaque** — Lesson 6.11.
///
/// Alpha 1 and not 0, and the choice is the whole reason this is a named
/// function rather than a brace initialiser at three call sites. An unbound
/// sampler is a mistake worth seeing; returning coverage 0 would make that
/// mistake *invisible under an alpha test*, which is the one place a debug
/// colour must never hide.
[[nodiscard]] texel_sample unbound_sample()
{
    return {unbound_colour(), 1.0f};
}

/// A stored texel, decoded. **Every** read of the image goes through here, which
/// is what puts the decode before the filter rather than after it.
[[nodiscard]] texel_sample fetch(const texture& image, const sampler& samp, int x, int y)
{
    const int tx = wrap_texel(x, image.width(), samp.address_u);
    const int ty = wrap_texel(y, image.height(), samp.address_v);
    const Uint32 texel = image.texel(tx, ty);

    // LESSON 6.7: ONE BRANCH, IN THE ONE PLACE EVERY READ ALREADY GOES THROUGH.
    // That is the whole cost of giving a texture a colour space, and it is why
    // the decision belongs here rather than at the call site: there is exactly
    // one `fetch`, so there is exactly one place the question can be asked, and
    // no caller can forget to ask it.
    //
    // The branch is on a value that is constant for the whole draw, so it
    // predicts perfectly; §6 measures the cost at the noise floor.
    if (image.space() == texel_space::linear)
    {
        // A byte that is a NUMBER, not a colour. `value / 255`, no curve — which
        // is the same arithmetic an `_UNORM` GPU format performs, as against an
        // `_UNORM_SRGB` one.
        constexpr float inv_255 = 1.0f / 255.0f;
        return {{static_cast<float>((texel >> 16) & 0xFFu) * inv_255,
                 static_cast<float>((texel >> 8) & 0xFFu) * inv_255,
                 static_cast<float>(texel & 0xFFu) * inv_255},
                static_cast<float>((texel >> 24) & 0xFFu) * inv_255};
    }

    // LESSON 6.11. The alpha comes out of BOTH arms by the same `value / 255`,
    // with no curve in either — and that asymmetry is the file's whole thesis in
    // two lines of code. The colour branches on `texel_space` because a colour
    // may or may not have been through the transfer function; the coverage never
    // was, in any image, in any space, because it does not measure light.
    // Lesson 6.7 gave this function its one branch; 6.11 adds a number that
    // deliberately sits outside it.
    constexpr float inv_255 = 1.0f / 255.0f;
    return {to_linear(texel), static_cast<float>((texel >> 24) & 0xFFu) * inv_255};
}

} // namespace

texel_sample sample_nearest_rgba(const texture& image, const sampler& samp, float u, float v)
{
    if (image.empty()) { return unbound_sample(); }

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
        return unbound_sample();
    }

    return fetch(image, samp, static_cast<int>(x), static_cast<int>(y));
}

texel_sample sample_bilinear_rgba(const texture& image, const sampler& samp, float u, float v)
{
    if (image.empty()) { return unbound_sample(); }

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
        return unbound_sample();
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
    const texel_sample c00 = fetch(image, samp, x0,     y0);
    const texel_sample c10 = fetch(image, samp, x0 + 1, y0);
    const texel_sample c01 = fetch(image, samp, x0,     y0 + 1);
    const texel_sample c11 = fetch(image, samp, x0 + 1, y0 + 1);

    // ---- Two lerps along u, one along v -------------------------------------
    //
    // That is all bilinear interpolation is, and the order does not matter: doing
    // v first and u second gives the identical algebraic expression, because both
    // routes multiply out to the same four weights
    // (1-tu)(1-tv), tu(1-tv), (1-tu)tv, tu tv — which sum to 1, which is what makes
    // it an average and not a scaling. Same family as barycentric interpolation
    // (2.3): a weighted average of corner values whose weights sum to one.
    //
    // LESSON 6.11 LERPS FOUR NUMBERS WHERE 3.9 LERPED THREE, and the three
    // colour channels are computed by the identical expression in the identical
    // order — `a + (b - a) * t`, per channel, u first then v. That is not an
    // aesthetic preference: it is what makes this refactor provably free.
    // Floating-point addition is not associative, so reordering these three
    // multiply-adds would move the last bit of a great many texels, and the
    // reference render has been byte-identical since Lesson 5.2. `verify_611` §A
    // checks the two paths against each other exactly; the golden checks the
    // whole picture.
    const auto lerp = [](const texel_sample& a, const texel_sample& b, float t) {
        return texel_sample{{a.colour.r + (b.colour.r - a.colour.r) * t,
                             a.colour.g + (b.colour.g - a.colour.g) * t,
                             a.colour.b + (b.colour.b - a.colour.b) * t},
                            a.alpha + (b.alpha - a.alpha) * t};
    };

    const texel_sample top = lerp(c00, c10, tu);
    const texel_sample bottom = lerp(c01, c11, tu);
    return lerp(top, bottom, tv);
}

texel_sample sample_rgba(const texture& image, const sampler& samp, float u, float v)
{
    return (samp.texel_filter == filter::nearest)
         ? sample_nearest_rgba(image, samp, u, v)
         : sample_bilinear_rgba(image, samp, u, v);
}

// ---- The three-channel entry points, which are now one member access --------
//
// Kept as functions rather than deleted, because they are what nineteen lessons
// of code calls and because dropping a value you do not want is the caller's
// clearest possible statement that it does not want it. The compiler removes the
// alpha arithmetic from these paths entirely — it is dead, and one dead multiply-
// add in a leaf function is exactly the thing a compiler is best at.

linear_rgb sample_nearest(const texture& image, const sampler& samp, float u, float v)
{
    return sample_nearest_rgba(image, samp, u, v).colour;
}

linear_rgb sample_bilinear(const texture& image, const sampler& samp, float u, float v)
{
    return sample_bilinear_rgba(image, samp, u, v).colour;
}

linear_rgb sample(const texture& image, const sampler& samp, float u, float v)
{
    return sample_rgba(image, samp, u, v).colour;
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

// ---- Lesson 6.7: a normal map with an analytic answer ------------------------

texture make_normal_bumps(int size, int cells, float strength)
{
    if (size <= 0 || cells <= 0) { return {}; }

    // LINEAR, and set here rather than by the caller: an image whose meaning is
    // fixed by the function that produced it should arrive knowing what it is.
    texture t(size, size, 0xFF8080FFu, texel_space::linear);

    const float k = 2.0f * 3.14159265358979f * static_cast<float>(cells);
    const float inv = 1.0f / static_cast<float>(size);

    // STRENGTH IS THE MAXIMUM SLOPE, NOT THE AMPLITUDE, and the difference is
    // the whole usability of this function. `h = A cos(ku) cos(kv)` has a peak
    // gradient of `A k`, and `k` grows with the cell count — so a fixed
    // amplitude of 1 at six cells gives a slope of 37.7, which is a surface
    // tilted 88 degrees everywhere. Every normal points sideways, `n.l` is
    // almost zero, and the result is a dark mess that reads as a bug in the
    // shading rather than as an absurd input. (It was, for one build.)
    //
    // Dividing the amplitude by `k` makes `strength` mean something you can
    // reason about: **1.0 is a maximum tilt of 45 degrees**, 0.5 is 27, and the
    // number means the same thing at any cell count. A parameter whose effect
    // changes when you change an unrelated parameter is one nobody can author
    // against.
    const float amplitude = strength / k;

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            // TEXEL CENTRES, not corners — Lesson 3.9's half-texel, and the same
            // rule applies to GENERATING an image as to sampling one. Sample the
            // height field at `i/n` instead and the map is half a texel out of
            // step with the sampler that reads it, which shows as a normal map
            // whose bumps are subtly offset from where the shading expects them.
            const float u = (static_cast<float>(x) + 0.5f) * inv;
            const float v = (static_cast<float>(y) + 0.5f) * inv;

            // h = strength * cos(k u) * cos(k v), so the two partials are
            // straightforward — and the normal of a height field is
            // (-dh/du, -dh/dv, 1), normalised. That form is worth recognising:
            // the surface is the graph of h, its two tangent vectors are
            // (1, 0, dh/du) and (0, 1, dh/dv), and their cross product is
            // exactly that.
            const float dhdu = -amplitude * k * std::sin(k * u) * std::cos(k * v);
            const float dhdv = -amplitude * k * std::cos(k * u) * std::sin(k * v);

            const vec3 n = normalised_or(vec3{-dhdu, -dhdv, 1.0f},
                                         vec3{0.0f, 0.0f, 1.0f});

            // [-1,1] -> [0,255]. The `+ 0.5f` before the cast is rounding, not a
            // fudge: truncation would bias every channel downward by half a code,
            // which on a flat map is the difference between 128 (the lavender
            // that means "no change") and 127 (a surface tilted very slightly,
            // everywhere, in one direction).
            const auto enc = [](float c) {
                const float f = (c * 0.5f + 0.5f) * 255.0f;
                const float clamped = (f < 0.0f) ? 0.0f : (f > 255.0f ? 255.0f : f);
                return static_cast<Uint8>(clamped + 0.5f);
            };

            t.set_texel(x, y, pack_argb(enc(n.x), enc(n.y), enc(n.z)));
        }
    }

    return t;
}

// ---- Lesson 6.6: decoded file pixels -> a samplable texture ------------------

texture to_texture(const image_data& src, texel_space space)
{
    if (!src.valid()) { return {}; }

    // The space travels with the texture from here on. Note what is NOT done: the
    // pixels are not converted. `image_data` holds whatever bytes the file held,
    // and the space says how to READ them — converting on the way in would move
    // the decode earlier without removing it and would lose precision doing it
    // (Lesson 6.1's rule, and 6.5 made the same call for `material::tint`).
    texture out(src.width, src.height, 0xFF000000u, space);

    // Row-major, top row first, in BOTH representations — so the loop is a
    // straight walk with no vertical flip. `image_data` stores what the file
    // contained (5.3) and `texture` puts row 0 at the top (3.9), and those are
    // the same convention. The v flip that OBJ needs lives in the GEOMETRY
    // import, which is the only place it can be decided from: it is the format
    // of the MESH that disagrees about which way v points, not the image.
    for (int y = 0; y < src.height; ++y)
    {
        for (int x = 0; x < src.width; ++x)
        {
            const std::size_t at =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(src.width)
                 + static_cast<std::size_t>(x)) * 4u;

            // THE SHUFFLE. Four bytes in R, G, B, A order become one Uint32 in
            // ARGB8888 order. Written through `pack_argb` rather than as shifts
            // so the packing rule keeps exactly one definition in the engine
            // (1.6) — and so a wholesale memcpy, which would swap red and blue
            // on every little-endian machine, is not even expressible here.
            out.set_texel(x, y, pack_argb(src.pixels[at + 0],
                                          src.pixels[at + 1],
                                          src.pixels[at + 2],
                                          src.pixels[at + 3]));
        }
    }

    return out;
}

} // namespace engine
