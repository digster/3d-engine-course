// engine/include/engine/gfx/gpu_post.hpp — the first pass that draws no geometry.
//
// Lesson 6.12. Everything this engine has rendered so far has been a pass over a
// SCENE: bind a mesh, push its matrices, draw its triangles. This is the other
// kind, and the difference is worth naming before the code:
//
//   A SCENE PASS asks "what is in front of the camera?" and its cost scales with
//              the geometry.
//   A POST PASS asks "what should this finished image look like?" and its cost
//              scales with the number of PIXELS — one fragment each, every frame,
//              whatever the scene contains.
//
// A tonemap is the smallest possible example and therefore the right one to build
// first: one texture in, one texture out, five uniforms, no depth, no blending,
// no vertex buffer. Lesson 6.13 turns it into a stack with a bloom in it; the
// architecture question is that lesson's, and this file deliberately does not
// pre-empt it — see the note on `gpu_tonemap_pass`.
//
// ---------------------------------------------------------------------------
// WHY THIS IS A SECOND PASS AND NOT MORE FRAGMENT SHADER
// ---------------------------------------------------------------------------
//
// The tempting shortcut is to tonemap at the end of `scene.frag.hlsl`, where the
// colour already is. It costs nothing extra and it is wrong for three reasons,
// each of which becomes a real limitation within two lessons:
//
//   * AN EXPOSURE DERIVED FROM THE FRAME cannot be known while the frame is
//     being drawn. Auto-exposure reads the finished image's log-average
//     luminance, which does not exist until the last triangle has landed.
//   * BLOOM COMES BEFORE THE CURVE (6.13) and operates on the whole image.
//     A per-fragment tonemap has already compressed the values bloom needs.
//   * BLENDING WOULD COMPOSITE TONEMAPPED VALUES. `over` is linear in light, and
//     a curve is not linear, so `f(a) over f(b)` is not `f(a over b)`. Lesson
//     6.11's operator would quietly stop meaning what it means.
//
// So the scene renders into a float target, and this maps that target down.

#pragma once

#include <engine/gfx/gpu_device.hpp>
#include <engine/gfx/gpu_pipeline.hpp>
#include <engine/gfx/gpu_texture.hpp>
#include <engine/gfx/gpu_uniform.hpp>
#include <engine/gfx/bloom.hpp>
#include <engine/gfx/hdr.hpp>

#include <SDL3/SDL.h>

namespace engine {

/// The colour format this course renders HDR into, and why it is this one.
///
/// **16-bit float per channel**, which is the format essentially every engine
/// uses for an HDR scene buffer, for a reason that is worth checking rather than
/// inheriting:
///
///   * It holds the range. A half float reaches 65,504, and the measured peak of
///     a polished metal at the mirror angle in this engine's own test scene is
///     59,003 — which is inside it, but only just, and that is a useful thing to
///     know about your own content rather than a comfort.
///   * It holds the PRECISION where it matters. A half has 10 mantissa bits, so
///     about 3 decimal digits, and because it is floating point that precision is
///     RELATIVE — the dark end, where the eye is most sensitive and where an
///     8-bit sRGB encode spends most of its codes, gets the same relative
///     accuracy as the bright end. This is the answer to "why not 16-bit fixed
///     point": a fixed-point format would spread its steps evenly over a range
///     that is not perceived evenly.
///   * It costs 2x, not 3x. Eight bytes a pixel against four — where the CPU's
///     `hdr_buffer` pays three floats, twelve bytes, 3x. The GPU's cheaper
///     multiplier is entirely the half float.
///
/// `R11G11B10_FLOAT` is the other common choice — 4 bytes, same as LDR, no alpha
/// and no sign bit. It is a real option and this course does not take it, because
/// its 5-bit blue mantissa is visible as banding in smooth gradients and
/// diagnosing that costs more than the bandwidth saves at this scale.
inline constexpr SDL_GPUTextureFormat k_hdr_format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;

/// Owns the pipeline, sampler and uniform packing for one full-screen resolve.
///
/// **Deliberately not a "post-processing stack".** It is one pass with one
/// input, and Lesson 6.13 is where the question "how do several of these compose,
/// and who owns the intermediate targets?" gets asked properly. Building the
/// general machinery now, with exactly one user, would be inventing an
/// architecture to fit a single case — which is the mistake Lesson 6.5 spent a
/// whole section arguing against when it declined to give `material` fields that
/// nothing read.
class gpu_tonemap_pass
{
public:
    gpu_tonemap_pass() = default;
    ~gpu_tonemap_pass();

