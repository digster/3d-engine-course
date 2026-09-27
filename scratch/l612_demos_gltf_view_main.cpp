// demos/gltf_view/main.cpp — a glTF file on screen, through the public API only.
//
// Lesson 6.6, and it is `hello_cube`'s successor in one specific sense: that
// program was the acceptance test for the LIBRARY BOUNDARY (Lesson 5.1), and
// this one is the acceptance test for the ASSET SYSTEM. If a program outside the
// engine cannot say "load that file and draw it" in a handful of lines, then the
// asset system is not an asset system — it is a collection of functions that
// happen to live near each other.
//
//     cmake --build build --target gltf_view
//     ./build/demos/gltf_view                          shapes.glb, spinning
//     ./build/demos/gltf_view --model cube.gltf        the textured cube
//     ./build/demos/gltf_view --shot out.ppm           one frame, no window
//
// WHAT TO LOOK FOR. Three shapes with three materials, and the materials came
// out of the FILE: the octahedron is gold because `baseColorFactor` said
// (1.00, 0.71, 0.29) and `metallicFactor` said 1, so `f0_of` put that colour in
// the reflectance and `diffuse_albedo_of` gave it no diffuse lobe at all
// (Lesson 6.4). Nothing in this program types a colour.
//
// AND ONE THING THE STRUCTURE MAKES VISIBLE. The draw loop below is one
// `collect_triangles` + `draw_triangles` PER OBJECT, not one for the scene, and
// that is not laziness — `fill_style` carries exactly one material, so two
// objects with different materials cannot share a call. It is the CPU-side twin
// of Lesson 4.8's three pipelines and a sort, and it is what a draw-list sorted
// by material is FOR.

#include <engine/asset/asset_store.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/depth_buffer.hpp>
#include <engine/gfx/gltf.hpp>
#include <engine/gfx/light.hpp>
#include <engine/gfx/material.hpp>
#include <engine/gfx/mesh.hpp>
#include <engine/gfx/projector.hpp>
#include <engine/gfx/raster.hpp>
#include <engine/gfx/bounds.hpp>
#include <engine/gfx/scene.hpp>
#include <engine/gfx/cascade.hpp>
#include <engine/gfx/draw_order.hpp>
#include <engine/gfx/hdr.hpp>
#include <engine/gfx/mipmap.hpp>
#include <engine/gfx/shadow.hpp>
#include <engine/gfx/soft_renderer.hpp>
#include <engine/gfx/viewport.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/transform.hpp>
#include <engine/platform/app.hpp>

#include <engine/platform/main.hpp>

#include <memory>
#include <vector>

