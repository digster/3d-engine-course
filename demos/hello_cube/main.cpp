// demos/hello_cube/main.cpp — the smallest program that proves the boundary.
//
// A lit cube, spinning, drawn by the engine's software renderer. Every symbol in
// it comes from a header under <engine/...>, and there is no include of the
// engine's internals here — there COULD not be: this target's include path
// contains engine/include and nothing else, so `#include "gfx/raster.hpp"` does
// not resolve (Lesson 5.1).
//
// LESSON 5.2 REWROTE IT, and the diff is the argument for the application layer.
// The file lost its `main`, its SDL_Init, its window, its renderer, its streaming
// texture, its `present()` helper, its event drain, its `while (running)`, its
// teardown, and its hand-rolled PPM writer. **Seventeen SDL lifecycle calls
// became zero, and 160 lines of code became 96** — and not one of the lines that
// went was about a cube. The picture it draws is byte-identical.
//
// What it gained is a shape: five overrides on `engine::app`, each of which is
// one phase of the loop Lesson 1.4 derived. The engine calls them.
//
//     cmake --build build --target hello_cube
//     ./build/demos/hello_cube                       a window, spinning
//     ./build/demos/hello_cube --shot cube.ppm       one frame, no window, no display

#include <engine/asset/asset_store.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/depth_buffer.hpp>
#include <engine/gfx/image.hpp>
#include <engine/gfx/light.hpp>
#include <engine/gfx/mesh.hpp>
#include <engine/gfx/projector.hpp>
#include <engine/gfx/raster.hpp>
#include <engine/gfx/scene.hpp>
#include <engine/gfx/soft_renderer.hpp>
#include <engine/gfx/viewport.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/transform.hpp>
#include <engine/platform/app.hpp>

// The entry point. Exactly one .cpp per program may include this, and that file
// must not define main() — both rules are explained at the top of the header and
// both are enforced by the linker.
#include <engine/platform/main.hpp>

#include <cmath>
#include <vector>

namespace {

constexpr int k_width = 320;
constexpr int k_height = 180;
constexpr Uint32 k_background = 0xFF0C0E14u;   // pack_argb(12, 14, 20)

class cube_app final : public engine::app
{
public:
    /// Read argv and decide what kind of program this run is. No SDL exists yet,
    /// which is exactly why the surface can still be chosen here: `--shot` picks
    /// `headless`, and a headless run never opens a window, never asks for the
    /// video subsystem, and therefore runs on a build server with no display.
    [[nodiscard]] engine::app_config configure(int argc, char* argv[]) override
    {
        for (int i = 1; i < argc; ++i)
        {
            if (SDL_strcmp(argv[i], "--shot") == 0 && i + 1 < argc) { shot_path_ = argv[++i]; }
        }

        return {.title = "hello_cube — built on engine::engine",
                .draw_to = (shot_path_ != nullptr) ? engine::surface::headless
                                                   : engine::surface::renderer,
                .fb_width = k_width,
                .fb_height = k_height};
    }

    /// Build what we are drawing. The window and the framebuffer already exist.
    [[nodiscard]] bool on_start() override
    {
        // `with_normals` is the import step: a cube's eight positions become
        // twenty-four vertices, because a corner where three faces meet needs
        // three different normals and a vertex carries exactly one (Lesson 3.5).
        //
        // LESSON 5.4 CHANGED WHERE THAT GEOMETRY LIVES, and this program is the
        // smallest place to read the change. There used to be a `mesh_data cube_`
        // member here whose comment admitted the whole problem: "the data must
        // outlive every frame that uses it, which is why it is a member and not a
        // local". That is not a design, it is a promise — enforced by nothing,
        // checked by nobody, and true only for as long as this file stays small.
        //
        // LESSON 5.5 GAVE IT A NAME. The pool became an `engine::asset_store`, so
        // the cube is generated content stored under `"cube"` — findable,
        // replaceable, unloadable and countable by exactly the machinery that
        // handles a file on disk. This program loads nothing, and that is the
        // point of putting it through the same door anyway: if generated content
        // needed a second, shabbier mechanism, the first one would be wrong.
        object_.geometry = assets_.insert_mesh(
            "cube", engine::with_normals(engine::cube_mesh(), engine::normal_style::flat));
        object_.name = "cube";
        object_.tint = 0xFFE0A83Cu;             // amber; not an axis colour
        object_.surface = {.colour = {0.6f, 0.6f, 0.6f}, .shininess = 48.0f};
        object_.xform.position = {0.0f, 0.0f, 0.0f};
        object_.xform.scale = {1.0f, 1.0f, 1.0f};

        // One directional light. `direction` is the direction light TRAVELS,
        // which is the negation of the vector pointing at the source —
        // light.hpp shouts about this because it is the classic sign error.
        lights_.key.direction = engine::normalised(engine::vec3{-0.4f, -0.7f, -0.6f});
        lights_.key.colour = {1.0f, 0.97f, 0.90f};
        // Lesson 6.2: `intensity` became `irradiance`, and the value became pi.
        // A light of "intensity 1" was always a light of irradiance pi; the
        // pi has moved into the BRDF where it belongs, so it must now be
        // written down here. Same picture, stated honestly.
        lights_.key.irradiance = engine::k_reference_irradiance;

        // A pose that does not depend on the clock, so the shot is reproducible.
        if (shot_path_ != nullptr) { t_ = 1.0f; }
        return true;
    }

