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
