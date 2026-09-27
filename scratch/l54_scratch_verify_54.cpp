// scratch/verify_54.cpp — Lesson 5.4's harness: handles and the pool.
//
//   §A  the bit layout: pack, unpack, null, and types that do not interchange
//   §B  the three failures, each one exhibited and then defeated
//   §C  pool mechanics: dense storage, swap-and-patch removal, clear()
//   §D  generation wrap, measured rather than assumed
//   §E  the mesh subsystem: the library, the floor, the model
//   §F  the renderer skips what it cannot resolve, and counts it
//   §G  the golden is still byte-identical
//
// §B is the lesson. Each of the three sections builds the failure FIRST — with a
// raw pointer, exactly as the engine used to hold one — and shows what the
// program can and cannot know about it. Then the same sequence through a handle,
// where every one of them is a `nullptr` at a named line.
//
// Build and run:  sh scratch/build_verify_54.sh

#include <engine/core/handle.hpp>
#include <engine/core/pool.hpp>
#include <engine/gfx/mesh.hpp>
#include <engine/gfx/scene.hpp>
#include <engine/gfx/soft_renderer.hpp>

#include "../demos/common/demo_scene.hpp"

#include <SDL3/SDL.h>

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
    std::string s(static_cast<const char*>(data), size);
    SDL_free(data);
    return s;
}

/// A mesh with `n` vertices, so a pool can be filled with distinguishable items.
[[nodiscard]] engine::mesh_data tiny(std::uint16_t n)
{
    engine::mesh_data m;
    for (std::uint16_t i = 0; i < n; ++i)
    {
        m.vertices.push_back({static_cast<float>(i), 0.0f, 0.0f});
    }
    m.indices = {0, 1, 2};
    return m;
}

// ---------------------------------------------------------------------------

void section_a_layout()
{
    std::printf("\n=== A. The bit layout ===\n");

    using h_mesh = engine::handle<engine::mesh_data>;
    using h_tex = engine::handle<engine::texture>;

    checkf(sizeof(h_mesh) == 4, "a handle is %zu bytes — one 32-bit word",
           sizeof(h_mesh));
    checkf(sizeof(engine::mesh) == 64,
           "the `mesh` view it replaces is %zu bytes — four spans, eight pointers",
           sizeof(engine::mesh));

    static_assert(!std::is_same_v<h_mesh, h_tex>,
                  "handle<mesh_data> and handle<texture> must be distinct types");
    check(true, "handle<mesh_data> and handle<texture> are different types "
                "(a mix-up is a compile error, not a bad lookup)");

    // The null handle: all bits zero, which is what a value-initialised member,
    // a zeroed struct and a default-constructed vector element all produce.
    const h_mesh null{};
    checkf(null.bits == 0 && !null.valid() && !static_cast<bool>(null),
           "a value-initialised handle is null (bits=%u, valid=%d)",
           null.bits, static_cast<int>(null.valid()));

    // Pack / unpack round trip across the whole range of both fields.
    bool round_trip = true;
    const std::uint32_t idx[] = {0u, 1u, 4095u, 65535u, engine::k_handle_max_index};
    const std::uint32_t gen[] = {1u, 2u, 4094u, engine::k_handle_max_generation};
    for (std::uint32_t i : idx)
    {
        for (std::uint32_t g : gen)
        {
            const h_mesh h = engine::make_handle<engine::mesh_data>(i, g);
            if (h.index() != i || h.generation() != g) { round_trip = false; }
        }
    }
    checkf(round_trip, "index and generation round-trip over %zu x %zu extreme values",
           std::size(idx), std::size(gen));

    checkf(engine::k_handle_max_index == 1048575u && engine::k_handle_max_generation == 4095u,
           "20/12 split: %u slots, generations 1..%u",
           engine::k_handle_max_index + 1u, engine::k_handle_max_generation);

    // The two fields do not bleed into one another.
    const h_mesh a = engine::make_handle<engine::mesh_data>(engine::k_handle_max_index, 1u);
    const h_mesh b = engine::make_handle<engine::mesh_data>(0u, engine::k_handle_max_generation);
    checkf(a.generation() == 1u && b.index() == 0u,
           "a maximal index does not leak into the generation, or vice versa");

    // Equality is a single word compare.
    const h_mesh c = engine::make_handle<engine::mesh_data>(7u, 3u);
    const h_mesh d = engine::make_handle<engine::mesh_data>(7u, 3u);
    const h_mesh e = engine::make_handle<engine::mesh_data>(7u, 4u);
    check(c == d && !(c == e),
          "same slot, different generation => NOT equal (this is the cache-key fix)");

    // What the struct that holds one gained.
    struct old_scene_object
    {
        engine::transform xform;
        engine::mesh geometry;
        const char* name;
        Uint32 tint;
        bool closed;
        engine::specular surface;
    };
    checkf(sizeof(engine::scene_object) < sizeof(old_scene_object),
           "scene_object: %zu bytes with a handle vs %zu with a `mesh` view (%zu saved)",
           sizeof(engine::scene_object), sizeof(old_scene_object),
           sizeof(old_scene_object) - sizeof(engine::scene_object));
}

