// engine/include/engine/gfx/font.hpp — outlines, rasterized once, into a grid of coverage.
//
// Lesson 6.18, and the first thing to say is what has been missing. This engine
// can light a surface with a microfacet BRDF, sample an irradiance cube, cascade
// a shadow map, bloom the highlights, tonemap to a display and deduce its own
// frame order — and it cannot put a single character on the screen. Every number
// Module 6 measured went to a terminal, which means **the picture and the
// measurement have never once been on the same surface.**
//
// ---------------------------------------------------------------------------
// THE ONE SENTENCE TO KEEP
// ---------------------------------------------------------------------------
//
//   **A glyph is a coverage mask, not a picture.**
//
// `colour.hpp` has said since Lesson 1.6 that alpha is a coverage fraction
// rather than a quantity of light, and `blend.hpp` built Lesson 6.11 on it. Here
// that sentence stops being a caveat and becomes the design, because a glyph has
// *nothing but* coverage in it:
//
//   * The atlas is **one byte per texel**, not four. There is no colour in a
//     glyph — the colour is chosen at the draw call, per string.
//   * The atlas texture is **never `_SRGB`**. A transfer function is the
//     encoding of a light intensity, and coverage is not one. Uploading this
//     array as sRGB and letting the sampler "decode" it applies an inverse gamma
//     to an area fraction, which is meaningless and, worse, *looks nearly
//     right* — text 15% too thin at the mid tones. §7 of the lesson measures it.
//   * A mip chain averages these bytes **with no transfer function at all**,
//     which is exactly what `mipmap.hpp` (6.10) already does for an alpha
//     channel and does not do for colour.
//
// ---------------------------------------------------------------------------
// WHY WE DO NOT HAND-ROLL THIS — and precisely how much of it we do
// ---------------------------------------------------------------------------
//
// The test this course applies to a library, stated once in `image.hpp` and
// applied every time since: **is the hard part the SUBJECT?** For a TrueType
// rasterizer it is not, and the reason is worth being specific about, because
// "fonts are hard" is not an argument.
//
// Turning an outline into coverage is a weekend: quadratic B-splines, flatten to
// line segments, scanline fill with the non-zero winding rule, anti-alias by
// exact area. That part *is* graphics, and if it were the whole job we would
// write it. It is not. The rest of the job is FILE FORMAT:
//
//   * `cmap` subtable formats 0, 4, 6, 12 and 13 — five different encodings of
//     "which glyph is this character", and a font in the wild may carry any of
//     them, sometimes several at once with different platform ids.
//   * composite glyphs: `glyf` entries that are other glyphs with a 2x2 affine
//     transform and a point-matching rule, nested.
//   * hinting: a **stack-based bytecode virtual machine**, per glyph, whose job
//     is to move control points onto the pixel grid at small sizes.
//   * `GPOS`: a lookup-table language for pair positioning, with class-based
//     coverage, which is where every font shipped in the last fifteen years puts
//     its kerning (see `font_atlas::kerning` below — this is not hypothetical,
//     it is the situation the font this engine ships is in).
//
// None of that teaches you anything about an engine. So `stb_truetype` does the
// decode and the rasterization, in exactly one translation unit (`font.cpp`,
// PRIVATE in `engine/CMakeLists.txt`, same as stb_image and cgltf), and this
// header names no third-party type.
//
// WHAT WE DO WRITE, because these three *are* the subject:
//
//   1. **The atlas packer.** Where glyphs land in one texture, and why padding
//      between them is not optional. This is 6.10's bleed argument arriving in a
//      place where it bites at *magnification* rather than minification.
//   2. **The layout.** Pen, baseline, bearing, advance, kerning, and the
//      question of what to round and when — which is where text is actually won
//      or lost, and where §4 of the lesson finds the difference between an error
//      that cancels and an error that accumulates.
//   3. **The compositing** (`overlay.hpp`), because that is a blend equation and
//      a colour space, and this course has opinions about both.
//
// ---------------------------------------------------------------------------
// THE NINETY-PERCENT PICTURE — what this deliberately is not
// ---------------------------------------------------------------------------
//
// This is a **Latin debug-text system**, and the gap between that and "text"
// is enormous. Naming the gap honestly:
//
//   * **NO SHAPING.** One codepoint becomes one glyph, in logical order. That is
//     wrong for Arabic (glyphs change shape by position), for Devanagari
//     (reordering and conjuncts), for any font with ligatures, and for combining
//     marks. The library for that job is HarfBuzz, it is bigger than this
//     engine's renderer, and it is the right answer when you need it.
//   * **NO BIDI.** Mixed right-to-left and left-to-right runs need the Unicode
//     bidirectional algorithm (UAX #9) before layout can begin.
//   * **NO LINE BREAKING** beyond an explicit `\n`. Real wrapping needs UAX #14
//     break opportunities, not spaces.
//   * **NO SUBPIXEL (RGB-stripe) ANTIALIASING.** We produce one coverage value
//     per texel; ClearType-style rendering produces three, one per subpixel, and
//     is display-orientation dependent.
//   * **NO SIGNED DISTANCE FIELDS.** An atlas baked at one size is crisp at that
//     size and soft elsewhere; SDF text (Green, 2007) is the standard answer for
//     text that scales, and `§10` of the lesson says exactly when to reach for
//     it and what it costs.
//   * **ONE FONT, ONE SIZE, ONE STYLE PER ATLAS.** No fallback chain, so a
//     codepoint this atlas does not carry renders as `missing` and is counted
//     rather than silently skipped.

