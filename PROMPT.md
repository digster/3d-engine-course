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

---

## 2026-07-18 — Lesson 1.8

> Based on the STATE and the project's claude instructions, work on the next.

**Response:** Lesson 1.8 — Checkpoint: Pong, the last lesson of Module 1. A complete
playable game in `src/game/pong.{hpp,cpp}`, `reflect()` added to `vec2.hpp`, `main.cpp`
reduced to a host, and the lesson page with seven SVG diagrams. Module 1 complete: the
three living pages (index, conventions, math-toolbox) reissued at the boundary.

**Decisions made in this session:**

| Decision | Choice | Rationale |
|---|---|---|
| Where Pong lives | A new `src/game/` directory, three modules before Module 5 needs it | The refactor's difficulty is decided entirely by how many wrong-way dependencies accumulate first. Zero of them makes it a rename. The test for engine-ness: *could a different game use this unchanged?* |
| The new idea for a "consolidation" lesson | Swept collision — collision as a fact about an **interval**, not an instant | A checkpoint still needs a spine. This is the first lesson where anything can *hit* anything, so it is the first place the discrete-sampling failure can exist at all — and it recurs as aliasing (M2), texture shimmer (M3) and shadow acne (M6). |
| The naive test | **Kept in the shipped code**, behind `state::swept_collision`, toggled with `[K]` | Pedagogy §5 requires showing the artifact. A bug you can summon on demand teaches more than one you can only describe — and the toggle makes the two rules differ in exactly one thing. |
| Demonstrating tunnelling | Reuse 1.4's existing `[1-4]` sim-rate keys | At 10 Hz the opening serve tunnels immediately; at 60 Hz it is unreachable. No new controls, and it makes the fixed timestep the *cause* rather than a separate topic. |
| The headline claim | `\|v_axis\|·h < size_a + size_b`, tabulated per sim rate | At 60 Hz the safe ceiling is 480 px/s and the ball caps at 260 — the bug is not merely untested but **unobservable**, which is a sharper statement about test suites than any amount of exhortation. |
| Paddle bounce | Angle from hit position, **not** an honest `reflect` | A mirror paddle conserves `v.y` for the whole match, so no player can place a shot and there is no game. Exercise 1.8.1 has the student break it and diagnose it. The lie is documented, which is what separates a design decision from a bug. |
| Wall bounce | The general `reflect()` even though a sign flip would do | Same cost (`constexpr`, folds to a negation), and it is the form that survives Exercise 1.8.4's angled wall. Worked example 2 shows a 45° wall turning a vertical drop horizontal — something no component negation can produce. |
| Randomness | Hand-rolled xorshift32 **inside the state**, not `SDL_rand` | `SDL_rand` exists and works (verified in `SDL_stdinc.h`), but keeps state in a hidden global — which would mean the simulation is not a function of its inputs, and replays and determinism die silently. |
| Code listings | Spliced from the real files by a script (`@@LISTING:path@@`) | §8 demands zero placeholders and that every listing compile at its point in the course. Splicing makes drift impossible rather than unlikely, and gets the HTML escaping right every time. |
| Verification | A throwaway harness in the scratchpad, not a test suite in the repo | Testing is Module 8's lesson; adding a framework now would spoil it. The harness proved every number in the lesson, and found that two of its own first assertions were wrong rather than the code. |

**Bug found and fixed along the way:** the KaTeX loader sat *below* `<!-- SHARED-SCRIPT:END -->`
in the template, so `apply-shared.py` never propagated it — every lesson shipped without a maths
renderer. It stayed invisible for six lessons because raw TeX is *also* the documented
CDN-unreachable fallback, and because lessons 1.1–1.7 contain no display math at all. 1.8 is the
first lesson with equations and the first to catch it. Moved inside the region; the duplicate
standalone copies in `conventions.html` and `math-toolbox.html` removed so those pages do not
load KaTeX twice.

---

## 2026-07-21 — Lesson 2.1

> next

**Response:** Lesson 2.1 — Lines: DDA, then Bresenham, the first lesson of Module 2.
`src/gfx/raster.{hpp,cpp}` with three line routines, `main.cpp` rebuilt as an octant-fan
demo with a magnified pixel inspector, and the lesson page with six SVG diagrams. Also
fixed a text-vs-shape defect class by adding `docs/_template/check-page.js`.

**Decisions made in this session:**

| Decision | Choice | Rationale |
|---|---|---|
| The lesson's spine | Derive the midpoint decision, then **scale it into integers** | The scaling step ("the only uses are order-preserving, so multiply until the fractions vanish") is the transferable idea; it returns as fixed-point sub-pixel precision and depth encoding. Bresenham is the example, not the point. |
| The performance claim | **Measured, and it contradicted the textbook** | DDA is ~2.2× faster than the compact Bresenham on an M4 Pro. Isolated the cause: two data-dependent branches, not the floats — swapping `lround` for truncation changes nothing, and a one-branch Bresenham recovers most of the gap. Reported honestly rather than repeating folklore. |
| Which routine ships | **Bresenham anyway**, with the number written down | Exactness (integers are bit-identical across machines; 1.8 established floats are not), and its error term *is* 2.2's edge function. Modelling "we chose the slower one, here is why, disagree if you like" is worth more than being right about nanoseconds. Exercise 2.1.3 invites the opposite call. |
| Generalising to 8 octants | Derive octant 1 rigorously, present the compact form, then **verify** rather than re-derive | The fused sign bookkeeping teaches nothing. Flagged as a simplification per §10 and backed by an exhaustive check against a reflect-in/out reference over 1600 lines. Added a one-line termination proof, which *is* worth deriving. |
| Ties | Proved a small theorem: a tie exists iff `major/gcd` is even | Verified exhaustively (23,103 lines; 7,692 asymmetric; zero disagreements). It explains *why* 2.2 needs a fill rule, which turns a piece of trivia into the bridge to the next lesson. |
| Keeping the broken routine | `draw_line_naive` ships, switchable at run time | Same reasoning as Pong's naive collision test: a failure you can summon beats one you can only describe. The demo colours steep spokes differently so the artifact is *diagnosable*, not just visible. |
| Pong | Preserved on <kbd>Tab</kbd> rather than deleted | Losing a working deliverable would be a regression. Two demos in one binary is deliberately awkward — that awkwardness is the argument for Module 5's `demos/` split. |
| Demo construction | The inspector reads back the **real** routine's output | A diagram generated by different code from the thing it documents will eventually lie about it. |

**Tooling added:** `docs/_template/check-page.js` — highlighter round-trip, KaTeX *positive*
signal, and three SVG geometry checks. The new one is **text-vs-shape**: sample points along every
stroke and test them against text boxes. It caught this lesson's Figure 4 (threshold label sitting
on the sawtooth), and running it over the back catalogue found ten pre-existing instances
(1.5 ×2, 1.6 ×3, 1.7 ×4, conventions ×1) — reported, not silently folded into this lesson's scope.
Two false positives are designed out of it: `<defs>` arrowhead paths, and `.grid` graph paper
(whose class sits on the wrapping `<g>`, so `getAttribute` on the `<path>` returns null).

---

## Session — Clearing the back-catalogue diagram defects (2026-07-21, later)

**Prompt:** Fix the ten pre-existing text-on-shape defects that `docs/_template/check-page.js`
(added in 2.1) found in already-published lesson pages — each a label sitting on a line or curve.
Reproduce by serving `docs/` and running the checker in real Chromium (Playwright, not the preview
pane); require `pass: true` on `svgTextOnShape`. Fix by moving labels into clear space, adding a
short leader where a label must stay near what it annotates (see 2.1 Figure 1 for the pattern);
prefer the empty quadrant per `_template/README.md`. Do not move diagram geometry, hard-code
colours, or put `fill=` on an SVG `<text>`. Re-run `apply-shared.py --check`, re-verify each page,
and eyeball a screenshot of each edited figure — the checker cannot see a label on a filled
`<rect>`. Do not renumber or restructure any lesson.

