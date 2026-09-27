// scratch/verify_61.cpp — Lesson 6.1's harness: light, codes, and the two places
// the transfer function is allowed to happen.
//
//   §A  the curve itself: both branches, the join, and the round trip
//   §B  why the curve is shaped like that — the code budget, recomputed
//   §C  WHAT IT COSTS TO WRITE LIGHT WHERE A CODE IS EXPECTED
//   §D  the audit: every conversion in the engine is one of two kinds
//   §E  THE HEADLINE — the same quad into three targets, and only two are right
//   §F  the shader's own encode, against the hardware's
//   §G  the golden is still byte-identical
//
// §C AND §E ARE THE TWO THAT FOUND A REAL BUG. Until this lesson the engine
// claimed its window with SDL's default swapchain composition — SDR, whose
// header definition is "pixel values are in sRGB encoding" — and
// scene.frag.hlsl returned linear light into it. §E renders the identical quad
// into a UNORM target and an _SRGB one and shows that only one of them agrees
// with the CPU renderer, which has been correct since Lesson 1.6.
//
// Build and run:  sh scratch/build_verify_61.sh

#include <engine/core/assert.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/gpu_device.hpp>
#include <engine/gfx/gpu_mesh.hpp>
#include <engine/gfx/gpu_scene.hpp>
#include <engine/gfx/gpu_shader.hpp>
#include <engine/gfx/light.hpp>
#include <engine/gfx/mesh.hpp>
#include <engine/math/mat4.hpp>

#include "../demos/common/demo_scene.hpp"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool ok, const char* what)
{
    ++g_checks;
    if (!ok) { ++g_failures; }
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
}

void checkf(bool ok, const char* fmt, ...)
{
    char line[512];
    va_list args;
    va_start(args, fmt);
    SDL_vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    check(ok, line);
}

[[nodiscard]] std::string read_file(const char* path)
{
    std::size_t size = 0;
    void* data = SDL_LoadFile(path, &size);
    if (data == nullptr) { return {}; }
    std::string out(static_cast<const char*>(data), size);
    SDL_free(data);
    return out;
}

// ===========================================================================
//  §A — THE CURVE
// ===========================================================================

