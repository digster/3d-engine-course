# Prompt Log

A running log of the prompts that shaped this project, newest last.

---

## 2026-07-16 — Project inception

> Work on the following prompt and save it as is in the project's claude file for the future -

Followed by the full **Master Prompt — "Build a Professional 3D Game Engine" (SDL3 + C++)**.

The master prompt is stored verbatim in [CLAUDE.md](CLAUDE.md) and is the binding
specification for this project. It is not reproduced here to avoid the two copies
drifting apart — CLAUDE.md is the single source of truth.

**Response:** The §9 first response — curriculum, repository layout, course-wide
conventions, the reusable HTML lesson template, and the C++ style guide. Delivered as
real files under `docs/` plus the repo meta files. Stopped before Lesson 0.1 as §9
requires.

**Decisions ratified in this session:**

| Decision | Choice |
|---|---|
| World handedness | Right-handed, Y-up, −Z forward |
| Software rasterizer NDC | Matches SDL_GPU exactly (+Y up, z ∈ [0,1]) |
| First-response delivery | Real files + chat summary |
| Version control | `git init`, assistant never commits |
| SDL3 acquisition | CMake `FetchContent` at a pinned tag |

---

## 2026-07-16 → 2026-07-18 — Lessons 0.1 through 1.1

Not logged at the time. These sessions were driven by the §9 lesson-advance protocol
(`next` and equivalents) rather than by substantive new instructions; the record of what
each produced lives in `memory/2026-07-16.md` and in STATE.md's `completed` list.

---

## 2026-07-18 — Lesson 1.2

> use STATE.md and the project's claude instructions to work on the next.

**Response:** Lesson 1.2 — Input: State vs Events, produced as real files per §9. The
input subsystem (`src/core/input.{hpp,cpp}`), the `main.cpp` frame reorganisation, the
CMake change for a second translation unit, and the lesson page with five SVG diagrams
and an interactive level-versus-edge widget.

**Decisions made in this session (flagged for ratification):**

| Decision | Choice | Rationale |
|---|---|---|
| Where input lives | `src/core/`, not the `src/platform/` that ARCHITECTURE.md had planned | No platform layer exists until Module 5; input is a state cache, not a device driver. Revisit at the Module 5 refactor. |
| Code listings | Spliced from the real source files at publish time via `<!--INCLUDE:path-->` markers | A hand-copied listing is how a lesson drifts from `src/` — the §8 continuity bug. |
| Interactive widgets | First one introduced here (the CSS was already in the template) | §7 encourages them where they materially aid intuition; edge-vs-level is a timing idea that a still diagram can only half-show. |
| Deliberate defect retained | Demo movement stays per-frame, not per-second | It is exactly what 1.3 exists to fix, and it is more convincing on screen than in prose. |

## 2026-07-18 — Shared script region (tooling)

> The trailing `<script>` block in each lesson page under docs/lessons/ is duplicated per file
> (required — pages must be self-contained) but nothing propagates it, and it has drifted.
> [Details of the drift across 00-04 / 00-06 / 01-01 / 01-02, with 01-02 named as the canonical
> superset.] Task: (1) add the canonical script to lesson-template.html, reconciling to the
> superset; (2) extend apply-shared-css.py to stamp a SHARED-SCRIPT region, keeping --check and
> the inline-`fill=` lint, considering a rename to apply-shared.py; (3) add markers to the 8
> lesson pages plus index/conventions/cpp-style/math-toolbox and stamp; (4) verify over HTTP in
> a real browser (Playwright, not the preview pane) that cpp/cmake/bash highlighting still works
> on 00-04, 01-01 and 01-02, the theme toggle works, and no console errors appear.