namespace {

constexpr int k_width = 480;
constexpr int k_height = 270;
constexpr Uint32 k_background = 0xFF0C0E14u;

/// Pull a `transform` out of a glTF node's world matrix.
///
/// **AND SAY WHAT IT CANNOT DO, because this is a real seam and not a helper.**
/// `engine::transform` is T * R * S with a `mat3` rotation; a glTF node may
/// carry an arbitrary 4x4 `matrix`, and not every 4x4 is a TRS — a sheared or
/// non-uniformly-scaled-then-rotated node is perfectly legal glTF and cannot be
/// expressed here at all. What comes back is the translation, the scale
/// recovered as the length of each basis column, and the rotation recovered by
/// dividing those lengths out.
///
/// That is exact for every node built from `translation`/`rotation`/`scale`
/// properties, which is what an exporter writes and what both of this course's
/// assets use. It silently drops shear. The honest fix is for `scene_object` to
/// hold a `mat4` — which is what an engine eventually does, and which Lesson
/// 5.9's hierarchy already computes — and it is not this lesson's to make.
[[nodiscard]] engine::transform transform_of(const engine::mat4& m)
{
    engine::transform t;
    t.position = {m.c3.x, m.c3.y, m.c3.z};

    const engine::vec3 cx{m.c0.x, m.c0.y, m.c0.z};
    const engine::vec3 cy{m.c1.x, m.c1.y, m.c1.z};
    const engine::vec3 cz{m.c2.x, m.c2.y, m.c2.z};

    t.scale = {engine::length(cx), engine::length(cy), engine::length(cz)};

    // A zero-length column means a degenerate node (scale 0 on an axis). Leaving
    // the rotation column as the basis vector keeps the matrix finite instead of
    // producing NaNs that spread through every subsequent frame.
    const engine::vec3 rx = (t.scale.x > 0.0f) ? cx * (1.0f / t.scale.x)
                                               : engine::vec3{1.0f, 0.0f, 0.0f};
    const engine::vec3 ry = (t.scale.y > 0.0f) ? cy * (1.0f / t.scale.y)
                                               : engine::vec3{0.0f, 1.0f, 0.0f};
    const engine::vec3 rz = (t.scale.z > 0.0f) ? cz * (1.0f / t.scale.z)
                                               : engine::vec3{0.0f, 0.0f, 1.0f};
    t.rotation = engine::mat3{rx, ry, rz};
    return t;
}

class gltf_app final : public engine::app
{
public:
    [[nodiscard]] engine::app_config configure(int argc, char* argv[]) override
    {
        for (int i = 1; i < argc; ++i)
        {
            if (SDL_strcmp(argv[i], "--shot") == 0 && i + 1 < argc)
            {
                shot_path_ = argv[++i];
            }
            else if (SDL_strcmp(argv[i], "--model") == 0 && i + 1 < argc)
            {
                model_name_ = argv[++i];
            }
            else if (SDL_strcmp(argv[i], "--bumps") == 0 && i + 1 < argc)
            {
                // Cells across the map; 0 turns normal mapping off entirely, so
                // the two pictures can be put side by side. Lesson 6.7 — and the
                // map is GENERATED rather than shipped, which is the same door
                // `insert_mesh` has used for generated geometry since 5.5.
                bump_cells_ = SDL_atoi(argv[++i]);
            }
            else if (SDL_strcmp(argv[i], "--shadow") == 0 && i + 1 < argc)
            {
                // The map's side, in texels; 0 turns the whole pass off so the
                // before picture and the after picture come out of the same
                // binary. Lesson 6.8 — and the number is on the command line
                // rather than fixed because RESOLUTION IS THE SUBJECT: every
                // formula in §4 is a multiple of `world_per_texel`, and halving
                // this doubles the bias the same geometry needs.
                shadow_res_ = SDL_atoi(argv[++i]);
            }
            else if (SDL_strcmp(argv[i], "--bias") == 0 && i + 1 < argc)
            {
                const char* w = argv[++i];
                bias_ = (SDL_strcmp(w, "none") == 0)     ? engine::shadow_bias::none
                      : (SDL_strcmp(w, "constant") == 0) ? engine::shadow_bias::constant
                      : (SDL_strcmp(w, "normal") == 0)   ? engine::shadow_bias::normal_offset
                                                         : engine::shadow_bias::slope_scaled;
            }
            else if (SDL_strcmp(argv[i], "--constant-bias") == 0 && i + 1 < argc)
            {
                constant_bias_ = static_cast<float>(SDL_atof(argv[++i]));
            }
            else if (SDL_strcmp(argv[i], "--pcf") == 0 && i + 1 < argc)
            {
                pcf_ = SDL_atoi(argv[++i]);
            }
            // LESSON 6.9. `--cascades 4` splits the CAMERA's frustum instead of
            // boxing the whole scene. `--cascades 1` is 6.8's single map fitted
            // to the frustum rather than the scene, which is worth having
            // separately: it isolates "cascades" from "camera-fitted".
            // LESSON 6.10. `--mips` builds a chain per albedo texture and binds
            // it; `--aniso N` turns on anisotropic filtering. Both off by
            // default, because a chain costs 33% more memory and a surface that
            // was never undersampled gains nothing from one.
            // A fine checker on the ground, which is the surface that minifies.
            // This is Lesson 3.9's shimmering floor, made reachable again so the
            // fix has something to fix.
            else if (SDL_strcmp(argv[i], "--floor-tex") == 0 && i + 1 < argc)
            {
                floor_tex_ = SDL_atoi(argv[++i]);
            }
            else if (SDL_strcmp(argv[i], "--mips") == 0)
            {
                mips_ = true;
            }
            else if (SDL_strcmp(argv[i], "--aniso") == 0 && i + 1 < argc)
            {
                mips_ = true;
                aniso_ = SDL_atoi(argv[++i]);
            }
            else if (SDL_strcmp(argv[i], "--mip-nearest") == 0)
            {
                // 3.5's rule: the failure has to be reachable. This is the
                // visible line across the floor that trilinear exists to hide.
                mips_ = true;
                mip_filter_ = engine::filter::nearest;
            }
            // LESSON 6.11. `--alpha` adds the transparency showcase: two
            // overlapping glass panes and a cutout leaf card. `--no-sort` draws
            // the blended objects in scene order so the failure is one keystroke
            // away, and `--blend-encoded` composites on the stored bytes, which
            // is the 43%-too-dark bug made visible.
            // LESSON 6.12. `--hdr` renders into a float buffer and resolves it;
            // `--tonemap` picks the curve, `--ev` the exposure, `--white` the
            // point that `reinhard-white` sends to 1, and `--luma` switches from
            // per-channel to luminance-only. Without `--hdr` the demo draws
            // exactly what it drew in 6.11, into 8 bits, with a clamp.
            else if (SDL_strcmp(argv[i], "--hdr") == 0)
            {
                hdr_ = true;
            }
            else if (SDL_strcmp(argv[i], "--tonemap") == 0 && i + 1 < argc)
            {
                hdr_ = true;
                const char* n = argv[++i];
                if (SDL_strcmp(n, "reinhard") == 0)      { tone_.op = engine::tonemap::reinhard; }
                else if (SDL_strcmp(n, "white") == 0)    { tone_.op = engine::tonemap::reinhard_white; }
                else if (SDL_strcmp(n, "aces") == 0)     { tone_.op = engine::tonemap::aces; }
                else                                     { tone_.op = engine::tonemap::clamp; }
            }
            else if (SDL_strcmp(argv[i], "--ev") == 0 && i + 1 < argc)
            {
                hdr_ = true;
                ev100_ = static_cast<float>(SDL_atof(argv[++i]));
            }
            else if (SDL_strcmp(argv[i], "--white") == 0 && i + 1 < argc)
            {
                tone_.white = static_cast<float>(SDL_atof(argv[++i]));
            }
            else if (SDL_strcmp(argv[i], "--luma") == 0)
            {
                tone_.per_channel = false;
            }
            // A surface polished enough to blow the lid off. The demo's default
            // roughness is 0.49, which peaks at 0.8676 — just under 1 — and that
            // is exactly why the course has got away without HDR for six modules.
            else if (SDL_strcmp(argv[i], "--polish") == 0 && i + 1 < argc)
            {
                polish_ = static_cast<float>(SDL_atof(argv[++i]));
            }
            else if (SDL_strcmp(argv[i], "--alpha") == 0)
            {
                alpha_demo_ = true;
            }
            else if (SDL_strcmp(argv[i], "--no-sort") == 0)
            {
                sort_blended_ = false;
            }
            else if (SDL_strcmp(argv[i], "--blend-encoded") == 0)
            {
                blend_encoded_ = true;
            }
            else if (SDL_strcmp(argv[i], "--cascades") == 0 && i + 1 < argc)
            {
                cascades_ = SDL_atoi(argv[++i]);
            }
            else if (SDL_strcmp(argv[i], "--csm-lambda") == 0 && i + 1 < argc)
            {
                csm_lambda_ = static_cast<float>(SDL_atof(argv[++i]));
            }
            else if (SDL_strcmp(argv[i], "--csm-blend") == 0 && i + 1 < argc)
            {
                csm_blend_ = static_cast<float>(SDL_atof(argv[++i]));
            }
            // 3.5's rule: the failure has to be reachable, or the fix is folklore.
            else if (SDL_strcmp(argv[i], "--no-snap") == 0)
            {
                csm_snap_ = false;
            }
            else if (SDL_strcmp(argv[i], "--shadow-cull") == 0 && i + 1 < argc)
            {
                const char* w = argv[++i];
                shadow_cull_ = (SDL_strcmp(w, "front") == 0) ? engine::cull_mode::front
                             : (SDL_strcmp(w, "back") == 0)  ? engine::cull_mode::back
                                                             : engine::cull_mode::none;
            }
            else if (SDL_strcmp(argv[i], "--no-ground") == 0)
            {
                ground_ = false;
            }
            else if (SDL_strcmp(argv[i], "--no-ground-cast") == 0)
            {
                // Keep the ground, drop it from the shadow pass. See `casters()`
                // for the two separate things this changes.
                ground_casts_ = false;
            }
            else if (SDL_strcmp(argv[i], "--pose") == 0 && i + 1 < argc)
            {
                // Where on the orbit a `--shot` is taken, in radians. It exists
                // because a METAL has essentially one interesting camera angle —
                // the one that puts a facet in the mirror direction — and being
                // able to sweep past it is how you see that for yourself rather
                // than being told.
                pose_ = static_cast<float>(SDL_atof(argv[++i]));
            }
        }

        return {.title = "gltf_view — a file, drawn",
                .draw_to = (shot_path_ != nullptr) ? engine::surface::headless
                                                   : engine::surface::renderer,
                .fb_width = k_width,
                .fb_height = k_height};
    }

