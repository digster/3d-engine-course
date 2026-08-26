// engine/include/engine/gfx/debug_draw.hpp — drawing and measuring the things a
// HUMAN needs to see.
//
// Wireframes, axis triads, world-space lines, the depth buffer made visible, and
// two measurements that only exist to be printed. None of it is how a frame is
// rendered; all of it is how a frame is UNDERSTOOD, and every one of these was
// written the moment some lesson needed to see inside its own pipeline.
//
// Grouping them is Lesson 5.1's second smallest decision and its most repeatable
// one: they were scattered through the demo because each arrived alone, and the
// thing they have in common is not their shape but their PURPOSE. Debug code is
// a component, not a leftover — it ships disabled, it is budgeted separately
// (Lesson 3.10's `zone::overlay`), and Lesson 5.10 turns this header into a real
// debug-draw system with a command queue behind it.

#pragma once

#include <engine/gfx/depth_buffer.hpp>
#include <engine/gfx/framebuffer.hpp>
#include <engine/gfx/mesh.hpp>
#include <engine/gfx/projector.hpp>
#include <engine/gfx/viewport.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/vec3.hpp>

#include <SDL3/SDL_stdinc.h>

namespace engine {

/// Draw a line whose endpoints are in VIEW space.
///
/// The one-dimensional case of the whole lesson, and the place the fix is easiest
/// to read: build both endpoints in clip space, hand them to the clipper, and only
/// then divide. Before Lesson 3.3 a line crossing the near plane simply vanished —
/// which is why walking over a gridline used to make it disappear rather than run
/// off the bottom of the screen.
void line3(framebuffer& fb, vec3 a, vec3 b,
           Uint32 colour, const projector& pr);

/// Draw a WORLD-space line through the view matrix and the projector. Convenience
/// for the world grid and axes, whose endpoints are natural world-space constants.
void line3_world(framebuffer& fb, const mat4& view, const projector& pr,
                 vec3 a, vec3 b, Uint32 colour);

// ---------------------------------------------------------------------------
// Drawing a MESH, in MODEL SPACE
// ---------------------------------------------------------------------------
//
// The geometry itself now lives in gfx/mesh.hpp as vertices + triangle indices
// (Lesson 2.12). What has not changed since Lesson 2.8 is the claim those
// coordinates make: a mesh is not ANYWHERE. It is not at the left of the screen,
// not two metres from the door, not big or small. It is a shape in ITS OWN
// coordinates — model space — and the model matrix says where that space goes.
//
// Which is why the same twelve-vertex icosahedron and the same eight-vertex cube
// serve every object in the scene below, at different sizes, orientations and
// places. That is what model space buys, and it is why a game ships one crate and
// places four hundred.

/// Draw a mesh as a wireframe, with edge brightness standing in for depth.
///
/// Every triangle contributes its three edges. Shared edges are therefore drawn
/// TWICE — 60 line draws for the icosahedron's 30 unique edges — which is honest
/// waste worth naming rather than hiding. Deduplicating would mean building an edge
/// list, and the moment Module 3 fills these triangles the duplication vanishes on
/// its own, so we pay it and say so.
///
/// The brightness is a **cue, not a calculation** — there is no depth buffer yet
/// (Lesson 3.1) and no lighting (Lesson 3.6), so this is simply "edges further away
/// are dimmer" so the wireframe can be read at all. Without it a spinning wireframe
/// is a genuinely ambiguous picture: your eye flips between two interpretations,
/// because a wireframe has discarded the only information that could settle it.
///
/// @param point_w  the fourth component the vertices are sent with. 1 is correct;
///                  0 reproduces Lesson 2.6, where every translation is ignored.
void draw_mesh(framebuffer& fb, const mesh& geometry,
               const mat4& m, const projector& pr, float point_w);

/// The three transformed basis vectors, in the course's axis colours.
///
/// Exactly Lesson 2.5's picture with a third arrow: the columns of the matrix,
/// drawn. Whatever the cube is doing, these three arrows are why — and as of
/// Lesson 2.8 they have a name. Sending the model's basis vectors through the
/// model matrix as DIRECTIONS returns its first three columns, so these arrows
/// are the object's own axes, expressed in world space.
/// @param dir_w  the fourth component the AXES are sent with. 0 is correct — they
///               are directions. 1 translates them, which is the classic
///               transform-a-normal-as-a-point bug, drawn.
void draw_axes3(framebuffer& fb, const mat4& m, const projector& pr,
                float point_w, float dir_w);

/// Paint the depth buffer itself into the framebuffer, stretched to its own range.
///
/// Two things are true at once and both are worth seeing. Raw, the buffer is very
/// nearly uniform white: with near = 0.3 the entire visible scene occupies about
/// two percent of the [0,1] range, because depth is distributed as 1/z (§3.6).
/// Stretched between the minimum and maximum actually present, the same numbers
/// show a perfectly readable depth image. The HUD prints the two endpoints, so
/// the readable picture never lets you forget how narrow the band is.
///
/// Returns the (min, max) actually found, for the HUD.
struct depth_range { float lo = 1.0f; float hi = 0.0f; };

[[nodiscard]] depth_range show_depth(framebuffer& fb,
                                     const depth_buffer& db,
                                     const viewport& vp);

/// How many pixels two framebuffers disagree on, inside the viewport rectangle.
///
/// The lesson's headline number. The painter's algorithm and the z-buffer are run
/// on identical geometry every frame and their outputs compared; the count is the
/// size of the region where sorting gets the wrong answer. On the icosahedron it
/// is zero and stays zero. On the cycle it is hundreds, and no amount of
/// improving the sort will move it.
[[nodiscard]] int count_differences(const framebuffer& a,
                                    const framebuffer& b,
                                    const viewport& vp);

/// The brightest single channel anywhere in the viewport, 0..255.
///
/// Lesson 3.7's cheapest honest instrument. A highlight is by definition the
/// brightest thing on a surface, so one number tracks it: watch it climb as the
/// camera swings into the mirror direction, and — on a coarse mesh — watch it
/// COLLAPSE as the object turns and the peak falls between vertices. Reading the
/// maximum rather than a named pixel means the measurement does not have to know
/// where the highlight went, which is exactly the thing under investigation.
///
/// The value is an *encoded* channel (Lesson 1.6), because that is what the screen
/// shows and what the reader can compare against the picture.
[[nodiscard]] int brightest_channel(const framebuffer& fb, const viewport& vp);

}   // namespace engine
