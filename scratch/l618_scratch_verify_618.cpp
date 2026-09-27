// scratch/verify_618.cpp — every number Lesson 6.18 quotes, produced here.
//
//   sh scratch/build_verify_618.sh                                # as configured
//   ENGINE_CFLAGS="-O2 -DNDEBUG" sh scratch/build_verify_618.sh   # release, for §K
//
// §A  the bake: what one font at one size actually produced
// §B  the packer, against the floor it could not have beaten
// §C  the layout arithmetic, checked by hand
// §D  snapping: the error that cancels and the error that accumulates
// §E  the padding, and what a missing texel of it does
// §F  UTF-8, including the three rules that are security bugs
// §G  THE GAMMA MEASUREMENT — stem weight, in three wrong spaces and one right one
// §H  where the overlay composites: before or after the curve
// §I  the frame graph: the overlay declared as a pass, by somebody else's code
// §J  THE INSTRUMENT — the same overlay, CPU and GPU, channel for channel
// §K  what a frame of text costs
#include "../demos/common/demo_scene.hpp"

#include <engine/core/bench.hpp>
#include <engine/gfx/blend.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/font.hpp>
#include <engine/gfx/frame_graph.hpp>
#include <engine/gfx/framebuffer.hpp>
#include <engine/gfx/gpu_overlay.hpp>
#include <engine/gfx/gpu_shader.hpp>
#include <engine/gfx/hdr.hpp>
#include <engine/gfx/overlay.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using engine::font_atlas;
using engine::font_bake_options;
using engine::font_status;
using engine::framebuffer;
using engine::glyph;
using engine::glyph_quad;
using engine::overlay_batch;
using engine::overlay_blend;
using engine::text_layout_options;
using engine::text_metrics;
using engine::vec2;

// COUNTING ALLOCATIONS, BECAUSE §K CLAIMS A STEADY-STATE FRAME PERFORMS NONE.
// The same instrument Lesson 6.17 §H used, for the same reason: "the vectors
// keep their capacity" is a design intention until something counts.
std::size_t g_allocations = 0;
bool g_count_allocations = false;

void* operator_new_counted(std::size_t n)
{
    if (g_count_allocations) { ++g_allocations; }
    return std::malloc(n == 0 ? 1 : n);
}

