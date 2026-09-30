// scratch/verify_618b.cpp — every number Lesson 6.18b quotes, produced here.
//
//   sh scratch/build_verify_618b.sh                                # as configured
//   ENGINE_CFLAGS="-O2 -DNDEBUG" sh scratch/build_verify_618b.sh   # release, for §L's times
//
// §A  the record: three 16-byte rows, and the struct that is not, read back from the GPU
// §B  the reflection's six counts, and the arithmetic of a grid
// §C  randomness: a hash, bit for bit on both processors — and three that are not random
// §D  the emission clock: the fraction, the ring, the wrap, and a pool too small
// §E  births spread through the step, and the shells without it
// §F  the step: implicit drag, the bounce, and why the closed form is not enough
// §G  CPU and GPU, particle by particle, for ten simulated seconds
// §H  counting on a GPU: the lost update, the atomic, the list and its order
// §I  `cycle`: the buffer's load op, set wrong
// §J  the frame graph learns buffers: derived cycles, refusals, and the same frame twice
// §K  the draw: billboards, additive light, depth, and the order that does not matter
// §L  the budget: a CPU step and its upload against a GPU step
//
// §G AND §I ARE THE TWO THAT MATTER. §G is the port's claim — the kernel does
// what `step_slot` does — held to the only standard a float computation can be
// held to: bit-exact where the arithmetic is integer, within a stated bound where
// it is not, and an honest account of the particles for which the bound does not
// hold. §I is the lesson's one new failure: a flag that is right for every buffer
// the engine had before this lesson and wrong for the first one whose contents
// outlive a frame.

#include <engine/gfx/frame_graph.hpp>
#include <engine/gfx/gpu_compute.hpp>
#include <engine/gfx/gpu_debug.hpp>
#include <engine/gfx/gpu_device.hpp>
#include <engine/gfx/gpu_particles.hpp>
#include <engine/gfx/gpu_post.hpp>
#include <engine/gfx/gpu_shader.hpp>
#include <engine/gfx/gpu_texture.hpp>
#include <engine/gfx/hdr.hpp>
#include <engine/gfx/particles.hpp>
#include <engine/math/mat4.hpp>

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using engine::emitter_settings;
using engine::particle;
using engine::particle_pool;
using engine::step_uniforms;
using engine::vec3;

namespace {

int g_checks = 0;
std::vector<particle> g_cpu_after_g;   ///< §G's CPU pool at step 600, for §H
long g_last_distinct = 0;
int g_failures = 0;

void check(bool ok, const char* what)
{
    ++g_checks;
    if (!ok) { ++g_failures; }
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
}

void check_eq(long long got, long long want, const char* what)
{
    ++g_checks;
    const bool ok = got == want;
    if (!ok) { ++g_failures; }
    std::printf("  [%s] %s  (got %lld, want %lld)\n", ok ? "PASS" : "FAIL", what, got, want);
}

void section(const char* title)
{
    std::printf("\n---------------------------------------------------------------------------\n");
    std::printf("%s\n", title);
    std::printf("---------------------------------------------------------------------------\n");
}

double now_ms()
{
    return 1000.0 * static_cast<double>(SDL_GetPerformanceCounter())
         / static_cast<double>(SDL_GetPerformanceFrequency());
}

constexpr float k_h = 1.0f / 60.0f;
constexpr Uint32 k_pool = 65536;

/// Where the figure data goes. Relative to the repository root, which is where
/// the build script runs the binary from.
const char* k_data_dir = "scratch/_618b";

FILE* open_data(const char* name)
{
    std::string path = std::string(k_data_dir) + "/" + name;
    return std::fopen(path.c_str(), "w");
}

float ulp_distance(float a, float b)
{
    if (a == b) { return 0.0f; }
    Sint32 ia = 0;
    Sint32 ib = 0;
    std::memcpy(&ia, &a, 4);
    std::memcpy(&ib, &b, 4);
    if (ia < 0) { ia = static_cast<Sint32>(0x80000000u) - ia; }
    if (ib < 0) { ib = static_cast<Sint32>(0x80000000u) - ib; }
    const long long d = static_cast<long long>(ia) - static_cast<long long>(ib);
    return static_cast<float>(d < 0 ? -d : d);
}

// ===========================================================================
//  The GPU rig
// ===========================================================================

struct rig
{
    engine::gpu_device gpu;
    engine::gpu_particles particles;
    engine::gpu_compute_pipeline probe;
    engine::gpu_shader vert;
    engine::gpu_shader frag;
    engine::gpu_texture hdr;
    engine::gpu_texture depth;
    SDL_GPUTextureFormat depth_format = SDL_GPU_TEXTUREFORMAT_INVALID;
    SDL_GPUTransferBuffer* readback = nullptr;
    int w = 320;
    int h = 180;
    bool ok = false;
};

bool build_rig(rig& r)
{
    if (!r.gpu.create(nullptr, false).ok()) { return false; }
    if (!r.vert.load(r.gpu, "particle.vert", engine::shader_stage::vertex)) { return false; }
    if (!r.frag.load(r.gpu, "particle.frag", engine::shader_stage::fragment)) { return false; }

    const SDL_GPUTextureFormat wanted[] = {SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
                                           SDL_GPU_TEXTUREFORMAT_D24_UNORM,
                                           SDL_GPU_TEXTUREFORMAT_D16_UNORM};
    r.depth_format = engine::supported_depth_format(r.gpu, wanted, 3);

    if (!r.particles.create(r.gpu, k_pool, r.vert.handle(), r.frag.handle(),
                            engine::k_hdr_format, r.depth_format))
    {
        return false;
    }
    if (!r.probe.load(r.gpu, "particles_probe.comp")) { return false; }

    if (!r.hdr.create_colour_target(r.gpu, engine::k_hdr_format, static_cast<Uint32>(r.w),
                                    static_cast<Uint32>(r.h), "verify hdr", true))
    {
        return false;
    }
    if (!r.depth.create_depth(r.gpu, r.depth_format, static_cast<Uint32>(r.w),
                              static_cast<Uint32>(r.h), "verify depth"))
    {
        return false;
    }

    SDL_GPUTransferBufferCreateInfo tb{};
    tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    tb.size = static_cast<Uint32>(r.w * r.h * 8);
    r.readback = SDL_CreateGPUTransferBuffer(r.gpu.handle(), &tb);
    r.ok = r.readback != nullptr;
    return r.ok;
}

void submit_and_wait(rig& r, SDL_GPUCommandBuffer* cb)
{
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cb);
    if (fence == nullptr) { return; }
    SDL_WaitForGPUFences(r.gpu.handle(), true, &fence, 1);
    SDL_ReleaseGPUFence(r.gpu.handle(), fence);
}

/// Copy `bytes` of a device buffer back to the CPU, and wait for it.
std::vector<Uint8> download_bytes(rig& r, SDL_GPUBuffer* buffer, Uint32 bytes)
{
    std::vector<Uint8> out;
    SDL_GPUTransferBufferCreateInfo tb{};
    tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    tb.size = bytes;
    SDL_GPUTransferBuffer* t = SDL_CreateGPUTransferBuffer(r.gpu.handle(), &tb);
    if (t == nullptr) { return out; }

    SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cb);
    SDL_GPUBufferRegion src{};
    src.buffer = buffer;
    src.offset = 0;
    src.size = bytes;
    SDL_GPUTransferBufferLocation dst{};
    dst.transfer_buffer = t;
    dst.offset = 0;
    SDL_DownloadFromGPUBuffer(copy, &src, &dst);
    SDL_EndGPUCopyPass(copy);
    submit_and_wait(r, cb);

    out.resize(bytes);
    const void* mapped = SDL_MapGPUTransferBuffer(r.gpu.handle(), t, false);
    if (mapped != nullptr)
    {
        std::memcpy(out.data(), mapped, bytes);
        SDL_UnmapGPUTransferBuffer(r.gpu.handle(), t);
    }
    SDL_ReleaseGPUTransferBuffer(r.gpu.handle(), t);
    return out;
}

template <class T>
std::vector<T> download(rig& r, SDL_GPUBuffer* buffer, Uint32 count)
{
    const std::vector<Uint8> bytes = download_bytes(r, buffer, count * static_cast<Uint32>(sizeof(T)));
    std::vector<T> out(count);
    if (bytes.size() == out.size() * sizeof(T)) { std::memcpy(out.data(), bytes.data(), bytes.size()); }
    return out;
}

/// A device buffer for the probe, zero-filled.
SDL_GPUBuffer* probe_buffer(rig& r, Uint32 bytes, const char* name)
{
    SDL_GPUBufferCreateInfo info{};
    info.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
    info.size = bytes;
    SDL_GPUBuffer* b = engine::create_named_buffer(r.gpu.handle(), info, name);
    if (b == nullptr) { return nullptr; }

    std::vector<Uint8> zeros(bytes, 0);
    SDL_GPUTransferBufferCreateInfo tb{};
    tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb.size = bytes;
    SDL_GPUTransferBuffer* t = SDL_CreateGPUTransferBuffer(r.gpu.handle(), &tb);
    void* m = SDL_MapGPUTransferBuffer(r.gpu.handle(), t, false);
    std::memcpy(m, zeros.data(), bytes);
    SDL_UnmapGPUTransferBuffer(r.gpu.handle(), t);
    SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cb);
    SDL_GPUTransferBufferLocation src{};
    src.transfer_buffer = t;
    SDL_GPUBufferRegion dst{};
    dst.buffer = b;
    dst.size = bytes;
    SDL_UploadToGPUBuffer(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);
    submit_and_wait(r, cb);
    SDL_ReleaseGPUTransferBuffer(r.gpu.handle(), t);
    return b;
}

struct probe_uniforms
{
    Uint32 mode = 0;
    Uint32 count = 0;
    Uint32 base = 0;
    Uint32 pad = 0;
};

/// Run the probe kernel once over `threads` threads (rounded up to groups).
void run_probe(rig& r, SDL_GPUBuffer* words, SDL_GPUBuffer* rows, SDL_GPUBuffer* naive,
               const probe_uniforms& u, Uint32 threads)
{
    SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
    SDL_GPUStorageBufferReadWriteBinding rw[3]{};
    rw[0].buffer = words;
    rw[1].buffer = rows;
    rw[2].buffer = naive;
    SDL_GPUComputePass* pass = SDL_BeginGPUComputePass(cb, nullptr, 0, rw, 3);
    SDL_BindGPUComputePipeline(pass, r.probe.handle());
    SDL_PushGPUComputeUniformData(cb, 0, &u, sizeof(u));
    SDL_DispatchGPUCompute(pass, r.probe.groups_for_items(threads), 1, 1);
    SDL_EndGPUComputePass(pass);
    submit_and_wait(r, cb);
}

// ===========================================================================
//  §A — the record
// ===========================================================================

