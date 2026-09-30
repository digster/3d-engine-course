// demos/particles/main.cpp — Lesson 6.18b: sixty-five thousand sparks the CPU never touches.
//
// A fountain of sparks over a floor, bouncing, cooling and blooming. Every
// particle is created, stepped, counted and drawn on the GPU: per fixed step the
// CPU sends one 144-byte uniform block, and per frame it issues ONE indirect
// draw whose instance count it never learns.
//
// Press [C] and the same fountain is simulated on the CPU instead — the lesson's
// "before" — and the HUD shows what that costs: the step, and the 3 MB that
// cross to the GPU every frame whether or not a single spark moved. The picture
// does not change, because the draw does not care who produced the pool.
//
// THIS IS ALSO THE FIRST PROGRAM IN THE COURSE WHOSE FRAME IS DECLARED. Lesson
// 6.17 built the frame graph and every program since has recorded its passes by
// hand; here the compute passes, the scene and the particles are declared as
// reads and writes, and the graph derives the order, the load and store ops —
// and, new in this lesson, each buffer's `cycle` flag. The bloom and the display
// pass stay hand-recorded, exactly as 6.13 and 6.18 left them; §11 of the lesson
// says why that line is where it is.
//
//     cmake --build build --target particles
//     ./build/demos/particles                      a window: the fountain, on the GPU
//     ./build/demos/particles --cpu                start with the CPU simulating
//     ./build/demos/particles --count 262144       a bigger pool (a power of two)
//     ./build/demos/particles --shot out.ppm       one frame, no window, 2.5 s in
//
//     [C] CPU / GPU     [B] bloom     [Space] pause the emitter     [G] dump the frame graph
//
// [G] prints through the `gpu` log category, which is silent by default (5.3):
// run with `--log gpu=info` to see it.

#include <engine/gfx/font.hpp>
#include <engine/gfx/frame_graph.hpp>
#include <engine/gfx/gpu_device.hpp>
#include <engine/gfx/gpu_mesh.hpp>
#include <engine/gfx/gpu_overlay.hpp>
#include <engine/gfx/gpu_particles.hpp>
#include <engine/gfx/gpu_post.hpp>
#include <engine/gfx/gpu_scene.hpp>
#include <engine/gfx/gpu_shader.hpp>
#include <engine/gfx/gpu_texture.hpp>
#include <engine/gfx/gpu_uniform.hpp>
#include <engine/gfx/hdr.hpp>
#include <engine/gfx/light.hpp>
#include <engine/gfx/mesh.hpp>
#include <engine/gfx/overlay.hpp>
#include <engine/gfx/particles.hpp>
#include <engine/math/mat4.hpp>
#include <engine/asset/search_path.hpp>
#include <engine/platform/app.hpp>

#include <engine/platform/main.hpp>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr int k_width = 1280;
constexpr int k_height = 720;
constexpr int k_shot_steps = 150;   ///< 2.5 s of fountain before the shot

/// The steps one frame may carry. `fixed_step` never runs more than eight a
/// frame (its spiral-of-death guard, Lesson 1.4), so neither does the graph.
constexpr int k_max_steps = 8;

// ---------------------------------------------------------------------------
//  What each declared pass is handed
// ---------------------------------------------------------------------------
//
// One context per KIND of pass and a plain function that records it — 6.17's
// shape. The contexts live in the app, so they outlive `execute`.

struct step_pass
{
    const engine::gpu_particles* p = nullptr;
    engine::step_uniforms u{};
};

struct compact_pass
{
    const engine::gpu_particles* p = nullptr;
    engine::fg_buffer state{};
};

struct scene_pass
{
    const engine::gpu_scene_renderer* renderer = nullptr;
    const engine::gpu_draw_item* items = nullptr;
    int count = 0;
    engine::camera_uniforms camera{};
    engine::scene_light_uniforms light{};
    SDL_GPUSampler* sampler = nullptr;
};

