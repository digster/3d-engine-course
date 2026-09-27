// engine/include/engine/gfx/overlay.hpp — 2D, on top, in pixels.
//
// Lesson 6.18. Module 1 owned the 2D case completely: `put_pixel`, `fill_rect`,
// a blit, and a framebuffer the student wrote every byte of. Then Module 4 moved
// the renderer to the GPU and the 2D path did not come along, because nothing
// needed it — every demo since has been a camera looking at a mesh. This file is
// 2D arriving on the other side of that boundary, and arriving in a form BOTH
// renderers can consume, which is the property that makes it testable.
//
// ---------------------------------------------------------------------------
// WHAT AN OVERLAY IS, PRECISELY
// ---------------------------------------------------------------------------
//
// A list of axis-aligned quads in PIXEL SPACE, each carrying a colour and a
// rectangle of the glyph atlas. That is the whole data model, and its smallness
// is the point: there is no scene, no camera, no depth, no sort key, no
// material. An overlay quad is decided by the code that asks for it and drawn in
// the order it was asked for, which is exactly the semantics a HUD wants and
// exactly the semantics a 3D renderer cannot give you.
//
// PIXEL SPACE, ORIGIN TOP-LEFT, +Y DOWN — `font.hpp` fixes this and says why the
// flip to clip space happens in exactly one place. Note what this buys: an
// overlay laid out at 1280x720 does not scale with the window, it RE-LAYS OUT,
// because text that scales is text that resamples and text that resamples is
// soft. That is a real design decision with a real cost (a resize re-bakes
// nothing but re-lays everything) and the alternative — a virtual canvas scaled
// to fit — is what makes so much game UI look slightly blurry.
//
// ---------------------------------------------------------------------------
// ONE PIPELINE, ONE TEXTURE, TWO KINDS OF QUAD
// ---------------------------------------------------------------------------
//
// A panel behind a readout is an untextured rectangle; the readout is textured
// by the glyph atlas. Those look like two different draws and they are not: a
// solid quad is a textured quad whose coverage happens to be 1 everywhere, and
// `font_atlas` bakes a 2x2 fully-covered block for precisely this. So the panel
// and the text are in ONE vertex buffer, in ONE draw call, with no state change
// between them — which is why `rect()` and `text()` below live on the same type
// and write into the same arrays.
//
// ---------------------------------------------------------------------------
// THE COLOUR SPACE, WHICH IS THE REAL SUBJECT
// ---------------------------------------------------------------------------
//
// Compositing a glyph is `over`: `result = src * a + dst * (1 - a)`, with `a`
// the glyph's coverage. Lesson 6.11 established — with a number — that `over` is
// a LERP and therefore must happen in linear light, and that doing it on stored
// sRGB codes gives 43% of the light it should at half coverage.
//
// **Text is where that error is most visible, and it has a name: stem weight.**
// A 16 px glyph is mostly edge. Its stems are one to two pixels wide, so a large
// fraction of its pixels are partially covered, so a large fraction of its
// pixels are exactly the case where the two blend spaces disagree most. Get it
// wrong and light text on a dark ground looks THIN and spindly while dark text
// on a light ground looks FAT — the same error, in opposite directions, which is
// why it is so often mistaken for a font problem or a hinting problem.
//
// `composite_overlay` therefore takes the blend space as a parameter, with the
// wrong one available on purpose. It is the fifth member of a family this course
// keeps deliberately: `draw_line_naive` (1.4), `mix_encoded` (1.6),
// `blend_space::encoded` (2.4), `interpolation::affine` (3.2). A failure you can
// summon with one keystroke teaches more than a paragraph describing it, and §7
// of the lesson summons this one and measures it.
//
// AND THE HONEST PART, which most treatments of this skip. Linear compositing is
// physically correct and it is *not* what a font's outlines were tuned against.
// Type designers and hinting engines have spent thirty years compensating for
// sRGB-space blending, so switching to correct blending makes light-on-dark text
// measurably thinner than the designer intended. The answer is not to go back to
// the wrong arithmetic; it is **stem darkening** — a coverage adjustment applied
// before compositing, which every serious text renderer has and which §7.4
// derives, measures, and then declines to enable by default.

#pragma once

#include <engine/gfx/colour.hpp>
#include <engine/gfx/font.hpp>
#include <engine/math/vec2.hpp>

