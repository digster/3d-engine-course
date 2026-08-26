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

#pragma once

// ---- Core: time, input, and measurement -----------------------------------
#include <engine/core/clock.hpp>
#include <engine/core/fixed_step.hpp>
#include <engine/core/input.hpp>
#include <engine/core/profile.hpp>

// ---- Maths ----------------------------------------------------------------
#include <engine/math/mat2.hpp>
#include <engine/math/mat3.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/transform.hpp>
#include <engine/math/vec2.hpp>
#include <engine/math/vec3.hpp>
#include <engine/math/vec4.hpp>

// ---- Graphics: the CPU side -----------------------------------------------
#include <engine/gfx/clip.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/debug_draw.hpp>
#include <engine/gfx/depth_buffer.hpp>
#include <engine/gfx/framebuffer.hpp>
#include <engine/gfx/image.hpp>
#include <engine/gfx/light.hpp>
#include <engine/gfx/mesh.hpp>
#include <engine/gfx/obj.hpp>
#include <engine/gfx/projector.hpp>
#include <engine/gfx/raster.hpp>
#include <engine/gfx/scene.hpp>
#include <engine/gfx/soft_renderer.hpp>
#include <engine/gfx/texture.hpp>
#include <engine/gfx/viewport.hpp>

// ---- Graphics: the GPU side -----------------------------------------------
#include <engine/gfx/gpu_buffer.hpp>
#include <engine/gfx/gpu_debug.hpp>
#include <engine/gfx/gpu_device.hpp>
#include <engine/gfx/gpu_mesh.hpp>
#include <engine/gfx/gpu_pipeline.hpp>
#include <engine/gfx/gpu_present.hpp>
#include <engine/gfx/gpu_scene.hpp>
#include <engine/gfx/gpu_shader.hpp>
#include <engine/gfx/gpu_texture.hpp>
#include <engine/gfx/gpu_uniform.hpp>

// ---- Platform: how a program starts ---------------------------------------
//
// Lesson 5.2. `platform.hpp` and `app.hpp` are here; `platform/main.hpp` is
// NOT, and that omission is deliberate rather than an oversight. Including it
// defines a program's entry point, so it belongs in exactly one .cpp per
// program — and an umbrella whose job is "include everything, it is harmless"
// must not be a way to get a `main` by accident.
#include <engine/platform/app.hpp>
#include <engine/platform/platform.hpp>
