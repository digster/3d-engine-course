// engine/include/engine/gfx/shadow.hpp — what the light can see.
//
// Lesson 6.8. Every light this engine has ever had is *unoccluded*. `lambert()`
// asks whether a surface FACES the light and calls the answer illumination —
// which is a question about the surface's own orientation and nothing else. It
// cannot tell the difference between a floor in the open and the same floor
// under a table, because nothing in the shading equation has ever consulted the
// rest of the scene. That is why every object in every picture so far floats:
// contact shadows are the cue the eye uses for "this thing is resting on that
// thing", and we have never drawn one.
//
// THE IDEA IS ONE SENTENCE, AND THE ENGINE ALREADY OWNS EVERY PIECE OF IT.
// Render the scene from the light's point of view, keeping only the depth. What
// you get is a record of the nearest surface along every ray the light emits —
// which is exactly the question "what can the light see?", and exactly what a
// z-buffer computes. Lesson 3.1 built the z-buffer; this file points it in a
// different direction and reads it back.
//
// WHAT MAKES THE LESSON HARD IS NOT THE IDEA, IT IS THE COMPARISON. The number
// stored in the map and the number tested against it describe the same point on
// the same surface and are computed **differently**: one came out of rasterising
// a triangle from the light, sampled at a texel centre; the other came out of
// rasterising the same triangle from the camera and then being projected into
// the light's space. They disagree, always, and that disagreement has a name —
// **shadow acne** — and a magnitude that can be derived rather than guessed at.
// §4 of the lesson does the derivation; `slope_scaled_bias` below is its result,
// and every constant in this file is a consequence of it rather than a taste.
//
// WHAT THIS FILE DOES NOT DO, named rather than discovered later:
//
//   - **One light.** `lighting` has one key and this has one map. Two lights are
//     two maps and two lookups, which is a linear cost and no new ideas.
//   - **Directional only.** A spot light needs a perspective projection here
//     instead of an orthographic one, and inherits `perspective`'s non-uniform
//     depth distribution along with it — see §4.6.
//   - **One cascade.** A single map stretched over a whole outdoor scene gives
//     the near field far too few texels. Splitting the camera frustum into
//     ranges and giving each its own map is *cascaded* shadow mapping, and it is
//     an extension of this file rather than a replacement for it.
//   - **No texel snapping.** The fit below depends on the SCENE's bounds and not
//     on the camera's, so the light's box does not move when the camera does and
//     the shimmer that snapping exists to cure cannot occur. A camera-fitted box
//     — which is what cascades need — must snap, and that is where it belongs.

#ifndef ENGINE_GFX_SHADOW_HPP
#define ENGINE_GFX_SHADOW_HPP

#include <engine/gfx/bounds.hpp>
#include <engine/gfx/cull.hpp>
#include <engine/gfx/depth_buffer.hpp>
#include <engine/gfx/framebuffer.hpp>
#include <engine/gfx/light.hpp>
#include <engine/gfx/mesh.hpp>
#include <engine/gfx/projector.hpp>
#include <engine/gfx/scene.hpp>
#include <engine/gfx/soft_renderer.hpp>
#include <engine/gfx/viewport.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/vec3.hpp>

#include <memory>
#include <span>
#include <vector>

namespace engine {

/// The camera a directional light gets, and the three numbers derived from it
/// that the bias arithmetic needs.
///
/// **A DIRECTIONAL LIGHT HAS NO POSITION**, which is the whole difficulty. The
/// sun is modelled as parallel rays with no origin (`directional_light` carries a
/// direction and an irradiance and nothing else, Lesson 3.6), so there is no eye
/// to put a `perspective` frustum on. What replaces the pyramid is a BOX — an
/// `orthographic` projection, derived in `mat4.hpp` — and a box has to be fitted
/// to something. It is fitted to the scene's bounds.
struct light_camera
{
    /// World -> light view space. A pure rotation-and-translation, from
    /// `look_at` with the light's direction as the forward axis.
    mat4 view_from_world = mat4::identity();

