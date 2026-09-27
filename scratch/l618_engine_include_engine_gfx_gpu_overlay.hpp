// engine/include/engine/gfx/gpu_overlay.hpp — the overlay, on the GPU.
//
// Lesson 6.18, and the third of the three files text needed. `font.hpp` bakes
// glyphs, `overlay.hpp` turns strings into quads and can composite them on the
// CPU, and this one draws the same quads with SDL_GPU. Nothing above this file
// mentions SDL_GPU, which is why `hello_cube` — which has no device at all —
// gets text for free.
//
// ---------------------------------------------------------------------------
// WHERE IT GOES IN THE FRAME, WHICH IS A DECISION WITH A RIGHT ANSWER
// ---------------------------------------------------------------------------
//
// The engine's frame now ends: scene -> HDR float target -> bloom -> tonemap ->
// swapchain. An overlay could be composited at two points in that chain and they
// are not a matter of taste.
//
//   * **INTO THE HDR TARGET, BEFORE THE TONEMAP.** The text then goes through
//     the curve with everything else. Pure white text at linear 1.0 comes out of
//     the ACES fit at **0.8014**, which is sRGB code **232** rather than 255 —
//     your white UI is grey, by a measurable 9%. Worse, it is grey by an amount
//     that changes when the auto-exposure changes, so the HUD dims when the
//     player walks into the sun. It also blooms, which some games want on
//     purpose (diegetic UI, a holographic visor) and which no debug overlay
//     wants.
//   * **INTO THE SWAPCHAIN, AFTER THE TONEMAP.** The text is display-referred,
//     because that is what it is: a UI colour is a code somebody picked in a
//     colour picker, not a quantity of light arriving from a surface. White is
//     255 and stays 255.
//
// **This engine composites after**, and §8 of the lesson measures the 232. The
// distinction is exactly `hdr.hpp`'s scene-referred/display-referred line, and
// text is the first thing in the course that lives on the display-referred side
// from birth.
//
// ---------------------------------------------------------------------------
// WHAT THIS MEANS FOR THE FRAME GRAPH
// ---------------------------------------------------------------------------
//
// The overlay pass reads nothing and writes the swapchain, which the resolve has
// just written. In Lesson 6.17's vocabulary that is one line:
//
//     const fg_texture back2 = fg.keep(overlay_pass, back1);
//
// `keep` — "my output depends on what was already in this target" — because an
// alpha blend has the destination as an operand. From that one word the graph
// derives the ordering edge (this pass follows the resolve), the LOAD op, and
// the resolve's STORE op. **This is the first use of that API by somebody other
// than the lesson that wrote it**, and §6 reports honestly what it cost,
// including the one thing that had never been exercised: `keep` against an
// IMPORTED resource.

#pragma once

#include <engine/gfx/font.hpp>
#include <engine/gfx/gpu_buffer.hpp>
#include <engine/gfx/gpu_device.hpp>
#include <engine/gfx/gpu_pipeline.hpp>
#include <engine/gfx/gpu_texture.hpp>
#include <engine/gfx/overlay.hpp>

#include <SDL3/SDL.h>

namespace engine {

/// The vertex-stage uniform block. **Must match `overlay.vert.hlsl`'s `cbuffer
/// Viewport` field for field**, which is the standing hazard of this whole API:
/// nothing checks it, and a mismatch reads adjacent memory as a float.
struct overlay_viewport_uniforms
{
    float inv_half_width = 0.0f;    ///<  0 — 2 / viewport width
    float inv_half_height = 0.0f;   ///<  4 — 2 / viewport height
    float pad0 = 0.0f;              ///<  8
    float pad1 = 0.0f;              ///< 12 — 16-byte block, as SDL requires
};

static_assert(sizeof(overlay_viewport_uniforms) == 16, "must match overlay.vert.hlsl");

/// The fragment-stage uniform block. Matches `overlay.frag.hlsl`'s `cbuffer
/// Overlay`.
struct overlay_shading_uniforms
{
    float encode = 0.0f;        ///<  0 — 1 when the target is plain UNORM
    float stem_darken = 0.0f;   ///<  4 — k in coverage^(1-k); 0 disables
    float pad0 = 0.0f;          ///<  8
    float pad1 = 0.0f;          ///< 12
};

static_assert(sizeof(overlay_shading_uniforms) == 16, "must match overlay.frag.hlsl");

/// Draws an `overlay_batch` into a render pass somebody else began.
///
/// **It is handed a pass rather than beginning one**, the same split
/// `gpu_post_stack::resolve_into` makes and for the same reason: the target is
/// the swapchain, and the swapchain's pass belongs to the frame. Under the frame
/// graph it is handed one by `execute`, which is the arrangement this class was
/// shaped for.
class gpu_overlay
{
public:
    gpu_overlay() = default;
    ~gpu_overlay();

