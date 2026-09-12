// engine/include/engine/gfx/gpu_scene.hpp — drawing a SCENE, rather than a thing.
//
// Lesson 4.8. Everything Module 4 has built so far draws one piece of geometry:
// a triangle (4.4), a torus (4.5), the same torus seven times (4.5's instancing),
// the same torus again with a camera (4.6) and a texture (4.7). Module 3's scene
// is not that. It is up to four objects with DIFFERENT MESHES, DIFFERENT
// TRANSFORMS and DIFFERENT MATERIALS, and one of them is a ground plane that must
// not be back-face culled while the others must be.
//
// ---------------------------------------------------------------------------
// THE THING THE SOFTWARE RASTERIZER LET US GET AWAY WITH
// ---------------------------------------------------------------------------
//
// `collect_triangles` walks every object, transforms every vertex, and appends
// the results to ONE array. `draw_triangles` then fills that array in a single
// loop, re-binding the material per triangle from `raster_triangle::surface` —
// and Lesson 3.8 flagged that field, in writing, as a cheat a GPU cannot make:
//
//     "material parameters are pipeline state, so a real renderer BATCHES BY
//      MATERIAL and a scene with three materials is three draws."
//
// This file is where that bill arrives. There is no way to change a uniform in
// the middle of a draw call, and no way at all to change the cull mode or the
// fill mode — those are baked into a pipeline object, which was Lesson 4.1's
// entire argument for pipelines existing. So the scene becomes:
//
//     for each object:  push its matrices, push its material, bind its texture,
//                       bind the pipeline its SURFACE STYLE needs, draw.
//
// Four objects is four draws. That is not a regression and it is not overhead to
// be optimised away later — it is the shape a GPU renderer has, and the reason
// every engine you will read has a "sort the draw list" step. `draw_stats`
// counts what the sort saves, in this scene, on this frame.
//
// ---------------------------------------------------------------------------
// WHAT THIS FILE IS NOT
// ---------------------------------------------------------------------------
//
// It is not a render graph, a material system, or a scene graph. It owns three
// pipelines, a depth target and a 1x1 white texture, and it turns an array of
// `gpu_draw_item` into command-buffer calls. Every larger idea it gestures at —
// materials that own their pipeline state (Module 6), a sorted and bucketed draw
// list, a frame graph — is named where the pressure for it shows up and built
// where the course reaches it. The point of writing the small version first is
// that the big one then arrives as an answer rather than as a framework.

#pragma once

#include <engine/gfx/gpu_debug.hpp>
#include <engine/gfx/gpu_device.hpp>
#include <engine/gfx/gpu_mesh.hpp>
#include <engine/gfx/gpu_pipeline.hpp>
#include <engine/gfx/gpu_shader.hpp>
#include <engine/gfx/gpu_texture.hpp>
#include <engine/gfx/gpu_uniform.hpp>
#include <engine/math/mat3.hpp>
#include <engine/math/mat4.hpp>

#include <SDL3/SDL.h>

namespace engine {

/// **Forward-declared, and that is a physical-design decision** — Lesson 6.16.
///
/// `instancing.hpp` includes THIS header, because a batch key is made of
/// `gpu_draw_item`'s fields. So this header cannot include that one: the two
/// would form a cycle, and `#pragma once` would resolve it by silently giving
/// whichever file was reached second a half-defined view of the other.
///
/// A forward declaration breaks it exactly, and costs nothing because
/// `render_batched` below takes a POINTER AND A COUNT rather than a
/// `std::span` — `std::span<const T>` needs `T` complete, and `render` has taken
/// a pointer and a count since Lesson 4.8 anyway, so the constraint lands on a
/// spelling the class had already chosen for other reasons.
struct instance_batch;

/// Which pipeline an object needs — the part of a material that **cannot be a
/// number in a buffer**.
///
/// Lesson 3.4 discovered that back-face culling is only valid on a closed
/// surface, put a `closed` flag on the demo's object struct, and noted that in a
/// real engine the flag belongs on the material because cull mode is pipeline
/// state. That sentence is now enforced by the hardware: these three values are
/// three separate `SDL_GPUGraphicsPipeline` objects, created at startup, and
/// switching between them mid-frame costs a bind.
///
/// Three, and not more, on purpose. Every additional axis of pipeline state
/// multiplies the number of objects to create — two cull modes times two fill
/// modes times two depth settings is eight — which is why real engines either
/// enumerate a small fixed set like this one or build them lazily and cache. The
/// combinatorial growth is the honest reason "just make it a parameter" is not
/// available.
enum class surface_style
{
    /// A closed solid: cull back faces. Lesson 3.4's optimisation, now free.
    solid,

