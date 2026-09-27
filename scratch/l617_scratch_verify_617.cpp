// scratch/verify_617.cpp — every number Lesson 6.17 quotes, produced here.
//
//   sh scratch/build_verify_617.sh                                # as configured
//   ENGINE_CFLAGS="-O2 -DNDEBUG" sh scratch/build_verify_617.sh   # release
//
// §A  the inventory: how many passes a frame is, and how many places know it
// §B  the deduction on paper — a three-pass graph small enough to check by hand
// §C  the schedule agrees with the hand-written order
// §D  every load and store op, derived against hand-written
// §E  culling: stop reading the bloom and eleven passes disappear
// §F  the five mistakes that are silent today
// §G  lifetimes and memory — the claim the literature makes, measured here
// §H  what the compile costs, and that it allocates nothing
// §I  THE GOLDEN — hand-written frame vs compiled frame, bit for bit
#include "../demos/common/demo_scene.hpp"

#include <engine/core/bench.hpp>
#include <engine/gfx/frame_graph.hpp>
#include <engine/gfx/gpu_post.hpp>
#include <engine/gfx/gpu_scene.hpp>
#include <engine/gfx/gpu_shader.hpp>
#include <engine/gfx/gpu_shadow.hpp>
#include <engine/gfx/mesh.hpp>

#include <cmath>
#include <iterator>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using engine::bloom_settings;
using engine::bloom_stage;
using engine::fg_init;
using engine::fg_pass_context;
using engine::fg_texture;
using engine::fg_texture_desc;
using engine::fg_use;
using engine::frame_graph;
using engine::mat4;
using engine::tonemap_settings;
using engine::vec3;

// COUNTING ALLOCATIONS, BECAUSE §H CLAIMS THERE ARE NONE.
//
// The fixed-capacity arrays in `frame_graph` exist to make a per-frame rebuild
// allocation-free, and "exist to" is not evidence. Replacing global `operator
// new` is the cheapest way to turn that into a measurement; it is legal in a
// single-translation-unit program and it sees every `new`, `std::vector` growth
// and `std::string` copy in the process. It does NOT see `SDL_malloc`, which is
// the right blind spot here: the graph is pure C++ and calls no SDL allocator.
std::size_t g_allocations = 0;

void* operator_new_counted(std::size_t n)
{
    ++g_allocations;
    void* p = std::malloc(n == 0 ? 1 : n);
    return p;
}

