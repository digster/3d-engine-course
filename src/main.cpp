// src/main.cpp — the engine's entry point.
//
// The loop is the one settled in Lesson 1.4 and does not change again:
//
//     drain events -> tick clock -> update input -> N fixed steps -> render
//
// What changed in 1.5 is the last word. Rendering no longer means asking SDL to
// draw shapes; it means writing pixels into memory we own, then handing that
// block to the GPU as a texture. Everything visible below — the gradient, the
// boxes, the ball, the trail — is bytes this program wrote by hand.

#include "core/clock.hpp"
#include "core/fixed_step.hpp"
#include "core/input.hpp"
#include "gfx/framebuffer.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>   // Provides the cross-platform entry point. NOTE:
                             // <SDL3/SDL.h> deliberately does NOT include this,
                             // so we include it explicitly, exactly once, here.

#include <array>
#include <cstring>

namespace {

// ---- The framebuffer's size ------------------------------------------------
// Deliberately small. At 320x180 the window's 1280x720 is exactly 4x, so every
// pixel we write becomes a visible 4x4 block — which is the point of a lesson
// about individual pixels. It is also a real technique: rendering at a fixed
// internal resolution and scaling to the window is how a great many games ship,
// and it means a window resize needs no code at all.
constexpr int k_fb_width = 320;
constexpr int k_fb_height = 180;

// ---- Demo constants, now in framebuffer pixels ------------------------------
constexpr float k_pixels_per_second = 90.0f;
constexpr float k_box_size = 10.0f;
constexpr float k_track_var_y = 24.0f;
constexpr float k_track_raw_y = 40.0f;
constexpr float k_track_lerp_y = 56.0f;

constexpr float k_gravity = 300.0f;
constexpr float k_ball_size = 6.0f;
constexpr float k_ball_start_y = 84.0f;
constexpr float k_floor_y = 168.0f;

constexpr Uint32 k_throttle_ms = 50;

/// How many past ball positions to draw as a trail. One entry per simulation
/// step, so the spacing between dots is literally the fixed timestep made
/// visible — closer together at 120 Hz, further apart at 10 Hz.
constexpr std::size_t k_trail_length = 150;

/// Everything the simulation owns (Lesson 1.4). Two of these exist so the
/// renderer can interpolate between them.
struct sim_state
{
    float box_x = 0.0f;
    float box_dir = 1.0f;
    float ball_y = k_ball_start_y;
    float ball_vy = 0.0f;
};

void step_simulation(sim_state& s, float h)
{
    s.box_x += k_pixels_per_second * s.box_dir * h;
    const float max_x = static_cast<float>(k_fb_width) - k_box_size;
    if (s.box_x > max_x) { s.box_x = max_x; s.box_dir = -1.0f; }
    if (s.box_x < 0.0f)  { s.box_x = 0.0f;  s.box_dir =  1.0f; }

    s.ball_y += s.ball_vy * h;
    s.ball_vy += k_gravity * h;

    if (s.ball_y + k_ball_size > k_floor_y)
    {
        s.ball_y = k_floor_y - k_ball_size;
        s.ball_vy = -s.ball_vy;
    }
}

[[nodiscard]] float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

// ---- The two ways to fill the background -----------------------------------
// Both produce the identical image. The difference is entirely in how the
// pixels are addressed, which is the measurement Lesson 1.5 §3.5 is about.

/// The safe way: one bounds-checked call per pixel.
void draw_gradient_safe(engine::framebuffer& fb)
{
    for (int y = 0; y < fb.height(); ++y)
    {
        for (int x = 0; x < fb.width(); ++x)
        {
            const Uint8 r = static_cast<Uint8>(x >> 2);
            const Uint8 g = static_cast<Uint8>(y >> 2);
            fb.put_pixel(x, y, engine::pack_argb(r, g, 60));
        }
    }
}

/// The fast way: fetch the row pointer once, then walk it.
///
/// Same arithmetic, same output. What disappears is per-pixel work that the
/// loop already knows the answer to — the bounds check (x and y are loop
/// counters bounded by the buffer's own size) and the `y * width` multiply
/// (constant for the whole row).
void draw_gradient_fast(engine::framebuffer& fb)
{
    for (int y = 0; y < fb.height(); ++y)
    {
        Uint32* const line = fb.row(y);
        const Uint8 g = static_cast<Uint8>(y >> 2);
        for (int x = 0; x < fb.width(); ++x)
        {
            const Uint8 r = static_cast<Uint8>(x >> 2);
            line[x] = engine::pack_argb(r, g, 60);
        }
    }
}

/// Copy the framebuffer into a streaming texture, one row at a time.
///
/// Row by row, rather than one memcpy of the whole thing, because the pitch SDL
/// hands back is the GPU's, not ours: drivers routinely align each row to 64 or
/// 256 bytes, leaving padding at the end that is not part of the image. Assume
/// the pitches match and the picture shears diagonally on exactly the machines
/// where they do not. Lesson 1.5 §4.3.
[[nodiscard]] bool upload(SDL_Texture* texture, const engine::framebuffer& fb)
{
    void* dst_pixels = nullptr;
    int dst_pitch = 0;

    // Locking a streaming texture gives write-only access to driver memory whose
    // previous contents are undefined — SDL's header is explicit that if you
    // need to keep the image, you keep it yourself. We do: `fb` is the master
    // copy and this is only ever a one-way push.
    if (!SDL_LockTexture(texture, nullptr, &dst_pixels, &dst_pitch))
    {
        return false;
    }

    Uint8* const dst = static_cast<Uint8*>(dst_pixels);
    const int row_bytes = fb.pitch();

    for (int y = 0; y < fb.height(); ++y)
    {
        std::memcpy(dst + static_cast<std::size_t>(y) * static_cast<std::size_t>(dst_pitch),
                    fb.row(y),
                    static_cast<std::size_t>(row_bytes));
    }

    SDL_UnlockTexture(texture);
    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    // ---- 1. Initialise ------------------------------------------------------
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    const int sdl_version = SDL_GetVersion();
    SDL_Log("Engine starting — SDL %d.%d.%d",
            SDL_VERSIONNUM_MAJOR(sdl_version),
            SDL_VERSIONNUM_MINOR(sdl_version),
            SDL_VERSIONNUM_MICRO(sdl_version));

    // ---- 2. Window and renderer --------------------------------------------
    SDL_Window* window = SDL_CreateWindow("Engine", 1280, 720, SDL_WINDOW_RESIZABLE);
    if (window == nullptr)
    {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr)
    {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    int vsync = SDL_RENDERER_VSYNC_DISABLED;
    SDL_SetRenderVSync(renderer, vsync);

    // ---- 3. The framebuffer, and the texture that shows it -----------------
    engine::framebuffer fb(k_fb_width, k_fb_height);

    // SDL_PIXELFORMAT_ARGB8888 matches pack_argb exactly: a 32-bit integer with
    // alpha in the most significant bits. STREAMING because we rewrite the whole
    // thing every frame and want to lock it rather than pay for an upload path
    // meant for images that rarely change.
    SDL_Texture* screen_texture = SDL_CreateTexture(renderer,
                                                    SDL_PIXELFORMAT_ARGB8888,
                                                    SDL_TEXTUREACCESS_STREAMING,
                                                    k_fb_width, k_fb_height);
    if (screen_texture == nullptr)
    {
        SDL_Log("SDL_CreateTexture failed: %s", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Nearest-neighbour, so a 4x upscale produces crisp 4x4 blocks rather than a
    // blurred interpolation of them. For a lesson about individual pixels,
    // anything else would hide the subject.
    SDL_SetTextureScaleMode(screen_texture, SDL_SCALEMODE_NEAREST);

    // ---- 4. Subsystems and demo state --------------------------------------
    engine::clock clk;
    engine::input in;
    engine::fixed_step stepper(60.0f);

    sim_state previous;
    sim_state current;

    float var_box_x = 0.0f;
    float var_box_dir = 1.0f;

    // The ball's recent positions, as a ring buffer. Written once per simulation
    // step, so the trail records the simulation's path rather than the renderer's.
    std::array<int, k_trail_length> trail_y{};
    std::size_t trail_head = 0;
    std::size_t trail_count = 0;

    bool fast_gradient = true;
    Uint64 gradient_ns = 0;      // smoothed, for the readout

    const Uint32 k_var_colour = engine::pack_argb(236, 122, 92);
    const Uint32 k_raw_colour = engine::pack_argb(226, 196, 110);
    const Uint32 k_lerp_colour = engine::pack_argb(122, 196, 152);
    const Uint32 k_ball_colour = engine::pack_argb(126, 162, 236);
    const Uint32 k_trail_colour = engine::pack_argb(70, 90, 130);
    const Uint32 k_floor_colour = engine::pack_argb(90, 90, 110);

    SDL_Log("Controls: [G] gradient path · [1-4] sim rate · [V] vsync · [T] throttle · [R] reset · [Esc] quit");

    // ---- 5. The loop --------------------------------------------------------
    bool running = true;
    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            in.feed_event(event);

            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                SDL_Log("Quit requested");
                running = false;
                break;

            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                SDL_Log("Window %u close requested", static_cast<unsigned>(event.window.windowID));
                running = false;
                break;

            default:
                // Window resizes need no handling at all now: the framebuffer is
                // a fixed size and SDL stretches it over whatever the window
                // currently is.
                break;
            }
        }

        clk.tick();
        in.update();

        // --- 5a. Controls ----------------------------------------------------
        if (in.key_pressed(SDL_SCANCODE_ESCAPE)) { running = false; }
        if (in.key_pressed(SDL_SCANCODE_G)) { fast_gradient = !fast_gradient; }

        if (in.key_pressed(SDL_SCANCODE_1)) { stepper.set_rate(10.0f); }
        if (in.key_pressed(SDL_SCANCODE_2)) { stepper.set_rate(30.0f); }
        if (in.key_pressed(SDL_SCANCODE_3)) { stepper.set_rate(60.0f); }
        if (in.key_pressed(SDL_SCANCODE_4)) { stepper.set_rate(120.0f); }

        if (in.key_pressed(SDL_SCANCODE_V))
        {
            vsync = (vsync == SDL_RENDERER_VSYNC_DISABLED) ? 1 : SDL_RENDERER_VSYNC_DISABLED;
            if (!SDL_SetRenderVSync(renderer, vsync))
            {
                SDL_Log("SDL_SetRenderVSync(%d) failed: %s", vsync, SDL_GetError());
            }
        }

        if (in.key_pressed(SDL_SCANCODE_R))
        {
            previous = sim_state{};
            current = sim_state{};
            var_box_x = 0.0f;
            var_box_dir = 1.0f;
            trail_head = 0;
            trail_count = 0;
        }

        // --- 5b. Simulate ----------------------------------------------------
        stepper.begin_frame(clk.dt());
        while (stepper.next_step())
        {
            previous = current;
            step_simulation(current, stepper.h());

            trail_y[trail_head] = static_cast<int>(current.ball_y);
            trail_head = (trail_head + 1) % k_trail_length;
            if (trail_count < k_trail_length) { ++trail_count; }
        }

        const float alpha = stepper.alpha();

        var_box_x += k_pixels_per_second * var_box_dir * clk.dt();
        const float var_max_x = static_cast<float>(k_fb_width) - k_box_size;
        if (var_box_x > var_max_x) { var_box_x = var_max_x; var_box_dir = -1.0f; }
        if (var_box_x < 0.0f)      { var_box_x = 0.0f;      var_box_dir =  1.0f; }

        // --- 5c. Draw, one pixel at a time -----------------------------------
        // Note what is missing: there is no fb.clear() call. The gradient writes
        // every pixel in the buffer, so clearing first would be writing 57,600
        // pixels twice. Skipping a redundant clear is a real optimisation, not a
        // trick — and the moment a background stops covering the whole frame,
        // clear() has to come back or the previous frame shows through as
        // smearing. Module 2's rasterizer will need it for exactly that reason.
        //
        // The background gradient, timed. Both paths write the same image; only
        // the addressing differs.
        const Uint64 gradient_start = SDL_GetTicksNS();
        if (fast_gradient) { draw_gradient_fast(fb); }
        else               { draw_gradient_safe(fb); }
        const Uint64 gradient_end = SDL_GetTicksNS();

        // A rolling average, because a single frame's measurement is noise.
        const Uint64 sample = gradient_end - gradient_start;
        gradient_ns = (gradient_ns * 15 + sample) / 16;

        // The floor line the ball bounces on: a one-pixel-tall rectangle.
        fb.fill_rect(0, static_cast<int>(k_floor_y), k_fb_width, 1, k_floor_colour);
        fb.fill_rect(0, static_cast<int>(k_ball_start_y), k_fb_width, 1, k_floor_colour);

        // The trail: individual pixels, one per simulation step. This is
        // put_pixel earning its keep — scattered single points, where clipping
        // once would buy nothing.
        const int trail_x = k_fb_width / 2;
        for (std::size_t i = 0; i < trail_count; ++i)
        {
            fb.put_pixel(trail_x - 6, trail_y[i], k_trail_colour);
            fb.put_pixel(trail_x + 6, trail_y[i], k_trail_colour);
        }

        // The three timing boxes from Lesson 1.4, now drawn by us.
        fb.fill_rect(static_cast<int>(var_box_x), static_cast<int>(k_track_var_y),
                     static_cast<int>(k_box_size), static_cast<int>(k_box_size), k_var_colour);

        fb.fill_rect(static_cast<int>(current.box_x), static_cast<int>(k_track_raw_y),
                     static_cast<int>(k_box_size), static_cast<int>(k_box_size), k_raw_colour);

        fb.fill_rect(static_cast<int>(lerp(previous.box_x, current.box_x, alpha)),
                     static_cast<int>(k_track_lerp_y),
                     static_cast<int>(k_box_size), static_cast<int>(k_box_size), k_lerp_colour);

        fb.fill_rect(trail_x - static_cast<int>(k_ball_size) / 2,
                     static_cast<int>(lerp(previous.ball_y, current.ball_y, alpha)),
                     static_cast<int>(k_ball_size), static_cast<int>(k_ball_size), k_ball_colour);

        // --- 5d. Present ------------------------------------------------------
        if (!upload(screen_texture, fb))
        {
            SDL_Log("SDL_LockTexture failed: %s", SDL_GetError());
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // NULL destination rectangle means "the entire rendering target", so our
        // 320x180 image is stretched over the whole window at whatever size it
        // currently is.
        SDL_RenderTexture(renderer, screen_texture, nullptr, nullptr);

        // The readout is still drawn by SDL, *over* our framebuffer, because we
        // have no font of our own yet. Module 6 fixes that; until then this is
        // the one thing on screen we did not draw ourselves.
        SDL_SetRenderScale(renderer, 2.0f, 2.0f);
        SDL_SetRenderDrawColor(renderer, 225, 225, 215, 255);
        SDL_RenderDebugTextFormat(renderer, 6.0f, 6.0f,
                                  "fps %7.1f   sim %3.0f Hz   alpha %.2f   framebuffer %dx%d",
                                  static_cast<double>(clk.fps()),
                                  static_cast<double>(stepper.rate_hz()),
                                  static_cast<double>(alpha),
                                  fb.width(), fb.height());
        SDL_RenderDebugTextFormat(renderer, 6.0f, 18.0f,
                                  "gradient: %-13s %6.3f ms for %d pixels   [G] to switch",
                                  fast_gradient ? "row pointer" : "put_pixel",
                                  static_cast<double>(gradient_ns) / 1000000.0,
                                  fb.width() * fb.height());
        SDL_RenderDebugText(renderer, 6.0f, 34.0f,
                            "[1-4] sim rate   [V] vsync   [T] throttle   [R] reset   [Esc] quit");
        SDL_SetRenderScale(renderer, 1.0f, 1.0f);

        SDL_RenderPresent(renderer);

        if (in.key_down(SDL_SCANCODE_T))
        {
            SDL_Delay(k_throttle_ms);
        }
    }

    // ---- 6. Shut down, in reverse order of creation -------------------------
    SDL_DestroyTexture(screen_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
