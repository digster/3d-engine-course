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
    ///
    /// **Three vertices, one instance, no buffers bound.** That call —
    /// `SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0)` — is the whole draw, and the
    /// triangle's corners are computed from `SV_VertexID` inside the vertex
    /// shader. See `fullscreen.vert.hlsl` for the derivation, including why it is
    /// one triangle rather than two.
    void render(SDL_GPUCommandBuffer* cb, SDL_GPURenderPass* pass,
                SDL_GPUTexture* hdr, const tonemap_settings& settings,
                bool shader_encodes) const;

    [[nodiscard]] double create_ms() const { return pipeline_.create_ms(); }

private:
    gpu_pipeline pipeline_;
    gpu_sampler sampler_;
};

} // namespace engine