void section_a(rig* r)
{
    section("§A  the record: three 16-byte rows, read back from the GPU");

    std::printf("  C++  sizeof(particle) %zu  age @%zu  velocity @%zu  life @%zu  previous @%zu  serial @%zu\n",
                sizeof(particle), offsetof(particle, age), offsetof(particle, velocity),
                offsetof(particle, life), offsetof(particle, previous), offsetof(particle, serial));

    struct naive_cpp { vec3 position; vec3 velocity; float age; };
    std::printf("  C++  the tempting struct {vec3, vec3, float}: sizeof %zu, velocity @%zu, age @%zu\n",
                sizeof(naive_cpp), offsetof(naive_cpp, velocity), offsetof(naive_cpp, age));

    if (r == nullptr) { std::printf("  (no GPU: the device half skipped)\n"); return; }

    constexpr Uint32 n = 4;
    SDL_GPUBuffer* words = probe_buffer(*r, 64, "probe words");
    SDL_GPUBuffer* rows = probe_buffer(*r, n * 48, "probe rows");
    SDL_GPUBuffer* naive = probe_buffer(*r, n * 64, "probe naive");
    probe_uniforms u{};
    u.mode = 0;
    u.count = n;
    run_probe(*r, words, rows, naive, u, n);

    const std::vector<float> rf = download<float>(*r, rows, n * 12);
    const std::vector<float> nf = download<float>(*r, naive, n * 16);

    // Where did each known value land? Search the first two records.
    auto where = [](const std::vector<float>& v, float value, int from) {
        for (int i = from; i < static_cast<int>(v.size()); ++i) { if (v[static_cast<std::size_t>(i)] == value) { return i * 4; } }
        return -1;
    };

    std::printf("  GPU  Particle   : position.x @%d  age @%d  velocity.x @%d  life @%d  previous.x @%d  "
                "next record's position.x @%d\n",
                where(rf, 1.0f, 0), where(rf, 4.0f, 0), where(rf, 5.0f, 0), where(rf, 8.0f, 0),
                where(rf, 9.0f, 0), where(rf, 1.0f, 1));
    check(where(rf, 4.0f, 0) == 12 && where(rf, 5.0f, 0) == 16 && where(rf, 8.0f, 0) == 28
              && where(rf, 9.0f, 0) == 32 && where(rf, 1.0f, 1) == 48,
          "the GPU's Particle has C++'s offsets and C++'s 48-byte stride");

    Uint32 serial_bits = 0;
    std::memcpy(&serial_bits, &rf[11], 4);
    check_eq(serial_bits, 12, "the uint in row 2 arrives as the bits of 12u");

    const int nv = where(nf, 4.0f, 0);
    const int na = where(nf, 7.0f, 0);
    const int stride = where(nf, 1.0f, 1);
    std::printf("  GPU  Naive      : velocity.x @%d  age @%d  stride %d   (C++ says 12, 24 and 28)\n",
                nv, na, stride);
    check(nv == 16 && na == 28 && stride == 32,
          "the tempting struct is laid out 16/28/32 by this toolchain: C++ would read garbage");

    // What C++ would read if it trusted its own struct: velocity from byte 12.
    std::printf("  C++  reading the GPU's Naive through naive_cpp: velocity = (%g, %g, %g), age = %g\n",
                static_cast<double>(nf[3]), static_cast<double>(nf[4]), static_cast<double>(nf[5]),
                static_cast<double>(nf[6]));

    SDL_ReleaseGPUBuffer(r->gpu.handle(), words);
    SDL_ReleaseGPUBuffer(r->gpu.handle(), rows);
    SDL_ReleaseGPUBuffer(r->gpu.handle(), naive);
}

// ===========================================================================
//  §B — reflection and grids
// ===========================================================================

std::string read_text(const std::string& path)
{
    std::size_t size = 0;
    void* data = SDL_LoadFile(path.c_str(), &size);
    if (data == nullptr) { return {}; }
    std::string s(static_cast<const char*>(data), size);
    SDL_free(data);
    return s;
}

void section_b()
{
    section("§B  the reflection's six counts, and the arithmetic of a grid");

    const std::string step = read_text(engine::shader_path("particles_step.comp.json"));
    const std::string compact = read_text(engine::shader_path("particles_compact.comp.json"));
    const std::string vert = read_text(engine::shader_path("particle.vert.json"));
    std::printf("  particles_step.comp.json    : %s\n", step.c_str());
    std::printf("  particles_compact.comp.json : %s\n", compact.c_str());

    engine::compute_resources cr{};
    check(engine::parse_compute_reflection(step, cr) && cr.readwrite_storage_buffers == 1
              && cr.uniform_buffers == 1 && cr.threads_x == 64,
          "step: one read-write buffer, one uniform block, 64 threads a group");
    check(engine::parse_compute_reflection(compact, cr) && cr.readonly_storage_buffers == 1
              && cr.readwrite_storage_buffers == 2,
          "compact: the pool read-only, the arguments and the list read-write");

    engine::shader_resources sr{};
    check(!engine::parse_shader_reflection(step, sr),
          "4.3's parser REFUSES a compute reflection (\"storage_buffers\" is not a key there)");
    check(!engine::parse_compute_reflection(vert, cr),
          "and the compute parser refuses a vertex shader's");
    check(engine::parse_shader_reflection(vert, sr) && sr.storage_buffers == 2,
          "particle.vert: two storage buffers (the pool, the list), no inputs");

    check_eq(engine::groups_for(1000, 64), 16, "1,000 threads at 64 a group: 16 groups");
    check_eq(16 * 64 - 1000, 24, "...and 24 threads with nothing to do");
    check_eq(engine::groups_for(65536, 64), 1024, "65,536: exactly 1,024 groups, no tail");
    check_eq(engine::groups_for(0xFFFFFFFFu, 64), 67108864, "2^32 - 1 threads: no overflow");
    const Uint32 naive = (0xFFFFFFFFu + 63u) / 64u;
    std::printf("  control: (n + 63) / 64 at n = 2^32 - 1 gives %u groups\n", naive);
    check(naive != 67108864u, "CONTROL: the textbook ceiling wraps");
}

// ===========================================================================
//  §C — randomness
// ===========================================================================

/// The failure that looks most like it works: one LCG step from a seed that IS
/// the index. Consecutive particles get consecutive seeds, and an LCG maps
/// consecutive seeds to values a constant apart.
Uint32 lcg_of_index(Uint32 i) { return i * 1664525u + 1013904223u; }

/// The shader-folklore hash, in float: fract(sin(x) * 43758.5453).
float sin_hash(float x)
{
    const float s = std::sin(x) * 43758.5453f;
    return s - std::floor(s);
}

void section_c(rig* r)
{
    section("§C  randomness: a hash, bit for bit — and three that are not random");

    // ---- CPU: quality of four generators ---------------------------------
    //
    // Two statistics, both about PAIRS, because a spark's direction is a pair of
    // draws: the correlation between draw k of particle i and particle i+1, and
    // how many of a 64x64 grid of cells the pairs (u(2i), u(2i+1)) reach.
    constexpr int n = 1 << 16;
    auto stats = [&](const char* name, auto&& u_of) {
        double sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0;
        std::vector<int> cells(64 * 64, 0);
        std::vector<float> seen;
        seen.reserve(n);
        for (int i = 0; i < n; ++i)
        {
            const double x = u_of(static_cast<Uint32>(i));
            const double y = u_of(static_cast<Uint32>(i + 1));
            sx += x; sy += y; sxx += x * x; syy += y * y; sxy += x * y;
            const float a = static_cast<float>(u_of(static_cast<Uint32>(2 * i)));
            const float b = static_cast<float>(u_of(static_cast<Uint32>(2 * i + 1)));
            const int cx = std::clamp(static_cast<int>(a * 64.0f), 0, 63);
            const int cy = std::clamp(static_cast<int>(b * 64.0f), 0, 63);
            ++cells[static_cast<std::size_t>(cy * 64 + cx)];
            seen.push_back(a);
        }
        const double m = n;
        const double cov = sxy / m - (sx / m) * (sy / m);
        const double var = std::sqrt((sxx / m - (sx / m) * (sx / m)) * (syy / m - (sy / m) * (sy / m)));
        int reached = 0;
        double chi = 0.0;
        const double expect = m / 4096.0;
        for (int c : cells) { reached += (c > 0) ? 1 : 0; chi += (c - expect) * (c - expect) / expect; }
        std::sort(seen.begin(), seen.end());
        const long distinct = std::unique(seen.begin(), seen.end()) - seen.begin();
        g_last_distinct = distinct;
        std::printf("  %-36s  corr(i, i+1) %+.4f   cells %4d/4096   chi2 %9.0f   distinct %6ld/%d\n",
                    name, var > 0 ? cov / var : 0.0, reached, chi, distinct, n);
        return std::make_pair(var > 0 ? cov / var : 0.0, reached);
    };

    const auto pcg = stats("pcg_hash(i)", [](Uint32 i) { return engine::unit_float(engine::pcg_hash(i)); });
    const auto lcg = stats("one LCG step, seeded by i", [](Uint32 i) { return engine::unit_float(lcg_of_index(i)); });
    const auto sin_small = stats("fract(sin(i) * 43758.5453), i < 2^17",
                                 [](Uint32 i) { return sin_hash(static_cast<float>(i)); });
    check(g_last_distinct < 8192, "the sine hash in float: fewer than 8,192 distinct values from 65,536");
    const auto sin_big = stats("fract(sin(i) * 43758.5453), i >= 2^24",
                               [](Uint32 i) { return sin_hash(static_cast<float>(i + (1u << 24))); });

    check(std::fabs(pcg.first) < 0.02 && pcg.second == 4096,
          "PCG: neighbours uncorrelated, every one of 4,096 cells reached");
    check(lcg.first > 0.99, "one LCG step per index: neighbours correlated almost perfectly");
    // THE PREDICTION HERE WAS REFUSED. It said the sine hash, past 2^24, would
    // leave half the cells empty; it reaches all 4,096. What it actually lacks is
    // BITS: x 43758.5453 lifts the product to where a float keeps only eight
    // fractional bits, so fract() can return few distinct values — and past 2^24,
    // float(i) cannot represent odd integers, so every other pair of neighbours
    // is the SAME input.
    (void)sin_small;
    check(sin_big.first > 0.4, "the sine hash past 2^24: neighbours correlated (half share an input)");

    // The figure: the pairs for three of them.
    if (FILE* f = open_data("c_pairs.csv"))
    {
        std::fprintf(f, "i,pcg_a,pcg_b,lcg_a,lcg_b,sin_a,sin_b\n");
        for (int i = 0; i < 2048; ++i)
        {
            const Uint32 a = static_cast<Uint32>(2 * i);
            const Uint32 b = a + 1u;
            std::fprintf(f, "%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n", i,
                         static_cast<double>(engine::unit_float(engine::pcg_hash(a))),
                         static_cast<double>(engine::unit_float(engine::pcg_hash(b))),
                         static_cast<double>(engine::unit_float(lcg_of_index(a))),
                         static_cast<double>(engine::unit_float(lcg_of_index(b))),
                         static_cast<double>(sin_hash(static_cast<float>(a + (1u << 24)))),
                         static_cast<double>(sin_hash(static_cast<float>(b + (1u << 24)))));
        }
        std::fclose(f);
    }

    // ---- The rng a particle actually uses: serial -> directions ----------
    {
        // The LCG-per-particle failure as a SPARK would show it: the first two
        // draws of consecutive serials, as (cos, phi).
        const engine::particle_rng a(1000, 7);
        const engine::particle_rng b(1001, 7);
        (void)a;
        (void)b;
    }

    if (r == nullptr) { return; }

    // ---- GPU: a million hashes, bit for bit ------------------------------
    constexpr Uint32 m = 1u << 20;
    SDL_GPUBuffer* words = probe_buffer(*r, m * 4, "probe hashes");
    SDL_GPUBuffer* rows = probe_buffer(*r, 48, "probe rows");
    SDL_GPUBuffer* naive = probe_buffer(*r, 64, "probe naive");

    probe_uniforms u{};
    u.mode = 1;
    u.count = m;
    u.base = 0x9E3779B9u;
    run_probe(*r, words, rows, naive, u, m);
    std::vector<Uint32> got = download<Uint32>(*r, words, m);
    Uint32 same = 0;
    for (Uint32 i = 0; i < m; ++i) { same += (got[i] == engine::pcg_hash(u.base + i)) ? 1u : 0u; }
    check_eq(same, m, "pcg_hash: 1,048,576 GPU outputs identical to C++'s, bit for bit");

    u.mode = 5;
    run_probe(*r, words, rows, naive, u, m);
    got = download<Uint32>(*r, words, m);
    same = 0;
    for (Uint32 i = 0; i < m; ++i)
    {
        const float f = engine::unit_float(engine::pcg_hash(u.base + i));
        Uint32 bits = 0;
        std::memcpy(&bits, &f, 4);
        same += (got[i] == bits) ? 1u : 0u;
    }
    check_eq(same, m, "unit_float: the same million floats, bit for bit — the conversion does not round");

    // The folklore hash on both processors: the same expression, two answers.
    u.mode = 6;
    u.base = 0;
    run_probe(*r, words, rows, naive, u, m);
    got = download<Uint32>(*r, words, m);
    same = 0;
    Uint32 far_off = 0;
    for (Uint32 i = 0; i < m; ++i)
    {
        const float c = sin_hash(static_cast<float>(i));
        float g = 0.0f;
        std::memcpy(&g, &got[i], 4);
        Uint32 bits = 0;
        std::memcpy(&bits, &c, 4);
        same += (got[i] == bits) ? 1u : 0u;
        far_off += (std::fabs(g - c) > 0.01f) ? 1u : 0u;
    }
    std::printf("  fract(sin(i) * 43758.5453), i < 2^20, on both processors: %u of %u identical, "
                "%u differ by more than 0.01\n", same, m, far_off);
    check(m - same > m / 3, "the sine hash is not even the same function: over a third of outputs differ");

    SDL_ReleaseGPUBuffer(r->gpu.handle(), words);
    SDL_ReleaseGPUBuffer(r->gpu.handle(), rows);
    SDL_ReleaseGPUBuffer(r->gpu.handle(), naive);
}

