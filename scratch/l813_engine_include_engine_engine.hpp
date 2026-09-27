// engine/include/engine/engine.hpp — the whole public API, in one include.
//
// AN UMBRELLA HEADER, and a contested idea, so here is the honest account rather
// than a recommendation.
//
// WHAT IT BUYS. Discoverability, mostly. A newcomer who writes
// `#include <engine/engine.hpp>` gets everything and can start typing; they do
// not have to know that `lambert` lives in gfx/light.hpp before they have any
// idea what Lambert is. It is also a TABLE OF CONTENTS: this file is the shortest
// complete statement of what the engine offers, and the fastest way to see
// whether something is public is to look for it here.
//
// WHAT IT COSTS. Compile time, and the cost is not theoretical. Including this
// pulls in every public header and, transitively, most of SDL — so every
// translation unit that uses it pays for the whole surface whether it touches
// three symbols or three hundred. Worse, it destroys the ONE property that makes
// incremental builds fast: with explicit includes, editing gpu_scene.hpp
// recompiles the files that use the GPU scene; with an umbrella, it recompiles
// everything. Lesson 5.1 measures both numbers on this project rather than
// asserting them.
//
// SO WHO SHOULD USE IT. Small programs, throwaway tests, and the first hour of
// learning the API — where the whole build is a second and nobody edits the
// engine. Anything that will be built more than a few hundred times should
// include what it uses, and the demos in this repository do.
//
// SDL ships exactly this arrangement — <SDL3/SDL.h> is an umbrella over
// <SDL3/SDL_video.h> and forty siblings — and the same advice applies to it for
// the same reasons. We are in respectable company either way.

// ---- ONE RULE, AND LESSON 5.12 FOUND OUT WHY IT NEEDS A MACHINE ------------
//
// **Every public header appears below.** That sentence is the only thing that
// makes the paragraph above true, and for thirteen lessons it was false.
//
// Lesson 5.12 counted: this file listed 40 of the engine's 55 public headers.
// One of the fifteen absences was correct and is documented below; the other
// fourteen were not obscure — they were the ENTIRE ECS, the asset
// store, handles, logging, assertions and the action map, which is to say very
// nearly everything Module 5 built. Its history explains itself: 5.2 remembered
// to add `platform/`, 5.11 remembered `debug_lines`, and 5.3, 5.4, 5.5, 5.8,
// 5.9 and 5.10 did not. A rule kept by memory is kept about half the time.
//
// The failure mode is what makes it worth a paragraph. A newcomer does exactly
// what the comment above tells them to do, writes `#include <engine/engine.hpp>`
// and then `engine::ecs::registry`, and gets an error saying that name does not
// exist — which reads as *"this engine has no ECS"* rather than *"the umbrella
// is stale"*. The one document whose job is to answer "is this public?" was
// answering "no" about eleven headers that are.
//
// So it is now CHECKED rather than remembered, at configure time, in
// `engine/CMakeLists.txt`: CMake globs `include/engine/**.hpp`, greps this file,
// and fails the configure listing anything missing. That is a glob used as a
// LINT and not as a source list, which is the distinction that makes it the
// right tool here — a missed header is a silent wrong answer, and the check
// costs 4 ms.

#pragma once

// ---- Core: time, input, diagnostics and storage ----------------------------
#include <engine/core/actions.hpp>
#include <engine/core/assert.hpp>
#include <engine/core/bench.hpp>
#include <engine/core/clock.hpp>
#include <engine/core/fixed_step.hpp>
#include <engine/core/handle.hpp>
#include <engine/core/input.hpp>
#include <engine/core/log.hpp>
#include <engine/core/pool.hpp>
#include <engine/core/profile.hpp>

// ---- Animation: skeletons, and the surfaces they bend ----------------------
//
// Lesson 7.6, and for the first time in the run the lint did NOT catch anybody:
// these two lines went in with the headers, in the same edit, by somebody who
// had just read the paragraph above about the check firing on people who know
// about it. **Five catches in four lessons, then a miss — in the check's
// favour.**
//
// So it was exercised on purpose instead, by deleting both lines and
// reconfiguring, which is how 5.12 proved it in the first place and is the only
// way to learn anything from a check that stays quiet. It names both headers and
// stops the configure. Worth doing here specifically because this is the first
// NEW DIRECTORY since 5.11 and therefore the first time the glob has had to
// notice a whole subsystem rather than one more file in a familiar folder — and
// because the failure it prevents scales with that: a public `engine::anim` the
// table of contents does not mention reads as an engine with no animation in it.
#include <engine/anim/clip.hpp>
#include <engine/anim/skeleton.hpp>
#include <engine/anim/skin.hpp>