    gpu_overlay(const gpu_overlay&) = delete;
    gpu_overlay& operator=(const gpu_overlay&) = delete;

    /// Build the pipeline and the vertex/index streams.
    ///
    /// @param target_format what the overlay WRITES. Normally the swapchain's;
    ///        a pipeline whose colour format does not match its attachment fails
    ///        to create, which is SDL catching one of the two mistakes in this
    ///        file that it can catch.
    /// @param max_quads sizes the two stream buffers, which are allocated once
    ///        and rewritten every frame.
    [[nodiscard]] bool create(const gpu_device& dev,
                              SDL_GPUShader* vertex, SDL_GPUShader* fragment,
                              SDL_GPUTextureFormat target_format,
                              int max_quads = 4096);

    void destroy();

    [[nodiscard]] bool valid() const { return pipeline_.valid() && sampler_.valid(); }

    /// Upload an atlas's coverage bytes into a texture this object owns.
    ///
    /// Separate from `create` because a font is content and a pipeline is code:
    /// the same overlay draws several fonts over a program's life, and re-baking
    /// at a new size must not rebuild a pipeline. Recording an upload needs a
    /// command buffer, which is why this takes one and `create` does not.
    [[nodiscard]] bool set_font(const gpu_device& dev, SDL_GPUCommandBuffer* cb,
                                const font_atlas& atlas);

    /// Copy this frame's geometry to the device. **Outside any render pass** —
    /// an upload is a copy pass, and SDL forbids nesting one inside a render
    /// pass. Returns false when there is nothing to draw, which is not an error.
    [[nodiscard]] bool upload(SDL_GPUCommandBuffer* cb, const overlay_batch& batch);

    /// Draw what `upload` sent, into a pass that is already begun.
    ///
    /// @param shader_encodes true when the target is a plain UNORM format, so
    ///        the shader must apply the sRGB transfer function. **Blending is
    ///        then wrong and cannot be made right here** — see `overlay.hpp`.
    ///        Ask `gpu_device::is_srgb_format` rather than assuming.
    /// @param stem_darken passed through to the shader; 0 disables.
    void record(SDL_GPUCommandBuffer* cb, SDL_GPURenderPass* pass,
                const overlay_batch& batch, bool shader_encodes,
                float stem_darken = 0.0f) const;

    /// Bytes of vertex data the last `upload` sent. Useful, and the number §5
    /// compares against the atlas's own size.
    [[nodiscard]] Uint32 last_vertex_bytes() const { return last_vertex_bytes_; }
    [[nodiscard]] Uint32 last_index_bytes() const { return last_index_bytes_; }

    /// Indices the last `upload` sent, which is what `record` draws.
    [[nodiscard]] Uint32 last_index_count() const { return last_index_count_; }

    [[nodiscard]] const gpu_texture& atlas_texture() const { return atlas_; }
    [[nodiscard]] int max_quads() const { return max_quads_; }

private:
    gpu_pipeline pipeline_;
    gpu_sampler sampler_;
    gpu_texture atlas_;
    gpu_stream_buffer vertices_;
    gpu_stream_buffer indices_;

    int max_quads_ = 0;
    Uint32 last_vertex_bytes_ = 0;
    Uint32 last_index_bytes_ = 0;
    Uint32 last_index_count_ = 0;
};

} // namespace engine