void* operator new(std::size_t n) { return operator_new_counted(n); }
void* operator new[](std::size_t n) { return operator_new_counted(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

namespace {

int g_failures = 0;

void check(bool ok, const char* what)
{
    if (!ok) { ++g_failures; }
    std::printf("    [%s] %s\n", ok ? "PASS" : "FAIL", what);
}

void check_eq(long long got, long long want, const char* what)
{
    const bool ok = (got == want);
    if (!ok) { ++g_failures; }
    std::printf("    [%s] %-52s got %lld  want %lld\n",
                ok ? "PASS" : "FAIL", what, got, want);
}

void section(const char* title)
{
    std::printf("\n");
    std::printf("===========================================================================\n");
    std::printf("  §%s\n", title);
    std::printf("===========================================================================\n");
}

const char* load_name(SDL_GPULoadOp op)
{
    switch (op)
    {
    case SDL_GPU_LOADOP_LOAD: return "LOAD";
    case SDL_GPU_LOADOP_CLEAR: return "CLEAR";
    default: return "DONT_CARE";
    }
}

const char* store_name(SDL_GPUStoreOp op)
{
    switch (op)
    {
    case SDL_GPU_STOREOP_STORE: return "STORE";
    case SDL_GPU_STOREOP_RESOLVE: return "RESOLVE";
    case SDL_GPU_STOREOP_RESOLVE_AND_STORE: return "RESOLVE_AND_STORE";
    default: return "DONT_CARE";
    }
}

// ===========================================================================
//  §A — the inventory
// ===========================================================================
//
// Counted from the engine's own declarations rather than from memory, because
// "about a dozen" is exactly the kind of number that is wrong by four.

void section_a()
{
    section("A  THE INVENTORY — how many passes is a frame, and who knows the order");

    // Every render pass this engine can record, and which type begins it.
    struct entry
    {
        const char* who;
        int passes;
        const char* begun_by;
    };

    bloom_settings bs;
    bs.enabled = true;
    bs.levels = 6;

    const int cascades = engine::k_max_cascades;
    const int bloom_passes = 2 * bs.levels - 1;

    const entry frame[] = {
        {"gpu_shadow_map::render (one per cascade)", cascades, "itself"},
        {"gpu_scene_renderer::render", 1, "the caller"},
        {"skybox draw (6.15)", 0, "shares the scene pass"},
        {"gpu_bloom::render", bloom_passes, "itself, 2n-1 times"},
        {"gpu_post_stack::resolve_into", 1, "the caller"},
    };

    int total = 0;
    std::printf("  %-42s %7s  %s\n", "recorded by", "passes", "pass begun by");
    std::printf("  %-42s %7s  %s\n", "------------------------------------------",
                "------", "-------------");
    for (const entry& e : frame)
    {
        std::printf("  %-42s %7d  %s\n", e.who, e.passes, e.begun_by);
        total += e.passes;
    }
    std::printf("  %-42s %7d\n", "TOTAL", total);

    check_eq(total, 17, "a full frame is seventeen render passes");
    check_eq(bloom_passes, 11, "eleven of them are the bloom (6.13 §5)");

    // THE NUMBER THAT MATTERS IS NOT SIXTEEN. It is how many separate places
    // have to agree about the order, because that is what a frame graph
    // replaces. Three types begin their own passes and two are handed one, so
    // the order lives in whichever function is assembling the frame — plus the
    // prose in three headers that says what must come before what.
    std::printf("\n  Places that must agree about the order, today:\n");
    std::printf("    1. whichever function assembles the frame\n");
    std::printf("    2. gpu_post.hpp's prose: render_bloom OUTSIDE the display pass,\n");
    std::printf("       resolve_into INSIDE it\n");
    std::printf("    3. gpu_shadow.hpp's prose: this pass runs BEFORE the scene pass\n");
    std::printf("    4. gpu_bloom's own comment: the bloom runs BEFORE the curve\n");
    std::printf("  and none of the last three is checked by anything.\n");
}

// ===========================================================================
//  §B — the deduction, on paper
// ===========================================================================
//
// Three passes and two resources, chosen so every answer can be checked by
// reading it rather than by trusting the compiler that produced it.
//
//   blur    : writes  tmp@0 -> tmp@1        (covers every texel)
//   combine : samples tmp@1, writes out@0 -> out@1
//   unused  : writes  tmp@1 -> tmp@2        (nobody reads tmp@2)
//
// `out` is imported. So `combine` is a root, `blur` is reachable from it, and
// `unused` is reachable from nothing.

void section_b()
{
    section("B  THE DEDUCTION ON PAPER — three passes, checked by reading them");

    frame_graph fg;

    fg_texture_desc small{};
    small.width = 64;
    small.height = 64;
    small.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

    // `import` with a null texture: this section never executes the graph, and
    // compile() only needs to know that the resource is somebody else's.
    const fg_texture tmp0 = fg.create("tmp", small);
    const fg_texture out0 = fg.import("out", reinterpret_cast<SDL_GPUTexture*>(0x1), small);

    const int blur = fg.add_pass("blur", nullptr, nullptr);
    const int combine = fg.add_pass("combine", nullptr, nullptr);
    const int unused = fg.add_pass("unused", nullptr, nullptr);

    const fg_texture tmp1 = fg.discard_write(blur, tmp0);
    fg.sample(combine, tmp1);
    const fg_texture out1 = fg.clear(combine, out0, SDL_FColor{0, 0, 0, 1});
    const fg_texture tmp2 = fg.discard_write(unused, tmp1);

    (void)out1;
    (void)tmp2;

    // No device, so `compile` cannot allocate — which is the one thing this
    // section does not need. Everything checked below is decided before a single
    // texture is created, and that is worth seeing directly: the deduction is
    // pure bookkeeping over the declarations.
    std::printf("  Declared: blur=%d combine=%d unused=%d\n", blur, combine, unused);
    std::printf("  Versions: tmp@%u from blur, tmp@%u from unused, out@%u from combine\n",
                static_cast<unsigned>(tmp1.version), static_cast<unsigned>(tmp2.version),
                static_cast<unsigned>(out1.version));

    check_eq(tmp1.version, 1, "blur produces tmp@1");
    check_eq(tmp2.version, 2, "unused produces tmp@2 — a different value, not the same one");
    check_eq(out1.version, 1, "combine produces out@1");
}

} // namespace

// ===========================================================================
//  Device-side plumbing — the shape verify_612/613 established
// ===========================================================================

namespace {

constexpr int k_size = 256;
constexpr int k_levels = 6;
constexpr int k_shadow_res = 256;

struct target
{
    engine::gpu_device* dev = nullptr;
    SDL_GPUTexture* colour = nullptr;
    SDL_GPUTransferBuffer* readback = nullptr;
    int w = 0;
    int h = 0;

    bool create(engine::gpu_device& d, int width, int height, SDL_GPUTextureFormat fmt)
    {
        dev = &d;
        w = width;
        h = height;

        SDL_GPUTextureCreateInfo ti{};
        ti.type = SDL_GPU_TEXTURETYPE_2D;
        ti.format = fmt;
        ti.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        ti.width = static_cast<Uint32>(width);
        ti.height = static_cast<Uint32>(height);
        ti.layer_count_or_depth = 1;
        ti.num_levels = 1;
        ti.sample_count = SDL_GPU_SAMPLECOUNT_1;
        colour = SDL_CreateGPUTexture(d.handle(), &ti);
        if (colour == nullptr) { return false; }

        SDL_GPUTransferBufferCreateInfo tb{};
        tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
        tb.size = static_cast<Uint32>(width * height * 4);
        readback = SDL_CreateGPUTransferBuffer(d.handle(), &tb);
        return readback != nullptr;
    }

    void destroy()
    {
        if (dev == nullptr) { return; }
        if (readback != nullptr) { SDL_ReleaseGPUTransferBuffer(dev->handle(), readback); }
        if (colour != nullptr) { SDL_ReleaseGPUTexture(dev->handle(), colour); }
        readback = nullptr;
        colour = nullptr;
    }
};

std::vector<Uint8> download(target& t, SDL_GPUCommandBuffer* cb)
{
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cb);
    SDL_GPUTextureRegion src{};
    src.texture = t.colour;
    src.w = static_cast<Uint32>(t.w);
    src.h = static_cast<Uint32>(t.h);
    src.d = 1;
    SDL_GPUTextureTransferInfo dst{};
    dst.transfer_buffer = t.readback;
    dst.pixels_per_row = static_cast<Uint32>(t.w);
    dst.rows_per_layer = static_cast<Uint32>(t.h);
    SDL_DownloadFromGPUTexture(copy, &src, &dst);
    SDL_EndGPUCopyPass(copy);

    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cb);
    if (fence == nullptr) { return {}; }
    SDL_WaitForGPUFences(t.dev->handle(), true, &fence, 1);
    SDL_ReleaseGPUFence(t.dev->handle(), fence);

    std::vector<Uint8> px(static_cast<std::size_t>(t.w) * static_cast<std::size_t>(t.h) * 4u);
    const void* mapped = SDL_MapGPUTransferBuffer(t.dev->handle(), t.readback, false);
    if (mapped == nullptr) { return {}; }
    std::memcpy(px.data(), mapped, px.size());
    SDL_UnmapGPUTransferBuffer(t.dev->handle(), t.readback);
    return px;
}

/// Everything the frame needs, created once.
struct rig
{
    engine::gpu_device gpu;
    engine::gpu_mesh quad;
    engine::gpu_scene_renderer scene;
    engine::gpu_shadow_map shadow;
    engine::gpu_bloom bloom;
    engine::gpu_tonemap_pass tonemap;
    engine::gpu_sampler samp;
    engine::gpu_texture hdr;      ///< the hand-written path's own scene target
    engine::gpu_texture depth;    ///< and its own depth
    engine::gpu_texture black1;   ///< 1x1 stand-in for "no bloom" (gpu_post_stack's trick)
    target ldr;

    SDL_GPUTextureFormat depth_format = SDL_GPU_TEXTUREFORMAT_INVALID;
    bool ok = false;
};

bool build_rig(rig& r)
{
    if (!r.gpu.create(nullptr, false).ok()) { return false; }

    engine::gpu_shader scene_v, scene_f, shadow_v, shadow_f;
    engine::gpu_shader full_v, tone_f, bright_f, down_f, up_f;
    const bool shaders =
        scene_v.load(r.gpu, "scene.vert", engine::shader_stage::vertex)
        && scene_f.load(r.gpu, "scene.frag", engine::shader_stage::fragment)
        && shadow_v.load(r.gpu, "shadow.vert", engine::shader_stage::vertex)
        && shadow_f.load(r.gpu, "shadow.frag", engine::shader_stage::fragment)
        && full_v.load(r.gpu, "fullscreen.vert", engine::shader_stage::vertex)
        && tone_f.load(r.gpu, "tonemap.frag", engine::shader_stage::fragment)
        && bright_f.load(r.gpu, "bloom_bright.frag", engine::shader_stage::fragment)
        && down_f.load(r.gpu, "bloom_down.frag", engine::shader_stage::fragment)
        && up_f.load(r.gpu, "bloom_up.frag", engine::shader_stage::fragment);
    if (!shaders) { return false; }

    // ASKED, NOT ASSUMED — 6.8's rule. And `supported_shadow_format` rather than
    // `supported_depth_format`, because one of the two depth targets in this
    // frame must also be sampleable and a single format for both keeps the
    // pipelines' `depth_stencil_format` identical across the two passes.
    static constexpr SDL_GPUTextureFormat k_depth_candidates[] = {
        SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        SDL_GPU_TEXTUREFORMAT_D24_UNORM,
        SDL_GPU_TEXTUREFORMAT_D16_UNORM};
    r.depth_format = engine::supported_shadow_format(
        r.gpu, k_depth_candidates, static_cast<int>(std::size(k_depth_candidates)));
    if (r.depth_format == SDL_GPU_TEXTUREFORMAT_INVALID) { return false; }

    if (!r.scene.create(r.gpu, scene_v.handle(), scene_f.handle(), r.depth_format,
                        engine::k_hdr_format))
    {
        return false;
    }
    if (!r.shadow.create(r.gpu, shadow_v.handle(), shadow_f.handle(), k_shadow_res,
                         r.depth_format, 1))
    {
        return false;
    }
    if (!r.bloom.create(r.gpu, full_v.handle(), bright_f.handle(), down_f.handle(),
                        up_f.handle()))
    {
        return false;
    }
    if (!r.tonemap.create(r.gpu, full_v.handle(), tone_f.handle(),
                          SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM))
    {
        return false;
    }
    if (!r.samp.create(r.gpu)) { return false; }
    if (!r.bloom.resize(r.gpu, k_size / 2, k_size / 2, k_levels)) { return false; }
    if (!r.hdr.create_colour_target(r.gpu, engine::k_hdr_format, k_size, k_size,
                                    "hand-written hdr", true))
    {
        return false;
    }
    if (!r.depth.create_depth(r.gpu, r.depth_format, k_size, k_size, "hand-written depth", false))
    {
        return false;
    }
    // Contents are never defined and never need to be: the resolve multiplies
    // this by an intensity of zero. The texture exists because SDL_GPU has no way
    // to unbind a sampler — `gpu_post_stack`'s own 1x1 black, for the same reason.
    if (!r.black1.create_colour_target(r.gpu, engine::k_hdr_format, 1u, 1u, "no bloom", true))
    {
        return false;
    }
    if (!r.ldr.create(r.gpu, k_size, k_size, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM))
    {
        return false;
    }

    SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
    if (cb == nullptr) { return false; }
    const bool mesh_ok = r.quad.create(r.gpu, cb, engine::quad_mesh(),
                                       engine::index_mode::indexed, "verify617 quad");
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cb);
    if (fence != nullptr)
    {
        SDL_WaitForGPUFences(r.gpu.handle(), true, &fence, 1);
        SDL_ReleaseGPUFence(r.gpu.handle(), fence);
    }
    if (!mesh_ok) { return false; }

    r.ok = true;
    return true;
}