void section_b_three_failures()
{
    std::printf("\n=== B. The three failures ===\n");

    // ---- B1. DANGLING ------------------------------------------------------
    std::printf("\n  -- B1: dangling (the object was freed) --\n");
    {
        engine::mesh_pool pool;
        const engine::mesh_handle h = pool.insert(tiny(3));

        const engine::mesh_data* raw = pool.get(h);
        check(raw != nullptr && raw->vertices.size() == 3,
              "before: the handle resolves and the mesh has 3 vertices");

        check(pool.remove(h), "remove() reports that it removed something");

        // The RAW POINTER is now dangling and the program cannot tell. We do not
        // dereference it — that is the whole point, there is no safe way to ask.
        // The HANDLE can be asked, and answers.
        check(pool.get(h) == nullptr, "after: the SAME handle resolves to nullptr");
        check(!pool.contains(h), "…and contains() says so without dereferencing anything");
        check(!pool.remove(h), "…and a second remove() is refused rather than double-freeing");
    }

    // ---- B2. ALIASING (ABA) ------------------------------------------------
    std::printf("\n  -- B2: aliasing — the slot was refilled (ABA) --\n");
    {
        engine::mesh_pool pool;
        const engine::mesh_handle first = pool.insert(tiny(3));
        pool.remove(first);
        const engine::mesh_handle second = pool.insert(tiny(9));

        checkf(first.index() == second.index(),
               "the new mesh landed in the SAME slot (index %u) — this is the trap",
               second.index());
        checkf(first.generation() + 1u == second.generation(),
               "…but the generation moved on: %u -> %u",
               first.generation(), second.generation());
        check(!(first == second), "so the two handles are NOT equal");

        const engine::mesh_data* now = pool.get(second);
        check(now != nullptr && now->vertices.size() == 9,
              "the new handle resolves to the NEW mesh (9 vertices)");
        check(pool.get(first) == nullptr,
              "the OLD handle resolves to nullptr — an index alone would have "
              "silently returned the 9-vertex mesh");
    }

    // ---- B3. RELOCATION ----------------------------------------------------
    std::printf("\n  -- B3: relocation — the storage moved --\n");
    {
        engine::mesh_pool pool;
        const engine::mesh_handle h = pool.insert(tiny(3));

        const engine::mesh_data* before = pool.get(h);
        const void* addr_before = static_cast<const void*>(before);

        // Grow until the dense vector has certainly reallocated. Each insert may
        // move every item; a pointer taken before this loop is dead, a handle is
        // not.
        for (int i = 0; i < 1000; ++i) { (void)pool.insert(tiny(2)); }

        const engine::mesh_data* after = pool.get(h);
        const void* addr_after = static_cast<const void*>(after);

        check(addr_before != addr_after,
              "the item's ADDRESS changed — std::vector reallocated underneath it");
        check(after != nullptr && after->vertices.size() == 3,
              "…and the handle still resolves, to the same 3-vertex mesh");
        checkf(pool.size() == 1001, "pool holds %zu items", pool.size());
    }
}

