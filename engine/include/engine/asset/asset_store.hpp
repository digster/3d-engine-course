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
#include <engine/gfx/image.hpp>
#include <engine/gfx/mesh.hpp>
#include <engine/gfx/obj.hpp>

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

    /// The handle for `name` if it is resident, or null. Never touches the disk.
    [[nodiscard]] mesh_handle find_mesh(std::string_view name,
                                        const mesh_import& settings = {}) const;
    [[nodiscard]] image_handle find_image(std::string_view name) const;

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

    // ---- Unloading ---------------------------------------------------------

    /// Release `h` and everything derived from it. Returns how many assets went.
    ///
    /// Zero means the handle was already stale — which is not an error, and is the
    /// answer a second unload of the same thing should give.
    int unload_mesh(mesh_handle h);
    int unload_image(image_handle h);

    /// Release everything. Returns the count. Every outstanding handle goes stale.
    int unload_all();

    // ---- Resolving ---------------------------------------------------------

    /// The pools, for whoever has to turn handles back into data — the renderer,
    /// mostly. Const: only the store may add to or remove from them.
    [[nodiscard]] const mesh_pool& meshes() const { return meshes_; }
    [[nodiscard]] const image_pool& images() const { return images_; }

    [[nodiscard]] const mesh_data* mesh_at(mesh_handle h) const { return meshes_.get(h); }
    [[nodiscard]] const image_data* image_at(image_handle h) const { return images_.get(h); }

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

    search_path paths_;

    mesh_pool meshes_;
    image_pool images_;

    /// key (name + import settings) -> handle. The ONE source of truth for
    /// "have I seen this before".
    std::unordered_map<std::string, mesh_handle> mesh_by_key_;
    std::unordered_map<std::string, image_handle> image_by_key_;

    /// Every source -> derived edge. A flat vector rather than a multimap because
    /// it is walked whole on unload and is expected to hold tens of entries; if it
    /// ever holds thousands, sort it and binary-search.
    std::vector<derivation> mesh_derivations_;

    asset_counters counters_;
};

}   // namespace engine
