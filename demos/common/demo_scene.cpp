// demos/common/demo_scene.cpp — building the demo scene.

#include "demo_scene.hpp"

#include <engine/gfx/colour.hpp>
#include <engine/gfx/debug_draw.hpp>
#include <engine/gfx/framebuffer.hpp>
#include <engine/gfx/depth_buffer.hpp>
#include <engine/gfx/raster.hpp>
#include <engine/gfx/soft_renderer.hpp>

#include <SDL3/SDL.h>

#include <string>
#include <utility>

namespace demo {

[[nodiscard]] engine::mat3 build_spin(spin s, float t)
{
    // The two tumble modes share both ingredients and differ ONLY in which
    // rotation is applied first — the 3-D restatement of Lesson 2.5 §3.6.
    switch (s)
    {
    case spin::about_x:   return engine::rotation_x(t);
    case spin::about_y:   return engine::rotation_y(t);
    case spin::about_z:   return engine::rotation_z(t);
    case spin::tumble_xy: return engine::rotation_x(t) * engine::rotation_y(1.1f);
    case spin::tumble_yx: return engine::rotation_y(1.1f) * engine::rotation_x(t);
    }
    return engine::mat3::identity();
}

[[nodiscard]] engine::transform make_plank(engine::vec2 a, engine::vec2 b,
                                           float width, float tilt, float overhang)
{
    const engine::vec3 end_a{a.x, a.y, -tilt};
    const engine::vec3 end_b{b.x, b.y, +tilt};

    const engine::vec3 along = end_b - end_a;

    const engine::vec3 axis_y = engine::normalised(along);               // model +y
    const engine::vec3 axis_x = engine::normalised(                      // model +x
        engine::vec3{-(b.y - a.y), b.x - a.x, 0.0f});
    const engine::vec3 axis_z = engine::cross(axis_x, axis_y);           // model +z

    engine::transform t;
    t.rotation = engine::mat3{axis_x, axis_y, axis_z};
    t.scale    = {width, engine::length(along) + 2.0f * overhang, 1.0f};
    t.position = (end_a + end_b) * 0.5f;
    return t;
}

void scene_assets::build()
{
    // Three copies, once, at startup: 8 + 4 + 12 positions and 36 + 6 + 60
    // indices, which is 636 bytes of copying in exchange for never again having
    // to know which meshes are safe to borrow. `to_mesh_data` is the bridge from
    // Lesson 2.12's `inline constexpr` arrays to storage the store controls.
    //
    // NAMED, as of Lesson 5.5, and the names are not decoration. They make the
    // built-ins findable (`store.find_mesh("cube")`), replaceable, unloadable and
    // countable by exactly the same machinery as `torus.obj` — which is the test
    // of whether generated content is really a first-class asset or a special
    // case wearing the same type.
    cube = store.insert_mesh("cube", engine::to_mesh_data(engine::cube_mesh()));
    quad = store.insert_mesh("quad", engine::to_mesh_data(engine::quad_mesh()));
    icosahedron = store.insert_mesh("icosahedron",
                                    engine::to_mesh_data(engine::icosahedron_mesh()));
}

void build_floor(scene_assets& assets, floor_geometry& g, int cells)
{
    if (g.cells == cells && g.geometry) { return; }   // rebuilt only on [T]
    g.cells = cells;

    engine::mesh_data floor;

    const int n = cells + 1;   // vertices per side
    for (int j = 0; j < n; ++j)
    {
        const float tz = static_cast<float>(j) / static_cast<float>(cells);
        const float z = k_floor_near + tz * (k_floor_far - k_floor_near);
        for (int i = 0; i < n; ++i)
        {
            const float tx = static_cast<float>(i) / static_cast<float>(cells);
            const float x = -k_floor_x + tx * (2.0f * k_floor_x);
            floor.vertices.push_back({x, 0.0f, z});
            floor.uvs.push_back({x / k_floor_cell, z / k_floor_cell});
        }
    }

    // Two triangles per quad, both wound counter-clockwise seen from ABOVE
    // (+y), which is the side anybody stands on.
    for (int j = 0; j < cells; ++j)
    {
        for (int i = 0; i < cells; ++i)
        {
            const auto v = [&](int ii, int jj) {
                return static_cast<std::uint16_t>(jj * n + ii);
            };
            floor.indices.push_back(v(i, j));     floor.indices.push_back(v(i, j + 1));
            floor.indices.push_back(v(i + 1, j + 1));
            floor.indices.push_back(v(i, j));     floor.indices.push_back(v(i + 1, j + 1));
            floor.indices.push_back(v(i + 1, j));
        }
    }

    // ONE call, as of Lesson 5.5. `insert_mesh` replaces a name it already holds:
    // it unloads the previous floor — cascading to anything derived from it — and
    // stores the new one. The pool reuses the slot, so the new handle differs from
    // the old one only in its generation. Watch that in a debugger once and the
    // design stops being abstract: same index, next generation, and the previous
    // handle is now a number the store refuses.
    g.geometry = assets.store.insert_mesh("floor", std::move(floor));
}

/// Print what the store currently holds.
///
/// **It goes to the log rather than to the HUD, and that is a layout fact rather
/// than a design one** — the software demo's HUD is full, and a number squeezed
/// into a row that another lesson already owns is a number nobody reads. Printed
/// on every acquire and every unload, which is exactly when it changes, so the
/// terminal beside the window is a running account of what is resident.
void log_asset_counters(const scene_assets& assets)
{
    const engine::asset_counters& c = assets.store.counters();
    SDL_Log("  assets: %d live | %d file(s) read, %d cache hit(s), %d generated, "
            "%d derived, %d unloaded, %d failed | %.1f KB read",
            assets.store.live_count(), c.files_read, c.cache_hits, c.inserted,
            c.derived, c.unloaded, c.loads_failed,
            static_cast<double>(c.bytes_read) / 1024.0);
}

void load_model(scene_assets& assets, model_state& m, model_choice c, bool apply_uv_flip)
{
    m.choice = c;
    m.uv_flipped = apply_uv_flip;
    m.unloaded = false;

    const Uint64 t0 = SDL_GetTicksNS();

    if (c == model_choice::generated)
    {
        // NOT A COPY ANY MORE, and the deleted copy is worth a sentence. This
        // branch used to duplicate four vectors on every keypress, because the
        // demo had two owners of the same geometry and no way to say "the same
        // one". Now `generated` is an asset in the store like any other and this
        // is an assignment of one integer: two handles naming one mesh, which is
        // exactly what a handle is for.
        m.geometry = m.generated;
        m.cached = true;
        m.load = {};
        m.load.status = engine::obj_status::ok;
        if (const engine::mesh_data* g = assets.store.mesh_at(m.geometry))
        {
            m.load.vertices = static_cast<int>(g->vertices.size());
            m.load.triangles = static_cast<int>(g->triangle_count());
        }
    }
    else
    {
        // ONE CALL, and everything that used to be here went with it: the path
        // assembly, the `std::string` that owned it, the parse, and the uv flip.
        //
        // The flip goes in as an IMPORT SETTING rather than being applied
        // afterwards, which is the change with a consequence: the same file
        // imported both ways is two different meshes, so the store keys on the
        // settings as well as the name and toggling [2] loads a SECOND asset
        // instead of quietly rewriting the first. That is what every real
        // pipeline does, and it is the reason `mesh_import` is a struct rather
        // than a bool parameter — the day it grows a second field, nothing here
        // changes.
        const engine::mesh_import settings{.flip_uv_v = apply_uv_flip};
        const engine::mesh_load got = assets.store.load_mesh(file_of(c), settings);
        m.geometry = got.handle;
        m.load = got.report;
        m.cached = got.cached;
    }

    const Uint64 t1 = SDL_GetTicksNS();
    m.load_ms = static_cast<double>(t1 - t0) / 1.0e6;

    // Validate whatever we got, INCLUDING a failed load — on failure the handle is
    // null, `mesh_at` returns nullptr, and an empty mesh reports zeroes rather
    // than crashing the report.
    //
    // Run on a CACHE HIT too, deliberately, even though the answer cannot have
    // changed. Validation is a pure function of the mesh and it is the demo's
    // HUD data, not the store's: an asset system that cached the *validator's*
    // opinion would have to decide when that opinion expires, and this demo has
    // no need to buy that problem. Module 8's cooked assets do, and that is where
    // it belongs.
    const engine::mesh_data* loaded = assets.store.mesh_at(m.geometry);
    m.check = loaded != nullptr ? engine::validate(loaded->view()) : engine::mesh_report{};

    if (m.load.ok() && loaded != nullptr)
    {
        SDL_Log("%s %-24s %5d verts (%+d split), %5d tris, euler %+d, "
                "volume %+.4f, %s%s  [%.2f ms]",
                m.cached ? "Cached" : "Loaded",
                name_of(c), m.load.vertices, m.load.split_vertices, m.load.triangles,
                m.check.euler, static_cast<double>(m.check.signed_volume),
                m.check.closed() ? "closed" : "OPEN",
                m.check.consistently_wound() ? "" : ", WINDING INCONSISTENT",
                m.load_ms);
    }
    else
    {
        // The store has already logged WHY, at the point that knew (Lesson 5.3).
        // What the demo adds is the CONSEQUENCE, which is the only thing it knows.
        SDL_Log("%s will not draw: %s (line %d)", name_of(c),
                engine::name_of(m.load.status), m.load.line);
    }
    log_asset_counters(assets);
}

int unload_model(scene_assets& assets, model_state& m)
{
    // The generated torus is not unloadable from here, and the refusal is a real
    // rule rather than a special case: it is the CONTROL for the round-trip
    // comparison, `torus.obj` was written from it, and a demo that could delete
    // its own reference would be a demo whose measurement means nothing. Naming
    // an asset is also how you protect it.
    if (m.choice == model_choice::generated)
    {
        SDL_Log("[Bksp] make_torus() is the round-trip control and stays resident");
        return 0;
    }

    const int went = assets.store.unload_mesh(m.geometry);
    if (went == 0)
    {
        SDL_Log("[Bksp] nothing to unload — %s is already gone", name_of(m.choice));
        return 0;
    }

    m.unloaded = true;
    // The handle is DELIBERATELY LEFT IN PLACE rather than nulled, and this is the
    // whole demonstration. `build_scene` keeps writing it into the scene every
    // frame, `collect_triangles` keeps failing to resolve it, and the object
    // disappears with a number next to it instead of a crash. Nulling it here
    // would tidy away the exact thing worth looking at.
    SDL_Log("[Bksp] unloaded %s: %d asset(s) released, %d still resident. "
            "The scene still holds handle %u:%u — watch `unresolved` on the HUD.",
            name_of(m.choice), went, assets.store.live_count(),
            m.geometry.index(), m.geometry.generation());
    log_asset_counters(assets);
    return went;
}

int build_scene(engine::scene_object (&out)[k_max_objects], scene_kind kind, spin mode, float t,
                const scene_assets& assets, const floor_geometry& floor,
                const model_state& model, float roughness)
{
    const engine::mat3 spinning = build_spin(mode, t);

    // Three hues that are not the axis colours, so nothing in the scene can be
    // mistaken for a coordinate axis (conventions.html §8 reserves red/green/blue
    // for x/y/z, and that rule is worth honouring outside diagrams too).
    constexpr Uint32 k_amber = 0xFFE0A83Cu;
    constexpr Uint32 k_teal = 0xFF3CB8A8u;
    constexpr Uint32 k_violet = 0xFFA070D8u;

    if (kind == scene_kind::floor)
    {
        // One object, no transform at all: the floor is authored in world
        // coordinates, so its model matrix is the identity. That is unusual and
        // worth noticing — every other object in this course is a unit shape
        // placed by a transform, and a ground plane is the one thing it is more
        // honest to build where it lives.
        out[0].xform = engine::transform{};
        out[0].geometry = floor.geometry;
        out[0].name = "ground plane (checkered)";
        out[0].mat.tint = k_amber;   // unused: the floor is shaded from its uvs
        out[0].closed = false;   // a sheet, not a solid — 3.4 must not cull it
        return 1;
    }

    if (kind == scene_kind::model)
    {
        // ONE OBJECT, AND NOTHING ABOUT IT WAS TYPED HERE. Its vertex count, its
        // triangle count, its winding and whether it is closed at all are properties
        // of a file, discovered at load time. Every other branch of this function
        // builds geometry the course authored; this one places geometry it did not.
        const float s = display_scale(model.choice);
        out[0].xform.scale = {s, s, s};
        out[0].xform.position = {0.0f, 1.0f, 0.0f};
        out[0].xform.rotation = spinning * engine::rotation_x(0.35f);
        out[0].geometry = model.geometry;
        out[0].name = name_of(model.choice);
        out[0].mat.tint = k_amber;
        // The torus is the one mesh in the demo dense enough for a per-vertex
        // highlight to look like a highlight at all — 1,225 vertices against the
        // icosahedron's 12. [L] to the coarser models and watch it fall apart, which
        // is Lesson 3.8's argument arriving as a picture rather than a claim.
        // LESSON 6.4 RE-AUTHORED THIS, and the old line is worth keeping in view:
        //     out[0].mat.surface = {{0.85f, 0.85f, 0.85f}, shininess};
        // A specular reflectance of 0.85 is not a material. Run it back through
        // `ior_from_f0` and it claims an index of refraction of 25.3, where glass
        // is 1.5 and diamond is 2.4. It was that large because it was absorbing
        // the 17x normalisation error Lesson 6.3 measured. The physical number is
        // 0.04, it is the default, and it does not need writing down.
        out[0].mat.surface = {.roughness = roughness};

        // The 3.4 debt, paid. `closed` used to be a promise typed next to the
        // geometry; here it is the validator's answer. Note that it takes BOTH
        // conditions: twisted.obj is topologically closed and still unsafe to cull,
        // because culling reads winding and its winding disagrees with itself.
        out[0].closed = model.check.closed() && model.check.consistently_wound();
        return 1;
    }

    if (kind == scene_kind::solids)
    {
        // The hero. Tilted by a fixed rotation as well as the animated one so its
        // symmetry is never accidentally axis-aligned.
        out[0].xform.scale    = {0.9f, 0.9f, 0.9f};
        out[0].xform.position = {0.0f, 1.0f, 0.0f};
        out[0].xform.rotation = spinning * engine::rotation_x(0.5f);
        out[0].geometry       = assets.icosahedron;
        out[0].name           = "icosahedron (uniform, spinning)";
        out[0].mat.tint           = k_amber;
        out[0].closed         = true;
        // A WHITE highlight on an amber body — which is what a plastic looks like,
        // and not a stylistic choice. A dielectric (plastic, paint, glass, skin)
        // mirrors light off its clear surface layer without tinting it, so its
        // highlight is the colour of the LAMP; only the light that gets *inside*
        // picks up the pigment, and that is the diffuse term. Module 6 gives this a
        // name and a number (F0 ~ 0.04 for most dielectrics).
        out[0].mat.surface        = {.roughness = roughness};   // dielectric: F0 = 0.04

        out[1].xform.scale    = {1.8f, 0.35f, 0.9f};
        out[1].xform.position = {-1.6f, 0.5f, 0.4f};
        out[1].xform.rotation = spinning;
        out[1].geometry       = assets.cube;
        out[1].name           = "slab   (non-uniform, spinning)";
        out[1].mat.tint           = k_teal;
        out[1].closed         = true;
        // A TINTED highlight, and the contrast with the icosahedron is the point: a
        // METAL colours what it reflects, because it has no clear layer over a
        // pigmented interior — a metal is reflection all the way down. Gold is
        // yellow in its highlight, copper orange. Compare the two objects under a
        // white light and the difference is unmistakable.
        //
        // LESSON 6.4 MADE IT ACTUALLY BE ONE. Until now the metal was faked by
        // hand-typing a teal highlight colour beside a teal tint — two numbers
        // that had to be kept in step by whoever remembered. `metallic = 1` says
        // it once: `f0_of` takes the F0 from the albedo, `diffuse_albedo_of`
        // removes the diffuse lobe entirely, and the object cannot fall out of
        // agreement with itself. That is the metallic workflow, and it arrives
        // as a CONSEQUENCE of where Fresnel put the colour.
        out[1].mat.surface        = {.roughness = roughness, .metallic = 1.0f};

        out[2].xform.scale    = {1.2f, 0.25f, 1.2f};
        out[2].xform.position = {1.4f, 0.125f, 0.9f};
        out[2].xform.rotation = engine::mat3::identity();
        out[2].geometry       = assets.cube;
        out[2].name           = "plinth (non-uniform, still)";
        out[2].mat.tint           = k_violet;
        out[2].closed         = true;
        // THE CONTROL. No highlight at all, so [H] and [E] must not change one pixel
        // of it. Two of three objects were controls for the composition order in 2.8
        // and for the normal matrix in 3.6; the habit is worth keeping.
        out[2].mat.surface        = {};
        return 3;
    }

    if (kind == scene_kind::cycle)
    {
        // THE CYCLE. Three planks laid along the sides of an equilateral
        // triangle, each one tilted so that it passes IN FRONT OF the next at the
        // corner they share and BEHIND the previous one at the other end. Woven,
        // exactly like three sticks laid over and under each other.
        //
        //   at corner 2:  A in front of B
        //   at corner 3:  B in front of C
        //   at corner 1:  C in front of A     <- and now it is a loop
        //
        // No ordering of three items can satisfy all three constraints; that is
        // what "cyclic" means. And every plank's centre sits at exactly z = 0, so
        // their average depths are IDENTICAL — the sort key cannot even prefer a
        // wrong answer, it has nothing to compare. Verified in
        // scratch/verify_31.cpp; Lesson 3.1 §1.3.
        constexpr float R = 1.2f;
        constexpr float k_third = 2.0f * 3.14159265358979f / 3.0f;
        const engine::vec2 c1{R * std::cos(1.5707963f), R * std::sin(1.5707963f)};
        const engine::vec2 c2{R * std::cos(1.5707963f + k_third),
                              R * std::sin(1.5707963f + k_third)};
        const engine::vec2 c3{R * std::cos(1.5707963f + 2.0f * k_third),
                              R * std::sin(1.5707963f + 2.0f * k_third)};

        constexpr float k_width = 1.05f;
        constexpr float k_tilt = 0.55f;
        constexpr float k_overhang = 0.5f;

        out[0].xform = make_plank(c1, c2, k_width, k_tilt, k_overhang);
        out[0].geometry = assets.quad;
        out[0].name = "plank A (C1->C2)";
        out[0].mat.tint = k_amber;
        out[0].closed = false;

        out[1].xform = make_plank(c2, c3, k_width, k_tilt, k_overhang);
        out[1].geometry = assets.quad;
        out[1].name = "plank B (C2->C3)";
        out[1].mat.tint = k_teal;
        out[1].closed = false;

        out[2].xform = make_plank(c3, c1, k_width, k_tilt, k_overhang);
        out[2].geometry = assets.quad;
        out[2].name = "plank C (C3->C1)";
        out[2].mat.tint = k_violet;
        out[2].closed = false;

        // Lift the weave so its centre sits exactly on the camera's target. With
        // the elevation at zero that makes all three planks EQUIDISTANT from the
        // eye, so their average view depths are identical to the last bit — a
        // renderer that sorts whole objects has literally nothing to compare.
        for (int i = 0; i < 3; ++i) { out[i].xform.position.y += 0.6f; }
        return 3;
    }

    if (kind == scene_kind::intersect)
    {
        // TWO QUADS PASSING THROUGH EACH OTHER. Their centres are a clear 0.2
        // apart in z, so the sort key gives a confident answer — and the answer is
        // right on one side of the intersection line and wrong on the other.
        //
        // This is the deeper failure. The cycle at least has no correct order; here
        // a correct order exists PER PIXEL and simply cannot be expressed
        // per-triangle. No sorting algorithm, however clever, can fix that: the
        // question "which triangle is in front" has no single answer.
        out[0].xform.scale    = {2.6f, 2.0f, 1.0f};
        out[0].xform.position = {0.0f, 1.1f, +0.1f};
        out[0].xform.rotation = engine::rotation_y(+0.7f);
        out[0].geometry       = assets.quad;
        out[0].name           = "quad A (nearer centre)";
        out[0].mat.tint           = k_amber;
        out[0].closed         = false;

        out[1].xform.scale    = {2.6f, 2.0f, 1.0f};
        out[1].xform.position = {0.0f, 1.1f, -0.1f};
        out[1].xform.rotation = engine::rotation_y(-0.7f);
        out[1].geometry       = assets.quad;
        out[1].name           = "quad B (further centre)";
        out[1].mat.tint           = k_teal;
        out[1].closed         = false;
        return 2;
    }

    // NEAR-COPLANAR. The z-buffer's own failure mode, and the reason a depth
    // format is a decision rather than a detail.
    //
    // Two large panels a hair apart, both turned well away from face-on so that
    // depth varies strongly across the screen. B is nearer everywhere, so the
    // correct picture is "B, entirely". Whether you get it depends on whether the
    // buffer can represent a gap this small at this distance — press [B] and
    // watch D16_UNORM fail to. Lesson 3.1 §3.6.
    out[0].xform.scale    = {3.0f, 2.4f, 1.0f};
    out[0].xform.position = {0.0f, 1.1f, 0.0f};
    out[0].xform.rotation = engine::rotation_y(0.9f);
    out[0].geometry       = assets.quad;
    out[0].name           = "panel A (behind)";
    out[0].mat.tint           = k_amber;
    out[0].closed         = false;

    out[1].xform.scale    = {3.0f, 2.4f, 1.0f};
    out[1].xform.position = {0.0f, 1.1f, 0.001f};   // one millimetre nearer. That is all.
    out[1].xform.rotation = engine::rotation_y(0.9f);
    out[1].geometry       = assets.quad;
    out[1].name           = "panel B (1 mm in front)";
    out[1].mat.tint           = k_teal;
    out[1].closed         = false;
    return 2;
}

void draw_world(engine::framebuffer& fb, const engine::mat4& view, const engine::projector& pr)
{
    constexpr float reach = 2.5f;
    const Uint32 faint = engine::pack_argb(40, 44, 60);
    const Uint32 axis_line = engine::pack_argb(70, 76, 100);

    // Gridlines every half unit, with the two lines through the origin brighter
    // so the world's own axes are readable inside the mesh of the floor.
    for (int i = -5; i <= 5; ++i)
    {
        const float f = static_cast<float>(i) * 0.5f;
        const Uint32 c = (i == 0) ? axis_line : faint;
        engine::line3_world(fb, view, pr, {f, 0.0f, -reach}, {f, 0.0f, reach}, c);
        engine::line3_world(fb, view, pr, {-reach, 0.0f, f}, {reach, 0.0f, f}, c);
    }

    // The world's own axis triad, at the world origin, in the course colours.
    // Every object's position is measured from exactly this point.
    engine::line3_world(fb, view, pr, {0.0f, 0.0f, 0.0f}, {0.9f, 0.0f, 0.0f}, engine::pack_argb(150, 66, 66));
    engine::line3_world(fb, view, pr, {0.0f, 0.0f, 0.0f}, {0.0f, 0.9f, 0.0f}, engine::pack_argb(78, 130, 100));
    engine::line3_world(fb, view, pr, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.9f}, engine::pack_argb(82, 108, 156));
}