    /// A sheet with two visible sides — the ground plane, a quad, a plank. Cull
    /// nothing, because "the back" of a sheet is a face somebody can see.
    two_sided,

    /// `SDL_GPU_FILLMODE_LINE`, cull nothing. Module 2's wireframe milestone,
    /// which cost Lessons 2.1 and 2.12 to build, as one enum value. Kept because
    /// it is still the fastest way to see geometry that is drawing wrongly, and
    /// because the contrast is worth feeling.
    wireframe
};

[[nodiscard]] const char* name_of(surface_style s);

/// The **second** state axis, and the first one this course has had to add —
/// Lesson 6.11.
///
/// `surface_style` is one axis with three values, so the renderer has held three
/// pipelines since Lesson 4.8. Transparency is not a fourth value of that axis:
/// a blended surface is still solid, two-sided or wireframe, and every
/// combination is legitimate. So it is a second axis, and the pipeline count
/// becomes a **product** rather than a sum — 3 x 3 = 9.
///
/// **That multiplication is the real cost of transparency at the API level**, and
/// it is worth feeling now, at nine, because it is how every real engine ends up
/// with a pipeline cache keyed on a hash of the whole state. Add MSAA (6.14) and
/// it is 18. Add a depth-only variant and 27. Nobody enumerates that; they build
/// the state, hash it, and create on miss. We enumerate, because nine is
/// countable and because `create_ms()` then measures what the ninth costs.
///
/// **Note what is NOT on this axis: `alpha_mode::mask`.** Masking is a uniform
/// (`material_uniforms::alpha_cutoff`) and a `clip` in the shader, so a masked
/// draw uses the `opaque` blend style and adds no pipeline at all. `blend.hpp`
/// argues the general case; this enum is where the argument becomes a number.
enum class blend_style
{
    /// No blending, depth written. Every draw before this lesson.
    opaque,

    /// `src*a + dst*(1-a)`, depth tested and NOT written.
    alpha,

    /// `src + dst*(1-a)`, depth tested and NOT written. For sources whose colour
    /// is already scaled by their coverage — see `alpha_storage`.
    premultiplied
};

[[nodiscard]] const char* name_of(blend_style b);

/// One object, ready to draw.
///
/// **Note what is a pointer and what is a value.** The mesh is borrowed — it
/// lives on the device and is owned by whoever uploaded it — while the matrices
/// and the material are copied, because they are small and they change every
/// frame. That split is the seed of Module 5's handle system: the day this struct
/// holds a `mesh_handle` instead of a `const gpu_mesh*` is the day the renderer
/// stops being able to be handed a dangling pointer.
struct gpu_draw_item
{
    const gpu_mesh* mesh = nullptr;         ///< borrowed; must outlive the draw

    /// T*R*S, from `engine::parent_from_local` — Lesson 2.8.
    mat4 world_from_model = mat4::identity();

    /// The INVERSE TRANSPOSE of the above, from `engine::normal_matrix` — Lesson
    /// 3.6. Stored as a `mat3` here and unpacked into three padded `vec4` at push
    /// time, for the reason `object_uniforms` documents.
    mat3 normal_from_model = mat3::identity();

