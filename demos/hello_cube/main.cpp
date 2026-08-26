// demos/hello_cube/main.cpp — the smallest program that proves the boundary.
//
// A lit cube, spinning, drawn by the engine's software renderer. A hundred and
// sixty lines of code, and every single symbol in them comes from a header under
// <engine/...>. There is no include of the engine's internals here, and there
// COULD not be: this target's include path contains engine/include and nothing
// else, so `#include "gfx/raster.hpp"` — the spelling every file in this
// repository used until Lesson 5.1 — does not resolve.
//
// That is the acceptance test for the whole lesson, and it is worth being precise
// about what it tests. Not "is the API nice". Not "did the refactor compile". It
// tests whether a program written from OUTSIDE the library can make a picture,
// which is the only question that distinguishes a library from a directory.
//
// What this file does NOT do is as interesting as what it does. It never mentions
// a triangle, an edge function, a barycentric weight or a perspective divide. All
// of that is behind `collect_triangles` and `draw_triangles` now — which is what
// the engine is FOR, and which is why the sandbox demo (whose whole job is to
// show you the insides) is a bad advertisement for the API and this file is a
// good one.
//
//     cmake --build build --target hello_cube
//     ./build/demos/hello_cube                       a window, spinning
//     ./build/demos/hello_cube --shot cube.ppm       one frame, no window

#include <engine/core/clock.hpp>
#include <engine/core/input.hpp>
#include <engine/gfx/depth_buffer.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/framebuffer.hpp>
#include <engine/gfx/light.hpp>
#include <engine/gfx/mesh.hpp>
#include <engine/gfx/projector.hpp>
#include <engine/gfx/raster.hpp>
#include <engine/gfx/scene.hpp>
#include <engine/gfx/soft_renderer.hpp>
#include <engine/gfx/viewport.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/transform.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cmath>
#include <cstring>
#include <vector>

namespace {

constexpr int k_width = 320;
constexpr int k_height = 180;

/// Copy the framebuffer into a streaming texture. Lesson 1.5's presentation step,
/// which is SDL's business and not the engine's — note that nothing in
/// <engine/...> knows this window exists.
void present(SDL_Texture* texture, const engine::framebuffer& fb)
{
    void* dst_pixels = nullptr;
    int dst_pitch = 0;
    if (!SDL_LockTexture(texture, nullptr, &dst_pixels, &dst_pitch)) { return; }

    Uint8* const dst = static_cast<Uint8*>(dst_pixels);
    for (int y = 0; y < fb.height(); ++y)
    {
        std::memcpy(dst + static_cast<std::size_t>(y) * static_cast<std::size_t>(dst_pitch),
                    fb.row(y), static_cast<std::size_t>(fb.pitch()));
    }
    SDL_UnlockTexture(texture);
}

}   // namespace