void section_a_curve()
{
    std::printf("\n=== A. The curve ===\n");

    // The two ends are fixed points, and they have to be: black is black and
    // white is white in any encoding worth the name.
    check(engine::srgb_to_linear(0.0f) == 0.0f && engine::linear_to_srgb(0.0f) == 0.0f,
          "0 maps to 0 in both directions");
    checkf(std::fabs(engine::linear_to_srgb(1.0f) - 1.0f) < 1e-6f
               && std::fabs(engine::srgb_to_linear(1.0f) - 1.0f) < 1e-6f,
           "1 maps to 1 in both directions (encode gives %.7f)",
           static_cast<double>(engine::linear_to_srgb(1.0f)));

    // THE JOIN. The curve is piecewise, and the two pieces are chosen to meet:
    // below linear 0.0031308 it is a straight line of slope 12.92, above it a
    // shifted power. A "gamma 2.2" approximation has no toe at all, and that is
    // exactly where this subject does its damage.
    constexpr float k_join_linear = 0.0031308f;
    const float below = engine::linear_to_srgb(k_join_linear * 0.999f);
    const float above = engine::linear_to_srgb(k_join_linear * 1.001f);
    checkf(std::fabs(above - below) < 1e-4f,
           "the two branches MEET at linear %.7f: %.6f vs %.6f, a step of %.2e",
           static_cast<double>(k_join_linear), static_cast<double>(below),
           static_cast<double>(above), static_cast<double>(std::fabs(above - below)));

    const float toe_slope = engine::linear_to_srgb(0.001f) / 0.001f;
    checkf(std::fabs(toe_slope - 12.92f) < 0.01f,
           "the toe is a straight line of slope %.4f — twelve times steeper than a "
           "gamma curve near zero, which is why one linear step crosses twelve codes there",
           static_cast<double>(toe_slope));

    // THE ROUND TRIP MUST BE LOSSLESS FOR ALL 256 CODES, or every decode/encode
    // pair in the engine slowly grinds an image down.
    int worst = 0;
    for (int i = 0; i < 256; ++i)
    {
        const Uint8 code = static_cast<Uint8>(i);
        const int back = engine::linear_to_srgb_u8(engine::srgb_to_linear_u8(code));
        worst = std::max(worst, std::abs(back - i));
    }
    checkf(worst == 0, "decode -> encode is LOSSLESS for all 256 codes (worst error %d)", worst);

    // AND THE APPROXIMATION EVERYBODY REACHES FOR. pow(x, 1/2.2) is not the sRGB
    // curve; it is a different curve that happens to be close in the middle.
    int worst_code = 0;
    float worst_at = 0.0f;
    for (int i = 0; i <= 1000; ++i)
    {
        const float lin = static_cast<float>(i) / 1000.0f;
        const int exact = engine::linear_to_srgb_u8(lin);
        const int gamma = static_cast<int>(std::lround(
            255.0f * std::pow(lin, 1.0f / 2.2f)));
        if (std::abs(exact - gamma) > worst_code)
        {
            worst_code = std::abs(exact - gamma);
            worst_at = lin;
        }
    }
    checkf(worst_code >= 4,
           "pow(x, 1/2.2) is NOT the sRGB curve: worst disagreement %d codes, at linear "
           "%.4f. Close in the midtones, wrong where the toe is",
           worst_code, static_cast<double>(worst_at));

    // The fast path from Lesson 3.10 must still round-trip, or it is not a
    // drop-in for the exact one.
    int fast_worst = 0;
    for (int i = 0; i < 256; ++i)
    {
        const float lin = engine::srgb_to_linear_u8(static_cast<Uint8>(i));
        fast_worst = std::max(fast_worst,
                              std::abs(engine::linear_to_srgb_u8_fast(lin) - i));
    }
    checkf(fast_worst == 0,
           "the fitted fast encode also round-trips all 256 codes exactly (worst %d) — "
           "which is what makes encode_mode a speed knob rather than a correctness one",
           fast_worst);
}

// ===========================================================================
//  §B — THE CODE BUDGET
// ===========================================================================

void section_b_budget()
{
    std::printf("\n=== B. Why the curve is shaped like that ===\n");

    // 256 codes have to cover the whole range. Spend them evenly in LIGHT and
    // the darks get almost none — which matters because the eye's discrimination
    // is roughly proportional to the ratio between neighbouring intensities, not
    // their difference.
    int even_dark = 0;
    int srgb_dark = 0;
    for (int i = 0; i < 256; ++i)
    {
        const float even = static_cast<float>(i) / 255.0f;              // light, evenly spaced
        const float srgb = engine::srgb_to_linear_u8(static_cast<Uint8>(i));
        if (even <= 0.1f) { ++even_dark; }
        if (srgb <= 0.1f) { ++srgb_dark; }
    }
    checkf(even_dark == 26 && srgb_dark == 90,
           "of 256 codes, the darkest tenth of the LIGHT range gets %d under even spacing "
           "and %d under sRGB — the curve is a budget, and it spends where the eye looks",
           even_dark, srgb_dark);

    int even_bright = 0;
    int srgb_bright = 0;
    for (int i = 0; i < 256; ++i)
    {
        const float even = static_cast<float>(i) / 255.0f;
        const float srgb = engine::srgb_to_linear_u8(static_cast<Uint8>(i));
        if (even >= 0.5f) { ++even_bright; }
        if (srgb >= 0.5f) { ++srgb_bright; }
    }
    checkf(even_bright == 128 && srgb_bright == 68,
           "…and the brightest HALF of the light range, where the eye can barely tell two "
           "steps apart, takes %d codes evenly and only %d under sRGB",
           even_bright, srgb_bright);

    // The single number that makes the whole subject concrete.
    const float half_code = engine::srgb_to_linear_u8(128);
    const int half_light = engine::linear_to_srgb_u8(0.5f);
    checkf(std::fabs(half_code - 0.2159f) < 0.001f && half_light == 188,
           "code 128 is NOT half the light: it emits %.4f. Half the light is stored as "
           "code %d",
           static_cast<double>(half_code), half_light);
}

