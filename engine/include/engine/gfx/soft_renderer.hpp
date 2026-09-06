// engine/include/engine/gfx/soft_renderer.hpp — the CPU render pipeline:
// scene objects in, pixels out.
//
// Modules 2 and 3 built this a stage at a time, and every stage of it lived in
// the demo's `main.cpp` — in an anonymous namespace, which is C++ for "nothing
// outside this file may ever refer to this". That was true and unremarkable
// while there was one caller. It stopped being either the moment a second
// program wanted the same picture: FOUR verification harnesses in a row
// (Lessons 4.6 through 4.9) had to TRANSCRIBE the scene by hand, and a
// transcription can drift from its original with nothing to notice.
//
// So this header is Lesson 5.1's central move: the pipeline becomes part of the
// engine's public API. Everything it exposes had to be designed on the way out,
// because a function with one caller in the same file has never been asked what
// its contract is.
//
//     collect_triangles()   objects -> transformed, clipped, shaded triangles
//     sort_back_to_front()  the painter's algorithm, alone and timeable
//     draw_triangles()      triangles -> pixels, through raster.hpp
//
// The GPU counterpart is <engine/gfx/gpu_scene.hpp>, and the two consume the
// same `scene_object` (Lesson 4.8's split view is what made that a requirement
// rather than a nicety).

#pragma once

#include <engine/gfx/depth_buffer.hpp>
#include <engine/gfx/framebuffer.hpp>
#include <engine/gfx/light.hpp>
#include <engine/gfx/projector.hpp>
#include <engine/gfx/raster.hpp>
#include <engine/gfx/scene.hpp>
#include <engine/math/mat3.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/vec3.hpp>
#include <engine/math/vec4.hpp>

#include <SDL3/SDL_stdinc.h>

#include <cstddef>
#include <span>
#include <vector>

namespace engine {

/// One triangle, already projected, ready to hand to the rasterizer.
///
/// The three vertices carry pixel coordinates AND device depth, which is all the
/// z-buffer needs. `sort_key` is the extra thing the painter's algorithm needs
/// and the z-buffer does not — keeping it in the struct rather than recomputing
/// it lets the two paths be compared on exactly the same geometry.
struct raster_triangle
{
    vertex v[3];

    /// Average VIEW-space z of the three corners: the painter's algorithm's
    /// entire idea, and its entire problem. View z is negative in front of the
    /// camera, so a MORE negative key is further away and sorts first.
    ///
    /// Note what this number is not: it is not "the depth of the triangle",
    /// because a triangle stretched in depth does not have one. §1.2.
    float sort_key = 0.0f;

    /// What the **wrong** facing test says about this triangle: the sign of
    /// `dot(face_normal, camera_forward)`, computed in VIEW space (Lesson 3.4 §3.4).
    ///
    /// Kept alongside the geometry purely so the two tests can be compared on
    /// identical triangles, every frame, and the disagreement counted. The right
    /// test needs no storage at all — it is the sign of an area the rasterizer
    /// already computes, which is most of the argument for using it.
    bool front_by_forward = false;

