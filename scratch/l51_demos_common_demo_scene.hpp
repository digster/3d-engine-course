// demos/common/demo_scene.hpp — the scene the demos draw, and the content that
// makes it up.
//
// EVERYTHING IN THIS FILE IS DEMO CODE, and that is the point of the file
// existing. Lesson 5.1 split the tree into an engine library and the programs
// that use it, and the split forced a question about every single symbol: is
// this something an engine provides, or something a program decides? The
// pipeline that turns objects into pixels is the engine's. WHICH objects, at
// which sizes, spinning how fast, lit from which angle — that is a program's,
// and it lives here.
//
// It is a LIBRARY rather than a lump inside one `main.cpp` for a reason with a
// receipt: four verification harnesses in a row had to transcribe `build_scene`
// by hand, because it was trapped in an anonymous namespace. Anything a second
// program might legitimately want has to be linkable, and demo content is no
// exception.
//
// The rule that keeps this file honest: nothing here may be needed by a shipped
// game. The moment something is, it is not demo content — it is an engine
// feature that has not been named yet.

#pragma once

#include <engine/gfx/framebuffer.hpp>
#include <engine/gfx/mesh.hpp>
#include <engine/gfx/obj.hpp>
#include <engine/gfx/projector.hpp>
#include <engine/gfx/scene.hpp>
#include <engine/gfx/texture.hpp>
#include <engine/gfx/viewport.hpp>
#include <engine/math/mat3.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/transform.hpp>
#include <engine/math/vec2.hpp>
#include <engine/math/vec3.hpp>

#include <SDL3/SDL_stdinc.h>

#include <cmath>
#include <cstdint>
#include <vector>

