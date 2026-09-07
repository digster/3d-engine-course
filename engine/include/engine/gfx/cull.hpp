// engine/include/engine/gfx/cull.hpp — which faces to throw away.
//
// Lesson 6.5, and this file exists for one reason, stated as a rule in
// `projector.hpp` back in 5.1: **a type two components share is a header, not a
// section of whichever one happened to define it first.** Where a type lives is
// decided by who needs it, not by who wrote it.
//
// `cull_mode` was defined in `raster.hpp` in Lesson 3.4 because the rasterizer
// was the only thing that had an opinion about it. Lesson 6.5 gives `material.hpp`
// an opinion too — `cull_of()` turns a mesh's own geometry into a culling
// decision — and at that moment the enum has two homes and needs its own.
//
// A NOTE ON THE ARGUMENT I REACHED FOR FIRST, AND WHY IT IS NOT HERE.
//
// The obvious justification is compile time: `raster.hpp` is 700 lines, Lesson
// 5.1 measured it as the most expensive header in the engine, and pulling it in
// for a three-value enum sounds indefensible. So that is what this comment said,
// with 5.1's number quoted in it — until the number was re-measured on the
// machine that is actually building this, where `raster.hpp` costs 0.258 s and
// `material.hpp` costs 0.261 s. Materials already include `texture.hpp`, which
// dominates both. **The marginal saving is approximately zero**, and 5.1's own
// note says why the quote was misused: THE RATIO IS WHAT TRAVELS, NOT THE
// SECONDS.
//
// The split stands anyway, on the physical-design rule alone — two components
// share the type, so it gets a header. That rule never needed a performance
// argument, and reaching for one I had not taken myself nearly shipped a false
// number in a course whose whole premise is that measurements are taken.
// (`cull.hpp` does cost 0.038 s, which is the one figure here that was measured
// before it was written down.)

#pragma once

namespace engine {

/// Which faces to throw away before rasterizing.
///
/// Field order and meaning mirror `SDL_GPUCullMode` exactly — verified against
/// `SDL3/SDL_gpu.h`, where the enumerators are `NONE`, `FRONT`, `BACK` in that
/// order — so Module 4's port is a rename. It lives in `fill_style` for the same
/// reason: SDL_GPU carries `cull_mode` in `SDL_GPURasterizerState` alongside
/// `fill_mode` and `front_face`, which is to say it is *pipeline state*, decided
/// once and bound, not a per-draw argument.
///
/// **This is an optimisation, not a correctness fix**, and Lesson 3.4 spends real
/// time on that distinction. The z-buffer already produces the right picture with
/// culling switched off; what culling buys is not drawing roughly half of a closed
/// mesh's triangles at all.
enum class cull_mode
{
    /// Draw everything. **The default**, and the only setting that is correct for
    /// *every* mesh — because culling is only valid on geometry that is closed.
    /// A ground plane, a billboard, a leaf card and a sheet of paper are all
    /// single-sided, and back-face culling makes them vanish when seen from behind.
    none,

    /// Throw away front faces. Useful for looking inside a closed mesh, and
    /// genuinely used in real renderers — rendering the inside of a skybox cube, or
    /// the back faces of shadow volumes.
    front,

    /// Throw away back faces. **The one you want** on any closed mesh: no face
    /// pointing away from the camera can be visible, because a closer front face is
    /// always in the way.
    back
};
} // namespace engine