    /// The surface parameters this triangle should be shaded with — Lesson 3.8.
    ///
    /// **This field is a cheat, and the cheat is instructive.** Under per-pixel
    /// shading the material is read inside the fragment loop, so it has to be
    /// available per triangle; a GPU cannot do that, because a material is
    /// pipeline state and changing it means ending the draw call and starting
    /// another. Real renderers therefore **batch by material**, and a scene with
    /// three materials is three draws.
    ///
    /// A software rasterizer can just carry the pointer along, and this one does,
    /// because splitting the demo's single pass into per-object draws would break
    /// the painter's-algorithm comparison that has run since 3.1 (that one needs a
    /// sort ACROSS objects). It is named here rather than hidden: the moment this
    /// engine reaches Module 4, this field stops being possible. That is the same
    /// pressure 3.4 found with cull modes and 3.7 found with `specular`, arriving
    /// for the third time and from a new direction.
    microsurface surface{};
};

/// Per-face brightness, so adjacent faces of a solid can be told apart.
///
/// **This is a debug palette, not lighting.** There is no light in the scene, no
/// normal is consulted, and the number depends only on which triangle this
/// happens to be. Lesson 3.6 replaces it with a Lambert term computed from a real
/// normal and a real light direction — and keeps this behind `[G]`, because the two
/// pictures side by side are the fastest way to see what "the shading does not
/// change when the object turns" actually means. Eighth keep-the-wrong-thing
/// bargain in this engine, and the first where the wrong thing was ever the default.
///
/// The scaling happens in LINEAR LIGHT, decoding and re-encoding around it,
/// because "70% as bright" is a statement about light and multiplying a stored
/// sRGB value by 0.7 does not produce 70% of the light (Lesson 1.6).
[[nodiscard]] Uint32 face_shade(Uint32 base, std::size_t face);

// ---------------------------------------------------------------------------
// Lesson 3.6 — normals, and light that is actually light
// ---------------------------------------------------------------------------

// LESSON 3.8 SPLIT THE OLD `shade_mode` IN TWO, and that split is the lesson.
//
// 3.6 shipped one enum with `palette / flat / smooth`, and its own doc comment
// admitted the problem: "interpolating the COLOUR across a triangle and
// interpolating the NORMAL and shading each pixel are different things". They are
// different things because they are answers to two INDEPENDENT questions:
//
//   WHERE DOES THE NORMAL COME FROM?   a property of the MESH and how it was split
//   WHERE IS THE EQUATION EVALUATED?   a property of the PIPELINE
//
// One enum could not hold both, because the combinations are a grid rather than a
// list. Two enums can, and the grid has cells worth naming (see `is_degenerate`).

/// Where the surface normal comes from. [Q] cycles.
enum class normal_source
{
    /// One normal per TRIANGLE, from the cross product of its own two edges. Every
    /// point on the face gets the same normal, so the face is flat by construction
    /// whatever the evaluation does with it.
    face,

    /// One normal per VERTEX, as the mesh authored it. Shared vertices carry an
    /// averaged normal, which is what makes a tessellated surface read as curved —
    /// and is also why a mesh with SPLIT vertices (a cube's 24) stays faceted no
    /// matter which evaluation is chosen. Lesson 3.6 measured that: 0 pixels differ
    /// between the two sources on `cube.obj`.
    vertex
};

/// Where the shading equation is evaluated. [G] cycles.
enum class shade_eval
{
    /// Lesson 3.1's debug ramp: no light, no normal, indexed by triangle number.
    /// Not an evaluation point at all — kept in the same cycle because it is the
    /// "off" position, and because a fake next to three real ones is instructive.
    palette,

    /// ONCE PER TRIANGLE, at the centroid. All three corners get that one answer,
    /// so the fill interpolates between three equal values and the face comes out
    /// uniform.
    flat,

    /// ONCE PER VERTEX, and the fill interpolates the ANSWERS. This is **Gouraud
    /// shading** (1971) — and note that this engine has been doing it since 3.6
    /// without the name. 2.4 built the interpolation; 3.6 fed it lit colours; that
    /// combination is Gouraud, complete.
    gouraud,