    /// World -> light clip space: `orthographic * view_from_world`. What the
    /// shadow pass projects with, and what `visibility` projects with. **One
    /// matrix, formed once**, because two spellings of the same transform that
    /// drift apart by one term produce a shadow displaced from its caster, which
    /// looks like a bug in the fit.
    mat4 clip_from_world = mat4::identity();

    /// View -> light clip: the `orthographic` box alone, without the rotation.
    ///
    /// Kept beside the composed matrix because the two consumers want different
    /// halves: `collect_triangles` takes a `projector`, which is view -> clip by
    /// definition, while `visibility` wants world -> clip in one hop and must not
    /// pay for two multiplies per fragment.
    mat4 clip_from_view = mat4::identity();

    /// The map's pixel grid, including the y-flip. Shared with the pass that
    /// WROTE the map, so a lookup lands on the texel the write landed on.
    viewport vp{};

    /// **The side of one shadow texel, in world units.** The single most useful
    /// number in this file: the map's resolving power, in metres. Everything the
    /// bias derivation needs is this times a trigonometric factor.
    float world_per_texel = 0.0f;

    /// `far - near`, in world units — how many metres one unit of DEVICE depth
    /// is worth. The bias is derived in world units and applied in device units,
    /// and this is the conversion between them.
    ///
    /// It is a single number, and that is a property of `orthographic` rather
    /// than a convenience: an orthographic projection makes device depth an
    /// AFFINE function of view-space depth, so one device unit is worth the same
    /// number of metres everywhere in the box. `perspective` cannot say that —
    /// §4.6, and Lesson 4.9 measured what it costs.
    float depth_range = 1.0f;

    /// Is this a real fit, or the default?
    [[nodiscard]] bool valid() const { return world_per_texel > 0.0f; }
};

/// Fit a directional light's orthographic box around `scene`.
///
/// **The box is made SQUARE in x and y**, and that is a decision rather than an
/// oversight. The map is square, so a non-square box would give texels that
/// resolve better along one axis than the other — and every formula in §4 says
/// "one texel" as though a texel had a single size. Squaring costs resolution on
/// the narrow axis and buys one number, `world_per_texel`, that is honest in
/// every direction.
///
/// The near and far planes come out of the fit as well, from the scene's extent
/// **along the light's axis**, which is why `orthographic` had to accept a
/// negative near plane: the scene straddles whatever origin we picked, and half
/// of it is behind.
///
/// @param key        the light. Only its `direction` is read.
/// @param scene      world-space bounds of everything that CASTS a shadow.
/// @param resolution the map's side, in texels. Must be > 0.
[[nodiscard]] light_camera fit_directional(const directional_light& key,
                                           const aabb& scene, int resolution);

/// Which bias policy to apply. [B] cycles in the demo, and every one of them is
/// reachable **because the failures are the lesson** (Lesson 3.5's rule).
enum class shadow_bias
{
    /// No bias at all. **This is the acne**, and it is the first picture the
    /// lesson shows: a moiré of dark stripes across every lit surface, at the
    /// shadow map's texel frequency, because half of every texel's footprint is
    /// downhill of the point the map sampled.
    none,

    /// A single constant subtracted everywhere, in device-depth units.
    ///
    /// The obvious fix, and the one every first implementation writes. It cannot
    /// be right, because the error it is cancelling is proportional to the
    /// surface's slope and this is not — so it must be sized for the STEEPEST
    /// surface in the scene, and every flatter surface then pays a bias it did
    /// not need. That surplus is **peter-panning**: shadows detach from the
    /// objects casting them, worst where an object touches the ground.
    constant,

    /// `constant + slope_scaled_bias(...)` — the derived answer, and the default.
    ///
    /// §4.4 derives it: the depth error is at most half a texel diagonal of
    /// lateral travel times the surface's depth gradient, and the gradient is
    /// `tan(theta)` where `cos(theta) = n . l`. Each surface therefore gets
    /// exactly the bias its own geometry requires and no more.
    slope_scaled,