void* operator new(std::size_t n) { return operator_new_counted(n); }
void* operator new[](std::size_t n) { return operator_new_counted(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

namespace {

int g_failures = 0;

void check(bool ok, const char* what)
{
    if (!ok) { ++g_failures; }
    std::printf("    [%s] %s\n", ok ? "PASS" : "FAIL", what);
}

void check_eq(long long got, long long want, const char* what)
{
    const bool ok = (got == want);
    if (!ok) { ++g_failures; }
    std::printf("    [%s] %-52s got %lld  want %lld\n",
                ok ? "PASS" : "FAIL", what, got, want);
}

void check_near(double got, double want, double tol, const char* what)
{
    const bool ok = std::fabs(got - want) <= tol;
    if (!ok) { ++g_failures; }
    std::printf("    [%s] %-42s got %12.6f  want %12.6f\n",
                ok ? "PASS" : "FAIL", what, got, want);
}

void section(const char* title)
{
    std::printf("\n");
    std::printf("===========================================================================\n");
    std::printf("  §%s\n", title);
    std::printf("===========================================================================\n");
}

// The font this engine ships. Resolved the way every other asset is — relative
// to the executable — because `demo_common` puts `assets/` beside the binary.
const char* font_path()
{
    static std::string path;
    if (path.empty())
    {
        const char* base = SDL_GetBasePath();
        path = std::string(base != nullptr ? base : "./") + "assets/fonts/Karla-Regular.ttf";
    }
    return path.c_str();
}

// ---------------------------------------------------------------------------
//  Measuring "how much ink is on the page"
// ---------------------------------------------------------------------------
//
// The whole of §G rests on one number, so it is worth being precise about what
// it is. **Ink mass** is the total LIGHT the text adds to the page, summed over
// every pixel, measured in linear light:
//
//     mass = sum over pixels of ( luminance(pixel) - luminance(background) )
//
// It is linear-light rather than code-value on purpose: the question being asked
// is "does this text emit the right amount of light", and comparing sRGB codes
// would beg exactly the question under test. Two renderings of the same string
// that differ in mass differ in apparent weight, which is what an eye sees as a
// stem being thin or fat.
double ink_mass(const framebuffer& fb, Uint32 background)
{
    const engine::linear_rgb bg = engine::to_linear(background);
    const double bg_luma = static_cast<double>(engine::luminance(bg));
    double mass = 0.0;
    for (int y = 0; y < fb.height(); ++y)
    {
        for (int x = 0; x < fb.width(); ++x)
        {
            const engine::linear_rgb c = engine::to_linear(fb.pixel_at(x, y));
            mass += static_cast<double>(engine::luminance(c)) - bg_luma;
        }
    }
    return mass;
}

/// Render one string into a fresh framebuffer, and hand back the buffer.
framebuffer render_string(const font_atlas& atlas, const char* text, Uint32 ink, Uint32 paper,
                          overlay_blend blend, float stem_darken, int w = 256, int h = 40)
{
    framebuffer fb(w, h);
    fb.clear(paper);

    overlay_batch batch;
    batch.begin(w, h);
    (void)batch.text_top_left(atlas, text, vec2{4.0f, 8.0f}, ink);
    engine::composite_overlay(fb, atlas, batch, blend, stem_darken);
    return fb;
}

} // namespace

// ===========================================================================
//  §A — the bake
// ===========================================================================

font_atlas g_atlas;

void section_a()
{
    section("A — the bake: one font, one size, and the number that is not what you asked for");

    font_bake_options opts{};
    opts.pixel_height = 16.0f;

    const font_status status = engine::load_font(font_path(), opts, g_atlas);
    std::printf("\n  load_font(\"%s\") -> %s\n", font_path(), engine::name_of(status));
    check(status == font_status::ok, "the font loaded and baked");
    if (status != font_status::ok) { return; }

    std::printf("\n  atlas      %d x %d, %zu bytes (R8)\n",
                g_atlas.width, g_atlas.height, g_atlas.byte_count());
    std::printf("  glyphs     %zu, %d missing\n", g_atlas.glyphs.size(), g_atlas.missing_glyphs);
    std::printf("  ascent     %8.4f px\n", static_cast<double>(g_atlas.ascent));
    std::printf("  descent    %8.4f px   (negative: it is BELOW the baseline)\n",
                static_cast<double>(g_atlas.descent));
    std::printf("  line_gap   %8.4f px\n", static_cast<double>(g_atlas.line_gap));
    std::printf("  line_height%8.4f px\n", static_cast<double>(g_atlas.line_height()));

    // THE FIRST SURPRISE, AND IT IS NOT A BUG. `ScaleForPixelHeight` maps
    // ascent-descent onto the requested height, and Karla's ascent-descent is
    // 1.169 em. So a "16 px" font has a 13.69 px em square, and the same request
    // to a font with a smaller ascent would produce visibly larger glyphs.
    check_near(static_cast<double>(g_atlas.ascent - g_atlas.descent), 16.0, 1e-3,
               "ascent - descent IS the requested pixel height");
    const double em_px = 16.0 * 1000.0 / 1169.0;
    std::printf("\n  Karla: unitsPerEm 1000, hhea ascent 917, descent -252.\n");
    std::printf("  So ascent-descent = 1169 units = %.4f em, and a \"16 px\" font\n",
                1169.0 / 1000.0);
    std::printf("  has an EM SQUARE of 16 x 1000/1169 = %.4f px.\n", em_px);
    check_near(em_px, 13.6869, 1e-3, "the em square of a 16 px Karla");

    // line_gap is zero here, and a renderer that used it for leading would
    // produce lines that touch. Worth asserting so the claim in the lesson is
    // about this font rather than about fonts in general.
    check_near(static_cast<double>(g_atlas.line_gap), 0.0, 1e-6,
               "this font's suggested leading is ZERO");

    // ---- Kerning: where the data lives -------------------------------------
    std::printf("\n  kerning: %zu non-zero pairs out of %d ordered pairs (%.2f%%)\n",
                g_atlas.kerning.size(), 95 * 95,
                100.0 * static_cast<double>(g_atlas.kerning.size()) / (95.0 * 95.0));
    std::printf("  dense table would be %d B; sparse is %zu B\n",
                95 * 95 * 4, g_atlas.kerning.size() * sizeof(engine::kern_pair));
    check_eq(static_cast<long long>(g_atlas.kerning.size()), 186,
             "186 kerning pairs, from GPOS (this font has no `kern` table)");

    const float av = g_atlas.kern(U'A', U'V');
    const float ii = g_atlas.kern(U'i', U'i');
    std::printf("\n  kern('A','V') = %+.4f px      kern('i','i') = %+.4f px\n",
                static_cast<double>(av), static_cast<double>(ii));
    check(av < 0.0f, "'AV' kerns TIGHTER (a negative adjustment)");
    check_near(static_cast<double>(ii), 0.0, 1e-9, "'ii' does not kern at all");

    // ---- Digit advances: not tabular, and it matters for a readout ---------
    std::printf("\n  digit advances at 16 px:\n   ");
    float min_adv = 1e9f, max_adv = 0.0f;
    for (char32_t d = U'0'; d <= U'9'; ++d)
    {
        const glyph* g = g_atlas.find(d);
        std::printf(" %c=%.3f", static_cast<char>(d), static_cast<double>(g->advance));
        min_adv = std::min(min_adv, g->advance);
        max_adv = std::max(max_adv, g->advance);
    }
    std::printf("\n  narrowest %.4f ('1'), widest %.4f ('8'), spread %.4f px\n",
                static_cast<double>(min_adv), static_cast<double>(max_adv),
                static_cast<double>(max_adv - min_adv));
    check(max_adv - min_adv > 3.0f, "digits are NOT tabular in this font");

    // FIGURE 1'S NUMBERS. One glyph, in full, so the anatomy diagram quotes the
    // engine rather than a drawing.
    std::printf("\n  %-6s %8s %8s %8s %8s %8s\n",
                "glyph", "w", "h", "off_x", "off_y", "advance");
    for (const char32_t cp : {U'H', U'g', U'j', U'.'})
    {
        const glyph* g = g_atlas.find(cp);
        std::printf("  %-6c %8d %8d %8.4f %8.4f %8.4f\n", static_cast<char>(cp),
                    g->width(), g->height(), static_cast<double>(g->offset_x),
                    static_cast<double>(g->offset_y), static_cast<double>(g->advance));
    }

    const text_metrics narrow = engine::measure_text(g_atlas, "11.1 ms");
    const text_metrics wide = engine::measure_text(g_atlas, "88.8 ms");
    std::printf("\n  \"11.1 ms\" is %.4f px wide; \"88.8 ms\" is %.4f px. A readout that\n",
                static_cast<double>(narrow.width), static_cast<double>(wide.width));
    std::printf("  changes between them BREATHES by %.4f px, sixty times a second.\n",
                static_cast<double>(wide.width - narrow.width));

    text_layout_options tab{};
    tab.tabular_digits = true;
    const text_metrics narrow_t = engine::measure_text(g_atlas, "11.1 ms", tab);
    const text_metrics wide_t = engine::measure_text(g_atlas, "88.8 ms", tab);
    check_near(static_cast<double>(wide_t.width - narrow_t.width), 0.0, 1e-4,
               "tabular_digits makes the two exactly the same width");
}

// ===========================================================================
//  §B — the packer, judged against the floor
// ===========================================================================

void section_b()
{
    section("B — the packer, against what it could not have beaten");

    // THE OBVIOUS MEASUREMENT, WHICH MEASURES THE WRONG THING. "What fraction
    // of the atlas is glyphs?" sounds like a verdict on the packer and is not:
    // the numerator is the area of the glyphs and the denominator is a power of
    // two, so the answer is fixed before the packer runs. A perfect packer and a
    // hopeless one score identically.
    const double area = static_cast<double>(g_atlas.width) * static_cast<double>(g_atlas.height);
    const double naive = 100.0 * static_cast<double>(g_atlas.packed_texels) / area;
    std::printf("\n  atlas %d x %d = %.0f texels, glyphs + padding = %d texels\n",
                g_atlas.width, g_atlas.height, area, g_atlas.packed_texels);
    std::printf("  \"occupancy\" = %.1f%%  <- this number is about the FONT, not the packer\n",
                naive);

    // THE HONEST ONE. The packer laid its shelves top-down and stopped; how much
    // of the space it reached for did it use? Numerator and denominator now both
    // depend on what the packer did.
    const double used = static_cast<double>(g_atlas.width)
                      * static_cast<double>(g_atlas.used_height);
    const double efficiency = 100.0 * static_cast<double>(g_atlas.packed_texels) / used;
    std::printf("\n  the packer touched %d of %d rows: %.0f texels\n",
                g_atlas.used_height, g_atlas.height, used);
    std::printf("  shelf efficiency = %.1f%%  <- of the space it used, this much is glyphs\n",
                efficiency);
    // THE GUARD IS NOT DECORATION. The first version of this section shipped
    // without it, against a stale library where `used_height` was still 0 — so
    // `efficiency` was `inf` and `inf > 80.0` is TRUE. The check passed, printed
    // "inf%", and reported a pass. **A check whose degenerate case is a pass is
    // not a check**, which is 6.16's `golden_615` finding (two failed file reads
    // comparing equal) arriving in a division instead of a comparison.
    check(g_atlas.used_height > 0 && g_atlas.used_height <= g_atlas.height,
          "the packer reported a plausible footprint at all");
    check(std::isfinite(efficiency) && efficiency > 80.0,
          "shelf packing wastes less than a fifth of the rows it uses");

    // AND WHAT THE ATLAS SIZE ACTUALLY DEPENDS ON. 7,145 texels of glyph will
    // not fit in 64x64 = 4,096, so 128 is forced — by the font, at this size,
    // regardless of the packer. The packer's contribution is that it needed only
    // `used_height` of those 128 rows.
    std::printf("\n  64x64 holds %d texels and the glyphs need %d, so 128 is forced.\n",
                64 * 64, g_atlas.packed_texels);
    std::printf("  What the packer bought is the %d unused rows at the bottom, which\n",
                g_atlas.height - g_atlas.used_height);
    std::printf("  is where the next size of the same font goes.\n");
    check(64 * 64 < g_atlas.packed_texels, "the next size down could not have held them");

    // FIGURE 2'S NUMBERS: the shelves the packer actually laid, recovered by
    // grouping the glyph rectangles on their top edge.
    {
        std::vector<std::pair<int, std::pair<int, int>>> shelves;   // y -> (height, count)
        for (const glyph& g : g_atlas.glyphs)
        {
            if (!g.has_ink()) { continue; }
            bool found = false;
            for (auto& s2 : shelves)
            {
                if (s2.first == g.y0)
                {
                    s2.second.first = std::max(s2.second.first, g.height());
                    ++s2.second.second;
                    found = true;
                    break;
                }
            }
            if (!found) { shelves.push_back({g.y0, {g.height(), 1}}); }
        }
        std::sort(shelves.begin(), shelves.end());
        std::printf("\n  %-8s %-8s %-8s\n", "shelf y", "height", "glyphs");
        for (const auto& s2 : shelves)
        {
            std::printf("  %-8d %-8d %-8d\n", s2.first, s2.second.first, s2.second.second);
        }
        std::printf("  %zu shelves, tallest first — which is what makes the slack small\n",
                    shelves.size());
    }

    // Every glyph must be inside the atlas and must not overlap any other. Both
    // are cheap to check and both are the kind of thing that produces a
    // plausible-looking atlas with one letter wearing another's tail.
    std::vector<Uint8> claimed(static_cast<std::size_t>(g_atlas.width)
                               * static_cast<std::size_t>(g_atlas.height), 0u);
    int overlaps = 0, out_of_bounds = 0;
    for (const glyph& g : g_atlas.glyphs)
    {
        if (!g.has_ink()) { continue; }
        if (g.x1 > g_atlas.width || g.y1 > g_atlas.height) { ++out_of_bounds; continue; }
        for (int y = g.y0; y < g.y1; ++y)
        {
            for (int x = g.x0; x < g.x1; ++x)
            {
                Uint8& slot = claimed[static_cast<std::size_t>(y)
                                      * static_cast<std::size_t>(g_atlas.width)
                                      + static_cast<std::size_t>(x)];
                if (slot != 0u) { ++overlaps; }
                slot = 1u;
            }
        }
    }
    check_eq(out_of_bounds, 0, "every glyph rectangle is inside the atlas");
    check_eq(overlaps, 0, "no two glyph rectangles overlap");

    // THE SOLID BLOCK, which makes one pipeline draw both text and panels.
    const int sx = static_cast<int>(g_atlas.solid_u * static_cast<float>(g_atlas.width));
    const int sy = static_cast<int>(g_atlas.solid_v * static_cast<float>(g_atlas.height));
    std::printf("\n  solid texel at uv (%.5f, %.5f) -> texel (%d, %d) = %u\n",
                static_cast<double>(g_atlas.solid_u), static_cast<double>(g_atlas.solid_v),
                sx, sy, g_atlas.texel(sx, sy));
    check_eq(g_atlas.texel(sx, sy), 255, "the solid texel is fully covered");
    check(claimed[static_cast<std::size_t>(sy) * static_cast<std::size_t>(g_atlas.width)
                  + static_cast<std::size_t>(sx)] == 0u,
          "and no glyph was packed on top of it");

    // DETERMINISM. Two bakes of the same font at the same size must produce
    // byte-identical atlases, or a golden image over text is a coin flip — 6.17's
    // argument about the frame graph's topological order, in a second place.
    font_atlas again;
    font_bake_options opts{};
    opts.pixel_height = 16.0f;
    check(engine::load_font(font_path(), opts, again) == font_status::ok, "re-baked");
    check(again.coverage == g_atlas.coverage, "two bakes produce byte-identical atlases");
}

// ===========================================================================
//  §C — the layout arithmetic, by hand
// ===========================================================================

void section_c()
{
    section("C — the layout arithmetic, checked against hand arithmetic");

    // Take four characters and walk the pen by hand. `layout_text` must agree,
    // and if it does not, one of the two is wrong in a way neither can hide.
    const char* text = "AVIi";
    text_layout_options opts{};
    opts.snap_to_pixel = false;   // the raw arithmetic first; §D adds the snap

    std::vector<glyph_quad> quads;
    const text_metrics m = engine::layout_text(g_atlas, text, vec2{0.0f, 0.0f}, opts, quads);

    double pen = 0.0;
    std::printf("\n  %-4s %10s %10s %10s %12s\n", "ch", "kern", "pen", "advance", "quad x0");
    const char* p = text;
    char32_t prev = 0;
    std::size_t q = 0;
    while (*p != '\0')
    {
        const char32_t cp = static_cast<char32_t>(*p++);
        const glyph* g = g_atlas.find(cp);
        const double k = (prev != 0) ? static_cast<double>(g_atlas.kern(prev, cp)) : 0.0;
        pen += k;
        const double qx = pen + static_cast<double>(g->offset_x);
        std::printf("  %-4c %10.4f %10.4f %10.4f %12.4f\n",
                    static_cast<char>(cp), k, pen, static_cast<double>(g->advance), qx);
        if (g->has_ink() && q < quads.size())
        {
            check_near(static_cast<double>(quads[q].x0), qx, 1e-4, "quad x0 matches by hand");
            ++q;
        }
        pen += static_cast<double>(g->advance);
        prev = cp;
    }
    check_near(static_cast<double>(m.width), pen, 1e-4, "the advance width matches by hand");

    // ADVANCE WIDTH IS NOT INK WIDTH, and the difference is a real rectangle a
    // caller might clip to.
    std::printf("\n  advance width %.4f px, ink [%.4f, %.4f] = %.4f px\n",
                static_cast<double>(m.width), static_cast<double>(m.ink_x0),
                static_cast<double>(m.ink_x1), static_cast<double>(m.ink_width()));
    check(std::fabs(m.ink_width() - m.width) > 0.01f,
          "ink width and advance width are DIFFERENT numbers");

    // A glyph whose ink starts left of the pen: the negative left side bearing.
    // Panel-sizing code that uses `width` alone clips these.
    int negative_lsb = 0;
    for (std::size_t i = 0; i < g_atlas.glyphs.size(); ++i)
    {
        if (g_atlas.glyphs[i].has_ink() && g_atlas.glyphs[i].offset_x < 0.0f)
        {
            ++negative_lsb;
        }
    }
    std::printf("  %d of %zu glyphs have ink LEFT of the pen (negative bearing)\n",
                negative_lsb, g_atlas.glyphs.size());
    check(negative_lsb > 0, "some glyphs lean out of their own advance box");

    // ---- Newlines and tabs -------------------------------------------------
    std::vector<glyph_quad> two_lines;
    const text_metrics ml = engine::layout_text(g_atlas, "ab\ncd", vec2{10.0f, 20.0f},
                                                opts, two_lines);
    check_eq(ml.lines, 2, "a newline starts a second line");
    check_near(static_cast<double>(two_lines[2].y0 - two_lines[0].y0),
               static_cast<double>(g_atlas.line_height()), 0.75,
               "the second line sits one line_height lower");
    check_near(static_cast<double>(two_lines[2].x0 - 10.0f),
               static_cast<double>(two_lines[0].x0 - 10.0f), 1e-4,
               "and starts at the same x the first line did");

    std::vector<glyph_quad> tabbed;
    text_layout_options tabs = opts;
    tabs.tab_stop = 32.0f;
    (void)engine::layout_text(g_atlas, "a\tb", vec2{100.0f, 0.0f}, tabs, tabbed);
    // The quad's x0 is the PEN plus the glyph's own left side bearing, so the
    // bearing has to come back out before the tab stop is visible. Comparing the
    // quad against 32 directly is off by exactly `offset_x`, which is the kind of
    // near-miss that tempts people to widen the tolerance until it passes.
    const float b_bearing = g_atlas.find(U'b')->offset_x;
    check_near(static_cast<double>(tabbed[1].x0 - b_bearing - 100.0f), 32.0, 1e-4,
               "a tab advances to the next stop MEASURED FROM THE LINE START");

    // A codepoint the atlas does not carry is counted, not silently dropped.
    const text_metrics missing = engine::measure_text(g_atlas, "a\xE2\x9C\x93z");   // U+2713
    check_eq(missing.missing, 1, "a codepoint outside the range is COUNTED");
    check_eq(missing.glyphs, 2, "and the two it does carry still draw");
}

// ===========================================================================
//  §D — snapping: the error that cancels and the error that accumulates
// ===========================================================================

void section_d()
{
    section("D — what to round, and when");

    // Sixty characters of ordinary text. Three layouts:
    //   1. no snapping at all              — the exact pen, blurry on screen
    //   2. snap the POSITION (ours)        — each quad rounded off an exact pen
    //   3. snap the ADVANCE                — the pen itself rounded each step
    const char* line = "the quick brown fox jumps over the lazy dog, and again.";

    text_layout_options exact{};
    exact.snap_to_pixel = false;
    std::vector<glyph_quad> q_exact;
    const text_metrics m_exact = engine::layout_text(g_atlas, line, vec2{0.0f, 0.0f},
                                                     exact, q_exact);

    text_layout_options snapped{};
    snapped.snap_to_pixel = true;
    std::vector<glyph_quad> q_snap;
    (void)engine::layout_text(g_atlas, line, vec2{0.0f, 0.0f}, snapped, q_snap);

    // The third is not an option in this engine, so it is simulated here — which
    // is the honest way to show a mistake you refuse to ship.
    std::vector<float> rounded_x;
    {
        float pen = 0.0f;
        char32_t prev = 0;
        const char* p = line;
        const char* const end = p + std::strlen(line);
        while (p < end)
        {
            const char32_t cp = engine::next_codepoint(p, end);
            const glyph* g = g_atlas.find(cp);
            if (g == nullptr) { continue; }
            if (prev != 0) { pen += g_atlas.kern(prev, cp); }
            if (g->has_ink()) { rounded_x.push_back(pen + g->offset_x); }
            pen += std::floor(g->advance + 0.5f);   // THE MISTAKE: round the advance
            prev = cp;
        }
    }

    double max_snap_err = 0.0;
    for (std::size_t i = 0; i < q_exact.size(); ++i)
    {
        max_snap_err = std::max(max_snap_err,
                                std::fabs(static_cast<double>(q_snap[i].x0 - q_exact[i].x0)));
    }
    double max_round_err = 0.0, final_round_err = 0.0;
    for (std::size_t i = 0; i < q_exact.size() && i < rounded_x.size(); ++i)
    {
        const double e = std::fabs(static_cast<double>(rounded_x[i] - q_exact[i].x0));
        max_round_err = std::max(max_round_err, e);
        final_round_err = e;
    }

    std::printf("\n  %zu glyphs over %.2f px of line.\n", q_exact.size(),
                static_cast<double>(m_exact.width));
    std::printf("\n  snap the POSITION (what this engine does):\n");
    std::printf("    worst placement error  %.4f px   (bounded at 0.5 by construction)\n",
                max_snap_err);
    std::printf("\n  round the ADVANCE (the mistake):\n");
    std::printf("    worst placement error  %.4f px\n", max_round_err);
    std::printf("    error at the LAST glyph %.4f px  <- it does not come back\n",
                final_round_err);

    // FIGURE 3'S SERIES: both errors, every fifth glyph, so the diagram plots
    // measurements rather than a sketch of what a random walk looks like.
    std::printf("\n  %-6s %10s %10s\n", "glyph", "snap err", "round err");
    for (std::size_t i = 0; i < q_exact.size(); i += 5u)
    {
        const double se = static_cast<double>(q_snap[i].x0 - q_exact[i].x0);
        const double re = (i < rounded_x.size())
                            ? static_cast<double>(rounded_x[i] - q_exact[i].x0) : 0.0;
        std::printf("  %-6zu %10.4f %10.4f\n", i, se, re);
    }

    check(max_snap_err <= 0.5 + 1e-6, "snapping the position is bounded at half a pixel");
    check(max_round_err > 2.0, "rounding the advance drifts by more than two pixels");
    check(final_round_err > 1.0, "and the drift is still there at the end of the line");

    // AND THE POINT OF SNAPPING AT ALL. An unsnapped quad lands between texels,
    // so a linear sampler averages two of them. Measure the worst fractional
    // offset in the unsnapped layout: any non-zero value is a glyph that will be
    // resampled.
    int fractional = 0;
    for (const glyph_quad& q : q_exact)
    {
        if (std::fabs(q.x0 - std::floor(q.x0 + 0.5f)) > 1e-4f) { ++fractional; }
    }
    std::printf("\n  without snapping, %zu of %zu glyphs land off the pixel grid;\n",
                static_cast<std::size_t>(fractional), q_exact.size());
    std::printf("  with it, every one of them is on it.\n");
    check(fractional > 0, "unsnapped text lands between pixels");
    int on_grid = 0;
    for (const glyph_quad& q : q_snap)
    {
        if (std::fabs(q.x0 - std::floor(q.x0 + 0.5f)) < 1e-4f) { ++on_grid; }
    }
    check_eq(on_grid, static_cast<long long>(q_snap.size()), "snapped text is entirely on it");
}

// ===========================================================================
//  §E — the padding
// ===========================================================================

void section_e()
{
    section("E — one texel of padding, and what happens without it");

    font_bake_options bare{};
    bare.pixel_height = 16.0f;
    bare.padding = 0;
    font_atlas unpadded;
    check(engine::load_font(font_path(), bare, unpadded) == font_status::ok,
          "baked a second atlas with NO padding");

    // THE MEASUREMENT. For every glyph, look at the column of texels immediately
    // right of its rectangle. With padding that column is empty; without it, the
    // column belongs to the next glyph on the shelf. A bilinear tap at the
    // glyph's right edge reads half of it.
    const auto neighbour_ink = [](const font_atlas& a)
    {
        int lit = 0, total = 0;
        for (const glyph& g : a.glyphs)
        {
            if (!g.has_ink() || g.x1 >= a.width) { continue; }
            for (int y = g.y0; y < g.y1; ++y)
            {
                ++total;
                if (a.texel(g.x1, y) != 0u) { ++lit; }
            }
        }
        return std::pair<int, int>{lit, total};
    };

    const auto [lit_pad, total_pad] = neighbour_ink(g_atlas);
    const auto [lit_bare, total_bare] = neighbour_ink(unpadded);
    std::printf("\n  padding = 1: %d of %d edge-adjacent texels carry a NEIGHBOUR's ink\n",
                lit_pad, total_pad);
    std::printf("  padding = 0: %d of %d  (%.1f%%)\n", lit_bare, total_bare,
                100.0 * static_cast<double>(lit_bare) / static_cast<double>(total_bare));
    check_eq(lit_pad, 0, "with padding, nothing bleeds in");
    check(lit_bare > 0, "without it, neighbours are one bilinear tap away");

    std::printf("\n  the cost of the padding: %d texels vs %d, atlas %dx%d vs %dx%d\n",
                g_atlas.packed_texels, unpadded.packed_texels,
                g_atlas.width, g_atlas.height, unpadded.width, unpadded.height);

    // AND THE HONEST QUALIFIER: at a 1:1 blit with NEAREST filtering, none of
    // this matters. The bleed appears the moment anything samples between texels
    // — a scaled overlay, a rotated one, or a mip chain. One texel is enough for
    // bilinear at level 0; a mipped atlas needs `1 << levels`, because a level-N
    // texel spans 2^N of level 0's.
    std::printf("\n  at a 1:1 NEAREST blit this costs nothing and buys nothing.\n");
    std::printf("  it is insurance against the first person who scales the overlay.\n");
}

// ===========================================================================
//  §F — UTF-8, including the rules that are security bugs
// ===========================================================================

void section_f()
{
    section("F — the decoder, and the three rules most hand-rolled ones omit");

    const auto decode_one = [](const char* bytes, std::size_t n)
    {
        const char* p = bytes;
        return engine::next_codepoint(p, bytes + n);
    };

    check_eq(static_cast<long long>(decode_one("A", 1)), 0x41, "ASCII 'A'");
    check_eq(static_cast<long long>(decode_one("\xC3\xA9", 2)), 0xE9, "two-byte U+00E9");
    check_eq(static_cast<long long>(decode_one("\xE2\x9C\x93", 3)), 0x2713, "three-byte U+2713");
    check_eq(static_cast<long long>(decode_one("\xF0\x9F\x8E\xB2", 4)), 0x1F3B2,
             "four-byte U+1F3B2");

    // (a) THE OVER-LONG FORM. 0xC0 0xAF is a two-byte encoding of U+002F '/'.
    // A naive decoder returns '/', which walks straight past any path check
    // performed on the raw bytes. This is not a rendering bug.
    check_eq(static_cast<long long>(decode_one("\xC0\xAF", 2)), 0xFFFD,
             "an OVER-LONG '/' is refused, not decoded to 0x2F");

    // (b) SURROGATES are UTF-16 machinery and are not characters.
    check_eq(static_cast<long long>(decode_one("\xED\xA0\x80", 3)), 0xFFFD,
             "U+D800 encoded in UTF-8 is ill-formed");

    // (c) BEYOND U+10FFFF: the four-byte form can express more than Unicode has.
    check_eq(static_cast<long long>(decode_one("\xF7\xBF\xBF\xBF", 4)), 0xFFFD,
             "U+1FFFFF is past the end of Unicode");

    check_eq(static_cast<long long>(decode_one("\x80", 1)), 0xFFFD, "a stray continuation byte");
    check_eq(static_cast<long long>(decode_one("\xE2\x9C", 2)), 0xFFFD, "a truncated sequence");

    // TERMINATION. A decoder that does not advance on a malformed byte is an
    // infinite loop, which is the other way this function can be a security bug.
    {
        const char junk[] = "\x80\x80\xC0\xAF\xFF";
        const char* p = junk;
        const char* const end = junk + sizeof(junk) - 1;
        int steps = 0;
        while (p < end && steps < 100) { (void)engine::next_codepoint(p, end); ++steps; }
        check(p == end, "every malformed byte still advances the pointer");
        check_eq(steps, 5, "five bytes, five steps, no loop");
    }
}

// ===========================================================================
//  §G — THE GAMMA MEASUREMENT
// ===========================================================================

void section_g()
{
    section("G — stem weight: the same glyphs, composited four ways");

    const Uint32 black = engine::pack_argb(0, 0, 0);
    const Uint32 white = engine::pack_argb(255, 255, 255);
    const char* sample = "Handgloves 0123";

    // 1. CORRECT: decode, lerp light, re-encode.
    framebuffer fb_lin = render_string(g_atlas, sample, white, black,
                                       overlay_blend::linear, 0.0f);
    // 2. WRONG: lerp the stored codes. What a plain UNORM target's blender does.
    framebuffer fb_enc = render_string(g_atlas, sample, white, black,
                                       overlay_blend::encoded, 0.0f);

    const double mass_lin = ink_mass(fb_lin, black);
    const double mass_enc = ink_mass(fb_enc, black);

    std::printf("\n  white on black, \"%s\" at 16 px\n", sample);
    std::printf("    linear blending   ink mass %10.4f\n", mass_lin);
    std::printf("    encoded blending  ink mass %10.4f   (%.1f%% of correct)\n",
                mass_enc, 100.0 * mass_enc / mass_lin);
    check(mass_enc < mass_lin, "blending codes makes light-on-dark text DIMMER");

    // The single number Lesson 6.11 put on the books, arriving here as a
    // property of a whole string rather than of one pixel: half-coverage white
    // over black is 0.5 of the light correctly and 0.2159 incorrectly — 43%.
    const double half_correct = static_cast<double>(engine::srgb_to_linear_u8(
        engine::red_of(engine::blend_over(black, white, 0.5f))));
    const double half_wrong = static_cast<double>(engine::srgb_to_linear_u8(
        engine::red_of(engine::blend_over_encoded(black, white, 0.5f))));
    std::printf("\n    one pixel at half coverage: %.4f correct, %.4f wrong (%.1f%%)\n",
                half_correct, half_wrong, 100.0 * half_wrong / half_correct);
    check_near(100.0 * half_wrong / half_correct, 43.0, 1.5,
               "6.11's 43% figure, re-derived");

    // ---- AND NOW THE OTHER DIRECTION, which is the part usually left out ----
    framebuffer fb_lin_inv = render_string(g_atlas, sample, black, white,
                                           overlay_blend::linear, 0.0f);
    framebuffer fb_enc_inv = render_string(g_atlas, sample, black, white,
                                           overlay_blend::encoded, 0.0f);
    // Ink mass is negative here: the text REMOVES light from a white page. Take
    // the magnitude, which is "how much light the glyphs subtract".
    const double mass_lin_inv = -ink_mass(fb_lin_inv, white);
    const double mass_enc_inv = -ink_mass(fb_enc_inv, white);
    std::printf("\n  black on white, the same string\n");
    std::printf("    linear blending   ink mass %10.4f\n", mass_lin_inv);
    std::printf("    encoded blending  ink mass %10.4f   (%.1f%% of correct)\n",
                mass_enc_inv, 100.0 * mass_enc_inv / mass_lin_inv);
    check(mass_enc_inv > mass_lin_inv, "blending codes makes dark-on-light text HEAVIER");

    std::printf("\n  SAME ERROR, OPPOSITE DIRECTIONS. That is why this bug survives\n");
    std::printf("  review: half the people looking at it see text that is too thin\n");
    std::printf("  and half see text that is too fat, and both blame the font.\n");

    // ---- THE THIRD MISTAKE: an sRGB atlas ----------------------------------
    //
    // Uploading coverage as an `_SRGB` texture makes the sampler decode an area
    // fraction. Simulated here by pushing each coverage byte through
    // `srgb_to_linear` before compositing, which is exactly what the hardware
    // would do.
    {
        font_atlas decoded = g_atlas;
        for (std::uint8_t& c : decoded.coverage)
        {
            c = static_cast<std::uint8_t>(
                std::lround(255.0f * engine::srgb_to_linear_u8(c)));
        }
        framebuffer fb_srgb = render_string(decoded, sample, white, black,
                                            overlay_blend::linear, 0.0f);
        const double mass_srgb = ink_mass(fb_srgb, black);
        std::printf("\n  atlas uploaded as _SRGB (the sampler 'decodes' coverage)\n");
        std::printf("    ink mass %10.4f   (%.1f%% of correct)\n",
                    mass_srgb, 100.0 * mass_srgb / mass_lin);
        check(mass_srgb < mass_lin, "an _SRGB coverage atlas makes text too thin");

        // AND NOW THE THING THAT FELL OUT OF PUTTING THE TWO NUMBERS SIDE BY
        // SIDE. They are the same, and not by coincidence. On white over black:
        //
        //   encoded blending:  code = 255a, light = srgb_to_linear(a)
        //   _SRGB atlas:       a' = srgb_to_linear(a), light = a'
        //
        // TWO BUGS IN TWO DIFFERENT FILES — one in the colour space of the
        // blend, one in the format of a texture — are the same function applied
        // at different points in the pipeline. Which means measuring
        // light-on-dark text CANNOT TELL THEM APART.
        std::printf("\n    and it is the SAME NUMBER as encoded blending (%.4f vs %.4f),\n",
                    mass_srgb, mass_enc);
        std::printf("    to within the 8-bit quantisation of the modified atlas — because\n");
        std::printf("    both are srgb_to_linear applied to the coverage, in two places.\n");
        check(std::fabs(mass_srgb - mass_enc) / mass_enc < 0.01,
              "the two bugs agree to within 1% on white-over-black");

        // SO HERE IS THE TEST THAT SEPARATES THEM, and it is the reason §G draws
        // the inverse case at all. An _SRGB atlas thins the glyph whichever way
        // the contrast runs, because it corrupts COVERAGE. Encoded blending
        // thins light-on-dark and FATTENS dark-on-light, because it corrupts the
        // lerp. Run both directions and the sign of the second tells you which
        // bug you have.
        framebuffer fb_srgb_inv = render_string(decoded, sample, black, white,
                                                overlay_blend::linear, 0.0f);
        const double mass_srgb_inv = -ink_mass(fb_srgb_inv, white);
        std::printf("\n    DIAGNOSTIC:                 white-on-black   black-on-white\n");
        std::printf("      encoded blending            %6.1f%%          %6.1f%%\n",
                    100.0 * mass_enc / mass_lin, 100.0 * mass_enc_inv / mass_lin_inv);
        std::printf("      _SRGB coverage atlas        %6.1f%%          %6.1f%%\n",
                    100.0 * mass_srgb / mass_lin, 100.0 * mass_srgb_inv / mass_lin_inv);
        check(mass_srgb_inv < mass_lin_inv,
              "an _SRGB atlas thins text in BOTH directions — which is the tell");

        std::printf("\n    NOTE HOW SMALL BOTH ARE. Neither looks broken; both look\n");
        std::printf("    like a lighter or heavier font weight — which is why they ship.\n");
    }

    // FIGURE 4'S RAMP. One pixel of white over black, at every tenth of
    // coverage, through both blend spaces — the curve the ink-mass totals above
    // are the area under.
    std::printf("\n  %-10s %-14s %-14s %s\n", "coverage", "correct light", "encoded light",
                "ratio");
    for (int i = 0; i <= 10; ++i)
    {
        const float a = static_cast<float>(i) / 10.0f;
        const double right = static_cast<double>(engine::srgb_to_linear_u8(
            engine::red_of(engine::blend_over(black, white, a))));
        const double wrong = static_cast<double>(engine::srgb_to_linear_u8(
            engine::red_of(engine::blend_over_encoded(black, white, a))));
        std::printf("  %-10.2f %-14.4f %-14.4f %s\n", static_cast<double>(a), right, wrong,
                    (right > 0.0) ? std::to_string(100.0 * wrong / right).substr(0, 5).c_str()
                                  : "-");
    }

    // ---- STEM DARKENING, and the reason it is not the default ---------------
    framebuffer fb_dark = render_string(g_atlas, sample, white, black,
                                        overlay_blend::linear, 0.2f);
    framebuffer fb_dark_inv = render_string(g_atlas, sample, black, white,
                                            overlay_blend::linear, 0.2f);
    const double mass_dark = ink_mass(fb_dark, black);
    const double mass_dark_inv = -ink_mass(fb_dark_inv, white);
    std::printf("\n  stem darkening k = 0.2, applied to coverage before compositing\n");
    std::printf("    white on black  %10.4f -> %10.4f  (%+.1f%%)\n",
                mass_lin, mass_dark, 100.0 * (mass_dark / mass_lin - 1.0));
    std::printf("    black on white  %10.4f -> %10.4f  (%+.1f%%)\n",
                mass_lin_inv, mass_dark_inv, 100.0 * (mass_dark_inv / mass_lin_inv - 1.0));
    check(mass_dark > mass_lin, "it adds weight to light text on dark");
    check(mass_dark_inv > mass_lin_inv, "and ALSO to dark text on light, which is wrong");
    std::printf("\n    ONE GLOBAL CONSTANT CANNOT BE RIGHT FOR BOTH: the correction\n");
    std::printf("    helps the first case and hurts the second by the same arithmetic,\n");
    std::printf("    because it does not know which way the contrast runs. That is why\n");
    std::printf("    it is a parameter here and zero by default.\n");

    check_near(static_cast<double>(engine::apply_stem_darkening(0.5f, 0.2f)), 0.5743, 1e-3,
               "coverage 0.5 at k=0.2 becomes 0.5^0.8");
    check_near(static_cast<double>(engine::apply_stem_darkening(0.0f, 0.2f)), 0.0, 1e-9,
               "and 0 stays 0");
    check_near(static_cast<double>(engine::apply_stem_darkening(1.0f, 0.2f)), 1.0, 1e-9,
               "and 1 stays 1");
}

// ===========================================================================
//  §H — where the overlay composites
// ===========================================================================

void section_h()
{
    section("H — before the curve or after it: what white costs");

    // Pure white text drawn INTO the HDR buffer is a linear 1.0, and the tonemap
    // then has its way with it.
    engine::tonemap_settings aces{};
    aces.op = engine::tonemap::aces;
    aces.exposure = 1.0f;

    const float through = engine::apply_tonemap(1.0f, engine::tonemap::aces);
    const Uint32 code = engine::linear_to_srgb_u8(through);
    std::printf("\n  linear 1.0 through the ACES fit -> %.6f -> sRGB code %u\n",
                static_cast<double>(through), code);
    check_near(static_cast<double>(through), 0.803797, 1e-5,
               "ACES(1.0) = 0.803797, which is sRGB code 232 and not 255");
    check(code < 255u, "so white text composited BEFORE the tonemap is not white");

    std::printf("\n  and it gets worse with exposure. The same white text, as the\n");
    std::printf("  auto-exposure moves:\n");
    std::printf("    %-10s %-12s %s\n", "exposure", "tonemapped", "sRGB code");
    for (const float e : {0.25f, 0.5f, 1.0f, 2.0f, 4.0f})
    {
        const float v = engine::apply_tonemap(1.0f * e, engine::tonemap::aces);
        std::printf("    %-10.2f %-12.6f %u\n", static_cast<double>(e),
                    static_cast<double>(v), engine::linear_to_srgb_u8(v));
    }
    std::printf("\n  A HUD that dims when the player walks into the sun is not a\n");
    std::printf("  stylistic choice, it is this table.\n");

    // The other operators, for completeness: the choice of curve changes the
    // number but not the conclusion.
    std::printf("\n  every operator, on a linear 1.0:\n");
    const engine::tonemap ops[] = {engine::tonemap::clamp, engine::tonemap::reinhard,
                                   engine::tonemap::reinhard_white, engine::tonemap::aces};
    for (const engine::tonemap op : ops)
    {
        const float v = engine::apply_tonemap(1.0f, op, 4.0f);
        std::printf("    %-16s %.6f -> code %u\n", engine::name_of(op),
                    static_cast<double>(v), engine::linear_to_srgb_u8(v));
    }
    check_near(static_cast<double>(engine::apply_tonemap(1.0f, engine::tonemap::clamp)), 1.0,
               1e-6, "only `clamp` leaves white alone, and it is the operator nobody ships");
}


// ===========================================================================
//  The GPU rig
// ===========================================================================

namespace {

constexpr int k_gpu_w = 256;
constexpr int k_gpu_h = 64;

/// A colour target plus the transfer buffer that reads it back — `verify_617`'s
/// `target`, unchanged, because a harness that reinvents its own readback is a
/// harness with two things that can be wrong.
struct target
{
    engine::gpu_device* dev = nullptr;
    SDL_GPUTexture* colour = nullptr;
    SDL_GPUTransferBuffer* readback = nullptr;
    int w = 0;
    int h = 0;

    bool create(engine::gpu_device& d, int width, int height, SDL_GPUTextureFormat fmt)
    {
        dev = &d;
        w = width;
        h = height;

        SDL_GPUTextureCreateInfo ti{};
        ti.type = SDL_GPU_TEXTURETYPE_2D;
        ti.format = fmt;
        ti.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        ti.width = static_cast<Uint32>(width);
        ti.height = static_cast<Uint32>(height);
        ti.layer_count_or_depth = 1;
        ti.num_levels = 1;
        ti.sample_count = SDL_GPU_SAMPLECOUNT_1;
        colour = SDL_CreateGPUTexture(d.handle(), &ti);
        if (colour == nullptr) { return false; }

        SDL_GPUTransferBufferCreateInfo tb{};
        tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
        tb.size = static_cast<Uint32>(width * height * 4);
        readback = SDL_CreateGPUTransferBuffer(d.handle(), &tb);
        return readback != nullptr;
    }

    void destroy()
    {
        if (dev == nullptr) { return; }
        if (readback != nullptr) { SDL_ReleaseGPUTransferBuffer(dev->handle(), readback); }
        if (colour != nullptr) { SDL_ReleaseGPUTexture(dev->handle(), colour); }
        readback = nullptr;
        colour = nullptr;
    }
};

std::vector<Uint8> download(target& t, SDL_GPUCommandBuffer* cb)
{
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cb);
    SDL_GPUTextureRegion src{};
    src.texture = t.colour;
    src.w = static_cast<Uint32>(t.w);
    src.h = static_cast<Uint32>(t.h);
    src.d = 1;
    SDL_GPUTextureTransferInfo dst{};
    dst.transfer_buffer = t.readback;
    dst.pixels_per_row = static_cast<Uint32>(t.w);
    dst.rows_per_layer = static_cast<Uint32>(t.h);
    SDL_DownloadFromGPUTexture(copy, &src, &dst);
    SDL_EndGPUCopyPass(copy);

    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cb);
    if (fence == nullptr) { return {}; }
    SDL_WaitForGPUFences(t.dev->handle(), true, &fence, 1);
    SDL_ReleaseGPUFence(t.dev->handle(), fence);

    std::vector<Uint8> px(static_cast<std::size_t>(t.w) * static_cast<std::size_t>(t.h) * 4u);
    const void* mapped = SDL_MapGPUTransferBuffer(t.dev->handle(), t.readback, false);
    if (mapped == nullptr) { return {}; }
    std::memcpy(px.data(), mapped, px.size());
    SDL_UnmapGPUTransferBuffer(t.dev->handle(), t.readback);
    return px;
}

struct rig
{
    engine::gpu_device gpu;
    engine::gpu_overlay overlay;
    target back;                ///< the "swapchain", an _SRGB target
    target back_unorm;          ///< the fallback path, for §G's GPU half
    bool ok = false;
};

bool build_rig(rig& r)
{
    if (!r.gpu.create(nullptr, false).ok()) { return false; }

    engine::gpu_shader vert, frag;
    if (!vert.load(r.gpu, "overlay.vert", engine::shader_stage::vertex)) { return false; }
    if (!frag.load(r.gpu, "overlay.frag", engine::shader_stage::fragment)) { return false; }

    // LESSON 4.5'S CHECK, ON THE NEW PIPELINE. `check_layout` compares the
    // attributes the pipeline declares against the inputs the shader reflects,
    // which is the only thing in the build that can catch a disagreement between
    // two files written in different languages.
    {
        engine::pipeline_desc probe(r.gpu, vert.handle(), frag.handle());
        probe.vertex_buffer(0, static_cast<Uint32>(sizeof(engine::overlay_vertex)));
        probe.attribute(0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 0);
        probe.attribute(1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 8);
        probe.attribute(2, 0, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, 16);
        const engine::layout_report rep = probe.check_layout(vert.inputs(), "overlay.vert");
        check_eq(rep.missing, 0, "no shader input is left unfed");
        check_eq(rep.extra, 0, "no attribute is declared that the shader ignores");
        check_eq(rep.type_mismatch, 0, "no float bits arrive where the shader reads an int");
        check_eq(rep.overrun, 0, "every attribute fits inside the vertex pitch");
    }

    // THE TARGET IS `_SRGB`, because that is what this engine asks the swapchain
    // for (6.1) and because the whole of §G depends on the blender decoding.
    if (!r.back.create(r.gpu, k_gpu_w, k_gpu_h, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB))
    {
        return false;
    }
    if (!r.back_unorm.create(r.gpu, k_gpu_w, k_gpu_h, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM))
    {
        return false;
    }
    if (!r.overlay.create(r.gpu, vert.handle(), frag.handle(),
                          SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB, 1024))
    {
        return false;
    }

    SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
    if (cb == nullptr) { return false; }
    const bool font_ok = r.overlay.set_font(r.gpu, cb, g_atlas);
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cb);
    if (fence != nullptr)
    {
        SDL_WaitForGPUFences(r.gpu.handle(), true, &fence, 1);
        SDL_ReleaseGPUFence(r.gpu.handle(), fence);
    }
    if (!font_ok) { return false; }

    r.ok = true;
    return true;
}