    /// ONCE PER PIXEL, with the fill interpolating the NORMAL and the POSITION and
    /// evaluating the equation itself. This is **Phong shading** — the
    /// interpolation scheme, which is a different thing from the Phong reflection
    /// model of 3.7 by the same author. We pair Phong *shading* with the
    /// Blinn-Phong *reflection model*, which is the usual modern combination and
    /// sounds like a contradiction until the two axes are separated.
    per_pixel
};

[[nodiscard]] inline const char* name_of(normal_source n)
{
    switch (n)
    {
    case normal_source::face:   return "FACE   (cross product)";
    case normal_source::vertex: return "VERTEX (as authored)";
    }
    return "?";
}

[[nodiscard]] inline const char* name_of(shade_eval e)
{
    switch (e)
    {
    case shade_eval::palette:   return "debug palette (3.1)";
    case shade_eval::flat:      return "FLAT      per triangle";
    case shade_eval::gouraud:   return "GOURAUD   per vertex";
    case shade_eval::per_pixel: return "PER-PIXEL per fragment";
    }
    return "?";
}




/// What this frame's normals did, for the HUD.
struct normal_stats
{
    int shaded = 0;         ///< vertices given a Lambert term
    int fell_back = 0;      ///< …of which had no authored normal, so used the face's
    float max_tilt = 0.0f;  ///< worst angle, in degrees, between the correct normal
                            ///< transform and the naive one. Zero unless something
                            ///< in the scene is non-uniformly scaled.
};

/// The angle between two directions, in degrees. Used only for the HUD's honesty
/// check, so the acos guard matters more than the speed.
[[nodiscard]] inline float angle_between_deg(vec3 a, vec3 b)
{
    const vec3 ua = normalised_or(a, {});
    const vec3 ub = normalised_or(b, {});
    const float c = std::clamp(dot(ua, ub), -1.0f, 1.0f);
    return std::acos(c) * 180.0f / 3.14159265358979f;
}

// ---------------------------------------------------------------------------
// Lesson 3.4 — back-face culling
// ---------------------------------------------------------------------------

/// What the demo does about faces pointing away from the camera. [U] cycles.
///
/// Three of these are `engine::cull_mode` and one is not: `back_by_forward` culls
/// with the *wrong test* — the one almost everybody writes first — so it can be
/// watched failing rather than described. Seventh time this bargain has been made
/// in the engine, and the pattern is now the house style: keep the wrong thing,
/// behind a key.
enum class cull_choice
{
    none,              ///< draw everything (engine::cull_mode::none)
    back,              ///< the right test, the useful direction (cull_mode::back)
    front,             ///< the right test, inverted — see inside things (cull_mode::front)
    back_by_forward    ///< THE CLASSIC BUG: dot(normal, camera_forward). Demo-only.
};

[[nodiscard]] inline const char* name_of(cull_choice c)
{
    switch (c)
    {
    case cull_choice::none:            return "NONE (draw all)";
    case cull_choice::back:            return "BACK (correct)";
    case cull_choice::front:           return "FRONT (inverted)";
    case cull_choice::back_by_forward: return "BACK by dot(n,fwd)";
    }
    return "?";
}


/// The `engine::cull_mode` this choice asks the rasterizer for.
///
/// `back_by_forward` maps to `none`, because that mode does its (wrong) culling in
/// the demo, in view space, before the triangles ever reach the rasterizer. That is
/// exactly where the bug lives in the codebases that have it.
[[nodiscard]] inline cull_mode to_engine_cull(cull_choice c)
{
    switch (c)
    {
    case cull_choice::back:  return cull_mode::back;
    case cull_choice::front: return cull_mode::front;
    case cull_choice::none:
    case cull_choice::back_by_forward: break;
    }
    return cull_mode::none;
}

/// What culling did to this frame, for the HUD.
struct cull_stats
{
    int submitted = 0;   ///< triangles handed to the rasterizer
    int front = 0;       ///< …of which front-facing, by the SCREEN-SPACE test
    int drawn = 0;       ///< …and how many survived the current cull mode
    int disagree = 0;    ///< triangles the two facing tests classify differently
};

/// Reusable working storage for `collect_triangles`.
///
/// The per-vertex arrays used to be fixed 64-element stacks, which was fine while
/// the largest mesh was a twelve-vertex icosahedron and silently wrong the moment
/// Lesson 3.2's tessellated floor arrived with 289. Vectors remove the cap; owning
/// them **across frames** rather than declaring them inside the function is what
/// removes the per-frame allocation, since `clear()` keeps the capacity.
///
/// This is the smallest possible taste of Module 8's allocators: the fix for
/// allocation in a hot loop is almost never a faster allocator, it is not
/// allocating.
struct projection_scratch
{
    std::vector<vec3> view_pos;

    /// The per-vertex CLIP positions. This used to be `std::vector<screen_point>`
    /// — the vertices were carried straight through to pixels here. Lesson 3.3
    /// stops one step earlier, because the divide is the thing clipping has to
    /// happen before, and a vertex that has already been divided has thrown away
    /// the only information the clipper could have used.
    std::vector<vec4> clip_pos;