#pragma once

#include <engine/math/vec2.hpp>

#include <SDL3/SDL.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace engine {

// ---- Coordinates, fixed once ------------------------------------------------
//
// EVERYTHING IN THIS FILE IS IN PIXELS, ORIGIN TOP-LEFT, +Y DOWN. That is the
// framebuffer's convention (row 0 is the top row — `image.hpp` says so about
// decoded images and `framebuffer.hpp` about ours), it is the window's, and it
// is what `stb_truetype`'s bitmap boxes use. It is NOT the 3D convention: the
// course's world space is right-handed with +y up (conventions §2), and clip
// space has +y up as well.
//
// The flip therefore has to happen exactly once, and this file is deliberately
// not where it happens. `gpu_overlay`'s vertex shader turns pixel space into
// clip space in one line, and the software compositor writes rows directly. Put
// the flip in two places and text is upside down on one renderer only.

/// Where one glyph lives in the atlas, and where it sits relative to the pen.
///
/// **Two rectangles, and confusing them is the classic first bug.** `x0..y1` is
/// a rectangle in the ATLAS, in texels. `offset_x/offset_y` plus the bitmap's
/// size is a rectangle on the SCREEN, relative to the pen position. They have
/// the same width and height (we blit 1:1 at the baked size) and nothing else in
/// common.
struct glyph
{
    /// Atlas texels, half-open: `[x0, x1)` by `[y0, y1)`. Zero-area for a glyph
    /// with no ink — a space — which is a real glyph with a real advance and no
    /// rectangle at all.
    std::uint16_t x0 = 0;
    std::uint16_t y0 = 0;
    std::uint16_t x1 = 0;
    std::uint16_t y1 = 0;

    /// Pen-to-bitmap offset, in pixels, +y down.
    ///
    /// `offset_y` is almost always NEGATIVE, and that is the single most
    /// surprising number in this file: the pen sits on the BASELINE, and a
    /// glyph's ink starts above it. For a 16 px Karla 'A' it is about -12.
    ///
    /// `offset_x` is the left side bearing, and it is negative for glyphs that
    /// lean left out of their own advance box — 'j' and 'f' in most faces, which
    /// is why an overlay that clips to a string's measured width clips them.
    float offset_x = 0.0f;
    float offset_y = 0.0f;

    /// How far the pen moves after drawing this glyph, in pixels.
    ///
    /// **Not the width of the ink.** A space's advance is its whole contribution
    /// and its ink is empty; an 'f' may draw wider than it advances. Laying text
    /// out with `x1 - x0` instead of this is the bug that makes every string
    /// slightly too narrow and every space vanish.
    float advance = 0.0f;

    [[nodiscard]] int width() const { return static_cast<int>(x1) - static_cast<int>(x0); }
    [[nodiscard]] int height() const { return static_cast<int>(y1) - static_cast<int>(y0); }
    [[nodiscard]] bool has_ink() const { return x1 > x0 && y1 > y0; }
};

