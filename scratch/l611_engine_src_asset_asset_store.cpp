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

// ---- Lesson 6.6: a whole model, which is the door glTF comes through ---------

namespace {

/// Is a base colour factor white to within a float's worth of authoring noise?
///
/// The test that decides whether this importer can represent a material exactly.
/// glTF §5.19.4: "If both factors and textures are present, the factor value
/// acts as a linear multiplier for the corresponding texture values." Our
/// `material` (3.9's rule) says a bound albedo image REPLACES the tint rather
/// than multiplying it — and the two rules agree exactly, and only, when the
/// factor is white.
///
/// A tolerance rather than an equality, because an exporter that round-trips 1.0
/// through a JSON decimal can hand back 0.99999994.
[[nodiscard]] bool is_white(linear_rgb c)
{
    constexpr float eps = 1.0f / 512.0f;
    return c.r > 1.0f - eps && c.g > 1.0f - eps && c.b > 1.0f - eps;
}

}   // namespace

model_load asset_store::load_model(std::string_view name)
{
    model_load out;
    const std::string key(name);

    // ---- The cache, and what it is keyed on --------------------------------
    //
    // A model is not stored as one asset — it becomes n meshes and m materials,
    // each under its own derived name — so there is no `model_by_key_` map and
    // there should not be. The cache test is instead whether THE FIRST PRIMITIVE
    // is resident, which works because the names are deterministic and because
    // a partial model is not a state this function can leave behind: it inserts
    // everything or it fails before inserting anything.
    if (find_mesh(key + "#0").valid())
    {
        out.cached = true;
        ++counters_.cache_hits;
        for (int i = 0; ; ++i)
        {
            const mesh_handle h = find_mesh(key + "#" + std::to_string(i));
            if (!h.valid()) { break; }
            out.meshes.push_back(h);
        }
        out.report.status = gltf_status::ok;
        out.report.primitives = static_cast<int>(out.meshes.size());
        return out;
    }

    const resolved_path found = paths_.resolve(name);
    if (!found.ok())
    {
        ++counters_.loads_failed;
        out.report.status = gltf_status::cannot_open;
        ENGINE_LOG_ERROR(log_asset, "asset_store: model '%.*s' is in none of the %zu root(s)",
                         static_cast<int>(name.size()), name.data(), paths_.roots().size());
        return out;
    }

    gltf_scene_data scene;
    out.report = load_gltf(found.path.c_str(), scene);
    ++counters_.files_read;
    counters_.bytes_read += found.bytes;

    if (!out.report.ok())
    {
        ++counters_.loads_failed;
        return out;
    }

    // ---- Materials, and the two decisions this lesson had to make ----------
    out.materials.reserve(scene.materials.size());
    for (const gltf_material_desc& desc : scene.materials)
    {
        material m;

        // 1. THE SURFACE, COPIED WITHOUT CONVERSION. roughness, metallic and an
        //    F0 of 0.04 are what glTF stores and what `microsurface` means, and
        //    the agreement is not luck — both derive from the same microfacet
        //    model with the same Disney alpha remap (6.3) and the same IOR of
        //    1.5 (6.4). An engine with a hand-tuned gloss parameter would need a
        //    fitted curve here, and the fit would be a guess.
        m.surface = desc.surface;

        // 2. THE BASE COLOUR, RE-ENCODED ON PURPOSE. `desc.base_colour` is
        //    linear (the factor always is); `material::tint` is sRGB-encoded,
        //    because 6.5 decided a tint is authored data and decodes at the
        //    input edge. So this is the one conversion in the importer, and it
        //    runs in the direction nobody expects: linear -> encoded.
        m.tint = to_encoded(desc.base_colour);

        // 2b. TRANSPARENCY, LESSON 6.11 — three fields that go across untouched,
        //     and the fact that they need no conversion is the point. `alpha` is
        //     a coverage fraction on both sides, `alpha_cutoff` is a comparison
        //     threshold on both sides, and `mode` was already spelt out as a
        //     switch at the cgltf boundary rather than cast. Compare the two
        //     lines above it, where the base colour has to change space.
        //
        //     Until this lesson `desc.alpha` was read by nothing at all and
        //     `desc.mode` did not exist, so every MASK and BLEND material in
        //     every imported file arrived here and became opaque — silently,
        //     which is what 6.6 §10 named and this closes.
        m.mode = desc.mode;
        m.alpha = desc.alpha;
        m.alpha_cutoff = desc.alpha_cutoff;

        // 3. THE IMAGE, if the material named one and the search path can find
        //    it. A miss is counted, logged once by `load_image`, and NOT fatal:
        //    the material keeps its base colour and the model draws.
        if (!desc.base_colour_uri.empty())
        {
            const texture_load tex = load_texture(desc.base_colour_uri);
            if (tex.ok())
            {
                m.albedo_map = tex.handle;
                m.samp = desc.samp;

                // THE ONE CONFORMANCE GAP, MEASURED RATHER THAN HIDDEN. With an
                // image bound, `material::textured()` is true and the fill takes
                // its albedo from the image alone — so a non-white factor is
                // silently dropped. Reported here, at the only point that knows
                // both halves. Making them multiply is a one-line change in
                // raster.cpp and a matching one in scene.frag.hlsl; it is
                // Exercise 2, and it is deliberately not made here because it
                // would move a picture this lesson is claiming does not move.
                if (!is_white(desc.base_colour))
                {
                    ++out.factor_texture_conflicts;
                    ENGINE_LOG_WARN(log_asset,
                        "glTF material '%s': baseColorFactor (%.3f %.3f %.3f) is not "
                        "white and a baseColorTexture is bound. The spec multiplies "
                        "them; this engine's albedo image REPLACES the tint (3.9), so "
                        "the factor is dropped.",
                        desc.name.c_str(), static_cast<double>(desc.base_colour.r),
                        static_cast<double>(desc.base_colour.g),
                        static_cast<double>(desc.base_colour.b));
                }
            }
            else
            {
                ++out.textures_missing;
            }
        }

        // ---- THE NORMAL MAP — Lesson 6.7 ------------------------------------
        //
        // The only difference from the albedo above is the second argument, and
        // it is the whole reason this could not be written a lesson ago:
        // `texel_space::linear`. A normal map's three channels are a direction
        // packed into bytes, not a colour, so decoding them through the sRGB
        // curve is arithmetic on numbers that were never a colour. Read that way
        // the flat value 0.5 comes back as 0.2140 and every surface tilts.
        //
        // Note that the SPACE is supplied here rather than by the caller or by
        // the parser, and this is the only place that could supply it: the
        // parser knows the URI and not what it will be used for; the caller
        // knows neither. The store knows, because it is resolving a
        // `normalTexture` — the slot the file bound it to IS the declaration of
        // what the bytes mean.
        if (!desc.normal_uri.empty())
        {
            const texture_load nrm = load_texture(desc.normal_uri, texel_space::linear);
            if (nrm.ok())
            {
                m.normal_map = nrm.handle;
                ++out.normal_maps_loaded;
            }
            else
            {
                ++out.textures_missing;
            }
        }

        // Namespaced by the file it came from: two models may both call a
        // material "Metal" and they are not the same material.
        out.materials.push_back(insert_material(key + ":" + desc.name, m));
    }

    // The default, materialised only if something needs it — so a well-formed
    // file does not pay for a material it never references.
    material_handle fallback;

    // ---- Geometry ----------------------------------------------------------
    out.meshes.reserve(scene.primitives.size());
    for (std::size_t i = 0; i < scene.primitives.size(); ++i)
    {
        gltf_primitive& prim = scene.primitives[i];

        const mesh_handle h = insert_mesh(key + "#" + std::to_string(i),
                                          std::move(prim.geometry));
        if (!h.valid()) { continue; }

        out.meshes.push_back(h);
        out.placements.push_back(prim.world_from_local);

        if (prim.material >= 0
            && static_cast<std::size_t>(prim.material) < out.materials.size())
        {
            out.mesh_material.push_back(out.materials[static_cast<std::size_t>(prim.material)]);
        }
        else
        {
            if (!fallback.valid())
            {
                const gltf_material_desc def = gltf_default_material();
                material m;
                m.tint = to_encoded(def.base_colour);
                m.surface = def.surface;
                fallback = insert_material(key + ":" + def.name, m);
                out.materials.push_back(fallback);
            }
            out.mesh_material.push_back(fallback);
        }
    }

    ++counters_.models_loaded;

    ENGINE_LOG_INFO(log_asset, "loaded model  %-24s %d prim  %d vert  %d tri  "
                               "%zu mat  %d normal map(s)  %s  root %d",
                    key.c_str(), out.report.primitives, out.report.vertices,
                    out.report.triangles, out.materials.size(),
                    out.normal_maps_loaded,
                    out.report.binary ? "glb" : "gltf", found.root_index);
    return out;
}