[[nodiscard]] engine::mat4 demo_orthographic()
{
    constexpr float half_w = 6.478f;                    // view-space units mapped to the viewport
    constexpr float half_h = half_w / k_scene_aspect;   // keep the same 16:9 shape
    constexpr float depth = k_scene_far - k_scene_near;
    return {{1.0f / half_w, 0.0f, 0.0f, 0.0f},
            {0.0f, 1.0f / half_h, 0.0f, 0.0f},
            {0.0f, 0.0f, -1.0f / depth, 0.0f},
            {0.0f, 0.0f, -k_scene_near / depth, 1.0f}};
}


// ---------------------------------------------------------------------------
// Lesson 5.1 — the characterization shot
// ---------------------------------------------------------------------------

Uint32 fnv1a(const void* data, std::size_t bytes)
{
    const auto* p = static_cast<const Uint8*>(data);
    Uint32 h = 2166136261u;
    for (std::size_t i = 0; i < bytes; ++i)
    {
        h = static_cast<Uint32>((h ^ p[i]) * 16777619u);
    }
    return h;
}

int write_reference_shot(const char* path)
{
    // Every one of these is a constant BECAUSE it is an input. Anything read from
    // the clock, the keyboard or the window would make two runs incomparable.
    constexpr float k_shot_t = 1.234f;
    // LESSON 6.4: shininess 32 read across into the new vocabulary. Lesson 6.3's
    // `alpha_from_blinn_exponent(32)` is 0.2425, whose perceptual roughness is
    // 0.4925 — so 0.49 is not a new choice, it is the old default translated. The
    // golden moves for the first time in fifteen lessons anyway, because the
    // CONSTANTS moved; see Lesson 6.4 §9.
    constexpr float k_shot_roughness = 0.49f;
    constexpr int k_shot_floor_cells = 4;
    constexpr float k_shot_light_elev = 0.70f;
    constexpr float k_shot_light_azim = 0.85f;
    const Uint32 k_shot_bg = engine::pack_argb(12, 14, 20);

    engine::framebuffer fb(k_fb_width, k_fb_height);
    engine::depth_buffer depth(k_fb_width, k_fb_height);

    // Lesson 5.4: one owner for every mesh the shot draws. Built here rather
    // than passed in, because the whole value of this function is that its inputs
    // are constants — an asset store handed in from outside would be one more
    // thing that could differ between two runs, and (5.5) would bring its own
    // search path with it, which is the most important input of all.
    scene_assets assets;
    assets.build();

    floor_geometry floor;
    build_floor(assets, floor, k_shot_floor_cells);

    model_state model;
    model.generated = assets.store.insert_mesh("generated:torus",
                                               engine::make_torus(48, 24, 1.0f, 0.4f));
    load_model(assets, model, model_choice::torus, true);

    texture_set textures;
    textures.build();

    engine::lighting lights;
    {
        const float ce = std::cos(k_shot_light_elev);
        const engine::vec3 to_light{ce * std::sin(k_shot_light_azim),
                                    std::sin(k_shot_light_elev),
                                    ce * std::cos(k_shot_light_azim)};
        lights.key.direction = -to_light;
        lights.key.colour = {1.0f, 0.97f, 0.90f};
        // Lesson 6.2: `intensity` became `irradiance`, and the value became pi.
        // A light of "intensity 1" was always a light of irradiance pi; the
        // pi has moved into the BRDF where it belongs, so it must now be
        // written down here. Same picture, stated honestly.
        lights.key.irradiance = engine::k_reference_irradiance;
    }

    const orbit_camera cam;
    const engine::mat4 view_from_world = cam.view();
    const engine::vec3 eye_world = cam.eye();
    const engine::mat4 proj = engine::perspective(k_scene_fovy, k_scene_aspect,
                                                  k_scene_near, k_scene_far);

    constexpr scene_kind k_shot_scenes[] = {
        scene_kind::solids, scene_kind::cycle, scene_kind::intersect,
        scene_kind::zfight, scene_kind::floor, scene_kind::model,
        // …and the floor a SEVENTH time, from a camera standing on it. The first
        // six frames all report `straddle = 0`: from the default viewpoint no
        // triangle crosses the near plane, so Lesson 3.3's clipper — code this
        // refactor is about to move — is never called. A characterization test
        // that does not reach a branch cannot pin it.
        scene_kind::floor,

        // …and the model an EIGHTH time, LIT PER PIXEL — added in Lesson 6.4, for
        // exactly the reason frame 6 was added in Lesson 5.1, which is worth
        // reading twice because it is the same mistake caught twice.
        //
        // 6.4 replaced the entire shading model and this shot moved by 2,400
        // pixels out of 403,200. That is not because the change was small; it is
        // because THE INSTRUMENT BARELY POINTS AT THE THING THAT CHANGED. Frames
        // 4, 5 and 6 bind a texture, and `shading::textured` is UNLIT by
        // construction (raster.hpp says so) — the sampled colour goes straight to
        // the pixel. Frame 1's three planks face away from the light entirely and
        // encode to `albedo * ambient` exactly, a term 6.4 does not touch. So of
        // seven frames, THREE exercised the shading equation at all, and the
        // torus — the one mesh in the demo dense enough to show a highlight,
        // chosen for that reason in Lesson 3.8 — was drawn unlit.
        //
        // `shading::lit` with the texture bound as the ALBEDO is the combination
        // that runs the whole path: per-pixel normals (3.8), a sampled albedo
        // (3.9), and the Cook-Torrance BRDF (6.4) evaluated per fragment.
        //
        // AND THE MOMENT TO ADD IT IS NOW, which is the transferable part: this
        // lesson re-baselines the golden anyway, so extending the test costs
        // nothing here and costs a re-baseline at every other lesson. A
        // characterization test is cheapest to improve exactly when it breaks.
        scene_kind::model};
    constexpr int k_shot_frames = static_cast<int>(std::size(k_shot_scenes));

    /// The camera for frame 6: down at ankle height and a metre from the target,
    /// which puts the near edge of the ground plane behind the eye.
    orbit_camera close_cam;
    close_cam.radius = 1.0f;
    close_cam.elevation = 0.05f;
    close_cam.target = {0.0f, 0.35f, 0.0f};

    std::vector<Uint8> rgb;
    rgb.reserve(static_cast<std::size_t>(k_fb_width) * k_fb_height * k_shot_frames * 3u);

    std::vector<engine::raster_triangle> tris;
    engine::projection_scratch scratch;
    const engine::sampler samp;

    for (int f = 0; f < k_shot_frames; ++f)
    {
        const scene_kind kind = k_shot_scenes[f];

        engine::scene_object scene[k_max_objects];
        const int count = build_scene(scene, kind, spin::about_z, k_shot_t,
                                      assets, floor, model, k_shot_roughness);

        const bool lit_pass = (f == k_shot_frames - 1);   ///< frame 7, Lesson 6.4
        const bool close_up = (f == k_shot_frames - 2);
        const engine::mat4 view = close_up ? close_cam.view() : view_from_world;
        const engine::vec3 eye = close_up ? close_cam.eye() : eye_world;

        const engine::viewport& vp = (kind == scene_kind::floor)
                                   ? k_full_viewport : k_scene_viewport;
        const engine::projector pr{proj, vp, engine::near_mode::clip};

        fb.clear(k_shot_bg);
        draw_world(fb, view, pr);

        // Every field left at its default, because every default is the right
        // answer — which is what makes this the reference render.
        engine::collect_stats measured;
        engine::collect_triangles(tris, scratch, {scene, static_cast<std::size_t>(count)},
                                  assets.meshes(), {view, eye}, pr, lights,
                                  engine::render_options{}, &measured);

        // The two uv surfaces read the orientation chart; everything else reads
        // its vertex colours. One texture rather than none, so the sampler is on
        // the covered path.
        const bool bind_texture = (kind == scene_kind::floor) || (kind == scene_kind::model);

        // LESSON 6.5: the material holds a HANDLE, and the resolve to a pointer
        // happens here — once per draw, outside the fill loop. That ordering is
        // the whole architectural point of `bind_albedo`: a handle is how you
        // store a reference, a pointer is how you use one per pixel, and the
        // resolve is a named step between them.
        const engine::material draw_material{
            .albedo_map = bind_texture ? textures.pick(albedo_source::uv_grid)
                                       : engine::texture_handle{},
            .samp = samp,
            .surface = lit_pass ? engine::microsurface{.roughness = 0.30f}
                                : engine::microsurface{}};
        const engine::texture_binding albedo = engine::bind_albedo(draw_material,
                                                                   textures.pool);
        const engine::fill_style style{
            .interp = engine::interpolation::perspective,
            .shade = lit_pass    ? engine::shading::lit
                   : bind_texture ? engine::shading::textured
                                  : engine::shading::vertex_colour,
            .space = engine::blend_space::linear,
            .cull = engine::cull_mode::none,
            // `lit` REQUIRES this to be non-null — raster.hpp falls back to
            // vertex colours when it is null rather than dereferencing nothing,
            // which would make frame 7 silently retest frame 5.
            .lights = lit_pass ? &lights : nullptr,
            // One surface for the whole draw, because `fill_style` carries one.
            // Roughness 0.30 is smoother than the scene default, so the highlight
            // is small enough to have a SHAPE — a broad one covers the mesh and
            // pins nothing about where D peaks.
            .surface = draw_material.surface,
            .model = engine::specular_model::cook_torrance,
            .eye = eye,
            .albedo = albedo,
            .encode = engine::encode_mode::fast,
            .traverse = engine::traversal::scanline};

        depth.clear();
        engine::cull_stats cull;
        engine::quad_stats quads;
        engine::draw_triangles(fb, &depth, tris, false, style, &cull, &quads);

        const std::size_t frame_start = rgb.size();
        int painted = 0;   ///< pixels the frame changed from the clear colour
        for (int y = 0; y < k_fb_height; ++y)
        {
            for (int x = 0; x < k_fb_width; ++x)
            {
                const Uint32 px = fb.pixel_at(x, y);
                if (px != k_shot_bg) { ++painted; }
                rgb.push_back(static_cast<Uint8>((px >> 16) & 0xFFu));
                rgb.push_back(static_cast<Uint8>((px >> 8) & 0xFFu));
                rgb.push_back(static_cast<Uint8>(px & 0xFFu));
            }
        }
        (void)quads;   // scanline traversal does not fill it; [5] is the mode that does

        const Uint32 hash = fnv1a(rgb.data() + frame_start, rgb.size() - frame_start);
        SDL_Log("shot %d  %-28s in=%4d straddle=%3d out=%4d  drawn=%4d  "
                "painted=%6d  hash=%08X",
                f, name_of(kind), measured.clip.input, measured.clip.straddling,
                measured.clip.output,
                cull.drawn, painted, hash);
    }

    const Uint32 whole = fnv1a(rgb.data(), rgb.size());
    SDL_Log("shot ALL  %d frames, %zu bytes, hash=%08X", k_shot_frames, rgb.size(), whole);

    SDL_IOStream* io = SDL_IOFromFile(path, "wb");
    if (io == nullptr)
    {
        SDL_Log("reference shot: cannot open %s: %s", path, SDL_GetError());
        return 1;
    }
    char header[64];
    const int header_len = SDL_snprintf(header, sizeof(header), "P6\n%d %d\n255\n",
                                        k_fb_width, k_fb_height * k_shot_frames);
    SDL_WriteIO(io, header, static_cast<std::size_t>(header_len));
    SDL_WriteIO(io, rgb.data(), rgb.size());
    SDL_CloseIO(io);
    SDL_Log("reference shot: wrote %s", path);
    return 0;
}

}   // namespace demo