// ===========================================================================
//  §D — the emission clock
// ===========================================================================

void section_d()
{
    section("§D  the emission clock: the fraction, the ring, the wrap, a pool too small");

    // ---- The fraction -------------------------------------------------------
    constexpr int steps = 3600;   // one minute at 60 Hz
    engine::emission_clock clock;
    Uint64 born = 0;
    Uint64 rounded = 0;
    int c333 = 0;
    int c334 = 0;
    for (int i = 0; i < steps; ++i)
    {
        const engine::emission_window w = clock.advance(20000.0f, k_h, 1u << 20);
        born += w.count;
        rounded += static_cast<Uint64>(20000.0f * k_h);   // truncation, the tempting version
        c333 += (w.count == 333) ? 1 : 0;
        c334 += (w.count == 334) ? 1 : 0;
    }
    std::printf("  one minute at 20,000/s, h = 1/60: born %llu   (333 x %d, 334 x %d)\n",
                static_cast<unsigned long long>(born), c333, c334);
    std::printf("  truncating 333.33 per step instead: %llu — %llu short, %.2f%%\n",
                static_cast<unsigned long long>(rounded),
                static_cast<unsigned long long>(born - rounded),
                100.0 * static_cast<double>(born - rounded) / static_cast<double>(born));
    check(born >= 1199999 && born <= 1200001, "the carry: 1,200,000 in a minute, within one");
    check(born - rounded == 1200, "CONTROL: truncating loses 1,200 a minute (20 a second)");

    // ---- float carry against double, over an hour ------------------------
    {
        double cd = 0.0;
        float cf = 0.0f;
        Uint64 nd = 0;
        Uint64 nf = 0;
        for (int i = 0; i < 216000; ++i)
        {
            cd += 20000.0 * static_cast<double>(k_h);
            const double wd = std::floor(cd);
            cd -= wd;
            nd += static_cast<Uint64>(wd);
            cf += 20000.0f * k_h;
            const float wf = std::floor(cf);
            cf -= wf;
            nf += static_cast<Uint64>(wf);
        }
        std::printf("  one hour: double carry %llu, float carry %llu (differ by %lld); "
                    "exact 20,000 x 3,600 = 72,000,000; h as a float is %.10f\n",
                    static_cast<unsigned long long>(nd), static_cast<unsigned long long>(nf),
                    static_cast<long long>(nd) - static_cast<long long>(nf),
                    static_cast<double>(k_h));
    }

    // ---- The ring across the wrap ------------------------------------------
    {
        const Uint32 start = 0xFFFFFFFFu - 2u;
        std::printf("  serials across the wrap, pool 1024 (mask) vs pool 1000 (modulo):\n");
        bool pow2_continuous = true;
        bool mod_continuous = true;
        Uint32 prev_mask = (start - 1u) & 1023u;
        Uint32 prev_mod = (start - 1u) % 1000u;
        for (Uint32 k = 0; k < 6; ++k)
        {
            const Uint32 s = start + k;
            const Uint32 sm = engine::slot_of(s, 1023u);
            const Uint32 so = s % 1000u;
            std::printf("    serial %10u  ->  slot %4u  |  slot %4u\n", s, sm, so);
            pow2_continuous = pow2_continuous && (sm == ((prev_mask + 1u) & 1023u));
            mod_continuous = mod_continuous && (so == (prev_mod + 1u) % 1000u);
            prev_mask = sm;
            prev_mod = so;
        }
        check(pow2_continuous, "a power-of-two pool: consecutive serials, consecutive slots, across the wrap");
        check(!mod_continuous, "CONTROL: a pool of 1,000 jumps at the wrap");
        std::printf("  the wrap arrives after %.1f hours at 20,000/s\n",
                    4294967296.0 / 20000.0 / 3600.0);
    }

    // ---- A pool too small ----------------------------------------------------
    //
    // Run the CPU pool and, at every step, count the slots whose particle was
    // ALIVE before the step and was replaced by a birth during it.
    for (const Uint32 cap : {65536u, 32768u})
    {
        particle_pool pool(cap);
        emitter_settings e{};
        Uint64 births = 0;
        Uint64 cut = 0;
        double life_lost = 0.0;
        for (int i = 0; i < 600; ++i)
        {
            std::vector<particle> before(pool.particles().begin(), pool.particles().end());
            const step_uniforms u = pool.step(e, k_h);
            births += u.count;
            for (Uint32 d = 0; d < u.count; ++d)
            {
                const particle& old = before[engine::slot_of(u.first + d, u.mask)];
                if (engine::is_alive(old))
                {
                    ++cut;
                    life_lost += static_cast<double>(old.life - old.age);
                }
            }
        }
        std::printf("  pool %6u: %llu births, %llu cut short (%.1f%%), mean %.3f s of life lost; "
                    "a slot comes round every %.3f s\n",
                    cap, static_cast<unsigned long long>(births),
                    static_cast<unsigned long long>(cut),
                    births ? 100.0 * static_cast<double>(cut) / static_cast<double>(births) : 0.0,
                    cut ? life_lost / static_cast<double>(cut) : 0.0,
                    static_cast<double>(cap) / 20000.0);
        if (cap == 65536u) { check(cut == 0, "65,536 slots at 20,000/s: no particle outlives its slot (3 s < 3.28 s)"); }
        else               { check(cut > 0, "CONTROL: 32,768 slots cut every particle older than 1.64 s"); }
    }
}

// ===========================================================================
//  §E — births through the step
// ===========================================================================

void section_e()
{
    section("§E  births spread through the step, and the shells without it");

    // Two pools, identical but for one thing: the second spawns every birth of a
    // step at age 0, as if all were born at the instant the step ended.
    // THE FIRST VERSION OF THIS SECTION USED THE DEMO'S FOUNTAIN, and the
    // measurement refused the prediction: with speeds spread over 4 to 7 m/s the
    // two variants were equally smooth (0.221 against 0.232). A step's births
    // leave at speeds that differ by 3 m/s, so after t seconds they are spread
    // over 3t metres — more than a shell's 6.7-11.7 cm by the second step. Shells
    // need a NARROW speed range to survive, and that is what an emitter of
    // like-for-like droplets or tracer rounds has. So both are measured.
    for (int variant = 0; variant < 4; ++variant)
    {
    emitter_settings e{};
    e.cone = 0.12f;   // narrow, so the shells are easy to see side-on
    e.drag = 0.0f;
    const bool narrow = variant < 2;
    if (narrow) { e.speed_min = 6.9f; e.speed_max = 7.0f; }
    const bool spread = (variant % 2) == 0;
    {
        particle_pool pool(k_pool);
        for (int i = 0; i < 90; ++i)
        {
            if (spread)
            {
                (void)pool.step(e, k_h);
            }
            else
            {
                // step_slot, with birth_age forced to zero.
                static engine::emission_clock clock;
                if (i == 0) { clock.reset(); }
                const engine::emission_window w = clock.advance(e.rate, k_h, pool.capacity());
                const step_uniforms u = engine::make_step_uniforms(e, w, k_h, pool.capacity());
                for (Uint32 s = 0; s < pool.capacity(); ++s)
                {
                    particle& p = pool.particles()[s];
                    const Uint32 d = (s - u.first) & u.mask;
                    if (d < u.count) { p = engine::spawn_particle(u.first + d, 0.0f, u); continue; }
                    if (!engine::is_alive(p)) { continue; }
                    p.previous = p.position;
                    engine::integrate_particle(p, u, u.h, u.inv_drag_h);
                    p.age = p.age + u.h;
                }
            }
        }

        // Height histogram of the young (age < 0.4 s) in 5 mm bins, and how
        // lumpy it is: the mean absolute difference between neighbouring bins,
        // over the mean count.
        std::vector<int> bins(400, 0);
        int young = 0;
        for (const particle& p : pool.particles())
        {
            if (!engine::is_alive(p) || p.age >= 0.4f) { continue; }
            const int b = static_cast<int>(p.position.y / 0.005f);
            if (b >= 0 && b < 400) { ++bins[static_cast<std::size_t>(b)]; ++young; }
        }
        double diff = 0.0;
        double sum = 0.0;
        int used = 0;
        for (int b = 20; b < 300; ++b)
        {
            diff += std::abs(bins[static_cast<std::size_t>(b)] - bins[static_cast<std::size_t>(b - 1)]);
            sum += bins[static_cast<std::size_t>(b)];
            ++used;
        }
        const double lumpiness = (sum > 0) ? (diff / used) / (sum / used) : 0.0;
        std::printf("  %-10s %-28s young %6d   bin-to-bin change / mean count %.3f\n",
                    narrow ? "6.9-7 m/s" : "4-7 m/s",
                    spread ? "births spread (birth_age)" : "births at the step's end",
                    young, lumpiness);

        static const char* names[4] = {"e_spread.csv", "e_shells.csv", "e_wide_spread.csv", "e_wide_shells.csv"};
        if (FILE* f = open_data(names[variant]))
        {
            // EVERY SECOND young particle, across all their ages. The first
            // version wrote the first 6,000 in slot order, which is serial order,
            // which is age order — so the figure showed only the oldest young
            // particles and none of the shells nearest the emitter.
            std::fprintf(f, "x,y\n");
            int seen = 0;
            for (const particle& p : pool.particles())
            {
                if (!engine::is_alive(p) || p.age >= 0.4f) { continue; }
                if ((seen++ % 2) != 0) { continue; }
                std::fprintf(f, "%.4f,%.4f\n", static_cast<double>(p.position.x),
                             static_cast<double>(p.position.y));
            }
            std::fclose(f);
        }
        if (variant == 0) { check(lumpiness < 0.35, "narrow speeds, spread births: the heights vary smoothly"); }
        if (variant == 1) { check(lumpiness > 0.8, "CONTROL: narrow speeds, births at one instant stack in shells"); }
        if (variant == 3) { check(lumpiness < 0.35, "4-7 m/s: the spread of speeds smears the shells by itself"); }
    }
    }
    std::printf("  shell spacing at 4-7 m/s and h = 1/60: %.1f to %.1f cm\n",
                100.0 * 4.0 / 60.0, 100.0 * 7.0 / 60.0);
}