    /// The albedo, the highlight and the exponent. `material_uniforms::albedo` is
    /// LINEAR — decode the demo's `Uint32` tint before it gets here.
    material_uniforms material{};

    /// The albedo image, or `nullptr` for "this surface has no texture", in which
    /// case the renderer binds its own 1x1 white texture and the material's
    /// `textured` flag decides what the shader does with it.
    ///
    /// Bound even when unused, because a draw with nothing at a sampler slot the
    /// shader declares draws NOTHING — silently. That failure has now cost this
    /// project two debugging sessions (Lessons 4.6 and 4.7 each hit it once), and
    /// the fix both times was the same: hand the shader the identity element of
    /// the feature it is missing.
    SDL_GPUTexture* texture = nullptr;

    /// The normal map, or `nullptr` for "this surface is as flat as its
    /// triangles" — Lesson 6.7, and the renderer then binds its own 1x1 FLAT
    /// NORMAL for the same reason it binds a white texel for the albedo: a
    /// declared sampler slot with nothing bound draws nothing at all.
    ///
    /// Note that the two fallbacks are different constants and both are the
    /// identity element of their own operation. White is the identity for a
    /// multiply; (128, 128, 255) — the lavender — is the identity for a basis
    /// change, because it decodes to (0, 0, 1) and `T*0 + B*0 + N*1` is `N`.
    SDL_GPUTexture* normal_map = nullptr;

    surface_style style = surface_style::solid;

    /// Which blend state this draw needs — Lesson 6.11.
    ///
    /// A **second** enum beside `style` rather than more values of it, because
    /// they are independent: see `blend_style`. Defaults to `opaque`, so every
    /// draw item built before this lesson selects pipeline `[style][0]`, which is
    /// the same pipeline object it selected before.
    ///
    /// **The renderer does not derive this from the material**, and that is
    /// deliberate. It could — `material::needs_sorting()` is right there — but the
    /// caller is the one who knows whether this draw is in the sorted pass, and a
    /// renderer that silently switched a draw to a depth-write-disabled pipeline
    /// because of a uniform would be making an ordering decision on the caller's
    /// behalf without being able to do the sort that has to go with it.
    blend_style blend = blend_style::opaque;
};

/// What one `render()` call actually did. The HUD's numbers, and §4's evidence.
struct draw_stats
{
    int items = 0;              ///< how many were submitted
    int draws = 0;              ///< how many `SDL_DrawGPUIndexedPrimitives` were issued
    int pipeline_binds = 0;     ///< how many of those needed a different pipeline
    int texture_binds = 0;      ///< …and a different texture
    Uint32 triangles = 0;       ///< the total, before any culling the GPU does
    Uint32 uniform_bytes = 0;   ///< pushed per frame: the per-draw blocks add up

    /// Pipeline binds that a perfectly sorted list would have needed: one per
    /// distinct style present. The gap between this and `pipeline_binds` is what
    /// sorting the draw list is worth, in this frame, measured rather than
    /// asserted.
    int ideal_pipeline_binds = 0;
};

/// Owns the pipelines, the depth target and the fallback texture that turn a list
/// of `gpu_draw_item` into a picture.
///
/// **Movable, not copyable**, like every resource type in this engine.
class gpu_scene_renderer
{
public:
    gpu_scene_renderer() = default;
    ~gpu_scene_renderer();

    gpu_scene_renderer(const gpu_scene_renderer&) = delete;
    gpu_scene_renderer& operator=(const gpu_scene_renderer&) = delete;

