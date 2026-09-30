#!/usr/bin/env python3
"""Assemble docs/lessons/07-08-audio.html.

Same pipeline as build_71.py through build_77.py. No STATE block: STATE.md is
the sole resume key and lesson pages end at Further Reading (CLAUDE.md §9).

Prose from scratch/l78_body_{a,b,c,d,e}.html, figures from scratch/figs_78.py,
and every code listing PINNED at writing time — see LISTING_SOURCE.

WHY THE PINS MATTER PARTICULARLY HERE. This lesson lists TWO FILES IT DID NOT
CREATE — `core/log.hpp` and `core/log.cpp` — and both are touched by almost every
later lesson. It also lists `engine.hpp` and both CMakeLists, which are the three
most frequently edited files in the repository. A builder that opened a repository
path would render this page's listings as they stand TODAY rather than as they
stood when the prose was written about them, and the prose quotes them.

WHEN COPYING THIS FILE, EMPTY LISTING_SOURCE AS WELL AS FIGURES — build_66.py
inherited 6.5's three pins verbatim and they sat inert for a whole lesson. Here
the pins are generated from LISTING_META by `_pin`, so a path added to
LISTING_META without a pin file beside it fails loudly at build time.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/07-08-audio.html"

FIGURES = {
    "FIG_SAMPLES": ("l78_fig1.svg", "1",
        "<strong>The whole of digital audio, and the arithmetic that follows from it.</strong> A "
        "sound is a travelling variation in air pressure; a sample is that pressure measured once; "
        "a recording is the measurement repeated at even intervals, forever. At 48 kHz the interval "
        "is <code>20.83 us</code>, which is the number to hold next to a frame: <strong>one frame "
        "of a 60 Hz game is 800 samples</strong>, so anything the game decides once a frame is a "
        "decision the mixer must spread across eight hundred of them — which is section 7 in a "
        "sentence. The table on the right is the other consequence. There is nothing in the file "
        "but the samples, so everything is an operation on an array of numbers, and the array is "
        "384 KB a second."),

    "FIG_CLOCKS": ("l78_fig2.svg", "2",
        "<strong>Two clocks, and only one of them negotiates.</strong> Above, the simulation since "
        "Lesson 1.4: frames vary, a long one is absorbed by the accumulator, and a 38 ms frame is a "
        "hitch nobody dies of. Below, the device: 512 frames consumed every 10.67 ms on a crystal "
        "oscillator, whatever else the machine is doing. Two buffers the mixer fails to deliver are "
        "not a slow frame — they are <strong>21 ms of silence with a step discontinuity at each "
        "end</strong>, and a step contains energy at every frequency, which is what a click IS. So "
        "the audio budget is not \u0022be fast on average\u0022 but <strong>never be late</strong>, "
        "and those are different engineering problems: a mixer that takes 30 us for ninety-nine "
        "buffers and 12 ms for the hundredth clicks once a second at an average load of 0.3%. The "
        "bars at the bottom are the whole of the margin you get to play with."),

    "FIG_HEADROOM": ("l78_fig3.svg", "3",
        "<strong>Sixteen footsteps are not sixteen times as loud, and the peak is not a bound.</strong> "
        "On log-log axes the rms of N uncorrelated voices lands on <strong>sqrt(N) exactly</strong> "
        "— 0.577 x sqrt(N) measured, with no fitting — because uncorrelated signals have a zero "
        "cross term, so their mean squares add and amplitude grows as the square root. The control "
        "is what makes that mean anything: mix the SAME voice N times and it is perfectly "
        "correlated, amplitudes add, and the peak lands on <strong>N exactly</strong>. The measured "
        "peak of the uncorrelated mix sits between them at 7.68x for sixteen voices, and the table "
        "explains why it cannot be pinned down — <strong>the peak keeps growing with how long you "
        "listen</strong>, 6.98x after ten milliseconds and 10.46x after eight seconds, because it "
        "is the largest coincidence that happened to occur rather than a property of the signals. "
        "Headroom is a bet. <code>1/N</code> throws away 12 dB to win it every time."),

    "FIG_CLIP": ("l78_fig4.svg", "4",
        "<strong>What the clamp actually does, and why it is a last resort rather than a plan.</strong> "
        "Left: a 0.5 sine at a master gain of 3.0, drawn dashed, and the same sine after the clamp. "
        "The flat tops are the distortion — a straight line is not part of a sine, so what comes "
        "out is a sine plus a series of odd harmonics. Right, measured with a Goertzel at each "
        "harmonic: at gain 3.0 the <strong>third harmonic is 16.5 dB below the fundamental</strong> "
        "and total harmonic distortion is 15.22%, against about 0.01% for any amplifier you would "
        "buy. Two rows are controls. Gain 1.0 and gain 2.0 report harmonics at -160 dB, which is "
        "the measurement\u0027s own floor and not the signal\u0027s — and gain 2.0 is the "
        "interesting one, because 0.5 x 2.0 is exactly 1.000, the peak column says 1.000, and "
        "nothing clips. Full scale is a legal value."),

    "FIG_PAN": ("l78_fig5.svg", "5",
        "<strong>The hole in the middle of the obvious pan law, and where the pan value comes "
        "from.</strong> Upper left, the two gains as a sound sweeps across the image: the linear "
        "law crosses at 0.5 and the constant-power law at <strong>0.7071</strong>. Lower left, why "
        "that matters — two speakers playing the same signal are correlated, so what decides "
        "loudness is the POWER the pair delivers, and <code>0.5^2 + 0.5^2</code> is 0.5, which is "
        "<strong>-3.01 dB</strong>. A sound swept left to right audibly ducks as it crosses, and "
        "the fix is to ask for <code>L^2 + R^2 = 1</code>, whose parameterisation is a cosine and a "
        "sine. Right, the geometry: <code>pan</code> is one dot product with the listener\u0027s "
        "right axis, which is already the sine of the angle off the median plane. Two things fall "
        "out free — elevation folds to the centre, which is exactly where two speakers can put it, "
        "and front and back give the same answer, which is not a bug but the limit of a stereo "
        "pair."),

    "FIG_FALLOFF": ("l78_fig6.svg", "6",
        "<strong>The exponent is 1, not 2 — and then the physical law has to be made to end.</strong> "
        "Energy from a point spreads over a sphere, so INTENSITY falls as <code>1/d^2</code>; that "
        "part everyone remembers and it is right. But a sample is a PRESSURE, and pressure is the "
        "square root of intensity, so the number you multiply a sample by falls as <code>1/d</code>. "
        "Using the wrong one costs <strong>-12.04 dB per doubling instead of -6.02</strong>, which "
        "sounds like every sound in the game is at the bottom of a well. The other problem is "
        "visible at the right-hand edge: <code>1/d</code> never reaches zero, so at the "
        "<code>max_distance</code> every engine needs, the sound is still at "
        "<strong>-33.98 dB</strong> — quiet, audible, and about to be cut to silence, which is a "
        "step and therefore a click. <code>inverse_ranged</code> subtracts that value and rescales, "
        "reaching exactly zero; what it costs is honest and visible, because subtracting a constant "
        "steepens the far field by 8.7 dB at 32 m."),

    "FIG_RAMP": ("l78_fig7.svg", "7",
        "<strong>Lesson 7.7\u0027s rule, arriving in a subsystem that had never heard of "
        "animation.</strong> The game computes a gain once a frame; the mixer needs one per sample, "
        "and there are 800 samples in a frame. So <strong>the gain is a sampled quantity with a "
        "reconstruction</strong>, and writing the setter the obvious way chooses a zero-order hold. "
        "At the worst boundary of a fly-by the gain changes by 0.2222 in one frame, and the step "
        "that injects is <strong>0.100745 in a single sample — 7.70x the largest step the waveform "
        "itself ever takes</strong>. The ramp injects 1.00x, which is to say nothing: its largest "
        "sample-to-sample change IS the waveform\u0027s own motion. And the reason a fifth of full "
        "scale for one sample in eight hundred is audible at all is in the numbers on the right — "
        "only <strong>1.2% of the injected energy sits at the tone\u0027s own frequency</strong> "
        "and the other 98.8% is spread across every frequency there is. You are not hearing a level "
        "change; you are hearing sixty impulses a second."),

    "FIG_RESAMPLE": ("l78_fig8.svg", "8",
        "<strong>Two taps, and an honest number for what they cost.</strong> Playing a 44.1 kHz "
        "file on a 48 kHz device and playing a sound an octave up are the same operation — a cursor "
        "advancing by a fractional step — so the interpolation rule is written once and used twice. "
        "Left: every output sample is a weighted average of two input samples, so it lands on the "
        "CHORD and never on the curve. Right, measured: <strong>62.40 dB against SDL\u0027s "
        "85.05</strong>, with a control establishing that not resampling at all gives 91.67 dB — "
        "the 16-bit quantisation floor, which is what this meter reads when the answer is perfect. "
        "The table underneath is the honest half: <strong>the same code manages 89.00 dB at 200 Hz "
        "and 20.43 dB at 10 kHz</strong>, because linear interpolation is a low-pass filter and how "
        "much it damages a signal depends on how far the signal moves between samples. The number "
        "is a fact about your content."),

    "FIG_DEMO": ("l78_fig9.svg", "9",
        "<strong>A picture of something you cannot see.</strong> Every other demo in this "
        "repository can be judged by looking at it; this one cannot, so the map is not the result "
        "— it is the EXPLANATION of the result. Three emitters, each joined to the listener and "
        "each carrying two bars: left gain above, right gain below, drawn in decibels from -60 to 0 "
        "because an amplitude bar would spend 98% of its length on the first 6 dB. The faint rings "
        "are 2, 4, 8, 16 and 32 metres from the LISTENER, so <strong>each ring is one halving — six "
        "decibels</strong>, and walking outward past them is the inverse law made visible. The "
        "violet emitter uses <code>falloff::none</code> and its bars never move, which is not a "
        "broken emitter but what a music bus is. This frame was captured with <code>--shot</code>, "
        "which opens no device at all: silent, deterministic, and reproducible on a build machine "
        "with no sound card."),

    "FIG_BUDGET": ("l78_fig10.svg", "10",
        "<strong>The deadline is comfortable, which is exactly what relocates the danger.</strong> "
        "Sixty-four voices — each a fractional-step read, a linear interpolation, two multiplies "
        "and two gain increments — cost <strong>29.76 us of a 10,667 us budget</strong>, which is "
        "0.28%, or about three cycles per voice per frame. So the thing that will cause a dropout "
        "is not the mixing; it is anything UNBOUNDED that happens on that thread. One log line "
        "costs 1.02 us, <strong>more than mixing four voices</strong> — and the number is not the "
        "argument, because 1,022 ns is a MEAN taken on an idle machine writing to a local file. It "
        "is a syscall, and a syscall has no upper bound. Sixty-four voices cannot cost more than "
        "29.8 us; one unlucky log line can cost more than the whole buffer. The tables are the "
        "other half of the trade and a run on real hardware, where the device turned out to be "
        "running at 44,100 Hz rather than the 48,000 this engine defaults to."),
}

LISTING_META = {
    "engine/include/engine/audio/sound.hpp":    ("new", "new"),
    "engine/src/audio/sound.cpp":               ("new", "new"),
    "engine/include/engine/audio/spatial.hpp":  ("new", "new"),
    "engine/src/audio/spatial.cpp":             ("new", "new"),
    "engine/include/engine/audio/mixer.hpp":    ("new", "new"),
    "engine/src/audio/mixer.cpp":               ("new", "new"),
    "engine/include/engine/core/log.hpp":       ("modified", "modified"),
    "engine/src/core/log.cpp":                  ("modified", "modified"),
    "engine/include/engine/engine.hpp":         ("modified", "modified"),
    "engine/CMakeLists.txt":                    ("modified", "modified"),
    "demos/CMakeLists.txt":                     ("modified", "modified"),
    "demos/audio/main.cpp":                     ("new", "new"),
    "scratch/verify_78.cpp":                    ("new", "new"),
    "scratch/build_verify_78.sh":               ("new", "new"),
    "scratch/devcheck_78.cpp":                  ("new", "new"),
}

LISTING_LANG = {
    "scratch/build_verify_78.sh": ("bash", "shell"),
    "engine/CMakeLists.txt": ("cmake", "CMake"),
    "demos/CMakeLists.txt": ("cmake", "CMake"),
}


def _pin(path):
    return "scratch/l78_" + path.replace("/", "_")


LISTING_SOURCE = {path: _pin(path) for path in LISTING_META}


def esc(text):
    return (text.replace("&", "&amp;")
                .replace("<", "&lt;")
                .replace(">", "&gt;")
                .replace('"', "&quot;")
                .replace("'", "&#x27;"))


def listing(path):
    with open(LISTING_SOURCE.get(path, path)) as fh:
        body = fh.read()
    tag, word = LISTING_META[path]
    lang, label = LISTING_LANG.get(path, ("cpp", "C++"))
    return (
        '  <figure class="listing">\n'
        '    <figcaption>\n'
        f'      <span class="path">{path}</span>\n'
        f'      <span class="tag {tag}">{word}</span>\n'
        f'      <span class="lang" data-lang="{lang}">{label}</span>\n'
        '    </figcaption>\n'
        f'    <pre><code class="lang-{lang}">{esc(body)}</code></pre>\n'
        '  </figure>\n'
    )


def figure(key):
    name, num, caption = FIGURES[key]
    with open(f"scratch/{name}") as fh:
        svg = fh.read().rstrip()
    svg = "\n".join("    " + ln if ln.strip() else ln for ln in svg.split("\n"))
    return (
        '  <figure class="dia bleed">\n'
        f'{svg}\n'
        '    <figcaption>\n'
        f'      <span class="fignum">Figure {num}.</span>\n'
        f'      {caption}\n'
        '    </figcaption>\n'
        '  </figure>\n'
    )


HEAD = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>7.8 — SDL3 Audio: Streams, Mixing, and 3D Sound · Build a Professional 3D Game Engine</title>
<meta name="description" content="The first subsystem in this engine whose output you cannot look at, which changes what the work is: a bug you cannot see has to be measured, on a thread you do not own, against a deadline that does not negotiate. The arithmetic is the simplest in the course - mixing is addition, a distance curve is one divide, panning is two trigonometric functions - and everything hard follows from addition. Sum sixteen uncorrelated voices and the level grows as the square root of N, not N, measured at exactly 0.577 times root N, so the obvious 1/N headroom throws away 12 dB; but the PEAK follows neither and keeps growing the longer you listen, 6.98x at ten milliseconds and 10.46x at eight seconds, which makes headroom a bet rather than a bound. Pan with the first law that occurs to you and a sound sweeping across the image ducks by 3.01 dB in the middle, because two speakers add in POWER. Attenuate with one over d squared - the law everyone remembers, and the right law for the wrong quantity - and everything falls off at minus 12 dB per doubling instead of minus 6, because a sample is a pressure and pressure is the square root of intensity. Then Lesson 7.7 arriving in a subsystem that had never heard of animation: the GAIN is a lossy reconstruction too, sampled at 60 Hz and reconstructed at 48 kHz, and a zero-order hold injects a step of 0.1007 in one sample - 7.7 times the largest step the waveform itself ever takes - of which 98.8 percent is broadband, which is what a click IS. Also: SDL3 audio streams and the resume SDL2 did not need, a WAV loader honest about which half to hand away, a linear resampler measured at 62.40 dB against SDL 85.05 and falling to 20.43 dB at 10 kHz, generational handles meeting the case they were designed for, one mutex on a real-time thread with the compromise stated, and a dropout counter that turned out to be measuring SDL working correctly.">

<!-- SHARED-CSS:BEGIN -->
<!-- The shared course stylesheet, linked rather than inlined. Single source of
     truth: docs/shared/course.css. Edit that file; this page carries no copy.
     Still no build step - the link resolves straight off the filesystem, so this
     page opens by double-clicking, offline. -->
<link rel="stylesheet" href="../shared/course.css">
<!-- SHARED-CSS:END -->

<!-- KaTeX (optional). If unreachable the raw TeX remains readable, and every
     equation is also stated in prose + .eq-plain, so nothing is lost. -->
<link rel="stylesheet"
      href="https://cdn.jsdelivr.net/npm/katex@0.16.11/dist/katex.min.css"
      integrity="sha384-nB0miv6/jRmo5UMMR1wu3Gz6NLsoTkbqJghGIsx//Rlm+ZU03BU6SQNC66uf4l5+"
      crossorigin="anonymous">
</head>
<body>

<header class="masthead">
  <div class="masthead-inner">
    <a class="course" href="../index.html">Build a Professional 3D Game Engine</a>
    <span class="spacer"></span>
    <a href="../index.html">Contents</a>
    <a href="../conventions.html">Conventions</a>
    <a href="../math-toolbox.html">Math Toolbox</a>
    <button class="theme-toggle" id="theme-toggle" type="button" aria-label="Toggle colour theme">Theme</button>
  </div>
</header>

<div class="wrap">

"""

