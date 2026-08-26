// engine/src/gfx/debug_draw.cpp — implementation of the debug overlay.

#include <engine/gfx/debug_draw.hpp>

#include <engine/gfx/clip.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/raster.hpp>
#include <engine/math/mat2.hpp>   // engine::rotation — the 2-D arrowhead
#include <engine/math/vec2.hpp>

#include <algorithm>
#include <cmath>

namespace engine {

void line3(framebuffer& fb, vec3 a, vec3 b,
           Uint32 colour, const projector& pr)
{
    clip_vertex ca{to_clip(a, pr.proj), {}, colour};
    clip_vertex cb{to_clip(b, pr.proj), {}, colour};

    switch (pr.near)
    {
    case near_mode::clip:
        if (!clip_segment_near(ca, cb)) { return; }
        break;

    case near_mode::drop:
        if (near_distance(ca.position) < 0.0f
         || near_distance(cb.position) < 0.0f) { return; }
        break;

    case near_mode::none:
        break;
    }

    const screen_point pa = screen_from_clip(ca.position, pr.vp);
    const screen_point pb = screen_from_clip(cb.position, pr.vp);
    draw_line(fb, to_pixel(pa.xy.x), to_pixel(pa.xy.y),
                          to_pixel(pb.xy.x), to_pixel(pb.xy.y), colour);
}

void line3_world(framebuffer& fb, const mat4& view, const projector& pr,
                 vec3 a, vec3 b, Uint32 colour)
{
    line3(fb, xyz(view * point(a)),
          xyz(view * point(b)), colour, pr);
}

void draw_mesh(framebuffer& fb, const mesh& geometry,
               const mat4& m, const projector& pr, float point_w)
{
    // Transform each vertex ONCE, into view space, and keep the results. This is
    // the whole practical argument for indexed geometry: the icosahedron's twelve
    // vertices are shared by twenty triangles, so transforming per-vertex rather
    // than per-triangle-corner does 12 matrix multiplies instead of 60.
    vec3 p[64];
    const std::size_t vertex_count = std::min(geometry.vertices.size(), std::size(p));
    for (std::size_t i = 0; i < vertex_count; ++i)
    {
        // A vertex is a POSITION, so w = 1 and the model+view matrix's translation
        // is added in full. p[] comes out in VIEW space — the projection to the
        // screen happens later, inside line3, through `proj`.
        p[i] = xyz(m * to_vec4(geometry.vertices[i], point_w));
    }

    // Walk the index array three at a time; each triple is one triangle, and each
    // triangle draws its three edges.
    for (std::size_t t = 0; t + 2 < geometry.indices.size(); t += 3)
    {
        const std::size_t tri[3] = {geometry.indices[t], geometry.indices[t + 1],
                                    geometry.indices[t + 2]};
        for (int e = 0; e < 3; ++e)
        {
            const std::size_t ia = tri[e];
            const std::size_t ib = tri[(e + 1) % 3];
            if (ia >= vertex_count || ib >= vertex_count) { continue; }

            // Depth from view-space z (distance in front of the camera, negative
            // since the camera looks down -z). Nearer edges (z closer to 0) are
            // brighter. The window -13..-2 covers the scene's view-space z spread
            // across the dolly range; a cue, not a z-buffer (Lesson 3.1).
            const float mid_z = 0.5f * (p[ia].z + p[ib].z);
            const float t01 = std::clamp((mid_z + 13.0f) / 11.0f, 0.0f, 1.0f);
            const Uint8 v = static_cast<Uint8>(70.0f + 165.0f * t01);
            line3(fb, p[ia], p[ib], pack_argb(v, v, static_cast<Uint8>(v * 0.94f)), pr);
        }
    }
}

void draw_axes3(framebuffer& fb, const mat4& m, const projector& pr,
                float point_w, float dir_w)
{
    const Uint32 col[3] = {pack_argb(236, 92, 92),     // x
                           pack_argb(122, 196, 152),   // y
                           pack_argb(126, 162, 236)};  // z
    const vec3 basis[3] = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};

    // Where the object actually is. A position, so w = 1.
    const vec3 origin = xyz(m * to_vec4(vec3{}, point_w));