    /// Build the **nine** pipelines (Lesson 6.11; three until 6.10) and the 1x1
    /// white texture.
    ///
    /// @param depth_format what `supported_depth_format` returned, or
    ///        `SDL_GPU_TEXTUREFORMAT_INVALID` for a device with no usable depth
    ///        format — in which case the pipelines are built without a depth
    ///        attachment and the scene draws with Lesson 3.1's problem back.
    /// @param colour_format `SDL_GPU_TEXTUREFORMAT_INVALID` means "the device's
    ///        swapchain format", which is right for anything drawing into the
    ///        window. `verify_48` renders offscreen and passes its own.
    ///
    /// The upload command buffer is acquired and submitted internally, because
    /// the only thing to upload is four bytes and making a caller thread a
    /// command buffer through for that would be ceremony.
    /// @param instanced_vertex **Lesson 6.16.** `scene_instanced.vert`, or
    ///        `nullptr` for "this renderer does no instanced draws". When given,
    ///        ONE more pipeline is created — solid, opaque — bringing the count
    ///        to ten.
    ///
    ///        **One and not nine**, and the asymmetry is the honest part. The
    ///        nine exist because `surface_style` and `blend_style` are
    ///        independent axes that every draw must choose from; instancing is
    ///        neither, it is a THIRD axis, and crossing it in would make
    ///        eighteen. We build the one combination that is worth instancing —
    ///        many copies of a closed opaque mesh, which is what foliage, crates
    ///        and crowds are — and say plainly that a wireframe instanced draw
    ///        or a blended instanced draw is unavailable. `batch_instances`
    ///        refuses to batch blended geometry anyway, for ordering reasons
    ///        that are correctness rather than convenience, so that half of the
    ///        gap is a rule and not a shortcut.
    ///
    ///        The general answer — build the state, hash it, create on miss — is
    ///        named in `blend_style`'s comment and is Module 9's pipeline cache.
    [[nodiscard]] bool create(const gpu_device& dev,
                              SDL_GPUShader* vertex, SDL_GPUShader* fragment,
                              SDL_GPUTextureFormat depth_format,
                              SDL_GPUTextureFormat colour_format
                                  = SDL_GPU_TEXTUREFORMAT_INVALID,
                              SDL_GPUSampleCount samples = SDL_GPU_SAMPLECOUNT_1,
                              SDL_GPUShader* instanced_vertex = nullptr);

    void destroy();

    [[nodiscard]] bool valid() const { return pipelines_[0][0].valid(); }

    /// Create or re-create the depth attachment at `w` x `h`.
    ///
    /// A depth target must match its colour target's dimensions exactly, and the
    /// window is resizable, so this is called every frame with the swapchain's
    /// size and does nothing on the frames where the size has not changed.
    /// Forgetting it is a crash on the first resize, not a wrong picture.
    ///
    /// @return false when there is no usable depth format, which is not an error
    ///         — the caller then begins its pass without a depth attachment.
    /// @param samples **Lesson 6.14.** Must match the colour target's count —
    ///        every attachment in a pass shares its sample positions, so a 4x
    ///        colour target beside a 1x depth target is a pass that cannot begin.
    [[nodiscard]] bool ensure_depth(const gpu_device& dev, Uint32 w, Uint32 h,
                                    SDL_GPUSampleCount samples = SDL_GPU_SAMPLECOUNT_1);

    /// Fill in the depth attachment description for a pass. Only valid after a
    /// successful `ensure_depth`.
    ///
    /// **Cleared to 1, the far plane**, because SDL_GPU's NDC runs 0 at near to 1
    /// at far (conventions §4) and the comparison is LESS. Clear it to 0 and
    /// every fragment in the scene fails its test.
    [[nodiscard]] SDL_GPUDepthStencilTargetInfo depth_target_info() const;

    [[nodiscard]] bool has_depth() const { return depth_.valid(); }
    [[nodiscard]] SDL_GPUTextureFormat depth_format() const { return depth_format_; }