    gpu_tonemap_pass(const gpu_tonemap_pass&) = delete;
    gpu_tonemap_pass& operator=(const gpu_tonemap_pass&) = delete;

    /// Build the pipeline and the sampler.
    ///
    /// @param target_format the format this pass WRITES — normally the device's
    ///        swapchain format. Baked into the pipeline at creation (Lesson 4.4),
    ///        which is why it is a parameter and not a per-frame argument.
    ///
    /// **No depth state, no blending, no vertex input.** All three absences are
    /// deliberate and each one is a sentence: a full-screen triangle has nothing
    /// to be occluded by, nothing to composite with, and no vertices to fetch.
    [[nodiscard]] bool create(const gpu_device& dev,
                              SDL_GPUShader* vertex, SDL_GPUShader* fragment,
                              SDL_GPUTextureFormat target_format);

    void destroy();

    [[nodiscard]] bool valid() const { return pipeline_.valid(); }

    /// Record the resolve into `pass`.
    ///
    /// @param hdr the float colour target the scene was rendered into. It must
    ///        have been created with `SAMPLER` usage as well as `COLOR_TARGET`,
    ///        or this reads a texture the driver never made sampleable — see
    ///        `create_hdr_target`.
    /// @param settings exposure, curve and white point; packed by `uniforms_of`.
    /// @param shader_encodes true when the pass's target is a plain UNORM format
    ///        and the transfer function is therefore this shader's job.
    /// @param bloom half-resolution bloom to composite BEFORE the curve — Lesson
    ///        6.13. **Must not be null**, even with `bloom_intensity` at zero:
    ///        SDL_GPU has no way to unbind a sampler, and a pipeline whose shader
    ///        declares `t1` must have something bound there. `gpu_post_stack`
    ///        keeps a 1x1 black texture for exactly this, and the reason it is a
    ///        texture rather than a branch is that a branch would cost a
    ///        divergent fetch on every fragment to save a multiply on some.
    /// @param bloom_intensity see `bloom_settings::intensity`.
    ///
    /// **Three vertices, one instance, no buffers bound.** That call —
    /// `SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0)` — is the whole draw, and the
    /// triangle's corners are computed from `SV_VertexID` inside the vertex
    /// shader. See `fullscreen.vert.hlsl` for the derivation, including why it is
    /// one triangle rather than two.
    void render(SDL_GPUCommandBuffer* cb, SDL_GPURenderPass* pass,
                SDL_GPUTexture* hdr, const tonemap_settings& settings,
                bool shader_encodes,
                SDL_GPUTexture* bloom = nullptr, float bloom_intensity = 0.0f) const;

