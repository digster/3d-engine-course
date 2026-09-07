// engine/include/engine/asset/asset_store.hpp — loading, finding, and unloading.
//
// Lesson 5.5. Lesson 5.4 built the MECHANISM — a handle that can be told it is out
// of date, and a pool that issues them. This file is the POLICY, and it answers the
// three questions a pool does not:
//
//   LOADING     where does a mesh come from, and who decides? Before this file,
//               forty lines of demo code opened a file, timed it, validated it,
//               flipped its uvs and logged about it. Four of those five steps are
//               an engine's job and exactly one is a program's.
//   FINDING     ask for "torus.obj" twice and get the SAME handle, having read the
//               file ONCE. That is a name -> handle map, and it is the first thing
//               in this engine that has needed one.
//   UNLOADING   the word this whole module has been building towards. Nothing in
//               this engine has ever freed anything.
//
// ---------------------------------------------------------------------------
// WHY THERE IS NO REFERENCE COUNT, WHICH IS THE DESIGN DECISION IN THIS FILE
// ---------------------------------------------------------------------------
//
// The obvious answer to "when may I free this?" is to count who is holding it, and
// it is the wrong answer HERE for a reason that comes straight out of 5.4:
//
//     A HANDLE IS A VALUE. Four bytes, trivially copyable, no lifetime of its own.
//     That is precisely what makes it useful — you can memcpy it, put it in a POD
//     component, write it to a scene file, print it in a log line.
//
// Reference counting requires the opposite: copying must have a side effect, so a
// handle would need a copy constructor, a destructor, and a pointer back to its
// store. It would stop being four bytes, stop being trivially copyable, stop being
// serializable without fixups, and stop being safe to hold in an ECS component —
// which is to say it would lose every property Lesson 5.4 was written to obtain.
// A refcounted handle is a `shared_ptr` with extra steps, and we already knew what
// `shared_ptr` costs.
//
// So the policy is EXPLICIT UNLOAD, and it is safe to get wrong precisely because
// 5.4 made staleness detectable: unloading something still referenced is not
// undefined behaviour, it is a lookup that returns null and a counter that goes up.
// The failure mode of forgetting to unload is a leak, which a total is enough to
// find; the failure mode of unloading too early is a missing object with a number
// next to it. Neither is a crash. That is the trade, and it is the one most engines
// make at level granularity.
//
// THERE IS ONE PLACE A LIFETIME RULE IS UNAVOIDABLE, and it is the hole Lesson 5.4
// deliberately left: DERIVED ASSETS. `with_normals` turns a loaded mesh into a
// different mesh; a mipmap chain, a shader variant and a GPU upload are all the
// same shape. The dependency there is INTERNAL — the store made the derived asset,
// nobody outside asked for it by name, and it has no independent reason to exist —
// so it can be tracked without exposing anything, and it is: unloading a source
// unloads everything derived from it, transitively. That is a dependency edge, not
// a reference count, and the difference is that nobody outside this file can hold
// one.
//
// ---------------------------------------------------------------------------
// AND WHY IT IS NOT A SINGLETON
// ---------------------------------------------------------------------------
//
// A handle needs its pool, so there is real pressure to make the store global and
// stop passing it. Refused, for a concrete reason rather than a stylistic one:
// Lesson 5.4 ended with `demos/sandbox` holding TWO mesh pools in one frame, and
// a store is a plain object precisely so that a program can have two — a game and
// its editor, a level and the one streaming in behind it, a test fixture and the
// real thing. `engine::platform` and `engine::app` do not own one either; owning
// one there would be a singleton with better manners.

#pragma once