// ===========================================================================
//  §C — WRITING LIGHT WHERE A CODE IS EXPECTED
// ===========================================================================

void section_c_cost()
{
    std::printf("\n=== C. The cost of writing light into an SDR swapchain ===\n");

    // This is the bug the lesson found, expressed as arithmetic before it is
    // expressed as pixels. Take a linear value, write it raw into an 8-bit
    // buffer whose contents MEAN sRGB codes, and ask what the display emits.
    struct row { float linear; float expect_ratio; };
    const row rows[] = {{0.05f, 12.0f}, {0.20f, 6.0f}, {0.50f, 2.3f}, {0.80f, 1.3f}};

    for (const row& r : rows)
    {
        const int raw = static_cast<int>(r.linear * 255.0f + 0.5f);
        const float shown = engine::srgb_to_linear(static_cast<float>(raw) / 255.0f);
        const float ratio = r.linear / shown;
        checkf(std::fabs(ratio - r.expect_ratio) < 0.5f,
               "linear %.2f written raw shows as %.4f — %.2fx too dark (code %d, correct %d)",
               static_cast<double>(r.linear), static_cast<double>(shown),
               static_cast<double>(ratio), raw, engine::linear_to_srgb_u8(r.linear));
    }

    // THE SHAPE OF THE ERROR IS WHY IT SURVIVED FOUR MODULES. It is not a
    // uniform darkening — it is savage in shadow and nearly absent near white,
    // which reads as "moody" rather than "broken".
    const float dark = 0.05f / engine::srgb_to_linear(std::round(0.05f * 255.0f) / 255.0f);
    const float light = 0.80f / engine::srgb_to_linear(std::round(0.80f * 255.0f) / 255.0f);
    checkf(dark > 5.0f * light,
           "the error is %.1fx at linear 0.05 and %.1fx at 0.80 — worst where nobody looks "
           "for a bug, mildest where everybody checks",
           static_cast<double>(dark), static_cast<double>(light));

    // And the opposite mistake, for completeness: encoding twice.
    const float once = engine::linear_to_srgb(0.5f);
    const float twice = engine::linear_to_srgb(once);
    checkf(twice > once,
           "encoding twice pushes 0.5 to %.4f then %.4f — the washed-out, milky picture, "
           "which is the same mistake with the sign flipped",
           static_cast<double>(once), static_cast<double>(twice));
}

// ===========================================================================
//  §D — THE AUDIT
// ===========================================================================