    /// Lesson 3.6. The per-vertex WORLD-space normals, and the colour each vertex
    /// ends up with once the light has been applied to it.
    ///
    /// Both are computed ONCE PER VERTEX and then read by every triangle that uses
    /// that vertex — which is the same argument indexed geometry made in Lesson 2.12
    /// and the reason per-vertex lighting is cheap. On the icosahedron it is 12
    /// shading calls instead of 60.
    ///
    /// Lighting in WORLD space, not view space, and that is a real choice. A light's
    /// direction is authored in world space, so shading there means the light does
    /// not have to be re-derived every time the camera moves. It also makes the HUD
    /// honest: orbit the camera and the shading must NOT change, because Lambert
    /// does not depend on where you are standing. (Specular does — Lesson 3.7.)
    std::vector<vec3> world_normal;
    std::vector<Uint32> vertex_colour;

    /// Lesson 3.7. The per-vertex WORLD positions.
    ///
    /// **Lambert never needed these, and specular does.** A directional light *is* a
    /// direction, and Lambert asks only for the angle between two directions — so
    /// through 3.6 this function could compose `view_from_world * world_from_model`
    /// once and send each vertex straight into view space, never forming a world
    /// position at all. A highlight has to know where the *eye* is relative to *this
    /// point*, which is a question about places rather than directions, so the world
    /// position now has to exist.
    ///
    /// It costs a second matrix multiply per vertex — model to world, then world to
    /// view, rather than one combined hop. That is the honest price of view
    /// dependence, and it is worth noticing that there is a price at all. (The usual
    /// dodge is to transform the *eye* into model space once per object and shade
    /// there, trading one matrix inverse per object against one multiply per vertex;
    /// Exercise 3.7.4 works it out.)
    std::vector<vec3> world_pos;
};

/// What the near plane did to this frame's geometry. Purely for the HUD.
///
/// Reported as properties of the GEOMETRY rather than of the mode, so the same
/// three numbers mean the same thing whichever setting [K] is on: `straddling` is
/// how many triangles genuinely cross the plane, whether or not the current mode
/// does anything sensible about them. That is what makes the modes comparable —
/// only `output` changes.
struct clip_stats
{
    int input = 0;        ///< triangles the scene offered
    int in_front = 0;     ///< wholly in front of the near plane — the common case
    int straddling = 0;   ///< crossing it: the ones that need cutting
    int behind = 0;       ///< wholly behind it: nothing to draw either way
    int output = 0;       ///< triangles actually handed to the rasterizer
};

/// Where the camera is, and what it is looking at.
///
/// Two values that have travelled together at every call site since Lesson 3.7,
/// because view-dependent shading needs both: the matrix to get into view space,
/// and the eye's WORLD position to point a highlight at. Keeping them apart meant
/// every caller had to remember to derive them from the same camera — and one
/// that did not would produce a picture with a highlight in the wrong place and
/// nothing else wrong, which is the worst kind of bug to find.
struct camera_view
{
    mat4 view_from_world;   ///< world -> view (Lesson 2.9's look_at)
    vec3 eye_world;         ///< the eye's position, in world space
};

/// The policy choices the CPU pipeline offers.
///
/// **EVERY DEFAULT HERE IS THE CORRECT ANSWER**, and that is the design rule, not
/// a coincidence. `trs_order::tsr` deforms; `normal_source::face` faceted;
/// `correct_normal_matrix = false` is Lesson 3.6's bug. All three are reachable,
/// because a failure you can summon on a keypress teaches more than a paragraph
/// describing it — but reaching one costs you a line of code that says its name
/// out loud. Default-constructing this struct cannot get you a wrong picture.
///
/// It also exists because of a number: `collect_triangles` took FIFTEEN
/// parameters at eight call sites, seven of which were these. Adding a knob meant
/// editing eight calls, so nobody would; gathering them means adding a field, and
/// the call sites that do not care never mention it.
/// State that always travels together should travel as one thing — the same
/// argument `fill_style` made in Lesson 3.2 and `projector` made in 3.3, arriving
/// a third time and settling the pattern.
struct render_options
{
    trs_order compose = trs_order::trs;                  ///< 2.8 — T*R*S
    cull_choice cull = cull_choice::none;                ///< 3.4
    normal_source normals = normal_source::vertex;       ///< 3.6
    shade_eval shading = shade_eval::gouraud;            ///< 3.6 / 3.8
    specular_model specular = specular_model::cook_torrance;   ///< 3.7, 6.4
    bool correct_normal_matrix = true;                   ///< 3.6 — inverse transpose
};

/// What `collect_triangles` measured on the way through. Purely instrumentation:
/// pass `nullptr` and the renderer does not fill it in.
struct collect_stats
{
    clip_stats clip;        ///< what the near plane did (3.3)
    normal_stats normals;   ///< what the normals did (3.6)