    [[nodiscard]] bool on_start() override
    {
        // ---- THE WHOLE LOAD. One call. -------------------------------------
        //
        // Behind it: a search path resolves the name, cgltf parses the container
        // (text or binary — this program never finds out which), the node tree is
        // flattened, every primitive's accessors are read into a `mesh_data` and
        // inserted under a derived name, every material's base colour image is
        // resolved, decoded, channel-shuffled and cached, and every material is
        // built and named. That list is the argument for an asset system: not one
        // step of it is about THIS program.
        // ---- An OBJ is a mesh, not a model — Lesson 6.7 --------------------
        //
        // One branch, and it earns its place by making a point no glTF asset in
        // this repository can: **OBJ has no tangent attribute at all**, so a
        // model loaded this way gets its frame from `with_tangents` and nowhere
        // else. `assets/torus.obj` also has a real uv chart on every triangle,
        // which `assets/cube.gltf` does not — Lesson 6.6 gave that cube one uv
        // per box corner so its round trip could be index-for-index, and the
        // consequence is that four of its six faces have ZERO uv area and
        // therefore no tangent frame at all. Normal mapping is what makes that
        // visible; see §7 of the lesson.
        const std::string requested(model_name_);
        if (requested.size() > 4
            && requested.compare(requested.size() - 4, 4, ".obj") == 0)
        {
            if (!load_obj_model(requested)) { return false; }
            return finish_setup();
        }

        const engine::model_load model = assets_.load_model(model_name_);

        if (!model.ok())
        {
            SDL_Log("gltf_view: could not load '%s' (%s)",
                    model_name_, engine::name_of(model.report.status));
            return false;
        }

        SDL_Log("gltf_view: %s -> %d primitive(s), %d vertices, %d triangles, "
                "%zu material(s)%s",
                model_name_, model.report.primitives, model.report.vertices,
                model.report.triangles, model.materials.size(),
                model.report.binary ? "  [.glb]" : "  [.gltf]");
        if (model.textures_missing > 0)
        {
            SDL_Log("  %d texture(s) could not be resolved — those surfaces fall "
                    "back to their base colour", model.textures_missing);
        }

        // ---- File data -> scene objects ------------------------------------
        //
        // The only loop this program owns, and it is a translation between two
        // representations rather than any kind of work: a handle, a placement and
        // a material become a `scene_object`.
        objects_.reserve(model.meshes.size());
        for (std::size_t i = 0; i < model.meshes.size(); ++i)
        {
            engine::scene_object obj;
            obj.geometry = model.meshes[i];
            obj.name = "gltf primitive";
            obj.xform = transform_of(model.placements[i]);

            // A material by VALUE on the object, resolved from the handle here —
            // Lesson 6.5's rule, and this is a case where it applies cleanly:
            // each primitive has exactly one material, so a handle on the object
            // would add a lookup and buy nothing. The pool still exists, and
            // still shares: two primitives naming one file material resolve to
            // one entry in it.
            if (const engine::material* m = assets_.material_at(model.mesh_material[i]))
            {
                obj.mat = *m;
            }

            // THE FACT, MEASURED, NOT PROMISED — 6.5's `cull_of` pair. A file's
            // geometry is untrusted input, so `validate()` answers whether it is
            // closed and the file's `doubleSided` supplies the intent. A glTF
            // primitive is very often an open shell, and culling one makes it
            // vanish from behind.
            if (const engine::mesh_data* g = assets_.mesh_at(obj.geometry))
            {
                obj.closed = engine::validate(g->view()).closed();
            }

            // FRAME THE MODEL FROM ITS OWN GEOMETRY. A viewer with a
            // hard-coded camera distance works for exactly one asset, which is
            // the asset it was written against — and every glTF file has a
            // different scale, because the spec's unit is the metre and a chair
            // and a building are both legal.
            //
            // The bounds are accumulated in WORLD space, so the node transforms
            // are what make the plinth (a cube flattened by its own node) count
            // as flat rather than as a cube. Note that the accessors already
            // carry `min`/`max` — the spec requires them on POSITION precisely
            // so a viewer can do this WITHOUT decoding the vertices — and
            // reading them instead is Exercise 3.
            if (const engine::mesh_data* g = assets_.mesh_at(obj.geometry))
            {
                const engine::mat4 m = model.placements[i];
                for (const engine::vec3 v : g->vertices)
                {
                    const engine::vec4 w = m * engine::vec4{v.x, v.y, v.z, 1.0f};
                    bounds_.expand(engine::vec3{w.x, w.y, w.z});
                }
            }

            objects_.push_back(obj);
        }

        return finish_setup();
    }

    /// Load an OBJ as one object and DERIVE its tangent frame.
    [[nodiscard]] bool load_obj_model(const std::string& name)
    {
        const engine::mesh_load loaded = assets_.load_mesh(name);
        if (!loaded.ok())
        {
            SDL_Log("gltf_view: could not load '%s' (%s)",
                    name.c_str(), engine::name_of(loaded.report.status));
            return false;
        }

        const engine::mesh_data* src = assets_.mesh_at(loaded.handle);
        if (src == nullptr) { return false; }

        // A DERIVED ASSET, through the door Lesson 5.5 built for exactly this
        // shape: the tangent pass produces a mesh FROM a mesh, with no name of
        // its own and no reason to exist beyond its source — so unloading the
        // source takes it. `with_normals` was the first thing to have this
        // shape and 5.5's header named the pattern then.
        const engine::mesh_handle framed =
            assets_.derive_mesh(loaded.handle, engine::with_tangents(src->view()));
        if (!framed.valid()) { return false; }

        const engine::mesh_data* g = assets_.mesh_at(framed);
        if (g == nullptr) { return false; }

        SDL_Log("gltf_view: %s -> %zu vertices, %zu triangles, %zu tangents "
                "(OBJ carries none, so every one was DERIVED from the uv chart)",
                name.c_str(), g->vertices.size(), g->triangle_count(),
                g->tangents.size());

        engine::scene_object obj;
        obj.geometry = framed;
        obj.name = "obj model";
        obj.mat.tint = 0xFFE0A83Cu;
        obj.mat.surface = {.roughness = 0.40f};
        obj.closed = engine::validate(g->view()).closed();
        objects_.push_back(obj);

        for (const engine::vec3 v : g->vertices)
        {
            bounds_.expand(v);
        }
        return true;
    }