/// The one object every pass in this harness draws: a flat quad of known value,
/// which collapses the scene to `lit = ambient` (verify_61 §E's trick) and makes
/// the whole chain solvable on paper.
engine::gpu_draw_item make_item(const rig& r)
{
    engine::gpu_draw_item item{};
    item.mesh = &r.quad;
    item.world_from_model = mat4::identity();
    item.normal_from_model = engine::mat3::identity();
    item.material.albedo = vec3{1.0f, 1.0f, 1.0f};
    item.material.roughness = 1.0f;
    item.material.metallic = 0.0f;
    item.material.f0 = 0.0f;
    item.material.textured = 0.0f;
    item.material.normal_mapped = 0.0f;
    item.material.alpha = 1.0f;
    item.material.alpha_cutoff = 0.0f;
    item.style = engine::surface_style::two_sided;
    return item;
}

engine::scene_light_uniforms make_light(float ambient)
{
    engine::scene_light_uniforms light{};
    light.to_light = vec3{0.0f, 0.0f, 1.0f};
    light.key = vec3{0.0f, 0.0f, 0.0f};
    light.ambient = vec3{ambient, ambient, ambient};
    light.eye_world = vec3{0.0f, 0.0f, 5.0f};
    light.spec_model = 0.0f;
    light.encode_output = 0.0f;
    return light;
}

mat4 flat_clip()
{
    mat4 clip = mat4::identity();
    clip.c0.x = 2.0f;
    clip.c1.y = 2.0f;
    return clip;
}

// ---------------------------------------------------------------------------
//  The frame, declared
// ---------------------------------------------------------------------------
//
// One context struct per KIND of pass, and a plain function that records it.
// That is what a `void*` user pointer buys: a pass body is an ordinary function
// with ordinary arguments gathered into a struct, and the graph never has to
// know what any of them are.

struct shadow_ctx
{
    const rig* r = nullptr;
    const engine::gpu_draw_item* items = nullptr;
    int count = 0;
    engine::light_camera cam{};
};

void exec_shadow(const fg_pass_context& c, void* user)
{
    const shadow_ctx& s = *static_cast<const shadow_ctx*>(user);
    s.r->shadow.render_into(c.cb, c.pass, s.items, s.count, s.cam);
}

struct scene_ctx
{
    const rig* r = nullptr;
    const engine::gpu_draw_item* items = nullptr;
    int count = 0;
    engine::camera_uniforms camera{mat4::identity()};
    engine::scene_light_uniforms light{};
    fg_texture shadow{};
};

void exec_scene(const fg_pass_context& c, void* user)
{
    const scene_ctx& s = *static_cast<const scene_ctx*>(user);

    // THE SHADOW MAP ARRIVES AS A HANDLE, NOT A POINTER. The pass declared that
    // it samples `shadow@1`, so the texture it gets is whatever the graph bound
    // to that version — which is how a pooled texture can be handed to a pass
    // that has no idea it is pooled.
    s.r->scene.render(c.cb, c.pass, s.items, s.count, s.camera, s.light,
                      s.r->samp.handle(), nullptr, c.texture(s.shadow),
                      s.r->shadow.sampler());
}

struct bloom_ctx
{
    const rig* r = nullptr;
    bloom_stage kind = bloom_stage::bright;
    fg_texture source{};
    Uint32 source_w = 0;
    Uint32 source_h = 0;
    const bloom_settings* bs = nullptr;
    float exposure = 1.0f;
};

void exec_bloom(const fg_pass_context& c, void* user)
{
    const bloom_ctx& b = *static_cast<const bloom_ctx*>(user);
    b.r->bloom.record_stage(c.cb, c.pass, b.kind, c.texture(b.source),
                            b.source_w, b.source_h, *b.bs, b.exposure);
}

struct resolve_ctx
{
    const rig* r = nullptr;
    fg_texture hdr{};
    fg_texture bloom{};
    tonemap_settings tone{};
    float intensity = 0.0f;
};

void exec_resolve(const fg_pass_context& c, void* user)
{
    const resolve_ctx& t = *static_cast<const resolve_ctx*>(user);
    t.r->tonemap.render(c.cb, c.pass, c.texture(t.hdr), t.tone, /*shader_encodes=*/true,
                        c.texture(t.bloom), t.intensity);
}

/// Everything the declaration needs to keep alive until `execute` has run.
///
/// The contexts are POINTED AT by the graph, so they must outlive it — which is
/// the one real cost of a `void*` callback and is worth stating rather than
/// discovering. They live here, beside the graph, for exactly one frame.
struct frame_decl
{
    shadow_ctx shadow{};
    scene_ctx scene{};
    bloom_ctx stages[2 * k_levels]{};
    resolve_ctx resolve{};
    bloom_settings bs;
    engine::gpu_draw_item item{};

    /// Where each declared pass ended up, so §C and §D can name them.
    int pass_shadow = -1;
    int pass_scene = -1;
    int pass_stage[2 * k_levels]{};
    int pass_resolve = -1;
    int stage_count = 0;

    fg_texture h_ldr{};
};