TAIL = """
  <nav class="lesson-nav" aria-label="Lesson navigation (bottom)">
    <a class="prev-l" href="07-07b-gltf-characters.html">
      <span class="dir">← Previous</span>
      <span class="ttl">7.7b — Animated Characters from glTF: Skins and Clips</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="08-01-integrators.html">
      <span class="dir">Next →</span>
      <span class="ttl">8.1 — Integrators: Why One Explodes</span>
    </a>
  </nav>

</div>

<!-- SHARED-SCRIPT:BEGIN -->
<!-- The shared page script (theme toggle, TOC scrollspy, syntax highlighter),
     linked rather than inlined. Single source of truth: docs/shared/course.js.
     A plain classic script at end of body, so it runs exactly where the inline
     copy used to: after the DOM is parsed, before KaTeX's deferred render. -->
<script src="../shared/course.js"></script>
<script defer
        src="https://cdn.jsdelivr.net/npm/katex@0.16.11/dist/katex.min.js"
        integrity="sha384-7zkQWkzuo3B5mTepMUcHkMB5jZaolc2xDwL6VFqjFALcbeS9Ggm/Yr2r3Dy4lfFg"
        crossorigin="anonymous"></script>
<script defer
        src="https://cdn.jsdelivr.net/npm/katex@0.16.11/dist/contrib/auto-render.min.js"
        integrity="sha384-43gviWU0YVjaDtb/GhzOouOXtZMP/7XUzwPTstBeZFe/+rCMvRwr4yROQP43s0Xk"
        crossorigin="anonymous"
        onload="renderMathInElement(document.body, {
          delimiters: [
            {left: '\\\\[', right: '\\\\]', display: true},
            {left: '\\\\(', right: '\\\\)', display: false}
          ],
          throwOnError: false
        });"></script>
<!-- SHARED-SCRIPT:END -->
</body>
</html>
"""


def main():
    parts = []
    for name in ("a", "b", "c", "d", "e"):
        with open(f"scratch/l78_body_{name}.html") as fh:
            parts.append(fh.read())

    page = HEAD + "\n".join(parts) + TAIL

    for key in FIGURES:
        page = page.replace(f"@@{key}@@", figure(key))

    page = re.sub(r"@@LISTING:([^@]+)@@", lambda m: listing(m.group(1)), page)

    left = re.findall(r"@@[A-Z0-9_:./-]+@@", page)
    if left:
        raise SystemExit(f"unsubstituted placeholders: {sorted(set(left))}")

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as fh:
        fh.write(page)
    # BYTES, not characters — build_74.py's note applies. The page is UTF-8 and
    # full of × − ° ₁, so `len(page)` is several thousand short of `wc -c`.
    print(f"wrote {OUT}  ({len(page.encode('utf-8')):,} bytes, "
          f"{page.count(chr(10)):,} lines)")


if __name__ == "__main__":
    main()