    /// Record the whole scene into `pass`.
    ///
    /// The per-frame blocks are pushed onto `cb` first and then never again; the
    /// per-draw blocks are pushed between draws. Both go on the COMMAND BUFFER
    /// rather than the pass, which is why `cb` is a separate parameter and not
    /// something the pass could hand us.
    ///
    /// Items are drawn **in the order given**. This function does not sort them,
    /// and the reason is that sorting is a policy decision with more than one
    /// right answer (by pipeline? by material? front-to-back for early-z? by
    /// distance for transparency?) and it belongs to the caller who knows what
    /// the frame is for. What this function does is COUNT what the caller's order
    /// cost, so the policy can be evaluated rather than argued about.
    ///
    /// A null or invalid mesh is skipped rather than being an error: a scene
    /// whose model failed to load should draw the rest of itself.
    /// @param log  Lesson 4.9's frame log, or `nullptr`. When present, every
    ///        bind, push and draw below is recorded as it is issued — from the
    ///        SAME statement that issues it, never from a parallel description
    ///        of what the function is believed to do. Instrumentation that can
    ///        drift from the code it describes is worse than none, because it
    ///        is believed. `verify_49` §D checks the log against `draw_stats`,
    ///        which is a second reading of the same events.
    /// The environment's three resources, **gathered into a struct because the
    /// alternative had stopped being readable**. Lesson 6.15.
    ///
    /// `render` already takes nine parameters, four of them optional pointers,
    /// and `hdr.hpp`'s `resolve` said the same thing one lesson earlier: "two
    /// inputs is the point at which this signature stops scaling... a third
    /// would need a struct". Three more would have made twelve. They also
    /// genuinely belong together — an irradiance map baked from one sky and a
    /// prefiltered chain baked from another produce a diffuse and a specular
    /// term that disagree about where the sun is, which is a failure the type
    /// system can help with by making them arrive as one thing.
    ///
    /// **All three are required together.** A null `scene_environment` (or a
    /// null pointer to one) binds the renderer's own identity fallbacks — a 1x1
    /// BLACK cube and a 1x1 black table — which is the fourth instance of this
    /// class's stated pattern: hand the shader the identity element of the
    /// feature it is missing. Black is the identity for the ADDITION the
    /// ambient term performs, where white was the identity for the albedo's
    /// multiply and lavender for a basis change. Note that the fallback is
    /// bound whether or not `ibl_intensity` is zero: HLSL does not elide a
    /// resource because a branch did not reach it, and an unbound sampler slot
    /// is undefined rather than empty.
    struct scene_environment
    {
        SDL_GPUTexture* irradiance = nullptr;   ///< TextureCube, slot 3
        SDL_GPUTexture* prefiltered = nullptr;  ///< TextureCube, slot 4
        SDL_GPUTexture* brdf_lut = nullptr;     ///< Texture2D, slot 5

        /// The sampler for both cube maps. **CLAMP_TO_EDGE and trilinear**, and
        /// the mip filter is not optional: the prefiltered chain is indexed by a
        /// continuous roughness, so `nearest` between levels would make a
        /// smoothly-varying roughness step visibly between blur radii.
        SDL_GPUSampler* cube_sampler = nullptr;

        /// The sampler for the table. **CLAMP_TO_EDGE, linear, one level.**
        /// Repeat here would wrap a grazing `n.v` round to the head-on entry.
        SDL_GPUSampler* lut_sampler = nullptr;

        [[nodiscard]] bool complete() const
        {
            return irradiance != nullptr && prefiltered != nullptr
                && brdf_lut != nullptr && cube_sampler != nullptr
                && lut_sampler != nullptr;
        }
    };