namespace demo {

/// The software demos' framebuffer, in pixels.
///
/// 320x180 since Lesson 1.5, and 16:9 on purpose: the projection matrix bakes an
/// aspect ratio into x, so a framebuffer of a different shape would stretch the
/// picture. It lives here rather than in the engine because a resolution is a
/// PROGRAM's decision — the engine is handed a `framebuffer` and never asks how
/// big anybody wanted it.
inline constexpr int k_fb_width = 320;
inline constexpr int k_fb_height = 180;

// ===========================================================================
// Lesson 2.6 — mat3, mat4, and the first three-dimensional thing we have drawn
// ===========================================================================

/// Which rotation the cube view is showing. [Z] cycles.
enum class spin
{
    about_x,
    about_y,
    about_z,
    tumble_xy,   ///< Rx * Ry — y first
    tumble_yx    ///< Ry * Rx — x first
};

[[nodiscard]] inline const char* name_of(spin s)
{
    switch (s)
    {
    case spin::about_x:   return "rotation_x(t)";
    case spin::about_y:   return "rotation_y(t)";
    case spin::about_z:   return "rotation_z(t)";
    case spin::tumble_xy: return "Rx(t) * Ry(1.1)   [y first]";
    case spin::tumble_yx: return "Ry(1.1) * Rx(t)   [x first]";
    }
    return "?";
}

[[nodiscard]] inline spin next_spin(spin s)
{
    switch (s)
    {
    case spin::about_x:   return spin::about_y;
    case spin::about_y:   return spin::about_z;
    case spin::about_z:   return spin::tumble_xy;
    case spin::tumble_xy: return spin::tumble_yx;
    case spin::tumble_yx: return spin::about_x;
    }
    return spin::about_x;
}

[[nodiscard]] engine::mat3 build_spin(spin s, float t);

// The viewport: the 3-D scene is drawn into a 16:9 rectangle in the left part of
// the 320x180 framebuffer. NDC's [-1,1] x [-1,1] square maps onto it, and because
// the projection matrix bakes the 16:9 aspect into x, the rectangle itself must be
// 16:9 or the picture stretches.
//
// As of Lesson 2.11 this is an `engine::viewport` (mirroring SDL_GPUViewport)
// rather than three loose constants: top-left (6, 41.625), 172 x 96.75 px (16:9),
// depth [0, 1]. Its centre is (92, 90) and it produces exactly the same pixels the
// old constants did — this lesson named and homed the transform, it did not move a
// single vertex.
inline constexpr engine::viewport k_scene_viewport{6.0f, 41.625f, 172.0f, 96.75f, 0.0f, 1.0f};

// ...and, as of Lesson 3.2, a second one: the WHOLE framebuffer.
//
// A ground plane cannot be made to fit a sub-rectangle. Measured (scratch/fit_floor.cpp):
// across every floor extent worth having, the near edge projects to |x_ndc| > 1 —
// which is not a tuning failure but geometry. A surface you are standing on fills
// the bottom of your view, and that is what makes it a good subject for this
// lesson in the first place.
//
// So the floor scene draws through a viewport that is the entire 320x180 buffer.
// That costs nothing and needs no new projection matrix, because 320x180 IS 16:9 —
// the same aspect the projection already bakes in. It is also the first time this
// course changes the viewport at all, which is the point of Lesson 2.11 having
// made it a parameter rather than three constants.
inline constexpr engine::viewport k_full_viewport{0.0f, 0.0f,
                                           static_cast<float>(k_fb_width),
                                           static_cast<float>(k_fb_height), 0.0f, 1.0f};

/// The three objects in the world, and why each one is here.
///
/// The icosahedron is the milestone's hero — a real mesh, twelve vertices and twenty
/// faces, not an axis-aligned box, so a rotation reads richly instead of looking like
/// a square wobbling. The other two are the SAME eight-vertex cube at different
/// transforms, kept because their roles are still teaching something (Lesson 2.8):
///
///   icosahedron  UNIFORM scale, spinning.   T*R*S and T*S*R are bit-identical.
///   slab (cube)  non-uniform, spinning.     The order matters, visibly and always.
///   plinth(cube) non-uniform, still.        Identity rotation, so likewise identical.
///
/// Two of the three are controls. That is the point of [O]: get the composition
/// order wrong and two thirds of the scene still looks perfect, which is exactly how
/// such a bug ships.
inline constexpr int k_max_objects = 4;

// ---------------------------------------------------------------------------
// Lesson 3.1 — the scenes that break sorting, and the scene that does not
// ---------------------------------------------------------------------------

/// Which scene is loaded. [C] cycles.
///
/// The first is the Module 2 milestone, on which a back-to-front sort works
/// perfectly. The other three each break it in a different, irreparable way —
/// which is the argument for solving visibility per PIXEL instead.
enum class scene_kind
{
    solids,      ///< 2.12's icosahedron and cubes. Sorting works here.
    cycle,       ///< three planks: A over B over C over A. No order is correct.
    intersect,   ///< two quads passing through each other. Order changes mid-triangle.
    zfight,      ///< two near-coplanar quads. The z-buffer's own failure mode.
    floor,       ///< Lesson 3.2 — a checkered plane running to the horizon.
    model        ///< Lesson 3.5 — geometry read off a disk. [L] chooses which.
};

[[nodiscard]] inline const char* name_of(scene_kind k)
{
    switch (k)
    {
    case scene_kind::solids:    return "solids (sorting looks fine)";
    case scene_kind::cycle:     return "CYCLE  (A>B>C>A)";
    case scene_kind::intersect: return "INTERSECTING quads";
    case scene_kind::zfight:    return "near-coplanar (z-fight)";
    case scene_kind::floor:     return "FLOOR (checker to horizon)";
    case scene_kind::model:     return "MODEL (loaded from disk)";
    }
    return "?";
}

[[nodiscard]] inline scene_kind next_scene(scene_kind k)
{
    switch (k)
    {
    case scene_kind::solids:    return scene_kind::cycle;
    case scene_kind::cycle:     return scene_kind::intersect;
    case scene_kind::intersect: return scene_kind::zfight;
    case scene_kind::zfight:    return scene_kind::floor;
    case scene_kind::floor:     return scene_kind::model;
    case scene_kind::model:     return scene_kind::solids;
    }
    return scene_kind::solids;
}

/// A flat panel through `a` and `b`, `width` across, whose `a` end is pushed AWAY
/// from the camera by `tilt` and whose `b` end is pulled TOWARD it by the same
/// amount, and which extends `overhang` past both ends.
///
/// The construction is worth reading, because it is Lesson 2.5's claim used as a
/// tool rather than admired: **a rotation matrix is its three columns, and its
/// columns are where the basis vectors land.** We want the quad's own x axis to
/// run across the plank, its y axis along it, and its z axis to be the face
/// normal — so we build those three directions and hand them over as columns.
/// No angles are computed anywhere, and no `rotation_x/y/z` is composed.
///
/// The three axes are orthonormal by construction: `axis_x` is perpendicular to
/// the plank's horizontal run and has no z component, so it is perpendicular to
/// `axis_y` whatever the tilt; and `axis_z` is their cross product. Its
/// determinant is +1, verified — a reflection here would flip every triangle's
/// winding and quietly matter in Lesson 3.4.
///
/// The `overhang` lengthens the quad WITHOUT moving its plane, so the depths at
/// `a` and `b` are still exactly -tilt and +tilt. It exists so the planks
/// properly cross at the corners rather than merely touching: end-to-end panels
/// overlap in a sliver, and a sliver is not a demonstration.
[[nodiscard]] engine::transform make_plank(engine::vec2 a, engine::vec2 b,
                                           float width, float tilt, float overhang);

// ---------------------------------------------------------------------------
// Lesson 3.2 — a surface that recedes, and something painted on it
// ---------------------------------------------------------------------------

/// The ground plane's extent, in world units. Deliberately lopsided: it starts
/// just in front of the default camera and runs a long way away, so one quad
/// spans a **37:1 range of depth**. That ratio is the whole demo — affine
/// interpolation is exact when w is constant and worst when it is not.
inline constexpr float k_floor_x = 9.0f;      ///< half-width
inline constexpr float k_floor_near = 6.0f;   ///< +z edge, about a unit in front of the eye
inline constexpr float k_floor_far = -30.0f;  ///< -z edge, 37 units away

/// How many world units one checker cell covers. Fixed, so that changing the
/// TESSELLATION changes only the interpolation error and never the pattern —
/// which is what makes the two comparable.
inline constexpr float k_floor_cell = 3.0f;

/// Storage for the floor. A `mesh` is a pair of non-owning spans (Lesson 2.12),
/// so unlike every mesh so far — which view `inline constexpr` arrays with
/// program lifetime — this one needs somebody to own its arrays. That somebody is
/// the demo, and the awkwardness is the point: it is exactly the pressure that
/// produces Module 5's asset system.
struct floor_geometry
{
    std::vector<engine::vec3> vertices;
    std::vector<engine::vec2> uvs;
    std::vector<std::uint16_t> indices;
    int cells = 0;                       ///< quads per side; 0 = not built yet