// ===========================================================================
//  §F — the step
// ===========================================================================

void section_f()
{
    section("§F  the step: implicit drag, the bounce, and why the closed form is not enough");

    // ---- Drag: explicit against implicit at k h = 2.5 --------------------
    const float k = 150.0f;
    float ve = 1.0f;
    float vi = 1.0f;
    for (int i = 0; i < 10; ++i)
    {
        ve = ve + (-k * ve) * k_h;
        vi = vi / (1.0f + k * k_h);
    }
    std::printf("  k = 150/s, h = 1/60 (k h = 2.5), ten steps from 1 m/s: explicit %.2f m/s, "
                "implicit %.2e m/s\n", static_cast<double>(ve), static_cast<double>(vi));
    check(std::fabs(ve) > 50.0f, "explicit drag past k h = 2 grows: |1 - k h| = 1.5 per step");
    check(vi > 0.0f && vi < 1e-4f, "implicit drag decays for any k h: 1 / (1 + k h) per step");
    std::printf("  at the demo's k = 0.4/s the two differ per step by %.2e of the velocity\n",
                static_cast<double>((1.0f - 0.4f * k_h) - 1.0f / (1.0f + 0.4f * k_h)));

    // ---- The bounce ---------------------------------------------------------
    emitter_settings e{};
    e.drag = 0.0f;
    const step_uniforms u = engine::make_step_uniforms(e, engine::emission_window{}, k_h, 64);
    particle p{};
    p.position = vec3{0.0f, 1.0f, 0.0f};
    p.life = 100.0f;
    float peak = 0.0f;
    float peaks[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    int bounces = 0;
    bool rising = false;
    for (int i = 0; i < 1200 && bounces < 4; ++i)
    {
        const float vy_before = p.velocity.y;
        engine::integrate_particle(p, u, k_h, u.inv_drag_h);
        if (vy_before < 0.0f && p.velocity.y > 0.0f) { rising = true; peak = 0.0f; }
        if (rising) { peak = std::max(peak, p.position.y); }
        if (rising && p.velocity.y < 0.0f)
        {
            rising = false;
            ++bounces;
            if (bounces < 4) { peaks[bounces] = peak; }
        }
    }
    std::printf("  dropped from 1 m, restitution 0.45: peaks %.4f, %.4f, %.4f m  "
                "(e^2 = %.4f; ratios %.4f, %.4f)\n",
                static_cast<double>(peaks[1]), static_cast<double>(peaks[2]),
                static_cast<double>(peaks[3]), 0.45 * 0.45,
                static_cast<double>(peaks[2] / peaks[1]), static_cast<double>(peaks[3] / peaks[2]));
    // THE PREDICTION WAS e^2 PER BOUNCE, and it holds only while a bounce lasts
    // many steps. The third peak is 3 mm, reached in three steps, and a bounce
    // resolved in three steps is not resolved. The control: the same drop at a
    // quarter of the step.
    float fine[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    {
        const float hf = k_h / 4.0f;
        const step_uniforms uf = engine::make_step_uniforms(e, engine::emission_window{}, hf, 64);
        particle pf{};
        pf.position = vec3{0.0f, 1.0f, 0.0f};
        pf.life = 100.0f;
        float pk = 0.0f;
        int nb = 0;
        bool up = false;
        for (int i = 0; i < 4800 && nb < 4; ++i)
        {
            const float vy = pf.velocity.y;
            engine::integrate_particle(pf, uf, hf, uf.inv_drag_h);
            if (vy < 0.0f && pf.velocity.y > 0.0f) { up = true; pk = 0.0f; }
            if (up) { pk = std::max(pk, pf.position.y); }
            if (up && pf.velocity.y < 0.0f) { up = false; ++nb; if (nb < 4) { fine[nb] = pk; } }
        }
    }
    std::printf("  the same drop at h/4: peaks %.4f, %.4f, %.4f m (ratios %.4f, %.4f, %.4f)\n",
                static_cast<double>(fine[1]), static_cast<double>(fine[2]), static_cast<double>(fine[3]),
                static_cast<double>(fine[1]), static_cast<double>(fine[2] / fine[1]),
                static_cast<double>(fine[3] / fine[2]));
    check(std::fabs(peaks[1] - 0.2025f) < 0.025f, "the first bounce keeps about e^2 of the drop");
    check(std::fabs(fine[2] / fine[1] - 0.2025f) < std::fabs(peaks[2] / peaks[1] - 0.2025f),
          "CONTROL: a quarter of the step brings the second ratio nearer e^2 — the step's error, not a bug");

    // ---- The closed form, and where it stops ------------------------------
    particle q{};
    q.velocity = vec3{1.0f, 6.0f, 0.0f};
    q.position = vec3{0.0f, 0.05f, 0.0f};
    q.life = 100.0f;
    float worst = 0.0f;
    for (int i = 1; i <= 60; ++i)
    {
        engine::integrate_particle(q, u, k_h, u.inv_drag_h);
        const float t = static_cast<float>(i) * k_h;
        const float y = 0.05f + 6.0f * t - 0.5f * 9.81f * t * t;
        if (y > 0.0f) { worst = std::max(worst, std::fabs(q.position.y - y)); }
    }
    std::printf("  no drag, before the bounce: the step against p0 + v0 t + g t^2 / 2 — worst %.4f m "
                "(semi-implicit Euler's first-order error, g h t / 2 = %.4f m at t = 1 s)\n",
                static_cast<double>(worst), 0.5 * 9.81 * (1.0 / 60.0) * 1.0);
    check(worst > 0.0f && worst < 0.1f, "without the ground a closed form exists, and the step is near it");
}

// ===========================================================================
//  §G — CPU and GPU, particle by particle
// ===========================================================================

struct comparison
{
    Uint32 alive_cpu = 0;
    Uint32 alive_gpu = 0;
    Uint32 bit_identical = 0;
    Uint32 serial_mismatch = 0;
    Uint32 alive_mismatch = 0;
    float worst_ulp = 0.0f;
    float worst_m = 0.0f;
    Uint32 over_1mm = 0;
    Uint32 within_1e5 = 0;
    Uint32 within_1e6 = 0;
    Uint32 live_identical = 0;
};

comparison compare_pools(const std::vector<particle>& cpu, const std::vector<particle>& gpu)
{
    comparison c{};
    for (std::size_t i = 0; i < cpu.size(); ++i)
    {
        const particle& a = cpu[i];
        const particle& b = gpu[i];
        c.alive_cpu += engine::is_alive(a) ? 1u : 0u;
        c.alive_gpu += engine::is_alive(b) ? 1u : 0u;
        c.bit_identical += (std::memcmp(&a, &b, sizeof(particle)) == 0) ? 1u : 0u;
        c.serial_mismatch += (a.serial != b.serial) ? 1u : 0u;
        c.alive_mismatch += (engine::is_alive(a) != engine::is_alive(b)) ? 1u : 0u;
        if (!engine::is_alive(a) || !engine::is_alive(b)) { continue; }
        c.live_identical += (std::memcmp(&a, &b, sizeof(particle)) == 0) ? 1u : 0u;
        const float d = engine::length(a.position - b.position);
        c.within_1e6 += (d <= 1e-6f) ? 1u : 0u;
        c.worst_m = std::max(c.worst_m, d);
        c.over_1mm += (d > 0.001f) ? 1u : 0u;
        c.within_1e5 += (d <= 1e-5f) ? 1u : 0u;
        c.worst_ulp = std::max({c.worst_ulp, ulp_distance(a.position.x, b.position.x),
                                ulp_distance(a.position.y, b.position.y),
                                ulp_distance(a.position.z, b.position.z)});
    }
    return c;
}

void section_g(rig& r)
{
    section("§G  CPU and GPU, particle by particle, for ten simulated seconds");

    particle_pool cpu(k_pool);
    emitter_settings e{};

    // Start the GPU pool from the CPU's (empty) state, so the two begin equal.
    {
        SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
        const std::vector<particle> zero(k_pool);
        (void)r.particles.set_state(cb, zero);
        submit_and_wait(r, cb);
    }

    // One step first, on its own: only births, so any difference is spawn's.
    {
        const step_uniforms u = cpu.step(e, k_h);
        SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
        const step_uniforms one[1] = {u};
        r.particles.simulate(cb, one);
        submit_and_wait(r, cb);

        const std::vector<particle> gpu = download<particle>(r, r.particles.state(), k_pool);
        const std::vector<particle> host(cpu.particles().begin(), cpu.particles().end());
        const comparison c = compare_pools(host, gpu);

        // Per field, over the births.
        Uint32 vel_same = 0;
        Uint32 life_same = 0;
        Uint32 pos_same = 0;
        float worst_v_ulp = 0.0f;
        for (Uint32 d = 0; d < u.count; ++d)
        {
            const Uint32 s = engine::slot_of(u.first + d, u.mask);
            const particle& a = host[s];
            const particle& b = gpu[s];
            life_same += (a.life == b.life) ? 1u : 0u;
            const bool vs = a.velocity == b.velocity;
            vel_same += vs ? 1u : 0u;
            pos_same += (a.position == b.position) ? 1u : 0u;
            worst_v_ulp = std::max({worst_v_ulp, ulp_distance(a.velocity.x, b.velocity.x),
                                    ulp_distance(a.velocity.y, b.velocity.y),
                                    ulp_distance(a.velocity.z, b.velocity.z)});
        }
        std::printf("  after ONE step (%u births): life identical %u, velocity identical %u, "
                    "position identical %u, whole record %u; worst velocity %g ULP\n",
                    u.count, life_same, vel_same, pos_same, c.bit_identical - (k_pool - u.count),
                    static_cast<double>(worst_v_ulp));
        check_eq(c.serial_mismatch, 0, "every serial in the same slot on both processors");
        check_eq(life_same, u.count, "every life identical: a multiply-add of exact floats");
    }

    // Then 599 more, checking at 1, 2, 5 and 10 seconds.
    FILE* f = open_data("g_agreement.csv");
    if (f != nullptr) { std::fprintf(f, "step,alive_cpu,alive_gpu,alive_mismatch,live_identical,within_1um,within_10um,over_1mm,worst_um\n"); }

    int done = 1;
    for (const int target : {6, 15, 30, 60, 120, 300, 600})
    {
        std::vector<step_uniforms> steps;
        while (done < target) { steps.push_back(cpu.step(e, k_h)); ++done; }
        SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
        r.particles.simulate(cb, steps);
        submit_and_wait(r, cb);

        const std::vector<particle> gpu = download<particle>(r, r.particles.state(), k_pool);
        const std::vector<particle> host(cpu.particles().begin(), cpu.particles().end());
        const comparison c = compare_pools(host, gpu);
        std::printf("  step %4d (%5.2f s): alive %5u / %5u (%u disagree)  live identical %5u  "
                    "within 1 um %5u  within 10 um %5u  over 1 mm %u  worst %.2f um\n",
                    done, done * k_h, c.alive_cpu, c.alive_gpu, c.alive_mismatch,
                    c.live_identical, c.within_1e6, c.within_1e5, c.over_1mm,
                    1e6 * static_cast<double>(c.worst_m));
        if (f != nullptr)
        {
            std::fprintf(f, "%d,%u,%u,%u,%u,%u,%u,%u,%.4f\n", done, c.alive_cpu, c.alive_gpu,
                         c.alive_mismatch, c.live_identical, c.within_1e6, c.within_1e5,
                         c.over_1mm, 1e6 * static_cast<double>(c.worst_m));
        }
        if (done == 600)
        {
            check_eq(c.serial_mismatch, 0, "after ten seconds, still every serial in the same slot");
            check_eq(c.alive_mismatch, 0, "after ten seconds, the same particles alive on both");
            check_eq(c.over_1mm, 0, "and not one live particle a millimetre from its twin");
            g_cpu_after_g.assign(cpu.particles().begin(), cpu.particles().end());

            // The CPU reference's own bytes, per build of the LIBRARY — so the two
            // builds of the same C++ can be compared with each other afterwards
            // (the lesson's §13: which CPU is the reference?).
#if defined(ENGINE_LIB_BUILD_TYPE)
            const std::string dump = std::string("g_cpu_") + ENGINE_LIB_BUILD_TYPE + ".bin";
#else
            const std::string dump = "g_cpu_unknown.bin";
#endif
            if (FILE* bin = open_data(dump.c_str()))
            {
                std::fwrite(g_cpu_after_g.data(), sizeof(particle), g_cpu_after_g.size(), bin);
                std::fclose(bin);
            }
        }
    }
    if (f != nullptr) { std::fclose(f); }
}

// ===========================================================================
//  §H — counting on a GPU
// ===========================================================================

void section_h(rig& r)
{
    section("§H  counting on a GPU: the lost update, the atomic, the list and its order");

    // ---- The race and the atomic --------------------------------------------
    constexpr Uint32 m = 1u << 20;
    SDL_GPUBuffer* rows = probe_buffer(r, 48, "probe rows");
    SDL_GPUBuffer* naive = probe_buffer(r, 64, "probe naive");
    Uint32 racy[3] = {};
    for (int trial = 0; trial < 3; ++trial)
    {
        SDL_GPUBuffer* words = probe_buffer(r, 64, "probe counter");
        probe_uniforms u{};
        u.mode = 2;
        u.count = m;
        run_probe(r, words, rows, naive, u, m);
        racy[trial] = download<Uint32>(r, words, 1)[0];
        SDL_ReleaseGPUBuffer(r.gpu.handle(), words);
    }
    SDL_GPUBuffer* words = probe_buffer(r, 64, "probe counter");
    probe_uniforms u{};
    u.mode = 3;
    u.count = m;
    run_probe(r, words, rows, naive, u, m);
    const Uint32 atomic = download<Uint32>(r, words, 1)[0];
    SDL_ReleaseGPUBuffer(r.gpu.handle(), words);
    std::printf("  %u threads each add one: words[0] + 1 -> %u, %u, %u (three runs); "
                "InterlockedAdd -> %u\n", m, racy[0], racy[1], racy[2], atomic);
    check_eq(atomic, m, "InterlockedAdd: every one of 1,048,576 increments counted");
    check(racy[0] < m / 100u, "the plain read-add-write loses more than 99% of them");

    // ---- The tail -------------------------------------------------------------
    {
        SDL_GPUBuffer* tail = probe_buffer(r, (1024u + 64u) * 4u, "probe tail");
        probe_uniforms t{};
        t.mode = 4;
        t.count = 1000;
        run_probe(r, tail, rows, naive, t, 1000);
        const std::vector<Uint32> w = download<Uint32>(r, tail, 1024u + 64u);
        Uint32 inside = 0;
        Uint32 past = 0;
        for (Uint32 i = 0; i < w.size(); ++i)
        {
            if (w[i] == 0xC0FFEEu) { (i < 1000u ? inside : past) += 1u; }
        }
        std::printf("  1,000 items, 16 groups of 64, no bounds check: %u writes in range, "
                    "%u past the end\n", inside, past);
        check_eq(past, 24, "the unguarded tail writes 24 elements past the end of the data");
        SDL_ReleaseGPUBuffer(r.gpu.handle(), tail);
    }
    SDL_ReleaseGPUBuffer(r.gpu.handle(), rows);
    SDL_ReleaseGPUBuffer(r.gpu.handle(), naive);

    // ---- The compaction, against the CPU's count -----------------------------
    if (g_cpu_after_g.empty()) { return; }
    Uint32 cpu_alive = 0;
    std::vector<Uint32> cpu_slots;
    for (Uint32 i = 0; i < g_cpu_after_g.size(); ++i)
    {
        if (engine::is_alive(g_cpu_after_g[i])) { ++cpu_alive; cpu_slots.push_back(i); }
    }

    std::vector<Uint32> lists[3];
    Uint32 counts[3] = {};
    for (int trial = 0; trial < 3; ++trial)
    {
        SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
        r.particles.count_living(cb, r.particles.state());
        submit_and_wait(r, cb);
        const std::vector<Uint32> args = download<Uint32>(r, r.particles.args(), 4);
        counts[trial] = args[1];
        lists[trial] = download<Uint32>(r, r.particles.alive(), args[1]);
        if (trial == 0)
        {
            std::printf("  args after compaction: {%u, %u, %u, %u}\n", args[0], args[1], args[2], args[3]);
        }
    }
    check_eq(counts[0], cpu_alive, "the GPU's count of the living is the CPU's, exactly");

    std::vector<Uint32> sorted = lists[0];
    std::sort(sorted.begin(), sorted.end());
    check(sorted == cpu_slots, "and the list holds exactly the CPU's living slots, once each");

    Uint32 moved = 0;
    Uint32 in_order = 0;
    for (std::size_t i = 0; i < lists[0].size() && i < lists[1].size(); ++i)
    {
        moved += (lists[0][i] != lists[1][i]) ? 1u : 0u;
        in_order += (i > 0 && lists[0][i] > lists[0][i - 1]) ? 1u : 0u;
    }
    std::printf("  the same state compacted twice: %u of %u list places hold a different slot; "
                "%.1f%% of neighbours ascending in the first\n", moved,
                static_cast<unsigned>(lists[0].size()),
                lists[0].size() > 1 ? 100.0 * in_order / static_cast<double>(lists[0].size() - 1) : 0.0);
    std::vector<Uint32> sorted1 = lists[1];
    std::sort(sorted1.begin(), sorted1.end());
    check(sorted1 == sorted, "the order changes and the set does not");

    // ---- The pass boundary SDL requires, removed ----------------------------
    //
    // Clear and compact in ONE compute pass: the compaction's atomics now depend
    // on a store of zero made by an earlier dispatch in the same pass, which
    // SDL_gpu.h says is not synchronised. On Vulkan and D3D12 SDL records no
    // barrier between the two (§10.1). On Metal SDL's encoder is created with
    // default settings, and this measures what that does here — twenty times,
    // because a race that wins once proves nothing.
    int exact = 0;
    for (int trial = 0; trial < 20; ++trial)
    {
        SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
        SDL_GPUStorageBufferReadWriteBinding rw[2]{};
        rw[0].buffer = r.particles.args();
        rw[1].buffer = r.particles.alive();
        SDL_GPUComputePass* pass = SDL_BeginGPUComputePass(cb, nullptr, 0, rw, 2);
        r.particles.record_clear(pass);
        r.particles.record_compact(cb, pass, r.particles.state());
        SDL_EndGPUComputePass(pass);
        submit_and_wait(r, cb);
        exact += (download<Uint32>(r, r.particles.args(), 4)[1] == cpu_alive) ? 1 : 0;
    }
    std::printf("  clear and compact in ONE pass, 20 trials: %d exact on this backend — SDL promises "
                "none of them\n", exact);
}

// ===========================================================================
//  §I — cycle, set wrong
// ===========================================================================

enum class between { nothing, fence, idle };

std::vector<particle> run_gpu_steps_cycled(rig& r, const std::vector<step_uniforms>& steps,
                                           bool cycle, between wait)
{
    {
        SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
        const std::vector<particle> zero(k_pool);
        (void)r.particles.set_state(cb, zero);
        submit_and_wait(r, cb);
    }

    // ONE COMMAND BUFFER PER STEP, and three ways of waiting between them. SDL
    // cycles a buffer only while it is still BOUND — referenced by a command
    // buffer SDL has not yet cleaned up — and the arms below find out when that
    // is: never waiting, waiting on the command buffer's fence, or waiting for the
    // whole device to go idle.
    for (const step_uniforms& u : steps)
    {
        SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
        SDL_GPUStorageBufferReadWriteBinding rw{};
        rw.buffer = r.particles.state();
        rw.cycle = cycle;
        SDL_GPUComputePass* pass = SDL_BeginGPUComputePass(cb, nullptr, 0, &rw, 1);
        r.particles.record_step(cb, pass, u);
        SDL_EndGPUComputePass(pass);
        if (wait == between::fence) { submit_and_wait(r, cb); }
        else                        { (void)SDL_SubmitGPUCommandBuffer(cb); }
        if (wait == between::idle)  { SDL_WaitForGPUIdle(r.gpu.handle()); }
    }
    SDL_WaitForGPUIdle(r.gpu.handle());
    return download<particle>(r, r.particles.state(), k_pool);
}

void section_i(rig& r)
{
    section("§I  cycle: the buffer's load op, set wrong");

    particle_pool cpu(k_pool);
    emitter_settings e{};
    std::vector<step_uniforms> steps;
    for (int i = 0; i < 120; ++i) { steps.push_back(cpu.step(e, k_h)); }
    const std::vector<particle> want(cpu.particles().begin(), cpu.particles().end());

    struct arm { const char* name; bool cycle; between wait; };
    const arm arms[4] = {{"cycle = false, back to back", false, between::nothing},
                         {"cycle = true,  back to back", true, between::nothing},
                         {"cycle = true,  fence wait each step", true, between::fence},
                         {"cycle = true,  GPU idle each step", true, between::idle}};
    comparison results[4];
    for (int a = 0; a < 4; ++a)
    {
        const std::vector<particle> got = run_gpu_steps_cycled(r, steps, arms[a].cycle, arms[a].wait);
        results[a] = compare_pools(want, got);
        std::printf("  %-34s alive %5u (CPU %5u)  serials wrong %5u  live within 10 um %5u\n",
                    arms[a].name, results[a].alive_gpu, results[a].alive_cpu,
                    results[a].serial_mismatch, results[a].within_1e5);
        if (a == 1 && std::getenv("VERIFY_618B_DUMP") == nullptr)
        {
            if (FILE* f = open_data("i_cycled.csv"))
            {
                std::fprintf(f, "x,y,alive\n");
                int written = 0;
                for (const particle& p : got)
                {
                    if (!engine::is_alive(p) || written >= 6000) { continue; }
                    std::fprintf(f, "%.4f,%.4f,1\n", static_cast<double>(p.position.x),
                                 static_cast<double>(p.position.y));
                    ++written;
                }
                std::fclose(f);
            }
        }
    }
    check(results[0].serial_mismatch == 0 && results[0].alive_mismatch == 0,
          "cycle = false: 120 steps back to back, the CPU's pool exactly");
    check(results[1].serial_mismatch > 1000 || results[1].alive_mismatch > 1000,
          "cycle = true, back to back: the pool is not the CPU's (SDL: undefined contents)");
    // THE PREDICTION HERE WAS REFUSED. It said cycling needs a busy GPU, so a
    // fence wait after every step would be correct. It is not: SDL's Metal
    // backend releases a command buffer's references only when a LATER submit
    // cleans it up (METAL_Submit scans for completed buffers; METAL_WaitForFences
    // does not), so at the next step's pass the buffer is still bound and cycles.
    // Only SDL_WaitForGPUIdle, which cleans every submitted buffer, prevents it.
    check(results[2].serial_mismatch > 1000 || results[2].alive_mismatch > 1000,
          "cycle = true with a FENCE wait after every step: still wrong");
    check(results[3].serial_mismatch == 0 && results[3].alive_mismatch == 0,
          "CONTROL: after SDL_WaitForGPUIdle nothing is bound, nothing cycles, and the pool is right");

    // ---- Which buffer does each cycled step start from? ---------------------
    //
    // Six steps, reading the pool back after each, and printing which serial
    // numbers are alive. A step that started from the previous step's output has
    // every serial born so far; one that started from an empty buffer has only
    // its own births; one that started from an OLDER buffer has that buffer's
    // births and its own, with a gap between.
    //
    // A FRESH pool for each mode, and that is not a detail. The first version of
    // this probe reused the rig's pool, whose buffer had already been cycled by the
    // arms above: SDL keeps every internal buffer it ever cycled to, and the
    // "undefined" contents it handed the probe were particles from those earlier
    // runs — serials up to 33,332 appearing after two steps. Which is the lesson's
    // point made twice, but it makes the pattern below unreadable.
    int diverged[2] = {0, 0};
    for (int mode = 0; mode < 2; ++mode)
    {
        engine::gpu_particles fresh;
        if (!fresh.create(r.gpu, k_pool, r.vert.handle(), r.frag.handle(), engine::k_hdr_format,
                          r.depth_format))
        {
            std::printf("  (could not create a fresh pool)\n");
            return;
        }
        SDL_WaitForGPUIdle(r.gpu.handle());
        particle_pool ref(k_pool);
        std::printf("  a fresh pool, %s, cycle = true, read back after each step:\n",
                    mode == 0 ? "submitted back to back" : "a fence wait after each step");
        for (int k = 1; k <= 6; ++k)
        {
            const step_uniforms u = ref.step(e, k_h);
            SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
            SDL_GPUStorageBufferReadWriteBinding rw{};
            rw.buffer = fresh.state();
            rw.cycle = true;
            SDL_GPUComputePass* pass = SDL_BeginGPUComputePass(cb, nullptr, 0, &rw, 1);
            fresh.record_step(cb, pass, u);
            SDL_EndGPUComputePass(pass);
            if (mode == 0) { (void)SDL_SubmitGPUCommandBuffer(cb); }
            else           { submit_and_wait(r, cb); }
            const std::vector<particle> got = download<particle>(r, fresh.state(), k_pool);
            Uint32 alive = 0;
            Uint32 lo = 0xFFFFFFFFu;
            Uint32 hi = 0;
            for (const particle& q : got)
            {
                if (!engine::is_alive(q)) { continue; }
                ++alive;
                lo = std::min(lo, q.serial);
                hi = std::max(hi, q.serial);
            }
            diverged[mode] += (alive != ref.alive_count()) ? 1 : 0;
            std::printf("    step %d: %4u alive, serials %4u..%4u   (the CPU: %4u alive, serials 0..%u)\n",
                        k, alive, alive ? lo : 0u, hi, ref.alive_count(), ref.born() - 1u);
        }
        fresh.destroy();
    }
    // What is ASSERTED is only what SDL's contract promises cannot be relied on:
    // the pool the steps produce is not the CPU's. WHICH buffer each step lands
    // on is the backend's business and its timing's, and is printed, not checked.
    check(diverged[0] >= 5, "back to back: from the second step on, the pool is not the CPU's");
    check(diverged[1] >= 5, "with a fence wait after each step: still not the CPU's");
}

// ===========================================================================
//  §J — the frame graph learns buffers
// ===========================================================================

struct step_ctx { const engine::gpu_particles* p = nullptr; step_uniforms u{}; };
struct draw_ctx
{
    const engine::gpu_particles* p = nullptr;
    engine::particle_draw_uniforms u{};
    engine::fg_buffer state{};
};
struct compact_ctx { const engine::gpu_particles* p = nullptr; engine::fg_buffer state{}; };

void exec_step(const engine::fg_pass_context& ctx, void* user)
{
    const auto* c = static_cast<const step_ctx*>(user);
    c->p->record_step(ctx.cb, ctx.compute, c->u);
}

void exec_clear(const engine::fg_pass_context& ctx, void* user)
{
    static_cast<const engine::gpu_particles*>(user)->record_clear(ctx.compute);
}

void exec_compact(const engine::fg_pass_context& ctx, void* user)
{
    const auto* c = static_cast<const compact_ctx*>(user);
    c->p->record_compact(ctx.cb, ctx.compute, ctx.buffer(c->state));
}

void exec_draw(const engine::fg_pass_context& ctx, void* user)
{
    const auto* c = static_cast<const draw_ctx*>(user);
    c->p->record_draw(ctx.cb, ctx.pass, c->u, ctx.buffer(c->state));
}

engine::particle_draw_uniforms camera_uniforms_for(const rig& r)
{
    const vec3 eye{0.0f, 1.6f, 6.0f};
    const engine::mat4 view = engine::look_at(eye, vec3{0.0f, 1.3f, 0.0f}, vec3{0.0f, 1.0f, 0.0f});
    const engine::mat4 proj = engine::perspective(0.87f, static_cast<float>(r.w) / static_cast<float>(r.h),
                                                  0.1f, 50.0f);
    engine::particle_draw_uniforms u{};
    u.clip_from_world = proj * view;
    u.right = engine::camera_right(view);
    u.up = engine::camera_up(view);
    u.size = 0.03f;
    u.alpha = 1.0f;
    u.intensity = 6.0f;
    u.seed = 0x51ED270Bu;
    return u;
}

struct graph_decl
{
    step_ctx steps[8]{};
    compact_ctx compact{};
    draw_ctx draw{};
    int pass_step[8]{};
    int pass_clear = -1;
    int pass_compact = -1;
    int pass_draw = -1;
    int step_count = 0;
};

/// The particle frame, declared. Nothing here says what order anything runs in,
/// which pass may cycle what, or which buffers a compute pass begins with.
bool declare_particles(engine::frame_graph& fg, graph_decl& d, rig& r,
                       const std::vector<step_uniforms>& steps)
{
    const engine::fg_buffer state0 = fg.import_buffer("particle state", r.particles.state(),
                                                      r.particles.state_bytes());
    const engine::fg_buffer alive0 = fg.import_buffer("alive list", r.particles.alive(),
                                                      r.particles.capacity() * 4u);
    const engine::fg_buffer args0 = fg.import_buffer("draw args", r.particles.args(), 16u);

    engine::fg_texture_desc hdr_desc{};
    hdr_desc.width = static_cast<Uint32>(r.w);
    hdr_desc.height = static_cast<Uint32>(r.h);
    hdr_desc.format = engine::k_hdr_format;
    const engine::fg_texture hdr0 = fg.import("hdr", r.hdr.handle(), hdr_desc);
    engine::fg_texture_desc depth_desc = hdr_desc;
    depth_desc.format = r.depth_format;
    depth_desc.depth = true;
    depth_desc.sampled = false;
    const engine::fg_texture depth0 = fg.create("depth", depth_desc);

    engine::fg_buffer state = state0;
    d.step_count = static_cast<int>(steps.size());
    for (int k = 0; k < d.step_count; ++k)
    {
        static const char* names[8] = {"step 0", "step 1", "step 2", "step 3",
                                       "step 4", "step 5", "step 6", "step 7"};
        d.steps[k].p = &r.particles;
        d.steps[k].u = steps[static_cast<std::size_t>(k)];
        d.pass_step[k] = fg.add_compute_pass(names[k], &exec_step, &d.steps[k]);
        state = fg.keep_buffer(d.pass_step[k], state, 0);   // read-modify-write
    }

    d.pass_clear = fg.add_compute_pass("clear args", &exec_clear,
                                       const_cast<engine::gpu_particles*>(&r.particles));
    const engine::fg_buffer args1 = fg.write_buffer(d.pass_clear, args0, 0);   // overwritten

    d.compact.p = &r.particles;
    d.compact.state = state;
    d.pass_compact = fg.add_compute_pass("compact", &exec_compact, &d.compact);
    fg.read_buffer(d.pass_compact, state);
    const engine::fg_buffer args2 = fg.keep_buffer(d.pass_compact, args1, 0);   // accumulated
    const engine::fg_buffer alive1 = fg.write_buffer(d.pass_compact, alive0, 1); // overwritten

    d.draw.p = &r.particles;
    d.draw.u = camera_uniforms_for(r);
    d.draw.state = state;
    d.pass_draw = fg.add_pass("particles", &exec_draw, &d.draw);
    fg.read_buffer(d.pass_draw, state);
    fg.read_buffer(d.pass_draw, alive1);
    fg.read_buffer(d.pass_draw, args2);
    (void)fg.clear(d.pass_draw, hdr0, SDL_FColor{0.0f, 0.0f, 0.0f, 1.0f});
    (void)fg.clear_depth(d.pass_draw, depth0, 1.0f);
    return d.pass_draw >= 0;
}

std::vector<Uint16> download_hdr(rig& r)
{
    SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cb);
    SDL_GPUTextureRegion src{};
    src.texture = r.hdr.handle();
    src.w = static_cast<Uint32>(r.w);
    src.h = static_cast<Uint32>(r.h);
    src.d = 1;
    SDL_GPUTextureTransferInfo dst{};
    dst.transfer_buffer = r.readback;
    dst.pixels_per_row = static_cast<Uint32>(r.w);
    dst.rows_per_layer = static_cast<Uint32>(r.h);
    SDL_DownloadFromGPUTexture(copy, &src, &dst);
    SDL_EndGPUCopyPass(copy);
    submit_and_wait(r, cb);

    std::vector<Uint16> px(static_cast<std::size_t>(r.w * r.h * 4));
    const void* mapped = SDL_MapGPUTransferBuffer(r.gpu.handle(), r.readback, false);
    if (mapped != nullptr)
    {
        std::memcpy(px.data(), mapped, px.size() * 2u);
        SDL_UnmapGPUTransferBuffer(r.gpu.handle(), r.readback);
    }
    return px;
}

/// Draw the pool as it stands, by hand: the control for the graph.
void draw_by_hand(rig& r, SDL_GPUCommandBuffer* cb, SDL_GPUBuffer* state, float depth_clear = 1.0f)
{
    SDL_GPUColorTargetInfo ct{};
    ct.texture = r.hdr.handle();
    ct.load_op = SDL_GPU_LOADOP_CLEAR;
    ct.store_op = SDL_GPU_STOREOP_STORE;
    ct.clear_color = SDL_FColor{0.0f, 0.0f, 0.0f, 1.0f};
    SDL_GPUDepthStencilTargetInfo dt{};
    dt.texture = r.depth.handle();
    dt.clear_depth = depth_clear;
    dt.load_op = SDL_GPU_LOADOP_CLEAR;
    dt.store_op = SDL_GPU_STOREOP_DONT_CARE;
    dt.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    dt.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cb, &ct, 1, &dt);
    r.particles.record_draw(cb, pass, camera_uniforms_for(r), state);
    SDL_EndGPURenderPass(pass);
}