#include <SDL3/SDL.h>

#include <span>
#include <string_view>
#include <vector>

namespace engine {

class framebuffer;

/// One corner of one quad. **20 bytes, and the layout is the interface** — the
/// GPU reads this memory through a vertex attribute description, so every field's
/// offset is load-bearing.
///
/// THE COLOUR IS FOUR BYTES AND NOT A `Uint32`, DELIBERATELY. Everywhere else in
/// this engine a colour is a packed word in **ARGB** order (`colour.hpp`, since
/// Lesson 1.6). A vertex attribute is not a word: `UBYTE4_NORM` hands the shader
/// the four bytes **in memory order**, so a packed ARGB word arrives on a
/// little-endian machine as (B, G, R, A) and the text comes out blue. That is not
/// a hypothetical — Lesson 6.15 shipped exactly this bug in a BRDF lookup table,
/// where it survived review because green is bits 8-15 in both layouts and so
/// half the data read correctly.
///
/// Naming the four bytes makes the question disappear rather than making it
/// catchable. `set_colour` is the one place the ARGB convention is unpacked.
struct overlay_vertex
{
    float x = 0.0f, y = 0.0f;   ///< pixel space, origin top-left, +y down
    float u = 0.0f, v = 0.0f;   ///< normalized atlas coordinates
    Uint8 r = 255, g = 255, b = 255, a = 255;   ///< sRGB-encoded, STRAIGHT alpha

    /// Unpack one of this engine's ARGB words into the four bytes above.
    void set_colour(Uint32 argb)
    {
        r = red_of(argb);
        g = green_of(argb);
        b = blue_of(argb);
        a = alpha_of(argb);
    }
};

static_assert(sizeof(overlay_vertex) == 20, "the GPU reads this layout by offset");

/// The most quads one batch may hold.
///
/// **Set by the index type, not by taste.** Indices are 16-bit, so vertex
/// indices must stay below 65,536 and four vertices per quad puts the ceiling at
/// 16,384. A 32-bit index buffer would lift it and double the index bandwidth
/// for a case that does not exist: 16,384 quads is about 16,000 characters on
/// screen at once, which is forty times what a debug overlay shows.
inline constexpr int k_max_overlay_quads = 16384;

/// Accumulated 2D geometry for one frame, in pixel space.
///
/// **Cleared and refilled every frame, and it keeps its storage.** `begin()`
/// resizes the arrays to zero without freeing them, so after the first frame an
/// overlay of a stable size performs no allocation at all — the same argument
/// `frame_graph` (6.17) makes for its fixed-capacity arrays, applied to a
/// container that can legitimately grow.
class overlay_batch
{
public:
    /// Start a frame. The viewport is remembered because the vertex positions
    /// are in pixels and something has to know what to divide by; `gpu_overlay`
    /// pushes it as a uniform and the software compositor ignores it.
    void begin(int viewport_width, int viewport_height);

    /// An axis-aligned rectangle in the given colour. Half-open: `[x0, x1)`.
    ///
    /// @param atlas needed only for `solid_u/solid_v` — the fully-covered texel
    ///        that makes this a textured draw like every other one.
    void rect(const font_atlas& atlas, float x0, float y0, float x1, float y1, Uint32 argb);

    /// A string, with its BASELINE ORIGIN at `pen`.
    ///
    /// Not the top-left corner: `pen.y` is the baseline, and the top of the line
    /// box is `pen.y - atlas.ascent`. `text_top_left` below exists so callers
    /// who think in boxes do not have to remember that.
    ///
    /// @return what the string occupied, so the caller can size a panel or place
    ///         the next run without measuring twice.
    text_metrics text(const font_atlas& atlas, std::string_view utf8, vec2 pen, Uint32 argb,
                      const text_layout_options& options = {});

    /// The same, positioned by the top-left of its line box.
    text_metrics text_top_left(const font_atlas& atlas, std::string_view utf8, vec2 top_left,
                               Uint32 argb, const text_layout_options& options = {});

    [[nodiscard]] std::span<const overlay_vertex> vertices() const { return vertices_; }
    [[nodiscard]] std::span<const Uint16> indices() const { return indices_; }