    /// Framing, the generated normal map and the light — shared by both paths.
    [[nodiscard]] bool finish_setup()
    {
        centre_ = bounds_.centre();
        const engine::vec3 extent = bounds_.extent();
        const float radius = bounds_.radius();

        // Enough distance that a sphere of that radius fits the vertical field
        // of view, plus a margin. tan(fov/2) is the half-height at unit depth,
        // so `radius / tan(fov/2)` is the depth at which it exactly fills.
        distance_ = 1.45f * radius / SDL_tanf(0.5f * k_fov_y);

        SDL_Log("  bounds (%.2f %.2f %.2f) .. (%.2f %.2f %.2f), camera at %.2f",
                static_cast<double>(bounds_.min.x), static_cast<double>(bounds_.min.y),
                static_cast<double>(bounds_.min.z), static_cast<double>(bounds_.max.x),
                static_cast<double>(bounds_.max.y), static_cast<double>(bounds_.max.z),
                static_cast<double>(distance_));

        // ---- LESSON 6.7: a normal map, generated and inserted -------------
        //
        // Through `insert_texture`, which is the same door a loaded one comes
        // through — 5.5's rule that an asset system which can only LOAD is
        // missing half its job, applied to the newest asset type. The texture
        // arrives carrying `texel_space::linear` because `make_normal_bumps`
        // set it: an image whose meaning is fixed by the function that made it
        // should not leave the caller to declare what it is.
        //
        // ONLY MATERIALS ON GEOMETRY THAT HAS TANGENTS GET ONE. A normal map is
        // a direction in the surface's own frame, and a mesh with no uvs has no
        // frame — `shapes.glb` carries POSITION only, so it is correctly left
        // flat and the log says so rather than the picture leaving you guessing.
        if (bump_cells_ > 0)
        {
            const engine::texture_handle bumps = assets_.insert_texture(
                "generated:bumps", engine::make_normal_bumps(256, bump_cells_, 1.0f));

            int mapped = 0;
            for (engine::scene_object& obj : objects_)
            {
                const engine::mesh_data* g = assets_.mesh_at(obj.geometry);
                if (g == nullptr || g->tangents.empty()) { continue; }
                obj.mat.normal_map = bumps;
                ++mapped;
            }
            SDL_Log("  normal map    : %d cells, applied to %d of %zu object(s) "
                    "(the rest carry no tangents, so they have no frame to map into)",
                    bump_cells_, mapped, objects_.size());
        }

        lights_.key.direction = engine::normalised(engine::vec3{-0.45f, -0.65f, -0.62f});
        lights_.key.colour = {1.0f, 0.97f, 0.90f};
        lights_.key.irradiance = engine::k_reference_irradiance;
        lights_.ambient = {0.05f, 0.06f, 0.08f};

        // ---- LESSON 6.8: SOMETHING FOR THE SHADOW TO LAND ON ---------------
        //
        // Every picture in this course so far has had its subject floating in
        // the dark, and that was fine while nothing could occlude anything. A
        // shadow needs a receiver, and the receiver is where the whole lesson
        // becomes visible: acne is a pattern ACROSS A LIT SURFACE, and a scene
        // with no large lit surface cannot show one.
        //
        // The quad is authored in the z = 0 plane (`quad_mesh`), so the rotation
        // below is the one that takes local +z to world +y. Written as three
        // columns rather than a rotation helper because Lesson 2.5's sentence is
        // the fastest way to check it: a matrix IS where the basis vectors land,
        // so read column 2 and confirm it says (0, 1, 0).
        caster_count_ = static_cast<int>(objects_.size());
        if (ground_)
        {
            const engine::mesh_handle quad = assets_.insert_mesh(
                "generated:ground",
                engine::with_normals(engine::quad_mesh(), engine::normal_style::flat));
            if (!quad.valid()) { return false; }

            const float side = 3.0f * SDL_max(SDL_max(extent.x, extent.z), 1e-3f);

            engine::scene_object g;
            g.geometry = quad;
            g.name = "ground";
            g.xform.position = {centre_.x, bounds_.min.y, centre_.z};
            g.xform.rotation = engine::mat3{{1.0f, 0.0f, 0.0f},
                                            {0.0f, 0.0f, -1.0f},
                                            {0.0f, 1.0f, 0.0f}};
            g.xform.scale = {side, side, 1.0f};
            g.mat.tint = 0xFF9A9AA2u;
            g.mat.surface = {.roughness = 0.85f};

            // LESSON 6.10. A checker DELIBERATELY finer than the screen can
            // resolve at distance, which is 3.9's shimmering floor rebuilt: the
            // sparkle is the missing minification filter, and it is the picture
            // this lesson exists to fix.
            if (floor_tex_ > 0)
            {
                const int side_px = floor_tex_;
                const int cell = SDL_max(1, side_px / 64);
                engine::texture checker(side_px, side_px, 0xFF000000u,
                                        engine::texel_space::srgb);
                for (int y = 0; y < side_px; ++y)
                {
                    for (int x = 0; x < side_px; ++x)
                    {
                        const bool on = ((x / cell) + (y / cell)) % 2 == 0;
                        checker.set_texel(x, y, on ? 0xFFF2F2F2u : 0xFF1A1A1Au);
                    }
                }
                g.mat.albedo_map = assets_.insert_texture("floor checker",
                                                          std::move(checker));
                g.mat.tint = 0xFFFFFFFFu;   // the texture IS the albedo (3.9)
            }

            // A sheet has no inside, so back-face culling would delete it when
            // seen from below — Lesson 3.4's rule, and `cull_of` is where it
            // lives.
            g.closed = false;
            objects_.push_back(g);
        }

        // ---- The map ------------------------------------------------------
        //
        // `shadow_res_` is a side, so the storage is its square: 1024 costs 4 MB
        // of float depth here and 2 MB as a 16-bit target on the GPU.
        // ---- LESSON 6.11: the transparency showcase -------------------------
        //
        // THREE OBJECTS, chosen so that each names one of the lesson's three
        // subjects and so that the failure of each is visible on its own:
        //
        //   a CUTOUT LEAF CARD, `alpha_mode::mask`. Its alpha channel is a hard
        //        stencil, so it needs no sorting and no blend state at all — the
        //        cheapest transparency there is, and the one people reach for
        //        last because "alpha" sounds like one feature.
        //   TWO GLASS PANES, `alpha_mode::blend`, deliberately OVERLAPPING and
        //        at different depths. One pane proves nothing; two prove the
        //        order matters, and `--no-sort` draws them in scene order so the
        //        wrong one is a keystroke away.
        //
        // They are built here rather than loaded, because the point is to have a
        // scene whose transparency is known exactly — a downloaded asset's glass
        // would be a second unknown in a measurement of the first.
        if (alpha_demo_)
        {
            // `with_normals` for the same reason the ground needs it: these are
            // lit surfaces, and `quad_mesh` is bare geometry with uvs and no
            // normals. A quad with no normal shades black, which looks like a
            // transparency bug and is not one.
            const engine::mesh_handle quad = assets_.insert_mesh(
                "generated:alpha quad",
                engine::with_normals(engine::quad_mesh(), engine::normal_style::flat));
            if (quad.valid())
            {
                const float span = SDL_max(SDL_max(extent.x, extent.z), 1e-3f);

                // ---- Which way do they face? ---------------------------------
                //
                // `quad_mesh` is authored in the z = 0 plane facing +z, and a
                // pane seen edge-on demonstrates nothing. So they are yawed to
                // face the camera's SHOT pose — `atan2(dx, dz)` is the angle that
                // sends +z to the horizontal direction back toward the eye —
                // and placed along that same axis, in front of the model.
                //
                // In a live window the camera keeps orbiting past them, which is
                // not a defect: it is the demonstration. As the eye crosses the
                // panes' plane the CORRECT order flips, `order_draws` flips with
                // it, and `--no-sort` visibly does not.
                const float yaw = SDL_atan2f(0.86f * SDL_cosf(pose_),
                                             0.86f * SDL_sinf(pose_));
                const float cy = SDL_cosf(yaw);
                const float sy = SDL_sinf(yaw);
                const engine::mat3 facing{{cy, 0.0f, sy},
                                          {0.0f, 1.0f, 0.0f},
                                          {-sy, 0.0f, cy}};

                // The unit vector from the model toward the camera, on the
                // ground plane. Everything below is placed along it.
                const engine::vec3 toward{0.86f * SDL_cosf(pose_), 0.0f,
                                          0.86f * SDL_sinf(pose_)};
                const engine::vec3 dir = engine::normalised(toward);
                const engine::vec3 side{-dir.z, 0.0f, dir.x};

                // ---- The leaf card ------------------------------------------
                //
                // A circle of full coverage inside a square of none, which is a
                // leaf reduced to the only property that matters here: an alpha
                // channel with a HARD EDGE. The transparent texels are black —
                // which is what an image editor leaves behind, and which is
                // precisely the colour that bleeds into the edge under a
                // straight-alpha filter (§5). Making them black is not laziness,
                // it is the realistic case.
                constexpr int k_leaf = 128;
                engine::texture leaf(k_leaf, k_leaf, 0x00000000u,
                                     engine::texel_space::srgb);
                for (int y = 0; y < k_leaf; ++y)
                {
                    for (int x = 0; x < k_leaf; ++x)
                    {
                        const float dx = (static_cast<float>(x) + 0.5f) / k_leaf - 0.5f;
                        const float dy = (static_cast<float>(y) + 0.5f) / k_leaf - 0.5f;
                        const bool inside = (dx * dx + dy * dy) < (0.34f * 0.34f);
                        leaf.set_texel(x, y, inside ? 0xFF4E9A3Cu : 0x00000000u);
                    }
                }

                engine::scene_object card;
                card.geometry = quad;
                card.name = "leaf card";
                card.xform.position = {centre_.x + dir.x * span * 1.4f - side.x * span * 1.1f,
                                       centre_.y,
                                       centre_.z + dir.z * span * 1.4f - side.z * span * 1.1f};
                card.xform.rotation = facing;
                card.xform.scale = {span * 1.5f, span * 1.5f, 1.0f};
                card.mat.albedo_map = assets_.insert_texture("leaf", std::move(leaf));
                card.mat.tint = 0xFFFFFFFFu;
                card.mat.surface = {.roughness = 0.7f};
                card.mat.mode = engine::alpha_mode::mask;
                card.mat.alpha_cutoff = engine::k_default_alpha_cutoff;
                card.closed = false;   // a sheet: `cull_of` will not cull it
                objects_.push_back(card);

                // ---- Two panes of glass -------------------------------------
                //
                // OVERLAPPING IN SCREEN SPACE AND SEPARATED IN DEPTH, which is
                // the only arrangement in which "which one is in front" is a
                // question the picture can answer. Coverage 0.45 and 0.55: both
                // well away from 0 and 1, where a compositing error hides.
                //
                // Note the build ORDER — near pane first — which is exactly the
                // wrong order for blending. `--no-sort` draws them like that.
                const float along[2] = {1.6f, 0.7f};
                const float across[2] = {0.30f, 0.75f};
                const Uint32 tints[2] = {0xFF4C7FD8u, 0xFFD8574Cu};
                const float alphas[2] = {0.45f, 0.55f};
                const char* names[2] = {"glass near", "glass far"};
                for (int k = 0; k < 2; ++k)
                {
                    engine::scene_object pane;
                    pane.geometry = quad;
                    pane.name = names[k];
                    pane.xform.position = {
                        centre_.x + dir.x * span * along[k] + side.x * span * across[k],
                        centre_.y,
                        centre_.z + dir.z * span * along[k] + side.z * span * across[k]};
                    pane.xform.rotation = facing;
                    pane.xform.scale = {span * 1.6f, span * 1.9f, 1.0f};
                    pane.mat.tint = tints[k];
                    pane.mat.surface = {.roughness = 0.25f};
                    pane.mat.mode = engine::alpha_mode::blend;
                    pane.mat.alpha = alphas[k];
                    pane.closed = false;
                    objects_.push_back(pane);
                }
            }
        }

        // ---- LESSON 6.12: take the lid off ---------------------------------
        //
        // One line, and it is what makes the rest of this lesson visible. The
        // scene's materials arrive from the file at roughness ~0.5, whose
        // mirror-angle peak is 0.8676 — JUST under 1, which is exactly why this
        // course has got away without an HDR pipeline for six modules. Polish
        // them and the same shading equation returns 11.59 at roughness 0.20 and
        // 2823.99 at 0.05, every bit of which an 8-bit target stores as code 255.
        //
        // AFTER every object exists, including the ground and 6.11's glass —
        // placing it earlier would have polished the model and left the floor
        // alone, which is the sort of half-applied setting that makes a
        // measurement quietly meaningless.
        if (polish_ >= 0.0f)
        {
            for (engine::scene_object& o : objects_)
            {
                o.mat.surface.roughness = polish_;
            }

            // AND A TORUS, because a sharp highlight has to be CAUGHT. This is
            // worth more than a convenience: at roughness 0.15 the specular lobe
            // is a few degrees wide, so a scene of flat faces presents the mirror
            // angle at no pixel at all and the HDR buffer comes back with a peak
            // of 0.22 — which is how a renderer with a real clipping problem can
            // look completely fine. A torus is curved in BOTH directions, so some
            // point on it satisfies the mirror condition from every viewpoint,
            // and the highlight cannot hide.
            const engine::mesh_handle torus = assets_.insert_mesh(
                "generated:hdr torus", engine::make_torus(64, 32, 1.0f, 0.38f));
            if (torus.valid())
            {
                const float span = SDL_max(SDL_max(extent.x, extent.z), 1e-3f);
                engine::scene_object t;
                t.geometry = torus;
                t.name = "polished torus";
                t.xform.position = {centre_.x, centre_.y + span * 0.25f, centre_.z};
                t.xform.scale = {span * 0.9f, span * 0.9f, span * 0.9f};
                t.mat.tint = 0xFFF0E4C8u;
                t.mat.surface = {.roughness = polish_, .metallic = 1.0f};
                t.closed = true;
                objects_.push_back(t);
            }

            SDL_Log("  polish        : every material forced to roughness %.2f, "
                    "plus a metal torus to catch the lobe", polish_);
        }

        // ---- LESSON 6.10: one chain per DISTINCT albedo texture -------------
        //
        // Keyed by pointer rather than one per object, because two objects
        // sharing a texture must share its chain — building it twice would
        // double the memory and make the 33% claim a lie.
        if (mips_)
        {
            for (const engine::scene_object& o : objects_)
            {
                const engine::texture* img = assets_.textures().get(o.mat.albedo_map);
                if (img == nullptr || img->empty()) { continue; }
                bool seen = false;
                for (const auto& e : chain_of_) { if (e.first == img) { seen = true; break; } }
                if (seen) { continue; }
                chains_.push_back(engine::build_mips(*img));
                chain_of_.emplace_back(img, chains_.size() - 1);
            }
            std::size_t base = 0, total = 0;
            for (std::size_t i = 0; i < chains_.size(); ++i)
            {
                base += static_cast<std::size_t>(chains_[i].width())
                      * static_cast<std::size_t>(chains_[i].height());
                total += chains_[i].texels();
            }
            SDL_Log("  mipmaps       : %zu chain(s), %d levels, %zu -> %zu texels (+%.1f%%), "
                    "aniso %d, between levels %s",
                    chains_.size(), chains_.empty() ? 0 : chains_[0].levels(), base, total,
                    base == 0 ? 0.0 : 100.0 * (static_cast<double>(total) / static_cast<double>(base) - 1.0),
                    aniso_,
                    mip_filter_ == engine::filter::linear ? "trilinear" : "NEAREST (the seam)");
        }

        if (shadow_res_ > 0)
        {
            engine::shadow_settings ss;
            ss.bias = bias_;
            ss.constant_bias = constant_bias_;
            ss.pcf_radius = pcf_;
            ss.cull = shadow_cull_;

            if (cascades_ > 0)
            {
                if (!cascade_.create(cascades_, shadow_res_)) { return false; }
                cascade_.settings().count = cascade_.count();
                cascade_.settings().lambda = csm_lambda_;
                cascade_.settings().snap = csm_snap_;
                cascade_.settings().blend_fraction = csm_blend_;
                cascade_.settings().near_d = 0.1f;
                // Stop the cascades WELL SHORT of the camera's own 100 m far
                // plane. Shadows out there are worth nothing and every metre
                // spent on them makes cascade 0 coarser.
                cascade_.settings().far_d = 4.0f * distance_;
                cascade_.apply_shadow_settings(ss);
                SDL_Log("  cascades      : %d x %dx%d, lambda %.2f, snap %s, blend %.2f",
                        cascade_.count(), shadow_res_, shadow_res_,
                        static_cast<double>(csm_lambda_), csm_snap_ ? "on" : "OFF",
                        static_cast<double>(csm_blend_));
            }
            if (!shadows_.create(shadow_res_)) { return false; }
            shadows_.settings() = ss;

            SDL_Log("  shadow map    : %dx%d, bias %s, PCF %dx%d, caster cull %s",
                    shadow_res_, shadow_res_, engine::name_of(bias_),
                    2 * pcf_ + 1, 2 * pcf_ + 1,
                    (shadow_cull_ == engine::cull_mode::front) ? "FRONT"
                    : (shadow_cull_ == engine::cull_mode::back) ? "BACK" : "none");
        }

        if (shot_path_ != nullptr) { t_ = pose_; }
        return true;
    }