/// The one overlay every GPU section draws: a panel, a label and a number. Built
/// by a function so the CPU and GPU paths cannot possibly be given different
/// input — which is the whole force of §J's comparison.
void build_demo_overlay(overlay_batch& batch, const font_atlas& atlas, int w, int h)
{
    batch.begin(w, h);
    batch.rect(atlas, 4.0f, 4.0f, 180.0f, 44.0f, engine::pack_argb(24, 26, 34, 220));
    (void)batch.text_top_left(atlas, "frame 12.34 ms", vec2{10.0f, 8.0f},
                              engine::pack_argb(236, 240, 248));
    (void)batch.text_top_left(atlas, "draws 41  tris 8192", vec2{10.0f, 24.0f},
                              engine::pack_argb(150, 220, 170));
}

// ---------------------------------------------------------------------------
//  The overlay, as a frame-graph pass
// ---------------------------------------------------------------------------

struct overlay_pass_data
{
    const engine::gpu_overlay* overlay = nullptr;
    const overlay_batch* batch = nullptr;
    bool shader_encodes = false;
};

void exec_overlay(const engine::fg_pass_context& ctx, void* user)
{
    const auto* d = static_cast<const overlay_pass_data*>(user);
    d->overlay->record(ctx.cb, ctx.pass, *d->batch, d->shader_encodes, 0.0f);
}

