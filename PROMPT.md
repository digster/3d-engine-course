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