    /// @param shadow the shadow map to bind at fragment slot 2, or `nullptr` for
    ///        "this scene has none" — in which case the renderer binds its own
    ///        **1x1 depth texture cleared to the far plane**, Lesson 6.8.
    ///
    ///        That fallback is the third of its kind in this class and the
    ///        pattern is now explicit: hand the shader the IDENTITY ELEMENT of
    ///        the feature it is missing. White is the identity for a multiply,
    ///        lavender for a basis change (6.7), and **`1.0` is the identity for
    ///        a depth comparison** — a map that says the nearest surface along
    ///        every ray is the far plane says nothing occludes anything. A
    ///        declared sampler slot with nothing bound draws nothing at all, and
    ///        silently, which has now cost this project three debugging sessions.
    /// @param shadow_sampler the COMPARISON sampler that goes with it, or
    ///        `nullptr` to use the renderer's own. It is a separate parameter
    ///        from `sampler` because it is a different kind of object —
    ///        `enable_compare` is on — and binding an ordinary sampler here is a
    ///        validation error rather than a wrong picture.
    draw_stats render(SDL_GPUCommandBuffer* cb, SDL_GPURenderPass* pass,
                      const gpu_draw_item* items, int count,
                      const camera_uniforms& camera,
                      const scene_light_uniforms& light,
                      SDL_GPUSampler* sampler,
                      frame_log* log = nullptr,
                      SDL_GPUTexture* shadow = nullptr,
                      SDL_GPUSampler* shadow_sampler = nullptr,
                      const cascade_uniforms* cascades = nullptr,
                      const scene_environment* environment = nullptr) const;

    /// Draw a frame that has already been culled and batched — Lesson 6.16.
    ///
    /// Records the same per-frame state `render` does (camera, light, cascades,
    /// shadow map, environment — one shared helper, so the two cannot drift) and
    /// then issues **one draw per batch** instead of one per object.
    ///
    /// `instances` is a vertex buffer holding `batches`' instance data end to
    /// end, in the order `batch_instances` produced — typically a
    /// `gpu_stream_buffer` written earlier in the same command buffer. It is
    /// bound at **slot 1**, with a per-batch OFFSET, which is the mechanism that
    /// lets one buffer serve every batch: `SDL_BindGPUVertexBuffers` takes an
    /// offset in bytes, so batch `k` binds at `first * sizeof(gpu_instance)` and
    /// the hardware's instance index then counts from zero within it.
    ///
    /// @return the same `draw_stats` shape as `render`, so the two are directly
    ///         comparable — which is the entire point of §10's measurement. Note
    ///         that `items` counts INSTANCES and `draws` counts BATCHES, so the
    ///         ratio between them is what instancing bought.
    ///
    /// A batch whose mesh is null or whose style is not solid+opaque is SKIPPED
    /// and counted in `draw_stats::items` but not `draws` — the renderer has one
    /// instanced pipeline (see `create`), and silently drawing such a batch with
    /// the wrong state would be worse than not drawing it.
    draw_stats render_batched(SDL_GPUCommandBuffer* cb, SDL_GPURenderPass* pass,
                              const instance_batch* batches, int count,
                              SDL_GPUBuffer* instances,
                              const camera_uniforms& camera,
                              const scene_light_uniforms& light,
                              SDL_GPUSampler* sampler,
                              frame_log* log = nullptr,
                              SDL_GPUTexture* shadow = nullptr,
                              SDL_GPUSampler* shadow_sampler = nullptr,
                              const cascade_uniforms* cascades = nullptr,
                              const scene_environment* environment = nullptr) const;

    /// Was an instanced pipeline created? False when `create` was given no
    /// `instanced_vertex`, in which case `render_batched` draws nothing.
    [[nodiscard]] bool has_instanced() const { return instanced_.valid(); }

    /// The white 1x1 texture, for callers that want to bind it themselves.
    [[nodiscard]] SDL_GPUTexture* white() const { return white_.handle(); }

    /// The flat 1x1 normal map, for callers that want to bind it themselves.
    [[nodiscard]] SDL_GPUTexture* flat_normal() const { return flat_normal_.handle(); }