struct image_diff
{
    Uint32 channels_differ = 0;
    Uint32 over_1ulp = 0;       ///< differ by more than one half-float ULP
    Uint32 over_4ulp = 0;
    Uint32 worst_ulp = 0;       ///< in half-float units at that value
    Uint32 pixels_lit = 0;
    double energy = 0.0;
    double energy_other = 0.0;
};

image_diff diff_hdr(const std::vector<Uint16>& a, const std::vector<Uint16>& b)
{
    image_diff d{};
    for (std::size_t i = 0; i < a.size(); i += 4)
    {
        bool lit = false;
        for (std::size_t c = 0; c < 3; ++c)
        {
            const float x = engine::half_to_float(a[i + c]);
            const float y = engine::half_to_float(b[i + c]);
            d.energy += static_cast<double>(x);
            lit = lit || x > 0.0f;
            d.energy_other += static_cast<double>(y);
            if (a[i + c] != b[i + c])
            {
                // Positive halfs order like their bit patterns, so the ULP distance
                // is the difference of the bits — the honest unit for a sum whose
                // every addition rounded to eleven significant bits.
                ++d.channels_differ;
                const Uint32 du = static_cast<Uint32>(std::abs(static_cast<int>(a[i + c])
                                                               - static_cast<int>(b[i + c])));
                d.over_1ulp += (du > 1u) ? 1u : 0u;
                d.over_4ulp += (du > 4u) ? 1u : 0u;
                d.worst_ulp = std::max(d.worst_ulp, du);
            }
        }
        d.pixels_lit += lit ? 1u : 0u;
    }
    return d;
}

