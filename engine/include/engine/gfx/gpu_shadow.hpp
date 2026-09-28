// engine/include/engine/gfx/gpu_shadow.hpp — the shadow map, on the GPU.
//
// Lesson 6.8. `gfx/shadow.hpp` is the same idea on the CPU and is where every
// derivation in the lesson is done; this file is the port, and the port is
// interesting for exactly three reasons, all of them things the software version
// could not express.
//
//   1. **A SECOND RENDER PASS.** Everything the GPU renderer has drawn since
//      Lesson 4.8 has happened inside one `SDL_BeginGPURenderPass`. A shadow map
//      is a pass that runs BEFORE that one, into a different attachment, with a
//      different pipeline — and passes are the unit SDL_GPU synchronises on, so
//      the write and the later read need no barrier of ours at all.
//
//   2. **A PASS WITH NO COLOUR ATTACHMENT.** `num_color_targets = 0` and
//      `has_depth_stencil_target = true`, which the CPU rasterizer cannot say
//      (`fill_style::depth_only` still needs a framebuffer to size the pass).
//      On tiled hardware — every phone, and Apple silicon — that absence is the
//      whole tile write, and it is the single largest saving in this lesson.
//
//   3. **A DEPTH TARGET THAT IS ALSO A TEXTURE**, which is `create_depth`'s new
//      `sampled` flag, and which forces `store_op` to `STORE` where the scene's
//      own depth buffer has been `DONT_CARE` since 4.7. The comment in
//      `gpu_scene.cpp` predicted this exact moment.
//
// WHAT THIS FILE DOES NOT OWN. The fit — `fit_directional`, in shadow.hpp — is
// shared, because the arithmetic that decides where a light's box goes is not a
// GPU question and having two versions of it is how a CPU shadow and a GPU
// shadow end up in different places.

#ifndef ENGINE_GFX_GPU_SHADOW_HPP
#define ENGINE_GFX_GPU_SHADOW_HPP

#include <engine/gfx/cascade.hpp>
#include <engine/gfx/gpu_debug.hpp>
#include <engine/gfx/gpu_device.hpp>
#include <engine/gfx/gpu_pipeline.hpp>
#include <engine/gfx/gpu_scene.hpp>
#include <engine/gfx/gpu_texture.hpp>
#include <engine/gfx/shadow.hpp>

#include <span>
#include <vector>

namespace engine {

/// **How far a GPU shadow lookup reads, in texels: `(radius + 1) * sqrt(2)`.**
///
/// Not `pcf_reach_texels`, which is the CPU's `(radius + 1/2) * sqrt(2)`, and the
/// difference is the FILTER, not the kernel. The CPU compares the nearest texel,
/// whose sample is at most half a texel away along each axis. Every GPU tap here
/// is `SampleCmp` through a LINEAR comparison sampler, which compares a 2x2 block
/// and blends the four answers — and any of the four can carry weight while a
/// whole texel away along each axis. The bias has to cover the furthest depth
/// the filter reads, so it is sized for this.
///
/// Lesson 6.17b found it for the lamps (11,098 disagreeing pixels -> 0) and the
/// fix after it measured it for the sun: on a bare ground, with one tap, the
/// CPU's reach left 27-36% of the pixels as acne at every elevation from 20 to
/// 75 degrees, and this reach leaves none (`verify_617b` §K). With the default
/// 3x3 kernel neither shows acne, which is how the sun's lookup carried the
/// CPU's number from 6.8 to here.
[[nodiscard]] inline float gpu_pcf_reach_texels(int radius)
{
    const int r = (radius < 0) ? 0 : radius;
    return (static_cast<float>(r) + 1.0f) * 1.41421356f;
}

/// Owns the depth texture, the depth-only pipeline and the comparison sampler
/// that a GPU shadow map is made of.
///
/// **Movable, not copyable**, like every device resource in this engine.
class gpu_shadow_map
{
public:
    gpu_shadow_map() = default;
    ~gpu_shadow_map();