/// A pass that clears the target to a known colour, standing in for the resolve.
struct clear_pass_data { int unused = 0; };
void exec_clear(const engine::fg_pass_context&, void*) {}

} // namespace

// ===========================================================================
//  §I — the frame graph, used by somebody who did not write it
// ===========================================================================

void section_i(rig& r)
{
    section("I — the overlay, declared as a frame-graph pass");

    using engine::fg_init;
    using engine::fg_texture;
    using engine::fg_texture_desc;
    using engine::fg_use;
    using engine::frame_graph;

    overlay_batch batch;
    build_demo_overlay(batch, g_atlas, k_gpu_w, k_gpu_h);

    SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
    check(r.overlay.upload(cb, batch), "the geometry uploaded outside any pass");

    fg_texture_desc back_desc{};
    back_desc.width = k_gpu_w;
    back_desc.height = k_gpu_h;
    back_desc.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
    back_desc.sampled = false;

    frame_graph fg;
    const fg_texture back0 = fg.import("backbuffer", r.back.colour, back_desc);

    clear_pass_data cd{};
    const int p_clear = fg.add_pass("resolve (stand-in)", &exec_clear, &cd);
    const fg_texture back1 = fg.clear(p_clear, back0, SDL_FColor{0.05f, 0.06f, 0.08f, 1.0f});

    overlay_pass_data od{};
    od.overlay = &r.overlay;
    od.batch = &batch;
    od.shader_encodes = false;

    // THE ONE LINE THIS SECTION EXISTS FOR. `keep` says "my output depends on
    // what was already in this target", which for an alpha blend is simply true
    // — the destination is an operand of `over`. Everything else is deduced.
    const int p_overlay = fg.add_pass("overlay", &exec_overlay, &od);
    const fg_texture back2 = fg.keep(p_overlay, back1);
    check(back2.valid(), "keep() returned a new version of the imported resource");

    check(fg.compile(r.gpu), "the graph compiled");
    check_eq(fg.live_pass_count(), 2, "both passes are live");
    check_eq(fg.culled_pass_count(), 0, "and neither was culled");
    check_eq(fg.scheduled(0), p_clear, "the resolve is scheduled first");
    check_eq(fg.scheduled(1), p_overlay, "and the overlay second — derived, not stated");

    // THE DERIVED OPS. The overlay LOADs (because `keep`), and the pass before it
    // must STORE (because a later pass consumes that version — and because the
    // resource is imported, so its final value is observable anyway).
    std::printf("\n  %-22s %-14s %-10s %-10s\n", "pass", "resource", "load", "store");
    for (int i = 0; i < fg.live_pass_count(); ++i)
    {
        const int p = fg.scheduled(i);
        for (int a = 0; a < fg.access_count(p); ++a)
        {
            const frame_graph::access_report rep = fg.access_at(p, a);
            const char* load = (rep.load_op == SDL_GPU_LOADOP_LOAD) ? "LOAD"
                             : (rep.load_op == SDL_GPU_LOADOP_CLEAR) ? "CLEAR" : "DONT_CARE";
            const char* store = (rep.store_op == SDL_GPU_STOREOP_STORE) ? "STORE" : "DONT_CARE";
            std::printf("  %-22s %-14s %-10s %-10s\n", fg.pass_name(p),
                        fg.resource_name(rep.resource), load, store);
            if (p == p_overlay)
            {
                check(rep.load_op == SDL_GPU_LOADOP_LOAD,
                      "the overlay's load op was DERIVED as LOAD");
                check(rep.store_op == SDL_GPU_STOREOP_STORE,
                      "and its store op as STORE, because the resource is imported");
            }
            else
            {
                check(rep.store_op == SDL_GPU_STOREOP_STORE,
                      "the pass before it must STORE, because somebody reads that version");
            }
        }
    }

    fg.execute(cb);
    const std::vector<Uint8> px = download(r.back, cb);
    check(!px.empty(), "the frame executed and read back");

    // The picture is not blank and not uniform: the panel is there and so is the
    // text. A weak check, and it is the RIGHT weak check here — §J does the
    // strong one, and this section is about the graph.
    int lit = 0;
    for (std::size_t i = 0; i < px.size(); i += 4u)
    {
        if (px[i] > 40u) { ++lit; }
    }
    std::printf("\n  %d of %d pixels are brighter than the panel\n", lit, k_gpu_w * k_gpu_h);
    check(lit > 100, "there is text on the screen");

    // ---- AND THE HONEST REPORT ON THE API ----------------------------------
    //
    // 6.17 predicted this lesson would be its API's first external user and
    // named the thing that had never been exercised: `keep` on an IMPORTED
    // resource. Exercise it directly — an overlay with no resolve in front of it
    // at all, blending onto whatever the texture already holds.
    {
        frame_graph solo;
        const fg_texture b0 = solo.import("backbuffer", r.back.colour, back_desc);
        const int p = solo.add_pass("overlay only", &exec_overlay, &od);
        const fg_texture b1 = solo.keep(p, b0);
        check(b1.valid(), "keep() on version 0 of an IMPORT returned a handle");
        check(solo.compile(r.gpu),
              "and it compiles — an import's version 0 is a real value, not undefined");
        check_eq(solo.live_pass_count(), 1, "one pass, and it survives culling");
        const frame_graph::access_report rep = solo.access_at(p, 0);
        check(rep.load_op == SDL_GPU_LOADOP_LOAD, "LOAD, as it must be");
        check(rep.init == fg_init::keep, "and the graph reports the verb it was given");
    }

    // The mistake the graph now catches that nothing caught before: declaring
    // the overlay with `discard_write` instead of `keep` — "I write every texel
    // and read none", which for a blend is a lie. The graph cannot know it is a
    // lie; what it CAN do is make the two statements different words at the call
    // site instead of two load ops buried in a struct three files away.
    {
        frame_graph wrong;
        const fg_texture b0 = wrong.import("backbuffer", r.back.colour, back_desc);
        const int p = wrong.add_pass("overlay, mis-declared", &exec_overlay, &od);
        const fg_texture b1 = wrong.discard_write(p, b0);
        (void)b1;
        check(wrong.compile(r.gpu), "discard_write also compiles — this is NOT caught");
        const frame_graph::access_report rep = wrong.access_at(p, 0);
        check(rep.load_op == SDL_GPU_LOADOP_DONT_CARE,
              "and it derives DONT_CARE, which would erase the frame under the text");
        std::printf("\n  SO THE GRAPH DOES NOT CATCH EVERYTHING, and saying so is the\n");
        std::printf("  point: it turns a load op into a SENTENCE ABOUT ARITHMETIC, which\n");
        std::printf("  a pass author can answer. It cannot check that the answer is true.\n");
    }
}