void section_d_audit()
{
    std::printf("\n=== D. Every conversion in the engine is one of two kinds ===\n");

    // The struct-level helpers must agree with the scalar ones, or the engine has
    // two definitions of the same curve and only one of them is tested.
    const Uint32 c = engine::pack_argb(11, 128, 222, 77);
    const engine::linear_rgb lin = engine::to_linear(c);
    checkf(std::fabs(lin.r - engine::srgb_to_linear_u8(11)) < 1e-7f
               && std::fabs(lin.g - engine::srgb_to_linear_u8(128)) < 1e-7f
               && std::fabs(lin.b - engine::srgb_to_linear_u8(222)) < 1e-7f,
           "to_linear() is srgb_to_linear_u8() three times: (%.4f, %.4f, %.4f)",
           static_cast<double>(lin.r), static_cast<double>(lin.g),
           static_cast<double>(lin.b));

    // ALPHA IS COVERAGE, NOT LIGHT. Running it through a transfer function is a
    // category error, and it is one the engine must never make.
    const Uint32 back = engine::to_encoded(lin);
    checkf(engine::alpha_of(back) == 255,
           "to_encoded() writes opaque alpha and never transfer-functions coverage "
           "(alpha %d)",
           engine::alpha_of(back));
    check(engine::red_of(back) == 11 && engine::green_of(back) == 128
              && engine::blue_of(back) == 222,
          "…and the three colour channels survive the round trip exactly");

    const Uint32 mixed = engine::mix_linear(engine::pack_argb(0, 0, 0, 40),
                                            engine::pack_argb(255, 255, 255, 200), 0.5f);
    checkf(engine::alpha_of(mixed) == 120,
           "mix_linear blends alpha LINEARLY — (40 + 200) / 2 = %d — because it is a "
           "fraction of a pixel, not a quantity of light",
           engine::alpha_of(mixed));
    checkf(engine::red_of(mixed) == 188,
           "…while the colour channels blend as light: black to white at t = 0.5 is code "
           "%d, not 128",
           engine::red_of(mixed));

    // The engine's two legal answers to "where does the encode happen", stated as
    // a test so that a third one cannot appear quietly.
    check(engine::is_srgb_format(SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB)
              && engine::is_srgb_format(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB),
          "is_srgb_format recognises both 8-bit sRGB swapchain formats");
    check(!engine::is_srgb_format(SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM)
              && !engine::is_srgb_format(SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT),
          "…and does not mistake a UNORM or a float target for one");
}

// ===========================================================================
//  Device-side plumbing for §E and §F  (the shape verify_48 established)
// ===========================================================================

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

/// Put the unit quad on the device.
///
/// A mesh upload needs its own command buffer and a fence, because the copy pass
/// is asynchronous like everything else (Lesson 4.2) and the first draw must not
/// race the transfer that fills the buffer it reads.
[[nodiscard]] bool upload_quad(engine::gpu_device& gpu, engine::gpu_mesh& quad)
{
    SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(gpu.handle());
    if (cb == nullptr) { return false; }
    const bool ok = quad.create(gpu, cb, engine::quad_mesh(),
                                engine::index_mode::indexed, "verify61 quad");
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cb);
    if (fence != nullptr)
    {
        SDL_WaitForGPUFences(gpu.handle(), true, &fence, 1);
        SDL_ReleaseGPUFence(gpu.handle(), fence);
    }
    return ok;
}