    for (int i = 0; i < 3; ++i)
    {
        // …and the arrows are DIRECTIONS, so w = 0. They must be rotated by the
        // matrix and not moved by it — we place them ourselves, at the object.
        const vec3 d = xyz(m * to_vec4(basis[i], dir_w));
        const vec3 tip = origin + d * 0.75f;
        const vec3 offset = origin;
        line3(fb, offset, tip, col[i], pr);

        // A small head, built in SCREEN space by rotating the projected arrow —
        // the same trick as 2.5's, now after the projection because that is where
        // the head has to be legible.
        //
        // This is the ONE place in the demo where dropping is still the right
        // answer, and it is worth being explicit about why rather than leaving it
        // looking like an oversight. The head is screen-space DECORATION: it is
        // constructed by rotating the projected shaft, so if either end of that
        // shaft is behind the eye the rotation has nothing meaningful to act on.
        // A decoration with no defined position is dropped; GEOMETRY is clipped.
        // Lesson 3.3 §4.4.
        const vec4 clip_a = to_clip(offset, pr.proj);
        const vec4 clip_b = to_clip(tip, pr.proj);
        if (near_distance(clip_a) < 0.0f || near_distance(clip_b) < 0.0f)
        {
            continue;
        }
        const screen_point a = screen_from_clip(clip_a, pr.vp);
        const screen_point b = screen_from_clip(clip_b, pr.vp);
        const vec2 back = normalised(a.xy - b.xy) * 5.0f;
        if (length_squared(back) > 0.0f)
        {
            const vec2 h1 = b.xy + rotation(0.5f) * back;
            const vec2 h2 = b.xy + rotation(-0.5f) * back;
            auto ix = [](float f) { return static_cast<int>(std::lround(f)); };
            draw_line(fb, ix(b.xy.x), ix(b.xy.y), ix(h1.x), ix(h1.y), col[i]);
            draw_line(fb, ix(b.xy.x), ix(b.xy.y), ix(h2.x), ix(h2.y), col[i]);
        }
    }
}

[[nodiscard]] depth_range show_depth(framebuffer& fb,
                                     const depth_buffer& db,
                                     const viewport& vp)
{
    const int x0 = std::max(0, static_cast<int>(vp.x));
    const int y0 = std::max(0, static_cast<int>(vp.y));
    const int x1 = std::min(fb.width(), static_cast<int>(vp.x + vp.w));
    const int y1 = std::min(fb.height(), static_cast<int>(vp.y + vp.h));

    depth_range range;
    for (int y = y0; y < y1; ++y)
    {
        for (int x = x0; x < x1; ++x)
        {
            const float d = db.depth_at(x, y);
            if (d >= depth_buffer::k_far) { continue; }   // never written
            range.lo = std::min(range.lo, d);
            range.hi = std::max(range.hi, d);
        }
    }

    const float span = range.hi - range.lo;
    const float inv = (span > 1e-9f) ? 1.0f / span : 0.0f;

    for (int y = y0; y < y1; ++y)
    {
        for (int x = x0; x < x1; ++x)
        {
            const float d = db.depth_at(x, y);
            if (d >= depth_buffer::k_far)
            {
                fb.put_pixel(x, y, pack_argb(16, 18, 26));   // untouched
                continue;
            }
            // NEAR is bright, far is dark — the opposite of the stored value, so
            // the picture reads the way a torch beam does rather than the way the
            // number does.
            const float t01 = 1.0f - std::clamp((d - range.lo) * inv, 0.0f, 1.0f);
            const Uint8 v = static_cast<Uint8>(30.0f + 215.0f * t01);
            fb.put_pixel(x, y, pack_argb(v, v, v));
        }
    }
    return range;
}

[[nodiscard]] int count_differences(const framebuffer& a,
                                    const framebuffer& b,
                                    const viewport& vp)
{
    const int x0 = std::max(0, static_cast<int>(vp.x));
    const int y0 = std::max(0, static_cast<int>(vp.y));
    const int x1 = std::min(a.width(), static_cast<int>(vp.x + vp.w));
    const int y1 = std::min(a.height(), static_cast<int>(vp.y + vp.h));

    int differ = 0;
    for (int y = y0; y < y1; ++y)
    {
        const Uint32* const ra = a.row(y);
        const Uint32* const rb = b.row(y);
        for (int x = x0; x < x1; ++x)
        {
            if (ra[x] != rb[x]) { ++differ; }
        }
    }
    return differ;
}

[[nodiscard]] int brightest_channel(const framebuffer& fb, const viewport& vp)
{
    const int x0 = std::max(0, static_cast<int>(vp.x));
    const int y0 = std::max(0, static_cast<int>(vp.y));
    const int x1 = std::min(fb.width(), static_cast<int>(vp.x + vp.w));
    const int y1 = std::min(fb.height(), static_cast<int>(vp.y + vp.h));

    int peak = 0;
    for (int y = y0; y < y1; ++y)
    {
        const Uint32* const row = fb.row(y);
        for (int x = x0; x < x1; ++x)
        {
            const Uint32 c = row[x];
            peak = std::max(peak, static_cast<int>(red_of(c)));
            peak = std::max(peak, static_cast<int>(green_of(c)));
            peak = std::max(peak, static_cast<int>(blue_of(c)));
        }
    }
    return peak;
}

}   // namespace engine
