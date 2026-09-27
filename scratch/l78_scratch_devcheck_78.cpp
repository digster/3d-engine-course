// scratch/devcheck_78.cpp — the one thing verify_78.cpp deliberately cannot do.
//
// Lesson 7.8. `verify_78` opens no device: every number it prints comes from
// `mixer::open_offline`, so it runs on a build machine with no sound card and is
// reproducible to the last bit. That is the right way to test a mixer and it
// leaves exactly one thing untested — **whether the device path works at all**.
// SDL_OpenAudioDeviceStream, the callback SDL runs on its own thread, the resume
// that SDL3 requires and SDL2 did not, and the shutdown order: none of those are
// exercised by an offline mix, and all of them are the kind of code that is
// either completely right or completely silent.
//
// So this is a second, deliberately tiny program that makes a noise for seven
// tenths of a second and then reports what the audio thread saw.
//
//   c++ -std=c++20 -O2 -I engine/include -I build/_deps/sdl3-src/include \
//       scratch/devcheck_78.cpp build-rel/engine/libengine.a \
//       -L build-rel/_deps/sdl3-build -lSDL3 \
//       -Wl,-rpath,"$PWD/build-rel/_deps/sdl3-build" -o build/demos/devcheck_78
//   ./build/demos/devcheck_78
//
// IT FOUND SOMETHING, AND THAT IS WHY IT IS IN THE REPOSITORY RATHER THAN IN A
// TERMINAL SCROLLBACK. The first version of `mixer_report` carried a counter
// called `starved`, incremented when the device's queue was empty as the callback
// began — the obvious underrun proxy. On this machine, on a mix using 0.2% of its
// budget with nothing wrong anywhere, it reported **28 of 58 buffers starved**.
// The proxy was measuring SDL3's pull model working correctly: the callback is
// invoked BECAUSE the device wants more, so an empty queue at that moment is the
// steady state.
//
// The counter is now called `queue_empty` and says so, and the question it was
// meant to answer is answered by `late`, which compares the mixer's own time
// against the mixer's own deadline and needs nothing from the driver. A healthy
// run prints `late 0`. That is a check whose degenerate case is a FAILURE, which
// is the only kind worth having.

#include <engine/audio/mixer.hpp>
#include <engine/audio/sound.hpp>

#include <SDL3/SDL.h>

#include <cstdio>

int main()
{
    engine::audio::mixer mix;
    if (!mix.open({.buffer_frames = 512}))
    {
        // Not a failure of the program: a machine with no playback device is a
        // machine this check cannot run on, and saying so is the correct exit.
        std::printf("devcheck_78: no playback device (%s)\n", SDL_GetError());
        return 0;
    }

    // Quiet on purpose — this is a check, not a demonstration — and faded at
    // both ends, because a looping tone whose last sample is mid-cycle clicks
    // once per second forever (§6).
    engine::audio::sound tone =
        engine::audio::make_tone(440.0f, 1.0f, mix.config().freq, 0.02f);
    engine::audio::apply_fade(tone, 0.01f, 0.01f);

    const engine::audio::voice_id v = mix.play(tone, {.gain = 1.0f, .loops = true});
    if (!v.valid())
    {
        std::printf("devcheck_78: play() refused the voice\n");
        mix.close();
        return 1;
    }

    SDL_Delay(700);

    const engine::audio::mixer_report r = mix.report();
    std::printf("device   %d Hz   %d frames   budget %.2f us\n",
                mix.config().freq, mix.config().buffer_frames, r.budget_us);
    std::printf("buffers  %llu   frames %llu   live voices %zu\n",
                static_cast<unsigned long long>(r.buffers),
                static_cast<unsigned long long>(r.frames_mixed), r.live_voices);
    std::printf("late     %llu   queue_empty %llu   clipped %llu\n",
                static_cast<unsigned long long>(r.late),
                static_cast<unsigned long long>(r.queue_empty),
                static_cast<unsigned long long>(r.clipped));
    std::printf("peak     %.4f   worst mix %.2f us (%.3f%% of budget)\n",
                static_cast<double>(r.peak), r.mix_us_max, 100.0 * r.worst_load());

    // The lifetime rule, obeyed: `tone` is about to go out of scope and the
    // mixer holds a pointer into it. `close()` would do this too; saying it
    // explicitly is what the rule looks like in code you are meant to copy.
    mix.stop_sound(tone);
    mix.close();
    return 0;
}