    /// Objects skipped because their `mesh_handle` did not resolve — Lesson 5.4.
    ///
    /// **A number that could not previously exist.** A dangling `std::span` is
    /// not detectable, so before handles there was no version of this field: an
    /// object whose geometry had been freed either drew garbage or crashed, and
    /// either way nothing counted it. Now the failure has a name, a count, and a
    /// place on the HUD, which is the difference between a bug and a symptom.
    int unresolved = 0;
};

/// Project every triangle of every object into screen space.
///
/// The vertex stage, whole: model matrix, world and view positions, per-vertex
/// lighting, projection to clip space, near-plane clipping, the perspective
/// divide, and the viewport map. What comes out is a flat list of
/// `raster_triangle` ready for `draw_triangles`.
///
/// One flat list ACROSS objects, deliberately. The painter's algorithm has to
/// sort across objects (sorting each object separately is wrong the moment two
/// of them overlap), and the z-buffer needs no sort at all; collecting once is
/// what makes the two strategies comparable on identical geometry.
///
/// `out` and `scratch` are the caller's, and are reused across frames on
/// purpose: `clear()` keeps the capacity, so a steady-state frame allocates
/// nothing. It is the smallest taste of Module 8's allocators — the fix for
/// allocation in a hot loop is almost never a faster allocator, it is not
/// allocating.
/// `meshes` resolves every object's `geometry` handle, and an object whose handle
/// does not resolve is SKIPPED and counted in `collect_stats::unresolved` — one
/// missing asset costs one object, never the frame (Lesson 5.4).
void collect_triangles(std::vector<raster_triangle>& out, projection_scratch& scratch,
                       std::span<const scene_object> objects, const mesh_pool& meshes,
                       const camera_view& camera, const projector& pr,
                       const lighting& lights, const render_options& opts,
                       collect_stats* stats = nullptr);

/// Draw a collected list of triangles.
///
/// One function, two algorithms, and the difference between them is two lines —
/// which is exactly the point worth taking away. The painter's algorithm needs a
/// sort of the whole scene, O(n log n) and growing, and it is still wrong; the
/// z-buffer needs no sort, no ordering, and no knowledge of the other triangles
/// at all, and it is right.
///
/// @param depth  the depth attachment, or nullptr for the painter's algorithm.
/// @param sorted true to sort back-to-front before drawing.
/// @param style  one style for the whole batch, which is how hardware works: you
///               bind a pipeline, draw everything that uses it, then bind
///               another. A scene with two materials is two batches, and sorting
///               draws by pipeline is a real optimisation in Module 6.
/// Sort the painter's algorithm's triangles, furthest first.
///
/// Extracted from `draw_triangles` in Lesson 3.10 so that the demo can run it
/// under `zone::sort` and `draw_triangles` can still do it for callers that do
/// not care. **One rule, one place, two callers** — the same discipline
/// `is_front_facing` established in 3.4: instrumentation may duplicate the
/// question, never the answer.
void sort_back_to_front(std::vector<raster_triangle>& tris);

void draw_triangles(framebuffer& fb, depth_buffer* depth,
                    std::vector<raster_triangle>& tris, bool sorted,
                    fill_style style, cull_stats* culled = nullptr,
                    quad_stats* quads = nullptr);

}   // namespace engine
