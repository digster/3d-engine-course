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

namespace engine {

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

} // namespace engine

#endif // ENGINE_GFX_GPU_SHADOW_HPP