// ---- Lesson 6.6: textures, which are DERIVED assets --------------------------
//
// Note the shape of this function against `load_image` above, because the
// difference is the whole reason 6.5 was right to wait. `load_image` is a cache
// over a FILE. This is a cache over a TRANSFORMATION of a file — and the extra
// machinery it needs is exactly one thing: a dependency edge, so that unloading
// the image cannot leave a texture behind pointing at nothing.

std::string asset_store::texture_key(std::string_view name, texel_space space)
{
    // THE DEFAULT SERIALISES TO NOTHING, which is `asset_key`'s rule from 5.5 and
    // it is load-bearing here for the same reason: a generated texture inserted
    // under a bare name and a loaded one requested with the default space must
    // land on the same key, or `insert_texture("checker", …)` and
    // `find_texture("checker")` disagree about where the checker lives.
    std::string key(name);
    if (space == texel_space::linear) { key += "|linear"; }
    return key;
}

texture_load asset_store::load_texture(std::string_view name, texel_space space)
{
    const std::string key = texture_key(name, space);

    if (const auto it = texture_by_key_.find(key); it != texture_by_key_.end())
    {
        if (textures_.contains(it->second))
        {
            ++counters_.cache_hits;
            texture_load hit;
            hit.handle = it->second;
            hit.source = find_image(name);   // keyed on the NAME; one decode, n readings
            hit.cached = true;
            hit.report.status = image_status::ok;
            if (const texture* t = textures_.get(hit.handle))
            {
                hit.report.width = t->width();
                hit.report.height = t->height();
            }
            return hit;
        }
        texture_by_key_.erase(it);
    }

    // THE SOURCE FIRST, through the door that already exists. Everything about
    // resolving a name, reading bytes, decoding them and counting the read is
    // `load_image`'s job and it is not repeated here — which is what makes
    // "the same image loaded twice is decoded once" true for textures too,
    // without this function knowing anything about it.
    texture_load out;
    const image_load img = load_image(name);
    out.report = img.report;
    out.source = img.handle;

    if (!img.ok()) { return out; }

    const image_data* pixels = images_.get(img.handle);
    if (pixels == nullptr) { return out; }

    out.handle = derive_texture(img.handle, key, to_texture(*pixels, space));
    if (!out.handle.valid())
    {
        ++counters_.loads_failed;
        return out;
    }

    ENGINE_LOG_INFO(log_asset, "made texture  %-24s %4dx%-4d %s  from image [handle %u:%u]",
                    key.c_str(), out.report.width, out.report.height,
                    (space == texel_space::linear) ? "DATA " : "sRGB ",
                    out.handle.index(), out.handle.generation());
    return out;
}