    /// Move the SAMPLE POINT along the surface normal instead of moving its
    /// depth — §4.5, and the first place Lesson 6.7's new fact matters, because
    /// the normal that must be used here is the GEOMETRIC one and 6.7 gave every
    /// surface a shading normal that disagrees with it.
    ///
    /// **It converts the error rather than removing it.** Depth error grows
    /// without bound as the surface turns edge-on to the light; lateral error is
    /// bounded by construction, because the offset never exceeds one texel. What
    /// it buys at grazing angles it pays for near silhouettes, where the lookup
    /// has moved sideways off the caster.
    normal_offset
};

[[nodiscard]] const char* name_of(shadow_bias b);

/// The knobs, gathered — the same argument `fill_style` (3.2) and `projector`
/// (3.3) made, arriving for state that always travels together.
struct shadow_settings
{
    shadow_bias bias = shadow_bias::slope_scaled;

    /// The constant term, in DEVICE depth units, under `constant` and
    /// `slope_scaled`. Defaults to zero under `slope_scaled` because the derived
    /// term already covers the geometry; what a constant is genuinely for is the
    /// map's *quantisation*, which `quantisation_bias()` computes exactly.
    float constant_bias = 0.0f;

    /// A multiplier on the derived slope term. **1.0 is the derivation**, and a
    /// number other than 1 is an admission that something upstream is not what
    /// the derivation assumed — which is worth knowing rather than absorbing
    /// silently into a magic constant.
    float slope_scale = 1.0f;

    /// A multiplier on the derived normal offset, in texels. 1.0 means "one
    /// texel at grazing incidence, nothing head-on".
    float normal_scale = 1.0f;

    /// Clamp on `tan(theta)`. At exactly grazing incidence the required bias is
    /// infinite and the honest answer is that a shadow map cannot resolve that
    /// surface at all; this is where we stop pretending. 10 corresponds to about
    /// 84 degrees.
    float max_slope = 10.0f;

    /// PCF kernel radius in texels: 0 is a single tap, 1 is 3x3, 2 is 5x5.
    ///
    /// **The taps are averaged AFTER the comparison, never before** — §6 shows
    /// with two numbers why filtering depths and then comparing is not a blurrier
    /// answer but a wrong one.
    int pcf_radius = 1;

    /// How dark a fully shadowed surface goes: 1 removes the key light entirely,
    /// 0 disables the shadow. **Not physical** — a real shadow is exactly the
    /// absence of the direct term — and it exists so the lesson can fade the
    /// effect in and out over an otherwise identical frame.
    float strength = 1.0f;

