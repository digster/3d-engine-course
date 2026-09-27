// engine/src/gfx/frame_graph.cpp — Lesson 6.17.
//
// The whole compiler is four passes over two small arrays, and each one is a
// paragraph of the lesson:
//
//   1. VALIDATE   every version has exactly one producer, or a good reason not to
//   2. CULL       keep what transitively feeds an imported write
//   3. ORDER      Kahn's algorithm, deterministic, cycle-detecting
//   4. DERIVE     store ops, lifetimes, and a pooled texture per transient
//
// Nothing here is clever, and that is deliberate: the value of a frame graph is
// in what it makes impossible to get wrong, not in the sophistication of the
// scheduler. A scheduler that reorders passes in ways the author cannot predict
// is a debugging problem, not a feature.

#include <engine/gfx/frame_graph.hpp>

#include <engine/core/log.hpp>

#include <SDL3/SDL.h>

#include <cstring>

namespace engine {

namespace {

/// Bytes per texel for the formats this engine renders into.
///
/// Deliberately a short, explicit table rather than a general one. SDL_GPU has
/// over eighty formats and this engine creates targets in five; a table that
/// pretends to know all eighty would be wrong in ways nobody would notice until
/// a memory figure was quoted in a lesson. An unknown format returns 0 and the
/// memory accounting says so instead of guessing.
std::size_t texel_bytes(SDL_GPUTextureFormat f)
{
    switch (f)
    {
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM:
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB:
    case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM:
    case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB:
    case SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT:
    case SDL_GPU_TEXTUREFORMAT_D32_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT:
        return 4;
    case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT:
        return 8;
    case SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT:
        return 16;
    case SDL_GPU_TEXTUREFORMAT_D16_UNORM:
        return 2;
    default:
        return 0;
    }
}

int sample_multiplier(SDL_GPUSampleCount s)
{
    switch (s)
    {
    case SDL_GPU_SAMPLECOUNT_2: return 2;
    case SDL_GPU_SAMPLECOUNT_4: return 4;
    case SDL_GPU_SAMPLECOUNT_8: return 8;
    default: return 1;
    }
}

const char* load_op_name(SDL_GPULoadOp op)
{
    switch (op)
    {
    case SDL_GPU_LOADOP_LOAD: return "LOAD";
    case SDL_GPU_LOADOP_CLEAR: return "CLEAR";
    default: return "DONT_CARE";
    }
}

const char* store_op_name(SDL_GPUStoreOp op)
{
    switch (op)
    {
    case SDL_GPU_STOREOP_STORE: return "STORE";
    case SDL_GPU_STOREOP_RESOLVE: return "RESOLVE";
    case SDL_GPU_STOREOP_RESOLVE_AND_STORE: return "RESOLVE_AND_STORE";
    default: return "DONT_CARE";
    }
}

const char* use_name(fg_use u)
{
    switch (u)
    {
    case fg_use::sample: return "sample";
    case fg_use::colour: return "colour";
    default: return "depth";
    }
}

} // namespace

// ===========================================================================
//  Descriptors
// ===========================================================================

bool fg_texture_desc::matches(const fg_texture_desc& o) const
{
    return width == o.width && height == o.height && format == o.format
        && samples == o.samples && layers == o.layers && depth == o.depth
        && sampled == o.sampled;
}

std::size_t fg_texture_desc::bytes() const
{
    const std::size_t per = texel_bytes(format);
    return per * static_cast<std::size_t>(width) * static_cast<std::size_t>(height)
         * static_cast<std::size_t>(layers)
         * static_cast<std::size_t>(sample_multiplier(samples));
}

// ===========================================================================
//  Declaration
// ===========================================================================

frame_graph::~frame_graph()
{
    destroy();
}

void frame_graph::reset()
{
    pass_count_ = 0;
    resource_count_ = 0;
    live_count_ = 0;
    naive_bytes_ = 0;
    pooled_bytes_ = 0;
    compiled_ = false;
}

void frame_graph::destroy()
{
    for (gpu_texture& t : pool_) { t.destroy(); }
    pool_size_ = 0;
    reset();
}

fg_texture frame_graph::create(const char* name, const fg_texture_desc& desc)
{
    if (resource_count_ >= k_max_fg_resources)
    {
        ENGINE_LOG_ERROR(log_gpu, "frame_graph: out of resource slots declaring '%s' (cap %d)",
                         name, k_max_fg_resources);
        return fg_texture{};
    }

    resource& r = resources_[resource_count_];
    r = resource{};
    r.name = name;
    r.desc = desc;
    r.imported = nullptr;
    r.versions = 0;

    fg_texture h{};
    h.index = static_cast<Uint16>(resource_count_);
    h.version = 0;
    ++resource_count_;
    return h;
}

fg_texture frame_graph::import(const char* name, SDL_GPUTexture* texture,
                               const fg_texture_desc& desc)
{
    const fg_texture h = create(name, desc);
    if (!h.valid()) { return h; }
    resources_[h.index].imported = texture;
    return h;
}

int frame_graph::add_pass(const char* name, fg_execute_fn fn, void* user)
{
    if (pass_count_ >= k_max_fg_passes)
    {
        ENGINE_LOG_ERROR(log_gpu, "frame_graph: out of pass slots adding '%s' (cap %d)",
                         name, k_max_fg_passes);
        return -1;
    }

    pass& p = passes_[pass_count_];
    p = pass{};
    p.name = name;
    p.fn = fn;
    p.user = user;
    return pass_count_++;
}

void frame_graph::sample(int pass, fg_texture h)
{
    if (pass < 0 || pass >= pass_count_ || !h.valid()) { return; }

    struct pass& p = passes_[pass];
    if (p.access_count >= k_max_fg_accesses)
    {
        ENGINE_LOG_ERROR(log_gpu, "frame_graph: pass '%s' exceeded %d accesses",
                         p.name, k_max_fg_accesses);
        return;
    }

    access& a = p.accesses[p.access_count++];
    a = access{};
    a.resource = h.index;
    a.version_in = h.version;
    a.version_out = h.version;   // a read produces nothing
    a.use = fg_use::sample;
    a.init = fg_init::keep;      // unused for a sample; `keep` is the honest word
}

fg_texture frame_graph::write(int pass, fg_texture h, fg_use use, fg_init init,
                              Uint32 layer, SDL_FColor colour, float depth)
{
    if (pass < 0 || pass >= pass_count_ || !h.valid()) { return fg_texture{}; }

    struct pass& p = passes_[pass];
    if (p.access_count >= k_max_fg_accesses)
    {
        ENGINE_LOG_ERROR(log_gpu, "frame_graph: pass '%s' exceeded %d accesses",
                         p.name, k_max_fg_accesses);
        return fg_texture{};
    }

    resource& r = resources_[h.index];

    // THE VERSION BUMP, AND WHY IT IS ON THE RESOURCE AND NOT THE HANDLE.
    // The new version must be unique across the whole frame, so it counts writes
    // to the resource rather than adding one to the handle the caller passed. The
    // difference shows up only when somebody writes an OLD version — `hdr@0`
    // after `hdr@1` already exists — which is a real mistake (it means "throw
    // away the scene and start again") and which this makes visible as a second
    // producer rather than as two passes both claiming `hdr@1`.
    const Uint16 out = static_cast<Uint16>(r.versions + 1);
    r.versions = out;

    access& a = p.accesses[p.access_count++];
    a = access{};
    a.resource = h.index;
    a.version_in = h.version;
    a.version_out = out;
    a.use = use;
    a.init = init;
    a.layer = layer;
    a.clear_colour = colour;
    a.clear_depth = depth;

    fg_texture result{};
    result.index = h.index;
    result.version = out;
    return result;
}

fg_texture frame_graph::discard_write(int pass, fg_texture h, Uint32 layer)
{
    return write(pass, h, fg_use::colour, fg_init::discard, layer,
                 SDL_FColor{0.0f, 0.0f, 0.0f, 1.0f}, 1.0f);
}

fg_texture frame_graph::clear(int pass, fg_texture h, SDL_FColor colour, Uint32 layer)
{
    return write(pass, h, fg_use::colour, fg_init::clear, layer, colour, 1.0f);
}

fg_texture frame_graph::keep(int pass, fg_texture h, Uint32 layer)
{
    return write(pass, h, fg_use::colour, fg_init::keep, layer,
                 SDL_FColor{0.0f, 0.0f, 0.0f, 1.0f}, 1.0f);
}

fg_texture frame_graph::clear_depth(int pass, fg_texture h, float value, Uint32 layer)
{
    return write(pass, h, fg_use::depth, fg_init::clear, layer,
                 SDL_FColor{0.0f, 0.0f, 0.0f, 1.0f}, value);
}

fg_texture frame_graph::keep_depth(int pass, fg_texture h, Uint32 layer)
{
    return write(pass, h, fg_use::depth, fg_init::keep, layer,
                 SDL_FColor{0.0f, 0.0f, 0.0f, 1.0f}, 1.0f);
}

fg_texture frame_graph::discard_depth(int pass, fg_texture h, Uint32 layer)
{
    return write(pass, h, fg_use::depth, fg_init::discard, layer,
                 SDL_FColor{0.0f, 0.0f, 0.0f, 1.0f}, 1.0f);
}

fg_texture frame_graph::resolve(int pass, fg_texture target, fg_texture resolved)
{
    if (pass < 0 || pass >= pass_count_ || !target.valid() || !resolved.valid())
    {
        return fg_texture{};
    }

    struct pass& p = passes_[pass];
    for (int i = 0; i < p.access_count; ++i)
    {
        access& a = p.accesses[i];
        if (a.use != fg_use::colour || a.resource != target.index
            || a.version_out != target.version)
        {
            continue;
        }

        resource& dst = resources_[resolved.index];
        const Uint16 out = static_cast<Uint16>(dst.versions + 1);
        dst.versions = out;

        a.resolve_resource = resolved.index;
        a.resolve_version_out = out;

        fg_texture result{};
        result.index = resolved.index;
        result.version = out;
        return result;
    }

    ENGINE_LOG_ERROR(log_gpu,
                     "frame_graph: pass '%s' has no colour write of '%s'@%u to resolve",
                     p.name, resources_[target.index].name,
                     static_cast<unsigned>(target.version));
    return fg_texture{};
}

// ===========================================================================
//  Compilation
// ===========================================================================

int frame_graph::producer_of(int res, int version) const
{
    if (version == 0) { return -1; }   // version 0 has no producer, by definition

    for (int p = 0; p < pass_count_; ++p)
    {
        const pass& pp = passes_[p];
        for (int i = 0; i < pp.access_count; ++i)
        {
            const access& a = pp.accesses[i];
            if (a.use != fg_use::sample && a.resource == res
                && a.version_out == static_cast<Uint16>(version))
            {
                return p;
            }
            if (a.resolve_resource == static_cast<Uint16>(res)
                && a.resolve_version_out == static_cast<Uint16>(version))
            {
                return p;
            }
        }
    }
    return -1;
}

bool frame_graph::compile(const gpu_device& dev)
{
    const Uint64 t0 = SDL_GetPerformanceCounter();

    compiled_ = false;
    live_count_ = 0;
    naive_bytes_ = 0;
    pooled_bytes_ = 0;
    peak_bytes_ = 0;

    for (int r = 0; r < resource_count_; ++r)
    {
        resources_[r].first_use = -1;
        resources_[r].last_use = -1;
        resources_[r].pool_slot = -1;
    }
    for (int p = 0; p < pass_count_; ++p) { passes_[p].alive = false; }

    // ---- 1. Validate ------------------------------------------------------
    //
    // Three rules, and each one is a bug class that is currently silent.
    bool ok = true;
    for (int p = 0; p < pass_count_; ++p)
    {
        const pass& pp = passes_[p];
        for (int i = 0; i < pp.access_count; ++i)
        {
            const access& a = pp.accesses[i];
            const resource& r = resources_[a.resource];

            // (a) READING SOMETHING NOBODY WROTE. Version 0 of a transient is
            // undefined contents; sampling it, or LOADing it into a pass, reads
            // whatever the pool's previous tenant left behind. Today that is a
            // picture with last frame's bloom faintly in it and no error
            // anywhere.
            const bool needs_value = (a.use == fg_use::sample) || (a.init == fg_init::keep);
            if (needs_value && a.version_in == 0 && r.imported == nullptr)
            {
                ENGINE_LOG_ERROR(log_gpu,
                                 "frame_graph: pass '%s' %s '%s'@0, which nothing produced",
                                 pp.name,
                                 (a.use == fg_use::sample) ? "samples"
                                                           : "declares keep on",
                                 r.name);
                ok = false;
            }

            // (b) TWO PRODUCERS OF ONE VERSION. Cannot happen through the API —
            // `write` bumps the resource's counter — but a caller who keeps an
            // old handle and writes it twice produces two *different* versions
            // from the same input, which is an ambiguity about which one later
            // readers meant. Detected as an input version that two writes share.
            if (a.use != fg_use::sample)
            {
                for (int q = p + 1; q < pass_count_; ++q)
                {
                    const pass& qq = passes_[q];
                    for (int j = 0; j < qq.access_count; ++j)
                    {
                        const access& b = qq.accesses[j];
                        if (b.use != fg_use::sample && b.resource == a.resource
                            && b.version_in == a.version_in)
                        {
                            ENGINE_LOG_ERROR(log_gpu,
                                             "frame_graph: '%s' and '%s' both write '%s' from "
                                             "version %u - which result did later passes mean?",
                                             pp.name, qq.name, r.name,
                                             static_cast<unsigned>(a.version_in));
                            ok = false;
                        }
                    }
                }
            }
        }

        // (c) ATTACHMENTS THAT DISAGREE. Every attachment in a pass shares the
        // pass's dimensions and sample positions — Lesson 6.14's hard rule — so
        // a 4x colour target beside a 1x depth target is a pass that cannot
        // begin. The driver says so at `SDL_BeginGPURenderPass`, by which point
        // the frame is half recorded.
        const fg_texture_desc* first = nullptr;
        for (int i = 0; i < pp.access_count; ++i)
        {
            const access& a = pp.accesses[i];
            if (a.use == fg_use::sample) { continue; }
            const fg_texture_desc& d = resources_[a.resource].desc;
            if (first == nullptr) { first = &d; continue; }
            if (d.width != first->width || d.height != first->height
                || d.samples != first->samples)
            {
                ENGINE_LOG_ERROR(log_gpu,
                                 "frame_graph: pass '%s' attaches %ux%u@%dx beside %ux%u@%dx",
                                 pp.name, d.width, d.height, sample_multiplier(d.samples),
                                 first->width, first->height, sample_multiplier(first->samples));
                ok = false;
            }
        }
    }
    if (!ok) { return false; }

    // ---- 2. Cull ----------------------------------------------------------
    //
    // A pass survives iff it transitively feeds a write to an IMPORTED resource.
    // Imports are the only thing whose value outlives the frame, so a transient
    // that no import depends on was computed for nobody.
    //
    // Backward reachability, iterated to a fixed point. `pass_count_` rounds is
    // the worst case (a chain), and it is 18 here.
    for (int p = 0; p < pass_count_; ++p)
    {
        const pass& pp = passes_[p];
        for (int i = 0; i < pp.access_count; ++i)
        {
            const access& a = pp.accesses[i];
            if (a.use == fg_use::sample) { continue; }
            if (resources_[a.resource].imported != nullptr
                || (a.resolve_resource != fg_texture::k_none
                    && resources_[a.resolve_resource].imported != nullptr))
            {
                passes_[p].alive = true;
            }
        }
    }

    for (int round = 0; round < pass_count_; ++round)
    {
        bool changed = false;
        for (int p = 0; p < pass_count_; ++p)
        {
            if (!passes_[p].alive) { continue; }
            const pass& pp = passes_[p];
            for (int i = 0; i < pp.access_count; ++i)
            {
                const access& a = pp.accesses[i];
                const int src = producer_of(a.resource, a.version_in);
                if (src >= 0 && !passes_[src].alive)
                {
                    passes_[src].alive = true;
                    changed = true;
                }
            }
        }
        if (!changed) { break; }
    }

    // ---- 3. Order ---------------------------------------------------------
    //
    // Kahn's algorithm over the live subgraph. An edge runs from the pass that
    // produced a version to every pass that consumes it.
    //
    // DETERMINISM IS A REQUIREMENT, NOT A NICETY. Ties are broken by declaration
    // index, so the same declarations always produce the same schedule. A graph
    // that picks a different legal order on different runs is one whose only
    // symptom is a golden-image test failing intermittently — and by then the
    // scheduler is the last thing anyone suspects.
    int indegree[k_max_fg_passes] = {};
    for (int p = 0; p < pass_count_; ++p)
    {
        if (!passes_[p].alive) { continue; }
        const pass& pp = passes_[p];
        for (int i = 0; i < pp.access_count; ++i)
        {
            const int src = producer_of(pp.accesses[i].resource, pp.accesses[i].version_in);
            if (src >= 0 && passes_[src].alive) { ++indegree[p]; }
        }
    }

    int total_live = 0;
    for (int p = 0; p < pass_count_; ++p) { total_live += passes_[p].alive ? 1 : 0; }

    bool emitted[k_max_fg_passes] = {};
    live_count_ = 0;
    while (live_count_ < total_live)
    {
        int pick = -1;
        for (int p = 0; p < pass_count_; ++p)
        {
            if (passes_[p].alive && !emitted[p] && indegree[p] == 0) { pick = p; break; }
        }

        if (pick < 0)
        {
            // A CYCLE. Name the passes still waiting, because "the graph did not
            // compile" is useless and "these four passes are waiting on each
            // other" is the answer.
            ENGINE_LOG_ERROR(log_gpu, "frame_graph: cycle - these passes never became ready:");
            for (int p = 0; p < pass_count_; ++p)
            {
                if (passes_[p].alive && !emitted[p])
                {
                    ENGINE_LOG_ERROR(log_gpu, "frame_graph:   '%s' (waiting on %d)",
                                     passes_[p].name, indegree[p]);
                }
            }
            return false;
        }

        emitted[pick] = true;
        schedule_[live_count_++] = pick;

        for (int q = 0; q < pass_count_; ++q)
        {
            if (!passes_[q].alive || emitted[q]) { continue; }
            const pass& qq = passes_[q];
            for (int i = 0; i < qq.access_count; ++i)
            {
                if (producer_of(qq.accesses[i].resource, qq.accesses[i].version_in) == pick)
                {
                    --indegree[q];
                }
            }
        }
    }

    // ---- 4a. Store ops ----------------------------------------------------
    //
    // STORE iff some live pass consumes this version, or the resource is
    // imported — because an import's value is observable outside the frame and
    // the graph cannot know who looks at it.
    //
    // This single rule reproduces two decisions the course made by hand and
    // argued about at length: Lesson 4.7's DONT_CARE on the scene depth (nothing
    // reads it) and Lesson 6.8's STORE on the shadow map (the scene pass samples
    // it). Neither is written down anywhere below.
    for (int s = 0; s < live_count_; ++s)
    {
        pass& pp = passes_[schedule_[s]];
        for (int i = 0; i < pp.access_count; ++i)
        {
            access& a = pp.accesses[i];

            switch (a.init)
            {
            case fg_init::keep:   a.load_op = SDL_GPU_LOADOP_LOAD; break;
            case fg_init::clear:  a.load_op = SDL_GPU_LOADOP_CLEAR; break;
            default:              a.load_op = SDL_GPU_LOADOP_DONT_CARE; break;
            }

            if (a.use == fg_use::sample) { continue; }

            bool consumed = resources_[a.resource].imported != nullptr;
            for (int t = 0; t < live_count_ && !consumed; ++t)
            {
                const pass& qq = passes_[schedule_[t]];
                for (int j = 0; j < qq.access_count; ++j)
                {
                    const access& b = qq.accesses[j];
                    if (b.resource == a.resource && b.version_in == a.version_out)
                    {
                        consumed = true;
                        break;
                    }
                }
            }

            if (a.resolve_resource != fg_texture::k_none)
            {
                // RESOLVE, not RESOLVE_AND_STORE: nothing reads the per-sample
                // data, and the SDL3 header calls keeping it "not recommended".
                // Lesson 6.14 made this argument; the graph now makes it once.
                a.store_op = SDL_GPU_STOREOP_RESOLVE;
            }
            else
            {
                a.store_op = consumed ? SDL_GPU_STOREOP_STORE : SDL_GPU_STOREOP_DONT_CARE;
            }
        }
    }

    // ---- 4b. Lifetimes ----------------------------------------------------
    for (int s = 0; s < live_count_; ++s)
    {
        const pass& pp = passes_[schedule_[s]];
        for (int i = 0; i < pp.access_count; ++i)
        {
            const access& a = pp.accesses[i];
            const auto touch = [&](int res) {
                resource& r = resources_[res];
                if (r.first_use < 0) { r.first_use = s; }
                r.last_use = s;
            };
            touch(a.resource);
            if (a.resolve_resource != fg_texture::k_none) { touch(a.resolve_resource); }
        }
    }

    // ---- 4c. Pool ---------------------------------------------------------
    //
    // Descriptor-keyed reuse, which is as close to aliasing as SDL_GPU gets.
    // Resources are visited in order of FIRST USE, and a pooled texture is
    // reusable when its descriptor matches exactly and its previous tenant's
    // interval has ended.
    int slot_free_after[k_max_fg_pool];
    for (int i = 0; i < k_max_fg_pool; ++i) { slot_free_after[i] = -1; }

    int order[k_max_fg_resources];
    int n_order = 0;
    for (int r = 0; r < resource_count_; ++r)
    {
        if (resources_[r].imported != nullptr || resources_[r].first_use < 0) { continue; }
        order[n_order++] = r;
    }
    for (int i = 1; i < n_order; ++i)
    {
        const int key = order[i];
        int j = i - 1;
        while (j >= 0 && resources_[order[j]].first_use > resources_[key].first_use)
        {
            order[j + 1] = order[j];
            --j;
        }
        order[j + 1] = key;
    }

    for (int i = 0; i < n_order; ++i)
    {
        resource& r = resources_[order[i]];
        naive_bytes_ += r.desc.bytes();

        int slot = -1;
        for (int s = 0; s < pool_size_; ++s)
        {
            if (slot_free_after[s] < r.first_use && pool_[s].valid()
                && r.desc.matches(fg_texture_desc{pool_[s].width(), pool_[s].height(),
                                                  pool_[s].format(), pool_[s].samples(),
                                                  r.desc.layers, r.desc.depth, r.desc.sampled}))
            {
                slot = s;
                break;
            }
        }

        if (slot < 0)
        {
            if (pool_size_ >= k_max_fg_pool)
            {
                ENGINE_LOG_ERROR(log_gpu, "frame_graph: pool exhausted at %d textures",
                                 k_max_fg_pool);
                return false;
            }
            slot = pool_size_++;

            const bool made = r.desc.depth
                ? (r.desc.layers > 1
                       ? pool_[slot].create_depth_array(dev, r.desc.format, r.desc.width,
                                                        r.desc.height, r.desc.layers, r.name,
                                                        r.desc.sampled)
                       : pool_[slot].create_depth(dev, r.desc.format, r.desc.width,
                                                  r.desc.height, r.name, r.desc.sampled,
                                                  r.desc.samples))
                : pool_[slot].create_colour_target(dev, r.desc.format, r.desc.width,
                                                   r.desc.height, r.name, r.desc.sampled,
                                                   r.desc.samples);
            if (!made)
            {
                ENGINE_LOG_ERROR(log_gpu, "frame_graph: could not create '%s' (%ux%u)",
                                 r.name, r.desc.width, r.desc.height);
                --pool_size_;
                return false;
            }
        }

        r.pool_slot = slot;
        slot_free_after[slot] = r.last_use;
    }

    // POOLED BYTES ARE COUNTED OVER SLOTS, NOT OVER CREATIONS. The obvious
    // version — add the bytes each time a texture is created — reports the truth
    // on the first frame and a saving of one hundred per cent on every frame
    // after it, because the pool is then warm and creates nothing. That is not a
    // subtle bug; it is a measurement that only works once, which is worse than
    // one that never works, because the first run looks right.
    bool slot_counted[k_max_fg_pool] = {};
    for (int i = 0; i < n_order; ++i)
    {
        const resource& r = resources_[order[i]];
        if (r.pool_slot >= 0 && !slot_counted[r.pool_slot])
        {
            slot_counted[r.pool_slot] = true;
            pooled_bytes_ += r.desc.bytes();
        }
    }

    // And the floor: the most simultaneously-live bytes at any one point in the
    // schedule. An engine that could alias raw memory would need this much and
    // no more; the gap between it and `naive_bytes_` is the entire prize.
    for (int s = 0; s < live_count_; ++s)
    {
        std::size_t live = 0;
        for (int i = 0; i < n_order; ++i)
        {
            const resource& r = resources_[order[i]];
            if (r.first_use <= s && s <= r.last_use) { live += r.desc.bytes(); }
        }
        if (live > peak_bytes_) { peak_bytes_ = live; }
    }

    compiled_ = true;
    compile_us_ = 1e6 * static_cast<double>(SDL_GetPerformanceCounter() - t0)
                / static_cast<double>(SDL_GetPerformanceFrequency());
    return true;
}

// ===========================================================================
//  Execution
// ===========================================================================

SDL_GPUTexture* frame_graph::texture(fg_texture h) const
{
    if (!h.valid() || h.index >= resource_count_) { return nullptr; }
    const resource& r = resources_[h.index];
    if (r.imported != nullptr) { return r.imported; }
    return (r.pool_slot >= 0) ? pool_[r.pool_slot].handle() : nullptr;
}

SDL_GPUTexture* fg_pass_context::texture(fg_texture h) const
{
    return (graph != nullptr) ? graph->texture(h) : nullptr;
}

void frame_graph::execute(SDL_GPUCommandBuffer* cb) const
{
    if (!compiled_ || cb == nullptr) { return; }

    for (int s = 0; s < live_count_; ++s)
    {
        const pass& pp = passes_[schedule_[s]];

        SDL_GPUColorTargetInfo colours[k_max_fg_colours]{};
        int colour_count = 0;
        SDL_GPUDepthStencilTargetInfo depth_info{};
        bool has_depth = false;

        for (int i = 0; i < pp.access_count; ++i)
        {
            const access& a = pp.accesses[i];
            if (a.use == fg_use::sample) { continue; }

            fg_texture h{};
            h.index = a.resource;
            h.version = a.version_out;

            if (a.use == fg_use::colour)
            {
                if (colour_count >= k_max_fg_colours) { continue; }
                SDL_GPUColorTargetInfo& ci = colours[colour_count++];
                ci = SDL_GPUColorTargetInfo{};
                ci.texture = texture(h);
                ci.mip_level = 0;
                ci.layer_or_depth_plane = a.layer;
                ci.clear_color = a.clear_colour;
                ci.load_op = a.load_op;
                ci.store_op = a.store_op;
                ci.cycle = false;
                if (a.resolve_resource != fg_texture::k_none)
                {
                    fg_texture rh{};
                    rh.index = a.resolve_resource;
                    rh.version = a.resolve_version_out;
                    ci.resolve_texture = texture(rh);
                    ci.resolve_mip_level = 0;
                    ci.resolve_layer = 0;
                    ci.cycle_resolve_texture = false;
                }
            }
            else
            {
                depth_info = SDL_GPUDepthStencilTargetInfo{};
                depth_info.texture = texture(h);
                depth_info.clear_depth = a.clear_depth;
                depth_info.load_op = a.load_op;
                depth_info.store_op = a.store_op;
                depth_info.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
                depth_info.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
                depth_info.cycle = false;
                depth_info.layer = static_cast<Uint8>(a.layer);
                has_depth = true;
            }
        }

        SDL_GPURenderPass* rp = SDL_BeginGPURenderPass(
            cb, (colour_count > 0) ? colours : nullptr, static_cast<Uint32>(colour_count),
            has_depth ? &depth_info : nullptr);
        if (rp == nullptr)
        {
            ENGINE_LOG_ERROR(log_gpu, "frame_graph: '%s' could not begin: %s",
                             pp.name, SDL_GetError());
            continue;
        }

        if (pp.fn != nullptr)
        {
            fg_pass_context ctx{};
            ctx.cb = cb;
            ctx.pass = rp;
            ctx.graph = this;
            ctx.pass_index = schedule_[s];
            pp.fn(ctx, pp.user);
        }

        SDL_EndGPURenderPass(rp);
    }
}

// ===========================================================================
//  Reporting
// ===========================================================================

int frame_graph::scheduled(int i) const
{
    return (i >= 0 && i < live_count_) ? schedule_[i] : -1;
}

const char* frame_graph::pass_name(int pass) const
{
    return (pass >= 0 && pass < pass_count_) ? passes_[pass].name : "";
}

const char* frame_graph::resource_name(int res) const
{
    return (res >= 0 && res < resource_count_) ? resources_[res].name : "";
}

bool frame_graph::pass_alive(int pass) const
{
    return (pass >= 0 && pass < pass_count_) && passes_[pass].alive;
}

int frame_graph::access_count(int pass) const
{
    return (pass >= 0 && pass < pass_count_) ? passes_[pass].access_count : 0;
}

frame_graph::access_report frame_graph::access_at(int pass, int i) const
{
    access_report out{};
    if (pass < 0 || pass >= pass_count_) { return out; }
    const struct pass& pp = passes_[pass];
    if (i < 0 || i >= pp.access_count) { return out; }

    const access& a = pp.accesses[i];
    out.resource = a.resource;
    out.version_in = a.version_in;
    out.version_out = a.version_out;
    out.use = a.use;
    out.init = a.init;
    out.load_op = a.load_op;
    out.store_op = a.store_op;
    out.layer = a.layer;
    return out;
}

std::size_t frame_graph::resource_bytes(int res) const
{
    return (res >= 0 && res < resource_count_) ? resources_[res].desc.bytes() : 0u;
}

int frame_graph::pool_slot(int res) const
{
    return (res >= 0 && res < resource_count_) ? resources_[res].pool_slot : -1;
}

int frame_graph::first_use(int res) const
{
    return (res >= 0 && res < resource_count_) ? resources_[res].first_use : -1;
}

int frame_graph::last_use(int res) const
{
    return (res >= 0 && res < resource_count_) ? resources_[res].last_use : -1;
}

void dump_frame_graph(const frame_graph& fg)
{
    ENGINE_LOG_INFO(log_gpu, "frame graph: %d passes declared, %d live, %d culled",
                    fg.pass_count(), fg.live_pass_count(), fg.culled_pass_count());

    for (int s = 0; s < fg.live_pass_count(); ++s)
    {
        const int p = fg.scheduled(s);
        ENGINE_LOG_INFO(log_gpu, "  %2d. %s", s, fg.pass_name(p));
        for (int i = 0; i < fg.access_count(p); ++i)
        {
            const frame_graph::access_report a = fg.access_at(p, i);
            if (a.use == fg_use::sample)
            {
                ENGINE_LOG_INFO(log_gpu, "        sample %s@%u",
                                fg.resource_name(a.resource), static_cast<unsigned>(a.version_in));
            }
            else
            {
                ENGINE_LOG_INFO(log_gpu, "        %s  %s@%u -> @%u  %s / %s",
                                use_name(a.use), fg.resource_name(a.resource),
                                static_cast<unsigned>(a.version_in),
                                static_cast<unsigned>(a.version_out),
                                load_op_name(a.load_op), store_op_name(a.store_op));
            }
        }
    }

    for (int r = 0; r < fg.resource_count(); ++r)
    {
        if (fg.first_use(r) < 0) { continue; }
        ENGINE_LOG_INFO(log_gpu, "  resource %-18s live [%d, %d]",
                        fg.resource_name(r), fg.first_use(r), fg.last_use(r));
    }
}

} // namespace engine
