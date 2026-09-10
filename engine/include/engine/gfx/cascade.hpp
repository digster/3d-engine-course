// engine/include/engine/gfx/cascade.hpp — one map is not enough.
//
// Lesson 6.9. Lesson 6.8 fitted a single orthographic box around the **whole
// scene** and rendered one depth map into it. Run `gltf_view --shadow 128` and
// you can see what that costs: the box must contain the far field, so the near
// field — the part the camera is actually looking at — gets whatever texels are
// left over. A 40-metre scene at 1024 texels is 39 mm per texel *everywhere*,
// including on the object two metres from your eye that fills half the screen.
//
// THE FIX IS A CHANGE OF QUESTION. 6.8 asked "what box contains the scene?" The
// scene is not what needs resolving — the **camera's frustum** is, and a frustum
// is mostly far away. Split it by distance into a few slices, fit a box around
// each slice, and give each its own map. The near slice is small, so its map is
// dense; the far slice is huge, so its map is coarse, and coarse is fine at
// forty metres because a texel there covers a fraction of a pixel anyway.
//
// THAT IS THE WHOLE IDEA. What makes it a lesson is that a camera-fitted box
// **moves**, and 6.8's box never did:
//
//   1 A BOX THAT MOVES SHIMMERS. 6.8's fit depends on the scene's bounds, so
//     walking the camera around changes nothing about the light's box and the
//     texel grid is nailed to the world. A slice-fitted box slides and resizes
//     every frame, its grid slides under the geometry, and every shadow edge
//     crawls and boils. `fit_directional_slice` cures this twice — a **sphere**
//     so the box cannot change SIZE when the camera merely turns, and **texel
//     snapping** so it cannot change PHASE when the camera moves. Both are
//     derived in the lesson; neither is a tweak.
//
//   2 EACH CASCADE HAS ITS OWN `world_per_texel`, AND THEREFORE ITS OWN BIAS.
//     This is where 6.8's §4 gets audited. Every bias term there was a multiple
//     of `world_per_texel`, and if that derivation was real then a cascade needs
//     **no new bias code at all** — just its own `light_camera`. It needed none.
//     A formula that survives a change in the quantity it was derived from is a
//     formula; one that needs a new fudge factor per cascade never was.
//
//   3 THE SEAM IS A NEW ARTEFACT WITH A NEW NAME. Two cascades meet at a split
//     distance with different texel grids and different biases, so they disagree
//     along that line and the disagreement is visible as a hard edge in the
//     shadowing. Blending a band either side is the usual answer and costs a
//     second lookup inside the band. `cascade_settings::blend_fraction`.
//
// WHAT THIS FILE DOES NOT DO:
//
//   - **No per-cascade resolution.** Every cascade is the same square. Real
//     engines often shrink the far ones; it is a memory decision, not an idea.
//   - **No caching.** Every cascade re-renders every frame. Distant cascades
//     change slowly and can be updated every Nth frame, which is the single
//     biggest performance win available here and is left as an exercise.
//   - **Directional only**, inherited from 6.8 for the same reason.

#ifndef ENGINE_GFX_CASCADE_HPP
#define ENGINE_GFX_CASCADE_HPP

#include <span>
#include <vector>

#include <engine/gfx/bounds.hpp>
#include <engine/gfx/shadow.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/vec3.hpp>

namespace engine
{

/// The most a cascade count may be. Four is the usual answer and the shader's
/// uniform block is sized for it; see `cascade_uniforms` in gpu_uniform.hpp.
inline constexpr int k_max_cascades = 4;

/// The parameters of the CAMERA's frustum, as the fit needs them.
///
/// Not a `mat4` projection, and that is deliberate. To fit a slice we need the
/// eight world-space corners of the sub-frustum between two distances, which is
/// an *inverse* projection — and `mat4` has no general `inverse()`: 2.9's
/// `rigid_inverse` handles a view matrix and nothing handles a perspective one.
///
/// We could write that inverse (a perspective matrix's is closed-form and
/// short). Building the corners straight from the field of view is fewer lines,
/// has no matrix to get transposed, and is what most engines do — the frustum's
/// shape is three numbers, and going through a matrix to recover them is a
/// round trip that can only lose precision.
struct camera_frustum
{
    /// Camera-to-world. This is `rigid_inverse(view_from_world)` — the view
    /// matrix inverted, which IS available because a view matrix is rigid.
    mat4 world_from_view = mat4::identity();

