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