// ---- Assets: finding files, and owning what they become --------------------
#include <engine/asset/asset_store.hpp>
#include <engine/asset/search_path.hpp>

// ---- Audio: the first subsystem with a thread of its own -------------------
//
// Lesson 7.8, and the lint fired again — on the whole DIRECTORY this time, three
// headers at once, from somebody who had just read 7.6's note above about being
// the first miss in the check's favour. **Six catches in five lessons.** The
// shape of the miss was the same every time and is worth naming for the last
// time: you write the header, the build compiles it, your harness links it and
// passes, and there is no moment at which anything makes you open this file.
//
// `mixer.hpp` includes `sound.hpp` and `spatial.hpp`, so two of these three lines
// are strictly redundant to the compiler and neither is redundant to the reader —
// this file is a table of contents, and a table of contents that omits a chapter
// because another chapter mentions it is not one.
#include <engine/audio/mixer.hpp>
#include <engine/audio/sound.hpp>
#include <engine/audio/spatial.hpp>

// ---- Entities: the ECS -----------------------------------------------------
//
// Note `ecs/pool.hpp` and `core/pool.hpp` are BOTH here and are two different
// containers — `engine::ecs::pool<T>` is keyed by entity and `engine::pool<T>`
// by handle. They are listed together on purpose: the collision is easier to
// understand from a table of contents that admits it than from a compiler error.
#include <engine/ecs/camera.hpp>
#include <engine/ecs/entity.hpp>
#include <engine/ecs/hierarchy.hpp>
#include <engine/ecs/pool.hpp>
#include <engine/ecs/registry.hpp>
#include <engine/ecs/view.hpp>

// ---- Maths ----------------------------------------------------------------
// Lesson 7.1 added the first line here since Module 2, and it did not have to be
// remembered: the configure-time lint below `add_library` in engine/CMakeLists.txt
// refused to configure the build until this line existed. That check was written
// in 5.12 and proved to fail by deleting an include; this is the first time it
// fired on a header somebody had actually just written. **It then fired twice
// more on the very next lesson**, for `axis_angle.hpp` and `rotation.hpp` — which
// is three catches in two lessons, against fourteen misses in the thirteen
// lessons before the check existed. **Four in three**, as of 7.3's
// `complex.hpp`: the lint fired again, on a header written twenty minutes
// earlier by somebody who knew about the lint. **Five in four** with 7.4's
// `quat.hpp`, and by now the interesting question is no longer whether the
// check earns its 4 ms. It is why knowing about a rule does not make anyone
// keep it: the header is written, it compiles, its harness passes, and the
// umbrella is a file you have no reason to open. A check that fires on people
// who know about it is doing the only job worth doing.
#include <engine/math/axis_angle.hpp>
#include <engine/math/bounds.hpp>
#include <engine/math/complex.hpp>
#include <engine/math/euler.hpp>
#include <engine/math/mat2.hpp>
#include <engine/math/mat3.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/rotation.hpp>
#include <engine/math/transform.hpp>
#include <engine/math/vec2.hpp>
#include <engine/math/vec3.hpp>
#include <engine/math/vec4.hpp>

// ---- Graphics: the CPU side -----------------------------------------------
#include <engine/gfx/antialias.hpp>
#include <engine/gfx/blend.hpp>
#include <engine/gfx/bloom.hpp>
#include <engine/gfx/cascade.hpp>
#include <engine/gfx/clip.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/cubemap.hpp>
#include <engine/gfx/cull.hpp>
#include <engine/gfx/debug_draw.hpp>
#include <engine/gfx/debug_lines.hpp>
#include <engine/gfx/depth_buffer.hpp>
#include <engine/gfx/draw_order.hpp>
#include <engine/gfx/font.hpp>
#include <engine/gfx/frame_graph.hpp>
#include <engine/gfx/framebuffer.hpp>
#include <engine/gfx/frustum.hpp>
#include <engine/gfx/gltf.hpp>
#include <engine/gfx/hdr.hpp>
#include <engine/gfx/image.hpp>
#include <engine/gfx/instancing.hpp>
#include <engine/gfx/light.hpp>
#include <engine/gfx/material.hpp>
#include <engine/gfx/mesh.hpp>
#include <engine/gfx/microfacet.hpp>
#include <engine/gfx/mipmap.hpp>
#include <engine/gfx/obj.hpp>
#include <engine/gfx/overlay.hpp>
#include <engine/gfx/projector.hpp>
#include <engine/gfx/raster.hpp>
#include <engine/gfx/renderable.hpp>
#include <engine/gfx/scene.hpp>
#include <engine/gfx/shadow.hpp>
#include <engine/gfx/soft_renderer.hpp>
#include <engine/gfx/texture.hpp>
#include <engine/gfx/viewport.hpp>

