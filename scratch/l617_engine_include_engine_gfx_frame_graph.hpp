// engine/include/engine/gfx/frame_graph.hpp — the frame, as data instead of as code.
//
// Lesson 6.17. By this point the engine can record eight kinds of render pass and
// there is no single place where a whole frame happens. `gpu_shadow_map::render`
// begins its own pass; `gpu_bloom::render` begins eleven; `gpu_scene_renderer::
// render` is handed one; `gpu_post_stack::resolve_into` is handed a different
// one and its header says, in prose, that it must be called INSIDE the display
// pass while `render_bloom` must be called OUTSIDE it. Every one of those rules
// is true, none of them is checked, and the only artefact that knows the whole
// order is whichever function happens to be assembling the frame today.
//
// ---------------------------------------------------------------------------
// WHAT THIS IS FOR, AND WHAT IT IS NOT FOR
// ---------------------------------------------------------------------------
//
// The usual pitch for a frame graph is MEMORY ALIASING: two transient targets
// whose lifetimes do not overlap can share one allocation, so a frame with
// twenty intermediates pays for the peak rather than the sum. That pitch is
// real in engines with twenty intermediates and it is **very nearly worthless
// here**, for two measured reasons that Lesson 6.17 §5 walks through:
//
//   * SDL_GPU HAS NO ALIASING PRIMITIVE. There is no placed resource, no heap,
//     no `SDL_CreateGPUTextureAliased`. Checked against SDL_gpu.h at 3.4.12: the
//     only reuse available is handing back a WHOLE texture whose descriptor
//     matches exactly, which is strictly weaker than sharing bytes.
//   * OUR FRAME IS A CHAIN, NOT A TREE. Every intermediate feeds the next stage,
//     so almost nothing is dead while anything else is live.
//
// What the graph actually buys — and this IS measured, in §6 and §7 — is that
// four facts a human currently maintains by hand become DERIVED from one
// statement each:
//
//   1. THE ORDER of the passes.
//   2. THE LOAD OP of every attachment. Lesson 6.13 called one of these
//      "load-bearing" and described its failure as looking like a tuning
//      problem. It is not a decision here; it follows from whether the pass
//      depends on what was already in the target.
//   3. THE STORE OP of every attachment. Lesson 4.7 chose DONT_CARE for the
//      scene's depth and Lesson 6.8 had to choose STORE for the shadow map's.
//      Both follow from "does any later pass consume this?".
//   4. WHICH PASSES RUN AT ALL. Turn the bloom off and eleven passes disappear
//      because nothing reads their output — not because somebody remembered to
//      write `if (!s.enabled) return;`.
//
// So the sentence to keep is: **a frame graph is not a memory optimiser that
// happens to order passes. It is an ordering-and-lifetime deduction that
// sometimes also saves memory.** Here it saves almost none, and is worth
// building anyway.
//
// ---------------------------------------------------------------------------
// THE ONE IDEA: A RESOURCE IS NOT A THING, IT IS A SEQUENCE OF VALUES
// ---------------------------------------------------------------------------
//
// Everything below follows from refusing to let "the HDR target" name a
// texture. It names a *variable*, and a variable has values over time:
//
//     hdr@0  undefined
//     hdr@1  what the scene pass produced
//     hdr@2  what the skybox pass produced from hdr@1
//
// A pass that samples `hdr@1` must run after whoever produced `hdr@1` and before
// whoever overwrites it. That is a data dependency in the ordinary compiler
// sense, and it is EXACT: it says nothing about textures, barriers or passes.
// Ordering, load ops, store ops and lifetimes all fall out of it, which is why
// this file has one concept in it and not four.
//
// A version is produced by exactly one pass. Two passes claiming to produce
// `hdr@2` is not a race to be scheduled around, it is an ambiguity in what the
// author meant, and `compile` rejects it by name rather than picking one.
//
// ---------------------------------------------------------------------------
// THE NINETY-PERCENT PICTURE — what this graph deliberately does not do
// ---------------------------------------------------------------------------
//
//   * NO SUBRESOURCE VERSIONS. A version covers the whole texture, so four
//     cascade passes writing four different LAYERS of one array serialise into
//     `sm@1 -> sm@2 -> sm@3 -> sm@4` even though they are independent. It costs
//     nothing today, because a command buffer submits its passes in order
//     anyway; it would cost something on an API with parallel queues. Per-layer
//     versions are the fix and they roughly double the bookkeeping.
//   * NO ASYNC COMPUTE, NO QUEUES, NO SPLIT BARRIERS. SDL_GPU has one graphics
//     queue as far as this API is concerned, so there is nothing to schedule
//     across.
//   * NO BUFFERS, ONLY TEXTURES. Every cross-pass resource in this engine is a
//     texture. Buffers would be the same machinery with a different descriptor.
//   * IT PLANS ONE COMMAND BUFFER. Multi-buffer frames are Module 9's job
//     system.
//
// The frame graph literature to read next is named in the lesson's Further
// Reading: O'Donnell's "FrameGraph: Extensible Rendering Architecture in
// Frostbite" (GDC 2017) is where the version-per-write idea above comes from.