    /// Designated initialisers, as of Lesson 3.5: `mesh` grew a fourth member and
    /// this floor carries no normals, so naming the three it does carry is both
    /// clearer and what stops `-Wmissing-field-initializers` complaining.
    ///
    /// This struct is what `engine::mesh_data` generalises — Lesson 3.2 said at the
    /// time that the awkwardness of owning geometry by hand was the pressure that
    /// would eventually produce a real type. It is kept as it is because the floor
    /// is *generated*, not loaded, and rewriting it to use `mesh_data` would gain
    /// nothing but churn in a demo that already works.
    [[nodiscard]] engine::mesh view() const
    {
        return engine::mesh{.vertices = vertices, .indices = indices, .uvs = uvs};
    }
};

/// Tessellate the ground plane into `cells` x `cells` quads.
///
/// The uvs are computed from the **world position**, not from the grid index, so
/// the checker pattern is bit-identical at every tessellation level. Only the
/// number of triangles the interpolation has to span changes — which is what lets
/// the demo answer "how much subdivision would affine interpolation need?" with a
/// number instead of a shrug.
void build_floor(floor_geometry& g, int cells);

// ---------------------------------------------------------------------------
// Lesson 3.5 — geometry from disk
// ---------------------------------------------------------------------------

/// Which model the [C] = model scene is showing. [L] cycles.
///
/// Four files and one mesh that never touched a disk. The last one is not padding:
/// `generated` is `make_torus()` in memory, and `torus` is that same mesh written
/// out by `save_obj` and read back. Rendering both and counting the pixels that
/// differ is how this lesson checks its own loader — the same trick Lessons 3.1
/// through 3.4 each used, applied now to an asset pipeline instead of an algorithm.
enum class model_choice
{
    torus,       ///< assets/torus.obj — 2,304 triangles, non-convex, a real workout
    cube,        ///< assets/cube.obj — twenty readable lines, and the index problem
    twisted,     ///< assets/twisted.obj — cube.obj with one face wound backwards
    quirks,      ///< assets/quirks.obj — every awkward-but-legal construct at once
    generated    ///< make_torus(), never written or read. The round trip's control.
};

[[nodiscard]] inline const char* name_of(model_choice c)
{
    switch (c)
    {
    case model_choice::torus:     return "torus.obj";
    case model_choice::cube:      return "cube.obj";
    case model_choice::twisted:   return "twisted.obj";
    case model_choice::quirks:    return "quirks.obj";
    case model_choice::generated: return "make_torus()";
    }
    return "?";
}

[[nodiscard]] inline const char* file_of(model_choice c)
{
    switch (c)
    {
    case model_choice::torus:     return "torus.obj";
    case model_choice::cube:      return "cube.obj";
    case model_choice::twisted:   return "twisted.obj";
    case model_choice::quirks:    return "quirks.obj";
    case model_choice::generated: break;
    }
    return nullptr;
}

[[nodiscard]] inline model_choice next_model(model_choice c)
{
    switch (c)
    {
    case model_choice::torus:     return model_choice::cube;
    case model_choice::cube:      return model_choice::twisted;
    case model_choice::twisted:   return model_choice::quirks;
    case model_choice::quirks:    return model_choice::generated;
    case model_choice::generated: return model_choice::torus;
    }
    return model_choice::torus;
}

/// How big to draw each model, so that all five fill the viewport comparably.
///
/// The torus spans 1.4 units, the cube 1, the pyramid 2 — a single scale would make
/// one of them a speck. In an engine this is the ASSET's business, not the
/// renderer's: a model is authored at its real size and placed by a transform.
/// Module 5's asset system computes and stores a bounding box for exactly this, and
/// Module 6's frustum culling needs the same number. Here it is a switch.
[[nodiscard]] inline float display_scale(model_choice c)
{
    switch (c)
    {
    case model_choice::torus:
    case model_choice::generated: return 1.5f;
    case model_choice::cube:      return 2.4f;
    case model_choice::twisted:   return 2.4f;
    case model_choice::quirks:    return 1.1f;
    }
    return 1.0f;
}

/// The loaded model, plus everything measured about it.
///
/// **`data` owns the arrays and `view()` borrows them**, which is the hazard this
/// type exists to make obvious. Reloading on [L] clears the vectors, so any `mesh`
/// taken before the reload now points at freed memory. The demo is safe because
/// `build_scene` runs after key handling and re-takes the view every frame — but
/// "safe because of the order two things happen in" is exactly the kind of safety
/// Module 5's handles replace with something a compiler can check.
struct model_state
{
    model_choice choice = model_choice::torus;
    engine::mesh_data data;        ///< whatever [L] currently names
    engine::obj_report load;       ///< what the file contained, and what we built
    engine::mesh_report check;     ///< …and whether it is safe to draw
    double load_ms = 0.0;          ///< wall-clock cost of the last load