/// Draw one quad whose emitted light is EXACTLY `ambient`, and read back the
/// centre pixel's red channel.
///
/// The scene is arranged so that the shading equation collapses: the key light
/// is black, the albedo is white, and there is no specular, so
/// `lit = base * (0 + ambient) = ambient`. That is deliberate — the question
/// under test is the transfer function, and any shading arithmetic left in the
/// path would be a second variable in a one-variable experiment.
[[nodiscard]] int render_flat(engine::gpu_device& gpu, engine::gpu_scene_renderer& renderer,
                              const engine::gpu_mesh& quad, SDL_GPUSampler* sampler,
                              target& t, float ambient, bool shader_encodes)
{
    if (!renderer.ensure_depth(gpu, static_cast<Uint32>(t.w), static_cast<Uint32>(t.h)))
    {
        return -1;
    }

    engine::gpu_draw_item item{};
    item.mesh = &quad;
    item.world_from_model = engine::mat4::identity();
    item.normal_from_model = engine::mat3::identity();
    item.material.albedo = engine::vec3{1.0f, 1.0f, 1.0f};
    item.material.specular = engine::vec3{0.0f, 0.0f, 0.0f};
    item.material.shininess = 1.0f;
    item.material.textured = 0.0f;
    item.texture = nullptr;
    item.style = engine::surface_style::two_sided;

    SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(gpu.handle());
    if (cb == nullptr) { return -1; }

    SDL_GPUColorTargetInfo cti{};
    cti.texture = t.colour;
    cti.clear_color = SDL_FColor{0.0f, 0.0f, 0.0f, 1.0f};
    cti.load_op = SDL_GPU_LOADOP_CLEAR;
    cti.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPUDepthStencilTargetInfo dsi = renderer.depth_target_info();
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cb, &cti, 1, &dsi);

    // An orthographic-ish clip transform that puts the unit quad over the whole
    // target: scale by 2 in x and y so [-0.5, 0.5] becomes [-1, 1].
    engine::mat4 clip = engine::mat4::identity();
    clip.c0.x = 2.0f;
    clip.c1.y = 2.0f;

    engine::scene_light_uniforms light{};
    light.to_light = engine::vec3{0.0f, 0.0f, 1.0f};
    light.key = engine::vec3{0.0f, 0.0f, 0.0f};          // no key: ambient only
    light.ambient = engine::vec3{ambient, ambient, ambient};
    light.eye_world = engine::vec3{0.0f, 0.0f, 5.0f};
    light.spec_model = 0.0f;
    light.encode_output = shader_encodes ? 1.0f : 0.0f;

    // A REAL SAMPLER, even though `textured` is 0 and nothing is sampled. The
    // renderer binds the white 1x1 fallback for an untextured item, and a binding
    // is a texture AND a sampler — passing null for the second one is a segfault
    // inside SDL, which is how this line came to have a comment.
    renderer.render(cb, pass, &item, 1, engine::camera_uniforms{clip}, light, sampler);
    SDL_EndGPURenderPass(pass);

    const std::vector<Uint8> px = download(t, cb);
    if (px.empty()) { return -1; }

    const std::size_t centre =
        (static_cast<std::size_t>(t.h / 2) * static_cast<std::size_t>(t.w)
         + static_cast<std::size_t>(t.w / 2)) * 4u;
    return px[centre];   // R8G8B8A8: red first
}

// ===========================================================================
//  §E — THE HEADLINE
// ===========================================================================

void section_e_targets(engine::gpu_device& gpu, engine::gpu_scene_renderer& renderer_unorm,
                       engine::gpu_scene_renderer& renderer_srgb,
                       const engine::gpu_mesh& quad, SDL_GPUSampler* sampler)
{
    std::printf("\n=== E. The same light, into three targets ===\n");

    constexpr int W = 16;
    constexpr int H = 16;
    constexpr float k_light = 0.5f;
    const int correct = engine::linear_to_srgb_u8(k_light);

    target unorm;
    target srgb;
    if (!unorm.create(gpu, W, H, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM)
        || !srgb.create(gpu, W, H, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB))
    {
        check(false, "could not create the two colour targets");
        unorm.destroy();
        srgb.destroy();
        return;
    }

    // 1. LINEAR LIGHT INTO A UNORM TARGET — what the engine did until this lesson.
    const int raw = render_flat(gpu, renderer_unorm, quad, sampler, unorm, k_light, false);
    checkf(raw >= 0 && std::abs(raw - 128) <= 1,
           "linear %.1f into a UNORM target stores code %d — the value written verbatim, "
           "and a display reads it as a CODE",
           static_cast<double>(k_light), raw);
    checkf(raw != correct,
           "…which is NOT the correct code %d. This is the bug, in one integer: the engine "
           "wrote %d where %d was meant, on every pixel, for four modules",
           correct, raw, correct);

    // 2. THE SAME LIGHT INTO AN _SRGB TARGET — what it does now.
    const int hw = render_flat(gpu, renderer_srgb, quad, sampler, srgb, k_light, false);
    checkf(hw >= 0 && std::abs(hw - correct) <= 1,
           "the identical shader into an _SRGB target stores code %d, and the CPU renderer's "
           "answer for the same light is %d — the hardware applied the curve on write",
           hw, correct);

    // 3. AND THE SHADER DOING IT ITSELF, into UNORM.
    const int sw = render_flat(gpu, renderer_unorm, quad, sampler, unorm, k_light, true);
    checkf(sw >= 0 && std::abs(sw - correct) <= 1,
           "the shader's own encode into a UNORM target stores code %d, within a code of the "
           "hardware's %d — so the fallback path is real rather than aspirational",
           sw, hw);

    // The three numbers together are the lesson.
    checkf(std::abs(hw - sw) <= 1 && std::abs(raw - hw) > 40,
           "SUMMARY at linear 0.5: raw %d, hardware-encoded %d, shader-encoded %d. Two of "
           "the three agree to within a code; the third is %d codes away",
           raw, hw, sw, std::abs(raw - hw));

    // …and the same experiment where the error is worst, because a single
    // midtone sample would understate it by a factor of five.
    constexpr float k_dark = 0.05f;
    const int dark_raw = render_flat(gpu, renderer_unorm, quad, sampler, unorm, k_dark, false);
    const int dark_hw = render_flat(gpu, renderer_srgb, quad, sampler, srgb, k_dark, false);
    checkf(dark_hw > 3 * dark_raw,
           "in shadow (linear %.2f) the gap widens: raw %d against %d correct — the error is "
           "a RATIO, so it is worst exactly where an image has the least contrast to spare",
           static_cast<double>(k_dark), dark_raw, dark_hw);

    unorm.destroy();
    srgb.destroy();
}