    [[nodiscard]] int quad_count() const { return static_cast<int>(vertices_.size() / 4u); }
    [[nodiscard]] bool empty() const { return vertices_.empty(); }

    /// Quads refused because the batch was full. **Counted, not silently
    /// dropped** — an overlay that stops drawing at 16,384 quads with no
    /// complaint is a bug that reads as "the log window stops updating".
    [[nodiscard]] int overflowed() const { return overflowed_; }

    [[nodiscard]] int viewport_width() const { return viewport_w_; }
    [[nodiscard]] int viewport_height() const { return viewport_h_; }

    [[nodiscard]] Uint32 vertex_bytes() const
    {
        return static_cast<Uint32>(vertices_.size() * sizeof(overlay_vertex));
    }
    [[nodiscard]] Uint32 index_bytes() const
    {
        return static_cast<Uint32>(indices_.size() * sizeof(Uint16));
    }

private:
    /// Push one quad. Returns false when the batch is full.
    bool push_quad(float x0, float y0, float x1, float y1,
                   float u0, float v0, float u1, float v1, Uint32 argb);

    std::vector<overlay_vertex> vertices_;
    std::vector<Uint16> indices_;
    std::vector<glyph_quad> scratch_;   ///< reused by `text()`, so it stops allocating
    int viewport_w_ = 0;
    int viewport_h_ = 0;
    int overflowed_ = 0;
};

// ---- The software path ------------------------------------------------------

/// Which arithmetic `composite_overlay` performs.
enum class overlay_blend
{
    /// Decode both operands, lerp in linear light, re-encode. **Correct**, and
    /// what an `_SRGB` render target's blend hardware does for free.
    linear,

    /// Lerp the stored sRGB codes. **Wrong**, kept as an instrument: it is what
    /// a plain UNORM target with a shader that encodes its own output does in
    /// silicon, which is a configuration this engine can still be in
    /// (`gpu_device` falls back to it when SDR_LINEAR is unavailable) and which
    /// a great many shipped renderers are in permanently.
    encoded
};

[[nodiscard]] const char* name_of(overlay_blend b);

/// Composite a batch into a framebuffer, on the CPU.
///
/// **This is the software rasterizer's overlay, and it exists for two reasons.**
/// The first is that it is useful: `hello_cube` has no GPU device and still
/// wants to say what it is doing. The second is that it is the INSTRUMENT — the
/// GPU path and this one consume the same `overlay_batch` and the same
/// `font_atlas`, so §9 renders one frame both ways and compares every channel.
/// Two renderers that agree on a picture are two renderers that agree on the
/// arithmetic, and that is a much stronger statement than either one looking
/// right.
///
/// Quads are assumed axis-aligned and are drawn in order. No clipping rectangle,
/// no rotation, no scaling: an overlay quad that needed any of those would not
/// be an overlay, it would be a sprite, and that is Module 9's editor.
///
/// @param stem_darken coverage adjustment applied before compositing, in the
///        sense of `apply_stem_darkening` below. Zero disables it.
void composite_overlay(framebuffer& target, const font_atlas& atlas,
                       const overlay_batch& batch,
                       overlay_blend blend = overlay_blend::linear,
                       float stem_darken = 0.0f);

/// Adjust a coverage value to compensate for correct compositing making text
/// thinner than its designer tuned it to look.
///
/// **The gamma-correct-text paradox, in one function.** Blending in linear light
/// is right, and it makes light-on-dark text visibly lighter-weight than the
/// same font rendered by software that blends in sRGB space — because the
/// outlines were hinted and the coverage tuned against that wrong arithmetic for
/// thirty years. Every serious text stack has a knob for this: FreeType calls it
/// stem darkening, Skia calls it gamma correction and contrast, and macOS's
/// historical "font smoothing" was a version of it.
///
/// Ours is the simplest form that is honest about what it is: a power curve on
/// coverage, `a' = a^(1 - k)`, which pushes partially-covered pixels up and
/// leaves 0 and 1 fixed. `k = 0` is off. §7.4 measures what `k = 0.2` does to
/// mean stem weight and explains why it is **not** the default: the correction
/// depends on which way round the contrast runs, and a single global constant
/// makes dark-on-light text worse by exactly as much as it makes light-on-dark
/// text better.
[[nodiscard]] float apply_stem_darkening(float coverage, float k);

} // namespace engine