    /// Whether `flip_uv_v` was applied on the way in — Lesson 3.9. Recorded rather
    /// than inferred, because "are these uvs in OBJ space or texture space?" is
    /// exactly the question a mesh cannot answer by looking at its own numbers, and
    /// every convention bug in a pipeline is somebody assuming the answer.
    bool uv_flipped = true;

    /// `make_torus()`, built once. The control for the round-trip comparison, and
    /// the source `assets/torus.obj` was written from.
    engine::mesh_data generated;
};

/// Load (or regenerate) the current model and measure it.
///
/// Both halves matter. The loader answers "what does the file say"; `validate()`
/// answers "is that safe to draw", and until this lesson nothing in the engine could
/// ask the second question at all. Its answer feeds `scene_object::closed`, so the
/// demo's back-face culling is now gated on a MEASUREMENT rather than on a promise —
/// which is the debt Lesson 3.4 §3.6 left, paid.
///
/// **Lesson 3.9 adds the uv flip**, and adds it here rather than inside the parser:
/// OBJ writes `v` from the bottom up and a texture is addressed from the top down
/// (`mesh.hpp`'s `flip_uv_v` quotes SDL_gpu.h on the point), so somebody has to
/// reconcile them and the import step is the somebody. `apply_uv_flip` is a
/// parameter rather than a constant so the disagreement can be switched back on
/// with [2] and looked at — it is not subtle when you can see it, and it is nearly
/// invisible when you cannot.
void load_model(model_state& m, model_choice c, bool apply_uv_flip);

/// Rebuild the scene for the current time, rotation mode and scene kind.
/// Returns how many objects were written.
///
/// Rebuilt from scratch every frame rather than accumulated into, deliberately.
/// Repeatedly multiplying a rotation by a small delta drifts — the matrix stops
/// being a rotation, and the object slowly shears. Deriving the whole transform
/// from one authoritative `t` cannot drift, and it is the pattern the engine keeps
/// (Module 5's transform component stores the *inputs*, never a running matrix).
int build_scene(engine::scene_object (&out)[k_max_objects], scene_kind kind, spin mode, float t,
                const floor_geometry& floor, const model_state& model, float shininess);

/// Draw the world through the camera: a ground grid on y = 0 and a marked origin.
///
/// This exists so that "world space" is a place you can see rather than a claim,
/// and as of Lesson 2.9 it is drawn THROUGH the view matrix — so the floor tilts
/// and turns as the camera orbits. As of Lesson 2.10 it goes on through `proj`, so
/// the far edge of the floor is smaller than the near edge: the floor's parallel
/// rails now visibly converge, which is perspective made obvious.
void draw_world(engine::framebuffer& fb, const engine::mat4& view, const engine::projector& pr);

// ---------------------------------------------------------------------------
// Lesson 3.9 — where a fragment's colour comes from
// ---------------------------------------------------------------------------

/// Which image the surface reads its albedo from. [M] cycles.
///
/// The first entry is not an image at all, and keeping it first is the argument:
/// a procedural rule and a texture lookup answer the same question, and putting
/// them one keypress apart is the cheapest way to see what the array buys (art you
/// could not have written a formula for) and what it costs (a finite grid, which
/// runs out under magnification and aliases under minification).
enum class albedo_source
{
    /// `shading::uv_checker` — Lesson 3.2's formula. No memory, no sampler, and
    /// **exact at every magnification**, because there is no grid to run out of.
    rule,