int main(int argc, char* argv[])
{
    // One frame, written to a file, and exit. Eight lines, and the same argument
    // Lesson 4.9 made for `--trace`: a program that can only be checked by a human
    // looking at a window cannot be checked by a build server, and the flag that
    // fixes that is usually more useful than the thing it was added to test.
    const char* shot_path = nullptr;
    for (int i = 1; i < argc; ++i)
    {
        if (SDL_strcmp(argv[i], "--shot") == 0 && i + 1 < argc) { shot_path = argv[++i]; }
    }

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = (shot_path != nullptr)
        ? nullptr
        : SDL_CreateWindow("hello_cube — built on engine::engine",
                           1280, 720, SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = (window != nullptr) ? SDL_CreateRenderer(window, nullptr) : nullptr;
    SDL_Texture* screen = (renderer != nullptr)
        ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                            SDL_TEXTUREACCESS_STREAMING, k_width, k_height)
        : nullptr;
    if (screen == nullptr && shot_path == nullptr)
    {
        SDL_Log("startup failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    if (screen != nullptr) { SDL_SetTextureScaleMode(screen, SDL_SCALEMODE_NEAREST); }

    // ---- What we are drawing ----------------------------------------------
    //
    // `with_normals` is the import step: a cube's eight positions become
    // twenty-four vertices, because a corner where three faces meet needs three
    // different normals and a vertex carries exactly one (Lesson 3.5). The
    // `mesh_data` owns those arrays; `view()` hands out a non-owning `mesh` that
    // borrows them, so the data must outlive every frame that uses it — which it
    // does, being a local of main().
    const engine::mesh_data cube = engine::with_normals(engine::cube_mesh(),
                                                        engine::normal_style::flat);

    engine::scene_object object;
    object.geometry = cube.view();
    object.name = "cube";
    object.tint = 0xFFE0A83Cu;             // amber; not an axis colour
    object.surface = {.colour = {0.6f, 0.6f, 0.6f}, .shininess = 48.0f};
    object.xform.position = {0.0f, 0.0f, 0.0f};
    object.xform.scale = {1.0f, 1.0f, 1.0f};

    // One directional light. `direction` is the direction light TRAVELS, which is
    // the negation of the vector pointing at the source — light.hpp shouts about
    // this because it is the classic sign error.
    engine::lighting lights;
    lights.key.direction = engine::normalised(engine::vec3{-0.4f, -0.7f, -0.6f});
    lights.key.colour = {1.0f, 0.97f, 0.90f};
    lights.key.intensity = 1.0f;

    // ---- How we are looking at it ------------------------------------------
    const engine::viewport vp{0.0f, 0.0f, static_cast<float>(k_width),
                              static_cast<float>(k_height), 0.0f, 1.0f};
    const engine::projector pr{
        engine::perspective(50.0f * 3.14159265f / 180.0f,
                            static_cast<float>(k_width) / static_cast<float>(k_height),
                            0.1f, 100.0f),
        vp, engine::near_mode::clip};
    const engine::vec3 eye{2.6f, 1.9f, 3.4f};
    const engine::mat4 view = engine::look_at(eye, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});

    engine::framebuffer fb(k_width, k_height);
    engine::depth_buffer depth(k_width, k_height);
    engine::clock clk;
    engine::input in;

    // The renderer's working storage, owned by the caller and reused. Owning it
    // ACROSS frames rather than declaring it inside the loop is what removes the
    // per-frame allocation: clear() keeps the capacity.
    std::vector<engine::raster_triangle> triangles;
    engine::projection_scratch scratch;

    float t = (shot_path != nullptr) ? 1.0f : 0.0f;   // a fixed pose for the shot
    bool running = true;
    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT) { running = false; }
            in.feed_event(event);
        }
        in.update();   // edges are computed once a frame, after the drain (Lesson 1.2)
        if (in.key_pressed(SDL_SCANCODE_ESCAPE)) { running = false; }

        clk.tick();
        if (shot_path == nullptr) { t += clk.dt(); }
        object.xform.rotation = engine::rotation_y(t) * engine::rotation_x(0.5f);

        fb.clear(engine::pack_argb(12, 14, 20));
        depth.clear();

        // Six of `render_options`' seven fields keep their defaults, and the
        // defaults are the correct answers — so this call says only the one thing
        // this program has an opinion about.
        const engine::render_options opts{.cull = engine::cull_choice::back};
        engine::collect_triangles(triangles, scratch, {&object, 1}, {view, eye},
                                  pr, lights, opts);

        const engine::fill_style style{.shade = engine::shading::vertex_colour,
                                       .cull = engine::cull_mode::back,
                                       .eye = eye};
        engine::draw_triangles(fb, &depth, triangles, false, style);

        if (shot_path != nullptr)
        {
            int painted = 0;
            for (int y = 0; y < fb.height(); ++y)
            {
                for (int x = 0; x < fb.width(); ++x)
                {
                    if (fb.pixel_at(x, y) != engine::pack_argb(12, 14, 20)) { ++painted; }
                }
            }
            SDL_Log("hello_cube: %d triangles, %d pixels painted",
                    static_cast<int>(triangles.size()), painted);
            SDL_IOStream* io = SDL_IOFromFile(shot_path, "wb");
            if (io == nullptr) { SDL_Log("cannot write %s: %s", shot_path, SDL_GetError()); return 1; }
            char header[32];
            const int n = SDL_snprintf(header, sizeof(header), "P6\n%d %d\n255\n",
                                       fb.width(), fb.height());
            SDL_WriteIO(io, header, static_cast<std::size_t>(n));
            for (int y = 0; y < fb.height(); ++y)
            {
                for (int x = 0; x < fb.width(); ++x)
                {
                    const Uint32 px = fb.pixel_at(x, y);
                    const Uint8 rgb[3] = {static_cast<Uint8>((px >> 16) & 0xFFu),
                                          static_cast<Uint8>((px >> 8) & 0xFFu),
                                          static_cast<Uint8>(px & 0xFFu)};
                    SDL_WriteIO(io, rgb, sizeof(rgb));
                }
            }
            SDL_CloseIO(io);
            SDL_Quit();
            return 0;
        }

        present(screen, fb);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, screen, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(screen);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
