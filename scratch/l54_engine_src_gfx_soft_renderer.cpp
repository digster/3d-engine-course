// engine/src/gfx/soft_renderer.cpp — implementation of the CPU render pipeline.

#include <engine/gfx/soft_renderer.hpp>

#include <engine/gfx/clip.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/texture.hpp>

#include <algorithm>
#include <cmath>

namespace engine {

[[nodiscard]] Uint32 face_shade(Uint32 base, std::size_t face)
{
    constexpr float k_steps[5] = {1.00f, 0.84f, 0.70f, 0.57f, 0.45f};
    const float k = k_steps[face % 5];
    const linear_rgb light = to_linear(base);
    return to_encoded({light.r * k, light.g * k, light.b * k});
}

void collect_triangles(std::vector<raster_triangle>& out, projection_scratch& scratch,
                       std::span<const scene_object> objects, const mesh_pool& meshes,
                       const camera_view& camera, const projector& pr,
                       const lighting& lights, const render_options& opts,
                       collect_stats* stats_out)
{
    // The policy, unpacked once. Aliases rather than `opts.` at ninety sites:
    // the body below is Modules 2 and 3, and it should read as it did.
    const trs_order order = opts.compose;
    const cull_choice cull = opts.cull;
    const normal_source nsrc = opts.normals;
    const shade_eval eval = opts.shading;
    const specular_model spec_model = opts.specular;
    const bool correct_normals = opts.correct_normal_matrix;
    const mat4& view_from_world = camera.view_from_world;
    const vec3 eye_world = camera.eye_world;

    out.clear();
    clip_stats stats;
    normal_stats nstats;

    int unresolved = 0;

    const int count = static_cast<int>(objects.size());
    for (int i = 0; i < count; ++i)
    {
        // ---- Lesson 5.4: resolve the handle, ONCE, at the top --------------
        //
        // `objects[i].geometry` is a `mesh_handle` — four bytes that mean nothing
        // without this pool. Turning it into geometry is the one operation that
        // can fail, and the shape of the failure is the whole point: a handle
        // whose slot has been freed or refilled returns `nullptr` HERE, at a
        // named line, instead of quietly reading whatever moved into the memory
        // a `std::span` used to point at.
        //
        // Resolved once per object rather than once per use, and that is not
        // merely tidier. Before this lesson the loop below said
        // `geometry.vertices[v]` INSIDE the per-vertex loop; a lookup
        // per vertex would have made handles cost something real. Resolve at the
        // boundary and use the view inside is the rule, and it is the same rule
        // every engine follows with every resource.
        const mesh_data* geometry_data = meshes.get(objects[i].geometry);
        if (geometry_data == nullptr)
        {
            // Skip the object and draw the rest of the scene. A missing asset
            // should cost you one object, never the frame — and the count goes
            // into the stats so "why is that gone?" has an answer other than
            // silence.
            ++unresolved;
            continue;
        }
        const mesh geometry = geometry_data->view();

        // Only the model matrix now. `view_from_model = view_from_world *
        // world_from_model` used to be composed right here, and Lesson 3.7 deleted
        // it: the shading needs each vertex's WORLD position, so the two hops have
        // to be taken separately and there is nothing left for the composed matrix
        // to do. The composition was a real optimisation; view-dependent shading is
        // what bought it out.
        const mat4 world_from_model = model_matrix(objects[i].xform, order);

        // ---- Lesson 3.6: the matrix that transforms NORMALS ----------------
        //
        // Not the model matrix. A normal is defined by being perpendicular to the
        // surface, and only the inverse transpose preserves that under a
        // non-uniform scale (mat4.hpp derives it). `[J]` switches to the naive
        // version so the failure can be watched rather than described — and note
        // that on the icosahedron, which is uniformly scaled, the two are
        // indistinguishable. That is the whole reason this bug survives in
        // codebases: two thirds of a typical scene looks perfect.
        const mat3 to_world_normal = correct_normals
            ? normal_matrix(world_from_model)
            : linear_of(world_from_model);
        const mat3 reference_normal = normal_matrix(world_from_model);

        // Transform each vertex ONCE. The icosahedron's twelve vertices are
        // shared by twenty triangles, so this is 12 matrix multiplies instead of
        // 60 — the practical argument for indexed geometry, from Lesson 2.12.
        const std::size_t vertex_count = geometry.vertices.size();
        std::vector<vec3>& view_pos = scratch.view_pos;
        std::vector<vec4>& clip_pos = scratch.clip_pos;
        std::vector<vec3>& world_normal = scratch.world_normal;
        std::vector<Uint32>& vertex_colour = scratch.vertex_colour;
        std::vector<vec3>& world_pos = scratch.world_pos;
        view_pos.clear();
        clip_pos.clear();
        world_normal.clear();
        vertex_colour.clear();
        world_pos.clear();
        view_pos.reserve(vertex_count);
        clip_pos.reserve(vertex_count);
        world_normal.reserve(vertex_count);
        vertex_colour.reserve(vertex_count);
        world_pos.reserve(vertex_count);

        // Shade at the vertices only when the evaluation point IS the vertex.
        // Under `flat` the answer is computed once per triangle below, and under
        // `per_pixel` the fill computes it — in which case the vertex colour must
        // stay the raw albedo, because that is what the fragment multiplies.
        const bool per_vertex_light = (eval == shade_eval::gouraud);

        for (std::size_t v = 0; v < vertex_count; ++v)
        {
            // Lesson 3.7 split this hop in two. It used to be one multiply by the
            // composed `view_from_model`; a highlight needs the world POSITION, so
            // the composition has to be taken apart. Same destination, one more
            // matrix multiply per vertex, and the extra multiply is what view
            // dependence costs.
            world_pos.push_back(xyz(world_from_model
                                    * engine::point(geometry.vertices[v])));
            view_pos.push_back(xyz(view_from_world * point(world_pos.back())));
            clip_pos.push_back(to_clip(view_pos.back(), pr.proj));

            // The authored normal, carried into world space. `normal_at` returns
            // the zero vector when the mesh has none, and zero survives the matrix
            // as zero — so the per-triangle loop can detect it and fall back to the
            // face normal without a second flag travelling alongside.
            const vec3 n_model = geometry.normal_at(v);
            world_normal.push_back(to_world_normal * n_model);

            if (per_vertex_light && nsrc == normal_source::vertex)
            {
                vertex_colour.push_back(
                    shade_encoded(objects[i].tint, world_normal.back(),
                                          eye_world - world_pos.back(), lights,
                                          objects[i].surface, spec_model));
            }
            else
            {
                vertex_colour.push_back(objects[i].tint);
            }

            // How far the naive transform would have tilted this normal. Measured
            // against the correct one every frame, whichever is in use, so the
            // number means the same thing in both modes — the same discipline
            // 3.3's clip_stats and 3.4's cull_stats follow.
            if (n_model != vec3{})
            {
                ++nstats.shaded;
                const float tilt = angle_between_deg(reference_normal * n_model,
                                                     linear_of(world_from_model) * n_model);
                nstats.max_tilt = std::max(nstats.max_tilt, tilt);
            }
        }

        // CLIP space -> pixels, for one already-clipped corner. Everything the
        // rasterizer needs, and nothing it does not: `engine::vertex` is a
        // screen-space type and this is the one conversion into it.
        const auto to_vertex = [&](const clip_vertex& cv) {
            const screen_point s = screen_from_clip(cv.position, pr.vp);
            // Lesson 3.8: the two varyings ride through unchanged. They are in
            // WORLD space and the divide does not touch them — only the position
            // is projected. A varying is data the fragment wants; the pipeline
            // carries it and does not interpret it.
            return vertex{to_pixel(s.xy.x), to_pixel(s.xy.y),
                                  s.depth, s.inv_w, cv.uv.x, cv.uv.y, cv.colour,
                                  cv.normal, cv.world};
        };

        const std::span<const std::uint16_t> idx = geometry.indices;
        for (std::size_t f = 0; f * 3 + 2 < idx.size(); ++f)
        {
            const std::size_t a = idx[f * 3 + 0];
            const std::size_t b = idx[f * 3 + 1];
            const std::size_t c = idx[f * 3 + 2];
            if (a >= vertex_count || b >= vertex_count || c >= vertex_count) { continue; }

            ++stats.input;

            // ---- Lesson 3.6: what colour is this triangle's light? ---------
            //
            // The FACE normal, from this triangle's own edges, in MODEL space —
            // and then through the same normal matrix as everything else, because
            // a face normal is a normal and obeys the same rule. Our winding is
            // counter-clockwise seen from outside (conventions §7), so
            // cross(b - a, c - a) points OUT, which is what a surface normal means.
            //
            // Computed for every triangle because both remaining shading modes can
            // need it: `flat` always, and `smooth` for any corner whose mesh gave
            // it no normal — the fallback that lets an unauthored mesh light
            // correctly instead of turning black.
            const std::span<const vec3> mv = geometry.vertices;
            const vec3 face_model = cross(mv[b] - mv[a], mv[c] - mv[a]);
            const vec3 face_world = to_world_normal * face_model;

            // ---- Lesson 3.8: the normal each corner will be shaded with ----
            //
            // ONE axis of the grid, resolved here. `face` gives all three corners
            // the triangle's own normal; `vertex` gives each its own, falling back
            // to the face normal for a mesh that authored none (3.6's rule, and
            // the fallback is counted so the HUD can say it happened).
            const auto normal_for = [&](std::size_t vi) -> vec3 {
                if (nsrc == normal_source::face) { return face_world; }
                if (world_normal[vi] != vec3{}) { return world_normal[vi]; }
                ++nstats.fell_back;
                return face_world;
            };
            const vec3 na = normal_for(a);
            const vec3 nb = normal_for(b);
            const vec3 nc = normal_for(c);

            Uint32 colour_a = 0;
            Uint32 colour_b = 0;
            Uint32 colour_c = 0;
            switch (eval)
            {
            case shade_eval::palette:
                // Lesson 3.1's ramp: no normal, no light, indexed by triangle
                // number. Kept so the comparison is one keypress away.
                colour_a = colour_b = colour_c = face_shade(objects[i].tint, f);
                break;

            case shade_eval::flat:
            {
                // ONE evaluation for the whole triangle, so all three corners get
                // the same colour and the fill interpolates between three equal
                // values — a flat face, at no extra cost.
                //
                // WHERE that one sample is taken has to be said out loud, because
                // both inputs vary across a face. The centroid, for both: it is the
                // only point that privileges no corner. With a face normal that is
                // the face normal; with vertex normals it is their average, which
                // is the normal the fill would have interpolated at the centre.
                const vec3 centroid =
                    (world_pos[a] + world_pos[b] + world_pos[c]) / 3.0f;
                const vec3 n_mid = (nsrc == normal_source::face)
                                         ? face_world
                                         : na + nb + nc;
                colour_a = colour_b = colour_c =
                    shade_encoded(objects[i].tint, n_mid, eye_world - centroid,
                                          lights, objects[i].surface, spec_model);
                break;
            }

            case shade_eval::gouraud:
            {
                // ONE evaluation per corner, and the fill interpolates the ANSWERS.
                // With vertex normals the per-vertex loop above already did the
                // work — `vertex_colour[vi]` is that cached answer, computed once
                // per vertex rather than once per corner-of-a-triangle, which is
                // the saving indexed geometry bought in 2.12.
                const auto pick = [&](std::size_t vi, vec3 n) -> Uint32 {
                    if (nsrc == normal_source::vertex
                        && world_normal[vi] != vec3{})
                    {
                        return vertex_colour[vi];
                    }
                    return shade_encoded(objects[i].tint, n,
                                                 eye_world - world_pos[vi], lights,
                                                 objects[i].surface, spec_model);
                };
                colour_a = pick(a, na);
                colour_b = pick(b, nb);
                colour_c = pick(c, nc);
                break;
            }

            case shade_eval::per_pixel:
                // NO evaluation here at all. The corner colour stays the raw
                // albedo, and the fill does the shading once per fragment from the
                // interpolated normal and position. This is the only branch that
                // sends the equation's INPUTS down the pipeline instead of its
                // output — which is the whole of Lesson 3.8 in one case label.
                colour_a = colour_b = colour_c = objects[i].tint;
                break;
            }

            // Lesson 3.8 adds the two varyings. They are attached HERE, before
            // clipping, so a triangle cut by the near plane gets a correctly
            // interpolated normal and position at its new corners — the clipper
            // lerps every field with the one crossing parameter (3.3 §3.4).
            const clip_vertex src[3] = {
                {clip_pos[a], geometry.uv_at(a), colour_a, na, world_pos[a]},
                {clip_pos[b], geometry.uv_at(b), colour_b, nb, world_pos[b]},
                {clip_pos[c], geometry.uv_at(c), colour_c, nc, world_pos[c]}};

            // How the triangle sits relative to the near plane — measured from the
            // geometry, not inferred from what the current mode does about it, so
            // the HUD's numbers mean the same thing in all three modes.
            int outside = 0;
            for (const clip_vertex& v : src)
            {
                if (near_distance(v.position) < 0.0f) { ++outside; }
            }
            if (outside == 0)      { ++stats.in_front; }
            else if (outside == 3) { ++stats.behind; }
            else                   { ++stats.straddling; }

            // The painter's sort key (Lesson 3.1) belongs to the SOURCE triangle,
            // so it is computed before clipping and shared by every piece the
            // clipper produces. Recomputing it per piece would let one half of a
            // wall sort in front of the other half — the pieces are the same
            // surface, and a sort must not be able to tell them apart.
            const float key = (view_pos[a].z + view_pos[b].z + view_pos[c].z) / 3.0f;

            // ---- The WRONG facing test (Lesson 3.4 §3.4) --------------------
            //
            // The face normal in VIEW space, from the cross product of two edges
            // (Lesson 1.7's right-hand rule, in 3-D). Our meshes are wound
            // counter-clockwise seen from outside, so for a triangle facing the
            // camera this points back toward the eye — which in view space, where
            // the camera sits at the origin looking down -z, means a POSITIVE z.
            //
            // The camera's forward axis is (0, 0, -1), so
            // `dot(normal, forward) = -normal.z`, and "facing me" comes out as
            // `normal.z > 0`. That is the test almost everybody writes first. It is
            // wrong under perspective, and §3.4 shows exactly where: it asks
            // whether the face points against the camera's AXIS, when the question
            // is whether it points against the RAY FROM THE EYE TO IT. Those differ
            // by more the further off-axis the triangle is.
            const vec3 face_normal =
                cross(view_pos[b] - view_pos[a], view_pos[c] - view_pos[a]);
            const bool front_by_forward = (face_normal.z > 0.0f);

            // ...and in this mode, act on it. Note WHERE this happens: in view
            // space, before the projection, which is precisely how the bug gets
            // into a codebase — it looks like a sensible early-out.
            if (cull == cull_choice::back_by_forward && !front_by_forward) { continue; }

            clip_vertex poly[k_clip_max_vertices];
            std::size_t n = 0;

            switch (pr.near)
            {
            case near_mode::clip:
                // One call, and the output is a polygon of 0, 3 or 4 vertices.
                n = clip_polygon_near(src, poly);
                break;

            case near_mode::drop:
                // Lesson 3.2's rule: one bad corner sinks the whole triangle.
                if (outside == 0) { poly[0] = src[0]; poly[1] = src[1]; poly[2] = src[2]; n = 3; }
                break;

            case near_mode::none:
                // Straight through to the divide, whatever `w` turns out to be.
                poly[0] = src[0]; poly[1] = src[1]; poly[2] = src[2]; n = 3;
                break;
            }

            // FAN the clipped polygon: (0, k-1, k) for k = 2 … n-1. Three vertices
            // give one triangle and four give two, which is where "a clipper
            // returns a variable number of triangles" stops being a design note and
            // becomes a loop. The fan is valid because Sutherland–Hodgman preserves
            // both convexity and winding — so every piece is wound the way the
            // original was, which Lesson 3.4's back-face test will depend on.
            for (std::size_t k = 2; k < n; ++k)
            {
                raster_triangle tri;
                tri.v[0] = to_vertex(poly[0]);
                tri.v[1] = to_vertex(poly[k - 1]);
                tri.v[2] = to_vertex(poly[k]);
                tri.sort_key = key;
                // The material travels with the geometry, because per-pixel
                // shading reads it inside the fill. See `raster_triangle::surface`
                // for why that is a cheat a GPU could not make.
                tri.surface = objects[i].surface;
                // Every piece of a clipped triangle lies in the SAME plane, so they
                // share one face normal and one answer from the wrong test.
                tri.front_by_forward = front_by_forward;
                out.push_back(tri);
                ++stats.output;
            }
        }
    }

    if (stats_out != nullptr) { *stats_out = {stats, nstats, unresolved}; }
}

void sort_back_to_front(std::vector<raster_triangle>& tris)
{
    // Furthest first. View-space z is NEGATIVE in front of the camera, so
    // "furthest" is "most negative" and plain ascending order is what we want.
    // Getting this backwards paints the scene inside out, which at least fails
    // loudly — unlike everything else about this algorithm.
    std::sort(tris.begin(), tris.end(),
              [](const raster_triangle& a, const raster_triangle& b)
              { return a.sort_key < b.sort_key; });
}

void draw_triangles(framebuffer& fb, depth_buffer* depth,
                    std::vector<raster_triangle>& tris, bool sorted,
                    fill_style style, cull_stats* culled,
                    quad_stats* quads)
{
    // The HUD's numbers, and a note on why they are gathered HERE rather than
    // returned by the rasterizer. `fill_triangle` culls internally — that is where
    // the hardware does it — so it could report back, but making every fill return
    // a bool would put a value at 100% of call sites that 99% of them ignore.
    // Counting in the caller costs one extra `edge_function` per triangle and keeps
    // the rule itself in ONE place: `is_front_facing`, which is what the rasterizer
    // calls too. Instrumentation duplicates the *question*, never the answer.
    if (culled != nullptr)
    {
        *culled = {};
        for (const raster_triangle& t : tris)
        {
            ++culled->submitted;
            const bool front = is_front_facing(t.v[0], t.v[1], t.v[2]);
            if (front) { ++culled->front; }
            if (front != t.front_by_forward) { ++culled->disagree; }

            const bool kept = (style.cull == cull_mode::none)
                           || (style.cull == cull_mode::back && front)
                           || (style.cull == cull_mode::front && !front);
            if (kept) { ++culled->drawn; }
        }
    }

    if (sorted) { sort_back_to_front(tris); }

    for (const raster_triangle& t : tris)
    {
        // Lesson 3.8: rebind the material per triangle when the fragment is the
        // one reading it. `style` is taken BY VALUE, so this is a local edit to a
        // local copy and the caller's pipeline object is untouched.
        //
        // Naming it again because it matters: a GPU cannot do this. Material
        // parameters live in a bound pipeline or a uniform buffer, and changing
        // them mid-draw means ending the draw. A renderer that shades per pixel
        // therefore sorts its geometry by material and issues one draw per batch,
        // and the reason this loop can be lazy is that it is not a GPU.
        style.surface = t.surface;
        // Lesson 4.1: `quads` accumulates across the whole draw, which is why
        // fill_triangle adds to it rather than assigning. A per-triangle lane
        // efficiency is not a number anybody wants.
        fill_triangle(fb, depth, t.v[0], t.v[1], t.v[2], style, quads);
    }
}

}   // namespace engine