// ===========================================================================
//  §J — THE INSTRUMENT: the same overlay, two renderers
// ===========================================================================

void section_j(rig& r)
{
    section("J — the same batch, composited on the CPU and on the GPU");

    overlay_batch batch;
    build_demo_overlay(batch, g_atlas, k_gpu_w, k_gpu_h);
    std::printf("\n  %d quads, %u vertex bytes, %u index bytes\n",
                batch.quad_count(), batch.vertex_bytes(), batch.index_bytes());

    const Uint32 clear = engine::pack_argb(13, 15, 20);

    // ---- CPU -----------------------------------------------------------------
    framebuffer cpu(k_gpu_w, k_gpu_h);
    cpu.clear(clear);
    engine::composite_overlay(cpu, g_atlas, batch, overlay_blend::linear, 0.0f);

    // ---- GPU -----------------------------------------------------------------
    SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
    check(r.overlay.upload(cb, batch), "uploaded");

    SDL_GPUColorTargetInfo ci{};
    ci.texture = r.back.colour;
    // The clear colour goes in as LINEAR and the `_SRGB` target encodes it, so
    // it has to be the DECODED form of the CPU's clear — otherwise the two
    // backgrounds differ and every pixel is a failure for the wrong reason.
    ci.clear_color = SDL_FColor{engine::srgb_to_linear_u8(engine::red_of(clear)),
                                engine::srgb_to_linear_u8(engine::green_of(clear)),
                                engine::srgb_to_linear_u8(engine::blue_of(clear)),
                                1.0f};
    ci.load_op = SDL_GPU_LOADOP_CLEAR;
    ci.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cb, &ci, 1, nullptr);
    r.overlay.record(cb, pass, batch, /*shader_encodes*/ false, 0.0f);
    SDL_EndGPURenderPass(pass);

    const std::vector<Uint8> gpu = download(r.back, cb);
    check(!gpu.empty(), "read back");
    if (gpu.empty()) { return; }

    // ---- Compare -------------------------------------------------------------
    //
    // A TOLERANCE, AND IT IS NOT ZERO. The two paths do the same arithmetic in
    // different orders and different precisions: the CPU decodes bytes to float,
    // lerps and re-encodes with `engine::linear_to_srgb`; the GPU's ROP does it
    // in fixed-function hardware whose intermediate precision is not specified.
    // 6.17 could demand bit-equality because both sides were the same shader;
    // here the honest claim is "within one code", and the instrument reports the
    // distribution rather than a yes/no — a comparison that can only say "same"
    // or "different" cannot tell a rounding difference from a bug.
    int differing = 0, worst = 0;
    long long total_diff = 0;
    int histogram[8] = {0};
    for (int y = 0; y < k_gpu_h; ++y)
    {
        for (int x = 0; x < k_gpu_w; ++x)
        {
            const Uint32 c = cpu.pixel_at(x, y);
            const std::size_t i = (static_cast<std::size_t>(y) * k_gpu_w
                                   + static_cast<std::size_t>(x)) * 4u;
            const int dr = std::abs(static_cast<int>(gpu[i + 0]) - engine::red_of(c));
            const int dg = std::abs(static_cast<int>(gpu[i + 1]) - engine::green_of(c));
            const int db = std::abs(static_cast<int>(gpu[i + 2]) - engine::blue_of(c));
            const int d = std::max({dr, dg, db});
            if (d > 0) { ++differing; }
            worst = std::max(worst, d);
            total_diff += d;
            histogram[std::min(d, 7)]++;
        }
    }
    const int pixels = k_gpu_w * k_gpu_h;
    std::printf("\n  %d of %d pixels differ at all; worst channel difference %d code%s\n",
                differing, pixels, worst, worst == 1 ? "" : "s");
    std::printf("  mean |difference| over all channels: %.4f codes\n",
                static_cast<double>(total_diff) / pixels);
    std::printf("\n  %-12s %s\n", "difference", "pixels");
    for (int d = 0; d < 8; ++d)
    {
        if (histogram[d] > 0)
        {
            std::printf("  %-12d %d%s\n", d, histogram[d], d == 7 ? " (7 or more)" : "");
        }
    }
    check(worst <= 1, "every pixel agrees to within one sRGB code");

    // ---- THE CONTROL, which is 6.16's `golden_615` finding acted on ---------
    //
    // A comparison that cannot fail proves nothing. Composite the CPU side in
    // the WRONG blend space and re-run the same comparison: it must report a
    // large difference, or the instrument is measuring nothing.
    framebuffer control(k_gpu_w, k_gpu_h);
    control.clear(clear);
    engine::composite_overlay(control, g_atlas, batch, overlay_blend::encoded, 0.0f);

    int control_diff = 0, control_worst = 0;
    for (int y = 0; y < k_gpu_h; ++y)
    {
        for (int x = 0; x < k_gpu_w; ++x)
        {
            const Uint32 c = control.pixel_at(x, y);
            const std::size_t i = (static_cast<std::size_t>(y) * k_gpu_w
                                   + static_cast<std::size_t>(x)) * 4u;
            const int d = std::max({std::abs(static_cast<int>(gpu[i + 0]) - engine::red_of(c)),
                                    std::abs(static_cast<int>(gpu[i + 1]) - engine::green_of(c)),
                                    std::abs(static_cast<int>(gpu[i + 2]) - engine::blue_of(c))});
            if (d > 1) { ++control_diff; }
            control_worst = std::max(control_worst, d);
        }
    }
    std::printf("\n  CONTROL: the same comparison against a CPU render in the WRONG\n");
    std::printf("  blend space reports %d differing pixels, worst %d codes.\n",
                control_diff, control_worst);
    check(control_diff > 200, "the comparison CAN fail, so passing it means something");

    // ---- AND THE FALLBACK PATH, measured on real hardware -------------------
    //
    // §G measured the encoded-blending error on the CPU. Here is the same error
    // produced by the GPU's own blender, by pointing the same draw at a plain
    // UNORM target with `shader_encodes = 1`. Nothing about the shader's maths
    // changes; the ROP's does.
    {
        engine::gpu_overlay unorm_overlay;
        engine::gpu_shader vert, frag;
        const bool loaded =
            vert.load(r.gpu, "overlay.vert", engine::shader_stage::vertex)
            && frag.load(r.gpu, "overlay.frag", engine::shader_stage::fragment);
        check(loaded, "reloaded the shaders for a second pipeline");
        check(unorm_overlay.create(r.gpu, vert.handle(), frag.handle(),
                                   SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 1024),
              "a pipeline targeting a plain UNORM format");

        SDL_GPUCommandBuffer* cb2 = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
        check(unorm_overlay.set_font(r.gpu, cb2, g_atlas), "atlas uploaded again");
        check(unorm_overlay.upload(cb2, batch), "geometry uploaded");

        SDL_GPUColorTargetInfo ci2{};
        ci2.texture = r.back_unorm.colour;
        // A plain UNORM target stores codes, so the clear colour IS the code.
        ci2.clear_color = SDL_FColor{engine::red_of(clear) / 255.0f,
                                     engine::green_of(clear) / 255.0f,
                                     engine::blue_of(clear) / 255.0f, 1.0f};
        ci2.load_op = SDL_GPU_LOADOP_CLEAR;
        ci2.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPURenderPass* pass2 = SDL_BeginGPURenderPass(cb2, &ci2, 1, nullptr);
        unorm_overlay.record(cb2, pass2, batch, /*shader_encodes*/ true, 0.0f);
        SDL_EndGPURenderPass(pass2);

        const std::vector<Uint8> unorm_px = download(r.back_unorm, cb2);
        check(!unorm_px.empty(), "read back the fallback path");

        // Compare it against the CPU's WRONG path. If the hardware really is
        // doing `blend_over_encoded`, these must agree — and they must disagree
        // with the correct one.
        int vs_wrong = 0, vs_right = 0;
        for (int y = 0; y < k_gpu_h && !unorm_px.empty(); ++y)
        {
            for (int x = 0; x < k_gpu_w; ++x)
            {
                const std::size_t i = (static_cast<std::size_t>(y) * k_gpu_w
                                       + static_cast<std::size_t>(x)) * 4u;
                const Uint32 w = control.pixel_at(x, y);
                const Uint32 c = cpu.pixel_at(x, y);
                if (std::abs(static_cast<int>(unorm_px[i]) - engine::red_of(w)) > 1)
                {
                    ++vs_wrong;
                }
                if (std::abs(static_cast<int>(unorm_px[i]) - engine::red_of(c)) > 1)
                {
                    ++vs_right;
                }
            }
        }
        std::printf("\n  the UNORM target's own blender, against the two CPU paths:\n");
        std::printf("    vs `blend_over_encoded` (wrong): %d pixels differ\n", vs_wrong);
        std::printf("    vs `blend_over`         (right): %d pixels differ\n", vs_right);
        check(vs_wrong < vs_right,
              "the hardware fallback IS `blend_over_encoded`, in silicon");
        unorm_overlay.destroy();
    }
}