struct spark_pass
{
    const engine::gpu_particles* p = nullptr;
    engine::particle_draw_uniforms u{};
    engine::fg_buffer state{};
};

void exec_step(const engine::fg_pass_context& ctx, void* user)
{
    const auto* s = static_cast<const step_pass*>(user);
    s->p->record_step(ctx.cb, ctx.compute, s->u);
}

void exec_clear(const engine::fg_pass_context& ctx, void* user)
{
    static_cast<const engine::gpu_particles*>(user)->record_clear(ctx.compute);
}

void exec_compact(const engine::fg_pass_context& ctx, void* user)
{
    const auto* c = static_cast<const compact_pass*>(user);
    c->p->record_compact(ctx.cb, ctx.compute, ctx.buffer(c->state));
}

void exec_scene(const engine::fg_pass_context& ctx, void* user)
{
    const auto* s = static_cast<const scene_pass*>(user);
    (void)s->renderer->render(ctx.cb, ctx.pass, s->items, s->count, s->camera, s->light,
                              s->sampler);
}

void exec_sparks(const engine::fg_pass_context& ctx, void* user)
{
    const auto* s = static_cast<const spark_pass*>(user);
    s->p->record_draw(ctx.cb, ctx.pass, s->u, ctx.buffer(s->state));
}

/// A floor: two triangles, 24 m on a side, facing up. Counter-clockwise from
/// above, which is the course's front face (conventions §7).
engine::mesh_data make_floor(float half)
{
    engine::mesh_data m;
    m.vertices = {{-half, 0.0f, -half}, {-half, 0.0f, half}, {half, 0.0f, half}, {half, 0.0f, -half}};
    m.normals.assign(4, engine::vec3{0.0f, 1.0f, 0.0f});
    m.uvs = {{0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}};
    m.indices = {0, 1, 2, 0, 2, 3};
    return m;
}

class particles_app final : public engine::app
{
public:
    [[nodiscard]] engine::app_config configure(int argc, char* argv[]) override
    {
        for (int i = 1; i < argc; ++i)
        {
            if (SDL_strcmp(argv[i], "--shot") == 0 && i + 1 < argc) { shot_path_ = argv[++i]; }
            else if (SDL_strcmp(argv[i], "--cpu") == 0) { cpu_mode_ = true; }
            else if (SDL_strcmp(argv[i], "--no-bloom") == 0) { bloom_.enabled = false; }
            else if (SDL_strcmp(argv[i], "--count") == 0 && i + 1 < argc)
            {
                capacity_ = static_cast<Uint32>(SDL_strtoul(argv[++i], nullptr, 10));
            }
        }

        // A GPU program claims a bare window (Lesson 4.2); a shot claims none,
        // and creates its device without one. But not without the VIDEO
        // subsystem, which `headless` deliberately leaves out: SDL_GPU needs it
        // windowless or not, and the failure without it — "Video subsystem not
        // initialized" from SDL_CreateGPUDevice — reads like a driver problem.
        const bool shot = shot_path_ != nullptr;
        return {.title = "particles — Lesson 6.18b",
                .window_width = k_width,
                .window_height = k_height,
                .resizable = false,
                .draw_to = shot ? engine::surface::headless : engine::surface::gpu,
                .extra_subsystems = shot ? SDL_INIT_VIDEO : 0u};
    }