/// One kerning adjustment, in pixels, for an ordered pair of codepoints.
///
/// **Sparse, and the sparsity was measured rather than assumed.** Over printable
/// ASCII there are 95 x 95 = 9,025 ordered pairs. The font this engine ships has
/// **186** non-zero ones — 2.06%, so a dense table would be 97.9% zeros. At two
/// pairs per entry that is 1.5 kB sparse against 36 kB dense, and the dense form
/// grows as the square of the character set the moment anybody wants Cyrillic.
struct kern_pair
{
    char32_t first = 0;
    char32_t second = 0;
    float advance = 0.0f;   ///< added to `first`'s advance, usually negative
};

/// What went wrong, if anything. Error handling by return value: the engine core
/// has exceptions off (Lesson 5.3), and a font that fails to load is an ordinary
/// event a shipping game must survive.
enum class font_status
{
    ok,
    cannot_open,      ///< the file is missing or unreadable
    not_a_font,       ///< present, but `stb_truetype` could not find a font in it
    bad_options,      ///< a pixel height <= 0, an empty codepoint range, …
    atlas_too_small,  ///< the glyphs do not fit in `max_atlas_width` squared
    no_glyphs         ///< the font resolved none of the requested codepoints
};

[[nodiscard]] const char* name_of(font_status s);

/// How to bake. Every field is a decision the lesson argues; none is a magic
/// number.
struct font_bake_options
{
    /// Pixels from the ascender to the descender — **not the em square.**
    ///
    /// This is `stb_truetype`'s `ScaleForPixelHeight`, and the distinction is
    /// worth a measurement because it is why "the same 16 px font" looks bigger
    /// in one typeface than another. Karla's `hhea` ascent is 917 and its
    /// descent is -252 in a 1000-unit em, so ascent-descent is **1.169 em**, and
    /// asking for 16 px here gives an em square of **13.69 px**. A font whose
    /// ascent+descent happened to be 1.0 em would give 16 px for the same
    /// request and look noticeably larger.
    float pixel_height = 16.0f;

    /// A contiguous codepoint range, stored densely, because for Latin text that
    /// is the right shape: one subtraction and a bounds check to find a glyph,
    /// no hashing, no branch misprediction per character. Printable ASCII by
    /// default — U+0020 through U+007E.
    char32_t first_codepoint = 0x20;
    int codepoint_count = 95;

    /// Texels of empty space around every glyph in the atlas.
    ///
    /// **One is the minimum and one is not always enough.** A bilinear tap at
    /// the edge of a glyph's rectangle reads half of the texel outside it, so a
    /// neighbour packed flush against it bleeds in — Lesson 6.10's argument,
    /// arriving here at *magnification* rather than minification. One texel
    /// suffices for a 1:1 blit with linear filtering; a mipped atlas needs
    /// `1 << levels`, because level N's texel spans 2^N of level 0's.
    int padding = 1;

    /// The atlas is square and power-of-two, grown by doubling until the glyphs
    /// fit or this is exceeded. Square-and-power-of-two is not a hardware
    /// requirement on any API this engine targets; it is so that a mip chain
    /// halves cleanly and so that two runs of the bake produce the same atlas.
    int max_atlas_width = 2048;

    /// Bake a kerning table. Costs one `stb_truetype` query per ordered pair —
    /// 9,025 of them for the default range, which is milliseconds — and is the
    /// difference between "AV" and "A V".
    bool kerning = true;
};

/// A font, rasterized at one size into one grid of coverage bytes.
///
/// **This is an immutable, renderer-agnostic artefact**, and that is deliberate:
/// the GPU overlay uploads `coverage` to a texture, the software compositor
/// indexes it directly, and neither one is mentioned here. A type that knew
/// about `SDL_GPUTexture` could not be used by the software rasterizer, and this
/// course keeps two renderers honest by making them consume the same data.
struct font_atlas
{
    /// `width * height` bytes, one per texel, row-major, top row first.
    /// **Coverage, in [0, 255], linear in area.** Not a colour, not encoded.
    std::vector<std::uint8_t> coverage;

    int width = 0;
    int height = 0;

    /// What was asked for, kept so that a caller can tell whether a draw is 1:1.
    float pixel_height = 0.0f;

    /// Vertical metrics, scaled to pixels. **`descent` is negative** (it points
    /// down from the baseline and +y is down, so it is *up* from the pen), which
    /// matches `stb_truetype` and every font format.
    ///
    /// `line_gap` is the font's own suggested leading, and it is **zero** in a
    /// great many fonts including the one shipped here — so a renderer that
    /// relies on it for line spacing produces lines that touch.
    float ascent = 0.0f;
    float descent = 0.0f;
    float line_gap = 0.0f;

