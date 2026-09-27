// engine/src/gfx/font.cpp — the one translation unit that contains stb_truetype.
//
// Lesson 6.18. The same isolation `image.cpp` (4.7) and `gltf.cpp` (6.6) apply,
// for the same reason and with the same receipt: `STB_TRUETYPE_IMPLEMENTATION` is
// defined here and nowhere else, `engine/CMakeLists.txt` puts `${stb_SOURCE_DIR}`
// on this library's PRIVATE include path, and `font.hpp` names no third-party
// type. A third-party library reaches exactly as far into a codebase as its types
// appear in headers, and stb_truetype's reach is this file.

#include <engine/gfx/font.hpp>

#include <engine/core/log.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>

// Third-party code compiled with our warning flags, which it was never written
// for. Silence rather than edit: an edited dependency is one you can no longer
// update. (`-Wcast-qual` fires on stb_truetype's internal const-stripping, which
// is safe there and not ours to fix.)
#if defined(__clang__) || defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wsign-compare"
#  pragma GCC diagnostic ignored "-Wcast-qual"
#  pragma GCC diagnostic ignored "-Wunused-function"
#endif
#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable : 4244 4245 4456 4996)
#endif

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC          // keep every symbol internal to this object file
#include "stb_truetype.h"

#if defined(__clang__) || defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
#if defined(_MSC_VER)
#  pragma warning(pop)
#endif