    [[nodiscard]] double create_ms() const { return pipeline_.create_ms(); }

private:
    gpu_pipeline pipeline_;
    gpu_sampler sampler_;      ///< NEAREST — the resolve is 1:1
    gpu_sampler bloom_sampler_; ///< LINEAR — the bloom is half resolution
};

// ---------------------------------------------------------------------------
// Lesson 6.13 — the bloom
// ---------------------------------------------------------------------------

/// The pyramid, its three pipelines, and the eleven render passes that fill it.
///
/// **This is the type that owns the bloom's intermediate targets**, and the
/// narrowness of that ownership is the design, not a limitation. Nothing outside
/// this class needs level 3; nothing outside it knows how many levels there are;
/// every intermediate's lifetime begins and ends inside one `render` call. A pass
/// that knows its own intermediates should own them, and a general resource pool
/// would be solving a problem this pass does not have. The general version — for
/// intermediates whose lifetimes CROSS passes — is Lesson 6.17's frame graph.
///
/// ---------------------------------------------------------------------------
/// ELEVEN RENDER PASSES, AND WHY IT CANNOT BE FEWER
/// ---------------------------------------------------------------------------
///
/// A render pass writes ONE set of colour targets. The pyramid has six levels
/// written at six different sizes, so: one bright pass, five downsamples, five
/// upsamples — `2n - 1` passes for `n` levels. There is no way to write two
/// differently-sized targets from one pass, and there is no way to read a texture
/// in the same pass that writes it.
///
/// That second constraint is the more interesting one, because it is what forces
/// the ping-pong structure every post-processing chain has. A fragment shader
/// sampling a texel OTHER than its own has no defined ordering against the writes
/// happening around it — it is a read-after-write hazard with no synchronisation
/// available inside a pass — so SDL_GPU (like every API under it) forbids binding
/// a texture as both input and output. The bloom escapes needing a ping-pong pair
/// only because every stage reads one level and writes a DIFFERENT one.
///
/// The cost is real and worth carrying forward: at eleven passes the bloom issues
/// more render passes than the entire rest of this engine's frame. Each one is a
/// begin/end pair, a pipeline bind, a sampler bind and a uniform push, for a draw
/// of three vertices. `verify_613` §F counts them.
class gpu_bloom
{
public:
    gpu_bloom() = default;
    ~gpu_bloom();

    gpu_bloom(const gpu_bloom&) = delete;
    gpu_bloom& operator=(const gpu_bloom&) = delete;

    /// Build the three pipelines and the sampler.
    ///
    /// **All three pipelines share `fullscreen.vert`**, which is the clearest
    /// evidence that 6.12 drew that abstraction in the right place: every post
    /// pass wants the same three corners, and only the question asked of each
    /// pixel differs. It is also why this class takes four shaders and not six.
    [[nodiscard]] bool create(const gpu_device& dev,
                              SDL_GPUShader* fullscreen_vertex,
                              SDL_GPUShader* bright_fragment,
                              SDL_GPUShader* down_fragment,
                              SDL_GPUShader* up_fragment);

    void destroy();

    [[nodiscard]] bool valid() const { return bright_.valid() && down_.valid() && up_.valid(); }

    /// Allocate the pyramid for a `full_width x full_height` scene target.
    ///
    /// Idempotent: a call with the size and level count it already has does
    /// nothing, so a caller may invoke it every frame and pay only on a resize.
    /// That matters more on the GPU than on the CPU — a texture allocation is a
    /// driver call and a possible stall, not a `malloc`.
    [[nodiscard]] bool resize(const gpu_device& dev, Uint32 full_width, Uint32 full_height,
                              int levels);

    /// Record the whole chain. **Begins and ends its own render passes**, so it
    /// must be called between command-buffer acquisition and the scene's pass,
    /// never inside one.
    ///
    /// @param scene the float colour target the scene was rendered into, with
    ///        both `COLOR_TARGET` and `SAMPLER` usage.
    /// @param exposure the SAME number the resolve will apply to the scene. If
    ///        the two disagree, the glow and the image it sits on were
    ///        photographed at different shutter speeds.
    void render(SDL_GPUCommandBuffer* cb, SDL_GPUTexture* scene,
                const bloom_settings& s, float exposure) const;

    /// The finished bloom — pyramid level 0, half the scene's dimensions.
    [[nodiscard]] SDL_GPUTexture* result() const;

    [[nodiscard]] int levels() const { return level_count_; }

    /// Total texels across every level, for the memory claim in §5.
    [[nodiscard]] std::size_t texels() const;