#pragma once

#include <engine/gfx/gpu_device.hpp>
#include <engine/gfx/gpu_texture.hpp>

#include <SDL3/SDL.h>

#include <cstddef>

namespace engine {

/// Capacity caps, and why they are caps rather than `std::vector`.
///
/// A frame graph is rebuilt **every frame**. A version that allocates is a
/// per-frame malloc storm hidden behind an architecture, which is the exact
/// failure mode Module 5's data-oriented lessons warned about. These arrays live
/// inside the `frame_graph` object, which the caller keeps across frames, so
/// building a frame touches no allocator at all — §7 measures what that costs.
///
/// The numbers are sized from the frame this engine actually renders (4 cascades
/// + scene + skybox + 11 bloom + resolve = 18 passes, 11 resources) with room to
/// roughly double. Exceeding one is a logged error from `add_pass` / `create`,
/// not a silent truncation.
inline constexpr int k_max_fg_passes = 48;
inline constexpr int k_max_fg_resources = 32;

/// Accesses per pass: reads plus writes. The scene pass is the widest user —
/// it samples a shadow map, three environment textures and a material texture,
/// and writes colour and depth — but only the cross-pass ones are declared here.
inline constexpr int k_max_fg_accesses = 12;

/// Colour attachments in one pass. `SDL_BeginGPURenderPass` takes an array, so
/// supporting more than one costs a constant rather than a branch; nothing in
/// this engine uses more than one yet.
inline constexpr int k_max_fg_colours = 4;

/// Versions per resource. The deepest is the shadow array at one per cascade
/// plus the initial undefined value.
inline constexpr int k_max_fg_versions = 12;

/// Pooled textures the graph may own at once.
inline constexpr int k_max_fg_pool = 32;

/// What a transient texture must be, in the only terms the pool can compare.
///
/// **Every field participates in `matches`**, and that is the whole reason the
/// aliasing story here is weak: two targets that differ in a single field cannot
/// share a texture, where an engine aliasing raw memory would only care that one
/// allocation is big enough for both.
struct fg_texture_desc
{
    Uint32 width = 0;
    Uint32 height = 0;
    SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_INVALID;
    SDL_GPUSampleCount samples = SDL_GPU_SAMPLECOUNT_1;

    /// Array layers. 4 for the cascade map (Lesson 6.9); 1 for everything else.
    Uint32 layers = 1;

    /// True for a depth/stencil target, false for a colour target. It selects
    /// which of `gpu_texture`'s two creation functions the pool calls, and the
    /// two produce textures with different usage flags — so it is part of the
    /// descriptor rather than a hint.
    bool depth = false;

    /// Must a shader be able to sample it? A multisample texture cannot be
    /// (Lesson 6.14), and a depth target that nothing reads should not be, since
    /// the usage flag can cost a different memory layout on some drivers.
    bool sampled = true;

    [[nodiscard]] bool matches(const fg_texture_desc& other) const;

    /// Footprint in bytes, for §5's memory table. Mip levels are not counted:
    /// every transient in this engine's frame is a single-level render target.
    [[nodiscard]] std::size_t bytes() const;
};

/// A handle to **one value of one resource** — not to a texture.
///
/// Passing the wrong version is the mistake this type exists to make visible:
/// `hdr@1` and `hdr@2` are different C++ values, so sampling the pre-skybox
/// image where you meant the post-skybox one is a different variable name and
/// not an identical pointer.
struct fg_texture
{
    static constexpr Uint16 k_none = 0xFFFFu;

    Uint16 index = k_none;   ///< which resource
    Uint16 version = 0;      ///< which value of it