    /// The objects the shadow pass rasterises. Everything, by default.
    ///
    /// **`--no-ground-cast` DROPS THE GROUND FROM THE MAP, AND TWO THINGS
    /// CHANGE AT ONCE.** The first is the fit: the box is sized to its contents,
    /// and a ground plane three times the model's width triples the box's side
    /// and cuts the texel density to a ninth — a 1024 map made to resolve like a
    /// 341. That is the commonest reason a shadow map looks blocky, and it costs
    /// one `subspan` to avoid.
    ///
    /// The second is more interesting and is §4.2's whole point: **the ground
    /// stops having acne.** Acne is a surface failing its own depth test, so a
    /// surface that is not IN the map cannot have it. That is a real trick and a
    /// bad general rule — it works here because this ground is one flat sheet,
    /// and it fails the moment a receiver is folded, stacked or curved enough to
    /// occlude itself.
    [[nodiscard]] std::span<const engine::scene_object> casters() const
    {
        const std::size_t n = ground_casts_ ? objects_.size()
                                            : static_cast<std::size_t>(caster_count_);
        return std::span<const engine::scene_object>(objects_.data(), n);
    }

    void on_fixed_step(float h) override
    {
        if (shot_path_ == nullptr) { t_ += h * 0.6f; }
        if (in().key_down(SDL_SCANCODE_ESCAPE)) { request_quit(); }
    }