    gpu_shadow_map(const gpu_shadow_map&) = delete;
    gpu_shadow_map& operator=(const gpu_shadow_map&) = delete;

    /// Build everything.
    ///
    /// @param vertex   `shadow.vert` — position only.
    /// @param fragment `shadow.frag` — writes nothing. See that file for why it
    ///        exists rather than being NULL.
    /// @param resolution the map's side in texels; square, because
    ///        `fit_directional` squares the box so that "one texel" has a single
    ///        size.
    /// @param format the depth format, from `supported_depth_format`. **Prefer
    ///        `D32_FLOAT` here even though the scene's own buffer does not need
    ///        it**: a shadow map's precision budget is spent on a comparison
    ///        rather than on ordering two nearby surfaces, and §4.3 measures what
    ///        a 16-bit map costs in bias.
    /// @param layers how many cascades. Lesson 6.9. **The texture is always a
    ///        2D ARRAY, even at one layer**, so the single-map case of 6.8 is
    ///        the one-cascade case of 6.9 and the shader has exactly one code
    ///        path. A `Texture2D` binding and a `Texture2DArray` binding cannot
    ///        both be right, and carrying two would mean two shaders.
    [[nodiscard]] bool create(const gpu_device& dev,
                              SDL_GPUShader* vertex, SDL_GPUShader* fragment,
                              int resolution, SDL_GPUTextureFormat format,
                              int layers = 1);

    void destroy();

    [[nodiscard]] bool valid() const { return pipeline_.valid() && depth_.valid(); }
    [[nodiscard]] int resolution() const { return resolution_; }

    /// Record the whole depth pass onto `cb`: begin, draw, end.
    ///
    /// **It owns its own pass**, which is the opposite of `gpu_scene_renderer::
    /// render` — that one is handed a pass because it draws into the caller's
    /// swapchain and the caller decides what else shares it. Nothing else can
    /// ever share this attachment, so a caller who had to open the pass would
    /// only be able to get it wrong.
    ///
    /// Items are drawn in the order given and `item.style` is ignored: a depth
    /// pass has one pipeline. `item.texture`, `item.normal_map` and
    /// `item.material` are ignored too — there is no fragment stage to read them.
    ///
    /// @param cam the fit, from `fit_directional`. Its `clip_from_world` is the
    ///        only per-frame uniform this pass has.
    /// @param layer which cascade to render into.
    ///        `SDL_GPUDepthStencilTargetInfo` has a `layer` field and no way to
    ///        say "all of them", so N cascades are N passes — which they were
    ///        always going to be anyway, since each has its own camera.
    void render(SDL_GPUCommandBuffer* cb, const gpu_draw_item* items, int count,
                const light_camera& cam, frame_log* log = nullptr,
                int layer = 0) const;

    /// Record the draws into a pass **somebody else** began — Lesson 6.17.
    ///
    /// `render` above is now this function with a begin and an end around it, so
    /// there is one copy of the viewport, the pipeline bind and the draw loop
    /// rather than two. The split exists because a frame graph derives the
    /// attachment, the layer and both ops from the passes' declared dataflow, so
    /// a pass that begins its own attachment is a pass the graph cannot schedule.
    ///
    /// **The layer is not a parameter here**, and that is the point: under the
    /// graph it is part of the write declaration, which is the only place that
    /// can also know whether the previous cascade's contents must survive.
    void render_into(SDL_GPUCommandBuffer* cb, SDL_GPURenderPass* pass,
                     const gpu_draw_item* items, int count,
                     const light_camera& cam, frame_log* log = nullptr) const;

    /// The map, for binding into the scene pass at fragment slot 2.
    [[nodiscard]] SDL_GPUTexture* texture() const { return depth_.handle(); }

    /// The comparison sampler that goes with it. See
    /// `gpu_sampler::create_comparison` for why an ordinary one will not do.
    [[nodiscard]] SDL_GPUSampler* sampler() const { return compare_.handle(); }

    [[nodiscard]] SDL_GPUTextureFormat format() const { return depth_.format(); }