    [[nodiscard]] bool valid() const { return index != k_none; }
};

/// How a pass treats what was already in an attachment. **This is a statement
/// about dataflow, not about SDL**, and the load op is derived from it:
///
///   `discard` -> DONT_CARE   the pass writes every texel and reads none
///   `clear`   -> CLEAR       the pass wants a known value underneath
///   `keep`    -> LOAD        the pass's output DEPENDS on the old contents
///
/// The third is the interesting one. `gpu_bloom`'s upsample blends `src + dst`,
/// so the destination is an operand; Lesson 6.13 wrote the load op by hand and
/// warned in a comment that getting it wrong "looks like a tuning problem rather
/// than a load op". Spelled as `keep`, it is not a load op at all — it is the
/// sentence "this pass reads its own target", from which the load op, the
/// ordering edge and the previous pass's store op all follow.
enum class fg_init : Uint8
{
    discard,
    clear,
    keep
};

/// What a pass does with a resource. Internal to an access record; exposed
/// because `frame_graph::access_at` reports it for the tables in §6.
enum class fg_use : Uint8
{
    sample,   ///< read through a sampler in a shader
    colour,   ///< a colour attachment of this pass
    depth     ///< the depth/stencil attachment of this pass
};

class frame_graph;

/// What a pass's callback is handed.
///
/// The render pass is **already begun** — with the attachments, load ops and
/// store ops the graph derived — so a pass body contains binds and draws and
/// never a `SDL_BeginGPURenderPass`. That inversion is the point: the ops are
/// exactly the facts we are trying to stop people writing by hand.
struct fg_pass_context
{
    SDL_GPUCommandBuffer* cb = nullptr;
    SDL_GPURenderPass* pass = nullptr;
    const frame_graph* graph = nullptr;
    int pass_index = -1;

    /// The real texture behind a handle. Valid for any handle the pass declared.
    [[nodiscard]] SDL_GPUTexture* texture(fg_texture h) const;
};

/// A plain function pointer and a `void*`, not `std::function`.
///
/// `std::function` type-erases, which means it may heap-allocate for any callable
/// larger than its small-buffer, and it is exactly the lambdas that capture a
/// renderer, a settings block and a draw list that exceed it. A frame graph
/// rebuilt every frame would then allocate once per pass, per frame, invisibly.
/// A function pointer plus a user pointer costs two words, never allocates, and
/// is callable from the engine core with exceptions off.
using fg_execute_fn = void (*)(const fg_pass_context& ctx, void* user);

/// Declare a frame; let the order, the ops and the lifetimes be deduced.
///
/// Usage is three phases, in this order, every frame:
///
/// ```cpp
/// fg.reset();                                  // 1. declare
/// const fg_texture hdr0 = fg.create("hdr", hdr_desc);
/// const fg_texture back = fg.import("backbuffer", swapchain, back_desc);
/// const int scene = fg.add_pass("scene", &draw_scene, &ctx);
/// const fg_texture hdr1 = fg.clear(scene, hdr0, black);
/// ...
/// if (!fg.compile(dev)) { /* the graph said what is wrong */ }   // 2. deduce
/// fg.execute(cb);                                                // 3. record
/// ```
///
/// **`reset` keeps the texture pool.** Declarations are per frame; the textures
/// they resolve to are not, because creating a render target is a driver call
/// and a possible stall — the same argument `gpu_bloom::resize` makes for being
/// idempotent.
class frame_graph
{
public:
    frame_graph() = default;
    ~frame_graph();

    frame_graph(const frame_graph&) = delete;
    frame_graph& operator=(const frame_graph&) = delete;

    /// Forget this frame's passes and resources. Keeps the pool and its textures.
    void reset();

    /// Release every pooled texture. Called by the destructor; call it by hand
    /// before the device goes away.
    void destroy();

    // ---- Phase 1: declare -------------------------------------------------

    /// A texture the graph owns, sized from the pool at `compile` time.
    ///
    /// The returned handle is **version 0: undefined contents**. Sampling it is
    /// an error the compiler catches, because nothing has produced a value yet.
    [[nodiscard]] fg_texture create(const char* name, const fg_texture_desc& desc);

    /// A texture somebody else owns — the swapchain, a streamed asset, last
    /// frame's history buffer.
    ///
    /// **Version 0 of an import is a real value**, not undefined: whatever is in
    /// the texture when the frame begins. So sampling it without a producer is
    /// legal, which is precisely the difference between an import and a create.
    ///
    /// Imports are also the graph's **cull roots**. A transient exists only to
    /// feed something; a pass survives iff it transitively feeds a write to an
    /// imported resource, because an import is the only thing whose value is
    /// observable after the frame ends.
    [[nodiscard]] fg_texture import(const char* name, SDL_GPUTexture* texture,
                                    const fg_texture_desc& desc);

