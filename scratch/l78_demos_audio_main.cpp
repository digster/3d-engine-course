// demos/audio/main.cpp — a listener, three emitters, and a top-down view of why
// it sounds the way it does.
//
// Lesson 7.8. Every other demo in this repository can be judged by looking at
// it. This one cannot, which is the difficulty the whole lesson is about — so
// the picture's job here is not to BE the result, it is to explain the result:
// the map shows where everything is and the panel shows the two gains that fell
// out of it, and you listen while watching both.
//
//   [1] [2] [3]  toggle the three emitters
//   [W][A][S][D] walk the listener       [Q] [E]  turn
//   [L]          cycle emitter 1's falloff law
//   [Space]      fire a one-shot ping where emitter 1 is
//   [M]          mute (master gain 0)        [Esc] quit
//
//     cmake --build build --target audio
//     ./build/demos/audio
//     ./build/demos/audio --t 2.5 --shot out.ppm       headless, silent
//
// **A HEADLESS RUN OPENS NO DEVICE.** `--shot` uses `mixer::open_offline`, so a
// screenshot is deterministic, silent, and runnable on a build machine with no
// sound card — and the gains on the panel are computed by exactly the same code
// that would have been driving speakers. That is the property `open_offline`
// exists for, used by a program rather than by a test.
//
// THE RULES ARE 5.1'S. Nothing here includes an engine internal, nothing here is
// compiled into the library, and it does not link `demo_common`: a program
// written to exercise one subsystem has no business reaching for a shared scene.
// Every sound it makes it synthesises, so the repository carries no audio asset.

#include <engine/audio/mixer.hpp>
#include <engine/audio/sound.hpp>
#include <engine/audio/spatial.hpp>
#include <engine/core/log.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/framebuffer.hpp>
#include <engine/gfx/raster.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/transform.hpp>
#include <engine/math/vec3.hpp>
#include <engine/platform/app.hpp>
#include <engine/platform/main.hpp>
#include <engine/ui/debug_ui.hpp>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

using engine::quat;
using engine::transform;
using engine::vec3;
using engine::audio::amplitude_to_db;
using engine::audio::emitter;
using engine::audio::falloff;
using engine::audio::listener;
using engine::audio::listener_from;
using engine::audio::mixer;
using engine::audio::mixer_report;
using engine::audio::sound;
using engine::audio::spatial_result;
using engine::audio::spatialise;
using engine::audio::voice_id;

constexpr int k_width = 960;
constexpr int k_height = 540;
constexpr float k_pi = std::numbers::pi_v<float>;

/// This demo's own log category, in the range Lesson 5.3 reserved for programs
/// outside the library. `log_audio` belongs to the engine's mixer; a demo that
/// logged under it would be claiming to be part of the subsystem it is using.
constexpr engine::log_category log_demo =
    static_cast<engine::log_category>(engine::log_category_count);

constexpr Uint32 k_background = engine::pack_argb(16, 18, 24);
constexpr Uint32 k_grid       = engine::pack_argb(38, 42, 52);
constexpr Uint32 k_listener   = engine::pack_argb(235, 238, 245);
constexpr Uint32 k_forward    = engine::pack_argb(110, 170, 255);
constexpr Uint32 k_right_axis = engine::pack_argb(235, 96, 96);
constexpr Uint32 k_ring       = engine::pack_argb(60, 66, 80);

/// Pixels per metre. 12 puts +/-40 m across a 960-wide view, which is the right
/// framing for this particular picture: the interesting part of a falloff curve
/// is the first few metres, and the last thirty exist only to show that the
/// curve does eventually end.
constexpr float k_scale = 12.0f;

struct source
{
    const char* name = "";
    sound audio;
    emitter e;
    voice_id voice;
    bool on = true;
    Uint32 colour = 0;
    spatial_result last;
};

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

/// World (x, z) to screen. The map is the GROUND PLANE seen from above, so world
/// +x is screen right and world -z (the direction an unturned listener faces) is
/// screen UP. Getting this mapping backwards is the one way to make a correct
/// pan law look wrong, so it is written down once, here.
void to_screen(vec3 p, int& sx, int& sy)
{
    sx = k_width / 2 + static_cast<int>(std::lround(p.x * k_scale));
    sy = k_height / 2 + static_cast<int>(std::lround(p.z * k_scale));
}