void set_pool(rig& r, const std::vector<particle>& ps)
{
    SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
    (void)r.particles.set_state(cb, ps);
    submit_and_wait(r, cb);
}

void section_j(rig& r)
{
    section("§J  the frame graph learns buffers: derived cycles, refusals, the same frame twice");

    particle_pool cpu(k_pool);
    emitter_settings e{};
    for (int i = 0; i < 60; ++i) { (void)cpu.step(e, k_h); }
    const std::vector<particle> start(cpu.particles().begin(), cpu.particles().end());
    std::vector<step_uniforms> steps;
    for (int i = 0; i < 3; ++i) { steps.push_back(cpu.step(e, k_h)); }
    const std::vector<particle> want(cpu.particles().begin(), cpu.particles().end());

    engine::frame_graph fg;
    graph_decl d{};
    fg.reset();
    check(declare_particles(fg, d, r, steps), "the particle frame declares");
    check(fg.compile(r.gpu), "and compiles");

    std::printf("  schedule:");
    for (int i = 0; i < fg.live_pass_count(); ++i) { std::printf(" [%s]", fg.pass_name(fg.scheduled(i))); }
    std::printf("\n");
    bool order_ok = fg.live_pass_count() == 6;
    const int want_order[6] = {d.pass_step[0], d.pass_step[1], d.pass_step[2], d.pass_clear,
                               d.pass_compact, d.pass_draw};
    for (int i = 0; i < 6 && order_ok; ++i) { order_ok = fg.scheduled(i) == want_order[i]; }
    check(order_ok, "three steps, then clear, compact and draw — derived from reads and writes alone");

    auto cycle_of = [&](int pass, int slot) {
        for (int i = 0; i < fg.access_count(pass); ++i)
        {
            const auto a = fg.access_at(pass, i);
            if (a.use == engine::fg_use::buffer_write && static_cast<int>(a.slot) == slot) { return a.cycle ? 1 : 0; }
        }
        return -1;
    };
    std::printf("  derived cycle flags: step state %d, clear args %d, compact args %d, compact alive %d\n",
                cycle_of(d.pass_step[0], 0), cycle_of(d.pass_clear, 0), cycle_of(d.pass_compact, 0),
                cycle_of(d.pass_compact, 1));
    check(cycle_of(d.pass_step[0], 0) == 0 && cycle_of(d.pass_clear, 0) == 1
              && cycle_of(d.pass_compact, 0) == 0 && cycle_of(d.pass_compact, 1) == 1,
          "keep -> cycle false (state, args accumulated); discard -> cycle true (args cleared, list)");
    engine::dump_frame_graph(fg);

    // ---- The declared frame against the hand-recorded one ----------------------
    set_pool(r, start);
    {
        SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
        fg.execute(cb);
        submit_and_wait(r, cb);
    }
    const std::vector<particle> by_graph = download<particle>(r, r.particles.state(), k_pool);
    const std::vector<Uint16> img_graph = download_hdr(r);

    set_pool(r, start);
    {
        SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
        r.particles.simulate(cb, steps);
        draw_by_hand(r, cb, r.particles.state());
        submit_and_wait(r, cb);
    }
    const std::vector<particle> by_hand = download<particle>(r, r.particles.state(), k_pool);
    const std::vector<Uint16> img_hand = download_hdr(r);

    check(std::memcmp(by_graph.data(), by_hand.data(), by_graph.size() * sizeof(particle)) == 0,
          "the declared frame's pool equals the hand-recorded frame's, byte for byte");
    const comparison vs_cpu = compare_pools(want, by_graph);
    check(vs_cpu.alive_mismatch == 0 && vs_cpu.serial_mismatch == 0,
          "and both are the CPU's three steps");
    const image_diff id = diff_hdr(img_graph, img_hand);
    std::printf("  the two frames' HDR images: %u lit pixels, %u of %u channels differ "
                "(%u by more than 1 ULP, %u by more than 4), worst %u half-float ULPs; "
                "energy %.3f vs %.3f\n",
                id.pixels_lit, id.channels_differ, static_cast<unsigned>(img_graph.size() / 4 * 3),
                id.over_1ulp, id.over_4ulp, id.worst_ulp, id.energy, id.energy_other);
    check(id.pixels_lit > 1000, "the frame drew sparks");
    check(std::fabs(id.energy - id.energy_other) < 1e-3 * id.energy,
          "and the two images carry the same light to a thousandth: only the order of the adds moved");

    // ---- What the graph refuses ------------------------------------------------
    engine::fg_texture_desc tiny{};
    tiny.width = 4;
    tiny.height = 4;
    tiny.format = engine::k_hdr_format;
    {
        engine::frame_graph g;
        const engine::fg_buffer b0 = g.import_buffer("buf", r.particles.args(), 16);
        const engine::fg_texture t0 = g.import("out", r.hdr.handle(), tiny);
        const int p = g.add_pass("render that writes", nullptr, nullptr);
        (void)g.write_buffer(p, b0, 0);
        (void)g.clear(p, t0, SDL_FColor{0, 0, 0, 1});
        check(!g.compile(r.gpu), "refused: a render pass that writes a buffer");
    }
    {
        engine::frame_graph g;
        const engine::fg_buffer b0 = g.import_buffer("buf", r.particles.args(), 16);
        const engine::fg_texture t0 = g.import("out", r.hdr.handle(), tiny);
        const int p = g.add_compute_pass("compute that attaches", nullptr, nullptr);
        (void)g.write_buffer(p, b0, 0);
        (void)g.clear(p, t0, SDL_FColor{0, 0, 0, 1});
        check(!g.compile(r.gpu), "refused: a compute pass with a colour attachment");
    }
    {
        engine::frame_graph g;
        const engine::fg_buffer a0 = g.import_buffer("a", r.particles.args(), 16);
        const engine::fg_buffer b0 = g.import_buffer("b", r.particles.alive(), 16);
        const int p = g.add_compute_pass("gap", nullptr, nullptr);
        (void)g.write_buffer(p, a0, 0);
        (void)g.write_buffer(p, b0, 2);
        check(!g.compile(r.gpu), "refused: read-write slots 0 and 2, with nothing in 1");
    }
    {
        engine::frame_graph g;
        const engine::fg_buffer a0 = g.import_buffer("a", r.particles.args(), 16);
        const engine::fg_buffer b0 = g.import_buffer("b", r.particles.alive(), 16);
        const int p = g.add_compute_pass("twice", nullptr, nullptr);
        (void)g.write_buffer(p, a0, 0);
        (void)g.write_buffer(p, b0, 0);
        check(!g.compile(r.gpu), "refused: two buffers in read-write slot 0");
    }
    {
        engine::frame_graph g;
        const engine::fg_buffer a0 = g.import_buffer("a", r.particles.args(), 16);
        const engine::fg_buffer b0 = g.import_buffer("b", r.particles.alive(), 16);
        const int p = g.add_compute_pass("fine", nullptr, nullptr);
        (void)g.write_buffer(p, a0, 0);
        (void)g.keep_buffer(p, b0, 1);
        check(g.compile(r.gpu), "CONTROL: slots 0 and 1 compile");
    }

    // ---- Culling: the simulation survives an unused draw ----------------------
    {
        engine::frame_graph g;
        graph_decl dd{};
        // Declare, then make the draw write a TRANSIENT nobody reads.
        const engine::fg_buffer st0 = g.import_buffer("particle state", r.particles.state(), r.particles.state_bytes());
        const int s0 = g.add_compute_pass("step", &exec_step, &dd.steps[0]);
        const engine::fg_buffer st1 = g.keep_buffer(s0, st0, 0);
        const engine::fg_texture t0 = g.create("scratch", tiny);
        const int dr = g.add_pass("draw into nothing", nullptr, nullptr);
        g.read_buffer(dr, st1);
        (void)g.clear(dr, t0, SDL_FColor{0, 0, 0, 1});
        check(g.compile(r.gpu), "a frame whose draw feeds nothing compiles");
        check(g.pass_alive(s0) && !g.pass_alive(dr),
              "the step survives (it writes an import) and the draw is culled");
    }
}