**Outcome:** all eleven defects fixed (ten text-on-shape + conventions' `+y`/`up` overlap) across
`conventions.html`, 1.5, 1.6, 1.7 and 1.8. Diff is label coordinates, explanatory comments, and one
leader line. All six pages `pass: true`; `apply-shared.py --check` clean; every edited figure
eyeballed in light and dark. A full 19-page sweep surfaced further pre-existing defects (0.3, 0.6,
1.2 text-on-shape; 0.1/0.2 duplicated KaTeX block) — reported, not folded into this scope.

---

## 2026-07-21 — Lesson 2.2

> next

**Response:** Lesson 2.2 — The Triangle: Edge Functions. `edge_function` and `fill_triangle`
added to `src/gfx/raster`, a demo with a half-plane view and a coverage counter, and the
lesson page with five SVG diagrams. Also fixed a duplicate KaTeX loader in lessons 0.1 and
0.2 that my 2.1 change had created.

**Decisions made in this session:**

| Decision | Choice | Rationale |
|---|---|---|
| Framing | Not "how to fill a triangle" but **"inside = three independent yes/no questions"** | The independence is the whole point — it is why edge functions beat scanline filling, why this parallelises, and why GPUs do it. Stated up front in §1 and paid off in the exercises. |
| `fill_triangle` and winding | **Accepts either**; measures the area once and orients the test | Module 2 rasterises in framebuffer space where the y-flip reverses the sign relative to `CCW = front`. A fill that silently dropped backwards triangles would be indistinguishable from a bug. Culling is 3.4's, in NDC. |
| The sign convention | **Measured, then stated**, with the NDC-vs-screen reconciliation spelled out | `(5,0),(0,10),(10,10)` → −100. This is the single most likely place to ruin someone's next six months, and it is the exact case the master prompt §10 says to pin down rather than assume. Also recorded in conventions.html. |
| Showing the fill rule | A **coverage counter**, not colours | A pixel drawn twice looks identical to one drawn once — the defect is invisible *because* the second write lands on the first. Count the operation instead of inspecting the result. Generalised into a callout. |
| The demo's quad | **Axis-aligned, split by its 45° diagonal** — after the rotated version showed only 3 red pixels | Double-draws only hit pixels whose centres are exactly on the seam. A rotated seam loses 2–3 stray pixels (dismissible as noise); a 45° one loses all 40. And axis-aligned splits are what real geometry is made of, so the catastrophic case is the common case. Written up as a warning rather than hidden. |
| The all-important check | Not "it looks right" but **exactly-once coverage**, plus a half-open boundary of exactly 73 px | 37 + 37 − 1. When a harness assertion failed I checked which was wrong — it was my assertion ("the rule never drops a pixel"), not the code. The correct property is more informative than the one I first wrote. |
| Scope | Barycentric coordinates **foreshadowed, not taught** | They fall out of §3.3's area reading, and the sum-to-total identity is given as a debug assertion — but normalising them and taking the consequences seriously is 2.3's whole job. |

**Continuity note.** `main.cpp` was rewritten again, so `[Tab]` now cycles three demos rather than
two. Lesson 2.1's listing of `main.cpp` remains the correct snapshot *as of 2.1* — the codebase is
a single evolving tree and each lesson's listings are its historical record (ARCHITECTURE §1). The
growing awkwardness of three demos in one binary is deliberate and is called out in the lesson as
the accumulating argument for Module 5's `demos/` split.

**Bug fixed along the way.** Lessons 0.1 and 0.2 carried their own standalone KaTeX loader below
the `SHARED-SCRIPT:END` marker, so after 2.1 moved the loader *inside* the shared region they had
two — loading KaTeX twice and running `renderMathInElement` twice over the same body. Found by the
background task's sweep, fixed the same way as `conventions.html` and `math-toolbox.html`. All 20
pages now carry exactly one.

---

## 2026-07-22 — Lesson 2.3

> next

**Response:** Lesson 2.3 — Barycentric Coordinates from Signed Areas. `struct barycentric` and
`barycentric_at` added to `src/gfx/raster`, two new triangle views (weight ramp, iso-line grid)
with a mouse probe, and the lesson page with three SVG diagrams.

**Decisions made in this session:**

| Decision | Choice | Rationale |
|---|---|---|
| Framing | **"Derived, not defined"** — the rasterizer already computes these numbers and throws two thirds of the information away | Matches the curriculum's own blurb, and it is true: 2.2 uses the edge functions' signs and discards their magnitudes, which were areas. The lesson adds one division, not a computation. |
| The property to teach as central | **Reconstruction**, not sum-to-1 | A rotated pairing still sums to 1 — verified, it reconstructs (5.2, 5.8) instead of (5, 5). So the sum has almost no diagnostic power while reconstruction has complete power. Generalised into a habit: prefer the invariant that *determines* the answer over one that *constrains* it. |
| Scope against 2.4 | Coordinates only; **attributes deferred** | 2.4 owns "colour, then UVs, then anything". So 2.3's demo visualises the *coordinate* (a w0 ramp, an iso-line grid) rather than blending three vertex colours, which would be 2.4's job and would need 1.6's linear-light care. |
| Where the weights are computed | A standalone `barycentric_at`, **not** folded into the fill loop | Keeps 2.3 about the idea. Exercise 2.3.3 asks the student to make it incremental and to measure whether stepping floats drifts — which is 2.4's actual implementation problem, previewed. |
| Uniqueness | Stated and proved (3 coefficients, 3 conditions) | Students otherwise file barycentric interpolation under "a reasonable blend". It is the *only* affine one, and saying so changes how the next four lessons read. |
| Precision | **Measured, both halves reported** | Worst \|sum−1\| = 2.4e-7 *and* bitwise 1.0f only ~85% of the time. The first number alone would suggest "close enough to compare"; the second is why you must not. |

**Two things caught by tooling, both worth noting.** `apply-shared.py`'s lint caught an inline
`fill="var(--axis-z)"` on an SVG `<text>` — the exact trap LEARNINGS documents, where CSS silently
overrides the attribute. And `check-page.js` caught two Figure 2 labels sitting on the sub-triangle
boundaries; fixed by computing each region's **centroid** from its vertices rather than placing the
labels by eye, which is the same discipline 2.2's Figure 1 needed.

---

## 2026-07-23 — Lesson 2.4

> Based on the STATE and the project's claude instructions, work on the next.

Resumed from `STATE.md` → `next: 2.4 — Interpolating Attributes Across a Triangle`, with the two
requirements 2.3 recorded: fold the weights into the fill loop while keeping **biased** (coverage)
and **unbiased** (interpolation) values apart, and blend in **linear light**.

### Judgement calls

| Question | Decision | Why |
|---|---|---|
| How to keep biased and unbiased weights apart | **One set of accumulators, one integer subtraction** | The bias is a known constant per edge (0 or −1) sitting in an *exact* integer, so `E = accumulator − bias` recovers the true value with no second walk, no re-evaluation and no drift to unwind. Coverage and interpolation then read the same three numbers one line apart, which is the clearest possible statement that they are different questions. |
| How to demonstrate the bias | **Derive its magnitude; show that the damage is unsamplable** | The displacement is `1/‖e‖` px — 0.088 on the demo triangle. Sweeping the stripe frequency gave wrong-pixel counts of 4, 0, 0, **15**, 4, 0 with the error never changing. So the demo sweeps the band count live rather than showing one fixed impressive number, and the lesson teaches *derive the magnitude, do not look for it*. |
| The float-drift argument | **Walked back after measuring** | I expected float-stepped weights to drift visibly. Measured 4.94e-6 over 4,000 adds = 0.0013 of a colour level. The honest case for integers is exactness, reproducibility and equal cost — not an artifact. Recorded in LEARNINGS.md: overselling a real principle with a symptom the student cannot reproduce teaches them to distrust the principle. |
| Where to put the attributes | **`struct vertex`, for correctness** | Reorientation swaps two vertices; swapping loose coordinates and forgetting loose colours yields the right shape in the right place shaded one corner out of step, for one winding only. A struct plus `std::swap` makes it unwritable rather than documented. |
| Shipping the wrong blend | **`blend_space::encoded`, defaulting off** | Third time this bargain has been made (2.1's `draw_line_naive`, 1.8's naive collision). The wrong option must be asked for by name; the right one is what you get by not thinking. |
| Generalising the loop | **Deliberately not yet** | The demo's uv fill is a second copy of the loop. Two copies is not evidence; four would be. Module 3 grows `vertex` as attributes earn their place, Module 4 hands it to GPU varyings — a general mechanism written now would be designed against guesses and deleted unread. |
| The `pow` cost | **Measured, bounded, deferred** | 232 µs/triangle = 11.2 ns/px = 11.5× the flat fill, nearly all in `linear_to_srgb`. A 4096-entry table is under 0.4 levels of error everywhere. Left as Exercise 2.4.3 — optimising 232 µs inside a 16.6 ms budget is optimising by reflex, which Module 3's profiling lesson exists to teach against. |

### Verification

A scratch harness produced every number in the lesson (18 checks, all passing), and the demo was
rendered **headlessly** by `#include`-ing `main.cpp` into a scratch translation unit — its helpers
live in an anonymous namespace — so the screenshots came from the shipped code. It printed the
predicted 156 and 85 before any of the prose was written.

Two mistakes `check-page.js` could not see, both caught by actually looking at the figures:
Figure 4's iso-lines sprawled outside the triangle (the exact defect 2.3's demo had to fix), and
Figure 1's value label crossed a dashed bar. Fixed by computing the iso-line endpoints **on the
triangle's edges** instead of eyeballing them.

---

## 2026-07-24 — Lesson 2.5

> next

Resumed from `STATE.md` → `next: 2.5 — Matrices as Basis Transforms`, honouring the two standing
instructions it recorded: derive the matrix from *where the basis vectors land*, and **do not**
introduce homogeneous coordinates.

### Judgement calls

| Question | Decision | Why |
|---|---|---|
| How to store a `mat2` | **Two `vec2` columns**, not `float[4]` | The columns are the images of the basis vectors — the whole lesson — so the type makes the idea structural. Column-major layout then falls out for free (which is what SDL_GPU and HLSL want, so no transpose at the Module 4 boundary), and the arithmetic can be written as its own derivation: `c0*x + c1*y` and `{a*b.c0, a*b.c1}`. The row-times-column form compiles the same and cannot be checked by reading it. |
| Whether to fix translation | **No — leave it broken, loudly** | `T(origin) = origin` falls out of the scaling rule with `c = 0`, so no 2×2 can translate. Stated, proved in one line, and left open on purpose: 2.7 earns the fourth component from this gap, and Exercise 2.5.3 has the student rotate about a point the painful way first. |
| How to demonstrate non-commutativity | **Draw both orders at once** | The demo ghosts the reversed composition as a gold outline, so turning the angle makes the two shapes visibly separate. A mode you flip between would make the reader hold two pictures in memory; overlaying them makes the difference the thing you are looking at. |
| Whether to trust the determinant | **Measure it with our own rasterizer** | Fill the transformed unit square with `fill_triangle`, count lit pixels, compare against `side²·|det|`. Identity/scale/shear exact, rotation −0.26%. Also forced an honest note about *why* the residual exists (pixel centres → error scales with perimeter → falls as ~1/side), which became Exercise 2.5.4. |
| The demo glyph | **An F** | A blob or a circle is symmetric and cannot show a reflection — and reflection is the entire point of the determinant's sign. |
| Where the y-flip lives | **One function, `to_screen`** | `mat2` is y-up (maths convention, rotation is CCW); the framebuffer is y-down. Baking the flip into the matrices would leave "which way does `rotation()` turn?" permanently ambiguous. One minus sign at the boundary, which 2.11 will name. |

### Verification