    /// Advance the spin by exactly one simulation step. Called zero or more times
    /// per frame by the accumulator — never assume once (Lesson 1.4).
    void on_fixed_step(float h) override
    {
        if (shot_path_ == nullptr) { t_ += h; }
        if (in().key_down(SDL_SCANCODE_ESCAPE)) { request_quit(); }
    }

    /// Draw. `alpha` is where this frame sits between the last two steps; the
    /// spin is smooth enough at 60 Hz that we use the stepped value directly and
    /// say so rather than pretending to interpolate.
    void on_frame(float alpha) override
    {
        (void)alpha;
        object_.xform.rotation = engine::rotation_y(t_) * engine::rotation_x(0.5f);

        fb().clear(k_background);
        depth_.clear();

        // Six of `render_options`' fields keep their defaults, and the defaults
        // are the correct answers — so this call says only the one thing this
        // program has an opinion about.
        const engine::render_options opts{.cull = engine::cull_choice::back};
        engine::collect_triangles(triangles_, scratch_, {&object_, 1}, assets_.meshes(),
                                  {view_, k_eye}, projector_, lights_, opts);

        const engine::fill_style style{.shade = engine::shading::vertex_colour,
                                       .cull = engine::cull_mode::back,
                                       .eye = k_eye};
        engine::draw_triangles(fb(), &depth_, triangles_, false, style);

        if (shot_path_ != nullptr)
        {
            SDL_Log("hello_cube: %d triangles", static_cast<int>(triangles_.size()));
            request_quit(engine::save_ppm(fb(), shot_path_));
        }
    }

private:
    static constexpr engine::vec3 k_eye{2.6f, 1.9f, 3.4f};

    const char* shot_path_ = nullptr;
    float t_ = 0.0f;

    /// Where the cube's arrays actually live — Lesson 5.4, promoted in 5.5.
    ///
    /// One mesh in a store built for a million is not overkill, it is the point:
    /// the program no longer says anything about geometry lifetime, so there is no
    /// rule here for a later change to break. It also brings a search path this
    /// program never uses, which costs one `SDL_GetBasePath()` at construction —
    /// the price of there being exactly one answer to "where do assets live"
    /// rather than one per program.
    engine::asset_store assets_;
    engine::scene_object object_;
    engine::lighting lights_;

    const engine::mat4 view_ = engine::look_at(k_eye, {0.0f, 0.0f, 0.0f},
                                               {0.0f, 1.0f, 0.0f});
    const engine::projector projector_{
        engine::perspective(50.0f * 3.14159265f / 180.0f,
                            static_cast<float>(k_width) / static_cast<float>(k_height),
                            0.1f, 100.0f),
        engine::viewport{0.0f, 0.0f, static_cast<float>(k_width),
                         static_cast<float>(k_height), 0.0f, 1.0f},
        engine::near_mode::clip};

    engine::depth_buffer depth_{k_width, k_height};

    // The renderer's working storage, owned across frames and reused. Owning it
    // here rather than declaring it inside on_frame() is what removes the
    // per-frame allocation: clear() keeps the capacity.
    std::vector<engine::raster_triangle> triangles_;
    engine::projection_scratch scratch_;
};

}   // namespace

ENGINE_MAIN(cube_app)