// ===========================================================================
//  §K — the draw
// ===========================================================================

void write_ppm(const char* name, const std::vector<Uint16>& px, int w, int h)
{
    FILE* f = open_data(name);
    if (f == nullptr) { return; }
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (std::size_t i = 0; i < px.size(); i += 4)
    {
        for (int c = 0; c < 3; ++c)
        {
            float v = engine::half_to_float(px[i + static_cast<std::size_t>(c)]);
            v = v / (1.0f + v);                                   // Reinhard, for the page only
            v = (v <= 0.0031308f) ? 12.92f * v : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
            std::fputc(static_cast<int>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f), f);
        }
    }
    std::fclose(f);
}

void section_k(rig& r)
{
    section("§K  the draw: billboards, additive light, depth, and the order that does not matter");

    particle_pool cpu(k_pool);
    emitter_settings e{};
    for (int i = 0; i < 150; ++i) { (void)cpu.step(e, k_h); }
    const std::vector<particle> ps(cpu.particles().begin(), cpu.particles().end());
    set_pool(r, ps);

    // Three draws of ONE state, each after its own compaction: the order of the
    // list is whatever the atomics produced, and the three may differ.
    std::vector<Uint16> img[3];
    for (int t = 0; t < 3; ++t)
    {
        SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
        r.particles.count_living(cb, r.particles.state());
        draw_by_hand(r, cb, r.particles.state());
        submit_and_wait(r, cb);
        img[t] = download_hdr(r);
    }
    const image_diff d01 = diff_hdr(img[0], img[1]);
    const image_diff d02 = diff_hdr(img[0], img[2]);
    std::printf("  one state, three compactions, three draws: %u lit pixels, energy %.3f / %.3f; "
                "channels differing %u and %u of %u (%u and %u by more than 1 ULP), worst %u ULPs\n",
                d01.pixels_lit, d01.energy, d01.energy_other, d01.channels_differ, d02.channels_differ,
                static_cast<unsigned>(img[0].size() / 4 * 3), d01.over_1ulp, d02.over_1ulp,
                std::max(d01.worst_ulp, d02.worst_ulp));
    check(d01.channels_differ > 0, "the order of the list changes the last bits of the picture");
    check(std::fabs(d01.energy - d01.energy_other) < 1e-3 * d01.energy,
          "and not the light it carries: the totals agree to a thousandth");
    if (FILE* f = open_data("k_order.csv"))
    {
        // Every differing channel's ULP distance, for the figure's histogram.
        std::fprintf(f, "ulp\n");
        for (std::size_t i = 0; i < img[0].size(); i += 4)
        {
            for (std::size_t c = 0; c < 3; ++c)
            {
                if (img[0][i + c] != img[1][i + c])
                {
                    std::fprintf(f, "%d\n", std::abs(static_cast<int>(img[0][i + c])
                                                    - static_cast<int>(img[1][i + c])));
                }
            }
        }
        std::fclose(f);
    }
    write_ppm("k_frame.ppm", img[0], r.w, r.h);

    // ---- The CPU path: the same particles, uploaded -------------------------
    {
        SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
        SDL_GPUBuffer* up = r.particles.upload_cpu_state(r.gpu, cb, ps);
        r.particles.count_living(cb, up);
        draw_by_hand(r, cb, up);
        submit_and_wait(r, cb);
        const std::vector<Uint16> cpu_img = download_hdr(r);
        const image_diff dc = diff_hdr(img[0], cpu_img);
        std::printf("  the same pool through the CPU path (uploaded, %u bytes): %u channels differ, "
                    "worst %u ULPs, energy %.3f vs %.3f\n", r.particles.state_bytes(),
                    dc.channels_differ, dc.worst_ulp, dc.energy, dc.energy_other);
        check(std::fabs(dc.energy - dc.energy_other) < 1e-3 * dc.energy,
              "the CPU path draws the same light: only the producer differs");
    }

    // ---- Depth: a 'wall' as a cleared depth value ------------------------------
    //
    // Clearing the depth attachment to the device depth of a plane d metres from
    // the camera stands in for a wall there. The first version put it at 4 m and
    // predicted nothing would show; 604 pixels did, because sparks thrown toward
    // the camera land up to four metres out and some are nearer than the wall.
    // So the wall is placed from the pool itself: just in front of the nearest spark.
    {
        float zmax = -1e9f;
        for (const particle& p : ps) { if (engine::is_alive(p)) { zmax = std::max(zmax, p.position.z); } }
        const engine::mat4 proj = engine::perspective(0.87f, static_cast<float>(r.w) / static_cast<float>(r.h),
                                                      0.1f, 50.0f);
        auto device_depth = [&](float metres) {
            const engine::vec4 c = proj * engine::vec4{0.0f, 0.0f, -metres, 1.0f};
            return c.z / c.w;
        };
        const float near_all = 6.0f - zmax - 0.2f;   // camera at z = 6, looking down -z
        for (const float d : {near_all, 6.0f})
        {
            SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
            r.particles.count_living(cb, r.particles.state());
            draw_by_hand(r, cb, r.particles.state(), device_depth(d));
            submit_and_wait(r, cb);
            const std::vector<Uint16> walled = download_hdr(r);
            const image_diff dw = diff_hdr(walled, walled);
            // Behind the wall, by count: sparks farther than d from the camera.
            int behind = 0;
            int alive_n = 0;
            for (const particle& p : ps)
            {
                if (!engine::is_alive(p)) { continue; }
                ++alive_n;
                const float dz = 6.0f - p.position.z;   // the camera looks down -z from z = 6
                behind += (dz > d) ? 1 : 0;
            }
            std::printf("  a depth wall %.2f m out (device depth %.5f; nearest spark %.2f m out): "
                        "%u lit pixels (was %u), light %.1f%% of the unwalled frame; "
                        "%d of %d sparks lie behind it\n", static_cast<double>(d),
                        static_cast<double>(device_depth(d)), static_cast<double>(6.0f - zmax),
                        dw.pixels_lit, d01.pixels_lit, 100.0 * dw.energy / d01.energy,
                        behind, alive_n);
            if (d == near_all) { check(dw.pixels_lit == 0, "a wall in front of every spark hides every spark"); }
            else               { check(dw.energy < 0.7 * d01.energy && dw.energy > 0.3 * d01.energy,
                                       "a wall through the fountain's axis removes about half the light"); }
        }
    }
}

