// src/gfx/raster.hpp — turning shapes into pixels.
//
// The first file of the software rasterizer, and the beginning of Module 2. A
// framebuffer knows how to set one pixel; this knows which pixels a shape is
// made of. Everything from here to the end of Module 3 — triangles, depth,
// texture — is that same question asked about a more interesting shape.
//
// Three line routines live here, and only one of them is meant to be used. The
// other two are kept because Lesson 2.1 is an argument, and an argument needs
// something to compare against.

#pragma once

#include <SDL3/SDL.h>

// Forward declaration, not an include: every function below takes a
// framebuffer by reference and none of them needs its layout. raster.cpp
// includes the real header because it actually writes pixels. The habit is
// from Lesson 1.8 §4.1 — in a header, include what you use and forward declare
// what you merely mention.
namespace engine { class framebuffer; }

namespace engine {

/// Draw a line between two pixel centres. **This is the one to call.**
///
/// Integer Bresenham, valid in all eight octants. The pixels chosen are exactly
/// those a midpoint decision would choose (Lesson 2.1 §3.3), computed with no
/// floating point at all: for a given four integers the output is exact,
/// repeatable, and free of any accumulated drift.
///
/// Endpoints are inclusive — both `(x0,y0)` and `(x1,y1)` are lit — so a line
/// from a point to itself lights that one pixel. Coordinates outside the
/// framebuffer are dropped by put_pixel rather than clipped away up front; see
/// the note in raster.cpp about what that costs and when it will need fixing.
void draw_line(framebuffer& fb, int x0, int y0, int x1, int y1, Uint32 colour);

/// The same line, by DDA: step the major axis by one and accumulate the minor
/// axis in a float.
///
/// **Kept for comparison, not for use.** It is the algorithm most people write
/// first, it is correct, and Lesson 2.1 measures it against draw_line rather
/// than asserting which is better. Its real defect is not speed but exactness:
/// the position is a running float sum, so the pixels it picks depend on the
/// order the additions happened in.
void draw_line_dda(framebuffer& fb, int x0, int y0, int x1, int y1, Uint32 colour);

/// The broken one: walk x from x0 to x1 and evaluate y = mx + b at each step.
///
/// **Kept so it can be seen failing.** It is the most obvious possible reading
/// of "a line is y = mx + b", and it produces a dotted line rather than a solid
/// one whenever the line is steeper than 45°, because y then changes by more
/// than one pixel per column. Lesson 2.1 §1 shows the gaps before explaining
/// them. Never call this for real work.
void draw_line_naive(framebuffer& fb, int x0, int y0, int x1, int y1, Uint32 colour);

} // namespace engine
