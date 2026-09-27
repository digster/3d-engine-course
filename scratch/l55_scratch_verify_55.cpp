// scratch/verify_55.cpp — Lesson 5.5's harness: the asset system.
//
//   §A  the search path: order, override, and the names it refuses
//   §B  loaded ONCE — the same name twice is the same handle and no file read
//   §C  import settings are part of an asset's identity
//   §D  generated content goes through the same door as loaded content
//   §E  derived assets, and the cascade that closes 5.4's hole
//   §F  unloading out from under a live scene
//   §G  failure is ordinary: four ways to not get an asset, none of them a crash
//   §H  the golden is still byte-identical
//
// §F is the lesson. It builds a real scene, frees a mesh the scene is still
// holding a handle to, renders, and asserts that the ONLY consequence is one
// object missing and `collect_stats::unresolved == 1`. Before Lesson 5.4 that
// sequence was undefined behaviour; before 5.5 there was no way to perform it.
//
// Build and run:  sh scratch/build_verify_55.sh

#include <engine/asset/asset_store.hpp>
#include <engine/asset/search_path.hpp>
#include <engine/gfx/mesh.hpp>
#include <engine/gfx/obj.hpp>
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

/// A directory beside the executable, for the override tests. Returns "" if it
/// could not be made — every §A check that needs it is skipped rather than failed,
/// because a read-only build directory is a fact about the machine and not a bug
/// in the engine.
[[nodiscard]] std::string make_override_root()
{
    const char* base = SDL_GetBasePath();
    if (base == nullptr) { return {}; }
    std::string dir = std::string(base) + "verify55_override";
    if (!SDL_CreateDirectory(dir.c_str())) { return {}; }

    // A DIFFERENT torus, so "which one did I get" has an answer with a number in
    // it: 8x6 segments is 96 triangles against the real asset's 2,304.
    const engine::mesh_data small = engine::make_torus(8, 6, 1.0f, 0.4f);
    if (!engine::save_obj((dir + "/torus.obj").c_str(), small.view())) { return {}; }
    return dir;
}

// ---------------------------------------------------------------------------

void section_a_search_path(const std::string& override_root)
{
    std::printf("\n=== A. The search path ===\n");

    const engine::search_path std_path = engine::search_path::standard();
    checkf(std_path.roots().size() == 1, "the standard path has %zu root",
           std_path.roots().size());
    checkf(!std_path.roots().empty()
               && std_path.roots()[0].find("assets") != std::string::npos,
           "…and it is next to the executable: %s",
           std_path.roots().empty() ? "-" : std_path.roots()[0].c_str());

    const engine::resolved_path hit = std_path.resolve("torus.obj");
    checkf(hit.ok() && hit.root_index == 0, "torus.obj resolves under root %d",
           hit.root_index);
    checkf(hit.bytes > 0, "…and SDL hands us its size for free: %llu bytes",
           static_cast<unsigned long long>(hit.bytes));

    check(!std_path.resolve("no_such_asset.obj").ok(),
          "a name that is not there does not resolve");

    // The names it refuses, and each one is a different way to leave the roots.
    check(!std_path.resolve("../secret").ok(), "a name containing '..' is refused");
    check(std_path.resolve("../secret").refused,
          "…and says so, so the caller does not log a second line for it");
    check(!std_path.resolve("/etc/passwd").ok(), "an absolute name is refused");
    check(!std_path.resolve("").ok(), "an empty name is refused");
    check(!std_path.resolve("a/../../b").ok(), "'..' in the MIDDLE is refused too");

    // A directory is not a file. Without the type test this would report success
    // and fail later, inside a loader, with a much worse message.
    engine::search_path parent;
    const char* base = SDL_GetBasePath();
    if (base != nullptr)
    {
        parent.add_root(base);
        check(!parent.resolve("assets").ok(),
              "a DIRECTORY of the right name is not a hit");
    }

    if (override_root.empty())
    {
        std::printf("  [SKIP] override ordering — could not create a test root\n");
        return;
    }

    // THE WHOLE POINT OF AN ORDERED LIST: put a root in front and its copy wins.
    engine::search_path over = engine::search_path::standard();
    over.prepend_root(override_root);
    const engine::resolved_path shadowed = over.resolve("torus.obj");
    checkf(shadowed.ok() && shadowed.root_index == 0,
           "with an override root in front, torus.obj resolves under root 0");
    checkf(shadowed.path.find("verify55_override") != std::string::npos,
           "…and it is the override's copy, not the shipped one");
    checkf(shadowed.bytes != hit.bytes,
           "…proved by the size: %llu vs %llu bytes",
           static_cast<unsigned long long>(shadowed.bytes),
           static_cast<unsigned long long>(hit.bytes));

    engine::search_path under = engine::search_path::standard();
    under.add_root(override_root);
    checkf(under.resolve("torus.obj").root_index == 0,
           "appended instead, the shipped copy still wins — order is the feature");
}