A scratch harness covered seven areas (all passing) and supplied every number in the lesson: the
worked example, `R(a)R(b) == R(a+b)` over 1,716 pairs, `det ≡ edge_function` over 28,561 matrices,
the pixel-counted areas, both composition orders, the zero-determinant collapse (all 1,681 sampled
inputs land on one line), and reflection reversing signed area. The demo was rendered headlessly by
`#include`-ing `main.cpp` into a scratch TU, so screenshots came from shipped code.

**A defect the tooling could not see, for the second lesson running.** The lattice loop skipped
`i == 0`, dropping the images of `x = 0` and `y = 0`, so the origin cell read as double size and the
unit square appeared not to line up with the grid — the exact opposite of what the figure argues.
`check-page.js` was green. Found by screenshotting the view. Now recorded in LEARNINGS.md as a
standing budget item rather than a thing to remember.

---

## 2026-07-25 — Lesson 2.6

> next

Resumed from `STATE.md` → `next: 2.6 — Building mat4 by Hand`, with its two standing instructions:
build `mat3` first so the idea carries over with nothing new to learn, and build `mat4` as "a `mat3`
plus a spare column", show the spare column does nothing, and **stop** — 2.7 is where `w` earns its
keep, and the payoff dies if the gap closes early.

### Judgement calls

| Question | Decision | Why |
|---|---|---|
| How much to re-derive in 3-D | **Almost nothing** | Lesson 2.5's argument never counted the axes, so every rule held. The lesson says so explicitly and spends its length on the two things that *are* new: rotation needing an axis, and the inert fourth column. Re-deriving would have implied the 2.5 proof was weaker than it is. |
| How to teach the three rotations | **From the cycle x → y → z → x** | Rotating about an axis turns the next two, one toward the other. That is why `R_y`'s minus sign sits below the diagonal while the other two have it above — the cycle wraps, but the matrix lists x's row above z's. Deriving it takes thirty seconds and is right; copying the shape of the other two is faster and wrong. |
| How to show non-commutativity | **Use a point lying on one of the axes** | `(1,0,0)` is unmoved by `R_x`, so doing `R_x` first provably wastes it and only the second rotation acts. One of the four steps doing literally nothing makes the asymmetry obvious rather than arithmetic. |
| How to verify "volume factor" | **Count lattice points inside the transformed unit cube** | The 3-D analogue of 2.5's pixel counting, and much tighter (0.00–0.01% at 120 steps/unit, since that is millions of samples). It also gave `inverse(mat3)` a real job — a point is inside iff `M⁻¹p ∈ [0,1)³`. |
| Whether to explain `w` | **No — diagnose and stop** | The lesson ends on the precise diagnosis: a matrix can only scale each column by a component of the input and add, so adding a *constant* needs a component that is always the same number. Naming the mechanism without assigning the value keeps 2.7's four questions (Exercise 2.6.4) intact. |
| Where to put the cube demo | **In `main.cpp`, making it five demos and 1,600+ lines** | STATE said the accumulating awkwardness *is* the Module 5 argument and should stay loud. The lesson now points at the file explicitly, so the listing's length is evidence rather than an accident. |

### Verification

A harness covering six areas, all passing, supplied every number: the worked example,
`rotation_z`'s top-left 2×2 matching `mat2` over 121 angles, the lattice-counted volumes,
`M * inverse(M) == I` over 300 matrices, the 4×4's exactly-zero displacement, and 500 matrices none
of which moved the origin.

**Two real defects.** `mat3::inverse` was wrong on first writing — transcribed straight into
column-stored members, two of nine cofactors used the wrong component. The rewrite names elements
in written notation first, fixing the class rather than the instance; the `M*inverse(M)==I` check
caught it immediately. And Figure 1's vector walk was drawn collinear with the x axis in dim grey,
making it invisible — the third lesson running where `check-page.js` was green over a diagram that
failed to make its argument. Both now in LEARNINGS.md.

---

## 2026-07-26 — Lesson 2.7

> next

Resumed from `STATE.md` → `next: 2.7 — Homogeneous Coordinates and What w Really Means`, which
specified the spine precisely: open with 2.6's diagnosis verbatim, answer four questions (set up by
Exercise 2.6.4), keep points-vs-directions as the through-line, and make the milestone "2.6's demo
starts working with no change to `mat4.hpp`".

### Judgement calls

| Question | Decision | Why |
|---|---|---|
| How to justify `w = 1` | **As the multiplier on the translation column** | "Because the arithmetic works" is not an argument. Measuring what other values do — `w` of 0/0.5/1/2 applies none/half/one/two of the offset — shows 1 is *forced*: it is the only value that applies `t` exactly once. That reframes `w` from a convention to a quantity. |
| How to sell `w = 0` for directions | **Show the error scaling with distance** | 10.77 → 107.70 → 1077.03 as the object moves ×1/×10/×100. The error *equals* the translation, so the bug is invisible at the origin and ruinous in a real level — the worst possible detection profile, and a much stronger argument than "directions are different". |
| How far to go on perspective | **Name it, show four numbers, stop** | Bottom row `(0,0,−1,0)` gives `x/w` = 1.0, 0.5, 0.2, 0.1 at increasing depth. That is enough to show `w` is not bookkeeping. Deriving projection here would spend 2.10's lesson and skip the similar-triangles argument that makes it inevitable. |
| Named constructors vs separate types | **`point()` / `direction()`, not two types** | The type-safe design does eliminate the bug at compile time, but roughly doubles the maths library's surface and collapses at the GPU boundary anyway. Exercise 2.7.5 argues the other side properly rather than the lesson pretending it is settled. |
| What the demo should show | **Two cubes, opposite composition orders** | `translation(-p)*R` spins in place; `R*translation(p)` orbits. That is Exercise 2.5.3 answered on screen *and* Lesson 2.5's "order matters" with translation finally in play. `[W]` collapses both onto the origin — 2.6 reproduced in one keystroke — and `[N]` breaks only the direction arrows while leaving the cubes perfect, which is why that bug survives review. |

### Verification

Five harness sections, all passing, supplied every number: the `w` multiplier table, the direction
error scaling, 40 affine compositions leaving the bottom row exactly `(0,0,0,1)`, 2.6's failing case
now moving exactly 1.2, and rotate-about-a-pivot preserving the pivot and distances.

**A diagram defect, for the third lesson running.** Figure 1 drew a room before and after moving,
with a lamp that should move and an arrow that should not — but both rooms were drawn identically,
so relative to the room nothing changed and the reader had to trust the labels. `check-page.js` was
green. Fixed with an identical ruler under both copies. Generalised in LEARNINGS.md: a before/after
figure needs something in it that provably did not change.


## 2026-07-27 — Lesson 2.8

> Based on the STATE and the project's claude instructions, work on the next.

Resumed from `STATE.md` → `next: 2.8 — The Space Chain: Model to World`. STATE specified the spine:
make the real subject a *discipline* (always know which space a coordinate is in), build the first
`model → world` link, introduce a `transform` struct, derive the `T·R·S` order rather than assert
it, and carry one vertex through by hand with a space-chain diagram annotated with real numbers.

### Judgement calls