    /// How long `create` spent inside `SDL_CreateGPUGraphicsPipeline`, summed
    /// over the three. Lesson 4.4 measured that this number is dominated by the
    /// driver's on-disk pipeline cache and varies by two orders of magnitude
    /// between a cold and a warm run; it is reported for that reason, not as a
    /// benchmark.
    [[nodiscard]] double create_ms() const { return create_ms_; }

private:
    /// What is resolved once per frame and read by every draw — Lesson 6.16.
    ///
    /// **Split out of `render` when `render_batched` arrived.** Until this lesson
    /// there was one draw loop, and the ninety lines that resolve the fallbacks
    /// and push the per-frame blocks were simply its prologue. Instanced
    /// submission adds a second loop that needs every one of those lines and none
    /// of the first loop's. Copying them would have produced two copies of one
    /// rule — the failure this engine has already paid for twice (Lesson 5.1's
    /// four hand-transcribed harnesses; 6.15's two spellings of an ARGB packing).
    ///
    /// The split is exactly at "does this change within a frame?", which is the
    /// same line Lesson 4.6 drew for uniform blocks: **data grouped by RATE of
    /// change**, not by what it describes.
    struct frame_setup
    {
        SDL_GPUTexture* shadow = nullptr;
        SDL_GPUSampler* shadow_sampler = nullptr;
        SDL_GPUTexture* irradiance = nullptr;
        SDL_GPUTexture* prefiltered = nullptr;
        SDL_GPUTexture* brdf_lut = nullptr;
        SDL_GPUSampler* cube_sampler = nullptr;
        SDL_GPUSampler* lut_sampler = nullptr;
    };

    /// Push the per-frame uniform blocks and resolve every fallback.
    ///
    /// `stats.uniform_bytes` is accumulated through the reference parameter,
    /// because the bytes are pushed here and the caller is the one reporting.
    [[nodiscard]] frame_setup begin_frame(SDL_GPUCommandBuffer* cb,
                                          const camera_uniforms& camera,
                                          const scene_light_uniforms& light,
                                          const cascade_uniforms* cascades,
                                          SDL_GPUTexture* shadow,
                                          SDL_GPUSampler* shadow_sampler,
                                          const scene_environment* environment,
                                          frame_log* log,
                                          draw_stats& stats) const;

    static constexpr int k_styles = 3;

    /// 6.11. The second axis — see `blend_style`. Three, not two, because
    /// premultiplied is a different pipeline and not a different shader.
    static constexpr int k_blends = 3;

    /// **Nine pipelines, indexed `[style][blend]`.** Lesson 4.8 had three; the
    /// count is a product now, and `create_ms()` reports what the extra six cost
    /// at startup rather than leaving it to be guessed at.
    gpu_pipeline pipelines_[k_styles][k_blends];

    /// 6.16. The TENTH pipeline: solid, opaque, and an instance-rate buffer at
    /// slot 1. Invalid when `create` was given no instanced vertex shader, which
    /// is the supported way to say "this renderer does no instanced draws" —
    /// every program built before this lesson keeps working unchanged, because
    /// the parameter defaults to null.
    gpu_pipeline instanced_;

    gpu_texture depth_;
    gpu_texture white_;
    gpu_texture flat_normal_;   ///< 6.7

    /// 6.8. A 1x1 sampled depth texture holding 1.0 — "nothing occludes". Its
    /// contents come from a render pass that clears it and draws nothing, which
    /// is the only way to write a depth texture at all: `SDL_UploadToGPUTexture`
    /// cannot target one.
    gpu_texture far_depth_;

    /// 6.15's identity fallbacks — a black cube and a black table. Both are
    /// 1x1, both are bound whenever the caller has no environment, and both
    /// exist because a declared-but-unbound sampler slot is undefined behaviour
    /// rather than a no-op.
    gpu_texture black_cube_;
    gpu_texture black_lut_;
    gpu_sampler cube_sampler_;

    /// 6.8. The comparison sampler for the fallback, and for callers that have a
    /// map but no sampler of their own.
    gpu_sampler shadow_sampler_;
    SDL_GPUTextureFormat depth_format_ = SDL_GPU_TEXTUREFORMAT_INVALID;
    SDL_GPUSampleCount samples_ = SDL_GPU_SAMPLECOUNT_1;   ///< 6.14
    Uint32 depth_w_ = 0;
    Uint32 depth_h_ = 0;
    double create_ms_ = 0.0;
};

} // namespace engine