    /// Which faces the shadow pass keeps.
    ///
    /// **`front` is the third acne cure**, and it is free: store only the BACK
    /// faces of casters, and every front face being shaded is then compared
    /// against a surface that is genuinely behind it. It works perfectly on
    /// closed geometry and fails completely on a ground plane, which has no back
    /// face to store — so the default is `none` and the demo puts it on a key.
    cull_mode cull = cull_mode::none;
};

/// What one shadow pass did. The HUD's numbers, and the evidence in §7.
struct shadow_stats
{
    int objects = 0;        ///< submitted
    int triangles = 0;      ///< after clipping, before culling
    int texels = 0;         ///< the map's area — how much memory the pass owns
    double render_ms = 0.0; ///< wall time of the depth pass
};

/// **`tan(theta)` from a cosine, clamped** — the whole of the slope term.
///
/// `n_dot_l` is `cos(theta)`, the angle between the surface normal and the
/// direction to the light, and `tan = sqrt(1 - c^2) / c`. It is written out here
/// rather than inlined at the two call sites because the CPU and the GPU must
/// agree on it to the bit, and a formula that exists twice eventually exists in
/// two versions.
///
/// Returns 0 for a surface facing away from the light: it is in shadow by its own
/// geometry (Lesson 3.6's clamp) and no shadow map is consulted.
[[nodiscard]] inline float slope_from_cosine(float n_dot_l, float max_slope)
{
    if (n_dot_l <= 0.0f) { return 0.0f; }
    const float c = (n_dot_l > 1.0f) ? 1.0f : n_dot_l;
    const float s2 = 1.0f - c * c;
    const float t = (s2 <= 0.0f) ? 0.0f : std::sqrt(s2) / c;
    return (t > max_slope) ? max_slope : t;
}

/// **How far the furthest tap can be from the fragment, in texels** — the
/// lateral reach the bias has to cover.
///
/// For a single tap (`radius == 0`) the answer is half a texel diagonal,
/// `sqrt(2)/2 = 0.7071`: the fragment sits somewhere inside its texel and the
/// stored depth was measured at that texel's centre.
///
/// **AND FOR PCF IT IS MUCH LARGER, WHICH IS THE FINDING §6.3 IS BUILT ON.** A
/// 3x3 kernel reads a texel one step away in each direction, so its furthest tap
/// is `1.5` texels away in each axis — `1.5 * sqrt(2) = 2.12` texels along the
/// diagonal, THREE TIMES the single-tap reach. Sizing the bias for one tap and
/// then turning PCF on puts the acne straight back, and it comes back looking
/// like a filtering bug rather than a bias one. Measured: on this course's
/// `shapes.glb` scene a correctly biased single-tap render has 78 stray pixels
/// and the same bias under a 3x3 kernel has **10,348**.
[[nodiscard]] inline float pcf_reach_texels(int radius)
{
    constexpr float k_root2 = 1.41421356237309505f;
    const float r = (radius < 0) ? 0.0f : static_cast<float>(radius);
    return (r + 0.5f) * k_root2;
}

/// The derived slope-scaled bias, in **device depth units**.
///
/// §4.4, in one line. The fragment and the furthest tap are at most
/// `reach_texels * world_per_texel` apart laterally, and walking that far across
/// a surface tilted at `theta` changes its depth by that distance times
/// `tan(theta)`. Divide by `depth_range` to express the answer in the units the
/// comparison is actually performed in.
///
/// Worked example (§4.4): a 1024-texel map fitted to a 20-metre box gives
/// `world_per_texel = 0.01953`; a surface at 60 degrees to the light has
/// `tan = 1.7321`; with a 24-metre depth range and a single tap the bias is
/// `0.7071 * 0.01953 * 1.7321 / 24 = 9.966e-4` — about a thousandth of the depth
/// range. The same surface at 84 degrees needs ten times that, and under a 3x3
/// kernel three times again.
[[nodiscard]] inline float slope_scaled_bias(float world_per_texel, float slope,
                                             float depth_range, float reach_texels)
{
    if (depth_range <= 0.0f) { return 0.0f; }
    return reach_texels * world_per_texel * slope / depth_range;
}

/// Half of one depth code, in device units — the bias the FORMAT requires,
/// independent of any geometry.
///
/// The stored value was rounded to the nearest code the depth buffer can hold
/// (`depth_buffer::quantise`), so it can be wrong by half a code in either
/// direction before any geometry is involved. This is exactly that, and it is a
/// CONSTANT across the whole map — which is a property of the orthographic
/// projection, not a simplification. Under `perspective` the same half-code is
/// worth wildly different amounts of world depth at different distances, which is
/// what Lesson 4.9 measured and what makes spot-light shadow maps harder than
/// this one.
[[nodiscard]] float quantisation_bias(depth_format format);

/// A depth buffer rendered from a light, plus everything needed to read it back.
///
/// **Move-only and heap-backed**, because it owns two full-resolution buffers and
/// a scratch list; copying one by accident would be an invisible megabyte.
class shadow_map
{
public:
    shadow_map() = default;

    shadow_map(const shadow_map&) = delete;
    shadow_map& operator=(const shadow_map&) = delete;
    shadow_map(shadow_map&&) noexcept = default;
    shadow_map& operator=(shadow_map&&) noexcept = default;