namespace engine {

const char* name_of(font_status s)
{
    switch (s)
    {
    case font_status::ok:              return "ok";
    case font_status::cannot_open:     return "cannot_open";
    case font_status::not_a_font:      return "not_a_font";
    case font_status::bad_options:     return "bad_options";
    case font_status::atlas_too_small: return "atlas_too_small";
    case font_status::no_glyphs:       return "no_glyphs";
    }
    return "?";
}

// ---- font_atlas accessors ---------------------------------------------------

const glyph* font_atlas::find(char32_t cp) const
{
    // Dense range, so this is a subtraction and a bounds check. The unsigned
    // arithmetic makes one comparison do both ends: a codepoint below the first
    // wraps to something enormous.
    const std::uint32_t index = static_cast<std::uint32_t>(cp)
                              - static_cast<std::uint32_t>(first_codepoint);
    if (index >= glyphs.size()) { return nullptr; }
    return &glyphs[index];
}

float font_atlas::kern(char32_t a, char32_t b) const
{
    if (kerning.empty()) { return 0.0f; }

    // Sorted by (first, second), so this is a binary search over the 2% of pairs
    // that are non-zero. `std::lower_bound` wants a strict weak ordering and
    // the comparator has to be the SAME one the table was sorted with — writing
    // it twice is how a lookup silently starts returning zero for half the
    // table, so the comparison lives in one place.
    const kern_pair key{a, b, 0.0f};
    const auto less = [](const kern_pair& p, const kern_pair& q)
    {
        return (p.first != q.first) ? (p.first < q.first) : (p.second < q.second);
    };
    const auto it = std::lower_bound(kerning.begin(), kerning.end(), key, less);
    if (it == kerning.end() || it->first != a || it->second != b) { return 0.0f; }
    return it->advance;
}

std::uint8_t font_atlas::texel(int x, int y) const
{
    if (x < 0 || y < 0 || x >= width || y >= height) { return 0; }
    return coverage[static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
                  + static_cast<std::size_t>(x)];
}

// ---- The packer -------------------------------------------------------------
//
// SHELF PACKING, and it is worth saying why this simple algorithm is close to
// optimal HERE when it is mediocre in general.
//
// A shelf packer sorts rectangles by height, then lays them left to right in
// rows ("shelves") whose height is set by the first rectangle placed on them.
// Its waste is the vertical slack under short rectangles on a tall shelf — so it
// is bad when heights vary wildly and good when they cluster. Glyph heights at
// one size cluster hard: for printable ASCII in Karla at 16 px, every glyph is
// between 3 and 14 texels tall, and sorting puts the 14s together and the 3s
// together. §5 measures the occupancy that results and compares it against the
// floor (the sum of the padded glyph areas), which is the only honest way to
// judge a packer: against what it could not have beaten, not against a feeling.
//
// The alternative worth knowing is skyline packing (Jylanki, 2010), which tracks
// a piecewise-constant upper envelope instead of rows. It wins on mixed-size
// input — a sprite atlas — and buys almost nothing on a font.

namespace {

/// One glyph's rasterized bitmap, before it has a home in the atlas.
struct baked_bitmap
{
    int codepoint = 0;
    int w = 0, h = 0;
    int offset_x = 0, offset_y = 0;
    float advance = 0.0f;
    std::vector<std::uint8_t> pixels;   ///< w*h coverage bytes
};

/// The side of the fully-covered block reserved for untextured rectangles.
/// Two, for the reason `font_atlas::solid_u` gives.
constexpr int k_solid_side = 2;

/// Round up to the next power of two, at least 16.
int next_pow2(int n)
{
    int v = 16;
    while (v < n) { v <<= 1; }
    return v;
}

/// Lay the bitmaps out in `atlas_w` x `atlas_h`, tallest first. Returns false if
/// they do not fit; fills `placement` with one (x, y) per entry of `order`.
bool shelf_pack(const std::vector<baked_bitmap>& glyphs, const std::vector<int>& order,
                int pad, int atlas_w, int atlas_h, int reserve,
                std::vector<std::pair<int, int>>& placement, int& used_height)
{
    placement.assign(glyphs.size(), {0, 0});

    int shelf_y = pad;        // top of the current shelf
    // `reserve` is the solid block's side, parked at the top-left of shelf 0.
    // Seeding `shelf_h` with it is what stops a font whose tallest glyph is
    // shorter than the block from overlapping it — which no real font at a
    // usable size does, and which costs one assignment to make impossible.
    int shelf_h = reserve;
    int pen_x = pad + (reserve > 0 ? reserve + pad : 0);

    for (const int i : order)
    {
        const baked_bitmap& g = glyphs[i];
        if (g.w == 0 || g.h == 0)
        {
            // A space. No rectangle, no placement, and deliberately not given a
            // 1x1 slot: a zero-area quad is never emitted, so nothing samples it.
            placement[static_cast<std::size_t>(i)] = {0, 0};
            continue;
        }

        if (pen_x + g.w + pad > atlas_w)
        {
            // New shelf. `shelf_h` is the tallest glyph on the one we are
            // leaving, which is the first one placed on it because the order is
            // sorted by descending height.
            shelf_y += shelf_h + pad;
            shelf_h = 0;
            pen_x = pad;
        }
        if (shelf_y + g.h + pad > atlas_h) { return false; }

        placement[static_cast<std::size_t>(i)] = {pen_x, shelf_y};
        pen_x += g.w + pad;
        shelf_h = std::max(shelf_h, g.h);
        used_height = shelf_y + shelf_h + pad;
    }
    return true;
}

} // namespace

font_status bake_font(const std::uint8_t* ttf, std::size_t bytes,
                      const font_bake_options& options, font_atlas& out)
{
    out = font_atlas{};

    if (ttf == nullptr || bytes == 0) { return font_status::cannot_open; }
    if (options.pixel_height <= 0.0f || options.codepoint_count <= 0
        || options.padding < 0 || options.max_atlas_width < 16)
    {
        ENGINE_LOG_ERROR(log_asset, "bake_font: nonsense options (height %.2f, count %d, pad %d)",
                         static_cast<double>(options.pixel_height), options.codepoint_count,
                         options.padding);
        return font_status::bad_options;
    }

    stbtt_fontinfo info{};
    // Offset for font index 0. A .ttc collection holds several faces and
    // `stbtt_GetFontOffsetForIndex` is how you reach the others; we take the
    // first and say so rather than pretending collections do not exist.
    const int offset = stbtt_GetFontOffsetForIndex(ttf, 0);
    if (offset < 0 || !stbtt_InitFont(&info, ttf, offset))
    {
        ENGINE_LOG_ERROR(log_asset, "bake_font: not a TrueType font (%zu bytes)", bytes);
        return font_status::not_a_font;
    }

    // SCALE. `ScaleForPixelHeight` maps (ascent - descent) to `pixel_height`, so
    // the em square comes out SMALLER than the number asked for by whatever the
    // font's ascent+descent exceeds 1 em — 1.169 for Karla, giving a 13.69 px em
    // for a "16 px" font. The alternative, `ScaleForMappingEmToPixels`, maps the
    // em square instead and makes different fonts agree on size while
    // disagreeing on how much vertical room they need. Neither is wrong; this
    // one is what a UI toolkit means by "16 px".
    const float scale = stbtt_ScaleForPixelHeight(&info, options.pixel_height);

    int ascent_u = 0, descent_u = 0, line_gap_u = 0;
    stbtt_GetFontVMetrics(&info, &ascent_u, &descent_u, &line_gap_u);
    out.pixel_height = options.pixel_height;
    out.ascent = static_cast<float>(ascent_u) * scale;
    out.descent = static_cast<float>(descent_u) * scale;
    out.line_gap = static_cast<float>(line_gap_u) * scale;
    out.padding = options.padding;
    out.first_codepoint = options.first_codepoint;

    // ---- Rasterize every glyph, before deciding where any of them go --------
    //
    // Two phases rather than one, because the packer needs every rectangle's
    // size before it can place the first one. The memory cost is the glyphs
    // twice over for the duration of the bake, which for ASCII at 16 px is 9 kB.
    std::vector<baked_bitmap> bitmaps;
    bitmaps.resize(static_cast<std::size_t>(options.codepoint_count));

    int missing = 0;
    for (int i = 0; i < options.codepoint_count; ++i)
    {
        const int cp = static_cast<int>(options.first_codepoint) + i;
        baked_bitmap& b = bitmaps[static_cast<std::size_t>(i)];
        b.codepoint = cp;

        if (stbtt_FindGlyphIndex(&info, cp) == 0)
        {
            // The font does not carry this codepoint. Counted, and left with a
            // zero advance so that a caller who draws it anyway gets a visible
            // pile-up rather than a plausible-looking wrong string.
            ++missing;
            continue;
        }

        int advance_u = 0, lsb_u = 0;
        stbtt_GetCodepointHMetrics(&info, cp, &advance_u, &lsb_u);
        b.advance = static_cast<float>(advance_u) * scale;

        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        stbtt_GetCodepointBitmapBox(&info, cp, scale, scale, &x0, &y0, &x1, &y1);
        b.w = x1 - x0;
        b.h = y1 - y0;
        b.offset_x = x0;
        b.offset_y = y0;

        if (b.w > 0 && b.h > 0)
        {
            b.pixels.resize(static_cast<std::size_t>(b.w) * static_cast<std::size_t>(b.h));
            // `MakeCodepointBitmap` writes coverage, one byte per texel, with
            // the glyph's own top-left at (0, 0) of the destination — which is
            // why the box above had to be queried first and its offsets kept.
            stbtt_MakeCodepointBitmap(&info, b.pixels.data(), b.w, b.h, /*stride*/ b.w,
                                      scale, scale, cp);
        }
    }
    out.missing_glyphs = missing;
    if (missing == options.codepoint_count)
    {
        ENGINE_LOG_ERROR(log_asset, "bake_font: the font carries none of U+%04X..U+%04X",
                         static_cast<unsigned>(options.first_codepoint),
                         static_cast<unsigned>(options.first_codepoint)
                             + static_cast<unsigned>(options.codepoint_count) - 1u);
        return font_status::no_glyphs;
    }

    // ---- Order by descending height, then pack ------------------------------
    std::vector<int> order;
    order.reserve(bitmaps.size());
    for (int i = 0; i < options.codepoint_count; ++i) { order.push_back(i); }
    std::sort(order.begin(), order.end(), [&bitmaps](int a, int b)
    {
        const baked_bitmap& ga = bitmaps[static_cast<std::size_t>(a)];
        const baked_bitmap& gb = bitmaps[static_cast<std::size_t>(b)];
        // Height first; codepoint as the tie-break, so the packing is
        // DETERMINISTIC. Two runs of the bake must produce byte-identical
        // atlases or a golden image test over text is a coin flip — the same
        // argument Lesson 6.17 made about the frame graph's topological order.
        if (ga.h != gb.h) { return ga.h > gb.h; }
        return ga.codepoint < gb.codepoint;
    });

    // The floor: the area every glyph needs including its own padding. Grow the
    // square from there rather than from an arbitrary 512, so the atlas is the
    // smallest power of two that could possibly work before the first attempt.
    std::size_t needed = static_cast<std::size_t>(k_solid_side + options.padding)
                       * static_cast<std::size_t>(k_solid_side + options.padding);
    for (const baked_bitmap& b : bitmaps)
    {
        if (b.w > 0 && b.h > 0)
        {
            needed += static_cast<std::size_t>(b.w + options.padding)
                    * static_cast<std::size_t>(b.h + options.padding);
        }
    }

    std::vector<std::pair<int, int>> placement;
    int used_height = 0;
    int side = next_pow2(static_cast<int>(std::ceil(std::sqrt(static_cast<double>(needed)))));
    bool packed = false;
    while (side <= options.max_atlas_width)
    {
        if (shelf_pack(bitmaps, order, options.padding, side, side, k_solid_side, placement,
                       used_height))
        {
            packed = true;
            break;
        }
        side <<= 1;
    }
    if (!packed)
    {
        ENGINE_LOG_ERROR(log_asset, "bake_font: %zu texels of glyphs will not fit in %dx%d",
                         needed, options.max_atlas_width, options.max_atlas_width);
        return font_status::atlas_too_small;
    }

    // ---- Blit the bitmaps into the atlas ------------------------------------
    out.width = side;
    out.height = side;
    out.coverage.assign(static_cast<std::size_t>(side) * static_cast<std::size_t>(side), 0u);
    out.glyphs.resize(static_cast<std::size_t>(options.codepoint_count));

    // The solid block, at the top-left of the first shelf, matching where
    // `shelf_pack` reserved it. Its uv is the block's CENTRE, so a bilinear tap
    // has half a texel of slack on every side.
    for (int row = 0; row < k_solid_side; ++row)
    {
        for (int col = 0; col < k_solid_side; ++col)
        {
            out.coverage[static_cast<std::size_t>(options.padding + row)
                             * static_cast<std::size_t>(side)
                         + static_cast<std::size_t>(options.padding + col)] = 255u;
        }
    }
    out.solid_u = (static_cast<float>(options.padding) + 0.5f * k_solid_side)
                / static_cast<float>(side);
    out.solid_v = out.solid_u;   // square atlas, same coordinate

    int packed_texels = (k_solid_side + options.padding) * (k_solid_side + options.padding);
    for (int i = 0; i < options.codepoint_count; ++i)
    {
        const baked_bitmap& b = bitmaps[static_cast<std::size_t>(i)];
        glyph& g = out.glyphs[static_cast<std::size_t>(i)];
        g.advance = b.advance;
        g.offset_x = static_cast<float>(b.offset_x);
        g.offset_y = static_cast<float>(b.offset_y);

        if (b.w == 0 || b.h == 0) { continue; }

        const auto [px, py] = placement[static_cast<std::size_t>(i)];
        g.x0 = static_cast<std::uint16_t>(px);
        g.y0 = static_cast<std::uint16_t>(py);
        g.x1 = static_cast<std::uint16_t>(px + b.w);
        g.y1 = static_cast<std::uint16_t>(py + b.h);

        for (int row = 0; row < b.h; ++row)
        {
            std::memcpy(&out.coverage[static_cast<std::size_t>(py + row)
                                          * static_cast<std::size_t>(side)
                                      + static_cast<std::size_t>(px)],
                        &b.pixels[static_cast<std::size_t>(row) * static_cast<std::size_t>(b.w)],
                        static_cast<std::size_t>(b.w));
        }
        packed_texels += (b.w + options.padding) * (b.h + options.padding);
    }
    out.packed_texels = packed_texels;
    out.used_height = used_height;

    // ---- Kerning ------------------------------------------------------------
    //
    // ONE QUERY PER ORDERED PAIR, and the pairs that come back non-zero are
    // kept. This looks wasteful — 9,025 queries for ASCII — and is microseconds,
    // because each is a lookup in a table the font already has.
    //
    // WHERE THE DATA LIVES IS THE INTERESTING PART. There are two mechanisms: the
    // legacy `kern` table, and `GPOS`, the OpenType layout table. Every font
    // shipped in the last fifteen years puts pair kerning in `GPOS` and many drop
    // `kern` entirely — the font this engine ships has NO `kern` table at all.
    // stb_truetype reads a useful subset of GPOS pair positioning (since 1.19),
    // which is why this works; a version before that, or a naive hand-rolled
    // reader of `kern`, returns zero for every pair in a modern font and the
    // symptom is "kerning does nothing", with no error to chase.
    if (options.kerning)
    {
        for (int a = 0; a < options.codepoint_count; ++a)
        {
            for (int b = 0; b < options.codepoint_count; ++b)
            {
                const int cp_a = static_cast<int>(options.first_codepoint) + a;
                const int cp_b = static_cast<int>(options.first_codepoint) + b;
                const int k = stbtt_GetCodepointKernAdvance(&info, cp_a, cp_b);
                if (k == 0) { continue; }
                out.kerning.push_back({static_cast<char32_t>(cp_a),
                                       static_cast<char32_t>(cp_b),
                                       static_cast<float>(k) * scale});
            }
        }
        // Already generated in (first, second) order by the nested loops, but
        // sorted explicitly anyway: `kern()` binary-searches, and an ordering
        // that is true by accident of the loop nesting is one refactor away from
        // being false with no failing test.
        std::sort(out.kerning.begin(), out.kerning.end(),
                  [](const kern_pair& p, const kern_pair& q)
                  {
                      return (p.first != q.first) ? (p.first < q.first)
                                                  : (p.second < q.second);
                  });
    }

    // THE OCCUPANCY REPORTED IS THE PACKER'S, over the rows it touched — not the
    // glyph area over the whole atlas, which is a fact about the font.
    ENGINE_LOG_INFO(log_asset,
                    "font baked: %dx%d atlas (%d rows used), %d glyphs (%d missing), "
                    "%zu kern pairs, %.1f%% shelf efficiency, %.2f px line height",
                    out.width, out.height, used_height, options.codepoint_count, missing,
                    out.kerning.size(),
                    (used_height > 0)
                        ? 100.0 * static_cast<double>(packed_texels)
                              / (static_cast<double>(side) * static_cast<double>(used_height))
                        : 0.0,
                    static_cast<double>(out.line_height()));
    return font_status::ok;
}

font_status load_font(const char* path, const font_bake_options& options, font_atlas& out)
{
    out = font_atlas{};
    if (path == nullptr) { return font_status::cannot_open; }

    // SDL_LoadFile rather than <cstdio>, for the reason `image.cpp` gives: it is
    // the one path in this engine that knows about SDL_GetBasePath, and a second
    // notion of the working directory is how an asset resolves in the editor and
    // not in the shipped game.
    std::size_t bytes = 0;
    void* data = SDL_LoadFile(path, &bytes);
    if (data == nullptr)
    {
        ENGINE_LOG_ERROR(log_asset, "load_font: cannot read '%s': %s", path, SDL_GetError());
        return font_status::cannot_open;
    }

    const font_status status = bake_font(static_cast<const std::uint8_t*>(data), bytes,
                                         options, out);
    SDL_free(data);
    return status;
}

// ---- UTF-8 ------------------------------------------------------------------

char32_t next_codepoint(const char*& p, const char* end)
{
    constexpr char32_t k_replacement = 0xFFFDu;
    if (p >= end) { return 0; }

    const auto byte = [](const char* q) { return static_cast<std::uint8_t>(*q); };
    const std::uint8_t b0 = byte(p);

    // ASCII: one byte, high bit clear. The overwhelmingly common case, so it is
    // first and costs one comparison.
    if (b0 < 0x80u) { ++p; return static_cast<char32_t>(b0); }

    int extra = 0;
    char32_t cp = 0;
    if ((b0 & 0xE0u) == 0xC0u)      { extra = 1; cp = b0 & 0x1Fu; }
    else if ((b0 & 0xF0u) == 0xE0u) { extra = 2; cp = b0 & 0x0Fu; }
    else if ((b0 & 0xF8u) == 0xF0u) { extra = 3; cp = b0 & 0x07u; }
    else { ++p; return k_replacement; }   // a continuation byte, or 0xF8+

    if (p + 1 + extra > end) { ++p; return k_replacement; }
    for (int i = 1; i <= extra; ++i)
    {
        if ((byte(p + i) & 0xC0u) != 0x80u) { ++p; return k_replacement; }
        cp = (cp << 6) | (byte(p + i) & 0x3Fu);
    }

    // THE THREE RULES MOST HAND-ROLLED DECODERS OMIT, and the first is a
    // security bug rather than a rendering one.
    //
    //  (a) OVER-LONG FORMS. 0xC0 0xAF decodes to U+002F '/' by the arithmetic
    //      above. A path check that rejects "/" and then decodes would be
    //      walked straight past. A codepoint must use the shortest sequence
    //      that can hold it.
    //  (b) SURROGATES. U+D800..U+DFFF are UTF-16 machinery and are not
    //      characters; encoded in UTF-8 they are ill-formed ("CESU-8").
    //  (c) BEYOND U+10FFFF. The four-byte form can express up to U+1FFFFF;
    //      Unicode stops at U+10FFFF.
    static constexpr char32_t k_min[4] = {0, 0x80u, 0x800u, 0x10000u};
    if (cp < k_min[extra]) { ++p; return k_replacement; }
    if (cp >= 0xD800u && cp <= 0xDFFFu) { ++p; return k_replacement; }
    if (cp > 0x10FFFFu) { ++p; return k_replacement; }

    p += 1 + extra;
    return cp;
}

// ---- Layout -----------------------------------------------------------------

text_metrics layout_text(const font_atlas& atlas, std::string_view utf8, vec2 pen,
                         const text_layout_options& options, std::vector<glyph_quad>& out)
{
    text_metrics m{};
    if (!atlas.valid()) { return m; }

    // TABULAR DIGITS need the widest digit's advance, and it is computed here
    // rather than baked because it depends on nothing but the atlas — and an
    // atlas field that is only meaningful when a *layout* option is set belongs
    // to the layout. Ten `find` calls; the compiler hoists it out of nothing,
    // but the loop below runs once per string, not once per glyph.
    float digit_pitch = 0.0f;
    if (options.tabular_digits)
    {
        for (char32_t d = U'0'; d <= U'9'; ++d)
        {
            if (const glyph* g = atlas.find(d)) { digit_pitch = std::max(digit_pitch, g->advance); }
        }
    }

    const float inv_w = 1.0f / static_cast<float>(atlas.width);
    const float inv_h = 1.0f / static_cast<float>(atlas.height);
    const float line_start_x = pen.x;

    float x = pen.x;
    float y = pen.y;
    bool any_ink = false;
    char32_t previous = 0;

    const char* p = utf8.data();
    const char* const end = p + utf8.size();
    while (p < end)
    {
        const char32_t cp = next_codepoint(p, end);
        if (cp == 0) { break; }
        ++m.codepoints;

        if (cp == U'\n')
        {
            x = line_start_x;
            y += atlas.line_height();
            ++m.lines;
            previous = 0;             // no kerning across a line break
            continue;
        }
        if (cp == U'\r') { continue; }
        if (cp == U'\t')
        {
            // Advance to the next multiple of `tab_stop` measured from where
            // this LINE started, which is what makes columns align. Measured
            // from the pen's absolute x and two indented blocks disagree.
            const float rel = x - line_start_x;
            const float next = std::floor(rel / options.tab_stop + 1.0f) * options.tab_stop;
            x = line_start_x + next;
            previous = 0;
            continue;
        }

        const glyph* g = atlas.find(cp);
        if (g == nullptr)
        {
            ++m.missing;
            previous = 0;
            continue;
        }

        // KERNING IS APPLIED BEFORE THE GLYPH IS PLACED, not after the previous
        // one advanced — they are the same arithmetic, and doing it here keeps
        // the rule "the pen is where this glyph starts" true at the one point
        // that matters, which is the snap below.
        if (options.kerning && previous != 0) { x += atlas.kern(previous, cp); }

        const float advance = (options.tabular_digits && cp >= U'0' && cp <= U'9')
                                ? digit_pitch
                                : g->advance;

        if (g->has_ink())
        {
            // THE SNAP, and note what is snapped: the glyph's top-left corner,
            // derived from a pen that is still carrying its fractional part. The
            // pen is never rounded, so the rounding error is bounded at half a
            // pixel per glyph and does not accumulate. Round `x` itself — or
            // round `advance` — and the errors add up along the line.
            //
            // The tabular-digit case shifts the glyph to the CENTRE of its
            // fixed pitch, because a digit narrower than the pitch would
            // otherwise hug the left of its cell and the column would look
            // ragged even though the advances are identical.
            const float centring = (options.tabular_digits && cp >= U'0' && cp <= U'9')
                                     ? 0.5f * (digit_pitch - g->advance)
                                     : 0.0f;

            float qx = x + centring + g->offset_x;
            float qy = y + g->offset_y;
            if (options.snap_to_pixel)
            {
                qx = std::floor(qx + 0.5f);
                qy = std::floor(qy + 0.5f);
            }

            glyph_quad q{};
            q.x0 = qx;
            q.y0 = qy;
            q.x1 = qx + static_cast<float>(g->width());
            q.y1 = qy + static_cast<float>(g->height());
            // HALF-OPEN TEXEL RANGE, DIVIDED BY THE ATLAS SIZE. No half-texel
            // offset anywhere: the quad spans exactly `width()` pixels and
            // exactly `width()` texels, so the sampler's own pixel centres land
            // on texel centres. A half-texel nudge here is the classic "text is
            // slightly blurry" fix that is fixing the wrong thing — it is
            // correct only when the quad does NOT land on the pixel grid.
            q.u0 = static_cast<float>(g->x0) * inv_w;
            q.v0 = static_cast<float>(g->y0) * inv_h;
            q.u1 = static_cast<float>(g->x1) * inv_w;
            q.v1 = static_cast<float>(g->y1) * inv_h;
            out.push_back(q);
            ++m.glyphs;

            const float rx0 = q.x0 - pen.x;
            const float ry0 = q.y0 - pen.y;
            const float rx1 = q.x1 - pen.x;
            const float ry1 = q.y1 - pen.y;
            if (!any_ink)
            {
                m.ink_x0 = rx0; m.ink_y0 = ry0; m.ink_x1 = rx1; m.ink_y1 = ry1;
                any_ink = true;
            }
            else
            {
                m.ink_x0 = std::min(m.ink_x0, rx0);
                m.ink_y0 = std::min(m.ink_y0, ry0);
                m.ink_x1 = std::max(m.ink_x1, rx1);
                m.ink_y1 = std::max(m.ink_y1, ry1);
            }
        }

        x += advance + options.tracking;
        m.width = std::max(m.width, x - line_start_x);
        previous = cp;
    }

    return m;
}

text_metrics measure_text(const font_atlas& atlas, std::string_view utf8,
                          const text_layout_options& options)
{
    // ONE IMPLEMENTATION, CALLED TWICE. A `measure` that walks the string with
    // its own copy of the advance arithmetic is a second source of truth, and
    // the failure mode is a background panel that is four pixels too narrow for
    // the string inside it — visible, trivial, and unattributable.
    //
    // The cost is the quads, which are thrown away. A caller measuring in a hot
    // loop should call `layout_text` and keep them.
    std::vector<glyph_quad> discard;
    return layout_text(atlas, utf8, vec2{0.0f, 0.0f}, options, discard);
}

} // namespace engine