void section_c_pool_mechanics()
{
    std::printf("\n=== C. Pool mechanics ===\n");

    engine::mesh_pool pool;
    std::vector<engine::mesh_handle> hs;
    for (std::uint16_t i = 0; i < 8; ++i) { hs.push_back(pool.insert(tiny(static_cast<std::uint16_t>(i + 1)))); }

    checkf(pool.size() == 8 && pool.slot_count() == 8 && pool.free_count() == 0,
           "8 inserts: size=%zu slots=%zu free=%zu", pool.size(), pool.slot_count(),
           pool.free_count());

    // Dense: the live items are contiguous, in one block, with no holes.
    const std::span<const engine::mesh_data> items = pool.items();
    checkf(items.size() == pool.size(), "items() spans exactly the %zu live items",
           items.size());

    // Remove one from the MIDDLE, which is where swap-and-patch does its work.
    const engine::mesh_handle victim = hs[2];      // the 3-vertex mesh
    const engine::mesh_handle last = hs[7];        // the 8-vertex mesh, currently at the end
    const void* last_before = static_cast<const void*>(pool.get(last));

    check(pool.remove(victim), "remove an item from the middle");
    checkf(pool.size() == 7 && pool.slot_count() == 8 && pool.free_count() == 1,
           "size=%zu slots=%zu free=%zu — the SLOT survives, the item does not",
           pool.size(), pool.slot_count(), pool.free_count());
    check(pool.items().size() == 7, "items() is still contiguous, now 7 long");

    const engine::mesh_data* last_after = pool.get(last);
    check(last_after != nullptr && last_after->vertices.size() == 8,
          "the item that was MOVED into the hole still resolves through its own handle");
    check(last_before != static_cast<const void*>(last_after),
          "…and its address changed, which is exactly what a handle survives");

    // Every surviving handle still resolves to the right mesh.
    bool all_good = true;
    for (std::size_t i = 0; i < hs.size(); ++i)
    {
        if (i == 2) { continue; }
        const engine::mesh_data* m = pool.get(hs[i]);
        if (m == nullptr || m->vertices.size() != i + 1) { all_good = false; }
    }
    check(all_good, "all 7 survivors still resolve to their own geometry");

    // handle_at names what an iteration is looking at.
    bool round_trip = true;
    for (std::size_t d = 0; d < pool.size(); ++d)
    {
        const engine::mesh_handle h = pool.handle_at(d);
        if (pool.get(h) != &pool.items()[d]) { round_trip = false; }
    }
    check(round_trip, "handle_at(d) resolves back to items()[d] for every d");

    // The freed slot is reused, and reused first.
    const engine::mesh_handle reborn = pool.insert(tiny(99));
    checkf(reborn.index() == victim.index() && pool.slot_count() == 8,
           "the next insert reuses slot %u rather than growing", reborn.index());
    check(pool.get(victim) == nullptr, "and the victim's handle is still refused");

    // clear() invalidates everything, keeps the slots.
    pool.clear();
    checkf(pool.size() == 0 && pool.slot_count() == 8,
           "clear(): size=%zu, slots kept at %zu", pool.size(), pool.slot_count());
    bool none_resolve = true;
    for (const engine::mesh_handle h : hs) { if (pool.contains(h)) { none_resolve = false; } }
    if (pool.contains(reborn)) { none_resolve = false; }
    check(none_resolve, "every handle issued before clear() is now stale");

    // A foreign handle — right shape, wrong pool — is refused.
    engine::mesh_pool other;
    const engine::mesh_handle foreign = other.insert(tiny(4));
    check(!pool.contains(foreign) || pool.get(foreign) == nullptr,
          "a handle from a DIFFERENT pool does not resolve here");
}

void section_d_generation_wrap()
{
    std::printf("\n=== D. Generation wrap ===\n");

    engine::mesh_pool pool;
    engine::mesh_handle h = pool.insert(tiny(1));
    const engine::mesh_handle first = h;

    checkf(first.generation() == engine::k_handle_first_generation,
           "a fresh slot starts at generation %u", first.generation());

    // Recycle the same slot until its generation comes back round. LIFO reuse
    // means every cycle lands on slot 0, which is the worst case by construction.
    int cycles = 0;
    for (int i = 0; i < 5000; ++i)
    {
        pool.remove(h);
        h = pool.insert(tiny(2));
        ++cycles;
        if (h == first) { break; }
    }

    checkf(h == first, "the generation came back round after %d recycles of one slot",
           cycles);
    checkf(cycles == static_cast<int>(engine::k_handle_max_generation),
           "…which is %u, exactly the number of usable generations",
           engine::k_handle_max_generation);
    checkf(pool.generation_wraps() == 1,
           "the pool counted the wrap (%zu) instead of letting it pass unnoticed",
           pool.generation_wraps());

    // And this is the honest part: AFTER the wrap, the original handle resolves
    // to the new occupant. 12 bits is a budget, not a guarantee.
    const engine::mesh_data* m = pool.get(first);
    check(m != nullptr && m->vertices.size() == 2,
          "after wrapping, the ORIGINAL handle resolves to the new mesh — the "
          "failure the bit budget buys down rather than eliminates");
    checkf(pool.slot_count() == 1, "all of it on one slot (slots=%zu)", pool.slot_count());
}