**Response:** Renamed to `apply-shared.py` and generalised it to a `Region` dataclass covering
`SHARED-CSS` (unchanged extraction, byte-identical behaviour) and `SHARED-SCRIPT` (extracted from
the template's own markers). Stamped all 12 pages; verified in Chromium.

**Corrections to the brief, flagged for ratification:**

| Finding | Action taken |
|---|---|
| `lesson-template.html` already held a **richer** script than 01-02 — full C++20 keyword set, HLSL types, `lang-hlsl`/`lang-c`/`lang-sh` dispatch. 01-02 is not the superset. | Built the true union: template's breadth + 01-02's SDL `Uint*`/`Sint*` types, `defined`/`override`/`final`, and the `::` rule. |
| A **literal** union would regress 12 pages: 01-02 files `bool char int long unsigned void` as keywords, the template as types, and `kw` is checked before `ty`. | Kept the template's classification; folded in only genuinely-absent words. Verified in-browser that `int`/`bool`/`Uint8` still render `.tok-t`. |
| 01-02's `::` shell-comment rule is **unanchored**, so `--target SDL3::SDL3 --config Release` renders as a comment. | Re-anchored to line start, capture groups renumbered, unit-tested in `node` before stamping. |
| Lesson STATE-block manifests still name `apply-shared-css.py`. | **1.2's updated** on a follow-up "ok update it" — it is the live resume key, so a dead filename there is a hazard. 00-01…01-01 left as history: the rename postdates them. Rule: only the newest lesson's STATE block tracks reality. |

> what about STATE.md?

**Response:** The one-line rename was the smallest part of it, and the question surfaced a real
bug. `lesson-template.html` carried `SHARED-SCRIPT` markers but had **never** carried
`SHARED-CSS` ones — those were hand-added to the 12 pages. Since a lesson is authored by copying
the template, Lesson 1.3 would have inherited script propagation and silently missed CSS
propagation (reported only as a `skip` line). Wrapped the template's `<style>` in markers, which
made the tool's two extraction paths pointless: collapsed both onto one marker-based
`Region.find()`, deleting `STYLE_RE`, the `Callable` field and both `_extract_*` helpers.
`--check` staying clean across the refactor proves extraction is byte-identical.

STATE.md itself gained `docs-tooling` and `docs-verify` convention entries — it is the resume
key, so without them a session picking up 1.3 would have quietly reintroduced the drift this
session removed.

---

## 2026-07-18 — Lesson 1.3

> next

**Response:** Lesson 1.3 — Frames, Delta Time, and Why Naive Loops Lie. The `clock`
subsystem (`src/core/clock.{hpp,cpp}`), a demo rebuilt around three time-scaled
behaviours with runtime vsync and throttle controls, and the lesson page with five SVG
diagrams and an interactive explicit-Euler widget.

**Decisions made in this session:**

| Decision | Choice | Rationale |
|---|---|---|
| Clock ceiling | `dt()` clamped at 0.25 s by default, with `raw_dt()` and `was_clamped()` alongside | The clamp is a deliberate lie; a system that distorts reality to stay safe should be able to say so. A profiler reading the clamped value would report a healthy frame during a stall. |
| Absolute time type | `Uint64` nanoseconds; only the delta becomes `float` | Verified by compiling it: a `float` seconds counter stops advancing entirely after ~24 h of uptime. |
| Demo default | vsync **off** | Honest default for an instrument. With vsync on, only one frame rate exists and the bug under study is invisible. |
| Physics in the demo | Perfectly elastic bounce, explicit Euler | Restitution 1.0 means the integrator is the *only* thing that can change the ball's energy, so the error is unambiguous — and at 20 fps it throws the ball out of the window in three bounces. |
| Scope held back | Semi-implicit Euler shown numerically but not adopted; integrators deferred to Module 7 | It is still first-order and still frame-rate dependent, so it does not solve 1.3's problem and would pre-empt a lesson that does it properly. |

---

## 2026-07-18 — Lesson 1.4

> next

**Response:** Lesson 1.4 — The Fixed Timestep with Interpolation, Derived. The `fixed_step`
accumulator (`src/core/fixed_step.{hpp,cpp}`), the main loop's final form, a three-box
demo comparing variable-dt / fixed-raw / fixed-interpolated motion, and the lesson page
with four SVG diagrams and an interactive interpolation widget.

**Decisions made in this session:**

| Decision | Choice | Rationale |
|---|---|---|
| Lesson structure | Three broken attempts in sequence, each fixing what the last one broke | The sequence *is* the understanding. A reader who has felt attempt one run at half speed will not later mistake the fixed timestep for the whole answer. |
| Accumulator as a class | `fixed_step` owns the subtraction | The invariant is maintained by one line; inline, skipping it hangs the app inside a single frame with no output. |
| Step-cap behaviour | Drain the excess whole steps, don't just return | Returning leaves `alpha > 1`, so the renderer starts extrapolating exactly when the machine is already struggling. |
| Demo box motion | Bounces, never wraps | Wrapping is a teleport, and interpolating a teleport slides the box backwards across the screen. Kept as Exercise 1.4.4 rather than shipped as a bug. |
| Determinism claim | Explicitly scoped to same-binary/same-machine | Cross-platform float determinism needs far more (FMA, x87, libm, vectorisation). Overclaiming here is how people ship broken lockstep. |
| Figure 3 | Rebuilt to plot lag-over-time rather than position offset | The "one step behind" offset is ~7 px at honest scale and reads as noise; plotting the lag itself makes the identical claim unmissable. |

---

## 2026-07-18 — Lesson 1.5

> next

**Response:** Lesson 1.5 — The Framebuffer: Your First Owned Pixels. The `gfx::framebuffer`
CPU pixel buffer, streaming-texture presentation with row-wise upload, the demo redrawn
entirely into our own memory, and the lesson page with four SVG diagrams and an
interactive 2-D-to-1-D index widget.

**Decisions made in this session:**

| Decision | Choice | Rationale |
|---|---|---|
| Framebuffer size | 320×180, scaled 4× to the window | Every pixel becomes a visible 4×4 block, so `put_pixel` has consequences you can see — and fixed internal resolution is a real shipping technique that also makes window resize free. |
| Upload path | `SDL_LockTexture` + row-by-row copy | SDL's docs prefer locking for streaming textures, and it makes the pitch lesson concrete: the driver's row stride may exceed `width × 4`. |
| Pixel format | `ARGB8888`, built with shifts, never byte writes | Keeps us in the packed-integer view, so the format matches on big-endian too and endianness stays out of the engine until Module 6's image loader. |
| API shape | `put_pixel` bounds-checked, `row(y)` the documented fast path | Safe-only forces slow code; fast-only forces every caller to be perfect. Measured the gap (5–15×) rather than asserting it. |
| Performance claims | All measured, and one corrected | A first benchmark showed row/column order made no difference; the cause was measuring through `put_pixel`, whose overhead hid the 10.9× cache effect. Written up as a pitfall and an exercise. |

---

## 2026-07-18 — Lesson 1.6

> next

**Response:** Lesson 1.6 — Colour, and an Honest Teaser of sRGB. The `gfx::colour` module
(exact sRGB transfer functions, a 256-entry decode LUT, `mix_encoded` vs `mix_linear`),
`pack_argb` relocated from `framebuffer.hpp`, the demo rebuilt as a comparison board, and
the lesson page with three SVG diagrams and an interactive display-calibration test.

**Decisions made in this session:**

| Decision | Choice | Rationale |
|---|---|---|
| Scope | Correct mixing functions, but **not** a linear pipeline | §3.5 says so explicitly. A real linear renderer needs a float framebuffer, headroom above 1.0 and tonemapping — Module 6. Implying the problem was solved would be worse than leaving it open. |
| `pack_argb`'s home | Moved to `colour.hpp`; framebuffer includes colour | The dependency belongs that way round. It was in the wrong file for the honest reason that 1.5 had nowhere else to put it. |
| Demo construction | One `draw_ramp` taking the mix rule as a function pointer | The two sides of a comparison must differ in exactly one thing, or you are comparing code paths rather than rules. |
| Alpha | Never converted, in either mixing rule | Alpha is coverage — a fraction of area, already linear. Converting it makes 50% behave like 20%, and the bug can hide for months because nothing composites yet. |
| Numbers | Every figure derived in Python, then reproduced by a 24-assertion C++ harness | The engine prints the two headline numbers at startup, so a build disagreeing with the prose is a real signal rather than a rounding difference. |

---

## 2026-07-18 — Lesson 1.7

> next

**Response:** Lesson 1.7 — 2D Vectors, Geometrically. `src/math/vec2.hpp` (header-only),
a demo pairing a live dot-product visualiser with a normalisation race, and the lesson
page with five SVG diagrams and a drag-to-explore widget.

**Decisions made in this session:**

| Decision | Choice | Rationale |
|---|---|---|
| Dot-product derivation | Via projection onto the axes, **not** the law of cosines | The brief bans assumed trigonometry beyond basics. The chosen route needs only "shadows add" and "components are projections", and it explains *why* the formula looks like that. |
| `vec2` is header-only | Yes, with the argument made in the lesson | Small, hot, stable code must be inlinable. The recompilation cost is named, and it is explicitly not a general licence. |
| Zero-vector normalisation | Returns `(0,0)`, presented as a choice | There is no correct answer, only a safe one. `normalised_or()` exists for callers who need a direction to exist. |
| Worked numbers | 3-4-5 triangles throughout | Checkable by hand. Cost a Figure 4 redraw, since a 4.8 shadow on a length-5 arrow overlaps almost exactly — solved with an offset dimension bar rather than by changing the numbers. |
| Line drawing | Local to `main.cpp`, debt flagged | A vector lesson must draw arrows. Lesson 2.1 derives DDA properly and moves it into `gfx/`. |