    void on_frame(float alpha) override
    {
        (void)alpha;

        fb().clear(k_background);
        depth_.clear();

        // ---- LESSON 6.12: the float target ---------------------------------
        //
        // Allocated on first use rather than in the constructor, because a demo
        // run without `--hdr` should not pay 6.22 MB for a buffer it never
        // touches — three times the 8-bit framebuffer, which is the honest price
        // of the whole lesson.
        //
        // IT IS CLEARED IN LINEAR LIGHT, and the conversion is the interesting
        // part: `k_background` is an authored sRGB colour and the HDR buffer
        // holds quantities of light, so the clear goes through `to_linear`.
        // Clearing it to the raw bytes would make the background 2.3x too bright
        // after the resolve re-encodes — 6.1's bug, in a new place.
        if (hdr_)
        {
            if (!hdr_fb_ || hdr_fb_->width() != fb().width()
                || hdr_fb_->height() != fb().height())
            {
                hdr_fb_ = std::make_unique<engine::hdr_buffer>(fb().width(), fb().height());
            }
            hdr_fb_->clear(engine::to_linear(k_background));
            tone_.exposure = engine::exposure_from_ev100(ev100_);
        }

        // The whole model spins about the world origin, so the objects keep
        // their relative placement — which is the point of not baking the node
        // transforms into the geometry.
        const engine::vec3 eye = orbit_eye();
        const engine::mat4 view = engine::look_at(eye, centre_, {0.0f, 1.0f, 0.0f});

        // ---- LESSON 6.8: THE DEPTH PASS ------------------------------------
        //
        // Before the camera draws anything, the light does. Every frame, because
        // the fit depends on the scene's bounds and an animated scene moves —
        // this one does not, and re-fitting anyway is the honest default: a map
        // cached against a scene that has changed is a shadow of where things
        // used to be.
        if (cascade_.valid())
        {
            // The camera's frustum, as the fit needs it: the view matrix
            // inverted (rigid, so `rigid_inverse` is the whole inverse) plus the
            // two numbers `perspective` was built from.
            engine::camera_frustum cf;
            cf.world_from_view = engine::rigid_inverse(view);
            cf.fovy_radians = k_fov_y;
            cf.aspect = static_cast<float>(k_width) / static_cast<float>(k_height);
            cascade_.render(casters(), assets_.meshes(), lights_.key, cf,
                            &shadow_stats_);
        }
        else if (shadows_.valid())
        {
            shadows_.render(casters(), assets_.meshes(), lights_.key, &shadow_stats_);
        }

        // ---- LESSON 6.11: THE ORDER, DECIDED BEFORE ANYTHING IS DRAWN ------
        //
        // The first thing in this course that has to happen *before* the frame
        // rather than during it. Opaque geometry never needed it — the z-buffer
        // IS the sort, per pixel — and blending takes that away, because `over`
        // is not commutative.
        //
        // REBUILT EVERY FRAME, and cheaply: the keys are 12 bytes and there are
        // as many as there are objects, so this is a sort of a handful of small
        // records rather than of the draws themselves. `order_stats_.out_of_order`
        // then says whether it was worth doing, which for an orbiting camera and
        // static geometry is "only while the camera crosses the panes' plane".
        order_.clear();
        order_.reserve(objects_.size());
        const engine::vec3 view_axis = engine::normalised(centre_ - eye);
        for (std::size_t i = 0; i < objects_.size(); ++i)
        {
            const engine::scene_object& o = objects_[i];
            order_.push_back(engine::draw_key{
                .index = static_cast<int>(i),
                .depth = engine::view_depth(o.xform.position, eye, view_axis),
                .mode = o.mat.mode});
        }
        order_stats_ = engine::order_draws(order_);

        int drawn = 0;
        for (const engine::draw_key& key : order_)
        {
            // `--no-sort` walks the ORIGINAL order instead, which is scene order
            // — near pane first, because that is how it was built. One keystroke,
            // and the far pane tints the near one instead of the other way round.
            const engine::scene_object& obj =
                sort_blended_ ? objects_[static_cast<std::size_t>(key.index)]
                              : objects_[static_cast<std::size_t>(&key - order_.data())];

            engine::collect_triangles(triangles_, scratch_, {&obj, 1}, assets_.meshes(),
                                      {view, eye}, projector_, lights_,
                                      engine::render_options{
                                          .cull = engine::cull_choice::back});

            // ONE RESOLVE PER DRAW, which is exactly where 6.5 said it belongs:
            // a handle is how you store a texture reference, a pointer is how
            // you read one per pixel, and `bind_albedo` is the named step
            // between them. Inside the fill loop it would be a bounds check and
            // a generation compare per fragment.
            engine::texture_binding albedo =
                engine::bind_albedo(obj.mat, assets_.textures());
            if (mips_)
            {
                // 6.10. The chain is nullable and null is the whole of "off",
                // so the unmipped path is the one every earlier lesson drew.
                for (const auto& e : chain_of_)
                {
                    if (e.first == albedo.image) { albedo.mips = &chains_[e.second]; break; }
                }
                albedo.samp.mip_filter = mip_filter_;
                albedo.samp.max_anisotropy = aniso_;
            }
            const engine::texture_binding normals =
                engine::bind_normal_map(obj.mat, assets_.textures());

            const engine::fill_style style{
                .interp = engine::interpolation::perspective,
                .shade = engine::shading::lit,
                .space = engine::blend_space::linear,
                .cull = engine::cull_of(obj.closed),
                .lights = &lights_,
                .surface = obj.mat.surface,
                .model = engine::specular_model::cook_torrance,
                .eye = eye,
                .albedo = albedo,
                .normal_map = normals,   // 6.7
                .encode = engine::encode_mode::fast,
                .traverse = engine::traversal::scanline,

                // 6.8. Null when `--shadow 0`, which is the whole of "turn the
                // feature off" — the same nullable-pointer bargain `lights` and
                // `albedo` already make, so a picture without shadows is the
                // picture this demo drew yesterday, bit for bit.
                .shadows = (!cascade_.valid() && shadows_.valid()) ? &shadows_ : nullptr,

                // 6.9. Takes priority when both are set; the view axis is what
                // selects a cascade, and it must be the AXIAL depth.
                .cascades = cascade_.valid() ? &cascade_ : nullptr,
                .view_eye = eye,
                .view_forward = engine::normalised(centre_ - eye),

                // ---- 6.11 -------------------------------------------------
                //
                // Straight off the material, because the material is where the
                // artist's intent lives (`material::mode`) and the fill is where
                // it becomes behaviour. `opacity` and `alpha_cutoff` are handed
                // over unconditionally: an opaque fill ignores both, which is
                // what keeps every earlier picture in this demo identical.
                //
                // (Designators must appear in DECLARATION order, which is why
                // they sit here beside `depth_only` rather than up beside
                // `encode` where they would read better. C++20 evaluates them in
                // declaration order regardless.)
                .transparency = obj.mat.mode,
                .opacity = obj.mat.alpha,
                .alpha_cutoff = obj.mat.alpha_cutoff,
                .blend_encoded = blend_encoded_,

                // 6.12. Null is the whole of "this fill is LDR", and it is the
                // sixth field on this struct to make that bargain. The
                // framebuffer is still passed — it supplies the dimensions —
                // and is simply never written to.
                .hdr = hdr_ ? hdr_fb_.get() : nullptr};

            engine::draw_triangles(fb(), &depth_, triangles_, false, style);
            drawn += static_cast<int>(triangles_.size());
        }

        // ---- LESSON 6.12: THE RESOLVE --------------------------------------
        //
        // Exposure, curve, encode — one pass over a FINISHED image, which is the
        // structural point of the lesson. Everything above this line rendered
        // quantities of light; this is where they become codes.
        //
        // It happens after the scene and before the overlay, and that ordering is
        // a real decision rather than an accident of where it was easiest to put:
        // the debug overlay is authored in sRGB and drawn at the codes it was
        // authored in, so tonemapping it would darken text nobody asked to be
        // darkened. Module 9's editor will want exactly the same split.
        if (hdr_ && hdr_fb_)
        {
            const engine::hdr_stats st = engine::measure(*hdr_fb_);
            engine::resolve(*hdr_fb_, fb(), tone_, engine::encode_mode::fast);

            if (shot_path_ != nullptr)
            {
                SDL_Log("  hdr           : max channel %.4f (%.1f stops), max luminance %.4f",
                        static_cast<double>(st.max_channel),
                        static_cast<double>(SDL_log(SDL_max(st.max_channel, 1e-6f))
                                            / SDL_log(2.0)),
                        static_cast<double>(st.max_luminance));
                SDL_Log("  over the lid  : %d of %d pixels (%.2f%%) have a channel above 1 — "
                        "every one of them would be code 255 under a clamp",
                        st.over_one, st.pixels,
                        static_cast<double>(100.0f * st.over_fraction()));
                SDL_Log("  exposure      : EV100 %.3f -> x%.4f   curve %s%s   white %.2f",
                        static_cast<double>(ev100_),
                        static_cast<double>(tone_.exposure), engine::name_of(tone_.op),
                        tone_.per_channel ? "" : " (luminance only)",
                        static_cast<double>(tone_.white));
                SDL_Log("  log-average   : %.5f  (arithmetic mean %.5f — the gap is what one "
                        "specular pixel does to a mean)",
                        static_cast<double>(st.log_mean_luminance),
                        static_cast<double>(st.mean_luminance));
            }
        }

        if (shot_path_ != nullptr)
        {
            SDL_Log("gltf_view: %zu object(s), %d triangles", objects_.size(), drawn);
            // 6.11. Printed whether or not anything is transparent, because
            // "blended: 0" is the answer that says the sort cost nothing — and a
            // counter you only see when the feature is on is a counter that
            // cannot tell you the feature is off.
            SDL_Log("  draw order    : %d opaque (%d of them masked), %d blended, "
                    "%d adjacent pair(s) out of order before sorting%s",
                    order_stats_.opaque, order_stats_.masked, order_stats_.blended,
                    order_stats_.out_of_order,
                    sort_blended_ ? "" : "  [--no-sort: the sort's result is IGNORED]");
            if (shadows_.valid() || cascade_.valid())
            {
                SDL_Log("  shadow pass  : %d caster triangles into %d texels, %.2f ms",
                        shadow_stats_.triangles, shadow_stats_.texels,
                        shadow_stats_.render_ms);

                if (cascade_.valid())
                {
                    // ONE LINE PER CASCADE, because the whole point of the
                    // lesson is that they differ — a single averaged number
                    // would hide exactly the thing worth seeing.
                    float near_d = cascade_.settings().near_d;
                    for (int i = 0; i < cascade_.count(); ++i)
                    {
                        const engine::light_camera& c = cascade_.map(i).camera();
                        const float far_d = cascade_.splits()[static_cast<std::size_t>(i)];
                        SDL_Log("  cascade %d    : [%6.2f, %6.2f] m  %.5f m/texel  "
                                "depth range %.3f",
                                i, static_cast<double>(near_d),
                                static_cast<double>(far_d),
                                static_cast<double>(c.world_per_texel),
                                static_cast<double>(c.depth_range));
                        near_d = far_d;
                    }
                }
                else
                {
                    const engine::light_camera& lc = shadows_.camera();
                    SDL_Log("  fit          : %.4f world units per texel, depth range %.3f",
                            static_cast<double>(lc.world_per_texel),
                            static_cast<double>(lc.depth_range));
                }
            }
            request_quit(engine::save_ppm(fb(), shot_path_));
        }
    }

private:
    /// A slow orbit at the fitted distance, looking at the model's own centre.
    /// A still frame is therefore reproducible and a live one shows every side.
    [[nodiscard]] engine::vec3 orbit_eye() const
    {
        return {centre_.x + distance_ * 0.86f * SDL_cosf(t_),
                centre_.y + distance_ * 0.42f,
                centre_.z + distance_ * 0.86f * SDL_sinf(t_)};
    }