    /// Fill in `scene_light_uniforms`' shadow half from a fit and a settings
    /// block, so the CPU and GPU paths cannot disagree about what a bias means.
    ///
    /// **One function, because the alternative is two transcriptions of eleven
    /// floats** — and eleven floats transcribed twice is a shadow that behaves
    /// differently on the two renderers for a reason nobody can find.
    static void fill_uniforms(scene_light_uniforms& out, const light_camera& cam,
                              const shadow_settings& set, int resolution);

    /// Fill in the per-cascade block. Lesson 6.9, and the same argument as
    /// `fill_uniforms`: the splits, the matrices and the per-cascade texel sizes
    /// are transcribed once or they are transcribed inconsistently.
    static void fill_cascade_uniforms(cascade_uniforms& out,
                                      const cascaded_shadow_map& csm,
                                      vec3 view_forward);

    [[nodiscard]] int layers() const { return layers_; }

private:
    gpu_texture depth_;
    int layers_ = 1;
    gpu_sampler compare_;
    gpu_pipeline pipeline_;
    int resolution_ = 0;
};

// ===========================================================================
// Lesson 6.17b — local lights on the GPU
// ===========================================================================

/// **An unshadowed local light, packed for the shader.** Lesson 6.17b.
///
/// Every field the shader reads that does not depend on a shadow map: the
/// position and range, `colour * intensity` premultiplied (so the fragment
/// multiplies once), the kind, the spot axis and glTF's cone scale and offset
/// (`cone_terms_of` — the CPU computes the cosines, the shader never calls
/// `cos`). The shadow slot is -1.
///
/// A free function rather than a `gpu_local_shadows` member because a scene
/// can have lights and no shadow maps at all, and that caller should not have
/// to create a shadow object to describe a lamp.
[[nodiscard]] gpu_local_light pack_local_light(const local_light& light);

/// One depth pass a set of local shadows needs this frame: a spot light's map,
/// or one face of a point light's cube. Reported so a caller can DECLARE each
/// pass to a frame graph (6.17) rather than have them begun behind its back.
struct local_shadow_job
{
    int light = -1;                 ///< index into the span `prepare` was given
    local_light_kind kind = local_light_kind::point;
    int face = 0;                   ///< the cube face, 0-5 in SDL order; 0 for a spot
    SDL_GPUTexture* texture = nullptr;   ///< the spot array or the cube array
    Uint32 layer = 0;               ///< spot: its slot; point: 6 * slot + face
    int resolution = 0;
    light_camera cam{};             ///< what the pass projects with
};

/// **Every local light's shadow maps on the GPU** — one layer of a depth 2D
/// array per shadowed spot, one cube of a depth CUBE ARRAY per shadowed
/// point. Lesson 6.17b.
///
/// **A FIXED BUDGET OF SLOTS**, chosen at `create`, and that is the honest
/// shape of the problem rather than a shortcut. The shader binds ONE spot
/// texture and ONE cube texture — a fragment shader's texture slots are
/// declared, not counted at run time — so the number of shadowed lights a
/// frame can have is a CAPACITY of the binding layout, and choosing it is a
/// memory decision made once rather than a per-frame one. Lights past the
/// budget are packed without a shadow and counted in `dropped()`, never
/// silently shadowed wrongly. (The alternative that removes the cap — one
/// big atlas texture, each light given a rectangle of it per frame — is the
/// ninety-percent picture's missing ten percent; the lesson's §12 names it.)
///
/// **The pipeline, the draw loop and the comparison sampler are 6.8's**: one
/// depth-only pipeline serves every map here and the cascades too, because a
/// pipeline is bound to a depth FORMAT, not to a texture type or a size. Which
/// is also why the cube faces' mirrored cameras cost nothing here — the
/// pipeline culls nothing (6.8 chose NONE for a ground plane's sake), so a
/// reversed winding changes no pixel of the map.
class gpu_local_shadows
{
public:
    gpu_local_shadows() = default;
    ~gpu_local_shadows();

    gpu_local_shadows(const gpu_local_shadows&) = delete;
    gpu_local_shadows& operator=(const gpu_local_shadows&) = delete;