| Question | Decision | Why |
|---|---|---|
| How to justify the T·R·S order | **Derive it from three sentences about meaning** | Scale is along the object's own axes, rotation about its own origin, translation is a world statement — each sentence forces one factor's position. A derivation the student can re-run beats a mnemonic they can transpose. |
| How to prove "deformed" vs "moved" | **Measure the angle between transformed axes** | The projection can make a rigid object look sheared. Transforming `x̂`/`ŷ` as directions and checking the dot product is zero is a test that reads off the matrix and cannot be fooled by the view. |
| Which objects to put in the scene | **One that breaks, two controls** | A uniform-scale post and an un-rotated plinth are *bit-identical* under the wrong order. Showing that two of three objects look perfect is the whole point: it is why the bug ships. |
| `parent_from_local` vs `world_from_local` | **`parent_from_local`** | Module 5's hierarchy widens "parent" without changing a character; naming for the world today would force a rename or leave a lie. Not speculative generality — no hierarchy machinery ships, only the name declines a known-temporary assumption. |
| How to write the model matrix | **Three scaled rotation columns, not `R*scale()`** | Identical floats, but it *states* "column k = axis k × size k" (Figure 5's claim) in code rather than leaving it to be re-derived — and it is 9 muls, not 27. |
| The ground plane's projection | **An oblique-z cabinet projection, flagged as a stopgap** | Dropping z (2.6) collapses the floor to a line; real perspective is 2.10. Naming the expiry date is the honest ninety-percent move. |

### Verification

Two scratch harnesses supplied every number the page prints: the worked vertex `(0.5,0.5,-0.5) →
(2.5,1.25,-5.0)`; T·R·S holding 90.000° and |x|=sx at all angles; T·S·R reaching 157.993° at 45°
with |x| sweeping 0.35–1.8; R·T·S orbiting (same 5.099 distance, wrong place); uniform/still controls
bit-identical over 360° (worst diff 0.000e+00); and a layout check confirming all three objects fit
the framebuffer panel in every order. The interactive widget's numbers were checked against the
harness in-browser at 45° and 90°.

**Three SVG defects, caught by `check-page.js`, missed by the eye.** First render had a label
spilling the right edge of Fig 1, a bottom-edge spill and colliding captions in Figs 2–3, and text
sitting on connector curves in Fig 5. Fixed by repositioning, shortening, and removing Fig 5's
connectors (colour already links each column to its axis). Re-ran to `pass: true` and verified both
themes. New LEARNINGS entries: the degenerate-case invisibility of transform-order bugs, proving
rigidity with a measurement rather than a picture, and naming for the code you are going to write.


## 2026-07-28 — Lesson 2.9

> next

Resumed from `STATE.md` → `next: 2.9 — The View Matrix: Deriving Look-At`. STATE carried an explicit
open question to settle first: look-at needs a cross product, but the course plan filed the cross
product in Lesson 3.4.

### Judgement calls

| Question | Decision | Why |
|---|---|---|
| Cross product in 2.9 or 3.4? | **2.9** | There is no honest way to build an orthonormal camera basis without it, and hand-rolling it unnamed would violate "derive, don't decree". Introduced here; 3.4 still *deepens* it (triangle normal + signed area). Revised `vec3.hpp`'s deferral comment and the toolbox label. |
| Add a general 4×4 inverse? | **No** | A camera is rigid (no scale), so `look_at` writes the closed-form inverse `Rᵀ, −Rᵀeye` directly. The general inverse still waits for a genuine need — same discipline the cross product followed until now. |
| Where does `look_at` live? | **`mat4.hpp`** | It is a matrix factory taking eye/target/up, like `translation`/`affine` and (next lesson) `perspective` — not a `transform` operation, so not `transform.hpp`. |
| What happens to the projection? | **`to_screen3` → plain orthographic of view space** | 2.8's oblique hack was a fake-3D stopgap; a real movable camera replaces it. Dolly (`[-]`/`[=]`) is a deliberate no-op under ortho, HUD-labelled — and it sets up 2.10's "distance must matter". |
| Frame the camera how? | **Camera = object with a transform; view = inverse of placement** | Makes "move camera left = move world right" literally true, and reuses the whole `a_from_b` / rigid-inverse machinery instead of presenting look-at as a magic formula. |

### Verification

One scratch harness, every section passing, supplied every number: the cross product's algebra and
area magnitude; the worked camera's orthonormal basis; eye→origin and target→(0,0,−d); the worked
point `(0,3,0) → (0, 1.94, −7.76)`; **`V · world_from_camera == I`** (the inverse, confirmed by
arithmetic); the "+2 eye → −2 view-x" see-saw; scene fit at five camera angles; and the
look-straight-up singularity (degenerate `right`).

**Six SVG defects across three figures, caught by `check-page.js`.** A spilling caption, a label
overlap, and four labels sitting on vectors/lines — plus the interactive widget's readout clipping
the viewBox edge. Fixed by moving labels off lines, splitting Fig 1's divider around the bridge
label, and compacting the widget readout. Re-ran to `pass: true`, both themes verified in a real
browser. New LEARNINGS: introduce math where first needed (not where a plan filed it); a projection
can make a rigid thing look sheared, so verify frames numerically; and the left-handed-basis mirror
trap from a swapped `cross` argument order.


## 2026-07-29 — Lesson 2.10 (the keystone)

> next

Resumed from `STATE.md` → `next: 2.10 — Perspective from Similar Triangles`, with STATE carrying an
explicit open question: does `xyz()` start dividing, or does a separate `perspective_divide()` keep
drop-vs-divide honest?

### Judgement calls

| Question | Decision | Why |
|---|---|---|
| `xyz()` divides, or a new function? | **Separate `perspective_divide()`** | Keeps 2.7's drop-vs-divide honesty; `xyz()` still drops. Two names mean a call site declares intent — a silent wrong choice is a bug that looks like a tuning problem. |
| Derive before the matrix? | **Similar triangles + a pinhole diagram first** | CLAUDE.md §4 is emphatic. The matrix is decreed nowhere: `x' = n·x/(-z)` comes from a picture, then "a matrix can't divide" motivates every remaining entry. |
| Show the artifact? | **A `[P]` perspective/orthographic toggle** | Reuses 2.9's now-orthographic demo as the "before". One matrix, one draw path, so the toggle isolates exactly what perspective adds — the same honesty as 2.4's coverage counter. |
| Which NDC target? | **SDL_GPU: `z ∈ [0,1]`, `+y` up, flip inside P** | The NDC-parity decision: target the GPU's clip space now so the Module 4 port is API-only. Flagged `⚠ VERIFY` against the SDL wiki. |
| Where does `perspective` live? | **`mat4.hpp`**, beside `look_at`/`translation`/`affine` | Matrix factories together. The orthographic comparison matrix stays demo-local — nothing else needs it until 2.11 owns viewport/ortho. |
| Near plane value? | **0.3** | Verified the orbit+dolly range never pushes a vertex closer than ~1.7 units, so no geometry crosses near — sidesteps clipping (3.3) honestly rather than pretending. |

### Verification

One harness, every section passing, supplied every number: the similar-triangles table reconnecting
to 2.7 (`1.0, 0.5, 0.2, 0.1`); the projection matrix entries (`f=√3`, `A=B=-1.0101`); depth
near→0/far→1 and its non-linearity (`z=-2 → 0.505`); the worked point `(2,1,-10) → ndc
(0.195,0.173,0.909)`; and the demo scene fitting the panel and staying in front (`min w = 1.74 >
near 0.3`) across the whole camera range. The default-frame probe chain became the Expected Result
HUD.

**Tooling note worth keeping:** the in-app Browser pane reported `clientWidth: 0`, which made
`check-page`'s layout checks fire ~17 false spills and a false `pageScrollsX`. Fix: `resize_window`
to a real size *after* each `navigate` (it does not persist). The pane's screenshots came out blank
(capture size ≠ DOM viewport), so real screenshots came from the **Playwright** MCP — consistent
with the standing memory to verify with Playwright, not the preview pane. New LEARNINGS: the matrix
can't divide so perspective defers (why `w` exists); depth is `1/z`-nonlinear and the near plane is
the precision knob; and A/B toggles should route both sides through one path varying one input.


## 2026-07-30 — Lesson 2.11

> next

Resumed from `STATE.md` → `next: 2.11 — The Viewport Transform`, which had already framed the job:
the demo had been mapping NDC to pixels with three hand-picked constants and a `-ndc.y`, underived
and homeless, and STATE left one decision open — engine type or demo-local for one more lesson.

### Judgement calls

| Question | Decision | Why |
|---|---|---|
| Engine type or demo-local? | **Engine — `src/gfx/viewport.hpp`** | 2.11 is literally the viewport's lesson, and the type is what gives the y-flip one home. Header-only, so no CMake churn (same precedent as `math/transform.hpp`). Placed in `gfx/` because it is the framebuffer boundary. |
| Mirror `SDL_GPUViewport`? | **Yes, field-for-field** | Module 4 then copies fields rather than translating, and any convention mismatch surfaces now. **Verified against the fetched `SDL3/SDL_gpu.h`** (x/y are the left/top offset) rather than assumed, and flagged `⚠ VERIFY` in the text. |
| How to write the flip? | **`(1 - t_y)`, not `-ndc.y`** | Algebraically identical; the first *states* that it reverses a fraction, the second just looks like a mysterious negative. The lesson's whole point is that this sign gets derived, not carried. |
| HUD room for a sixth space? | **Drop 2.9's R/U/B camera-axis rows** | The panel was full. The complete six-space chain is the module's climax and earns the space; the camera is still identified by eye + orbit params. |
| Should the picture change? | **No — and prove it** | Swept an NDC grid through both the old constants and the new viewport: worst difference `0.000e+00`. For a "give this a name" refactor, an unchanged image *is* the acceptance test. |

### Verification

The harness covered the teaching viewport (800×600: NDC top-left → pixel (0,0), centre → (400,300),
worked point → (600,225,0.9)), the bit-identical refactor, the default probe's full chain to
`screen(81.78, 75.47, d=0.959)` with y above centre because `ndc.y > 0`, and the min/max-depth
pin-in-front trick. The interactive widget was checked in-browser at four NDC points.

One real diagram defect caught by `check-page.js` (a caption line starting at x=392 spilled the
viewBox); moved to the left margin, re-ran green. Two standing environment notes reconfirmed: the
static server dies between turns, and the Playwright viewport must be resized to 1280×900 *after*
each navigate or the layout checks fire false spills. New LEARNINGS: a convention living in twelve
places will eventually be wrong in one (and the tell is that each copy needs a comment); and design
types to shadow the API you will port to.


## 2026-07-31 — Lesson 2.12 (Module 2 milestone)

> next

Resumed from `STATE.md` → `next: 2.12 — Milestone: A Spinning Wireframe Mesh`, which had left the
mesh choice explicitly open and warned off the OBJ loader (CLAUDE.md reserves it for 3.5).

### Judgement calls

| Question | Decision | Why |
|---|---|---|
| Which mesh? | **A hand-derived icosahedron** | Exactly derivable from φ, so "derive, don't decree" holds; tiny (12 verts / 20 faces); 5× vertex reuse makes indexed geometry concrete; and unlike a cube it is not axis-aligned, so rotation reads richly. A UV sphere would have been rote trig; the OBJ loader belongs to 3.5. |
| Store triangles or edges? | **Triangle indices** | A triangle list is what a mesh *is* — 3.x fills them, 3.4 culls them, Module 4 uploads them. An edge list would discard face data and need rebuilding two lessons later. Cost: each shared edge drawn twice (60 draws / 30 edges), named honestly rather than hidden. |
| Owning or non-owning? | **`std::span` — non-owning** | Correct for `inline constexpr` geometry with program lifetime, and *incorrect* the moment meshes load at runtime — which is exactly the pressure that justifies Module 5's asset system. Said so explicitly rather than pretending the design is final. |
| Mesh inside `transform`? | **No — a separate `scene_object`** | A transform is a placement, not a thing. Keeping them separate is the shape Module 5's ECS formalises (transform component + mesh component, attached independently). |
| How much new theory? | **Almost none; consolidation is the content** | It is a milestone. One new idea (indexed geometry), then the φ derivation, mesh validation, a six-space walk of one real vertex, and an honest "What Is Still Missing" section handing each gap to its Module 3 lesson. |

### Verification

The harness validated the shipped mesh data rather than trusting it: icosahedron `V=12, E=30, F=20`
→ Euler 2, every undirected edge in exactly two faces, every *directed* edge exactly once, **all
twenty faces wound outward**, degree uniformly 5, radius exactly `1.000000`, all thirty edges
`1.051462` (= `2/√(1+φ²)`, as derived). The cube's `8 − 18 + 12 = 2` also checks out — 18 edges, not
12, because triangulating each square face adds a diagonal. A camera sweep (az 0–360 × elevation
±1.4 × radius 4/7/14) set the dolly minimum to 4.0, the point at which the whole scene stays inside
the viewport. The in-page widget was confirmed to emit exactly 60 lines.

`check-page.js` caught two Figure 2 labels sitting on the connector curves; moved clear and re-ran
green. New LEARNINGS: hand-authored geometry is *data*, so validate it mechanically (and note that a
winding error is **invisible** in a wireframe until 3.4 turns on culling — the cheapest moment to get
a convention right is before it has a consumer); and the saving from indexed geometry is *work*, not
bytes, which is why the loop order (transform all vertices, then walk indices) is the load-bearing
part.

**Module 2 is complete** — the geometry pipeline is finished and every hop derived.


---

## 2026-08-01 — Lesson 3.1 (Module 3 opens)

> Based on the STATE and the project's claude instructions, work on the next.

Resumed from `STATE.md` → `next: 3.1 — The Painter's Problem and the Z-Buffer`, which left **two
decisions explicitly open**: where the depth buffer lives, and whether 3.1 connects `fill_triangle`
to the 3-D pipeline or stays wireframe-plus-depth.

### Judgement calls

| Question | Decision | Why |
|---|---|---|
| Depth buffer inside `framebuffer`, or beside it? | **Beside it, as `engine::depth_buffer`** | Decided by the hardware, not by taste: `SDL_BeginGPURenderPass` takes colour targets as an *array* and the depth-stencil target as a *separate, nullable* parameter (verified in `SDL3/SDL_gpu.h`). Also: 2-D demos, Pong, the HUD and Module 6's post-process intermediates are colour-only and would each pay 8 MB at 1080p for nothing; and the two have different formats and unrelated clear values. |
| Should the buffer own the depth test? | **No — the rasterizer compares** | `SDL_GPUDepthStencilState` carries `compare_op`, `enable_depth_test` and `enable_depth_write` as three independent pipeline knobs, because a shadow pass writes depth and no colour while a transparent pass tests depth and does not write it. A `test_and_set()` would bake one policy into storage and make those unexpressible. |
| Wireframe + depth, or fill? | **Fill** | A wireframe writes almost no pixels, so depth-testing one is a half-measure; and `fill_triangle` had existed since 2.2 without ever meeting the coordinate pipeline. Filling also makes the z-buffer's own demonstration possible: with depth testing and *no* culling, front faces beat back faces and a correct solid appears — which is precisely the point that culling (3.4) is an optimisation while the z-buffer is the correctness mechanism. |
| Two overloads, or a nullable pointer? | **`fill_triangle(fb, depth_buffer*, …)`** | Two overloads would duplicate the fill's set-up — the bias, six steps, three starting values — and `raster.cpp` already argues that duplicating something *subtle* is how one bias ends up wrong in one of three places. The nullable non-owning pointer is not a breach of the no-raw-owning-pointers rule: that rule is about ownership, and what a reference cannot express is *optionality*. It is also literally the shape `SDL_BeginGPURenderPass` uses. |
| Where does `z` go in `vertex`? | **Before `colour`, breaking every call site** | `Uint32` → `float` is narrowing in list-initialisation, so the old three-argument form is a **compile error** rather than a colour landing silently in the depth field. Aggregate initialisation earning its keep; a constructor taking `(int,int,Uint32)` would have compiled and produced garbage. |
| Model depth *formats*? | **Yes — `depth_format {f32, unorm24, unorm16}`** | §3.6 is not an aside; precision is a decision the type should be able to express, and it mirrors `SDL_GPU_TEXTUREFORMAT_D32_FLOAT / D24_UNORM / D16_UNORM`. Storage stays `float` and we round to the format's grid on write — the *behaviour* (z-fighting) is exact, the memory saving is the part not modelled, and the lesson says so. |

### Verification

Two harnesses, both under `scratch/`. `verify_31.cpp` checks the mathematics: over 199,273
well-conditioned random triangles, interpolating **device depth** is wrong by at most
`8.2 × 10⁻¹³` (double-precision noise) while interpolating **view-space z** is wrong by up to
**5.73 units — 243.6%** of the true depth. The worked example checks out exactly: `near = 1`,
`far = 100`, `A = B = −100/99`; the screen midpoint of a near-to-far edge is at `w = 1.980198`,
where linear interpolation of device depth gives **exactly** the true `0.5`, and linear
interpolation of view z gives `−50.5` against a truth of `−1.98` — **25.5× too far**.

`verify_31_render.cpp` links the real rasterizer and measures what the demo actually draws: the
cycle scene's three planks sit at exactly `±0.55` at each shared corner (A over B over C over A,
determinant `+1.000000`); painter-vs-z-buffer disagreement is **144 px** on the cycle and **162 px**
on the intersecting pair; the near-coplanar panels lose **478 of 875** covered pixels (54.6%) at
`D16_UNORM` and **zero** at `D24_UNORM` and `D32_FLOAT`; and the milestone scene occupies
`[0.95032, 0.96200]` — **1.17% of the depth range**.