    const char* shot_path_ = nullptr;
    const char* model_name_ = "shapes.glb";
    float t_ = 0.0f;
    float pose_ = 3.0f;   ///< the orbit angle a --shot freezes at
    int bump_cells_ = 6;  ///< 6.7: cells across the generated normal map; 0 = off

    // ---- Lesson 6.8 --------------------------------------------------------
    int shadow_res_ = 1024;        ///< the map's side in texels; 0 = no shadows
    int cascades_ = 0;          // 6.9: 0 = 6.8's single map
    bool mips_ = false;         // 6.10
    bool hdr_ = false;                       ///< 6.12: render into a float buffer
    float ev100_ = engine::k_reference_ev100;///< 6.12: the exposure, as an EV
    float polish_ = -1.0f;                   ///< 6.12: override every roughness
    engine::tonemap_settings tone_{};        ///< 6.12: curve, white point, per-channel
    std::unique_ptr<engine::hdr_buffer> hdr_fb_;   ///< 6.12: allocated on first use
    bool alpha_demo_ = false;      ///< 6.11: build the glass and the leaf card
    bool sort_blended_ = true;     ///< 6.11: --no-sort makes the failure reachable
    bool blend_encoded_ = false;   ///< 6.11: composite on stored bytes, wrongly
    std::vector<engine::draw_key> order_;   ///< 6.11: rebuilt every frame
    engine::order_report order_stats_{};    ///< 6.11: what the sort was worth
    int floor_tex_ = 0;         // checker side, 0 = untextured ground
    int aniso_ = 1;
    engine::filter mip_filter_ = engine::filter::linear;
    std::vector<engine::mip_chain> chains_;
    std::vector<std::pair<const engine::texture*, std::size_t>> chain_of_;
    float csm_lambda_ = 0.5f;   // uniform <-> logarithmic blend
    float csm_blend_ = 0.0f;    // 0 shows the seam, which is the point
    bool csm_snap_ = true;
    int pcf_ = 1;                  ///< kernel radius: 0 = one tap, 1 = 3x3
    int caster_count_ = 0;         ///< objects_ before the ground was appended
    bool ground_ = true;
    bool ground_casts_ = true;    ///< is the ground in the map? See casters().
    float constant_bias_ = 0.0f;
    engine::shadow_bias bias_ = engine::shadow_bias::slope_scaled;
    engine::cull_mode shadow_cull_ = engine::cull_mode::none;
    engine::shadow_map shadows_;
    engine::cascaded_shadow_map cascade_;   // 6.9
    engine::shadow_stats shadow_stats_;