void section_b_loaded_once()
{
    std::printf("\n=== B. Loaded once ===\n");

    engine::asset_store store;
    const engine::mesh_load first = store.load_mesh("torus.obj");
    checkf(first.ok() && !first.cached && first.report.vertices == 1225,
           "first acquire: %d vertices, cached=%d", first.report.vertices,
           static_cast<int>(first.cached));
    checkf(store.counters().files_read == 1 && store.counters().cache_hits == 0,
           "files_read=%d cache_hits=%d", store.counters().files_read,
           store.counters().cache_hits);

    const engine::mesh_load second = store.load_mesh("torus.obj");
    check(second.handle == first.handle, "the SAME name gives the SAME handle");
    check(second.cached, "…and says it came from the cache");
    checkf(store.counters().files_read == 1 && store.counters().cache_hits == 1,
           "…having read %d file(s), not 2 (hits=%d)",
           store.counters().files_read, store.counters().cache_hits);

    check(store.find_mesh("torus.obj") == first.handle,
          "find_mesh() answers without touching the disk");
    check(!store.find_mesh("torus.obj").valid()
              == false, "…and a resident name is non-null");
    check(!store.find_mesh("never_loaded.obj").valid(),
          "…and an unknown name is null rather than a load");

    checkf(store.name_of(first.handle) == "torus.obj",
           "the handle knows the key it was stored under: '%.*s'",
           static_cast<int>(store.name_of(first.handle).size()),
           store.name_of(first.handle).data());

    // The map is not the truth; the pool is. Unload behind the map's back and the
    // next acquire must reload rather than hand out a stale handle.
    const int went = store.unload_mesh(first.handle);
    checkf(went == 1, "unload released %d asset", went);
    const engine::mesh_load third = store.load_mesh("torus.obj");
    check(third.ok() && !third.cached, "after unloading, the name loads again");
    check(!(third.handle == first.handle), "…with a different handle");
    checkf(store.counters().files_read == 2, "…and a second file read (%d)",
           store.counters().files_read);
}

void section_c_import_settings()
{
    std::printf("\n=== C. Import settings are identity ===\n");

    engine::asset_store store;
    const engine::mesh_load flipped = store.load_mesh("torus.obj", {.flip_uv_v = true});
    const engine::mesh_load raw = store.load_mesh("torus.obj", {.flip_uv_v = false});

    check(flipped.ok() && raw.ok(), "both imports succeed");
    check(!(flipped.handle == raw.handle),
          "the same FILE imported two ways is two different assets");
    checkf(store.counters().files_read == 2 && store.counters().cache_hits == 0,
           "…so the file was read twice (%d) and nothing was a hit (%d)",
           store.counters().files_read, store.counters().cache_hits);

    // And they really do differ, which is the reason they must not share a slot.
    const engine::mesh_data* a = store.mesh_at(flipped.handle);
    const engine::mesh_data* b = store.mesh_at(raw.handle);
    bool uvs_differ = false;
    if (a != nullptr && b != nullptr && !a->uvs.empty() && a->uvs.size() == b->uvs.size())
    {
        for (std::size_t i = 0; i < a->uvs.size(); ++i)
        {
            if (a->uvs[i].y != b->uvs[i].y) { uvs_differ = true; break; }
        }
    }
    check(uvs_differ, "…and their uvs actually differ, which is why sharing would be wrong");

    check(store.find_mesh("torus.obj", {.flip_uv_v = false}) == raw.handle,
          "find_mesh() takes the settings too, or it would answer the wrong question");

    check(engine::asset_key("a.obj", {.flip_uv_v = true}) == "a.obj",
          "the DEFAULT import adds nothing to the key");
    check(engine::asset_key("a.obj", {.flip_uv_v = false}) == "a.obj|flip=0",
          "…and a non-default one is readable in a log and a debugger");
}

