// src/gfx/viewport.hpp — the last coordinate transform: NDC to framebuffer pixels.
//
// After the perspective divide (Lesson 2.10) a vertex is in normalized device
// coordinates: x and y in [-1, 1], z in [0, 1], with +y pointing UP. That range is
// device-independent — it says nothing about how big the window is or which corner
// is the origin. The VIEWPORT TRANSFORM is what nails it to an actual rectangle of
// pixels: it stretches the [-1, 1] square onto a w-by-h rectangle at some offset,
// and — the one subtlety — FLIPS Y, because the framebuffer counts rows downward
// from the top while NDC counts upward. Lesson 2.11.
//
// This is the final link of the chain the whole module has been building:
//
//     model -> world -> view -> clip -> NDC -> SCREEN
//                                               ^ here
//
// The struct mirrors SDL_GPU's own `SDL_GPUViewport` field-for-field (x, y, w, h,
// min_depth, max_depth), verified against SDL3/SDL_gpu.h, so Module 4 configures
// the GPU's viewport with the same numbers this one uses — the transform moves from
// our code onto the hardware without a translation step.

#pragma once

#include "math/vec3.hpp"

namespace engine {

/// A rectangle of the framebuffer, and the depth range mapped onto it.
///
/// Field names and meanings match `SDL_GPUViewport` exactly:
///   - `x`, `y`         the TOP-LEFT offset of the rectangle, in pixels
///   - `w`, `h`         its width and height, in pixels
///   - `min_depth`,     the depth range NDC's z (which is [0, 1]) maps onto —
///     `max_depth`      almost always [0, 1], but the GPU lets you narrow it, e.g.
///                      to pin a HUD in front of the scene without a separate pass.
struct viewport
{
    float x = 0.0f;          ///< left offset in pixels
    float y = 0.0f;          ///< top offset in pixels  (top, not bottom — see to_screen)
    float w = 0.0f;          ///< width in pixels
    float h = 0.0f;          ///< height in pixels
    float min_depth = 0.0f;  ///< NDC z = 0 maps here
    float max_depth = 1.0f;  ///< NDC z = 1 maps here

    /// Map a point from NDC to framebuffer pixels (x, y) plus a device depth (z).
    ///
    /// Each axis is an independent affine map — a scale and an offset — which is
    /// all the viewport transform is. Reading them out (Lesson 2.11 §3):
    ///
    ///   x:  ndc.x in [-1, 1]  ->  [x, x + w]         left edge to right edge
    ///   y:  ndc.y in [-1, 1]  ->  [y + h, y]         **flipped**: +1 (up) -> top row
    ///   z:  ndc.z in [0, 1]   ->  [min_depth, max_depth]
    ///
    /// The y map is the only one that reverses, and that reversal is the
    /// +y-up (NDC, SDL_GPU clip space) to +y-down (framebuffer, texture memory)
    /// boundary — a single sign that has hidden in one place or another since
    /// Lesson 2.5's `to_screen`. Here it finally has a name and a home.
    [[nodiscard]] constexpr vec3 to_screen(vec3 ndc) const
    {
        // (ndc + 1) / 2 remaps [-1, 1] to [0, 1]; then scale by the extent and add
        // the offset. For y the (1 - t) is the flip: NDC's top (ndc.y = +1) lands
        // on the framebuffer's top row (the smallest y), not the bottom.
        const float tx = ndc.x * 0.5f + 0.5f;         // [-1,1] -> [0,1]
        const float ty = ndc.y * 0.5f + 0.5f;         // [-1,1] -> [0,1], +y still up
        return {x + tx * w,
                y + (1.0f - ty) * h,                  // (1 - ty): the y flip
                min_depth + ndc.z * (max_depth - min_depth)};
    }
};

} // namespace engine