texture_handle asset_store::derive_texture(image_handle source,
                                           std::string_view key,
                                           texture data)
{
    if (!images_.contains(source))
    {
        ENGINE_LOG_WARN(log_asset, "asset_store: refusing to derive a texture from a "
                                   "stale image [handle %u:%u]",
                        source.index(), source.generation());
        return {};
    }

    const texture_handle h = textures_.insert(std::move(data));
    if (!h.valid()) { return {}; }

    texture_derivations_.push_back({source.bits, h.bits});
    texture_by_key_[std::string(key)] = h;
    ++counters_.derived;
    return h;
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

texture_handle asset_store::find_texture(std::string_view name, texel_space space) const
{
    const auto it = texture_by_key_.find(texture_key(name, space));
    if (it == texture_by_key_.end() || !textures_.contains(it->second)) { return {}; }
    return it->second;
}

material_handle asset_store::find_material(std::string_view name) const
{
    const auto it = material_by_key_.find(std::string(name));
    if (it == material_by_key_.end() || !materials_.contains(it->second)) { return {}; }
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

texture_handle asset_store::insert_texture(std::string_view name, texture data)
{
    const std::string key(name);
    if (const auto it = texture_by_key_.find(key); it != texture_by_key_.end())
    {
        release_texture(it->second);
        texture_by_key_.erase(it);
    }

    const texture_handle h = textures_.insert(std::move(data));
    if (!h.valid()) { return {}; }

    // NO DERIVATION EDGE, and that is the difference between this and
    // `load_texture`. A generated texture has no source image, so nothing will
    // ever cascade onto it and it is freed by name alone. The edge list stays a
    // record of real dependencies rather than a list of everything.
    texture_by_key_[key] = h;
    ++counters_.inserted;
    return h;
}

material_handle asset_store::insert_material(std::string_view name, material data)
{
    const std::string key(name);
    if (const auto it = material_by_key_.find(key); it != material_by_key_.end())
    {
        if (materials_.remove(it->second)) { ++counters_.unloaded; }
        material_by_key_.erase(it);
    }

    const material_handle h = materials_.insert(data);
    if (!h.valid()) { return {}; }

    material_by_key_[key] = h;
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

int asset_store::derived_count(image_handle source) const
{
    int total = 0;
    for (const derivation& d : texture_derivations_)
    {
        if (d.source == source.bits) { ++total; }
    }
    // Not recursive, unlike the mesh version, because nothing is derived FROM a
    // texture — yet. Mipmaps (Module 6) will be, and this is the function that
    // grows a recursive call when they are.
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

    // LESSON 6.6: THE CASCADE, which is the reason `derive_texture` exists at
    // all. Collect the children before removing anything, for the same
    // iterator-invalidation reason `release_mesh` gives — and note that the
    // texture's NAME entry has to go too, or `find_texture` would keep handing
    // out a handle to a slot the pool has already recycled. That is the failure
    // 5.4 was written to make impossible, so it does not crash; it returns null
    // and increments a counter. Correct, and still a bug worth not having.
    std::vector<texture_handle> children;
    for (const derivation& d : texture_derivations_)
    {
        if (d.source == h.bits) { children.push_back(texture_handle{d.derived}); }
    }

    int released = 0;
    for (const texture_handle child : children) { released += unload_texture(child); }

    std::erase_if(texture_derivations_, [h](const derivation& d) {
        return d.source == h.bits;
    });

    if (!images_.remove(h)) { return released; }
    ++counters_.unloaded;
    return released + 1;
}

int asset_store::release_texture(texture_handle h)
{
    if (!textures_.contains(h)) { return 0; }

    std::erase_if(texture_derivations_, [h](const derivation& d) {
        return d.derived == h.bits;
    });

    if (!textures_.remove(h)) { return 0; }
    ++counters_.unloaded;
    return 1;
}

int asset_store::unload_texture(texture_handle h)
{
    if (!textures_.contains(h)) { return 0; }

    for (auto it = texture_by_key_.begin(); it != texture_by_key_.end(); ++it)
    {
        if (it->second == h) { texture_by_key_.erase(it); break; }
    }
    return release_texture(h);
}

int asset_store::unload_material(material_handle h)
{
    if (!materials_.contains(h)) { return 0; }

    for (auto it = material_by_key_.begin(); it != material_by_key_.end(); ++it)
    {
        if (it->second == h) { material_by_key_.erase(it); break; }
    }
    if (!materials_.remove(h)) { return 0; }
    ++counters_.unloaded;
    return 1;
}

int asset_store::unload_all()
{
    const int total = static_cast<int>(meshes_.size() + images_.size()
                                       + textures_.size() + materials_.size());

    // `pool::clear()` bumps every live slot's generation, so every outstanding
    // handle goes stale rather than being left pointing at a slot that will be
    // refilled by the next level. That is the whole reason 5.4's clear() keeps
    // generations instead of resetting them.
    meshes_.clear();
    images_.clear();
    textures_.clear();
    materials_.clear();
    mesh_by_key_.clear();
    image_by_key_.clear();
    texture_by_key_.clear();
    material_by_key_.clear();
    mesh_derivations_.clear();
    texture_derivations_.clear();

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
    return static_cast<int>(meshes_.size() + images_.size()
                            + textures_.size() + materials_.size());
}

}   // namespace engine