    /// Dense over `[first_codepoint, first_codepoint + glyphs.size())`.
    char32_t first_codepoint = 0;
    std::vector<glyph> glyphs;

    /// Sorted by `(first, second)`, so a lookup is a binary search. Empty when
    /// `font_bake_options::kerning` was false or the font has no pair data.
    std::vector<kern_pair> kerning;

    /// The padding the packer used, kept because a consumer has to know: a mip
    /// chain over this atlas is only safe to `1 << padding` levels deep.
    int padding = 0;

    /// Texture coordinates of a **fully-covered texel**, for untextured
    /// rectangles.
    ///
    /// One pipeline and one texture draw both text and the panel behind it,
    /// because a solid quad is just a quad whose coverage is 1 everywhere. The
    /// cost is four texels; the alternative is a second pipeline, a second bind
    /// and a state change between the panel and the string on top of it.
    ///
    /// **Two by two rather than one by one**, deliberately: a bilinear tap at
    /// the exact centre of a 1x1 block is correct, and one bit of drift in the
    /// rasterizer's interpolation reads the transparent padding beside it and
    /// the panel gets a faint edge. A 2x2 block leaves a full texel of slack in
    /// every direction. It sits at the top-left of the first shelf, so on a
    /// well-packed atlas it costs no area at all.
    float solid_u = 0.0f;
    float solid_v = 0.0f;

    /// Texels the glyphs and their padding need. **The floor no packer can beat.**
    int packed_texels = 0;

    /// Rows of the atlas the packer actually touched — the bottom of its last
    /// shelf.
    ///
    /// **This, and not `packed_texels / (width * height)`, is what measures the
    /// PACKER.** Dividing the glyph area by the atlas area measures the GLYPHS:
    /// the atlas is a power of two, so the answer is fixed before the packer
    /// runs and a perfect packer scores exactly the same as a terrible one. The
    /// honest quantity is `packed_texels / (width * used_height)` — of the space
    /// it reached for, how much did it use? §5 of the lesson makes that mistake
    /// first and then corrects it, because it is an easy one to ship.
    int used_height = 0;

    /// Codepoints in the requested range the font did not have. Counted, not
    /// hidden: an atlas that silently dropped half its range renders text with
    /// holes and no error anywhere.
    int missing_glyphs = 0;

    [[nodiscard]] bool valid() const
    {
        return width > 0 && height > 0 && !glyphs.empty()
            && coverage.size() == static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    }

    /// The glyph for a codepoint, or null if this atlas does not carry it.
    [[nodiscard]] const glyph* find(char32_t cp) const;

    /// Baseline-to-baseline distance for consecutive lines.
    ///
    /// `ascent - descent + line_gap`, and the subtraction is not a typo:
    /// `descent` is negative.
    [[nodiscard]] float line_height() const { return ascent - descent + line_gap; }

    /// The kerning adjustment for an ordered pair, or 0.
    [[nodiscard]] float kern(char32_t a, char32_t b) const;

    /// One coverage byte, or 0 outside the atlas. Bounds-checked because the
    /// software compositor's inner loop is the one place a layout bug turns into
    /// a read out of bounds.
    [[nodiscard]] std::uint8_t texel(int x, int y) const;

    [[nodiscard]] std::size_t byte_count() const { return coverage.size(); }
};

// ---- Baking -----------------------------------------------------------------

/// Bake from bytes already in memory.
///
/// Takes the bytes rather than a path for the reason `image.hpp` gives: the
/// engine has exactly one notion of where a file lives (`search_path`, Lesson
/// 5.5), and a second loader with its own idea of the working directory is how
/// an asset resolves differently in the editor and in the shipped game.
[[nodiscard]] font_status bake_font(const std::uint8_t* ttf, std::size_t bytes,
                                    const font_bake_options& options, font_atlas& out);

/// Read a file and bake it. `path` is a real path — resolve a NAME first.
[[nodiscard]] font_status load_font(const char* path, const font_bake_options& options,
                                    font_atlas& out);

// ---- Text, decoded ----------------------------------------------------------