void draw_grid(engine::framebuffer& f)
{
    // Every FOUR metres, not every one. At 12 px/m a one-metre grid is a line
    // every twelve pixels, which is dense enough that the downsampled figure in
    // the lesson turned into a mesh with the actual content buried in it — and
    // dense enough on screen that the distance rings, which are the informative
    // thing here, had to compete with it.
    for (int m = -28; m <= 28; m += 4)
    {
        const int x = k_width / 2 + static_cast<int>(std::lround(m * k_scale));
        const int y = k_height / 2 + static_cast<int>(std::lround(m * k_scale));
        if (x >= 0 && x < k_width) { engine::draw_line(f, x, 0, x, k_height - 1, k_grid); }
        if (y >= 0 && y < k_height) { engine::draw_line(f, 0, y, k_width - 1, y, k_grid); }
    }
}

void draw_ring(engine::framebuffer& f, vec3 centre, float radius, Uint32 colour)
{
    constexpr int k_segments = 96;
    int px = 0;
    int py = 0;
    for (int i = 0; i <= k_segments; ++i)
    {
        const float a = 2.0f * k_pi * static_cast<float>(i) / k_segments;
        const vec3 p{centre.x + radius * std::cos(a), 0.0f, centre.z + radius * std::sin(a)};
        int sx = 0;
        int sy = 0;
        to_screen(p, sx, sy);
        if (i > 0) { engine::draw_line(f, px, py, sx, sy, colour); }
        px = sx;
        py = sy;
    }
}

void draw_blob(engine::framebuffer& f, vec3 p, int r, Uint32 colour)
{
    int sx = 0;
    int sy = 0;
    to_screen(p, sx, sy);
    for (int dy = -r; dy <= r; ++dy)
    {
        const int half = static_cast<int>(std::lround(std::sqrt(
            std::max(0.0f, static_cast<float>(r * r - dy * dy)))));
        engine::draw_line(f, sx - half, sy + dy, sx + half, sy + dy, colour);
    }
}

/// A gain, drawn as a bar in DECIBELS rather than in amplitude.
///
/// Amplitude bars are useless here and it is worth saying why: the interesting
/// range of a falloff curve is 0.02 to 1.0, and an amplitude bar spends 98% of
/// its length on the first 6 dB. A dB axis from -60 to 0 gives every halving the
/// same six pixels, which is what the ear does too.
void draw_meter(engine::framebuffer& f, int x, int y, int w, int h, float gain, Uint32 colour)
{
    engine::draw_line(f, x, y, x + w, y, k_ring);
    engine::draw_line(f, x, y + h, x + w, y + h, k_ring);
    const float db = amplitude_to_db(gain);
    const float t = std::clamp((db + 60.0f) / 60.0f, 0.0f, 1.0f);
    const int filled = static_cast<int>(std::lround(t * w));
    for (int dy = 1; dy < h; ++dy)
    {
        if (filled > 0) { engine::draw_line(f, x, y + dy, x + filled, y + dy, colour); }
    }
}

const char* name_of(falloff law)
{
    switch (law)
    {
    case falloff::none: return "none";
    case falloff::inverse: return "inverse";
    case falloff::linear: return "linear";
    case falloff::inverse_ranged: return "inverse_ranged";
    }
    return "?";
}

// ---------------------------------------------------------------------------
// The app
// ---------------------------------------------------------------------------