    [[nodiscard]] bool on_start() override
    {
        if (!gpu_.create(window(), false).ok())
        {
            SDL_Log("particles: no GPU device — this demo has no software path");
            return false;
        }

        // What the display pass writes: the swapchain's format, or — with no
        // window — an sRGB target of our own that the shot is read back from.
        display_format_ = (window() != nullptr) ? gpu_.report().swapchain_format
                                                : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
        if (window() == nullptr
            && !shot_target_.create_colour_target(gpu_, display_format_, k_width, k_height,
                                                  "shot target", false))
        {
            return false;
        }

        if (!load_shaders()) { return false; }

        const SDL_GPUTextureFormat wanted[] = {SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
                                               SDL_GPU_TEXTUREFORMAT_D24_UNORM,
                                               SDL_GPU_TEXTUREFORMAT_D16_UNORM};
        depth_format_ = engine::supported_depth_format(gpu_, wanted, 3);

        // ---- The scene: 6.2's renderer, into 6.12's float target ------------
        if (!renderer_.create(gpu_, scene_vs_.handle(), scene_fs_.handle(), depth_format_,
                              engine::k_hdr_format)
            || !post_.create(gpu_, fullscreen_vs_.handle(), tonemap_fs_.handle(),
                             bright_fs_.handle(), down_fs_.handle(), up_fs_.handle(),
                             display_format_)
            || !sampler_.create(gpu_, engine::filter::linear, engine::address_mode::repeat,
                                "scene sampler"))
        {
            SDL_Log("particles: the scene renderer or the post stack was not created");
            return false;
        }

        // ---- THE SPARKS -----------------------------------------------------
        //
        // Tested against the scene's depth and added to its light: the particle
        // pipeline shares the scene's depth format and writes the HDR format,
        // and the graph below attaches both.
        if (!particles_.create(gpu_, capacity_, particle_vs_.handle(), particle_fs_.handle(),
                               engine::k_hdr_format, depth_format_))
        {
            SDL_Log("particles: the particle system was not created");
            return false;
        }
        cpu_pool_ = engine::particle_pool(particles_.capacity());

        // The emitter: the defaults are the fountain; a pool bigger than the
        // default is filled proportionately, so --count changes the density
        // rather than leaving most of a big pool dead.
        emitter_.rate = 0.3f * static_cast<float>(particles_.capacity());

        if (!build_scene()) { return false; }
        build_hud();

        tone_.op = engine::tonemap::aces;
        tone_.exposure = engine::exposure_from_ev100(1.0f);
        bloom_.threshold = 1.2f;
        bloom_.intensity = 0.05f;

        // The shot is 2.5 seconds into the fountain. Run those steps now, outside
        // the graph — 150 compute passes would overflow its 48-pass cap, and a
        // pre-roll is not a frame.
        if (shot_path_ != nullptr) { preroll(); }
        return true;
    }

    void on_event(const SDL_Event& e) override
    {
        if (e.type != SDL_EVENT_KEY_DOWN || e.key.repeat) { return; }
        switch (e.key.key)
        {
        case SDLK_C:
            // A fresh fountain on the other processor: both pools start empty,
            // so the switch is visible as a restart rather than as a jump.
            cpu_mode_ = !cpu_mode_;
            cpu_pool_.reset();
            clock_.reset();
            zero_gpu_pool();
            break;
        case SDLK_B:     bloom_.enabled = !bloom_.enabled; break;
        case SDLK_SPACE: paused_ = !paused_; break;
        case SDLK_G:     dump_graph_ = true; break;
        default: break;
        }
    }

    // ---- The fixed step: SIMULATION ONLY, and on the GPU, deferred ----------
    //
    // Lesson 1.4's loop calls this 0 to 8 times a frame. On the CPU path the
    // step happens here. On the GPU path nothing can happen here — there is no
    // command buffer yet — so the step's uniform block is RECORDED, and the
    // frame declares one compute pass per recorded step. The emission clock
    // still advances here, once per step, so both paths spend exactly the same
    // births at exactly the same times.
    void on_fixed_step(float h) override
    {
        // A shot is EXACTLY the pre-roll's 150 steps. Without this line the
        // first frame adds however many steps its wall-clock dt happened to buy
        // — zero on one run, one on the next — and two shots of the same
        // fountain differ by a sixtieth of a second of motion.
        if (shot_path_ != nullptr) { return; }
        t_ += h;
        engine::emitter_settings e = emitter_;
        if (paused_) { e.rate = 0.0f; }

        if (cpu_mode_)
        {
            const Uint64 t0 = SDL_GetPerformanceCounter();
            (void)cpu_pool_.step(e, h);
            cpu_step_ms_ = ticks_ms(t0, SDL_GetPerformanceCounter());
            return;
        }

        if (step_count_ >= k_max_steps) { return; }
        const engine::emission_window w = clock_.advance(e.rate, h, particles_.capacity());
        steps_[step_count_++].u = engine::make_step_uniforms(e, w, h, particles_.capacity());
    }