// ===========================================================================
//  §F — THE TWO ENCODERS AGREE
// ===========================================================================

void section_f_agreement(engine::gpu_device& gpu, engine::gpu_scene_renderer& renderer_unorm,
                         engine::gpu_scene_renderer& renderer_srgb,
                         const engine::gpu_mesh& quad, SDL_GPUSampler* sampler)
{
    std::printf("\n=== F. Our curve against the hardware's, across the range ===\n");

    constexpr int W = 8;
    constexpr int H = 8;
    target unorm;
    target srgb;
    if (!unorm.create(gpu, W, H, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM)
        || !srgb.create(gpu, W, H, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB))
    {
        check(false, "could not create the two colour targets");
        unorm.destroy();
        srgb.destroy();
        return;
    }

    // Three implementations of one curve: ours in C++, ours in HLSL, and the
    // hardware's fixed-function write. Lesson 4.8 established that the floor
    // here is ONE CODE and that it is worst in the darks, because these are three
    // approximations of a curve rather than three readings of a table.
    // A RAMP, not three spot checks — and printed in full, because these are the
    // numbers Figure 5 is drawn from and a figure built on measurements nobody
    // can reproduce is a drawing.
    const float samples[] = {0.02f, 0.05f, 0.1f, 0.2f, 0.35f,
                             0.5f, 0.65f, 0.8f, 0.95f};
    int worst_shader = 0;
    int worst_cpu = 0;
    std::printf("      linear    raw   hw   sw  cpu    (raw is the pre-6.1 engine)\n");
    for (const float lin : samples)
    {
        const int raw = render_flat(gpu, renderer_unorm, quad, sampler, unorm, lin, false);
        const int hw = render_flat(gpu, renderer_srgb, quad, sampler, srgb, lin, false);
        const int sw = render_flat(gpu, renderer_unorm, quad, sampler, unorm, lin, true);
        const int cpu = engine::linear_to_srgb_u8(lin);
        if (hw < 0 || sw < 0 || raw < 0) { continue; }
        worst_shader = std::max(worst_shader, std::abs(hw - sw));
        worst_cpu = std::max(worst_cpu, std::abs(hw - cpu));
        std::printf("      %6.2f  %5d %4d %4d %4d\n",
                    static_cast<double>(lin), raw, hw, sw, cpu);
    }

    checkf(worst_shader <= 1,
           "the shader's encode is within %d code of the hardware's across the range",
           worst_shader);
    checkf(worst_cpu <= 1,
           "and engine::linear_to_srgb_u8 is within %d code of both — three implementations "
           "of one curve, in three languages, on two processors",
           worst_cpu);

    unorm.destroy();
    srgb.destroy();
}

// ===========================================================================
//  §G — THE GOLDEN
// ===========================================================================