// ===========================================================================
//  §K — what a frame of text costs
// ===========================================================================

void section_k()
{
    section("K — the cost of a frame of text");

    overlay_batch batch;

    // WARM IT UP FIRST. The claim is about a STEADY-STATE frame: the vectors
    // have reached their size and nothing grows. Counting the first frame would
    // measure the allocator warming up, which is 6.17's pooled-bytes mistake in
    // a new place — a measurement that only works once.
    for (int i = 0; i < 4; ++i) { build_demo_overlay(batch, g_atlas, k_gpu_w, k_gpu_h); }

    g_allocations = 0;
    g_count_allocations = true;
    for (int i = 0; i < 200; ++i) { build_demo_overlay(batch, g_atlas, k_gpu_w, k_gpu_h); }
    g_count_allocations = false;

    std::printf("\n  200 rebuilds of a %d-quad overlay: %zu heap allocations\n",
                batch.quad_count(), g_allocations);
    check_eq(static_cast<long long>(g_allocations), 0,
             "a steady-state frame of text allocates nothing");

    const std::size_t quads = static_cast<std::size_t>(batch.quad_count());
    const engine::bench_result build = engine::bench_run(quads, 200, [&]
    {
        build_demo_overlay(batch, g_atlas, k_gpu_w, k_gpu_h);
        return static_cast<double>(batch.quad_count());
    });
    std::printf("  build cost: %.1f ns per quad, %.3f us for the whole overlay"
                " (spread %.2f)\n",
                build.median_ns, build.median_ns * static_cast<double>(quads) / 1000.0,
                build.spread());

    // And what it costs the GPU to be TOLD about: one draw call, whatever the
    // string count.
    std::printf("\n  submission: %u vertex bytes + %u index bytes, ONE draw call\n",
                batch.vertex_bytes(), batch.index_bytes());
    std::printf("  the atlas: %zu bytes as R8; %zu as RGBA8 (4x)\n",
                g_atlas.byte_count(), g_atlas.byte_count() * 4u);

    // THE BAKE is the expensive part, and it happens once.
    {
        font_atlas throwaway;
        font_bake_options opts{};
        opts.pixel_height = 16.0f;
        const engine::bench_result bake = engine::bench_run(1, 5, [&]
        {
            (void)engine::load_font(font_path(), opts, throwaway);
            return static_cast<double>(throwaway.packed_texels);
        });
        std::printf("\n  baking the atlas: %.3f ms, once, at load time\n",
                    bake.median_ns / 1e6);
    }
}