Two things the harness changed. The cycle scene originally disagreed on only **7 pixels** — correct
geometry, invisible artifact — and a parameter sweep moved it to 144. And the disagreement counter
never read zero on the *solids* scene; the silhouette-tie explanation was plausible, so it was
tested rather than believed: culling back faces gives `6 of 12 tris kept, 0 px differ`, which is now
stated as fact in the pitfalls and handed to 3.4 by name.

`check-page.js` returned `pass: true`, but caught nothing that eyes did not: **Figure 2's near/far
labels were inverted relative to its own geometry** (the eye is at the bottom, so the *lower* line
is the nearer one) — a correctness bug in a diagram, found by reading the screenshot against the
coordinates. The lint also surfaced a genuine shared-CSS gap: widget SVGs are not `figure.dia`
SVGs, so their `<text>` fell back to black and vanished in dark mode. Fixed in the template and
re-stamped across all 31 pages, which repaired 2.12's widget too.

**Module 3 is open**, and the geometry pipeline now produces solids.

---

## 2026-08-02 — Lesson 3.2

> next

Resumed from `STATE.md` → `next: 3.2 — Perspective-Correct Interpolation`, which had flagged one
open decision (whether `vertex` should carry `w` or `inv_w`) and one free regression test (depth
must not move).

### Judgement calls

| Question | Decision | Why |
|---|---|---|
| `w` or `inv_w` on the vertex? | **`inv_w`, pre-divided** | The inner loop interpolates `1/w`, so storing `w` would mean a divide per vertex *and* the same divide again per pixel; and the divide-back at the end becomes a multiply by a reciprocal we had to compute anyway. Defaulting it to `1` also makes the 2-D path exactly correct with no special case — `w = 1` is the orthographic case, where the correction is the identity. |
| Two loops, or one? | **One, with affine as `inv_w = 1`** | Affine interpolation *is* the correct arithmetic with every `1/w` forced to 1 — a perspective renderer doing orthographic interpolation. Writing it that way is shorter than duplicating the fill's subtle set-up and it says the thing. Cost: a redundant divide in a mode that exists only to be shown failing. |
| Where do the growing knobs go? | **A `fill_style` struct** | `blend_space` (2.4) was about to be joined by interpolation and shading, with a lighting term due in 3.6. Not merely tidiness: it is the pipeline-object shape, and `SDL_GPUGraphicsPipelineCreateInfo` is this struct several times over. Every field defaults to the correct value, so the short call is the right call. |
| How does a uv become a colour with no textures yet? | **`shading::uv_checker`, admitted to be a placeholder** | A procedural debug pattern — which every engine ships, because it is how you check a uv layout before there is art. The header says outright that the enum stands in for a fragment shader, that 3.6 will strain it, and that Module 4 replaces it. Introduce the crude thing and let the strain motivate the right thing, the same arc `main.cpp` is on toward Module 5. |
| Interleaved or parallel uvs on `mesh`? | **Parallel span, empty = none** | Lets the cube and icosahedron carry no uvs and store none. Interleaving is what the GPU wants and what Module 4 revisits with the memory-layout diagram the decision deserves. |
| The floor spills outside the scene viewport | **Give the floor the whole framebuffer** | Measured rather than tuned: `scratch/fit_floor.cpp` swept every extent worth having and the near edge *always* projects outside the inset rect. That is geometry, not a tuning failure — a surface you stand on fills the bottom of your view, which is the same property that makes it a good subject. 320×180 is already 16:9, so no new projection is needed, and the change was to thread the viewport through as a parameter, which is what Lesson 2.11 built it to be. |

### Verification

`scratch/verify_32.cpp` links the real rasterizer. Over 199,241 random triangles carrying a random
attribute linear over their surface, perspective-correct interpolation is wrong by at most
`1.3 × 10⁻¹²` (double-precision noise) while affine is wrong by up to **9.89** on attributes whose
whole range is about ±13. The worked example checks out exactly: at the screen midpoint of an edge
running from `w = 1` to `w = 100`, the true `u` is `0.00990099`, affine gives `0.5` — **50.5× too
far** — and `0.005 / 0.505` gives the truth to every digit.

Two controls, both green. **Depth must not move:** over a triangle whose colours change on 20,590
pixels, the depth buffer differs on `0`. **The 2-D path must not move:** 17,275 covered pixels,
`0` differences between the two interpolation modes.

The tessellation sweep produced the lesson's best number and its most instructive mistake. The
pixel-difference count read 48.5%, 48.2%, 51.2%, 46.7% and then collapsed to 4.9% — which looks
like the error behaving oddly and is actually the *metric* saturating: two scrambled two-colour
images differ on about half their pixels no matter how much worse one gets. Measuring the worst uv
error instead gave `4.23 → 1.83 → 0.76 → 0.29 → 0.10 → 0.03`, from which second-order convergence
is obvious — and at 2,048 triangles the floor is still wrong on 381 pixels. Convergence is not
termination.