    void on_frame(float alpha) override
    {
        const Uint64 frame_start = SDL_GetPerformanceCounter();
        SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(gpu_.handle());
        if (cb == nullptr) { return; }

        // ---- The display target ---------------------------------------------
        SDL_GPUTexture* display = shot_target_.handle();
        Uint32 dw = k_width;
        Uint32 dh = k_height;
        if (window() != nullptr)
        {
            display = nullptr;
            if (!SDL_WaitAndAcquireGPUSwapchainTexture(cb, window(), &display, &dw, &dh)
                || display == nullptr)
            {
                // Minimised, or no image this frame: submit the empty buffer and
                // keep the steps for the next frame rather than dropping them.
                (void)SDL_SubmitGPUCommandBuffer(cb);
                return;
            }
        }
        if (!post_.resize(gpu_, dw, dh, bloom_)) { (void)SDL_SubmitGPUCommandBuffer(cb); return; }

        // ---- The CPU path's one extra line --------------------------------
        SDL_GPUBuffer* state = particles_.state();
        if (cpu_mode_)
        {
            state = particles_.upload_cpu_state(gpu_, cb, cpu_pool_.particles());
            if (state == nullptr) { (void)SDL_SubmitGPUCommandBuffer(cb); return; }
        }

        declare_frame(state, (shot_path_ != nullptr) ? 1.0f : alpha, dw, dh);
        if (fg_.compile(gpu_))
        {
            if (dump_graph_) { engine::dump_frame_graph(fg_); dump_graph_ = false; }
            fg_.execute(cb);
        }
        step_count_ = 0;

        // ---- The rest of the frame, as 6.13 and 6.18 left it ----------------
        if (bloom_.enabled) { post_.render_bloom(cb, bloom_, tone_.exposure); }

        update_hud(dw, dh);
        const bool hud = font_.valid() && overlay_.upload(cb, hud_batch_);

        SDL_GPUColorTargetInfo ct{};
        ct.texture = display;
        ct.load_op = SDL_GPU_LOADOP_DONT_CARE;   // the resolve writes every pixel
        ct.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cb, &ct, 1, nullptr);
        const bool encodes = !engine::is_srgb_format(display_format_);
        post_.resolve_into(cb, pass, tone_, encodes, bloom_);
        if (hud) { overlay_.record(cb, pass, hud_batch_, encodes); }
        SDL_EndGPURenderPass(pass);

        if (shot_path_ != nullptr)
        {
            write_shot(cb);
            request_quit();
            return;
        }

        (void)SDL_SubmitGPUCommandBuffer(cb);
        frame_ms_ = 0.9f * frame_ms_ + 0.1f * ticks_ms(last_frame_, frame_start);
        last_frame_ = frame_start;
    }

    void on_stop() override
    {
        if (gpu_.valid()) { SDL_WaitForGPUIdle(gpu_.handle()); }
        fg_.destroy();
        overlay_.destroy();
        particles_.destroy();
        floor_.destroy();
        torus_.destroy();
        sampler_.destroy();
        post_.destroy();
        renderer_.destroy();
        shot_target_.destroy();
        gpu_.destroy();
    }

