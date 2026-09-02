// engine/src/asset/asset_store.cpp — the policy on top of Lesson 5.4's mechanism.

#include <engine/asset/asset_store.hpp>

#include <engine/core/log.hpp>

#include <SDL3/SDL_timer.h>

#include <utility>

namespace engine {

std::string asset_key(std::string_view name, const mesh_import& settings)
{
    std::string key(name);

    // ONE KEY SPACE, and the default import is the one that spends nothing on it.
    // Generated content (`insert_mesh`) has no import settings and is stored under
    // its bare name; decorating a default import would put loaded and generated
    // assets in two key spaces that look like one.
    if (!(settings == mesh_import{})) { key += "|flip=0"; }
    return key;
}

asset_store::asset_store() : paths_(search_path::standard()) {}

asset_store::asset_store(search_path paths) : paths_(std::move(paths)) {}

// ---- Loading ---------------------------------------------------------------

mesh_load asset_store::load_mesh(std::string_view name, const mesh_import& settings)
{
    const std::string key = asset_key(name, settings);

    // THE CACHE, and note what makes it trustworthy: `contains` is 5.4's
    // three-test resolution, so a key whose asset was unloaded behind our back
    // does not survive here as a stale entry. The map can be wrong; the pool
    // cannot.
    if (const auto it = mesh_by_key_.find(key); it != mesh_by_key_.end())
    {
        if (meshes_.contains(it->second))
        {
            ++counters_.cache_hits;
            mesh_load hit;
            hit.handle = it->second;
            hit.cached = true;
            hit.report.status = obj_status::ok;
            if (const mesh_data* m = meshes_.get(hit.handle))
            {
                hit.report.vertices = static_cast<int>(m->vertices.size());
                hit.report.triangles = static_cast<int>(m->triangle_count());
            }
            return hit;
        }
        mesh_by_key_.erase(it);   // the pool freed it; the map catches up
    }

    const resolved_path found = paths_.resolve(name);
    if (!found.ok())
    {
        // ONE line for one problem — Lesson 5.3's rule, and it took a first run to
        // notice it was being broken. A refused name is reported by `resolve`,
        // which is the point that knows WHY; a name that is simply absent is
        // reported here, which is the point that knows it was wanted as an asset.
        // Logging both produced two entries for one failure, the second of them
        // less informative than the first.
        ++counters_.loads_failed;
        mesh_load miss;
        miss.report.status = obj_status::cannot_open;
        if (found.root_index == -1 && !found.refused)
        {
            ENGINE_LOG_ERROR(log_asset, "asset_store: mesh '%.*s' is in none of the "
                                        "%zu root(s)",
                             static_cast<int>(name.size()), name.data(),
                             paths_.roots().size());
        }
        return miss;
    }

    mesh_data data;
    mesh_load out;
    out.report = load_obj(found.path.c_str(), data);
    ++counters_.files_read;
    counters_.bytes_read += found.bytes;

    if (!out.report.ok())
    {
        // `load_obj` has already logged the reason at the point that knew it
        // (Lesson 5.3's rule: the deepest point logs, callers propagate). What
        // this layer adds is the only thing IT knows — that the asset will not
        // exist — and it adds it at the same level rather than a louder one.
        ++counters_.loads_failed;
        return out;
    }

    // THE IMPORT STEP, and it is here rather than in the parser for the reason
    // Lesson 3.5 settled and 3.9 restated: a loader stores what the file says, so
    // that two loads of a file are diffable and a loader is never a place where
    // data quietly differs from its source. Reconciling somebody else's
    // convention with ours is a separate act, performed once, at the boundary
    // where their data becomes ours. This function is that boundary.
    if (settings.flip_uv_v) { flip_uv_v(data); }

    out.handle = meshes_.insert(std::move(data));
    if (!out.handle.valid())
    {
        ++counters_.loads_failed;
        return out;
    }
    mesh_by_key_[key] = out.handle;

    ENGINE_LOG_INFO(log_asset, "loaded mesh   %-24s %5d verts %5d tris  "
                               "root %d  %llu bytes  [handle %u:%u]",
                    key.c_str(), out.report.vertices, out.report.triangles,
                    found.root_index, static_cast<unsigned long long>(found.bytes),
                    out.handle.index(), out.handle.generation());
    return out;
}

image_load asset_store::load_image(std::string_view name)
{
    const std::string key(name);

    if (const auto it = image_by_key_.find(key); it != image_by_key_.end())
    {
        if (images_.contains(it->second))
        {
            ++counters_.cache_hits;
            image_load hit;
            hit.handle = it->second;
            hit.cached = true;
            hit.report.status = image_status::ok;
            if (const image_data* img = images_.get(hit.handle))
            {
                hit.report.width = img->width;
                hit.report.height = img->height;
                hit.report.source_channels = img->source_channels;
                hit.report.bytes = img->byte_count();
            }
            return hit;
        }
        image_by_key_.erase(it);
    }

    const resolved_path found = paths_.resolve(name);
    if (!found.ok())
    {
        ++counters_.loads_failed;
        image_load miss;
        miss.report.status = image_status::cannot_open;
        if (found.root_index == -1 && !found.refused)
        {
            ENGINE_LOG_ERROR(log_asset, "asset_store: image '%.*s' is in none of the "
                                        "%zu root(s)",
                             static_cast<int>(name.size()), name.data(),
                             paths_.roots().size());
        }
        return miss;
    }

    image_data data;
    image_load out;
    // Qualified, because the member function we are inside HIDES the free
    // function of the same name. Unqualified `load_image(...)` here is a
    // compile error about too few arguments, which is a confusing way to be told
    // about a name lookup rule.
    out.report = engine::load_image(found.path.c_str(), data);
    ++counters_.files_read;
    counters_.bytes_read += found.bytes;

    if (!out.report.ok())
    {
        ++counters_.loads_failed;
        return out;
    }

    out.handle = images_.insert(std::move(data));
    if (!out.handle.valid())
    {
        ++counters_.loads_failed;
        return out;
    }
    image_by_key_[key] = out.handle;

    ENGINE_LOG_INFO(log_asset, "loaded image  %-24s %4dx%-4d %d ch  root %d  "
                               "[handle %u:%u]",
                    key.c_str(), out.report.width, out.report.height,
                    out.report.source_channels, found.root_index,
                    out.handle.index(), out.handle.generation());
    return out;
}

mesh_handle asset_store::find_mesh(std::string_view name, const mesh_import& settings) const
{
    const auto it = mesh_by_key_.find(asset_key(name, settings));
    if (it == mesh_by_key_.end() || !meshes_.contains(it->second)) { return {}; }
    return it->second;
}

image_handle asset_store::find_image(std::string_view name) const
{
    const auto it = image_by_key_.find(std::string(name));
    if (it == image_by_key_.end() || !images_.contains(it->second)) { return {}; }
    return it->second;
}

// ---- Generated content -----------------------------------------------------

mesh_handle asset_store::insert_mesh(std::string_view name, mesh_data data)
{
    const std::string key(name);

    // REPLACE rather than shadow. Two assets under one name is the state from
    // which no correct behaviour follows: `find` would have to pick, and every
    // caller would disagree about which. Unloading first also cascades, so
    // anything derived from the previous version goes with it — which is exactly
    // what you want when regenerating content, and exactly what Lesson 5.4's
    // `scene_mesh_cache` could not previously be told.
    if (const auto it = mesh_by_key_.find(key); it != mesh_by_key_.end())
    {
        release_mesh(it->second);
        mesh_by_key_.erase(it);
    }

    const mesh_handle h = meshes_.insert(std::move(data));
    if (!h.valid()) { return {}; }

    mesh_by_key_[key] = h;
    ++counters_.inserted;
    return h;
}

image_handle asset_store::insert_image(std::string_view name, image_data data)
{
    const std::string key(name);
    if (const auto it = image_by_key_.find(key); it != image_by_key_.end())
    {
        if (images_.remove(it->second)) { ++counters_.unloaded; }
        image_by_key_.erase(it);
    }

    const image_handle h = images_.insert(std::move(data));
    if (!h.valid()) { return {}; }

    image_by_key_[key] = h;
    ++counters_.inserted;
    return h;
}

// ---- Derived assets --------------------------------------------------------

mesh_handle asset_store::derive_mesh(mesh_handle source, mesh_data data)
{
    // A derivation from a source that is already gone would be an orphan: no name
    // to find it by, no source to free it with, and therefore nothing that will
    // ever free it. Refuse it, which turns a leak into a null handle the caller
    // is already prepared to see.
    if (!meshes_.contains(source))
    {
        ENGINE_LOG_WARN(log_asset, "asset_store: refusing to derive from a stale "
                                   "source [handle %u:%u]",
                        source.index(), source.generation());
        return {};
    }

    const mesh_handle h = meshes_.insert(std::move(data));
    if (!h.valid()) { return {}; }

    mesh_derivations_.push_back({source.bits, h.bits});
    ++counters_.derived;
    return h;
}

int asset_store::derived_count(mesh_handle source) const
{
    int total = 0;
    for (const derivation& d : mesh_derivations_)
    {
        if (d.source == source.bits)
        {
            ++total;
            total += derived_count(mesh_handle{d.derived});
        }
    }
    return total;
}

// ---- Unloading -------------------------------------------------------------

int asset_store::release_mesh(mesh_handle h)
{
    if (!meshes_.contains(h)) { return 0; }

    // Collect first, then recurse. Walking `mesh_derivations_` while the
    // recursion erases from it is the classic iterator-invalidation bug, and it
    // would show up only for a mesh with more than one dependent — which the
    // demo does not have and a real scene does.
    std::vector<mesh_handle> children;
    for (const derivation& d : mesh_derivations_)
    {
        if (d.source == h.bits) { children.push_back(mesh_handle{d.derived}); }
    }

    int released = 0;
    for (const mesh_handle child : children) { released += release_mesh(child); }

    std::erase_if(mesh_derivations_, [h](const derivation& d) {
        return d.source == h.bits || d.derived == h.bits;
    });

    if (meshes_.remove(h))
    {
        ++released;
        ++counters_.unloaded;
    }
    return released;
}

int asset_store::unload_mesh(mesh_handle h)
{
    if (!meshes_.contains(h)) { return 0; }

    // The name entry goes with the asset. Linear, because there is one map and it
    // is keyed the way lookups need it to be keyed; see `name_of`.
    for (auto it = mesh_by_key_.begin(); it != mesh_by_key_.end(); ++it)
    {
        if (it->second == h) { mesh_by_key_.erase(it); break; }
    }
    return release_mesh(h);
}

int asset_store::unload_image(image_handle h)
{
    if (!images_.contains(h)) { return 0; }

    for (auto it = image_by_key_.begin(); it != image_by_key_.end(); ++it)
    {
        if (it->second == h) { image_by_key_.erase(it); break; }
    }
    if (!images_.remove(h)) { return 0; }
    ++counters_.unloaded;
    return 1;
}

int asset_store::unload_all()
{
    const int total = static_cast<int>(meshes_.size() + images_.size());

    // `pool::clear()` bumps every live slot's generation, so every outstanding
    // handle goes stale rather than being left pointing at a slot that will be
    // refilled by the next level. That is the whole reason 5.4's clear() keeps
    // generations instead of resetting them.
    meshes_.clear();
    images_.clear();
    mesh_by_key_.clear();
    image_by_key_.clear();
    mesh_derivations_.clear();

    counters_.unloaded += total;
    return total;
}

// ---- Resolving -------------------------------------------------------------

std::string_view asset_store::name_of(mesh_handle h) const
{
    for (const auto& [key, handle] : mesh_by_key_)
    {
        if (handle == h) { return key; }
    }
    return {};
}

std::string_view asset_store::name_of(image_handle h) const
{
    for (const auto& [key, handle] : image_by_key_)
    {
        if (handle == h) { return key; }
    }
    return {};
}

int asset_store::live_count() const
{
    return static_cast<int>(meshes_.size() + images_.size());
}

}   // namespace engine