Both floor renders were dumped to PPM and looked at, which is how the widget's hairline gaps along
the split diagonal were caught: piece-based drawing straddled the two triangles' differing maps.
Rewritten with a `clipPath` per triangle — no gaps, fewer polygons, and the perspective path now
emits 13 elements regardless of subdivision, which demonstrates the lesson's point in the DOM.

`check-page.js` returned `pass: true` after four label collisions were fixed. It could not catch
the one that mattered: **Figure 5's caption still described a dashed line I had removed**, found by
reading the rendered figure against its own text.


## 2026-08-03 — Lesson 3.3

> Based on the STATE and the project's claude instructions, work on the next.

Resumed from `STATE.md` → `next: 3.3 — Near-Plane Clipping`. The block had already named the
artifact (orbit the floor and it vanishes, because `project()` returns invisible for `w ≤ 0.05`),
the fix (Sutherland–Hodgman in clip space at `z_clip ≥ 0`, *not* `w ≥ 0`), and one open decision:
whether the clipper needs its own vertex type.

### Judgement calls

| Question | Decision | Why |
|---|---|---|
| Does the clipper need its own vertex type? | **Yes — `clip_vertex`** | `engine::vertex` is a *screen-space* type: integer pixels, device depth, a pre-divided `inv_w`. Every field assumes the divide has happened, which is exactly what makes it the wrong type to clip with. One struct with a "divided yet?" flag would compile and silently produce nonsense; two types make that call a compile error. Third time this move has paid — `point()`/`direction()` (2.7), `xyz()`/`perspective_divide()` (2.10). |
| Clip triangles only, or segments too? | **Both** | The demo draws a great many lines — world grid, object axes, every wireframe edge — and a line crossing the near plane is exactly as broken as a triangle that does. It is also the honest way *in*: three cases you can hold in your head at once, before the polygon loop. Walking over a gridline no longer makes it blink out. |
| A new scene for the artifact? | **No — the existing floor** | STATE said the 1×1 floor *is* the wall-you-walk-into case, and the measurement agreed: dropping loses 31,747 of 57,600 pixels at the demo's default tessellation, and 100% at some camera angles. Adding a corridor scene would have been a larger diff for a weaker reason; it is Exercise 3.3.3 instead, with the harness numbers to compare against. The floor's dolly limit moved 4 → 1 so the camera can actually walk past the ground's near edge. |
| How do the three near modes travel through six drawing functions? | **A `projector` struct** | `proj` and `vp` were already a loose pair threaded through `line3`, `line3_world`, `draw_world`, `draw_mesh`, `draw_axes3` and `collect_triangles`, and this lesson was about to make it three. Same argument `fill_style` made in 3.2, one level up — and note the direction of the win: adding a knob made every call site *shorter*. |
| Where does the divide go now? | **Inside the triangle loop, after the clipper** | Costs a divide per triangle *corner* rather than per vertex — 60 instead of 12 on the icosahedron. Paid knowingly: the two-pass alternative (divide the unclipped, re-divide the clipped) is more code, more state, and wrong in ways easy to miss. Real hardware pays it too — vertex shader, clip, *then* divide. |
| The `none` mode can produce NaN inside the rasterizer | **Harden the engine, not the demo** | `float → int` is undefined out of range, and an interpolated `1/w` passing through zero reaches it. Three guards: `to_pixel` (clamp ±8000 — which also keeps `edge_function`'s products inside int32), `checker_at` (magenta on a non-finite uv), `linear_to_srgb_u8` (`!(linear > 0)`). The last is not demo scaffolding: Module 6's HDR pipeline pushes floats through the same function, and `std::clamp` *cannot* remove a NaN. |

### Verification

`scratch/verify_33.cpp`, nine sections, all green. The near plane is `z_clip = 0` to `1e-9` and
`−B/A = −0.30000001` against `near = 0.3`. Over 68,594 random straddling edges the clip-space
crossing parameter matches the view-space form `(z_a + near)/(z_a − z_b)` to `4.8e-7`, the new
vertex lands on the plane to `1.6e-6`, its uv matches an independently computed ray–plane
intersection to `2.3e-6`, and its `w` equals `near` to `1.7e-6`. All four Sutherland–Hodgman
configurations check out: 3 in → 3 out *bit-identical*, 2 in → 4, 1 in → 3, 0 in → 0.

The regression that mattered most: over **405** camera-and-tessellation combinations with nothing
straddling, clipping and dropping produce **0** differing pixels. A fix that changes the image
where there was no bug is not a fix.

Two things the harness corrected rather than confirmed. The winding check failed first, and the
clipper was innocent — the "before" area was computed from a triangle with a vertex behind the eye,
whose projection is exactly the garbage the lesson exists to prevent. Replaced with a continuity
test: slide a triangle through the plane and assert the sign never changes (502 clipped steps, both
windings, 0 flips). And the draft's claim that "no amount of subdivision removes the artifact" was
simply **false** — at 8×8 on a ground plane, dropping and clipping give bit-identical frames,
because the straddling strip is the one under your feet. The measured replacement is a better
argument: 2 triangles lose the whole frame, 512 still lose 51%, and it takes 8,192 before the hole
is finally pushed off every reachable view.

`check-page.js` returned `pass: true` after twelve label collisions and two spills were fixed. It
could not catch the one that mattered: **Figure 6's text said "the gold dot" about a marker that is
green**, found by rendering the figure and reading it against its own prose. Also caught by eye —
the widget's view frustum was drawn far outside its panel (fixed with a `clipPath`) and its "eye"
label sat underneath a draggable handle.


## 2026-08-04 — Lesson 3.4

> next

Resumed from `STATE.md` → `next: 3.4 — Back-Face Culling`. The block carried a measured debt from
Lesson 3.1 (29 px of painter-vs-z-buffer disagreement on the solids scene, with "cull back faces and
it reads 0" already *tested*), two named traps (which sign, and where to ask), and a verification
list.

### Judgement calls

| Question | Decision | Why |
|---|---|---|
| Where does the cull test live? | **In `fill_triangle`, between the area measurement and the reorientation** | The swap to positive area *destroys* the facing, so there is exactly one window and this is it. It is also where hardware runs it — after clipping and the divide, before rasterization — so the Module 4 port stays a rename. |
| Inline `area < 0`, or a named function? | **`is_front_facing`, in the header** | The rule now has two readers: the rasterizer that acts on it and the demo that counts it. Same argument `is_top_left` got in 2.4. And taking *screen-space* vertices makes the view-space bug unwritable against this API. |
| How does the HUD count culled triangles? | **In the caller, via the same `is_front_facing`** | `fill_triangle` could return a bool, but that puts a value at 100% of call sites that 99% ignore. Counting in `draw_triangles` costs one edge function per triangle and keeps the *rule* in one place — instrumentation may duplicate the question, never the answer. |
| Default for `fill_style::cull`? | **`none`** | Breaks 3.2's "every field defaults to correct" rule, deliberately: there is no universally correct cull mode. `back` is right for a closed mesh and destroys a ground plane. SDL_GPU lands identically — a zero-initialised `SDL_GPURasterizerState` is `CULLMODE_NONE`. |
| Per-object cull modes? | **No — a `closed` bool and a HUD warning** | The scene is one batch with one `fill_style`; per-object culling means splitting the draw. That friction *is* the lesson, and the answer is a material system (Module 6), not another parameter. |
| Where to put the wrong test? | **In `collect_triangles`, in view space** | Not for convenience — that is exactly where the bug lives in codebases that have it, because it looks like a sensible early-out. Seventh keep-the-wrong-thing bargain in this engine. |

### Verification

`scratch/verify_34.cpp`, eight sections, all green. The headline: **29 px → 0 px**, exactly as
Lesson 3.1 predicted, from an explanation it could only guess at.

The sign was measured rather than argued: a triangle that is provably counter-clockwise in NDC
(signed area `+0.18`) comes through the real viewport as pixels whose `edge_function` is
`−5184`. The determinant identity `dot(n,a) == det[a,b,c]` holds to `1.06e-6` over 399,935
triangles, and the ray test and the *unrounded* screen-area test disagree on **zero** of them —
768 (0.19%) disagree after rounding to integer pixels, every one a sliver at most **1.71 px** wide,
and **zero** among triangles at least 4 px wide. The forward-axis test misjudges 15.46% of triangles
at 55° fovy, 32.43% at 120°, and **0 of 200,000** under an orthographic projection.

### Three things the harness corrected

**"Exactly half the triangles" was folklore.** Measured: a cube shows **2 to 6** of its 12 (mean
5.55), an icosahedron **7 to 10** of 20 (mean 8.80). Never more than half, usually fewer — the eye
can lie *between* a parallel face pair's planes and see neither. Look a cube square in the face and
you see one face, not three.

**"Culling is invisible on closed geometry" was also false.** It changes up to 44 px over 1,008
camera/rotation combinations. Rather than tune a geometric threshold, I found the claim that needs
none: **100% of the changed pixels are pixels a back face had been drawn on**. Drawing front faces
first drops the worst case to 29 px (the tie), and the residue is quantisation (the two outlines
round independently). So culling is an optimisation *and*, marginally, a correctness improvement.

**And a test-design error of my own.** The identity check "failed" at 1.1e-4 because I normalised
by the magnitude of the *result* — and the triple product is a cancelling difference, so that ratio
is unbounded near degeneracy. Normalising by `|a||b||c|`, the size of the terms, gives 1.06e-6.

The timing is reported honestly: 54.8% of triangles removed bought 31.6% of the frame
(19.95 → 13.64 µs), because back faces were the ones the z-buffer was already rejecting cheapest.

`check-page.js` returned `pass: true` after nine label overlaps and nine labels-on-strokes were
fixed. It could not catch the one that mattered: **Figure 6 had the front and back arcs on the wrong
sides** — the eye is drawn on the left, so the solid front surface must bulge left, and my SVG arc
sweep flags said otherwise. Found by rendering the figure and reading it against its own labels.

---

## 2026-08-05 — Lesson 3.5

> Based on the STATE and the project's claude instructions, work on the next.

Resumed from `STATE.md` → `next: 3.5 — A Hand-Rolled OBJ Loader`. The block was unusually explicit:
parsing is an afternoon and must not be the lesson; the **index problem** is the spine; and two
debts were named for collection (3.3 §3.9 — you cannot subdivide your way out of a bug in geometry
you did not author; 3.4 §3.6 — you cannot assume its winding either).

### Judgement calls

| Question | Decision | Why |
|---|---|---|
| Ship a third-party model? | **No — author our own, and ship the generator** | `make_torus()` in `mesh.cpp` plus `save_obj()` produces `assets/torus.obj`, so the course ships no third-party geometry, every number in the lesson is reproducible, and the writer buys a **round-trip test** a reader alone cannot have. A torus is also the right subject: non-convex, self-occluding, has a uv seam by construction, and its Euler characteristic is 0 rather than 2. |
| Which model does the demo compare against? | **The in-memory `make_torus()`, every frame** | `assets/torus.obj` *was* written from that call, so "loaded == generated" is an end-to-end check of writer and reader together, in the currency this module has used for every claim it makes: **0 px differ**. |
| Return `Result<T,E>`? | **No — an enum, a line number and a counts struct** | Building a general result type here would be inventing a language feature for a problem we have exactly once. Module 5 has ten loaders wanting the same shape; that is when the shape earns a name. |
| Fatal or forgiving on bad input? | **Malformed is fatal, silly is counted** | `f 1/x/2` is not an OBJ file and guessing helps nobody; a zero-area face is a perfectly good file describing a silly triangle, and real files contain them. Both halves appear in the report. |
| `try_emplace` for the de-dup map? | **`find` then `emplace`, two lookups on a miss** | `try_emplace` needs the new index as an *argument*, so `uint16_t(65536)` — a silent 0 — is computed before the ceiling test can run. The value is never used, but correctness resting on "we return before that matters" breaks on the next edit. |
| Parse floats with `SDL_strtod`? | **No — `std::strtof`** | Checked the header: SDL's is documented to make *fewer* guarantees than the C runtime's, with scientific notation explicitly unspecified. Exporters emit `1.0e-5` constantly. `from_chars` is the principled answer and is guarded behind `__cpp_lib_to_chars` as Exercise 3.5.4. |
| Load normals nothing reads yet? | **Yes** | The file has them, re-parsing later is worse, and — the real reason — a normal *participates in deciding what a vertex is*. Without them the cube would not split into 24 and the lesson would have no evidence. |

### Verification

`scratch/verify_35.cpp`, eight sections, all green, plus `scratch/render_35.cpp` for offscreen
renders. Headlines: cube.obj **8 positions → 24 vertices (16 splits, 0 reused)**; the round trip is
bit-exact corner for corner and **0 px** on screen; the torus's χ is **0** and its signed volume
converges to `2π²Rr²` from below with clean second-order error; `twisted.obj` reports **4 reversed
edges** and volume `+0.667` instead of `+1.0`, and culling changes **2,459 px** on it versus **2 px**
on the correct cube.

### Three things the harness corrected

**My hand arithmetic on `quirks.obj` was wrong** — I predicted 15 unified vertices and the program
said 14, because I had mis-mapped the base quad's position indices. The program was right.

**Two normals genuinely coincide in `torus.obj`** — 1,152 `v` lines but only 1,150 `vn`. Not a bug:
on a torus the normal at `(u,v)` is *exactly* the normal at `(u+π, π−v)`, and in `float` the
identity survives to the last bit in two of the 576 pairs. A good reminder that the three index
streams are independent and their lengths mean nothing to each other.

**The seam nearly did not weld.** Computing the wrap angle as `sin(1.0f * τ)` gives `1.75e-07`
rather than 0, leaving the two copies of the seam an ulp apart — so a watertight torus would have
reported 48 boundary edges. Fixed by wrapping the *index* (`i % nu`); measured in §E rather than
left for a student to discover.

### And two figures that lied while `check-page.js` said `pass: true`

Figure 5's two cones were drawn at angles where the cancellation was invisible, so the picture
demonstrated nothing; rebuilt so the near cone nests exactly inside the far one and "green minus
amber is the solid" is literally what you see. Figure 4 was worse — it captioned a fan triangle
"leaves the polygon" and a point-in-polygon test put its centroid firmly *inside*. Rebuilding it
produced a better statement than the wrong one had: a fan is correct **iff the anchor corner can
see the whole polygon**, with convexity as the sufficient condition everyone quotes. The prose and
the `obj.cpp` comment were both sharpened to match.

Also caught by eye and then disproved by measurement: the rendered cube's silhouette looked
concave. Testing every row for contiguity and both edges for unimodality proved it convex — the
"notch" was Lesson 3.1's per-*triangle* debug palette shading the two halves of one face
differently. The eye was wrong and the measurement was right, which is the opposite of the usual
lesson and worth the same attention.

---

## Session — docs: extract the shared CSS/JS (not a lesson)

> why is the css copied over in the html files and not a common css file? Check the feasibility
> for a common file. Do not change anything in the repo yet.

Answered from the repo: the duplication was **deliberate** — `CLAUDE.md` §7 ("one fully
self-contained `.html` file … no external assets"), restated in `README.md` and
`ARCHITECTURE.md`, with `apply-shared.py` already keeping the 36 copies identical (zero drift at
the time of asking). Measured the cost: 26.6 KB CSS + 8.0 KB script × 36 pages = **1.18 MB, 18%
of the 6.7 MB tree**. Verified that `file://` does *not* block a relative stylesheet or classic
script, so extraction was feasible; recommended against it on the portability trade, and flagged
that the only unenforced gap was `--check` running on discipline alone.

> ok, since it's feasible lets extract the common css/js.

The user overrode §7. Two decisions taken up front: shared files at `docs/shared/` as a **single
copy** but gated on cross-browser verification first (fall back to per-directory copies if any
engine refused the upward `../shared/` traversal), and **no** inline-for-distribution escape
hatch — single-file portability dropped outright.

The gate passed in Chromium, Firefox **and WebKit**, so the single-copy layout shipped. Extracted
`docs/shared/course.css` + `course.js` byte-identically, rewrote all 36 pages to link them,
repurposed `apply-shared.py` from *propagating content* to *computing and verifying each page's
depth-relative link*, and added a positive-signal shared-asset check to `check-page.js`. Proved
no visual change by diffing computed styles against a pristine `HEAD` worktree — 25 selectors ×
11 pages, identical. 6.7 MB → 5.6 MB.

---

## 2026-08-06 — Lesson 3.6

> next

Resumed from `STATE.md` → `next: 3.6 — Normals and Lambert's Cosine Law`. The block named the
spine (derive the cosine law from a spreading beam, not a formula sheet), flagged the normal
matrix as the genuinely surprising part, and recorded a constraint that turned out to be
**wrong**: it said `mat3`/`mat4` have no `inverse()`. `mat3` has had one since Lesson 2.5, built
from cofactors and the adjugate — so the normal matrix needed no new machinery at all. Checking
the claim before designing around it saved a whole detour.

### Judgement calls

| Question | Decision | Why |
|---|---|---|
| Flat or per-vertex first? | **Both, with per-vertex as the default** | STATE planned flat as the starting point. Per-vertex costs the same code, uses the normals 3.5 loaded, and produces the lesson's best beat: `cube.obj` renders **pixel-identically** either way, because 3.5's split already gave each of its 24 vertices its own face's normal. A faceted mesh is faceted because of the split, not the shading model. |
| Store which light direction? | **The direction light TRAVELS**, plus `to_light()` | Physical description, and the negation gets a name so it cannot be silently skipped. The classic sign error. |
| Where does lighting live? | **`collect_triangles`, per vertex, in world space** | World space because lights are authored there, and it makes "orbiting the camera must not change the shading" a testable claim. Per vertex because shared vertices shade once — 12 evaluations instead of 60 on the icosahedron. |
| Extend `fill_style` with a light? | **No — the rasterizer is untouched** | Lighting's output is a vertex colour, and interpolating those is 2.4's job. That `fill_style` cannot host lighting is the pressure that produces a material system, and it is worth naming rather than working around. |
| Ambient term? | **Yes, and labelled a fudge in the header** | Without it the unlit half is exactly the background colour and the silhouette vanishes, which would make the lesson's own pictures worse. |
| Generate smooth normals for meshes without them? | **No — fall back to the face normal** | Correct, honest, and free. Generation needs adjacency and a smoothing-group policy, which is Exercise 3.6.4 with a pointer at `validate()`'s edge map. |

### Verification

`scratch/verify_36.cpp`, eight sections, all green, plus `scratch/render_36.cpp` for the
comparison renders. The numbers the lesson is built on: the footprint at 60° is exactly 2.00 and
the brightness 0.50; unclamped shading reads **−0.740**; the slab's scale tilts a 45° normal by
**47.50°** (137.50° to the tangent instead of 90°), with a worst case of **67.99°** over 20,000
random normals; rotation gives `5.96e−08`; uniform scale `0.0000°`; moving the light changes
6,104 px while moving the camera leaves the brightest lit pixel bit-identical; and flat vs
per-vertex is **0 px** on `cube.obj` against 5,576 on `torus.obj`.

Rendered, the normal-matrix bug costs **97.5% of a squashed torus's covered pixels**, worst
channel delta 135/255 — and the naive version does not look like noise, it looks like a correctly
lit *round* tube. It is lighting the shape the object had before the squash.

### Three figures that lied while `check-page.js` said `pass: true`

Figure 1 was drawn at 30° and labelled 60°. Figure 2 shaded the wrong half of the disc as unlit,
put the terminator on the wrong diagonal, and mislabelled a cosine as 0.28 where the geometry
gives 0.10. Figure 3 and the interactive widget disagreed with the worked example, because the
figure squashed one axis and the example squashed two.

All three are now generated from computed coordinates — a short script prints each sample point's
normal, its dot product with the light and its arrow endpoint, and the SVG carries those numbers.
The widget was changed to use the slab's full `(1.8, 0.35, 0.9)`, so dragging it to 0.35
reproduces the prose's 137.5° / 90.0° exactly, and dragging to 0.90 makes the scale uniform and
the whole problem disappear — which is §3.6 in one gesture.

A clean rebuild also caught a `-Wmissing-field-initializers` warning in `floor_geometry::view()`
that the incremental build had hidden, because `mesh` grew a fourth member in 3.5 and that file
had not been recompiled since.


## Session — docs: migrate 03-06 to the shared stylesheet (not a lesson)

Found by running `apply-shared.py --check` before adding a new page. It reported drift on exactly
one file: `docs/lessons/03-06-normals-and-lambert.html` still carried an 881-line inline copy of
the stylesheet and page script.

Not an oversight in the extraction run. The extraction branch was cut **before** 3.6 landed on
`main`, so the page did not exist on that branch and the merge had nothing to convert. Two correct
changes, and the gap opened between them.

Fixed by running the tool without `--check`; the page dropped 881 lines and gained the same
ten-line link-and-comment block every other page carries. Re-verified over HTTP in Chromium:
`check-page.js` → `pass: true`, 49 equations rendered, highlighter round-trip clean, no listing
corrupted.

The workflow note that comes out of it: this drift is **introduced by merging, not by editing**,
and it fails silently — a wrong shared link throws nothing and renders an unstyled, inert page that
reads as unfinished rather than broken. `docs/_template/README.md` said to run `--check` when a
page is added or moved; it also has to run after any merge that brings pages in from a branch cut
at a different time.


## 2026-08-07 — Lesson 3.7

> Based on the STATE and the project's claude instructions, work on the next.

`next: 3.7 — Specular and Blinn-Phong`. The first view-dependent term in the course, and the
first one whose *evaluation point* is visibly wrong — which is what 3.8 exists to fix.

### The claim that had to be re-derived

The first draft repeated, in five places including a figure and its alt text, that Phong's
highlight is cut off because **the mirror ray dips below the surface**. It cannot. `R` is `l`
mirrored about `n`, so `dot(n, R) == dot(n, l)`: if the light is above the surface, so is `R`,
always. The real condition is that `cos^p` answers only over the hemisphere *around* `R`, which is
not the *visible* hemisphere — the visible wedge it misses is exactly as wide as the light's angle
from the normal, so the trigger is "light and eye on the same side", which on a floor means the
sun is behind you.

Every measurement had passed while the explanation was wrong, because the numbers are equally
consistent with either story. What forced the correction was adding the **control**: rendering the
sun-*ahead* arrangement too, where Phong highlights all 30,806 pixels and there is no cut-off at
all.

### What the harnesses settled

`verify_37` (nine sections) and `render_37` (six) produced every number the lesson quotes.
`mirror_direction(n,l) == -reflect(l,n)` to 0.000000 over 20,000 pairs; `halfway` fed back through
`mirror_direction` returns `v` to 6e−6; `β = α/2` to 0.000018°; the 4× exponent rule *fitted* at
4.38× (p=4) falling to 4.01× (p=128); the hemisphere integrals `2π/(p+1)` and `2π/(p+2)` confirmed
by quadrature; and a default `specular` reproducing Lesson 3.6 in 100,000 random configurations
with **0 differing**.

Three of those checks passed on the first run while measuring the wrong thing — the per-vertex
peak on a mesh too dense to show the defect, the grazing comparison on a torus that already
contains every incidence angle, and an `n·l` leak test whose geometry drove the term to zero for
an unrelated reason. All three are written up in `LEARNINGS.md`.

### The figure that passed every check and still hid its claim

Figure 5 was a polar lobe plot on a linear radial scale. `check-page.js` said `pass: true`; no
label overlapped; the geometry was exact. And the one thing it existed to show — Blinn returning
0.063 at 90° where Phong returns nothing — was a dot fifteen pixels from the origin. It is now a
Cartesian plot of value against angle, where 0.063 is 6% of the height. The lobe view stays in
Figure 1, where the question is the *shape* of the spray rather than the size of the tail.

Two smaller ones, also invisible to the checker: the lobe was drawn *below* the surface line in
both Figure 1 and the widget (the function is defined there; the surface is not), and the widget's
default exponent was high enough that both models read 0 in the cut-off region — refuting the
widget's own caption if you dragged it.

### What the code did and did not gain

`raster.cpp` was untouched for the third lesson running: lighting still produces a vertex colour,
and interpolating those has been the fill's job since 2.4. What *did* change is structural — the
composed `view_from_model` is gone, because a highlight needs a world *position* and the two hops
have to come apart. That optimisation existed because the shading was view-independent; a feature
bought it out.


## 2026-08-08 — Lesson 3.8

> next

`next: 3.8 — Flat, Gouraud and Per-Pixel Shading`. Lesson 3.7 did not argue for this lesson, it
measured it: three tables of a highlight flickering, vanishing and arriving in the wrong shape,
none of which was a defect in the shading equation.

### The split

Lesson 3.6's `shade_mode { palette, flat, smooth }` could not be extended, and the reason was not
a missing value — it was holding two independent questions whose combinations form a grid. Where
a normal comes from is a property of the mesh; where the equation is evaluated is a property of
the pipeline. Two enums, six cells, and the cells have predictable behaviour a list could never
have expressed.

### Three predictions the harness overturned

- **`face × gouraud` is not unconditionally degenerate.** With a face normal the normal is
  constant, so all three evaluation points agree — but only while the shading is
  view-independent, and 3.7 ended that. Measured: 761 differing pixels with the highlight on. The
  corrected rule is *shorter* than the wrong one.
- **Per-pixel does not fix `cube.obj`.** The plan said 157 blank frames out of 180 would go to
  zero. They went to 157. Per-pixel fixes an interpolation error; a cube has six normals and no
  evaluation point invents a seventh. The 12×8 torus, which has smooth normals, went 58 → 0. That
  contrast is a better lesson than the one I planned, and it is the strongest argument for having
  two axes.
- **Per-pixel is cheaper than Gouraud below three pixels per triangle.** 0.91× at 320×180, rising
  to 2.15× at 4K. The folklore's unstated assumption is that a triangle covers many pixels; ours
  covers 2.8, so there are more vertices than covered pixels.

### The engine change

First change to `raster.cpp`'s inner loop since 3.2. `vertex` gained two varyings and stopped
being just a position; the clipper had to learn to carry them; `raster.hpp` now includes
`light.hpp`, which is a real layering violation shipped on purpose and written down, because the
clean fix is a caller-supplied fragment function and that is Module 4's answer.

### A figure that refuted its own caption

Figure 4 was captioned "continuous but not smooth", plotted correctly, and passed `check-page.js`
— and the Gouraud chord hugged the true curve so closely that it appeared to show the opposite.
The content was in the derivative. Adding a slope panel underneath made it immediate: the true
slope is a curve, Gouraud's is a staircase. Second lesson running where a figure passed every
automated check while hiding its claim.

---

## 2026-08-10 — `next` (Lesson 3.9: Texture Mapping and Bilinear Filtering)

> Based on the STATE and the project's claude instructions, work on the next.

Resumed from `STATE.md`'s `next: 3.9`, following its planning notes. Delivered
`docs/lessons/03-09-textures.html` (11,389 words of prose, 6 computed figures, an interactive
sampler widget, 19 listings, 7 pitfalls, 5 exercises), `src/gfx/texture.{hpp,cpp}`,
`flip_uv_v` on `mesh_data`, `shading::textured` + `fill_style::albedo` in the rasterizer, five
new demo keys, and `scratch/verify_39.cpp` (9 sections, all pass).

### What the lesson turns on

**A texel is a sample, not a square.** Everything else is a consequence: the half-texel offset,
which of two texels a point lies between, why a 1:1 blit can be bit-identical, and why nearest
filtering cannot see the offset at all.

### The three things I did not expect

- **Three of `verify_39`'s first five failures were the test, not the code.** The loudest was the
  1:1 blit failing while its *control passed* — impossible if the test is sound. Cause: this
  rasterizer samples attributes at integer pixel coordinates, so the half-texel question exists at
  **both ends** of the pipeline and a 1:1 quad needs its uvs offset too.
- **The obvious benchmark measured the sRGB encode, not the fetch.** Rule-vs-textured read 5.04×,
  which should not be believable for an array lookup that fits in L1. `uv_checker` encodes
  nothing; `textured` re-encodes through three `std::pow` calls. Holding the encode constant gives
  1.12× for a nearest fetch and 1.41× for bilinear. Both tables are kept and labelled.
- **Aliasing is not caused by a large footprint.** All four test images are 64 texels wide, so the
  footprint is identical at any given screen row — 62.46 texels per pixel near the horizon — yet
  the sparkle still ran 8.0% → 64.8% as the checker got finer. What changes is the *contrast*
  inside the footprint.

### The convention this settles

The uv origin, open since Lesson 3.5. Settled by quoting `SDL_gpu.h`'s "Coordinate System"
section rather than by trying both, and the flip lands at the **import** step — not the parser
(3.5: a loader must not alter its input) and not the sampler (which must match hardware). New
rule for the codebase: *a loader must not alter its input; a pipeline may.*

### The debt this adds, deliberately

`raster.hpp` now includes `texture.hpp` as well as `light.hpp`, and "textured **and** lit" is not
an enum value — it is `lit` plus a binding, so `shading` has stopped describing a fragment on its
own. 3.8 fixed the previous instance by splitting one enum into two; this one cannot be fixed that
way, because the combinations are a program rather than a grid. Fifth pressure pointing at Module
4's programmable fragment stage.