class audio_app final : public engine::app
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
            else if (SDL_strcmp(argv[i], "--t") == 0 && i + 1 < argc)
            {
                // One named local per argument. `SDL_clamp` is a macro that
                // expands its first argument three times, so an inlined `++i`
                // advances three times and eats the next two flags — Lesson 7.3
                // §14, and the bug is worth inheriting the fix for rather than
                // rediscovering.
                const double wanted = SDL_atof(argv[++i]);
                t_ = static_cast<float>(std::max(0.0, wanted));
            }
        }

        // SDL_INIT_AUDIO is NOT requested here, and that is deliberate: the
        // mixer initialises the subsystem itself, by refcount, so a program that
        // decides at runtime whether it wants sound does not have to decide in
        // `configure()`. The hook exists (`extra_subsystems`, Lesson 5.2) and is
        // the right place for a program that always wants audio up before
        // anything else runs.
        return {.title = "audio — a listener and three emitters",
                .draw_to = (shot_path_ != nullptr) ? engine::surface::headless
                                                   : engine::surface::renderer,
                .fb_width = k_width,
                .fb_height = k_height};
    }

    [[nodiscard]] bool on_start() override
    {
        SDL_SetLogPriority(log_demo, SDL_LOG_PRIORITY_INFO);

        build_sounds();

        // Headless: no device, so nothing is timed by hardware and a `--shot` is
        // reproducible. Windowed: a real device, and the callback runs.
        const bool opened = shot_path_ != nullptr
                                ? mixer_.open_offline({.buffer_frames = 512, .max_voices = 32})
                                : mixer_.open({.buffer_frames = 512, .max_voices = 32});
        if (!opened)
        {
            // NOT a fatal error. A machine with no sound card runs this demo
            // perfectly well; it just does not make a noise, and the map and the
            // panel still show every number.
            ENGINE_LOG_INFO(log_demo, "no audio device; the picture still works");
        }

        for (source& s : sources_) { start_voice(s); }

        ENGINE_LOG_INFO(log_demo, "[1][2][3] emitters  [WASD] walk  [Q][E] turn");
        ENGINE_LOG_INFO(log_demo, "[L] falloff law  [Space] ping  [M] mute  [Esc] quit");
        return ui_.start(window(), renderer()) || shot_path_ != nullptr;
    }

    void on_event(const SDL_Event& event) override
    {
        (void)ui_.handle_event(event);
        if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) { return; }

        switch (event.key.scancode)
        {
        case SDL_SCANCODE_ESCAPE: request_quit(); break;
        case SDL_SCANCODE_1: toggle(0); break;
        case SDL_SCANCODE_2: toggle(1); break;
        case SDL_SCANCODE_3: toggle(2); break;
        case SDL_SCANCODE_L: cycle_law(); break;
        case SDL_SCANCODE_M: mute_ = !mute_; break;
        case SDL_SCANCODE_SPACE: ping(); break;
        default: break;
        }
    }

    void on_input() override { ui_.begin_frame(); }

    void on_fixed_step(float h) override
    {
        const float walk = 6.0f * h;
        const float turn = 2.0f * h;

        vec3 forward = engine::rotate(listener_rot_, vec3{0.0f, 0.0f, -1.0f});
        vec3 right = engine::rotate(listener_rot_, vec3{1.0f, 0.0f, 0.0f});

        if (in().key_down(SDL_SCANCODE_W)) { listener_pos_ += forward * walk; }
        if (in().key_down(SDL_SCANCODE_S)) { listener_pos_ -= forward * walk; }
        if (in().key_down(SDL_SCANCODE_A)) { listener_pos_ -= right * walk; }
        if (in().key_down(SDL_SCANCODE_D)) { listener_pos_ += right * walk; }
        if (in().key_down(SDL_SCANCODE_Q)) { yaw_ -= turn; }
        if (in().key_down(SDL_SCANCODE_E)) { yaw_ += turn; }

        listener_rot_ = engine::quat_from_axis_angle(vec3{0.0f, 1.0f, 0.0f}, yaw_);
        t_ += h;
    }

    void on_frame(float alpha) override
    {
        (void)alpha;

        // ---- Move the emitters, then push the gains ------------------------
        //
        // ONCE A FRAME, and the mixer ramps to each new value across the next
        // buffer. That is the whole of §6: the gain curve is SAMPLED here, at
        // 60 Hz, and RECONSTRUCTED there, at 48 kHz, and a zero-order hold
        // reconstruction is a click on every frame boundary.
        sources_[0].e.position = vec3{6.0f * std::sin(t_ * 0.6f), 0.0f,
                                      -6.0f * std::cos(t_ * 0.6f)};
        const float sweep = std::fmod(t_ * 6.0f + 30.0f, 60.0f) - 30.0f;
        sources_[1].e.position = vec3{sweep, 0.0f, -2.0f};

        const transform placement{.position = listener_pos_, .rotation = listener_rot_};
        const listener lis = listener_from(placement);

        for (source& s : sources_)
        {
            s.last = spatialise(lis, s.e);
            if (s.on && s.voice.valid())
            {
                mixer_.set_gain(s.voice, s.last.gain);
            }
        }

        mixer_.update();

        // Offline: there is no audio thread, so nothing would ever call
        // `mix_into`. Drive it here at roughly real time so that the counters on
        // the panel mean something in a headless run.
        if (shot_path_ != nullptr) { drive_offline(); }

        draw(lis);

        if (shot_path_ != nullptr && t_ >= 0.0f)
        {
            request_quit(engine::save_ppm(fb(), shot_path_));
        }
    }

    void on_overlay() override
    {
        // GUARDED: a `--shot` run has no window and therefore no ImGui context,
        // and `ImGui::Begin` on no context is a segfault rather than a no-op.
        if (ui_.running()) { build_panel(); }
        ui_.render();
    }

    void on_stop() override
    {
        // ORDER MATTERS AND IT IS NOT OBVIOUS. The mixer holds non-owning
        // pointers into `sources_[i].audio`, so every voice must stop before
        // those vectors are destroyed. `close()` does that; leaving it to the
        // mixer's own destructor would work here only because it happens to be
        // declared before `sources_`, which is a fact about this file and not a
        // rule anybody should rely on.
        mixer_.close();
        ui_.stop();
    }

