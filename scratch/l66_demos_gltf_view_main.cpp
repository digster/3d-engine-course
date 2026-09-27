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
#include <engine/gfx/scene.hpp>
#include <engine/gfx/soft_renderer.hpp>
#include <engine/gfx/viewport.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/transform.hpp>
#include <engine/platform/app.hpp>

#include <engine/platform/main.hpp>

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
                    bounds_min_ = {SDL_min(bounds_min_.x, w.x), SDL_min(bounds_min_.y, w.y),
                                   SDL_min(bounds_min_.z, w.z)};
                    bounds_max_ = {SDL_max(bounds_max_.x, w.x), SDL_max(bounds_max_.y, w.y),
                                   SDL_max(bounds_max_.z, w.z)};
                }
            }

            objects_.push_back(obj);
        }

        centre_ = (bounds_min_ + bounds_max_) * 0.5f;
        const engine::vec3 extent = bounds_max_ - bounds_min_;
        const float radius = 0.5f * engine::length(extent);

        // Enough distance that a sphere of that radius fits the vertical field
        // of view, plus a margin. tan(fov/2) is the half-height at unit depth,
        // so `radius / tan(fov/2)` is the depth at which it exactly fills.
        distance_ = 1.45f * radius / SDL_tanf(0.5f * k_fov_y);

        SDL_Log("  bounds (%.2f %.2f %.2f) .. (%.2f %.2f %.2f), camera at %.2f",
                static_cast<double>(bounds_min_.x), static_cast<double>(bounds_min_.y),
                static_cast<double>(bounds_min_.z), static_cast<double>(bounds_max_.x),
                static_cast<double>(bounds_max_.y), static_cast<double>(bounds_max_.z),
                static_cast<double>(distance_));

        lights_.key.direction = engine::normalised(engine::vec3{-0.45f, -0.65f, -0.62f});
        lights_.key.colour = {1.0f, 0.97f, 0.90f};
        lights_.key.irradiance = engine::k_reference_irradiance;
        lights_.ambient = {0.05f, 0.06f, 0.08f};

        if (shot_path_ != nullptr) { t_ = pose_; }
        return true;
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

        // The whole model spins about the world origin, so the objects keep
        // their relative placement — which is the point of not baking the node
        // transforms into the geometry.
        const engine::vec3 eye = orbit_eye();
        const engine::mat4 view = engine::look_at(eye, centre_, {0.0f, 1.0f, 0.0f});

        int drawn = 0;
        for (const engine::scene_object& obj : objects_)
        {
            engine::collect_triangles(triangles_, scratch_, {&obj, 1}, assets_.meshes(),
                                      {view, eye}, projector_, lights_,
                                      engine::render_options{
                                          .cull = engine::cull_choice::back});

            // ONE RESOLVE PER DRAW, which is exactly where 6.5 said it belongs:
            // a handle is how you store a texture reference, a pointer is how
            // you read one per pixel, and `bind_albedo` is the named step
            // between them. Inside the fill loop it would be a bounds check and
            // a generation compare per fragment.
            const engine::texture_binding albedo =
                engine::bind_albedo(obj.mat, assets_.textures());

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
                .encode = engine::encode_mode::fast,
                .traverse = engine::traversal::scanline};

            engine::draw_triangles(fb(), &depth_, triangles_, false, style);
            drawn += static_cast<int>(triangles_.size());
        }

        if (shot_path_ != nullptr)
        {
            SDL_Log("gltf_view: %zu object(s), %d triangles", objects_.size(), drawn);
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

    static constexpr float k_fov_y = 50.0f * 3.14159265f / 180.0f;

    engine::asset_store assets_;
    std::vector<engine::scene_object> objects_;
    engine::lighting lights_;

    /// World-space bounds of everything loaded, and what the camera derives from
    /// them. Seeded to the extremes so the first vertex sets both.
    engine::vec3 bounds_min_{1e30f, 1e30f, 1e30f};
    engine::vec3 bounds_max_{-1e30f, -1e30f, -1e30f};
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