// ---- Graphics: the GPU side -----------------------------------------------
#include <engine/gfx/gpu_buffer.hpp>
#include <engine/gfx/gpu_debug.hpp>
#include <engine/gfx/gpu_device.hpp>
#include <engine/gfx/gpu_mesh.hpp>
#include <engine/gfx/gpu_overlay.hpp>
#include <engine/gfx/gpu_pipeline.hpp>
#include <engine/gfx/gpu_post.hpp>
#include <engine/gfx/gpu_present.hpp>
#include <engine/gfx/gpu_scene.hpp>
#include <engine/gfx/gpu_shader.hpp>
#include <engine/gfx/gpu_shadow.hpp>
#include <engine/gfx/gpu_texture.hpp>
#include <engine/gfx/gpu_uniform.hpp>

// ---- Physics: how a velocity becomes a position ---------------------------
//
// Lesson 8.1, and the lint fired AGAIN — seven catches in seven lessons, on a
// directory this time, from somebody who had read 7.8's note above about the
// shape of the miss and then reproduced it exactly. It is worth recording that
// reading the warning does not prevent the mistake, because the mistake is not
// a failure of memory: you write the header, the build compiles it, the harness
// links it and passes, and at no point does anything make you open THIS file.
// A check at configure time is what makes you open it, and that is the entire
// argument for having written it.
//
// One header today. By the end of the module this section is bodies, shapes,
// a broadphase, contact manifolds and a solver, and it will be the second
// longest block in this file.
//
// Lesson 8.3 added the third, `phys/inertia.hpp`, in the same edit again — two
// in a row now, which is the first time this file has been kept current across
// consecutive lessons since Module 5.
//
// Lesson 8.2 added the second, and — for the first time in EIGHT lessons — the
// lint did not have to find it. The line below was written in the same edit as
// the header it names, because the note above was read as a checklist rather
// than as a story about somebody else. That is worth recording precisely
// because it is not evidence that the habit has taken: seven of the last eight
// lessons missed this file, and the streak broke on the lesson immediately
// after the one that wrote a paragraph about missing it.
//
// Lesson 8.11 added `phys/constraint.hpp` in the same edit as the file itself.
// Noted for the count, not as a boast: the lint is what makes this reliable.
//
// Lesson 8.12 added `phys/ragdoll.hpp`, again in the same edit — the first
// header in this block that includes one from another subsystem's section
// (`anim/skeleton.hpp`), which is fine here because this file includes both.
//
// Lesson 8.13 added `phys/cast.hpp` and `phys/character.hpp`, both in the same
// edit, closing the module: fifteen headers, the longest block in this file.
#include <engine/phys/broadphase.hpp>
#include <engine/phys/cast.hpp>
#include <engine/phys/character.hpp>
#include <engine/phys/collide.hpp>
#include <engine/phys/constraint.hpp>
#include <engine/phys/convex.hpp>
#include <engine/phys/epa.hpp>
#include <engine/phys/gjk.hpp>
#include <engine/phys/inertia.hpp>
#include <engine/phys/manifold.hpp>
#include <engine/phys/integrate.hpp>
#include <engine/phys/ragdoll.hpp>
#include <engine/phys/rigid_body.hpp>
#include <engine/phys/shape.hpp>
#include <engine/phys/solver.hpp>

// ---- Platform: how a program starts ---------------------------------------
//
// Lesson 5.2. `platform.hpp` and `app.hpp` are here; `platform/main.hpp` is
// NOT, and that omission is deliberate rather than an oversight. Including it
// defines a program's entry point, so it belongs in exactly one .cpp per
// program — and an umbrella whose job is "include everything, it is harmless"
// must not be a way to get a `main` by accident.
#include <engine/platform/app.hpp>
#include <engine/platform/platform.hpp>

// ---- Tooling ---------------------------------------------------------------
//
// Lesson 5.11. `debug_ui.hpp` is here and <imgui.h> is NOT, and the distinction
// is the same one that keeps `main.hpp` out of this file: this header owns the
// UI's LIFECYCLE, which every program can safely have, while the widget
// vocabulary belongs to the tooling code that actually draws panels. A program
// that wants panels includes <imgui.h> itself, deliberately, in the file that
// draws them.
#include <engine/ui/debug_ui.hpp>