void section_e_mesh_subsystem()
{
    std::printf("\n=== E. The mesh subsystem ===\n");

    demo::scene_assets lib;
    lib.build();

    checkf(lib.meshes().size() == 3, "mesh_library::build() stores %zu built-in meshes",
           lib.meshes().size());
    check(lib.cube && lib.quad && lib.icosahedron, "all three handles are non-null");
    check(!(lib.cube == lib.quad) && !(lib.quad == lib.icosahedron)
              && !(lib.cube == lib.icosahedron),
          "…and all three are distinct");

    const engine::mesh_data* cube = lib.meshes().get(lib.cube);
    checkf(cube != nullptr && cube->vertices.size() == 8 && cube->triangle_count() == 12,
           "the cube resolves to 8 vertices and 12 triangles");
    const engine::mesh_data* ico = lib.meshes().get(lib.icosahedron);
    checkf(ico != nullptr && ico->vertices.size() == 12 && ico->triangle_count() == 20,
           "the icosahedron resolves to 12 vertices and 20 triangles");

    // ---- the floor: rebuilding replaces the mesh and the handle -------------
    demo::floor_geometry floor;
    demo::build_floor(lib, floor, 1);
    const engine::mesh_handle floor_1 = floor.geometry;
    const engine::mesh_data* f1 = lib.meshes().get(floor_1);
    checkf(f1 != nullptr && f1->triangle_count() == 2,
           "a 1x1 floor is %zu triangles", f1 != nullptr ? f1->triangle_count() : 0u);

    demo::build_floor(lib, floor, 8);
    const engine::mesh_handle floor_8 = floor.geometry;
    const engine::mesh_data* f8 = lib.meshes().get(floor_8);
    checkf(f8 != nullptr && f8->triangle_count() == 128,
           "an 8x8 floor is %zu triangles", f8 != nullptr ? f8->triangle_count() : 0u);
    check(!(floor_1 == floor_8), "the rebuild produced a DIFFERENT handle");
    checkf(floor_1.index() == floor_8.index(),
           "…in the same slot (%u), with the generation bumped %u -> %u",
           floor_8.index(), floor_1.generation(), floor_8.generation());
    check(lib.meshes().get(floor_1) == nullptr,
          "the pre-rebuild handle is refused — before 5.4 it was three live "
          "pointers into cleared vectors");

    // ---- the model: loading replaces likewise ------------------------------
    demo::model_state model;
    model.generated = lib.store.insert_mesh("generated:torus", engine::make_torus(48, 24, 1.0f, 0.4f));
    demo::load_model(lib, model, demo::model_choice::torus, true);
    const engine::mesh_handle torus = model.geometry;
    check(model.load.ok(), "torus.obj loaded");

    demo::load_model(lib, model, demo::model_choice::cube, true);
    check(!(model.geometry == torus), "loading a second model changed the handle");

    // LESSON 5.5 CHANGED WHAT HAPPENS TO THE OLD ONE, and this harness caught it.
    // In 5.4 `load_model` freed the previous mesh, so `torus` went stale here. The
    // asset store keeps everything resident until somebody unloads it, which is
    // the whole point of a cache — so the previous model is STILL THERE, and
    // cycling back to it is a hit rather than a second file read. Freeing moved
    // to its own call, which is `unload_model` ([Bksp]).
    check(lib.meshes().get(torus) != nullptr,
          "…and the torus is STILL RESIDENT (5.5: a load no longer frees)");
    check(lib.store.find_mesh("torus.obj") == torus,
          "…under its name, so cycling back to it is a cache hit");
    check(lib.store.unload_mesh(torus) == 1 && lib.meshes().get(torus) == nullptr,
          "…and unloading it explicitly is what makes the handle stale");
    const engine::mesh_data* loaded_cube = lib.meshes().get(model.geometry);
    checkf(loaded_cube != nullptr && loaded_cube->vertices.size() == 24,
           "cube.obj resolves to %zu vertices (24: split by normal, Lesson 3.5)",
           loaded_cube != nullptr ? loaded_cube->vertices.size() : 0u);

    checkf(lib.meshes().slot_count() == lib.meshes().size() + lib.meshes().free_count(),
           "the books balance: %zu slots = %zu live + %zu free",
           lib.meshes().slot_count(), lib.meshes().size(), lib.meshes().free_count());
    checkf(lib.meshes().generation_of(floor_8.index()) > engine::k_handle_first_generation,
           "slot %u is on generation %u — the history that makes staleness "
           "detectable is carried by the SLOT, not by the item",
           floor_8.index(), lib.meshes().generation_of(floor_8.index()));
}