    /// Allocate a `resolution` x `resolution` map.
    ///
    /// @param format the depth format to imitate. `f32` is exact and is what a
    ///        CPU buffer naturally is; `unorm16` and `unorm24` round on write
    ///        (Lesson 3.1) and are here so that §4.3's quantisation term can be
    ///        *seen* rather than only computed.
    [[nodiscard]] bool create(int resolution, depth_format format = depth_format::f32);

    [[nodiscard]] bool valid() const { return depth_ != nullptr; }
    [[nodiscard]] int resolution() const { return resolution_; }

    /// Rasterise `objects` from the light and keep only the depth.
    ///
    /// The scene's bounds are computed here, from the same meshes about to be
    /// drawn, so the fit can never disagree with the contents. Objects whose
    /// `mesh_handle` does not resolve are skipped, exactly as in
    /// `collect_triangles`.
    void render(std::span<const scene_object> objects, const mesh_pool& meshes,
                const directional_light& key, shadow_stats* stats = nullptr);

    /// **Is this point lit?** 1 for fully lit, 0 for fully shadowed, and the
    /// values between are what PCF produces.
    ///
    /// @param world_pos  the fragment's world-space position.
    /// @param geometric_normal the normal the GEOMETRY has, unit length. Under
    ///        `normal_offset` this is the direction the sample is moved along,
    ///        and it must be the geometric normal rather than Lesson 6.7's
    ///        mapped one — the artefact being cured is a fact about where the
    ///        triangles are, and a normal map does not move a triangle.
    /// @param n_dot_l    the cosine the shading equation already computed.
    ///        Passed in rather than recomputed, because two spellings of one
    ///        cosine is how a bias ends up disagreeing with the term it biases.
    [[nodiscard]] float visibility(vec3 world_pos, vec3 geometric_normal,
                                   float n_dot_l) const;

    [[nodiscard]] const light_camera& camera() const { return cam_; }
    [[nodiscard]] const depth_buffer* depth() const { return depth_.get(); }
    [[nodiscard]] const aabb& scene_bounds() const { return bounds_; }

    [[nodiscard]] shadow_settings& settings() { return set_; }
    [[nodiscard]] const shadow_settings& settings() const { return set_; }

    /// The scene's world-space bounds, computed from `objects` and `meshes`.
    ///
    /// Public and free-standing because it is the piece `demos/gltf_view` has
    /// been writing by hand since Lesson 6.6 to frame its camera, and a second
    /// caller is what promotes a loop into a function.
    [[nodiscard]] static aabb bounds_of(std::span<const scene_object> objects,
                                        const mesh_pool& meshes);

private:
    /// Heap-held because neither has a default constructor and both are large.
    std::unique_ptr<depth_buffer> depth_;

    /// **The colour target nothing writes to, and the reason it exists anyway.**
    ///
    /// `fill_triangle` clips its bounding box against the FRAMEBUFFER's
    /// dimensions, so a depth-only pass still has to hand it a colour target of
    /// the right size to say how big the pass is — even with
    /// `fill_style::depth_only` set, which skips the shading and the store. On
    /// the GPU there is genuinely no colour attachment: the render pass carries
    /// its own dimensions, `num_color_targets` is 0, and on tiled hardware the
    /// saving is the entire tile write. That gap is what a depth-only pass IS,
    /// and it is visible here as four megabytes that are allocated and never
    /// touched.
    std::unique_ptr<framebuffer> unused_colour_;

    light_camera cam_{};
    shadow_settings set_{};
    aabb bounds_{};
    int resolution_ = 0;

    /// Reused across frames: `clear()` keeps the capacity, so a steady-state
    /// frame allocates nothing (`collect_triangles`' bargain, Lesson 3.10).
    std::vector<raster_triangle> tris_;
    projection_scratch scratch_;
};

} // namespace engine

#endif // ENGINE_GFX_SHADOW_HPP