    /// Add a pass. Returns its index, or -1 if the cap is exceeded.
    [[nodiscard]] int add_pass(const char* name, fg_execute_fn fn, void* user);

    /// This pass reads `h` through a sampler.
    void sample(int pass, fg_texture h);

    /// This pass writes every texel of `h`'s colour attachment and does not read
    /// the old contents. -> DONT_CARE.
    [[nodiscard]] fg_texture discard_write(int pass, fg_texture h, Uint32 layer = 0);

    /// This pass wants a known value underneath what it draws. -> CLEAR.
    [[nodiscard]] fg_texture clear(int pass, fg_texture h, SDL_FColor colour,
                                   Uint32 layer = 0);

    /// This pass's output depends on what was already there — a blend, a decal,
    /// a second batch into the same target. -> LOAD, plus a dependency on
    /// whoever produced `h`.
    [[nodiscard]] fg_texture keep(int pass, fg_texture h, Uint32 layer = 0);

    /// The same three, for the depth attachment.
    [[nodiscard]] fg_texture clear_depth(int pass, fg_texture h, float value,
                                         Uint32 layer = 0);
    [[nodiscard]] fg_texture keep_depth(int pass, fg_texture h, Uint32 layer = 0);
    [[nodiscard]] fg_texture discard_depth(int pass, fg_texture h, Uint32 layer = 0);

    /// Attach a multisample resolve to this pass's colour write of `target`.
    ///
    /// Lesson 6.14's rule, made structural: with MSAA on, the scene's colour
    /// attachment is a texture nothing may sample and the single-sample resolve
    /// destination is what everything downstream reads. Declaring it here makes
    /// "forgot the resolve" impossible in the same way the load op is —
    /// `resolved` gets a new version, and that version is what later passes name.
    ///
    /// @param target the handle `clear`/`discard_write` returned for the
    ///        multisample attachment.
    /// @return the resolved resource's new version.
    [[nodiscard]] fg_texture resolve(int pass, fg_texture target, fg_texture resolved);

    // ---- Phase 2: deduce --------------------------------------------------

    /// Validate, cull, order, derive every load and store op, compute lifetimes
    /// and bind each transient to a pooled texture.
    ///
    /// Returns false and logs the specific problem for: a sampled version with
    /// no producer, two producers of one version, a cycle (naming the passes in
    /// it), a pass whose attachments disagree about size or sample count, or an
    /// exhausted pool. **Every one of those is a silent wrong picture today.**
    [[nodiscard]] bool compile(const gpu_device& dev);

    // ---- Phase 3: record --------------------------------------------------

    /// Begin, execute and end every live pass, in the compiled order.
    void execute(SDL_GPUCommandBuffer* cb) const;

    // ---- What the deduction found, for tooling and for the lesson ----------

    [[nodiscard]] int pass_count() const { return pass_count_; }
    [[nodiscard]] int resource_count() const { return resource_count_; }

    /// How many passes survived culling, and how many were dropped.
    [[nodiscard]] int live_pass_count() const { return live_count_; }
    [[nodiscard]] int culled_pass_count() const { return pass_count_ - live_count_; }

    /// The compiled order: `scheduled(i)` is the pass index that runs i'th.
    [[nodiscard]] int scheduled(int i) const;

    [[nodiscard]] const char* pass_name(int pass) const;
    [[nodiscard]] const char* resource_name(int res) const;
    [[nodiscard]] bool pass_alive(int pass) const;

    /// How many accesses a pass declared, and what the i'th one resolved to.
    /// Reported so a debug view — and §6's table — can print the derived ops
    /// beside the hand-written ones rather than trusting either.
    struct access_report
    {
        int resource = -1;
        Uint16 version_in = 0;
        Uint16 version_out = 0;
        fg_use use = fg_use::sample;
        fg_init init = fg_init::discard;
        SDL_GPULoadOp load_op = SDL_GPU_LOADOP_DONT_CARE;
        SDL_GPUStoreOp store_op = SDL_GPU_STOREOP_DONT_CARE;
        Uint32 layer = 0;
    };

    [[nodiscard]] int access_count(int pass) const;
    [[nodiscard]] access_report access_at(int pass, int i) const;

    /// The texture a handle resolved to after `compile`. Null before it.
    [[nodiscard]] SDL_GPUTexture* texture(fg_texture h) const;