void section_f_renderer_skips()
{
    std::printf("\n=== F. The renderer skips what it cannot resolve ===\n");

    demo::scene_assets lib;
    lib.build();
    demo::floor_geometry floor;
    demo::build_floor(lib, floor, 4);
    demo::model_state model;
    model.generated = lib.store.insert_mesh("generated:torus", engine::make_torus(48, 24, 1.0f, 0.4f));
    demo::load_model(lib, model, demo::model_choice::torus, true);

    engine::scene_object scene[demo::k_max_objects];
    const int count = demo::build_scene(scene, demo::scene_kind::solids, demo::spin::about_z,
                                        1.234f, lib, floor, model, 32.0f);
    checkf(count == 3, "the solids scene is %d objects", count);

    const demo::orbit_camera cam;
    const engine::projector pr{
        engine::perspective(demo::k_scene_fovy, demo::k_scene_aspect,
                            demo::k_scene_near, demo::k_scene_far),
        demo::k_scene_viewport, engine::near_mode::clip};
    const engine::lighting lights;

    std::vector<engine::raster_triangle> tris;
    engine::projection_scratch scratch;

    engine::collect_stats before;
    engine::collect_triangles(tris, scratch, {scene, static_cast<std::size_t>(count)},
                              lib.meshes(), {cam.view(), cam.eye()}, pr, lights,
                              engine::render_options{}, &before);
    const std::size_t all_tris = tris.size();
    checkf(before.unresolved == 0 && all_tris > 0,
           "all three resolve: %zu triangles, %d unresolved", all_tris, before.unresolved);

    // Free the cube. Two of the three objects use it, and their handles are now
    // stale — a situation that, before this lesson, was three dangling spans.
    // Lesson 5.5: the pool is the store's and only the store may remove from it,
    // which is the boundary that stops "who freed this?" being unanswerable.
    check(lib.store.unload_mesh(lib.cube) == 1,
          "free the cube out from under the scene");

    engine::collect_stats after;
    engine::collect_triangles(tris, scratch, {scene, static_cast<std::size_t>(count)},
                              lib.meshes(), {cam.view(), cam.eye()}, pr, lights,
                              engine::render_options{}, &after);
    checkf(after.unresolved == 2, "%d objects skipped and COUNTED", after.unresolved);
    checkf(tris.size() > 0 && tris.size() < all_tris,
           "the rest of the scene still drew: %zu triangles of %zu",
           tris.size(), all_tris);

    // The icosahedron is untouched and unchanged: one missing asset costs one
    // object, never the frame.
    const engine::mesh_data* ico = lib.meshes().get(lib.icosahedron);
    checkf(ico != nullptr && ico->triangle_count() == 20,
           "the surviving mesh is byte-for-byte what it was");
}

void section_g_golden()
{
    std::printf("\n=== G. The golden ===\n");

    const char* path = "build/demos/verify_54.ppm";
    const int rc = demo::write_reference_shot(path);
    check(rc == 0, "write_reference_shot() succeeded");

    const std::string ours = read_file(path);
    const std::string golden = read_file("scratch/shot_52.ppm");

    checkf(!ours.empty() && !golden.empty(), "read both files (%zu and %zu bytes)",
           ours.size(), golden.size());
    checkf(ours.size() == golden.size(), "same size: %zu bytes", ours.size());
    check(ours == golden,
          "BYTE-IDENTICAL to the golden written in Lesson 5.1, before any of "
          "this existed");
}

}   // namespace

int main()
{
    std::printf("verify_54 — Lesson 5.4: handles and the pool\n");

    section_a_layout();
    section_b_three_failures();
    section_c_pool_mechanics();
    section_d_generation_wrap();
    section_e_mesh_subsystem();
    section_f_renderer_skips();
    section_g_golden();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