private:
    void build_sounds()
    {
        sources_[0].name = "siren (orbits)";
        sources_[0].audio = engine::audio::make_tone(440.0f, 1.0f, 48000, 0.30f);
        sources_[0].colour = engine::pack_argb(255, 190, 90);
        sources_[0].e.ref_distance = 1.5f;
        sources_[0].e.max_distance = 40.0f;

        sources_[1].name = "flyby (crosses)";
        sources_[1].audio = engine::audio::make_tone(196.0f, 1.0f, 48000, 0.30f);
        sources_[1].colour = engine::pack_argb(120, 220, 160);
        sources_[1].e.ref_distance = 1.0f;
        sources_[1].e.max_distance = 50.0f;

        sources_[2].name = "music (no falloff)";
        sources_[2].audio = engine::audio::make_tone(330.0f, 1.0f, 48000, 0.15f);
        sources_[2].colour = engine::pack_argb(170, 150, 255);
        sources_[2].e.position = vec3{-9.0f, 0.0f, 7.0f};
        sources_[2].e.law = falloff::none;
        sources_[2].e.gain = 0.5f;

        // EVERY LOOPING TONE IS FADED AT BOTH ENDS, and a one-second tone at
        // 440 Hz does not end where it began: the last sample is mid-cycle, so
        // the wrap is a step and the step is a click, once a second, forever.
        // Three lines, and §6 measures what they are worth.
        for (source& s : sources_) { engine::audio::apply_fade(s.audio, 0.004f, 0.004f); }

        ping_ = engine::audio::make_tone(880.0f, 0.18f, 48000, 0.45f);
        engine::audio::apply_fade(ping_, 0.002f, 0.12f);
    }

    void start_voice(source& s)
    {
        if (!s.on) { return; }
        const transform placement{.position = listener_pos_, .rotation = listener_rot_};
        s.voice = mixer_.play_spatial(s.audio, listener_from(placement), s.e, {.loops = true});
    }

    void toggle(std::size_t i)
    {
        source& s = sources_[i];
        s.on = !s.on;
        if (s.on)
        {
            start_voice(s);
        }
        else
        {
            mixer_.stop(s.voice);
            s.voice = voice_id{};
        }
    }

    void cycle_law()
    {
        emitter& e = sources_[0].e;
        switch (e.law)
        {
        case falloff::inverse_ranged: e.law = falloff::inverse; break;
        case falloff::inverse: e.law = falloff::linear; break;
        case falloff::linear: e.law = falloff::none; break;
        case falloff::none: e.law = falloff::inverse_ranged; break;
        }
        ENGINE_LOG_INFO(log_demo, "siren falloff: %s", name_of(e.law));
    }

    /// Fire and forget. The handle returned here is DROPPED — which is the
    /// normal way to play a sound effect and is exactly the case generational
    /// handles exist for: the slot is reused within a second, and nothing that
    /// kept this id can ever act on the sound that replaces it.
    void ping()
    {
        const transform placement{.position = listener_pos_, .rotation = listener_rot_};
        const voice_id id = mixer_.play_spatial(ping_, listener_from(placement),
                                                sources_[0].e, {});
        pings_ += id.valid() ? 1 : 0;
    }

    /// Headless only: pretend to be the audio thread, so a shot's counters are
    /// not all zero and the offline path is exercised by a program.
    void drive_offline()
    {
        const int frames = mixer_.config().buffer_frames;
        scratch_.assign(static_cast<std::size_t>(frames) * 2u, 0.0f);
        const int buffers = std::max(1, static_cast<int>(mixer_.config().freq
                                                         / (60.0f * frames)));
        for (int b = 0; b < buffers; ++b) { mixer_.mix_into(scratch_.data(), frames); }
    }

    void draw(const listener& lis)
    {
        engine::framebuffer& f = fb();
        f.clear(k_background);
        draw_grid(f);

        int lx = 0;
        int ly = 0;
        to_screen(listener_pos_, lx, ly);

        // Rings around the LISTENER at powers of two, which is where the -6 dB
        // per doubling of §5 becomes something you can point at: each ring is
        // one halving of amplitude for any emitter using the inverse law.
        for (float r : {2.0f, 4.0f, 8.0f, 16.0f, 32.0f})
        {
            draw_ring(f, listener_pos_, r, k_grid);
        }

        for (const source& s : sources_)
        {
            if (!s.on) { continue; }
            if (s.e.law != falloff::none)
            {
                draw_ring(f, s.e.position, s.e.ref_distance, k_ring);
                draw_ring(f, s.e.position, s.e.max_distance, k_ring);
            }
            int sx = 0;
            int sy = 0;
            to_screen(s.e.position, sx, sy);
            engine::draw_line(f, lx, ly, sx, sy, s.colour);
            draw_blob(f, s.e.position, 5, s.colour);

            // The gain pair, drawn where the sound is: left bar above, right
            // below. Two bars at the same length is a centred sound.
            draw_meter(f, sx - 30, sy - 18, 60, 5, s.last.gain.left, s.colour);
            draw_meter(f, sx - 30, sy + 12, 60, 5, s.last.gain.right, s.colour);
        }

        // The listener's own frame: forward in blue (-Z), right in red (+X),
        // matching conventions.html §10's axis colours.
        const vec3 fwd = listener_pos_ + lis.forward * 4.0f;
        const vec3 rgt = listener_pos_ + lis.right * 3.0f;
        int fx = 0;
        int fy = 0;
        int rx = 0;
        int ry = 0;
        to_screen(fwd, fx, fy);
        to_screen(rgt, rx, ry);
        // Drawn after the emitter lines so the listener's own frame is never
        // buried under one of them — which it was, in the first version of this
        // picture, exactly when an emitter happened to sit on the +x axis.
        engine::draw_line(f, lx, ly, fx, fy, k_forward);
        engine::draw_line(f, lx - 1, ly, fx - 1, fy, k_forward);
        engine::draw_line(f, lx, ly, rx, ry, k_right_axis);
        engine::draw_line(f, lx, ly - 1, rx, ry - 1, k_right_axis);
        draw_blob(f, listener_pos_, 5, k_listener);
    }

    void build_panel()
    {
        const mixer_report rep = mixer_.report();

        ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(430.0f, 0.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("audio")) { ImGui::End(); return; }

        ImGui::Text("device %s   %d Hz   %d frames (%.2f ms)",
                    mixer_.is_open() ? "open" : "OFFLINE",
                    mixer_.config().freq, mixer_.config().buffer_frames,
                    1000.0 * mixer_.config().buffer_frames / mixer_.config().freq);
        ImGui::Text("master %s [M]     pings fired %d", mute_ ? "MUTED" : "1.00", pings_);
        ImGui::Separator();

        for (const source& s : sources_)
        {
            ImGui::Text("%-18s %s", s.name, s.on ? "on" : "OFF");
            ImGui::Text("   d %6.2f m   atten %6.4f (%6.2f dB)   %s",
                        static_cast<double>(s.last.distance),
                        static_cast<double>(s.last.attenuation),
                        static_cast<double>(amplitude_to_db(s.last.attenuation)),
                        name_of(s.e.law));
            ImGui::Text("   pan %5.2f    L %6.2f dB    R %6.2f dB",
                        static_cast<double>(s.last.pan),
                        static_cast<double>(amplitude_to_db(s.last.gain.left)),
                        static_cast<double>(amplitude_to_db(s.last.gain.right)));
        }

        ImGui::Separator();
        ImGui::Text("voices live %zu   started %llu   finished %llu",
                    rep.live_voices,
                    static_cast<unsigned long long>(rep.voices_started),
                    static_cast<unsigned long long>(rep.voices_finished));
        ImGui::Text("buffers %llu   late %llu   queue empty %llu   clipped %llu",
                    static_cast<unsigned long long>(rep.buffers),
                    static_cast<unsigned long long>(rep.late),
                    static_cast<unsigned long long>(rep.queue_empty),
                    static_cast<unsigned long long>(rep.clipped));
        ImGui::Text("peak %.4f   mix worst %.1f us of %.1f us budget (%.2f%%)",
                    static_cast<double>(rep.peak), rep.mix_us_max, rep.budget_us,
                    100.0 * rep.worst_load());
        ImGui::Separator();
        ImGui::TextUnformatted("[1][2][3] emitters  [WASD] walk  [Q][E] turn");
        ImGui::TextUnformatted("[L] falloff law  [Space] ping  [M] mute");
        ImGui::End();
    }

    mixer mixer_;
    engine::debug_ui ui_;

    source sources_[3];
    sound ping_;
    std::vector<float> scratch_;

    vec3 listener_pos_{0.0f, 0.0f, 0.0f};
    quat listener_rot_ = quat::identity();
    float yaw_ = 0.0f;
    float t_ = 0.0f;
    int pings_ = 0;
    bool mute_ = false;

    const char* shot_path_ = nullptr;
};

}   // namespace

ENGINE_MAIN(audio_app)
