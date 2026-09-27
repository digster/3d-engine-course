// engine/src/gfx/overlay.cpp — quads in, pixels out, on the CPU.
//
// Lesson 6.18. Two halves: the batch, which both renderers consume, and the
// software compositor, which is `hello_cube`'s overlay and §9's instrument.

#include <engine/gfx/overlay.hpp>

#include <engine/core/log.hpp>
#include <engine/gfx/blend.hpp>
#include <engine/gfx/framebuffer.hpp>

#include <algorithm>
#include <cmath>

namespace engine {

const char* name_of(overlay_blend b)
{
    switch (b)
    {
    case overlay_blend::linear:  return "linear";
    case overlay_blend::encoded: return "encoded";
    }
    return "?";
}

// ---- The batch --------------------------------------------------------------

void overlay_batch::begin(int viewport_width, int viewport_height)
{
    // `clear()` and not `= {}`: the capacity is the whole point. A vector that
    // reached 4,000 vertices last frame reaches it again this frame without
    // touching the allocator, which is what makes a per-frame rebuild free.
    vertices_.clear();
    indices_.clear();
    scratch_.clear();
    overflowed_ = 0;
    viewport_w_ = viewport_width;
    viewport_h_ = viewport_height;
}

bool overlay_batch::push_quad(float x0, float y0, float x1, float y1,
                              float u0, float v0, float u1, float v1, Uint32 argb)
{
    if (quad_count() >= k_max_overlay_quads)
    {
        ++overflowed_;
        return false;
    }

    const auto base = static_cast<Uint16>(vertices_.size());

    overlay_vertex v{};
    v.set_colour(argb);

    // WINDING. Conventions §7: counter-clockwise is front-facing, and the
    // overlay pipeline does not disable culling — so the order here has to be
    // right, in a space where +y points DOWN. Top-left, bottom-left,
    // bottom-right, top-right traverses counter-clockwise on screen once y is
    // flipped into clip space, which is where the flip happens. Get it backwards
    // and the overlay is invisible on the GPU while remaining perfect on the
    // CPU, because the software compositor does not cull — a divergence between
    // the two renderers that §9's comparison is built to catch.
    v.x = x0; v.y = y0; v.u = u0; v.v = v0; vertices_.push_back(v);   // 0: top-left
    v.x = x0; v.y = y1; v.u = u0; v.v = v1; vertices_.push_back(v);   // 1: bottom-left
    v.x = x1; v.y = y1; v.u = u1; v.v = v1; vertices_.push_back(v);   // 2: bottom-right
    v.x = x1; v.y = y0; v.u = u1; v.v = v0; vertices_.push_back(v);   // 3: top-right

    const Uint16 idx[6] = {base, static_cast<Uint16>(base + 1u), static_cast<Uint16>(base + 2u),
                           base, static_cast<Uint16>(base + 2u), static_cast<Uint16>(base + 3u)};
    indices_.insert(indices_.end(), idx, idx + 6);
    return true;
}

void overlay_batch::rect(const font_atlas& atlas, float x0, float y0, float x1, float y1,
                         Uint32 argb)
{
    // A rectangle is a textured quad that samples the one texel where coverage
    // is 1. Same pipeline, same texture, same draw call as the text on top of it.
    (void)push_quad(x0, y0, x1, y1,
                    atlas.solid_u, atlas.solid_v, atlas.solid_u, atlas.solid_v, argb);
}

text_metrics overlay_batch::text(const font_atlas& atlas, std::string_view utf8, vec2 pen,
                                 Uint32 argb, const text_layout_options& options)
{
    scratch_.clear();
    const text_metrics m = layout_text(atlas, utf8, pen, options, scratch_);
    for (const glyph_quad& q : scratch_)
    {
        if (!push_quad(q.x0, q.y0, q.x1, q.y1, q.u0, q.v0, q.u1, q.v1, argb)) { break; }
    }
    return m;
}

text_metrics overlay_batch::text_top_left(const font_atlas& atlas, std::string_view utf8,
                                          vec2 top_left, Uint32 argb,
                                          const text_layout_options& options)
{
    // The ascent is the distance from the top of the line box down to the
    // baseline, so this is the whole of the conversion — and it is here, once,
    // rather than at forty call sites each remembering it.
    return text(atlas, utf8, vec2{top_left.x, top_left.y + atlas.ascent}, argb, options);
}

// ---- Stem darkening ---------------------------------------------------------

float apply_stem_darkening(float coverage, float k)
{
    if (k <= 0.0f) { return coverage; }
    // a^(1-k). At k = 0.2 a coverage of 0.5 becomes 0.5^0.8 = 0.574, so a
    // half-covered edge pixel gains 15% of coverage while 0 and 1 do not move —
    // which is exactly the shape wanted, because it is the PARTIAL pixels that
    // correct compositing thinned.
    return std::pow(std::clamp(coverage, 0.0f, 1.0f), 1.0f - k);
}

// ---- The software compositor ------------------------------------------------

void composite_overlay(framebuffer& target, const font_atlas& atlas,
                       const overlay_batch& batch, overlay_blend blend, float stem_darken)
{
    if (!atlas.valid() || batch.empty()) { return; }

    const std::span<const overlay_vertex> verts = batch.vertices();
    const int quads = batch.quad_count();

    const float atlas_w = static_cast<float>(atlas.width);
    const float atlas_h = static_cast<float>(atlas.height);

    for (int q = 0; q < quads; ++q)
    {
        // Vertex 0 is the top-left corner and vertex 2 the bottom-right, by the
        // winding `push_quad` fixed. Reading the quad back from its corners
        // rather than carrying a parallel array of rectangles means there is one
        // description of a quad in this file and the GPU reads the same one.
        const overlay_vertex& v0 = verts[static_cast<std::size_t>(q) * 4u + 0u];
        const overlay_vertex& v2 = verts[static_cast<std::size_t>(q) * 4u + 2u];

        const Uint32 src = pack_argb(v0.r, v0.g, v0.b, v0.a);
        const float alpha_scale = static_cast<float>(v0.a) * (1.0f / 255.0f);

        // Pixel bounds. The quad's edges are already on the pixel grid when the
        // layout snapped them, so this rounds nothing that was not already
        // integral — but a caller is free to place a panel at a fractional
        // position, and a half-covered edge pixel is not something a compositor
        // with no rasterizer can express. It takes the pixel whose CENTRE is
        // inside the quad, which is the same rule `raster.cpp` applies (3.2).
        const int px0 = static_cast<int>(std::floor(v0.x + 0.5f));
        const int py0 = static_cast<int>(std::floor(v0.y + 0.5f));
        const int px1 = static_cast<int>(std::floor(v2.x + 0.5f));
        const int py1 = static_cast<int>(std::floor(v2.y + 0.5f));
        if (px1 <= px0 || py1 <= py0) { continue; }

        // Texel step per pixel. One for a 1:1 glyph blit; for a solid rectangle
        // both uv corners are equal so the step is zero and every pixel reads
        // the same fully-covered texel.
        const float du = (v2.u - v0.u) * atlas_w / static_cast<float>(px1 - px0);
        const float dv = (v2.v - v0.v) * atlas_h / static_cast<float>(py1 - py0);
        const float u_start = v0.u * atlas_w;
        const float v_start = v0.v * atlas_h;

        const int cx0 = std::max(px0, 0);
        const int cy0 = std::max(py0, 0);
        const int cx1 = std::min(px1, target.width());
        const int cy1 = std::min(py1, target.height());

        for (int y = cy0; y < cy1; ++y)
        {
            // NEAREST, not bilinear, and this is a decision rather than a
            // shortcut. The blit is 1:1 — one texel per pixel, both on the
            // integer grid — so a bilinear tap at the texel centre returns
            // exactly the texel anyway, and a nearest fetch says so in the code
            // instead of relying on the arithmetic working out. It also makes
            // this loop and the GPU's sampler agree exactly, which §9 needs.
            const int ty = static_cast<int>(v_start + (static_cast<float>(y - py0) + 0.5f) * dv);
            Uint32* row = target.row(y);
            for (int x = cx0; x < cx1; ++x)
            {
                const int tx =
                    static_cast<int>(u_start + (static_cast<float>(x - px0) + 0.5f) * du);

                float coverage = static_cast<float>(atlas.texel(tx, ty)) * (1.0f / 255.0f);
                if (coverage <= 0.0f) { continue; }

                coverage = apply_stem_darkening(coverage, stem_darken);
                const float a = coverage * alpha_scale;

                // THE ONE LINE THE WHOLE LESSON IS ABOUT. `blend_over` decodes
                // both operands, lerps the LIGHT, and re-encodes;
                // `blend_over_encoded` lerps the stored codes. Both are one call
                // and they differ by 43% of the light at half coverage, which on
                // a glyph means stem weight.
                row[x] = (blend == overlay_blend::linear)
                           ? blend_over(row[x], src, a)
                           : blend_over_encoded(row[x], src, a);
            }
        }
    }

    if (batch.overflowed() > 0)
    {
        ENGINE_LOG_WARN(log_gfx, "overlay: %d quad(s) dropped, batch full at %d",
                        batch.overflowed(), k_max_overlay_quads);
    }
}

} // namespace engine