void section_d_generated_content()
{
    std::printf("\n=== D. Generated content ===\n");

    engine::asset_store store;
    const engine::mesh_handle cube =
        store.insert_mesh("cube", engine::to_mesh_data(engine::cube_mesh()));
    checkf(cube.valid() && store.mesh_at(cube) != nullptr
               && store.mesh_at(cube)->vertices.size() == 8,
           "generated content gets a name and a handle like anything else");
    check(store.find_mesh("cube") == cube, "…and is findable by that name");
    checkf(store.counters().inserted == 1 && store.counters().files_read == 0,
           "…without a file being opened (inserted=%d, files_read=%d)",
           store.counters().inserted, store.counters().files_read);

    // Re-inserting a name REPLACES. Two assets under one name is the state from
    // which no correct behaviour follows.
    const engine::mesh_handle again =
        store.insert_mesh("cube", engine::to_mesh_data(engine::icosahedron_mesh()));
    check(!(again == cube), "re-inserting a name gives a new handle");
    check(!store.meshes().contains(cube), "…and the old handle stops resolving");
    checkf(store.mesh_at(again)->vertices.size() == 12,
           "…and the name now finds the new asset (%zu vertices)",
           store.mesh_at(again)->vertices.size());
    checkf(store.live_count() == 1, "…with %d asset resident, not 2",
           store.live_count());
}

void section_e_derived_assets()
{
    std::printf("\n=== E. Derived assets ===\n");

    engine::asset_store store;
    const engine::mesh_load src = store.load_mesh("torus.obj");
    check(src.ok(), "a source mesh");

    const engine::mesh_handle flat =
        store.derive_mesh(src.handle,
                          engine::with_normals(store.mesh_at(src.handle)->view(),
                                               engine::normal_style::flat));
    checkf(flat.valid() && store.mesh_at(flat) != nullptr,
           "with_normals() output is stored as a DERIVED asset");
    check(store.name_of(flat).empty(),
          "…with no name, because nobody asked for it by one");
    checkf(store.derived_count(src.handle) == 1, "the source has %d dependent",
           store.derived_count(src.handle));

    // A chain: derived from derived. Real pipelines produce these (a mesh, its
    // import, its GPU upload) and a cascade that stops after one hop is a leak.
    const engine::mesh_handle smooth =
        store.derive_mesh(flat, engine::with_normals(store.mesh_at(flat)->view(),
                                                     engine::normal_style::smooth));
    checkf(smooth.valid() && store.derived_count(src.handle) == 2,
           "a chain of two: derived_count(source) = %d",
           store.derived_count(src.handle));
    checkf(store.live_count() == 3, "%d assets resident", store.live_count());

    // THE CASCADE — the hole Lesson 5.4 left, closed.
    const int went = store.unload_mesh(src.handle);
    checkf(went == 3, "unloading the source released %d assets, not 1", went);
    check(!store.meshes().contains(flat), "the derived mesh went with it");
    check(!store.meshes().contains(smooth), "…and so did the one derived from THAT");
    checkf(store.live_count() == 0, "%d assets left", store.live_count());

    // An orphan is refused rather than created, because nothing would ever free it.
    const engine::mesh_handle orphan =
        store.derive_mesh(src.handle, engine::to_mesh_data(engine::cube_mesh()));
    check(!orphan.valid(), "deriving from a stale source is refused, not leaked");
    checkf(store.live_count() == 0, "…leaving %d assets", store.live_count());

    // Replacing a name cascades too, which is what regenerating content means.
    const engine::mesh_handle gen =
        store.insert_mesh("floor", engine::to_mesh_data(engine::quad_mesh()));
    const engine::mesh_handle gen_derived =
        store.derive_mesh(gen, engine::to_mesh_data(engine::cube_mesh()));
    check(gen_derived.valid() && store.live_count() == 2, "a generated asset with a dependent");
    const engine::mesh_handle regen =
        store.insert_mesh("floor", engine::to_mesh_data(engine::quad_mesh()));
    check(regen.valid() && !store.meshes().contains(gen_derived),
          "re-inserting the name took the dependent with it");
    checkf(store.live_count() == 1, "%d asset resident", store.live_count());

    // unload_all: everything goes, and every handle goes stale rather than
    // pointing at a slot the next level will refill.
    const engine::mesh_load again = store.load_mesh("torus.obj");
    const int all = store.unload_all();
    checkf(all == 2, "unload_all released %d", all);
    check(!store.meshes().contains(again.handle), "…and its handles are stale");
    check(!store.find_mesh("torus.obj").valid(), "…and its names are gone");
    checkf(store.live_count() == 0, "%d live", store.live_count());
}