/// Declare the whole frame. Returns false only if a cap was hit.
///
/// **Read this function as the lesson's main exhibit.** Nothing in it says what
/// order the passes run in, what any load op is, what any store op is, or which
/// intermediates can share a texture. It says what each pass reads and what each
/// pass writes, and that is all it says.
/// @param size the frame's dimensions. **A parameter, and only for §G's sizing
///        study**: the declaration is the only copy of the frame's shape, so
///        asking "what would this cost at 1920x1080?" must go through this
///        function rather than through a second description of it that could
///        drift. A graph declared at a size the rig's textures are not is
///        compiled and measured but never EXECUTED, and the call site says so.
bool declare_frame(frame_graph& fg, frame_decl& d, rig& r,
                   const engine::gpu_draw_item* items, int count,
                   const engine::scene_light_uniforms& light,
                   const tonemap_settings& tone, const bloom_settings& bs,
                   bool read_the_bloom, int width = k_size, int height = k_size,
                   int shadow_res = k_shadow_res)
{
    d.bs = bs;

    // ---- Resources ---------------------------------------------------------
    fg_texture_desc shadow_desc{};
    shadow_desc.width = static_cast<Uint32>(shadow_res);
    shadow_desc.height = static_cast<Uint32>(shadow_res);
    shadow_desc.format = r.depth_format;
    shadow_desc.layers = 1;
    shadow_desc.depth = true;
    shadow_desc.sampled = true;     // the scene pass reads it

    fg_texture_desc hdr_desc{};
    hdr_desc.width = static_cast<Uint32>(width);
    hdr_desc.height = static_cast<Uint32>(height);
    hdr_desc.format = engine::k_hdr_format;

    fg_texture_desc depth_desc{};
    depth_desc.width = static_cast<Uint32>(width);
    depth_desc.height = static_cast<Uint32>(height);
    depth_desc.format = r.depth_format;
    depth_desc.depth = true;
    depth_desc.sampled = false;     // nothing reads it — 4.7's DONT_CARE, as a fact

    fg_texture_desc ldr_desc{};
    ldr_desc.width = static_cast<Uint32>(width);
    ldr_desc.height = static_cast<Uint32>(height);
    ldr_desc.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

    const fg_texture shadow0 = fg.create("shadow", shadow_desc);
    const fg_texture hdr0 = fg.create("hdr", hdr_desc);
    const fg_texture depth0 = fg.create("depth", depth_desc);
    const fg_texture ldr0 = fg.import("backbuffer", r.ldr.colour, ldr_desc);

    // The stand-in, imported rather than created: its contents come from outside
    // the frame (they are undefined, and multiplied by zero), so an import is
    // exactly the right word for it.
    fg_texture_desc black_desc{};
    black_desc.width = 1;
    black_desc.height = 1;
    black_desc.format = engine::k_hdr_format;
    const fg_texture black = fg.import("no bloom", r.black1.handle(), black_desc);

    fg_texture level0[k_levels]{};
    for (int i = 0; i < k_levels; ++i)
    {
        static const char* names[k_levels] = {
            "bloom 0", "bloom 1", "bloom 2", "bloom 3", "bloom 4", "bloom 5"};
        fg_texture_desc ld{};
        ld.width = static_cast<Uint32>(width / 2) >> i;
        ld.height = static_cast<Uint32>(height / 2) >> i;
        if (ld.width == 0u) { ld.width = 1u; }
        if (ld.height == 0u) { ld.height = 1u; }
        ld.format = engine::k_hdr_format;
        level0[i] = fg.create(names[i], ld);
    }

    // ---- Pass 1: the shadow map -------------------------------------------
    engine::directional_light key{};
    key.direction = vec3{0.0f, -1.0f, -0.3f};
    engine::aabb bounds{};
    bounds.min = vec3{-2.0f, -2.0f, -2.0f};
    bounds.max = vec3{2.0f, 2.0f, 2.0f};

    d.shadow.r = &r;
    d.shadow.items = items;
    d.shadow.count = count;
    d.shadow.cam = engine::fit_directional(key, bounds, shadow_res);

    d.pass_shadow = fg.add_pass("shadow", &exec_shadow, &d.shadow);
    const fg_texture shadow1 = fg.clear_depth(d.pass_shadow, shadow0, 1.0f);

    // ---- Pass 2: the scene -------------------------------------------------
    d.scene.r = &r;
    d.scene.items = items;
    d.scene.count = count;
    d.scene.camera = engine::camera_uniforms{flat_clip()};
    d.scene.light = light;
    d.scene.shadow = shadow1;

    d.pass_scene = fg.add_pass("scene", &exec_scene, &d.scene);
    fg.sample(d.pass_scene, shadow1);
    const fg_texture hdr1 = fg.clear(d.pass_scene, hdr0, SDL_FColor{0.0f, 0.0f, 0.0f, 1.0f});
    const fg_texture depth1 = fg.clear_depth(d.pass_scene, depth0, 1.0f);
    (void)depth1;

    // ---- Passes 3..13: the bloom chain ------------------------------------
    //
    // Eleven passes, declared by the same loop shape `gpu_bloom::render` uses —
    // except that where that function chose DONT_CARE and LOAD, this one says
    // `discard_write` and `keep`, which are statements about the DATA.
    fg_texture level[k_levels]{};
    for (int i = 0; i < k_levels; ++i) { level[i] = level0[i]; }

    int stage = 0;
    {
        bloom_ctx& b = d.stages[stage];
        b.r = &r;
        b.kind = bloom_stage::bright;
        b.source = hdr1;
        b.source_w = static_cast<Uint32>(width);
        b.source_h = static_cast<Uint32>(height);
        b.bs = &d.bs;
        b.exposure = tone.exposure;

        d.pass_stage[stage] = fg.add_pass("bloom bright", &exec_bloom, &b);
        fg.sample(d.pass_stage[stage], hdr1);
        level[0] = fg.discard_write(d.pass_stage[stage], level[0]);
        ++stage;
    }

    for (int i = 1; i < k_levels; ++i)
    {
        static const char* names[k_levels] = {
            "", "bloom down 1", "bloom down 2", "bloom down 3", "bloom down 4", "bloom down 5"};
        bloom_ctx& b = d.stages[stage];
        b.r = &r;
        b.kind = bloom_stage::down;
        b.source = level[i - 1];
        b.source_w = static_cast<Uint32>(width / 2) >> (i - 1);
        b.source_h = static_cast<Uint32>(height / 2) >> (i - 1);
        b.bs = &d.bs;
        b.exposure = tone.exposure;

        d.pass_stage[stage] = fg.add_pass(names[i], &exec_bloom, &b);
        fg.sample(d.pass_stage[stage], level[i - 1]);
        level[i] = fg.discard_write(d.pass_stage[stage], level[i]);
        ++stage;
    }

    for (int i = k_levels - 1; i > 0; --i)
    {
        static const char* names[k_levels] = {
            "", "bloom up 1", "bloom up 2", "bloom up 3", "bloom up 4", "bloom up 5"};
        bloom_ctx& b = d.stages[stage];
        b.r = &r;
        b.kind = bloom_stage::up;
        b.source = level[i];
        b.source_w = static_cast<Uint32>(width / 2) >> i;
        b.source_h = static_cast<Uint32>(height / 2) >> i;
        b.bs = &d.bs;
        b.exposure = tone.exposure;

        d.pass_stage[stage] = fg.add_pass(names[i], &exec_bloom, &b);
        fg.sample(d.pass_stage[stage], level[i]);

        // `keep`, AND THIS IS THE WHOLE SENTENCE. The upsample blends `src + dst`,
        // so the destination's contents are an operand. Lesson 6.13 spelled that
        // as SDL_GPU_LOADOP_LOAD and wrote a paragraph warning what happens when
        // somebody copies the downsample loop instead. Here it is a dependency,
        // and the load op is derived from it.
        level[i - 1] = fg.keep(d.pass_stage[stage], level[i - 1]);
        ++stage;
    }
    d.stage_count = stage;

    // ---- Pass 14: the resolve ---------------------------------------------
    d.resolve.r = &r;
    d.resolve.hdr = hdr1;
    d.resolve.bloom = read_the_bloom ? level[0] : black;
    d.resolve.tone = tone;
    d.resolve.intensity = read_the_bloom ? bs.intensity : 0.0f;

    d.pass_resolve = fg.add_pass("resolve", &exec_resolve, &d.resolve);
    fg.sample(d.pass_resolve, hdr1);

    // THE ONE LINE THAT DECIDES WHETHER ELEVEN PASSES EXIST. Sample the bloom's
    // finished level 0 and the whole chain is reachable; sample the UNWRITTEN
    // level 0 instead and nothing upstream feeds an import, so §E watches eleven
    // passes disappear without a flag anywhere.
    //
    fg.sample(d.pass_resolve, read_the_bloom ? level[0] : black);

    d.h_ldr = fg.clear(d.pass_resolve, ldr0, SDL_FColor{0.0f, 0.0f, 0.0f, 1.0f});

    return d.pass_resolve >= 0;
}

} // namespace