// ===========================================================================
//  §L — the budget
// ===========================================================================

void section_l(rig& r)
{
    section("§L  the budget: a CPU step and its upload, against a GPU step");
#if !defined(ENGINE_LIB_BUILD_TYPE)
#define ENGINE_LIB_BUILD_TYPE "unknown"
#endif
    // The CPU step is LIBRARY code, so what matters is how libengine.a was built,
    // not how this file was (scratch/build_verify_618b.sh explains).
    const bool release = std::strcmp(ENGINE_LIB_BUILD_TYPE, "Release") == 0;
    std::printf("  libengine.a built as '%s': %s\n", ENGINE_LIB_BUILD_TYPE,
                release ? "these are the numbers the lesson quotes"
                        : "the CPU column is a DEBUG library's and must not be quoted");

    FILE* f = open_data("l_budget.csv");
    if (f != nullptr) { std::fprintf(f, "capacity,alive,cpu_ms,upload_ms,gpu_ms,bytes\n"); }

    for (const Uint32 cap : {16384u, 65536u, 262144u, 1048576u})
    {
        engine::gpu_particles gp;
        if (!gp.create(r.gpu, cap, r.vert.handle(), r.frag.handle(), engine::k_hdr_format, r.depth_format))
        {
            std::printf("  capacity %u: could not create\n", cap);
            continue;
        }

        // Fill to steady state: a rate that keeps about 69% of the pool alive.
        emitter_settings e{};
        e.rate = 0.3f * static_cast<float>(cap);
        particle_pool cpu(cap);
        for (int i = 0; i < 240; ++i) { (void)cpu.step(e, k_h); }
        const std::vector<particle> warm(cpu.particles().begin(), cpu.particles().end());
        {
            SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
            (void)gp.set_state(cb, warm);
            submit_and_wait(r, cb);
        }

        // CPU: sixty steps, timed as a block; the result is consumed.
        std::vector<step_uniforms> steps;
        const double c0 = now_ms();
        for (int i = 0; i < 60; ++i) { steps.push_back(cpu.step(e, k_h)); }
        const double c1 = now_ms();
        const Uint32 alive = cpu.alive_count();
        const double cpu_ms = (c1 - c0) / 60.0;

        // UPLOAD: what the CPU path pays the bus every frame, measured as
        // sixty uploads in sixty command buffers, each waited for.
        const double u0 = now_ms();
        for (int i = 0; i < 60; ++i)
        {
            SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
            (void)gp.upload_cpu_state(r.gpu, cb, cpu.particles());
            submit_and_wait(r, cb);
        }
        const double u1 = now_ms();
        const double upload_ms = (u1 - u0) / 60.0;

        // GPU: the same sixty steps in one command buffer, plus the compaction.
        SDL_GPUCommandBuffer* warmcb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
        gp.simulate(warmcb, std::span<const step_uniforms>(steps.data(), 1));
        submit_and_wait(r, warmcb);
        const double g0 = now_ms();
        SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(r.gpu.handle());
        gp.simulate(cb, steps);
        submit_and_wait(r, cb);
        const double g1 = now_ms();
        const double gpu_ms = (g1 - g0) / 60.0;

        const Uint32 bytes = cap * 48u;
        std::printf("  %8u slots (%7u alive): CPU step %8.3f ms (%5.2f ns/slot)   upload %.2f MB %7.3f ms"
                    "   GPU step %7.3f ms (%5.3f ns/slot)   %6.1fx\n",
                    cap, alive, cpu_ms, 1e6 * cpu_ms / cap, bytes / 1048576.0, upload_ms,
                    gpu_ms, 1e6 * gpu_ms / cap, (cpu_ms + upload_ms) / std::max(gpu_ms, 1e-6));
        if (f != nullptr)
        {
            std::fprintf(f, "%u,%u,%.4f,%.4f,%.4f,%u\n", cap, alive, cpu_ms, upload_ms, gpu_ms, bytes);
        }
        gp.destroy();
    }
    if (f != nullptr) { std::fclose(f); }
}

} // namespace

// ===========================================================================
//  main
// ===========================================================================

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    std::printf("Lesson 6.18b — Compute Shaders: GPU Particles\n");

    SDL_CreateDirectory(k_data_dir);

    if (!SDL_Init(SDL_INIT_VIDEO)) { std::printf("  SDL_Init(VIDEO): %s\n", SDL_GetError()); }
    rig r;
    const bool have_gpu = build_rig(r);
    if (have_gpu) { std::printf("\n  GPU: driver '%s'\n", SDL_GetGPUDeviceDriver(r.gpu.handle())); }
    else          { std::printf("\n  no GPU device: the device halves are skipped\n"); }

    section_a(have_gpu ? &r : nullptr);
    section_b();
    section_c(have_gpu ? &r : nullptr);
    section_d();
    section_e();
    section_f();
    if (have_gpu)
    {
        section_g(r);
        section_h(r);
        section_i(r);
        section_j(r);
        section_k(r);
        section_l(r);
    }

    std::printf("\n===========================================================================\n");
    std::printf("  %d checks, %d failure(s)\n", g_checks, g_failures);
    std::printf("===========================================================================\n");
    return g_failures == 0 ? 0 : 1;
}