    /// Build the two depth textures, the pipeline and the sampler.
    ///
    /// @param format the depth format for both maps (`D32_FLOAT` preferred, for
    ///        6.8's reason, and doubly here: a perspective map spends its
    ///        precision near the lamp and needs every bit it can get far away).
    /// @param max_spots  layers in the spot array. At least 1 is always made,
    ///        so the shader's binding is never empty.
    /// @param max_points cubes in the cube array; likewise at least 1.
    [[nodiscard]] bool create(const gpu_device& dev,
                              SDL_GPUShader* vertex, SDL_GPUShader* fragment,
                              SDL_GPUTextureFormat format,
                              const local_shadow_settings& settings = {},
                              int max_spots = 4, int max_points = 2);

    void destroy();

    [[nodiscard]] bool valid() const
    {
        return pipeline_.valid() && spots_.valid() && points_.valid();
    }

    /// Assign every shadow-casting light a slot, fit its cameras, list its
    /// passes, and pack one record per light into `out`.
    ///
    /// @return how many records were written: `lights.size()`, or `out.size()`
    ///         if that is smaller (the rest are simply not uploaded).
    ///
    /// **The records and the passes come from ONE call** so that the camera a
    /// pass renders with and the matrix the shader samples with cannot be two
    /// computations of one thing — the argument `fill_uniforms` made in 6.8,
    /// for ten floats; here it is sixteen more.
    int prepare(std::span<const local_light> lights, std::span<gpu_local_light> out);

    /// The passes the last `prepare` asked for: one per shadowed spot, six per
    /// shadowed point, in light order and SDL face order.
    [[nodiscard]] int job_count() const { return static_cast<int>(jobs_.size()); }
    [[nodiscard]] const local_shadow_job& job(int i) const
    {
        return jobs_[static_cast<std::size_t>(i)];
    }

    /// Shadowed lights past the slot budget in the last `prepare`, packed
    /// without a shadow. Non-zero is worth a log line and a larger budget.
    [[nodiscard]] int dropped() const { return dropped_; }

    /// Record every job, each in a pass of its own: begin, draw, end — 6.8's
    /// `gpu_shadow_map::render` shape, for a caller without a frame graph.
    void render(SDL_GPUCommandBuffer* cb, const gpu_draw_item* items, int count,
                frame_log* log = nullptr) const;

    /// Record job `job_index`'s draws into a pass somebody else began, with the
    /// job's `texture` and `layer` as its depth attachment — 6.17's
    /// `render_into` shape, so a frame graph can own the attachment and derive
    /// its ops.
    void render_into(SDL_GPUCommandBuffer* cb, SDL_GPURenderPass* pass, int job_index,
                     const gpu_draw_item* items, int count,
                     frame_log* log = nullptr) const;

    [[nodiscard]] SDL_GPUTexture* spot_texture() const { return spots_.handle(); }
    [[nodiscard]] SDL_GPUTexture* point_texture() const { return points_.handle(); }
    [[nodiscard]] SDL_GPUSampler* sampler() const { return compare_.handle(); }
    [[nodiscard]] SDL_GPUTextureFormat format() const { return spots_.format(); }
    [[nodiscard]] int max_spots() const { return max_spots_; }
    [[nodiscard]] int max_points() const { return max_points_; }

    [[nodiscard]] local_shadow_settings& settings() { return set_; }
    [[nodiscard]] const local_shadow_settings& settings() const { return set_; }

private:
    gpu_texture spots_;
    gpu_texture points_;
    gpu_sampler compare_;
    gpu_pipeline pipeline_;
    local_shadow_settings set_{};
    int max_spots_ = 0;
    int max_points_ = 0;
    int dropped_ = 0;

    /// Rebuilt by every `prepare`; `clear()` keeps the capacity, so a steady
    /// frame allocates nothing (the bargain `collect_triangles` made in 3.10).
    std::vector<local_shadow_job> jobs_;
};

} // namespace engine

#endif // ENGINE_GFX_GPU_SHADOW_HPP
