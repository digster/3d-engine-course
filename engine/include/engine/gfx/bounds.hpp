// engine/include/engine/gfx/bounds.hpp — an axis-aligned box, and what it is for.
//
// Lesson 6.8. This struct has existed three times already, written out by hand
// each time: `demos/gltf_view` accumulates a `bounds_min_`/`bounds_max_` pair to
// frame its camera, `mesh_report` measures an extent to report it, and Lesson
// 6.6's importer walks every position of every primitive for the same reason.
// Three call sites is the rule this course uses for promoting a pattern into a
// type (Lesson 3.3's `projector`, 3.8's `fill_style`), and a fourth arrives in
// this lesson: **a directional light has no position, so its shadow frustum has
// to be fitted to something, and the thing it is fitted to is this.**
//
// WHY A BOX AND NOT A SPHERE. A sphere is smaller to store, cheaper to
// transform, and rotation-invariant — genuinely better for a lot of culling. It
// is the wrong shape *here* for one reason: the quantity a shadow map needs is
// the extent of the scene **along the light's three axes**, and a box already
// answers that question directly in whatever frame you put it in, while a sphere
// would have to be re-tightened. Lesson 6.10's frustum culling will want both
// and will say so then.
//
// WHAT AN AABB IS NOT. It is not the object. `transformed()` below returns the
// box that contains the transformed *box*, which is generally larger than the
// box that contains the transformed *object* — rotate a long thin rod by 45° and
// the enclosing box grows by up to sqrt(3). That looseness is the price of
// axis-alignment and it is why the function is named for what it does rather
// than for what a caller might wish it did.

#ifndef ENGINE_GFX_BOUNDS_HPP
#define ENGINE_GFX_BOUNDS_HPP

#include <engine/math/mat4.hpp>
#include <engine/math/vec3.hpp>

#include <algorithm>   // std::min, std::max

namespace engine {

/// An axis-aligned bounding box, stored as its two extreme corners.
///
/// **A default-constructed box is EMPTY, and it is empty by being inside out** —
/// `min` holds +infinity and `max` holds -infinity. That is not a trick, it is
/// the identity element for `expand`: growing an empty box by a point gives
/// exactly that point, with no "is this the first one?" branch at the call site.
/// A first-point special case is the single most common bug in bounds code, and
/// this representation removes the opportunity rather than documenting it.
struct aabb
{
    vec3 min{ 1e30f,  1e30f,  1e30f};
    vec3 max{-1e30f, -1e30f, -1e30f};

    /// Has anything been added? False for the inside-out default.
    ///
    /// Checked on one axis, because `expand` only ever writes all three
    /// together — there is no way to reach a box that is empty in x and not
    /// in y.
    [[nodiscard]] bool empty() const { return min.x > max.x; }

    /// Grow to include `p`. The one mutator, and every other builder calls it.
    void expand(vec3 p)
    {
        min = {std::min(min.x, p.x), std::min(min.y, p.y), std::min(min.z, p.z)};
        max = {std::max(max.x, p.x), std::max(max.y, p.y), std::max(max.z, p.z)};
    }

    /// Grow to include all of `other`. A no-op if `other` is empty, which falls
    /// out of the inside-out representation without a test.
    void expand(const aabb& other)
    {
        min = {std::min(min.x, other.min.x), std::min(min.y, other.min.y),
               std::min(min.z, other.min.z)};
        max = {std::max(max.x, other.max.x), std::max(max.y, other.max.y),
               std::max(max.z, other.max.z)};
    }

    /// Push every face out by `m`. Negative shrinks, and is not guarded: a box
    /// shrunk past itself is inside out, which `empty()` already reports.
    void grow(float m)
    {
        min = {min.x - m, min.y - m, min.z - m};
        max = {max.x + m, max.y + m, max.z + m};
    }

    [[nodiscard]] vec3 centre() const
    {
        return {(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f};
    }

    /// The full width on each axis — `max - min`. Zero on an axis where the box
    /// is a plane, which a ground quad genuinely is.
    [[nodiscard]] vec3 extent() const
    {
        return {max.x - min.x, max.y - min.y, max.z - min.z};
    }

    /// Half the length of the box's diagonal — the radius of the smallest sphere
    /// centred on `centre()` that contains it.
    ///
    /// Used to frame a camera, and used in Lesson 6.8 to decide how far behind
    /// the scene a directional light's near plane has to start.
    [[nodiscard]] float radius() const
    {
        if (empty()) { return 0.0f; }
        const vec3 e = extent();
        return length(e) * 0.5f;
    }

    /// The eight corners, written into `out`.
    ///
    /// **The order is (x, then y, then z) with x varying fastest**, so corner `i`
    /// takes `max.x` when bit 0 is set, `max.y` when bit 1 is set and `max.z` when
    /// bit 2 is set. Fixed here so that a test can name a corner by index and mean
    /// the same one every time.
    void corners(vec3 out[8]) const
    {
        for (int i = 0; i < 8; ++i)
        {
            out[i] = {(i & 1) ? max.x : min.x,
                      (i & 2) ? max.y : min.y,
                      (i & 4) ? max.z : min.z};
        }
    }
};

/// The axis-aligned box containing this box's eight corners after `m`.
///
/// **Read the name literally.** This is the box around the transformed BOX, not
/// the box around the transformed contents — and under a rotation the two are
/// different, because an axis-aligned box has to grow to stay axis-aligned.
/// Applying it twice compounds that growth, which is the reason a scene keeps
/// object-space bounds and transforms them once rather than caching world-space
/// ones and re-transforming.
///
/// Lesson 6.8 uses it in the direction that is exactly right: world -> light
/// space, once, to find the extent of the scene along the light's axes.
[[nodiscard]] inline aabb transformed(const aabb& box, const mat4& m)
{
    if (box.empty()) { return {}; }

    vec3 c[8];
    box.corners(c);

    aabb out;
    for (const vec3& p : c)
    {
        // `point()` is Lesson 2.7's w = 1: this is a POSITION, so the matrix's
        // translation column applies in full. A direction would use `w = 0` and
        // would be the wrong question for a corner.
        const vec4 q = m * point(p);
        out.expand(vec3{q.x, q.y, q.z});
    }
    return out;
}

} // namespace engine

#endif // ENGINE_GFX_BOUNDS_HPP
