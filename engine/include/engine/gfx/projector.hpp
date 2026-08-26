// engine/include/engine/gfx/projector.hpp — everything needed to turn a
// view-space point into a pixel.
//
// Lesson 3.3 gathered the projection matrix, the viewport and the near-plane
// policy into one struct because they always travel together. Lesson 5.1 gave
// that struct a header of its own, for a reason worth stating: TWO different
// parts of the engine need it — the software renderer that fills triangles, and
// the debug draw that strokes lines — and a type two components share is a
// header, not a section of whichever one happened to define it first.
//
// That is the whole of physical design in one sentence. Where a type lives is
// decided by who needs it, not by who wrote it.

#pragma once

#include <engine/gfx/viewport.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/vec2.hpp>
#include <engine/math/vec3.hpp>
#include <engine/math/vec4.hpp>

#include <algorithm>
#include <cmath>

namespace engine {

// ---------------------------------------------------------------------------
// Lesson 3.3 — the near plane
// ---------------------------------------------------------------------------

/// What this demo does with geometry that crosses the near plane. [K] cycles.
///
/// Three settings rather than a bool, because there are **two different wrong
/// answers** here and they fail in visibly different ways. Seeing both is the
/// lesson; a bool would only let you see one.
enum class near_mode
{
    /// Lesson 3.2's behaviour: if any vertex is behind the near plane, drop the
    /// whole triangle (and the whole line). Never crashes, never produces a wild
    /// coordinate — and is catastrophically wrong the moment the geometry matters,
    /// because the surface you are walking into *disappears* instead of filling
    /// the view. Dolly into the floor scene and the ground under your feet goes.
    drop,

    /// No guard at all: divide by `w` whatever it happens to be. This is what the
    /// code did before the guard existed, and it is the failure Lesson 2.7 warned
    /// about in the abstract. A vertex behind the eye has `w < 0`, so `x/w` flips
    /// its sign and the vertex lands on the **opposite side of the screen**; the
    /// triangle stretches across the whole frame or turns inside out.
    none,

    /// Cut the geometry along the near plane and rasterize the part in front.
    /// **The answer**, and the default.
    clip
};

[[nodiscard]] inline const char* name_of(near_mode m)
{
    switch (m)
    {
    case near_mode::drop: return "DROP (3.2's bug)";
    case near_mode::none: return "NONE (no guard)";
    case near_mode::clip: return "CLIP (correct)";
    }
    return "?";
}


/// Everything needed to turn a view-space point into a pixel, in one object.
///
/// This used to be two loose parameters (`proj`, `vp`) threaded through six
/// functions, and Lesson 3.3 was about to make it three. Gathering them is the
/// same argument `fill_style` made in Lesson 3.2, applied one level up: state that
/// always travels together should travel as one thing, and adding a knob should
/// cost one field rather than one more parameter at every call site. It is also
/// the shape Module 5's renderer has — a camera hands the frame a projection, a
/// viewport, and its clipping rules, and everything downstream reads them.
struct projector
{
    mat4 proj;                  ///< view -> clip (Lesson 2.10)
    viewport vp;                ///< NDC -> pixels, with the y flip (2.11)
    near_mode near = near_mode::clip;   ///< what happens at the near plane (3.3)
};

/// VIEW space -> CLIP space. One matrix multiply — and the pipeline's new stopping
/// point, because clipping has to happen *before* the divide.
[[nodiscard]] inline vec4 to_clip(vec3 v_view, const mat4& proj)
{
    return proj * point(v_view);
}

/// A point that has been through the divide: pixels, device depth, and `1/w`.
///
/// `depth` is the device depth in [0, 1] that Lesson 2.11's viewport transform has
/// been producing all along, and which Lesson 3.1 gave somewhere to go. `inv_w` is
/// the reciprocal of the clip-space `w`, which Lesson 3.2 needs at every corner;
/// it is computed here because this is the last place that still *has* `w` —
/// `perspective_divide` consumes it and does not give it back.
///
/// Note what this struct no longer carries: a `visible` flag. Before Lesson 3.3
/// the projection answered "…and is this point usable at all?", which was the
/// polite way of saying the pipeline had no plan for geometry near the eye. Now it
/// has one, and it runs a step earlier.
struct screen_point
{
    vec2 xy;
    float depth;
    float inv_w;
};

/// A float pixel coordinate, rounded and made **safe to convert**.
///
/// Converting a float to an int is undefined behaviour when the value does not fit
/// in the int, and `near_mode::none` produces exactly that: a `w` near zero sends a
/// coordinate into the millions. This is not the fix for the near plane — clipping
/// is — it is what lets the broken mode be *shown* without the program losing its
/// meaning while you look at it.
///
/// The bound is 8,000 and not something larger for a second reason. `edge_function`
/// (raster.hpp) multiplies coordinate *differences*: at ±8,000 the products stay
/// comfortably inside a 32-bit int, and signed overflow is undefined too. One
/// constant, two undefined behaviours held off.
[[nodiscard]] inline int to_pixel(float f)
{
    constexpr float k_wild = 8000.0f;
    if (std::isnan(f)) { return 0; }   // every comparison with a NaN is false, so clamp cannot
    return static_cast<int>(std::lround(std::clamp(f, -k_wild, k_wild)));
}

/// CLIP space -> framebuffer pixels: the perspective divide, then the viewport map.
///
/// **This function assumes its input has already been clipped.** That is the whole
/// contract, and it is why it has no guard: `w` at or behind the eye is not a case
/// to branch on here, it is a case that must not reach here. The demo's `none` mode
/// deliberately breaks that contract to show what happens.
[[nodiscard]] inline screen_point screen_from_clip(const vec4& clip,
                                            const viewport& vp)
{
    // `w == 0` is the plane through the eye itself, where every ratio is 0/0 — a
    // NaN, not a large number. Nudging it off zero is demo scaffolding, not engine
    // policy: it makes `none` produce a wildly wrong *picture* rather than
    // undefined behaviour. Under `clip` the branch is unreachable, because the near
    // plane sits a whole `near` in front of `w = 0`.
    const float w = (clip.w == 0.0f) ? 1.0e-6f : clip.w;
    const vec3 ndc = perspective_divide({clip.x, clip.y, clip.z, w});

    // The viewport does the NDC -> pixel map AND the +y-up-to-+y-down flip. Its
    // third output is the device depth the z-buffer stores (Lesson 3.1).
    const vec3 screen = vp.to_screen(ndc);
    return {{screen.x, screen.y}, screen.z, 1.0f / w};
}

}   // namespace engine