    /// A 64x64 checkerboard of 8x8 squares: the same pattern as `rule`, now as
    /// texels. Chosen so the two can be compared directly — every visible
    /// difference between them is the sampler's doing and nothing else's.
    checker,

    /// The orientation chart. Four coloured quadrants and a white top-left mark,
    /// so "which way is v" is a thing you read rather than reason about.
    uv_grid,

    /// A 64x64 checkerboard of 32x32 squares — two texels per square. Deliberately
    /// finer than the screen can resolve at any distance, so the floor running to
    /// the horizon sparkles. That sparkle is §7's subject and mipmaps are Module 6.
    fine
};

[[nodiscard]] inline const char* name_of(albedo_source a)
{
    switch (a)
    {
    case albedo_source::rule:    return "RULE (procedural, 3.2)";
    case albedo_source::checker: return "checker 64px/8 cells";
    case albedo_source::uv_grid: return "uv grid (orientation)";
    case albedo_source::fine:    return "fine 64px/32 cells";
    }
    return "?";
}

[[nodiscard]] inline albedo_source next_albedo(albedo_source a)
{
    switch (a)
    {
    case albedo_source::rule:    return albedo_source::checker;
    case albedo_source::checker: return albedo_source::uv_grid;
    case albedo_source::uv_grid: return albedo_source::fine;
    case albedo_source::fine:    return albedo_source::rule;
    }
    return albedo_source::rule;
}




/// The three images, built once at startup.
///
/// Owned by the demo, like `floor_geometry` and for the same reason and with the
/// same complaint: there is still nothing in this engine whose job is to own
/// assets. Two lessons running have now noted it. Module 5's asset system is the
/// answer, and the pressure is being allowed to accumulate rather than relieved
/// early with a guess.
struct texture_set
{
    engine::texture checker;
    engine::texture uv_grid;
    engine::texture fine;

    void build()
    {
        // 64x64: big enough that magnification is visible on a 320x180 framebuffer
        // and small enough that a whole image fits comfortably in L1, which is what
        // keeps the per-pixel fetch honest rather than a cache-miss benchmark.
        checker = engine::make_checker(64, 8, 0xFFE8E2D6u, 0xFF3A4058u);
        uv_grid = engine::make_uv_grid(64);
        fine = engine::make_checker(64, 32, 0xFFE8E2D6u, 0xFF3A4058u);
    }