void section_g_golden()
{
    std::printf("\n=== G. The golden ===\n");

    const char* path = "build/demos/verify_61.ppm";
    check(demo::write_reference_shot(path) == 0, "write_reference_shot() succeeded");

    const std::string ours = read_file(path);
    const std::string golden = read_file("scratch/shot_52.ppm");
    checkf(!ours.empty() && ours.size() == golden.size(), "same size: %zu bytes", ours.size());
    check(ours == golden,
          "BYTE-IDENTICAL to the golden written in Lesson 5.1 — TWELVE lessons. The prediction "
          "in STATE.md was that this lesson would break it; it did not, and the reason is the "
          "finding: the SOFTWARE renderer has been correct since Lesson 1.6, and every fix here "
          "was on the GPU path's output stage");
}

}   // namespace

int main(int argc, char* argv[])
{
    bool debug = false;
    for (int i = 1; i < argc; ++i)
    {
        if (SDL_strcmp(argv[i], "--debug") == 0) { debug = true; }
    }

    // UNBUFFERED, so that a crash loses no output. This harness drives a GPU,
    // and a segfault three sections in with an empty log tells you nothing about
    // which section it was — which is exactly what happened while writing it.
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    // SDL_INIT_VIDEO, because SDL_CreateGPUDevice needs it — and without it the
    // device politely fails and §E/§F would "skip" on a machine that has a
    // perfectly good GPU. A harness that reports "no GPU" when it means "I forgot
    // to initialise" is worse than one that crashes.
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::printf("SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    std::printf("verify_61 — Lesson 6.1: linear and sRGB\n");
    std::printf("(debug assertions %s)\n",
                engine::debug_assertions_enabled() ? "ON" : "OFF");

    section_a_curve();
    section_b_budget();
    section_c_cost();
    section_d_audit();

    // ---- The GPU half -------------------------------------------------------
    engine::gpu_device gpu;
    if (gpu.create(nullptr, debug).ok())
    {
        engine::gpu_shader vs;
        engine::gpu_shader fs;
        engine::gpu_mesh quad;
        engine::gpu_sampler sampler;
        engine::gpu_scene_renderer renderer_unorm;
        engine::gpu_scene_renderer renderer_srgb;

        const bool shaders_ok = vs.load(gpu, "scene.vert", engine::shader_stage::vertex)
                                && fs.load(gpu, "scene.frag", engine::shader_stage::fragment);

        // TWO RENDERERS, because a pipeline's colour target format is baked in at
        // creation (Lesson 4.4) — which is itself part of this lesson's point:
        // "where does the encode happen" is pipeline state, decided before any
        // pixel exists, and not something a frame can change its mind about.
        const bool pipes_ok =
            shaders_ok
            && renderer_unorm.create(gpu, vs.handle(), fs.handle(),
                                     SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
                                     SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM)
            && renderer_srgb.create(gpu, vs.handle(), fs.handle(),
                                    SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
                                    SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB)
            && upload_quad(gpu, quad)
            && sampler.create(gpu, engine::filter::nearest, engine::address_mode::repeat);

        if (pipes_ok)
        {
            section_e_targets(gpu, renderer_unorm, renderer_srgb, quad, sampler.handle());
            section_f_agreement(gpu, renderer_unorm, renderer_srgb, quad, sampler.handle());
        }
        else
        {
            std::printf("\n=== E/F skipped: shaders or pipelines unavailable ===\n");
            std::printf("    (run from the repository root, after `cmake --build build`)\n");
        }

        sampler.destroy();
        quad.destroy();
        renderer_srgb.destroy();
        renderer_unorm.destroy();
        fs.destroy();
        vs.destroy();
        gpu.destroy();
    }
    else
    {
        std::printf("\n=== E/F skipped: no GPU device on this machine ===\n");
    }

    section_g_golden();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    SDL_Quit();
    return g_failures == 0 ? 0 : 1;
}