    /// How many render passes `render` will record: `2n - 1`.
    [[nodiscard]] int pass_count() const { return (level_count_ > 0) ? 2 * level_count_ - 1 : 0; }

private:
    gpu_pipeline bright_;
    gpu_pipeline down_;
    gpu_pipeline up_;      ///< the additive one
    gpu_sampler sampler_;  ///< LINEAR + clamp: the filter mode is doing arithmetic
    gpu_texture levels_[k_max_bloom_levels];
    int level_count_ = 0;
};

/// The HDR target, the bloom and the resolve, owned together and run in order.
///
/// **This is the answer to the second question `gpu_post.hpp` deferred** — who
/// owns what, and who knows the order — and it is deliberately the SMALL answer.
///
/// What it owns is exactly what crosses BETWEEN stages: the float scene target
/// that the scene pass writes and both later stages read, and the 1x1 black
/// texture that stands in for a bloom when there is none. What it does not own is
/// anything internal to a stage — the pyramid belongs to `gpu_bloom`, because
/// `gpu_bloom` is the only thing that can know its lifetimes.
///
/// **And the ordering is not a policy, it is physics.** The bloom runs before the
/// resolve because it operates on the PRE-curve image: it is looking for exactly
/// the values above 1 that Lesson 6.12 finally lets the engine keep, and after
/// the curve those values are all in the same place. Run a bloom on tonemapped
/// pixels and the bright pass has nothing left to select — a value of 6.38 and a
/// value of 55,917 have both become 1.0, so the threshold either takes everything
/// bright or nothing at all.
///
/// ---------------------------------------------------------------------------
/// WHAT THIS TYPE CANNOT DO, STATED PLAINLY
/// ---------------------------------------------------------------------------
///
/// You cannot insert a stage without editing it. There is no list, no registry,
/// no `add_stage`, and no way for a stage to declare what it reads and writes so
/// that something else can work out the lifetimes. With two stages that is the
/// right shape — a registry with two entries is an architecture pretending to be
/// a feature, which is the mistake Lesson 6.5 spent a section arguing against and
/// 6.12 declined to make with one stage.
///
/// It stops being the right shape somewhere around four or five stages, when the
/// intermediates start outliving the stage that produced them and two stages want
/// targets of the same size at different times. That is the frame graph, and it
/// is Lesson 6.17.
class gpu_post_stack
{
public:
    gpu_post_stack() = default;
    ~gpu_post_stack();

    gpu_post_stack(const gpu_post_stack&) = delete;
    gpu_post_stack& operator=(const gpu_post_stack&) = delete;

    /// @param target_format what the resolve WRITES — normally the swapchain's.
    [[nodiscard]] bool create(const gpu_device& dev,
                              SDL_GPUShader* fullscreen_vertex,
                              SDL_GPUShader* tonemap_fragment,
                              SDL_GPUShader* bright_fragment,
                              SDL_GPUShader* down_fragment,
                              SDL_GPUShader* up_fragment,
                              SDL_GPUTextureFormat target_format);

    void destroy();

    [[nodiscard]] bool valid() const { return tonemap_.valid() && bloom_.valid(); }

    /// Size the scene target and the pyramid. Idempotent; call it every frame.
    [[nodiscard]] bool resize(const gpu_device& dev, Uint32 width, Uint32 height,
                              const bloom_settings& s);

    /// The float target the SCENE PASS should render into.
    [[nodiscard]] SDL_GPUTexture* scene_target() const { return scene_.handle(); }

    /// Record bloom + resolve. `pass` must already be begun against the display.
    ///
    /// The split is deliberate: the bloom records its own render passes and the
    /// resolve records a draw into somebody else's, because the resolve's target
    /// is the swapchain and the swapchain's pass belongs to the frame, not to
    /// this type. So `render_bloom` is called BEFORE the display pass is begun
    /// and `resolve_into` inside it.
    void render_bloom(SDL_GPUCommandBuffer* cb, const bloom_settings& s, float exposure) const;

    void resolve_into(SDL_GPUCommandBuffer* cb, SDL_GPURenderPass* pass,
                      const tonemap_settings& tone, bool shader_encodes,
                      const bloom_settings& s) const;

    [[nodiscard]] const gpu_bloom& bloom() const { return bloom_; }

private:
    gpu_texture scene_;      ///< the float target every stage shares
    gpu_texture no_bloom_;   ///< 1x1 black, for when the bloom is off
    gpu_tonemap_pass tonemap_;
    gpu_bloom bloom_;
};

} // namespace engine