    /// The image for a choice, or `nullptr` for `rule` — which is not an error, it
    /// is the mode that has no image by definition.
    [[nodiscard]] const engine::texture* pick(albedo_source a) const
    {
        switch (a)
        {
        case albedo_source::rule:    return nullptr;
        case albedo_source::checker: return &checker;
        case albedo_source::uv_grid: return &uv_grid;
        case albedo_source::fine:    return &fine;
        }
        return nullptr;
    }
};

// ---------------------------------------------------------------------------
// Lesson 2.10 — the projection matrices the scene is drawn with
// ---------------------------------------------------------------------------

// The scene's field of view and clip planes. The near plane is small (0.3) so the
// orbiting/dollying camera never pushes a vertex behind it — verified: the closest
// the scene's geometry comes to the eye across the whole dolly range is ~1.7 units.
inline constexpr float k_scene_fovy = 55.0f * 3.14159265358979f / 180.0f;
inline constexpr float k_scene_aspect = 16.0f / 9.0f;   // matches the viewport rectangle
inline constexpr float k_scene_near = 0.3f;
inline constexpr float k_scene_far = 100.0f;

/// An ORTHOGRAPHIC projection matrix, for the [P] comparison — this is what the
/// demo used before Lesson 2.10, expressed as a matrix so it drops into the same
/// pipeline. It keeps `w = 1` (so the perspective divide is a harmless divide by
/// one) and maps a fixed view-space box to NDC, so distance changes nothing.
///
/// The half-extents are chosen so the on-screen scale matches the perspective
/// projection at the camera's default distance — that way pressing [P] swaps how
/// depth is handled without jumping the overall size, and the difference you see is
/// purely "near grows / far shrinks" versus "everything the same size".
///
/// A general `orthographic()` belongs in the engine eventually (2-D overlays,
/// shadow maps); it is kept local here because nothing but this comparison needs it
/// yet, and Lesson 2.11 is where the viewport/ortho machinery gets its real home.
[[nodiscard]] engine::mat4 demo_orthographic();

// ---------------------------------------------------------------------------
// Lesson 2.9 — the camera, and the view matrix
// ---------------------------------------------------------------------------

/// The demo camera orbits a fixed target on a sphere: azimuth around, elevation
/// up, radius out. This is not part of the engine — it is the demo's way of moving
/// an `eye` around so `look_at` has something to chew on. A real camera (Module 5)
/// stores a `transform`; this stores the three orbit angles because they are what
/// two arrow keys map onto cleanly.
struct orbit_camera
{
    engine::vec3 target{0.0f, 0.6f, 0.0f};
    float radius = 7.0f;
    float azimuth = 0.0f;     ///< radians; 0 looks down -z at the target
    float elevation = 0.35f;  ///< radians above the ground plane

    /// Where the eye sits, from the orbit angles. Standard spherical placement:
    /// azimuth sweeps around y, elevation lifts toward +y.
    [[nodiscard]] engine::vec3 eye() const
    {
        return target + engine::vec3{radius * std::cos(elevation) * std::sin(azimuth),
                                     radius * std::sin(elevation),
                                     radius * std::cos(elevation) * std::cos(azimuth)};
    }

    /// The view matrix for this camera. up_hint is world up; elevation is clamped
    /// (below) so it never becomes parallel to the look direction, which would make
    /// the `right = cross(up, backward)` degenerate.
    [[nodiscard]] engine::mat4 view() const
    {
        return engine::look_at(eye(), target, {0.0f, 1.0f, 0.0f});
    }
};

// ---------------------------------------------------------------------------
// Lesson 5.1 — the characterization shot
// ---------------------------------------------------------------------------

/// FNV-1a, 32-bit. A hash for identifying a frame, not for defending one.
[[nodiscard]] Uint32 fnv1a(const void* data, std::size_t bytes);

/// Render a fixed set of frames from fixed inputs and write them to one PPM.
///
/// **This was written BEFORE Lesson 5.1 moved a single file, and it is the reason
/// the move could be trusted.** A refactor's entire claim is that nothing
/// observable changed; a claim with no way to fail is not a claim. Seven scenes,
/// one deterministic frame each, driven through every stage that was about to
/// cross the boundary — `build_scene` -> `collect_triangles` -> cull/sort ->
/// `draw_triangles`, with `draw_world`'s overlay on top. Everything that could
/// vary is pinned to a constant inside, so the only thing left that can change
/// the output is the code.
///
/// It lives HERE, in the shared demo library, and that is the lesson eating its
/// own cooking: the test that made the refactor safe was the first thing the
/// refactor made shareable. `sandbox --shot` calls it, and so does
/// `scratch/verify_50.cpp` — which is how a harness can now check the demo's
/// picture without transcribing the demo.
///
/// @return 0 on success; 1 if the file could not be written.
int write_reference_shot(const char* path);

}   // namespace demo