/// Decode one UTF-8 sequence, advancing `p`. Returns U+FFFD on anything
/// malformed and still advances by one byte, so a decoder loop always terminates.
///
/// **Written out rather than assumed**, because the failure mode of a sloppy
/// decoder is a security bug rather than a rendering one: an over-long encoding
/// of `/` (0xC0 0xAF) that decodes to U+002F is the classic path-traversal
/// vector, and the rule that forbids it — a codepoint must use the shortest form
/// that can hold it — is three lines of arithmetic that most hand-rolled
/// decoders omit.
[[nodiscard]] char32_t next_codepoint(const char*& p, const char* end);

/// One glyph, placed. Pixel-space rectangle and atlas texture coordinates.
///
/// `u0..v1` are **normalized** (0..1), because that is what a sampler wants, and
/// the division happens once here rather than per vertex.
struct glyph_quad
{
    float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
    float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
};

/// How to lay a string out.
struct text_layout_options
{
    /// Round each glyph's quad onto the pixel grid.
    ///
    /// **On by default, and §4.4 measures why it is the right default and what
    /// it costs.** A glyph blitted at a fractional position is resampled by the
    /// bilinear filter, which is a low-pass over an image whose whole value is
    /// its high frequencies — the text goes soft. Snapping keeps it crisp at a
    /// cost of at most half a pixel of placement error per glyph.
    ///
    /// The part that is easy to get wrong: snap the POSITION, never the ADVANCE.
    /// The pen accumulates in float and each quad is rounded off it, so the
    /// error is bounded at 0.5 px forever. Round the advance instead and the
    /// error is a random walk that reaches **2.4 px over a 60-character line**
    /// at 16 px (measured, §4.4) — which reads as a word that drifts away from
    /// the column you lined it up with.
    bool snap_to_pixel = true;

    /// Lay digits out on a fixed pitch equal to the widest digit's advance.
    ///
    /// **For a readout that changes every frame this is not cosmetic.** Karla's
    /// digit advances at 16 px run from 4.53 px for '1' to 8.42 px for '8', so
    /// `"11.1 ms"` is 7.7 px narrower than `"88.8 ms"` and a frame-time counter
    /// visibly breathes. Most text has no business being laid out this way; a
    /// debug overlay is exactly the case that does.
    bool tabular_digits = false;

    /// Apply the atlas's kerning table.
    bool kerning = true;

    /// Extra pixels between every pair — letter-spacing. Negative tightens.
    float tracking = 0.0f;

    /// Pixels per tab stop. Tabs advance to the next multiple of this from the
    /// line's start x, which is what makes columns line up.
    float tab_stop = 4.0f * 8.0f;
};

/// What a laid-out string occupies.
struct text_metrics
{
    /// Advance width: where the pen ends up, relative to where it started.
    /// **Not the ink's extent** — see `ink_x0`/`ink_x1`, which differ for any
    /// glyph with a negative side bearing.
    float width = 0.0f;

    /// Baseline-relative extremes of the drawn pixels, +y down. `ink_y0` is
    /// negative for anything with ink above the baseline, which is nearly
    /// everything.
    float ink_x0 = 0.0f, ink_y0 = 0.0f, ink_x1 = 0.0f, ink_y1 = 0.0f;

    int lines = 1;
    int glyphs = 0;       ///< glyphs that produced a quad (ink only)
    int codepoints = 0;   ///< codepoints consumed, including spaces and newlines
    int missing = 0;      ///< codepoints this atlas does not carry

    [[nodiscard]] float ink_width() const { return ink_x1 - ink_x0; }
    [[nodiscard]] float ink_height() const { return ink_y1 - ink_y0; }
};

/// Lay a UTF-8 string out, appending one quad per inked glyph.
///
/// @param pen the BASELINE ORIGIN of the first line, in pixels. Not the top-left
///        corner of the text — that is `pen.y - atlas.ascent`, and getting this
///        wrong shifts every string up by an ascender, which looks like a
///        margin bug and is a coordinate-convention bug.
/// @param out appended to, never cleared. The caller owns the storage and the
///        overlay reuses one vector across frames, so a steady-state frame
///        performs no allocation at all.
text_metrics layout_text(const font_atlas& atlas, std::string_view utf8, vec2 pen,
                         const text_layout_options& options, std::vector<glyph_quad>& out);

/// The metrics without the quads. Same arithmetic, so the two cannot disagree —
/// it calls `layout_text` into a discarded buffer rather than duplicating it,
/// which is the whole reason this is a thin wrapper and not a second algorithm.
[[nodiscard]] text_metrics measure_text(const font_atlas& atlas, std::string_view utf8,
                                        const text_layout_options& options = {});

} // namespace engine