private:
    [[nodiscard]] static float ticks_ms(Uint64 a, Uint64 b)
    {
        return static_cast<float>(1000.0 * static_cast<double>(b - a)
                                  / static_cast<double>(SDL_GetPerformanceFrequency()));
    }

    [[nodiscard]] bool load_shaders()
    {
        using engine::shader_stage;
        const bool ok =
            scene_vs_.load(gpu_, "scene.vert", shader_stage::vertex)
            && scene_fs_.load(gpu_, "scene.frag", shader_stage::fragment)
            && particle_vs_.load(gpu_, "particle.vert", shader_stage::vertex)
            && particle_fs_.load(gpu_, "particle.frag", shader_stage::fragment)
            && fullscreen_vs_.load(gpu_, "fullscreen.vert", shader_stage::vertex)
            && tonemap_fs_.load(gpu_, "tonemap.frag", shader_stage::fragment)
            && bright_fs_.load(gpu_, "bloom_bright.frag", shader_stage::fragment)
            && down_fs_.load(gpu_, "bloom_down.frag", shader_stage::fragment)
            && up_fs_.load(gpu_, "bloom_up.frag", shader_stage::fragment)
            && overlay_vs_.load(gpu_, "overlay.vert", shader_stage::vertex)
            && overlay_fs_.load(gpu_, "overlay.frag", shader_stage::fragment);
        if (!ok) { SDL_Log("particles: a shader did not load — did the build compile them?"); }
        return ok;
    }

    [[nodiscard]] bool build_scene()
    {
        SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(gpu_.handle());
        if (cb == nullptr) { return false; }

        const engine::mesh_data floor = make_floor(12.0f);
        const engine::mesh_data torus = engine::make_torus(48, 24, 1.1f, 0.18f);
        const bool ok = floor_.create(gpu_, cb, floor.view(), engine::index_mode::indexed, "floor")
                     && torus_.create(gpu_, cb, torus.view(), engine::index_mode::indexed, "torus");
        if (!SDL_SubmitGPUCommandBuffer(cb) || !ok) { return false; }

        // A dark floor, so the sparks are the light in the room, and a torus
        // standing on edge beside the fountain, so some sparks fall BEHIND it —
        // the depth test, visible. Sparks do not collide with it: the ground
        // plane is the only thing a particle knows about (Exercise 3 changes that).
        engine::gpu_draw_item f{};
        f.mesh = &floor_;
        f.material.albedo = engine::vec3{0.10f, 0.10f, 0.11f};
        f.material.roughness = 0.55f;
        f.material.f0 = 0.04f;
        f.material.alpha = 1.0f;
        items_[0] = f;

        engine::gpu_draw_item t{};
        t.mesh = &torus_;
        const engine::mat3 stand{engine::vec3{1.0f, 0.0f, 0.0f}, engine::vec3{0.0f, 0.0f, 1.0f},
                                 engine::vec3{0.0f, -1.0f, 0.0f}};
        t.world_from_model = engine::affine(stand, engine::vec3{1.9f, 1.28f, -0.8f});
        t.normal_from_model = stand;
        t.material.albedo = engine::vec3{0.55f, 0.52f, 0.48f};
        t.material.roughness = 0.35f;
        t.material.metallic = 1.0f;
        t.material.f0 = 0.04f;
        t.material.alpha = 1.0f;
        items_[1] = t;
        return true;
    }

    void build_hud()
    {
        const std::string path =
            engine::search_path::beside_executable("assets").resolve("fonts/Karla-Regular.ttf").path;
        engine::font_bake_options opts{};
        opts.pixel_height = 18.0f;
        if (path.empty() || engine::load_font(path.c_str(), opts, font_) != engine::font_status::ok)
        {
            SDL_Log("particles: no font — drawing without a HUD");
            return;
        }
        SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(gpu_.handle());
        const bool ok = overlay_.create(gpu_, overlay_vs_.handle(), overlay_fs_.handle(),
                                        display_format_, 1024)
                     && overlay_.set_font(gpu_, cb, font_);
        (void)SDL_SubmitGPUCommandBuffer(cb);
        if (!ok) { font_ = engine::font_atlas{}; }
    }

    /// The camera: a slow orbit about the fountain, fixed for the shot.
    [[nodiscard]] engine::vec3 eye() const
    {
        const float a = (shot_path_ != nullptr) ? 0.55f : 0.55f + 0.08f * t_;
        return engine::vec3{7.2f * std::sin(a), 2.3f, 7.2f * std::cos(a)};
    }

    /// THE FRAME, DECLARED. Read it as a list of facts: what each pass reads and
    /// writes. Nothing here says what runs first, which load op anything gets,
    /// or which buffer a compute pass may cycle.
    void declare_frame(SDL_GPUBuffer* state_buffer, float alpha, Uint32 w, Uint32 h)
    {
        fg_.reset();

        // ---- Resources ------------------------------------------------------
        const engine::fg_buffer state0 =
            fg_.import_buffer("particle state", state_buffer, particles_.state_bytes());
        const engine::fg_buffer alive0 =
            fg_.import_buffer("alive list", particles_.alive(), particles_.capacity() * 4u);
        const engine::fg_buffer args0 = fg_.import_buffer("draw args", particles_.args(), 16u);

        engine::fg_texture_desc hdr_desc{};
        hdr_desc.width = w;
        hdr_desc.height = h;
        hdr_desc.format = engine::k_hdr_format;
        const engine::fg_texture hdr0 = fg_.import("hdr", post_.scene_target(), hdr_desc);

        // The depth is the graph's own: nothing outside the frame reads it, so it
        // is a transient, and the graph derives DONT_CARE for its final store.
        engine::fg_texture_desc depth_desc = hdr_desc;
        depth_desc.format = depth_format_;
        depth_desc.depth = true;
        depth_desc.sampled = false;
        const engine::fg_texture depth0 = fg_.create("depth", depth_desc);

        // ---- The steps: one compute pass each, READ-MODIFY-WRITE ------------
        engine::fg_buffer state = state0;
        static const char* names[k_max_steps] = {"step 0", "step 1", "step 2", "step 3",
                                                  "step 4", "step 5", "step 6", "step 7"};
        for (int k = 0; k < step_count_; ++k)
        {
            steps_[k].p = &particles_;
            const int pass = fg_.add_compute_pass(names[k], &exec_step, &steps_[k]);
            state = fg_.keep_buffer(pass, state, 0);   // -> cycle = false
        }

        // ---- Count the living: a reset, then the compaction -----------------
        const int clear = fg_.add_compute_pass("clear args", &exec_clear, &particles_);
        const engine::fg_buffer args1 = fg_.write_buffer(clear, args0, 0);   // -> cycle = true

        compact_.p = &particles_;
        compact_.state = state;
        const int compact = fg_.add_compute_pass("compact", &exec_compact, &compact_);
        fg_.read_buffer(compact, state);
        const engine::fg_buffer args2 = fg_.keep_buffer(compact, args1, 0);   // accumulated
        const engine::fg_buffer alive1 = fg_.write_buffer(compact, alive0, 1);

        // ---- The scene -------------------------------------------------------
        const engine::vec3 e = eye();
        const engine::vec3 target{0.0f, 1.3f, 0.0f};
        const engine::mat4 view = engine::look_at(e, target, engine::vec3{0.0f, 1.0f, 0.0f});
        const engine::mat4 proj = engine::perspective(0.87f, static_cast<float>(w) / static_cast<float>(h),
                                                      0.1f, 60.0f);
        scene_.renderer = &renderer_;
        scene_.items = items_;
        scene_.count = 2;
        scene_.camera = engine::camera_uniforms{proj * view};
        scene_.light = engine::scene_light_uniforms{};
        scene_.light.to_light = engine::normalised(engine::vec3{0.4f, 1.0f, 0.3f});
        scene_.light.key = engine::vec3{0.9f, 0.95f, 1.1f};     // a cool moon
        scene_.light.ambient = engine::vec3{0.015f, 0.016f, 0.02f};
        scene_.light.eye_world = e;
        scene_.sampler = sampler_.handle();

        const int scene = fg_.add_pass("scene", &exec_scene, &scene_);
        const engine::fg_texture hdr1 = fg_.clear(scene, hdr0, SDL_FColor{0.004f, 0.005f, 0.008f, 1.0f});
        const engine::fg_texture depth1 = fg_.clear_depth(scene, depth0, 1.0f);

        // ---- The sparks: over the scene, tested against its depth -----------
        sparks_.p = &particles_;
        sparks_.state = state;
        sparks_.u.clip_from_world = proj * view;
        sparks_.u.right = engine::camera_right(view);
        sparks_.u.up = engine::camera_up(view);
        sparks_.u.size = 0.028f;
        sparks_.u.alpha = alpha;
        sparks_.u.intensity = 3.0f;
        sparks_.u.seed = 0x51ED270Bu;

        const int sparks = fg_.add_pass("sparks", &exec_sparks, &sparks_);
        fg_.read_buffer(sparks, state);
        fg_.read_buffer(sparks, alive1);
        fg_.read_buffer(sparks, args2);
        (void)fg_.keep(sparks, hdr1);           // additive: the scene is an operand
        (void)fg_.keep_depth(sparks, depth1);   // tested, never written
    }

    void update_hud(Uint32 w, Uint32 h)
    {
        if (!font_.valid()) { return; }
        hud_batch_.begin(static_cast<int>(w), static_cast<int>(h));

        char line1[160];
        char line2[160];
        const double mb = static_cast<double>(particles_.state_bytes()) / (1024.0 * 1024.0);
        std::snprintf(line1, sizeof(line1), "%u sparks, simulated on the %s  [C]   frame %.2f ms",
                      particles_.capacity(), cpu_mode_ ? "CPU" : "GPU",
                      static_cast<double>(frame_ms_));
        if (cpu_mode_)
        {
            std::snprintf(line2, sizeof(line2),
                          "CPU step %.3f ms, %u alive, %.2f MB uploaded every frame",
                          static_cast<double>(cpu_step_ms_), cpu_pool_.alive_count(), mb);
        }
        else
        {
            std::snprintf(line2, sizeof(line2),
                          "144 B of uniforms a step; the count of the living never leaves the GPU");
        }

        const float pad = 8.0f;
        const float lh = font_.line_height();
        hud_batch_.rect(font_, 10.0f, 10.0f, 10.0f + 640.0f, 10.0f + 2.0f * lh + 2.0f * pad,
                        engine::pack_argb(10, 12, 18, 200));
        (void)hud_batch_.text_top_left(font_, line1, engine::vec2{10.0f + pad, 10.0f + pad},
                                       engine::pack_argb(236, 240, 248));
        (void)hud_batch_.text_top_left(font_, line2, engine::vec2{10.0f + pad, 10.0f + pad + lh},
                                       engine::pack_argb(250, 196, 120));
    }

    void zero_gpu_pool()
    {
        SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(gpu_.handle());
        if (cb == nullptr) { return; }
        const std::vector<engine::particle> zero(particles_.capacity());
        (void)particles_.set_state(cb, zero);
        (void)SDL_SubmitGPUCommandBuffer(cb);
    }

    void preroll()
    {
        const float h = 1.0f / 60.0f;
        t_ = static_cast<float>(k_shot_steps) * h;
        if (cpu_mode_)
        {
            for (int i = 0; i < k_shot_steps; ++i) { (void)cpu_pool_.step(emitter_, h); }
            return;
        }
        std::vector<engine::step_uniforms> steps;
        for (int i = 0; i < k_shot_steps; ++i)
        {
            const engine::emission_window w = clock_.advance(emitter_.rate, h, particles_.capacity());
            steps.push_back(engine::make_step_uniforms(emitter_, w, h, particles_.capacity()));
        }
        SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(gpu_.handle());
        particles_.simulate(cb, steps);
        (void)SDL_SubmitGPUCommandBuffer(cb);
    }

    /// Read the display target back and write it as a binary PPM.
    void write_shot(SDL_GPUCommandBuffer* cb)
    {
        SDL_GPUTransferBufferCreateInfo tb{};
        tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
        tb.size = static_cast<Uint32>(k_width * k_height * 4);
        SDL_GPUTransferBuffer* t = SDL_CreateGPUTransferBuffer(gpu_.handle(), &tb);

        SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cb);
        SDL_GPUTextureRegion src{};
        src.texture = shot_target_.handle();
        src.w = k_width;
        src.h = k_height;
        src.d = 1;
        SDL_GPUTextureTransferInfo dst{};
        dst.transfer_buffer = t;
        dst.pixels_per_row = k_width;
        dst.rows_per_layer = k_height;
        SDL_DownloadFromGPUTexture(copy, &src, &dst);
        SDL_EndGPUCopyPass(copy);

        SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cb);
        if (fence != nullptr)
        {
            SDL_WaitForGPUFences(gpu_.handle(), true, &fence, 1);
            SDL_ReleaseGPUFence(gpu_.handle(), fence);
        }

        const Uint8* px = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(gpu_.handle(), t, false));
        if (FILE* f = std::fopen(shot_path_, "wb"); f != nullptr && px != nullptr)
        {
            std::fprintf(f, "P6\n%d %d\n255\n", k_width, k_height);
            for (int i = 0; i < k_width * k_height; ++i) { std::fwrite(px + 4 * i, 1, 3, f); }
            std::fclose(f);
            SDL_Log("particles: wrote %s (%d x %d, %u sparks, %s)", shot_path_, k_width, k_height,
                    particles_.capacity(), cpu_mode_ ? "CPU" : "GPU");
        }
        if (px != nullptr) { SDL_UnmapGPUTransferBuffer(gpu_.handle(), t); }
        SDL_ReleaseGPUTransferBuffer(gpu_.handle(), t);
    }

    // ---- Options -------------------------------------------------------------
    const char* shot_path_ = nullptr;
    bool cpu_mode_ = false;
    bool paused_ = false;
    bool dump_graph_ = false;
    Uint32 capacity_ = 65536;

    // ---- The device and what draws with it ------------------------------------
    engine::gpu_device gpu_;
    SDL_GPUTextureFormat display_format_ = SDL_GPU_TEXTUREFORMAT_INVALID;
    SDL_GPUTextureFormat depth_format_ = SDL_GPU_TEXTUREFORMAT_INVALID;
    engine::gpu_texture shot_target_;
    engine::gpu_shader scene_vs_, scene_fs_, particle_vs_, particle_fs_;
    engine::gpu_shader fullscreen_vs_, tonemap_fs_, bright_fs_, down_fs_, up_fs_;
    engine::gpu_shader overlay_vs_, overlay_fs_;
    engine::gpu_scene_renderer renderer_;
    engine::gpu_post_stack post_;
    engine::gpu_sampler sampler_;
    engine::gpu_mesh floor_;
    engine::gpu_mesh torus_;
    engine::gpu_draw_item items_[2]{};

    // ---- The particles --------------------------------------------------------
    engine::gpu_particles particles_;
    engine::particle_pool cpu_pool_{64};
    engine::emitter_settings emitter_{};
    engine::emission_clock clock_;
    step_pass steps_[k_max_steps]{};
    int step_count_ = 0;

    // ---- The frame --------------------------------------------------------------
    engine::frame_graph fg_;
    compact_pass compact_{};
    scene_pass scene_{};
    spark_pass sparks_{};
    engine::tonemap_settings tone_{};
    engine::bloom_settings bloom_{.enabled = true};

    engine::font_atlas font_;
    engine::gpu_overlay overlay_;
    engine::overlay_batch hud_batch_;

    float t_ = 0.0f;
    float frame_ms_ = 16.0f;
    float cpu_step_ms_ = 0.0f;
    Uint64 last_frame_ = 0;
};

} // namespace

ENGINE_MAIN(particles_app)