    /// Vertical field of view, radians. The same number handed to `perspective`.
    float fovy_radians = 1.0f;

    /// Width over height.
    float aspect = 1.0f;
};

/// The eight world-space corners of one sub-frustum.
///
/// Ordering is the same bit pattern `aabb::corners` uses — bit 0 is +x, bit 1 is
/// +y, bit 2 is the FAR plane — so a test can name a corner by index and mean
/// the same thing in both places.
struct frustum_slice
{
    vec3 corners[8]{};
};

/// Where to cut the frustum between `near_d` and `far_d`, for cut `index` of
/// `count`. Returns a distance from the eye, in world units.
///
/// **The two obvious schemes are both wrong, in opposite directions.**
///
/// *Uniform* splits put the cuts at equal world distances: near + (far−near)·i/n.
/// Every slice is the same depth, and since a slice's box scales with how WIDE
/// the frustum is there, the far slices are enormous and the near ones tiny —
/// which sounds right until you notice that the near slice, the one filling your
/// screen, got the same map as the far one and did not need it.
///
/// *Logarithmic* splits put them at near·(far/near)^(i/n), and that ratio comes
/// out of a real argument: a surface at distance d covers screen pixels in
/// proportion to 1/d, so to hold TEXELS-PER-PIXEL constant the slice depths must
/// grow in proportion to d — which is exactly a geometric series. This is the
/// same 1/d that Lesson 4.7 met as depth precision, arriving from the other
/// side. It is theoretically right and practically miserable: with near = 0.1 m
/// the first cut lands around 0.3 m, so cascade 0 is spent on your own shoes.
///
/// **The practical scheme is the blend**, from Zhang et al. (2006):
///
///     d_i = lambda·log_i + (1 − lambda)·uniform_i
///
/// and `lambda` is the one number in this file chosen by eye rather than
/// derived. 0.5 is the usual starting point. That it is a *blend of two
/// principled schemes* rather than a magic curve is the point — each end is
/// explicable, and lambda says how much you believe the near field matters.
[[nodiscard]] float practical_split(int index, int count, float near_d,
                                    float far_d, float lambda);

/// The eight world-space corners of the sub-frustum between two eye distances.
///
/// Straight from the field of view: at distance d the half-height is
/// tan(fovy/2)·d and the half-width is that times the aspect ratio. Four corners
/// at `near_d`, four at `far_d`, all in view space, all pushed through
/// `world_from_view`.
///
/// Note that −z is forward (conventions §2), so a corner at distance d sits at
/// z = −d. Getting that sign wrong builds the frustum BEHIND the camera, and the
/// symptom is a shadow map that is correct only when you turn around.
[[nodiscard]] frustum_slice slice_corners(const camera_frustum& cam,
                                          float near_d, float far_d);

/// Fit a directional light's box around one frustum slice.
///
/// **The box is fitted to the slice's bounding SPHERE, not to its corners**, and
/// that is the whole anti-shimmer argument in one decision. A box fitted tightly
/// to eight corners changes size as the camera yaws, because the corners rotate
/// within it. Change the box size and `world_per_texel` changes, so every texel
/// in the map changes size, so every shadow edge in the picture moves — while
/// the camera merely turned on the spot. A sphere has no orientation: its radius
/// is the same whichever way the camera is facing, so the box side is constant
/// and the only thing left that moves is its POSITION.
///
/// **Then snapping deals with the position.** The box still translates as you
/// walk, and a grid that slides continuously under fixed geometry makes edges
/// crawl. Round the box centre, in light space, down to a whole number of
/// texels. The grid then advances in texel-sized jumps and lands on the same
/// world positions it landed on last frame, so a shadow edge either stays where
/// it was or moves exactly one texel.
///
/// @param key        the light. Only its `direction` is read.
/// @param slice      the sub-frustum to cover.
/// @param casters    world bounds of everything that CASTS. Used only for the
///                   near and far planes: an occluder BETWEEN the light and the
///                   slice is outside the slice but must still be drawn, so the
///                   depth range is pulled back to the casters' extent along the
///                   light axis. Miss this and objects stop casting the moment
///                   they leave the camera's view, which reads as shadows
///                   blinking out at the edge of the screen.
/// @param resolution the map's side, in texels.
/// @param snap       apply texel snapping. Exposed so the demo can turn it off
///                   and show the crawl, per Lesson 3.5's rule.
[[nodiscard]] light_camera fit_directional_slice(const directional_light& key,
                                                 const frustum_slice& slice,
                                                 const aabb& casters,
                                                 int resolution, bool snap);

/// How the frustum is cut and how the seams are handled.
struct cascade_settings
{
    /// How many cascades. Clamped to [1, `k_max_cascades`].
    int count = 4;