void section_f_unload_under_a_scene()
{
    std::printf("\n=== F. Unloading out from under a live scene ===\n");

    demo::scene_assets assets;
    assets.build();
    demo::floor_geometry floor;
    demo::build_floor(assets, floor, 4);
    demo::model_state model;
    model.generated = assets.store.insert_mesh("generated:torus",
                                               engine::make_torus(48, 24, 1.0f, 0.4f));
    demo::load_model(assets, model, demo::model_choice::torus, true);

    engine::scene_object scene[demo::k_max_objects];
    const int count = demo::build_scene(scene, demo::scene_kind::solids,
                                        demo::spin::about_z, 1.234f, assets,
                                        floor, model, 32.0f);
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
                              assets.meshes(), {cam.view(), cam.eye()}, pr, lights,
                              engine::render_options{}, &before);
    const std::size_t all_tris = tris.size();
    checkf(before.unresolved == 0 && all_tris == 44,
           "all three resolve: %zu triangles, %d unresolved", all_tris,
           before.unresolved);

    // Free the icosahedron — ONE object of the three uses it — while the scene
    // array still holds its handle. Nothing else changes.
    const int went = assets.store.unload_mesh(assets.icosahedron);
    checkf(went == 1, "unloaded the icosahedron: %d asset", went);

    engine::collect_stats after;
    engine::collect_triangles(tris, scratch, {scene, static_cast<std::size_t>(count)},
                              assets.meshes(), {cam.view(), cam.eye()}, pr, lights,
                              engine::render_options{}, &after);
    checkf(after.unresolved == 1, "exactly %d object skipped and COUNTED",
           after.unresolved);
    checkf(tris.size() == all_tris - 20,
           "…and the rest still drew: %zu triangles of %zu (the icosahedron's 20 gone)",
           tris.size(), all_tris);
    check(after.clip.input == static_cast<int>(tris.size()),
          "nothing else changed — the clipper saw exactly what was collected");

    // And the demo's own unload path, which is the [Bksp] key.
    const int model_went = demo::unload_model(assets, model);
    checkf(model_went >= 1, "unload_model released %d asset(s)", model_went);
    check(model.unloaded, "…and the demo recorded it");
    check(!assets.meshes().contains(model.geometry),
          "…and the handle the scene still holds is stale");

    // The refusal that protects the round-trip control.
    demo::model_state gen = model;
    gen.choice = demo::model_choice::generated;
    check(demo::unload_model(assets, gen) == 0,
          "unload_model refuses the generated control — naming an asset protects it");
}

void section_g_failure_is_ordinary()
{
    std::printf("\n=== G. Failure is ordinary ===\n");

    engine::asset_store store;

    const engine::mesh_load missing = store.load_mesh("no_such_file.obj");
    check(!missing.ok() && !missing.handle.valid(), "a missing file gives a null handle");
    check(missing.report.status == engine::obj_status::cannot_open,
          "…and a report that names the reason");

    const engine::mesh_load escaping = store.load_mesh("../../../etc/passwd");
    check(!escaping.ok(), "an escaping name gives a null handle");

    const engine::image_load bad_image = store.load_image("torus.obj");
    check(!bad_image.ok(), "an OBJ asked for as an image fails to decode");

    const engine::image_load good_image = store.load_image("uv_grid.png");
    checkf(good_image.ok() && good_image.report.width == 256,
           "…while a real image loads: %dx%d", good_image.report.width,
           good_image.report.height);
    check(store.load_image("uv_grid.png").cached, "images cache like meshes");
    check(store.find_image("uv_grid.png") == good_image.handle, "…and are findable");

    checkf(store.counters().loads_failed == 3,
           "%d failures, and not one of them a crash", store.counters().loads_failed);
    checkf(store.live_count() == 1, "%d asset resident — nothing half-loaded was kept",
           store.live_count());

    const int went = store.unload_image(good_image.handle);
    checkf(went == 1, "images unload too (%d)", went);
    check(store.unload_image(good_image.handle) == 0,
          "…and a second unload is refused rather than double-freeing");
}

void section_h_golden()
{
    std::printf("\n=== H. The golden ===\n");

    const char* path = "build/demos/verify_55.ppm";
    check(demo::write_reference_shot(path) == 0, "write_reference_shot() succeeded");

    const std::string ours = read_file(path);
    const std::string golden = read_file("scratch/shot_52.ppm");
    checkf(!ours.empty() && ours.size() == golden.size(),
           "same size: %zu bytes", ours.size());
    check(ours == golden,
          "BYTE-IDENTICAL to the golden written in Lesson 5.1 — five lessons now");
}

}   // namespace

int main()
{
    std::printf("verify_55 — Lesson 5.5: the asset system\n");

    const std::string override_root = make_override_root();

    section_a_search_path(override_root);
    section_b_loaded_once();
    section_c_import_settings();
    section_d_generated_content();
    section_e_derived_assets();
    section_f_unload_under_a_scene();
    section_g_failure_is_ordinary();
    section_h_golden();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