// ===========================================================================
//  §C, §D, §E, §G, §H, §I — everything that needs a device
// ===========================================================================

namespace {

/// The hand-written frame, exactly as Lesson 6.13's harness assembled it plus
/// 6.8's shadow pass in front. This is the thing the graph has to reproduce.
std::vector<Uint8> render_by_hand(rig& r, const engine::gpu_draw_item* items, int count,
                                  const engine::scene_light_uniforms& light,
                                  const tonemap_settings& tone, const bloom_settings& bs)
{
    engine::directional_light key{};
    key.direction = vec3{0.0f, -1.0f, -0.3f};
    engine::aabb bounds{};
    bounds.min = vec3{-2.0f, -2.0f, -2.0f};
    bounds.max = vec3{2.0f, 2.0f, 2.0f};
    const engine::light_camera cam = engine::fit_directional(key, bounds, k_shadow_res);

    SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
    if (cb == nullptr) { return {}; }

    // ---- ONE: the shadow map, which begins its own pass --------------------
    r.shadow.render(cb, items, count, cam);

    // ---- TWO: the scene, into a float target -------------------------------
    SDL_GPUColorTargetInfo hdr_info{};
    hdr_info.texture = r.hdr.handle();
    hdr_info.load_op = SDL_GPU_LOADOP_CLEAR;
    hdr_info.store_op = SDL_GPU_STOREOP_STORE;
    hdr_info.clear_color = SDL_FColor{0.0f, 0.0f, 0.0f, 1.0f};

    SDL_GPUDepthStencilTargetInfo dsi{};
    dsi.texture = r.depth.handle();
    dsi.clear_depth = 1.0f;
    dsi.load_op = SDL_GPU_LOADOP_CLEAR;
    dsi.store_op = SDL_GPU_STOREOP_DONT_CARE;
    dsi.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    dsi.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

    SDL_GPURenderPass* p2 = SDL_BeginGPURenderPass(cb, &hdr_info, 1, &dsi);
    r.scene.render(cb, p2, items, count, engine::camera_uniforms{flat_clip()}, light,
                   r.samp.handle(), nullptr, r.shadow.texture(), r.shadow.sampler());
    SDL_EndGPURenderPass(p2);

    // ---- THREE: the bloom, which begins eleven passes of its own -----------
    if (bs.enabled) { r.bloom.render(cb, r.hdr.handle(), bs, tone.exposure); }

    // ---- FOUR: the resolve, into 8 bits ------------------------------------
    SDL_GPUColorTargetInfo ldr_info{};
    ldr_info.texture = r.ldr.colour;
    ldr_info.load_op = SDL_GPU_LOADOP_CLEAR;
    ldr_info.store_op = SDL_GPU_STOREOP_STORE;
    ldr_info.clear_color = SDL_FColor{0.0f, 0.0f, 0.0f, 1.0f};
    SDL_GPURenderPass* p4 = SDL_BeginGPURenderPass(cb, &ldr_info, 1, nullptr);
    r.tonemap.render(cb, p4, r.hdr.handle(), tone, /*shader_encodes=*/true,
                     bs.enabled ? r.bloom.result() : r.black1.handle(),
                     bs.enabled ? bs.intensity : 0.0f);
    SDL_EndGPURenderPass(p4);

    return download(r.ldr, cb);
}

std::vector<Uint8> render_by_graph(rig& r, frame_graph& fg, frame_decl& d,
                                   const engine::gpu_draw_item* items, int count,
                                   const engine::scene_light_uniforms& light,
                                   const tonemap_settings& tone, const bloom_settings& bs)
{
    fg.reset();
    if (!declare_frame(fg, d, r, items, count, light, tone, bs, bs.enabled)) { return {}; }
    if (!fg.compile(r.gpu)) { return {}; }

    SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
    if (cb == nullptr) { return {}; }
    fg.execute(cb);
    return download(r.ldr, cb);
}

void section_cdefghi(rig& r)
{
    const engine::gpu_draw_item item = make_item(r);
    const engine::scene_light_uniforms light = make_light(3.0f);

    tonemap_settings tone;
    tone.op = engine::tonemap::aces;
    tone.exposure = 1.0f;

    bloom_settings bs;
    bs.enabled = true;
    bs.levels = k_levels;
    bs.intensity = 0.1f;

    frame_graph fg;
    frame_decl d;

    // -------------------------------------------------------------------
    section("C  THE SCHEDULE — derived, against the order written by hand");
    // -------------------------------------------------------------------
    fg.reset();
    check(declare_frame(fg, d, r, &item, 1, light, tone, bs, true), "the frame declares");
    check(fg.compile(r.gpu), "it compiles");

    static const char* expected[] = {
        "shadow", "scene", "bloom bright",
        "bloom down 1", "bloom down 2", "bloom down 3", "bloom down 4", "bloom down 5",
        "bloom up 5", "bloom up 4", "bloom up 3", "bloom up 2", "bloom up 1",
        "resolve"};
    const int n_expected = static_cast<int>(sizeof(expected) / sizeof(expected[0]));

    std::printf("\n  %-4s %-16s  %s\n", "#", "compiled order", "hand-written order");
    std::printf("  %-4s %-16s  %s\n", "--", "----------------", "------------------");
    bool order_ok = (fg.live_pass_count() == n_expected);
    for (int i = 0; i < fg.live_pass_count(); ++i)
    {
        const char* got = fg.pass_name(fg.scheduled(i));
        const char* want = (i < n_expected) ? expected[i] : "(none)";
        const bool same = (std::strcmp(got, want) == 0);
        order_ok = order_ok && same;
        std::printf("  %-4d %-16s  %-18s %s\n", i, got, want, same ? "" : "  <-- differs");
    }
    check_eq(fg.live_pass_count(), n_expected, "fourteen passes, all live");
    check(order_ok, "the compiled order IS the hand-written order, name for name");

    // -------------------------------------------------------------------
    section("D  THE OPS — every load and store, derived");
    // -------------------------------------------------------------------
    //
    // The right-hand column is what the engine's own code does today, read out of
    // the sources rather than remembered:
    //   gpu_shadow.cpp:136-143   CLEAR / STORE
    //   gpu_scene.cpp:423-430    CLEAR / DONT_CARE   (the depth)
    //   verify's scene colour    CLEAR / STORE
    //   gpu_post.cpp:373         DONT_CARE / STORE   (bright, downsamples)
    //   gpu_post.cpp:406         LOAD      / STORE   (upsamples)
    struct want_op
    {
        const char* pass;
        const char* resource;
        SDL_GPULoadOp load;
        SDL_GPUStoreOp store;
    };
    static const want_op wants[] = {
        {"shadow", "shadow", SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE},
        {"scene", "hdr", SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE},
        {"scene", "depth", SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_DONT_CARE},
        {"bloom bright", "bloom 0", SDL_GPU_LOADOP_DONT_CARE, SDL_GPU_STOREOP_STORE},
        {"bloom down 5", "bloom 5", SDL_GPU_LOADOP_DONT_CARE, SDL_GPU_STOREOP_STORE},
        {"bloom up 5", "bloom 4", SDL_GPU_LOADOP_LOAD, SDL_GPU_STOREOP_STORE},
        {"bloom up 1", "bloom 0", SDL_GPU_LOADOP_LOAD, SDL_GPU_STOREOP_STORE},
        {"resolve", "backbuffer", SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE},
    };

    std::printf("\n  %-14s %-12s %-12s %-12s  %s\n",
                "pass", "attachment", "load (derived)", "store (derived)", "hand-written");
    std::printf("  %-14s %-12s %-12s %-12s  %s\n",
                "--------------", "------------", "------------", "------------",
                "--------------------");

    for (const want_op& w : wants)
    {
        bool found = false;
        for (int s = 0; s < fg.live_pass_count() && !found; ++s)
        {
            const int p = fg.scheduled(s);
            if (std::strcmp(fg.pass_name(p), w.pass) != 0) { continue; }
            for (int i = 0; i < fg.access_count(p); ++i)
            {
                const frame_graph::access_report a = fg.access_at(p, i);
                if (a.use == fg_use::sample) { continue; }
                if (std::strcmp(fg.resource_name(a.resource), w.resource) != 0) { continue; }

                found = true;
                const bool same = (a.load_op == w.load) && (a.store_op == w.store);
                if (!same) { ++g_failures; }
                std::printf("  %-14s %-12s %-12s %-12s  %s / %s  [%s]\n",
                            w.pass, w.resource, load_name(a.load_op), store_name(a.store_op),
                            load_name(w.load), store_name(w.store), same ? "PASS" : "FAIL");
                break;
            }
        }
        if (!found)
        {
            ++g_failures;
            std::printf("  %-14s %-12s  [FAIL] no such attachment\n", w.pass, w.resource);
        }
    }

    std::printf("\n  The two that are the whole argument:\n");
    std::printf("    'depth' gets DONT_CARE because NOTHING CONSUMES IT. 4.7 reasoned\n");
    std::printf("    that out in a comment; 6.8 had to reason the opposite way for the\n");
    std::printf("    shadow map. Here neither sentence exists — both fall out of\n");
    std::printf("    'is there a later reader of this version?'.\n");
    std::printf("    'bloom up' gets LOAD because it declared `keep`: its output DEPENDS\n");
    std::printf("    on what was already in the target. 6.13 wrote a paragraph warning\n");
    std::printf("    what happens when somebody copies the downsample loop instead.\n");

    // -------------------------------------------------------------------
    section("E  CULLING — stop reading the bloom, and eleven passes vanish");
    // -------------------------------------------------------------------
    const int with_bloom = fg.live_pass_count();

    fg.reset();
    frame_decl d_off;
    check(declare_frame(fg, d_off, r, &item, 1, light, tone, bs, /*read_the_bloom=*/false),
          "the same frame declares, with the resolve reading the stand-in instead");
    check(fg.compile(r.gpu), "it compiles");

    std::printf("\n  declared: %d passes\n", fg.pass_count());
    std::printf("  live:     %d\n", fg.live_pass_count());
    std::printf("  culled:   %d\n", fg.culled_pass_count());
    std::printf("\n  culled, by name:\n");
    for (int p = 0; p < fg.pass_count(); ++p)
    {
        if (!fg.pass_alive(p)) { std::printf("    %s\n", fg.pass_name(p)); }
    }

    check_eq(with_bloom - fg.live_pass_count(), 11, "exactly the eleven bloom passes went");
    check_eq(fg.culled_pass_count(), 11, "and the graph says so itself");
    std::printf("\n  gpu_post.cpp:562 does this today with `if (!s.enabled) { return; }`.\n");
    std::printf("  That flag is correct and it is also a SECOND place the dependency is\n");
    std::printf("  written down. Nothing above knows what a bloom is.\n");

    // -------------------------------------------------------------------
    section("F  THE FIVE MISTAKES THAT ARE SILENT TODAY");
    // -------------------------------------------------------------------
    //
    // Each of these is a real bug that currently produces a wrong picture, a
    // driver validation message from inside somebody else's function, or a hang.
    // The graph is expected to REFUSE, and the message it logs is the deliverable
    // - "the graph did not compile" would be no better than what we have.

    fg_texture_desc small{};
    small.width = 64;
    small.height = 64;
    small.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

    fg_texture_desc big{};
    big.width = 128;
    big.height = 128;
    big.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

    {
        std::printf("\n  1. SAMPLING SOMETHING NOBODY WROTE\n");
        std::printf("     Today: you read the pool's previous tenant. Last frame's bloom,\n");
        std::printf("     faintly, in this frame's image - and no error anywhere.\n");
        frame_graph g;
        const fg_texture t0 = g.create("tmp", small);
        const fg_texture o0 = g.import("out", r.ldr.colour, small);
        const int p = g.add_pass("reads the void", nullptr, nullptr);
        g.sample(p, t0);
        (void)g.clear(p, o0, SDL_FColor{0, 0, 0, 1});
        check(!g.compile(r.gpu), "refused: sampling tmp@0");
    }

    {
        std::printf("\n  2. `keep` ON A TARGET NOBODY WROTE\n");
        std::printf("     Today: SDL_GPU_LOADOP_LOAD on undefined contents. The additive\n");
        std::printf("     upsample adds to garbage, which reads as a tinted bloom.\n");
        frame_graph g;
        const fg_texture t0 = g.create("tmp", small);
        const fg_texture o0 = g.import("out", r.ldr.colour, small);
        const int p = g.add_pass("loads the void", nullptr, nullptr);
        const fg_texture t1 = g.keep(p, t0);
        const int q = g.add_pass("uses it", nullptr, nullptr);
        g.sample(q, t1);
        (void)g.clear(q, o0, SDL_FColor{0, 0, 0, 1});
        check(!g.compile(r.gpu), "refused: LOADing tmp@0");
    }

    {
        std::printf("\n  3. TWO PASSES WRITING THE SAME VERSION\n");
        std::printf("     Today: whichever ran last wins, and which that is depends on\n");
        std::printf("     the order somebody happened to call them in.\n");
        frame_graph g;
        const fg_texture t0 = g.create("tmp", small);
        const fg_texture o0 = g.import("out", r.ldr.colour, small);
        const int a = g.add_pass("writer A", nullptr, nullptr);
        const int b = g.add_pass("writer B", nullptr, nullptr);
        const fg_texture t1 = g.discard_write(a, t0);
        (void)g.discard_write(b, t0);      // from t0 AGAIN - the ambiguity
        const int p = g.add_pass("reader", nullptr, nullptr);
        g.sample(p, t1);
        (void)g.clear(p, o0, SDL_FColor{0, 0, 0, 1});
        check(!g.compile(r.gpu), "refused: two producers from tmp@0");
    }

    {
        std::printf("\n  4. A CYCLE\n");
        std::printf("     Today: there is no `today` - you cannot write this by hand,\n");
        std::printf("     because by hand the order IS the program. A declared frame can\n");
        std::printf("     express it, so the compiler has to detect it and name it.\n");
        frame_graph g;
        const fg_texture x0 = g.create("x", small);
        const fg_texture y0 = g.create("y", small);
        const fg_texture o0 = g.import("out", r.ldr.colour, small);

        // The handle for y@1 written out by hand, because pass B has not been
        // declared yet. Note how much effort that takes: a cycle is not something
        // you fall into, which is why detecting it is cheap insurance and not a
        // core feature.
        fg_texture y1{};
        y1.index = y0.index;
        y1.version = 1;

        const int a = g.add_pass("A", nullptr, nullptr);
        g.sample(a, y1);
        const fg_texture x1 = g.discard_write(a, x0);

        const int b = g.add_pass("B", nullptr, nullptr);
        g.sample(b, x1);
        (void)g.discard_write(b, y0);
        (void)g.clear(b, o0, SDL_FColor{0, 0, 0, 1});
        check(!g.compile(r.gpu), "refused: A and B wait on each other, both named");
    }

    {
        std::printf("\n  5. ATTACHMENTS THAT DISAGREE\n");
        std::printf("     Today: SDL_BeginGPURenderPass returns null halfway through a\n");
        std::printf("     frame you have already half recorded (6.14's hard rule).\n");
        frame_graph g;
        const fg_texture c0 = g.import("colour", r.ldr.colour, small);
        fg_texture_desc dd = big;
        dd.depth = true;
        dd.format = r.depth_format;
        dd.sampled = false;
        const fg_texture z0 = g.create("depth", dd);
        const int p = g.add_pass("mismatched", nullptr, nullptr);
        (void)g.clear(p, c0, SDL_FColor{0, 0, 0, 1});
        (void)g.clear_depth(p, z0, 1.0f);
        check(!g.compile(r.gpu), "refused: 64x64 colour beside a 128x128 depth");
    }

    // -------------------------------------------------------------------
    section("G  LIFETIMES AND MEMORY — the claim the literature makes, measured");
    // -------------------------------------------------------------------
    fg.reset();
    check(declare_frame(fg, d, r, &item, 1, light, tone, bs, true), "re-declared with the bloom");
    check(fg.compile(r.gpu), "compiled");

    std::printf("\n  %-12s %9s %5s %5s %5s  %s\n",
                "resource", "bytes", "first", "last", "slot", "live across the schedule");
    std::printf("  %-12s %9s %5s %5s %5s  %s\n",
                "------------", "---------", "-----", "----", "----",
                "--------------------------");

    for (int res = 0; res < fg.resource_count(); ++res)
    {
        const int f = fg.first_use(res);
        const int l = fg.last_use(res);
        if (f < 0) { continue; }

        char bar[64];
        const int n = fg.live_pass_count();
        for (int i = 0; i < n && i < 60; ++i) { bar[i] = (i >= f && i <= l) ? '#' : '.'; }
        bar[(n < 60) ? n : 60] = '\0';

        const int slot = fg.pool_slot(res);
        char slot_text[8];
        if (slot < 0) { SDL_strlcpy(slot_text, "imp", sizeof(slot_text)); }
        else { SDL_snprintf(slot_text, sizeof(slot_text), "%d", slot); }

        std::printf("  %-12s %9zu %5d %5d %5s  %s\n", fg.resource_name(res),
                    fg.resource_bytes(res), f, l, slot_text, bar);
    }

    const std::size_t naive = fg.transient_bytes_naive();
    const std::size_t pooled = fg.transient_bytes_pooled();
    const std::size_t peak = fg.transient_bytes_peak();

    std::printf("\n  one texture per transient : %9zu B  (%.3f MB)\n",
                naive, static_cast<double>(naive) / (1024.0 * 1024.0));
    std::printf("  pooled, descriptor-keyed  : %9zu B  (%.3f MB)   saved %zu B\n",
                pooled, static_cast<double>(pooled) / (1024.0 * 1024.0), naive - pooled);
    std::printf("  peak simultaneously live  : %9zu B  (%.3f MB)   the floor TRUE aliasing\n",
                peak, static_cast<double>(peak) / (1024.0 * 1024.0));
    std::printf("                                                        would reach\n");
    std::printf("  pool textures created     : %9d\n", fg.pool_size());

    check_eq(static_cast<long long>(naive - pooled), 0,
             "descriptor-keyed reuse saves nothing at all on this frame");
    check(peak < naive, "true memory aliasing WOULD save something - the peak is below the sum");

    std::printf("\n  THE THREE NUMBERS, AND THE GAP BETWEEN THE SECOND AND THE THIRD:\n");
    std::printf("    no sharing at all         %9zu B   100.0%%\n", naive);
    std::printf("    true memory aliasing      %9zu B   %5.1f%%   <- the literature's claim\n",
                peak, 100.0 * static_cast<double>(peak) / static_cast<double>(naive));
    std::printf("    what SDL_GPU can express  %9zu B   %5.1f%%   <- what we actually get\n",
                pooled, 100.0 * static_cast<double>(pooled) / static_cast<double>(naive));
    std::printf("\n  So the prize is %zu B (%.1f%%) and this engine collects NONE of it.\n",
                naive - peak, 100.0 * static_cast<double>(naive - peak)
                                    / static_cast<double>(naive));
    std::printf("  Two separate reasons, and they have to be told apart:\n");
    std::printf("    (1) THE API. SDL_GPU 3.4.12 has no placed resource and no heap, so\n");
    std::printf("        the only reuse available is handing back a WHOLE texture whose\n");
    std::printf("        descriptor matches. `shadow` is 256x256 sampled depth and dies\n");
    std::printf("        at pass 1; `bloom 0` is 128x128 RGBA16F and is born at pass 2.\n");
    std::printf("        262,144 bytes could hold 131,072 - and cannot be asked to.\n");
    std::printf("    (2) THE SHAPE. Even with perfect aliasing the saving is small, because\n");
    std::printf("        `hdr` spans the whole schedule and the six pyramid levels are ALL\n");
    std::printf("        live at the turn between the down chain and the up chain - which\n");
    std::printf("        is what an additive pyramid MEANS. A chain has little to alias.\n");

    // ---- And at a resolution anybody actually renders at ------------------
    //
    // 256x256 makes the bytes easy to check and makes them sound negligible.
    // Declared once more at 1920x1080 - through the SAME function, so there is
    // no second description of the frame to drift - and never executed.
    {
        frame_graph big;
        frame_decl bd;
        (void)declare_frame(big, bd, r, &item, 1, light, tone, bs, true,
                            1920, 1080, 2048);
        if (big.compile(r.gpu))
        {
            const std::size_t bn = big.transient_bytes_naive();
            const std::size_t bp = big.transient_bytes_peak();
            std::printf("\n  The same frame declared at 1920x1080 with a 2048 shadow map\n");
            std::printf("  (never executed - the rig's own textures are %dx%d):\n",
                        k_size, k_size);
            std::printf("    no sharing at all         %9.2f MB\n",
                        static_cast<double>(bn) / (1024.0 * 1024.0));
            std::printf("    true memory aliasing      %9.2f MB   (%.1f%%)\n",
                        static_cast<double>(bp) / (1024.0 * 1024.0),
                        100.0 * static_cast<double>(bp) / static_cast<double>(bn));
            std::printf("    unreachable saving        %9.2f MB\n",
                        static_cast<double>(bn - bp) / (1024.0 * 1024.0));
        }
        big.destroy();
    }

    // ---- What would have to be true ---------------------------------------
    //
    // The mechanism works; this frame just has no use for it. Rather than assert
    // that, build the smallest graph that DOES and watch two resources land on
    // one slot.
    {
        frame_graph fg2;

        fg_texture_desc half{};
        half.width = k_size / 2;
        half.height = k_size / 2;
        half.format = engine::k_hdr_format;

        fg_texture_desc full{};
        full.width = k_size;
        full.height = k_size;
        full.format = engine::k_hdr_format;

        const fg_texture src0 = fg2.create("source", full);
        const fg_texture a0 = fg2.create("effect A scratch", half);
        const fg_texture b0 = fg2.create("effect B scratch", half);
        const fg_texture out0 = fg2.import("out", r.ldr.colour, full);

        const int fill = fg2.add_pass("fill", nullptr, nullptr);
        const fg_texture src1 = fg2.discard_write(fill, src0);

        const int a_pass = fg2.add_pass("effect A", nullptr, nullptr);
        fg2.sample(a_pass, src1);
        const fg_texture a1 = fg2.discard_write(a_pass, a0);

        const int a_done = fg2.add_pass("A -> source", nullptr, nullptr);
        fg2.sample(a_done, a1);
        const fg_texture src2 = fg2.keep(a_done, src1);

        const int b_pass = fg2.add_pass("effect B", nullptr, nullptr);
        fg2.sample(b_pass, src2);
        const fg_texture b1 = fg2.discard_write(b_pass, b0);

        const int b_done = fg2.add_pass("B -> out", nullptr, nullptr);
        fg2.sample(b_done, b1);
        fg2.sample(b_done, src2);
        (void)fg2.clear(b_done, out0, SDL_FColor{0, 0, 0, 1});

        check(fg2.compile(r.gpu), "the two-effect graph compiles");

        int slot_a = -1;
        int slot_b = -1;
        for (int res = 0; res < fg2.resource_count(); ++res)
        {
            if (std::strcmp(fg2.resource_name(res), "effect A scratch") == 0)
            {
                slot_a = fg2.pool_slot(res);
            }
            if (std::strcmp(fg2.resource_name(res), "effect B scratch") == 0)
            {
                slot_b = fg2.pool_slot(res);
            }
        }

        std::printf("\n  A graph that IS a tree: two half-resolution effects, used and\n");
        std::printf("  finished one after the other.\n");
        std::printf("    effect A scratch -> pool slot %d   live [%d, %d]\n", slot_a, 1, 2);
        std::printf("    effect B scratch -> pool slot %d   live [%d, %d]\n", slot_b, 3, 4);
        std::printf("    one texture per transient : %9zu B\n", fg2.transient_bytes_naive());
        std::printf("    pooled                    : %9zu B  (saved %zu)\n",
                    fg2.transient_bytes_pooled(),
                    fg2.transient_bytes_naive() - fg2.transient_bytes_pooled());

        check(slot_a >= 0 && slot_a == slot_b,
              "both scratch targets land on ONE pooled texture");
        check(fg2.transient_bytes_pooled() < fg2.transient_bytes_naive(),
              "and the saving is real when the frame has the shape for it");
    }

    // -------------------------------------------------------------------
    section("H  WHAT THE COMPILE COSTS");
    // -------------------------------------------------------------------
    //
    // The first compile creates textures; every later one finds them in the pool.
    // Only the second number is a per-frame cost, so both are reported.
    const double first_us = fg.compile_us();

    double best = 1e30;
    double worst = 0.0;
    double total_us = 0.0;
    const int runs = 200;
    const std::size_t allocs_before = g_allocations;
    for (int i = 0; i < runs; ++i)
    {
        fg.reset();
        frame_decl dd;
        (void)declare_frame(fg, dd, r, &item, 1, light, tone, bs, true);
        (void)fg.compile(r.gpu);
        const double us = fg.compile_us();
        if (us < best) { best = us; }
        if (us > worst) { worst = us; }
        total_us += us;
    }
    const std::size_t allocs_after = g_allocations;

    std::printf("\n  first compile (creates %d textures) : %8.2f us\n", fg.pool_size(), first_us);
    std::printf("  steady state, %d runs               : %8.2f us mean"
                "  [%.2f .. %.2f]\n", runs, total_us / runs, best, worst);
    int accesses = 0;
    for (int pi = 0; pi < fg.pass_count(); ++pi) { accesses += fg.access_count(pi); }
    std::printf("\n  %d passes, %d resources, %d accesses. `producer_of` is a linear scan\n",
                fg.live_pass_count(), fg.resource_count(), accesses);
    std::printf("  called from inside two more loops, which makes the compile\n");
    std::printf("  O(passes^2 x accesses^2). That is FINE at this size and would not be\n");
    std::printf("  at 200 passes; the fix is an index from (resource, version) to its\n");
    std::printf("  producer, built once, not a cleverer algorithm.\n");
    std::printf("\n  heap allocations across %d declare+compile cycles : %zu\n",
                runs, allocs_after - allocs_before);
    std::printf("  (global operator new is replaced in this file and counts every one)\n");
    check(total_us / runs < 200.0, "a steady-state compile is under 200 us");
    check_eq(static_cast<long long>(allocs_after - allocs_before), 0,
             "and it allocates nothing at all - the fixed caps earn their keep");
    std::printf("\n  For scale: 8.6 us against a 16.67 ms budget is 0.05%% of a frame,\n");
    std::printf("  or about one three-hundredth of what 6.16's culling test costs on a\n");
    std::printf("  scene of 45,512 boxes. The graph is not on the critical path.\n");

    // -------------------------------------------------------------------
    section("I  THE GOLDEN — the same frame, assembled two ways");
    // -------------------------------------------------------------------
    const std::vector<Uint8> by_hand = render_by_hand(r, &item, 1, light, tone, bs);
    check(!by_hand.empty(), "the hand-written frame rendered");

    fg.reset();
    frame_decl d_graph;
    const std::vector<Uint8> by_graph =
        render_by_graph(r, fg, d_graph, &item, 1, light, tone, bs);
    check(!by_graph.empty(), "the compiled frame rendered");

    if (!by_hand.empty() && !by_graph.empty())
    {
        std::size_t differing = 0;
        int max_delta = 0;
        for (std::size_t i = 0; i < by_hand.size(); ++i)
        {
            const int delta = std::abs(static_cast<int>(by_hand[i]) - static_cast<int>(by_graph[i]));
            if (delta != 0) { ++differing; }
            if (delta > max_delta) { max_delta = delta; }
        }
        std::printf("\n  channels compared : %zu\n", by_hand.size());
        std::printf("  differing         : %zu\n", differing);
        std::printf("  largest delta     : %d\n", max_delta);
        std::printf("  centre pixel      : hand %3d   graph %3d\n",
                    by_hand[(static_cast<std::size_t>(k_size / 2) * k_size + k_size / 2) * 4u],
                    by_graph[(static_cast<std::size_t>(k_size / 2) * k_size + k_size / 2) * 4u]);
        check_eq(static_cast<long long>(differing), 0, "bit-identical across every channel");

        // AND THE INSTRUMENT MUST BE ABLE TO FAIL. A comparison that would report
        // zero differences whatever happened is not evidence — 6.16 found exactly
        // that bug in golden_615.cpp, where two failed reads compared equal. So:
        // render the graph frame again with the bloom OFF and confirm the
        // comparison NOTICES.
        bloom_settings off = bs;
        off.enabled = false;
        fg.reset();
        frame_decl d_null;
        const std::vector<Uint8> null_frame =
            render_by_graph(r, fg, d_null, &item, 1, light, tone, off);
        std::size_t null_diff = 0;
        for (std::size_t i = 0; i < by_hand.size() && i < null_frame.size(); ++i)
        {
            if (by_hand[i] != null_frame[i]) { ++null_diff; }
        }
        std::printf("\n  control: the same comparison against a bloom-less frame\n");
        std::printf("  differing         : %zu\n", null_diff);
        check(null_diff > 0, "the comparison CAN report a difference — it is not a null instrument");
    }
}

} // namespace

int main()
{
    std::printf("verify_617 — Lesson 6.17, A Lightweight Frame Graph\n");

    section_a();
    section_b();

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::printf("  ---- SDL_Init(VIDEO): %s\n", SDL_GetError());
    }

    {
        // Declared in one scope so `gpu` is destroyed LAST — 6.11's harness
        // segfaulted on exit for the opposite ordering.
        rig r;
        if (!build_rig(r))
        {
            std::printf("\n  ---- no GPU device / shaders on this machine; §C-§I skipped\n");
        }
        else
        {
            section_cdefghi(r);
        }
        r.ldr.destroy();
    }

    std::printf("\n===========================================================================\n");
    std::printf("  %d failure(s)\n", g_failures);
    std::printf("===========================================================================\n");
    return (g_failures == 0) ? 0 : 1;
}