    /// First and last position in the SCHEDULE at which a resource is live,
    /// or -1 / -1 when nothing live touches it. This is the interval §5 colours.
    [[nodiscard]] int first_use(int res) const;
    [[nodiscard]] int last_use(int res) const;

    /// Bytes if every transient got its own texture, versus bytes the pool
    /// actually needed. The difference is what descriptor-keyed reuse saved —
    /// measured in §7, where it comes out at **zero on this engine's frame**, for
    /// a reason worth knowing before building this.
    ///
    /// **Counted over the SLOTS this frame used**, not over the textures this
    /// compile created — otherwise the second frame reports a saving of
    /// everything, because the pool was already warm.
    [[nodiscard]] std::size_t transient_bytes_naive() const { return naive_bytes_; }
    [[nodiscard]] std::size_t transient_bytes_pooled() const { return pooled_bytes_; }

    /// The largest total of simultaneously-live transient bytes. This is the
    /// floor an engine with real memory aliasing would reach, and comparing it
    /// against `transient_bytes_naive` says how much aliasing could EVER buy
    /// here — before asking whether SDL_GPU can express it (it cannot).
    [[nodiscard]] std::size_t transient_bytes_peak() const { return peak_bytes_; }

    /// One transient's footprint, for §7's table.
    [[nodiscard]] std::size_t resource_bytes(int res) const;

    /// Which pooled texture a resource was bound to, or -1. Two resources with
    /// the same slot are the aliasing, such as it is.
    [[nodiscard]] int pool_slot(int res) const;

    /// How many pooled textures exist. Grows on demand, never shrinks.
    [[nodiscard]] int pool_size() const { return pool_size_; }

    /// Microseconds the last `compile` took. The whole justification for the
    /// fixed-capacity arrays above, so it is measured rather than assumed.
    [[nodiscard]] double compile_us() const { return compile_us_; }

private:
    struct access
    {
        Uint16 resource = fg_texture::k_none;
        Uint16 version_in = 0;
        Uint16 version_out = 0;
        fg_use use = fg_use::sample;
        fg_init init = fg_init::discard;
        Uint32 layer = 0;
        SDL_FColor clear_colour{0.0f, 0.0f, 0.0f, 1.0f};
        float clear_depth = 1.0f;

        /// Which resource this attachment resolves into, or `k_none`.
        Uint16 resolve_resource = fg_texture::k_none;
        Uint16 resolve_version_out = 0;

        SDL_GPULoadOp load_op = SDL_GPU_LOADOP_DONT_CARE;
        SDL_GPUStoreOp store_op = SDL_GPU_STOREOP_DONT_CARE;
    };

    struct pass
    {
        const char* name = "";
        fg_execute_fn fn = nullptr;
        void* user = nullptr;
        access accesses[k_max_fg_accesses];
        int access_count = 0;
        bool alive = false;
    };

    struct resource
    {
        const char* name = "";
        fg_texture_desc desc;
        SDL_GPUTexture* imported = nullptr;
        Uint16 versions = 0;          ///< highest version produced
        int pool_slot = -1;
        int first_use = -1;
        int last_use = -1;
    };

    [[nodiscard]] fg_texture write(int pass, fg_texture h, fg_use use, fg_init init,
                                   Uint32 layer, SDL_FColor colour, float depth);

    /// Which pass produced (res, version), or -1. Version 0 is produced by
    /// nobody: undefined for a transient, the caller's contents for an import.
    [[nodiscard]] int producer_of(int res, int version) const;

    pass passes_[k_max_fg_passes];
    resource resources_[k_max_fg_resources];
    int pass_count_ = 0;
    int resource_count_ = 0;

    int schedule_[k_max_fg_passes];
    int live_count_ = 0;

    gpu_texture pool_[k_max_fg_pool];
    int pool_size_ = 0;

    std::size_t naive_bytes_ = 0;
    std::size_t pooled_bytes_ = 0;
    std::size_t peak_bytes_ = 0;
    double compile_us_ = 0.0;
    bool compiled_ = false;
};

/// The frame graph's whole deduction, printed. For a debug key and for §6.
///
/// One line per scheduled pass with its attachments and their derived ops, then
/// one line per resource with its lifetime interval and pool slot. Writing it out
/// is how you find that a pass you expected was culled — which, the first time it
/// happens, always looks like the pass being broken.
void dump_frame_graph(const frame_graph& fg);

} // namespace engine