#include <engine/asset/search_path.hpp>
#include <engine/gfx/gltf.hpp>      // 6.6: the second format, and the first with materials
#include <engine/gfx/image.hpp>
#include <engine/gfx/material.hpp>  // 6.6: materials are named, cached assets now
#include <engine/gfx/mesh.hpp>
#include <engine/gfx/obj.hpp>
#include <engine/gfx/texture.hpp>   // 6.6: and textures are derived ones

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace engine {

// ---- Import settings -------------------------------------------------------

/// What the import step does to geometry on the way in.
///
/// **These are part of an asset's IDENTITY, not arguments to a function**, and
/// that is the non-obvious part. The same `torus.obj` imported with the uv flip and
/// without it is two different meshes with two different vertex arrays; they cannot
/// share a cache entry, and a store that keyed on the filename alone would hand the
/// second requester the first one's geometry and be quietly wrong.
///
/// So the cache key is the name **and** these settings (`asset_key()` below). Every
/// real pipeline does this: Unity writes a `.meta` file beside each asset and
/// re-imports when it changes, Unreal keys its derived-data cache on a hash of the
/// settings. Ours is one bool and a string; the shape is the same.
struct mesh_import
{
    /// OBJ writes `v` upwards from the bottom left; a texture is addressed
    /// downwards from the top left. Lesson 3.9 established that the reconciliation
    /// belongs to the import step, and this is that step.
    bool flip_uv_v = true;

    [[nodiscard]] friend constexpr bool operator==(mesh_import, mesh_import) = default;
};

/// The cache key for a name plus its import settings.
///
/// **The DEFAULT import adds nothing**, so `asset_key("torus.obj", {})` is just
/// `"torus.obj"` and only a non-default import earns a suffix
/// (`"torus.obj|flip=0"`). That is not cosmetic. Generated content has no import
/// step and is stored under its bare name; if the default import decorated its key,
/// `insert_mesh("cube", …)` and `find_mesh("cube")` would disagree about where the
/// cube lives — which is exactly the bug the first run of `verify_55` found, in
/// §D, one check after the one that stored it.
///
/// A string rather than a hashed struct, deliberately: it is human-readable in a
/// log and in a debugger, which matters much more than the handful of bytes it
/// costs on a few hundred assets. When there are ten settings and fifty thousand
/// assets, hash it — and Module 8, which caches cooked assets on disk, is where
/// that becomes true.
[[nodiscard]] std::string asset_key(std::string_view name, const mesh_import& settings);

// ---- What an acquire returns -----------------------------------------------

/// The result of asking for a mesh: the handle, what the loader saw, and whether
/// the file was touched at all.
///
/// `cached` is not a nicety. It is the observable difference between a cache that
/// works and one that does not, and it is what `verify_55` asserts on: ask twice,
/// get the same handle, and see `files_read` stay where it was.
struct mesh_load
{
    mesh_handle handle;      ///< null if the load failed
    obj_report report;       ///< what the file contained; empty-ish on a cache hit
    bool cached = false;     ///< true if this name was already loaded

    [[nodiscard]] bool ok() const { return handle.valid(); }
};

/// The same, for images.
struct image_load
{
    image_handle handle;
    image_report report;
    bool cached = false;

    [[nodiscard]] bool ok() const { return handle.valid(); }
};

/// And for textures — **which are derived, not loaded**, and the extra field is
/// where that shows.
///
/// A texture is made FROM an image (`to_texture`), so acquiring one acquires two
/// assets and the caller may legitimately want both: the image to hand to the
/// GPU as bytes, the texture to sample on the CPU. Returning only the derived
/// half would force a second lookup by name to get back to the source, which is
/// exactly the kind of "the API knows and won't say" that makes people cache
/// things themselves.
struct texture_load
{
    texture_handle handle;
    image_handle source;     ///< the image it was built from
    image_report report;
    bool cached = false;

    [[nodiscard]] bool ok() const { return handle.valid(); }
};

/// What loading a whole MODEL produced — Lesson 6.6.
///
/// **The first acquire in this engine that returns more than one of anything**,
/// and that is not an inconvenience, it is what a scene format IS. A `.gltf` is
/// a node tree of primitives pointing into a material table; one file yields n
/// meshes, m materials and however many textures those materials named. An API
/// returning a single handle would have to pick which one, and every choice is
/// wrong for some caller.
///
/// The three vectors are parallel to nothing — `meshes` and `materials` are
/// indexed independently, and `placements[i]` is the world transform for
/// `meshes[i]`, which IS a parallel array and is documented as one.
struct model_load
{
    std::vector<mesh_handle> meshes;
    std::vector<mat4> placements;          ///< one per mesh, same index
    std::vector<material_handle> materials;
    std::vector<material_handle> mesh_material;  ///< one per mesh, same index

    gltf_report report;
    bool cached = false;

    /// Textures the materials asked for and the store could not resolve.
    /// **Counted, not fatal**: a model with a missing image is a model that
    /// draws in its base colour, which is a far better outcome than a model that
    /// does not draw.
    int textures_missing = 0;

    /// Materials whose base colour factor is not white AND which also carry a
    /// base colour texture — see `asset_store::load_model` for why that
    /// combination is the one conformance gap this importer has.
    int factor_texture_conflicts = 0;

    [[nodiscard]] bool ok() const { return !meshes.empty(); }
};

// ---- Counters --------------------------------------------------------------

/// Everything the store has done, so that claims about it can be checked.
///
/// A store with no counters can only be believed. These make "loaded once" a
/// number rather than an intention, and `leaked()` at shutdown is the whole safety
/// net that explicit unload is traded against.
struct asset_counters
{
    int files_read = 0;        ///< times a file was actually opened
    int cache_hits = 0;        ///< times a name was already resident
    int loads_failed = 0;      ///< resolves or decodes that did not produce an asset
    int inserted = 0;          ///< generated content given a name
    int derived = 0;           ///< assets produced FROM another asset
    int unloaded = 0;          ///< assets released, cascades included
    std::uint64_t bytes_read = 0;

    /// Models loaded from a scene format — Lesson 6.6. Counted separately from
    /// `files_read` because one model file reads MANY files (a `.gltf`, its
    /// `.bin`, and one per texture), and a counter that conflated the two would
    /// make "did the cache work?" unanswerable.
    int models_loaded = 0;
};

// ---- The store -------------------------------------------------------------

/// Owns every loaded asset, and is the only thing allowed to free one.
///
/// Not copyable — it owns pools, and a copy would issue handles that resolve in two
/// places and are unloaded from one. Movable, so it can be returned and stored.
class asset_store
{
public:
    /// A store over the standard search path (`<base>/assets/`).
    asset_store();

    /// A store over roots you choose. Used by tests, by tools, and by anything
    /// that wants an override directory in front.
    explicit asset_store(search_path paths);

    asset_store(const asset_store&) = delete;
    asset_store& operator=(const asset_store&) = delete;
    asset_store(asset_store&&) noexcept = default;
    asset_store& operator=(asset_store&&) noexcept = default;

    // ---- Loading -----------------------------------------------------------

    /// Get the mesh called `name`, loading it if it is not already resident.
    ///
    /// **Failure is the common case, not the error case.** A missing file, an
    /// unparseable one, or a name that escapes the search path all return a NULL
    /// handle and a report that says which — and the caller carries on. Lesson
    /// 5.4's renderer already skips an object whose handle does not resolve and
    /// counts it, so "the asset is missing" costs one object and never a frame.
    [[nodiscard]] mesh_load load_mesh(std::string_view name, const mesh_import& settings = {});

    /// Get the image called `name`, loading it if it is not already resident.
    ///
    /// Note the collision worth knowing about: `engine::load_image` is also a free
    /// function (gfx/image.hpp), and inside this class the member hides it. The
    /// implementation calls `engine::load_image` fully qualified for that reason.
    [[nodiscard]] image_load load_image(std::string_view name);

    /// **Load a whole model** — Lesson 6.6, and the door glTF comes through.
    ///
    /// One call does what `load_mesh` plus four `load_texture`s plus a pile of
    /// bookkeeping would: parse the file, insert each primitive's geometry under
    /// a derived name, resolve each material's base colour image against the
    /// search path, build one `material` per file material, and hand back
    /// handles into this store's own pools.
    ///
    /// **Names are derived and deterministic**, which is what makes the cache
    /// work: primitive 2 of `"shapes.glb"` is stored as
    /// `"shapes.glb#2"` and material `"gold"` as `"shapes.glb:gold"`. Load the
    /// file twice and the second call reads no files and returns the same
    /// handles — which `verify_66` asserts, because a cache nobody measured is a
    /// cache nobody has.
    ///
    /// **`settings.flip_uv_v` is IGNORED**, deliberately and loudly. glTF's
    /// texture coordinate origin is the upper left, the same as ours; OBJ's is
    /// the lower left. The flip belongs to the FORMAT and not to the caller's
    /// preference, and honouring the flag here would turn every glTF asset
    /// upside down for anyone who left the default alone. The parameter is
    /// accepted so the signature matches `load_mesh`, and refused so the
    /// difference cannot be a silent one.
    [[nodiscard]] model_load load_model(std::string_view name);

    /// Get the texture called `name`, **loading the image behind it if needed**.
    ///
    /// Lesson 6.6, and the reason it is `load_texture` rather than
    /// `make_texture(image_handle)` is the whole point of an asset system: a
    /// caller with a URI out of a glTF material wants a samplable texture, and
    /// every step between those two — resolve the name against the search path,
    /// decode the file, shuffle the channels, cache the result, remember that
    /// the texture depends on the image — is policy this class already owns.
    ///
    /// **Two callers asking for the same image get the same texture**, which is
    /// the property that matters for glTF specifically: a file's materials
    /// routinely share one atlas, and building it per material would decode the
    /// PNG four times and store four copies of it.
    ///
    /// The texture is stored as a DERIVED asset of the image, so
    /// `unload_image(t.source)` takes the texture with it. That is the same edge
    /// `derive_mesh` uses (5.5) and it exists for the same reason: nobody asked
    /// for the texture by a name of its own, so nothing outside can be holding a
    /// claim on it that the image does not already imply.
    [[nodiscard]] texture_load load_texture(std::string_view name);

    /// The handle for `name` if it is resident, or null. Never touches the disk.
    [[nodiscard]] mesh_handle find_mesh(std::string_view name,
                                        const mesh_import& settings = {}) const;
    [[nodiscard]] image_handle find_image(std::string_view name) const;
    [[nodiscard]] texture_handle find_texture(std::string_view name) const;
    [[nodiscard]] material_handle find_material(std::string_view name) const;

    // ---- Generated content -------------------------------------------------

    /// Give generated content a name and put it in the store.
    ///
    /// **An asset system that can only load is missing half its job.** A torus
    /// built by `make_torus`, a checkerboard built by `make_checker` and the
    /// built-in cube are all assets in every way that matters — they are owned,
    /// referred to by handle, found by name, and unloaded — and the only thing
    /// they lack is a file. Passing them through the same door as loaded content
    /// is what stops the engine growing a second, shabbier one.
    ///
    /// Re-inserting an existing name REPLACES it: the old asset (and everything
    /// derived from it) is unloaded first, so the old handle stops resolving.
    [[nodiscard]] mesh_handle insert_mesh(std::string_view name, mesh_data data);
    [[nodiscard]] image_handle insert_image(std::string_view name, image_data data);

    /// A generated texture, named — `make_checker` and `make_uv_grid` (3.9) go
    /// through this door, exactly as a generated mesh goes through
    /// `insert_mesh`. It is the same argument 5.5 made and it is worth repeating
    /// because 6.5 got it wrong: an asset system that can only LOAD is missing
    /// half its job, and the half it is missing is the half every test uses.
    [[nodiscard]] texture_handle insert_texture(std::string_view name, texture data);

    /// A material, named.
    ///
    /// **There is no `load_material`, and there never will be**, because a
    /// material is not a file. It arrives as part of one — a glTF's material
    /// table — and `load_model` is what puts it here. Naming it anyway is what
    /// buys the property glTF needs: a `.gltf` whose four primitives share one
    /// material must produce ONE material, and two loads of the same file must
    /// not produce eight.
    [[nodiscard]] material_handle insert_material(std::string_view name, material data);

    // ---- Derived assets ----------------------------------------------------

    /// Store a mesh produced FROM another mesh, with its lifetime bound to it.
    ///
    /// The derived asset has no name and cannot be found by one, because nobody
    /// asked for it: it exists only as a consequence of its source existing.
    /// Unloading the source unloads it, transitively — which is the rule that
    /// closes the hole Lesson 5.4 left open in `scene_mesh_cache`.
    ///
    /// A null or stale `source` refuses the derivation and returns null, rather
    /// than storing an orphan that nothing will ever free.
    [[nodiscard]] mesh_handle derive_mesh(mesh_handle source, mesh_data data);

    /// How many meshes are derived from `source`, directly or transitively.
    /// Diagnostic; `verify_55` uses it to prove the cascade.
    [[nodiscard]] int derived_count(mesh_handle source) const;

    /// How many textures are derived from `source`. Same idea, second type —
    /// and it is worth noticing that adding a second derivation KIND needed a
    /// second edge list rather than a generic one. A `vector<pair<uint32,
    /// uint32>>` with no type tag would have compiled, resolved a texture handle
    /// against the mesh pool, and been wrong in a way nothing could catch (5.4's
    /// aliasing failure, in a new costume). Two lists, two types, no tag.
    [[nodiscard]] int derived_count(image_handle source) const;

    // ---- Unloading ---------------------------------------------------------

    /// Release `h` and everything derived from it. Returns how many assets went.
    ///
    /// Zero means the handle was already stale — which is not an error, and is the
    /// answer a second unload of the same thing should give.
    int unload_mesh(mesh_handle h);
    int unload_image(image_handle h);
    int unload_texture(texture_handle h);
    int unload_material(material_handle h);

    /// Release everything. Returns the count. Every outstanding handle goes stale.
    int unload_all();

    // ---- Resolving ---------------------------------------------------------

    /// The pools, for whoever has to turn handles back into data — the renderer,
    /// mostly. Const: only the store may add to or remove from them.
    [[nodiscard]] const mesh_pool& meshes() const { return meshes_; }
    [[nodiscard]] const image_pool& images() const { return images_; }

    [[nodiscard]] const texture_pool& textures() const { return textures_; }
    [[nodiscard]] const material_pool& materials() const { return materials_; }

    [[nodiscard]] const mesh_data* mesh_at(mesh_handle h) const { return meshes_.get(h); }
    [[nodiscard]] const image_data* image_at(image_handle h) const { return images_.get(h); }
    [[nodiscard]] const texture* texture_at(texture_handle h) const { return textures_.get(h); }
    [[nodiscard]] const material* material_at(material_handle h) const { return materials_.get(h); }

    /// The key `h` was stored under, or "" for a derived or unknown asset.
    ///
    /// **O(n) in the number of resident assets, and diagnostic only.** A second map
    /// from handle to name would make it O(1) and would be a second thing to keep
    /// in sync with the first — the exact bookkeeping Lesson 5.4's `dense` sentinel
    /// was designed to avoid. One map, one truth, and a linear scan for the log
    /// line nobody prints per frame.
    [[nodiscard]] std::string_view name_of(mesh_handle h) const;
    [[nodiscard]] std::string_view name_of(image_handle h) const;

    // ---- The search path ---------------------------------------------------

    [[nodiscard]] search_path& paths() { return paths_; }
    [[nodiscard]] const search_path& paths() const { return paths_; }

    // ---- Facts -------------------------------------------------------------

    [[nodiscard]] const asset_counters& counters() const { return counters_; }

    /// Assets still resident. At shutdown this is what a leak looks like, and it
    /// is the number explicit unload is traded against — so it is worth printing.
    [[nodiscard]] int live_count() const;

private:
    struct derivation
    {
        std::uint32_t source = 0;    ///< a mesh_handle's bits
        std::uint32_t derived = 0;   ///< …and the mesh it produced
    };

    /// Free `h` and its dependents without touching the name maps' iterators.
    int release_mesh(mesh_handle h);
    int release_texture(texture_handle h);

    /// Store a texture built FROM an image, with its lifetime bound to it.
    /// Private: unlike `derive_mesh`, nothing outside has a reason to derive a
    /// texture by hand — `load_texture` is the only producer, and keeping it
    /// that way is what guarantees every texture in the store has a source.
    [[nodiscard]] texture_handle derive_texture(image_handle source,
                                                std::string_view key,
                                                texture data);

    search_path paths_;

    mesh_pool meshes_;
    image_pool images_;
    texture_pool textures_;      ///< 6.6
    material_pool materials_;    ///< 6.6

    /// key (name + import settings) -> handle. The ONE source of truth for
    /// "have I seen this before".
    std::unordered_map<std::string, mesh_handle> mesh_by_key_;
    std::unordered_map<std::string, image_handle> image_by_key_;
    std::unordered_map<std::string, texture_handle> texture_by_key_;
    std::unordered_map<std::string, material_handle> material_by_key_;

    /// Every source -> derived edge. A flat vector rather than a multimap because
    /// it is walked whole on unload and is expected to hold tens of entries; if it
    /// ever holds thousands, sort it and binary-search.
    std::vector<derivation> mesh_derivations_;

    /// image -> texture edges. A SECOND list rather than a tag on the first;
    /// see `derived_count(image_handle)`.
    std::vector<derivation> texture_derivations_;

    asset_counters counters_;
};

}   // namespace engine