    /// Blend between logarithmic (1) and uniform (0) splits. See
    /// `practical_split`.
    float lambda = 0.5f;

    /// Snap each cascade's box to its own texel grid.
    bool snap = true;

    /// How much of each cascade's depth to spend fading into the next, as a
    /// fraction of that cascade's own extent. 0 gives the hard seam — which is
    /// the picture the lesson shows first. 0.1 is a reasonable cure.
    float blend_fraction = 0.0f;

    /// The camera distances the whole cascade set covers. `far_d` is usually
    /// SHORTER than the camera's own far plane: shadows at 500 m are not worth
    /// a cascade, and stopping early makes every other cascade denser.
    float near_d = 0.1f;
    float far_d = 60.0f;
};

/// Which cascade a view-space depth falls in, and how far through its blend band
/// it is.
struct cascade_choice
{
    int index = 0;      ///< the cascade to read
    int next = 0;       ///< the one after it, or `index` in the last cascade
    float blend = 0.0f; ///< 0 = use `index` alone, 1 = use `next` alone
};

/// A set of shadow maps, one per frustum slice.
///
/// Each cascade is an ordinary 6.8 `shadow_map` with a different camera — which
/// is the strongest statement this file makes. Nothing about the rasterisation,
/// the depth compare, the PCF kernel or the bias needed to change; a cascade is
/// a fit, not a feature.
class cascaded_shadow_map
{
public:
    cascaded_shadow_map() = default;

    cascaded_shadow_map(const cascaded_shadow_map&) = delete;
    cascaded_shadow_map& operator=(const cascaded_shadow_map&) = delete;
    cascaded_shadow_map(cascaded_shadow_map&&) noexcept = default;
    cascaded_shadow_map& operator=(cascaded_shadow_map&&) noexcept = default;

    /// Allocate `count` maps of `resolution` square.
    [[nodiscard]] bool create(int count, int resolution,
                              depth_format format = depth_format::f32);

    [[nodiscard]] bool valid() const { return !maps_.empty(); }
    [[nodiscard]] int count() const { return static_cast<int>(maps_.size()); }
    [[nodiscard]] int resolution() const { return resolution_; }

    /// Fit every cascade to the camera and render each one.
    ///
    /// The caster bounds are computed once, here, from the same meshes about to
    /// be drawn — 6.8's rule, for 6.8's reason: a fit that disagrees with the
    /// contents is a bug that only shows on some frames.
    void render(std::span<const scene_object> objects, const mesh_pool& meshes,
                const directional_light& key, const camera_frustum& cam,
                shadow_stats* stats = nullptr);

    /// **Is this point lit?** `view_depth` is the positive distance from the eye
    /// along the view axis, and it is what selects the cascade — the same number
    /// the splits were computed in, so selection cannot disagree with the fit.
    [[nodiscard]] float visibility(vec3 world_pos, vec3 geometric_normal,
                                   float n_dot_l, float view_depth) const;

    /// Which cascade covers `view_depth`, and the blend weight there.
    [[nodiscard]] cascade_choice choose(float view_depth) const;

    /// The far distance of cascade `i`. `splits()[count()-1]` is `far_d`.
    [[nodiscard]] std::span<const float> splits() const { return splits_; }

    [[nodiscard]] const shadow_map& map(int i) const { return maps_[static_cast<std::size_t>(i)]; }
    [[nodiscard]] shadow_map& map(int i) { return maps_[static_cast<std::size_t>(i)]; }

    [[nodiscard]] const aabb& caster_bounds() const { return casters_; }

    [[nodiscard]] cascade_settings& settings() { return set_; }
    [[nodiscard]] const cascade_settings& settings() const { return set_; }

    /// Apply one bias/PCF policy to every cascade. The per-cascade part of the
    /// bias is `world_per_texel`, which lives in each cascade's own
    /// `light_camera` and is therefore already right — this only copies the
    /// POLICY, never a magnitude.
    void apply_shadow_settings(const shadow_settings& s);

private:
    std::vector<shadow_map> maps_;
    std::vector<float> splits_;
    cascade_settings set_{};
    aabb casters_{};
    int resolution_ = 0;
};

} // namespace engine

#endif // ENGINE_GFX_CASCADE_HPP