// ===========================================================================
//  §L — the golden, and why it is blind to this lesson
// ===========================================================================

void section_l()
{
    section("L — what the characterization shot can and cannot see");

    // STRUCTURAL, NOT HOPEFUL — 6.17's method. `write_reference_shot` renders
    // through `soft_renderer.cpp` and `raster.cpp`. Neither includes
    // `overlay.hpp`, `font.hpp` or `gpu_overlay.hpp`, and `demo_scene.cpp` draws
    // no text. So the golden CANNOT move, and an unchanged hash is evidence of
    // nothing about this lesson.
    std::printf("\n  write_reference_shot -> soft_renderer.cpp + raster.cpp\n");
    std::printf("  neither includes font.hpp, overlay.hpp or gpu_overlay.hpp,\n");
    std::printf("  and demo_scene.cpp draws no text.\n");
    std::printf("\n  So the golden is a NULL INSTRUMENT for the fourth lesson running.\n");
    std::printf("  It is still worth running: it proves this lesson did not break\n");
    std::printf("  anything ELSE, which is a different claim and a useful one.\n");
    std::printf("\n  The real test is §J, which had to be BUILT.\n");

    // `hello_cube` is where the software overlay actually ships, and it has its
    // own `--shot`. Say so, so a reader knows where to look.
    std::printf("\n  The software overlay's own picture: ./build/demos/hello_cube --shot x.ppm\n");
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    std::printf("Lesson 6.18 — Text and 2D Overlay Rendering\n");

    section_a();
    if (!g_atlas.valid())
    {
        std::printf("\nthe font did not load; nothing else can run\n");
        return 1;
    }
    section_b();
    section_c();
    section_d();
    section_e();
    section_f();
    section_g();
    section_h();

    // SDL_GPU needs the video subsystem even for a windowless device — the
    // failure without it is `SDL_CreateGPUDevice failed: Video subsystem not
    // initialized`, which reads like a driver problem and is not.
    rig r;
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::printf("\n  ---- SDL_Init(VIDEO): %s\n", SDL_GetError());
    }
    if (build_rig(r))
    {
        std::printf("\n  GPU: driver '%s'\n", SDL_GetGPUDeviceDriver(r.gpu.handle()));
        section_i(r);
        section_j(r);
    }
    else
    {
        std::printf("\n  no GPU device; §I and §J skipped\n");
    }
    section_k();
    section_l();

    r.overlay.destroy();
    r.back.destroy();
    r.back_unorm.destroy();

    std::printf("\n===========================================================================\n");
    std::printf("  %d failure%s\n", g_failures, g_failures == 1 ? "" : "s");
    std::printf("===========================================================================\n");
    return g_failures == 0 ? 0 : 1;
}