    static constexpr float k_fov_y = 50.0f * 3.14159265f / 180.0f;

    engine::asset_store assets_;
    std::vector<engine::scene_object> objects_;
    engine::lighting lights_;

    /// World-space bounds of everything loaded, and what the camera derives from
    /// them.
    ///
    /// **LESSON 6.8 REPLACED TWO LOOSE `vec3` WITH ONE `aabb`**, and the reason
    /// is not tidiness: the shadow map needs exactly this quantity to fit its
    /// orthographic box, so the pattern acquired its third caller and moved into
    /// the engine (`gfx/bounds.hpp`). The inside-out default that used to be
    /// spelled `1e30f` here by hand is now the type's own, which is what removes
    /// the first-vertex special case.
    engine::aabb bounds_;
    engine::vec3 centre_{};
    float distance_ = 4.0f;

    const engine::projector projector_{
        engine::perspective(k_fov_y,
                            static_cast<float>(k_width) / static_cast<float>(k_height),
                            0.1f, 100.0f),
        engine::viewport{0.0f, 0.0f, static_cast<float>(k_width),
                         static_cast<float>(k_height), 0.0f, 1.0f},
        engine::near_mode::clip};

    engine::depth_buffer depth_{k_width, k_height};

    std::vector<engine::raster_triangle> triangles_;
    engine::projection_scratch scratch_;
};

}   // namespace

ENGINE_MAIN(gltf_app)
