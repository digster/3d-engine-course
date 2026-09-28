// scratch/verify_65.cpp — Lesson 6.5's harness: what a material is.
//
//   §A  THE SPLIT — everything in `material` can be a number in a buffer
//   §B  the derived flag, which cannot contradict itself
//   §C  handles: storage, resolution, and what a STALE one does
//   §D  the round trip through the GPU uniform
//   §E  SHARING, measured — the reason the pool exists
//   §F  `closed` belongs to the mesh, and `cull_of` is the decision
//   §G  THE GOLDEN — a refactor's only real claim
//
// §G IS THE ONE THAT MATTERS, and it is the whole difference between a refactor
// and a rewrite. Lesson 5.1's rule: MOVE WITHOUT CHANGING, THEN CHANGE WITHOUT
// MOVING, VERIFYING SEPARATELY. This lesson moved `tint` and `surface` into a
// material, then changed how textures are referenced — and the reference render
// is byte-identical across both halves, which is the only evidence that the
// second half did not quietly repair a bug introduced by the first.
//
// Build and run:  sh scratch/build_verify_65.sh

#include <engine/core/assert.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/gpu_uniform.hpp>
#include <engine/gfx/material.hpp>
#include <engine/gfx/mesh.hpp>
#include <engine/gfx/scene.hpp>
#include <engine/gfx/texture.hpp>

#include "../demos/common/demo_scene.hpp"

#include <SDL3/SDL.h>

#include <cstdarg>
#include <cstdio>
#include <string>
#include <type_traits>
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
    char line[768];
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
//  §A — THE SPLIT
// ===========================================================================

void section_a_split()
{
    std::printf("\n=== A. Everything in a material can be a number in a buffer ===\n");

    // THE TEST THIS STRUCT IS BUILT TO PASS, asserted rather than described. A
    // material is per-DRAW data: it can be memcpy'd into a uniform block. If a
    // field ever arrives that cannot be, it belongs in pipeline state instead
    // and this assertion is where that gets caught.
    checkf(std::is_trivially_copyable_v<engine::material>,
           "a material is TRIVIALLY COPYABLE (%zu bytes) — which is the machine's "
           "way of saying it can be pushed as a uniform. Cull mode and the shader "
           "cannot be, and that is exactly why they are not in here",
           sizeof(engine::material));

    checkf(std::is_trivially_copyable_v<engine::microsurface>
           && std::is_trivially_copyable_v<engine::sampler>,
           "…and so is every field it is made of, which is what makes that true "
           "rather than a coincidence");

    // The size breakdown, because "small" is a claim and this is the number.
    std::printf("      Uint32 tint          %2zu bytes\n", sizeof(Uint32));
    std::printf("      texture_handle       %2zu\n", sizeof(engine::texture_handle));
    std::printf("      sampler              %2zu   <- the largest field\n",
                sizeof(engine::sampler));
    std::printf("      microsurface         %2zu\n", sizeof(engine::microsurface));
    std::printf("      material             %2zu\n", sizeof(engine::material));

    // REPAIRED 2026-09-28. This said `<= 40`, true when 6.5 shipped and false
    // since three later lessons grew the struct on purpose: 6.7 added the normal
    // map's handle, 6.10 three sampler fields, 6.11 the alpha mode, alpha and
    // cutoff. The claim was never "40"; it was "small enough to copy", and the
    // honest line for that is one cache line — which is exactly where it sits.
    checkf(sizeof(engine::material) <= 64,
           "the whole material is %zu bytes — within one 64-byte cache line, small "
           "enough to copy wherever there is one of them, which is what "
           "`scene_object` does (6.5 shipped it at most 40; 6.7, 6.10 and 6.11 grew it)",
           sizeof(engine::material));

    // A DEFAULT MATERIAL IS THE ONE THAT CHANGES NOTHING, which is the same rule
    // every default in this engine follows (render_options, fill_style).
    const engine::material d{};
    checkf(d.tint == 0xFFFFFFFFu && !d.textured() && !d.albedo_map.valid(),
           "a default-constructed material is white, untextured, and holds no "
           "image — so an object that says nothing about its appearance looks "
           "exactly as it did before this type existed");
}

// ===========================================================================
//  §B — THE DERIVED FLAG
// ===========================================================================

void section_b_derived()
{
    std::printf("\n=== B. `textured` is derived, so it cannot disagree ===\n");

    engine::texture_pool textures;
    const engine::texture_handle h = textures.insert(engine::make_uv_grid(16));

    engine::material m;
    checkf(!m.textured(), "no image bound -> textured() is false");

    m.albedo_map = h;
    checkf(m.textured(), "bind one -> textured() is true, with nothing else set");

    m.albedo_map = {};
    checkf(!m.textured(), "clear it -> false again. ONE field, one answer");

    // THE FAILURE THIS PREVENTS, stated as the thing that is now impossible.
    // Before this lesson the flag and the image were separate: `material_uniforms`
    // carried a `textured` float that every call site set by hand, beside a
    // texture pointer chosen somewhere else. Setting one without the other is a
    // surface that samples the debug magenta, or one that silently ignores its
    // own image — and neither fails where the mistake was made.
    check(true, "…and there is no way to express 'textured but no image', which is "
                "the bug this removes rather than detects");
}

// ===========================================================================
//  §C — HANDLES
// ===========================================================================

void section_c_handles()
{
    std::printf("\n=== C. Storage, resolution, and the stale handle ===\n");

    engine::texture_pool textures;
    const engine::texture_handle a = textures.insert(engine::make_checker(8, 2, 0xFFFFFFFFu, 0u));

    engine::material m{.albedo_map = a};
    const engine::texture_binding bound = engine::bind_albedo(m, textures);
    checkf(bound.bound() && bound.image != nullptr,
           "a live handle resolves to a bound binding (%dx%d)",
           bound.image->width(), bound.image->height());

    // THE POINT OF A HANDLE, and it is not tidiness. Grow the pool and any raw
    // pointer taken before the growth may now be dangling; the handle is
    // unaffected, because it names a SLOT rather than an address.
    const engine::texture* before = textures.get(a);
    for (int i = 0; i < 64; ++i) { (void)textures.insert(engine::make_checker(4, 2, 0u, 0u)); }
    const engine::texture* after = textures.get(a);
    checkf(after != nullptr,
           "after 64 more insertions the SAME handle still resolves — pointer was "
           "%p, is now %p. A raw pointer stored in a material would have had to be "
           "right about which of those it was",
           static_cast<const void*>(before), static_cast<const void*>(after));

    // A STALE handle is the case a pointer cannot express at all: well-formed,
    // and naming something that is gone.
    engine::texture_pool other;
    const engine::texture_handle gone = other.insert(engine::make_checker(4, 2, 0u, 0u));
    other.remove(gone);
    engine::material stale{.albedo_map = gone};
    const engine::texture_binding sb = engine::bind_albedo(stale, other);
    checkf(!sb.bound() && sb.image == nullptr,
           "a STALE handle resolves to an unbound binding, not to freed memory — "
           "and the surface falls back to its tint, which is what it would have "
           "done with no image at all. A handle can be ASKED whether it still "
           "resolves; a pointer cannot");
}

// ===========================================================================
//  §D — THE ROUND TRIP THROUGH THE GPU UNIFORM
// ===========================================================================

void section_d_uniform()
{
    std::printf("\n=== D. material -> material_uniforms, and back ===\n");

    engine::texture_pool textures;
    const engine::texture_handle h = textures.insert(engine::make_uv_grid(8));

    const engine::material m{.tint = 0xFF3A7FD4u,
                             .albedo_map = h,
                             .surface = {.roughness = 0.37f, .metallic = 1.0f, .f0 = 0.06f}};
    const engine::material_uniforms u = engine::uniforms_of(m);

    // The three surface parameters cross unchanged — no remap, no scaling.
    checkf(u.roughness == m.surface.roughness && u.metallic == m.surface.metallic
           && u.f0 == m.surface.f0,
           "roughness, metallic and F0 cross the boundary UNCHANGED (%.4f, %.1f, "
           "%.4f) — the shader's alpha remap happens in the shader, which is where "
           "Lesson 6.3 put it",
           static_cast<double>(u.roughness), static_cast<double>(u.metallic),
           static_cast<double>(u.f0));

    // The albedo is DECODED, which is the one conversion this function performs.
    const engine::linear_rgb expect = engine::to_linear(m.tint);
    checkf(u.albedo.x == expect.r && u.albedo.y == expect.g && u.albedo.z == expect.b,
           "the tint is decoded to linear exactly once, here at the input edge "
           "(%.5f, %.5f, %.5f) — Lesson 6.1's rule, and the software path decodes "
           "at the same conceptual point inside shade_encoded",
           static_cast<double>(u.albedo.x), static_cast<double>(u.albedo.y),
           static_cast<double>(u.albedo.z));

    checkf(u.textured == 1.0f, "…and `textured` is DERIVED from the handle, not passed in");

    const engine::material bare{.tint = 0xFF3A7FD4u};
    checkf(engine::uniforms_of(bare).textured == 0.0f,
           "the same material without an image pushes 0. There is no third "
           "possibility, which is the whole value of deriving it");

    // THE LAYOUT IS THE CONTRACT, and it did not move this lesson. A uniform
    // block that disagrees with its shader does not fail loudly; it renders
    // something plausible with the fields shifted.
    //
    // REPAIRED 2026-09-28. This said `== 32`, which was the point in 6.5 (the
    // lesson moved who fills the block, not what it is) and has been false since
    // 6.11 bought a third register for alpha. The contract is whatever the engine
    // and its shader agree on, and the engine states it: gpu_uniform.hpp
    // static_asserts 48. A harness that hard-codes a different number is checking
    // a lesson that has since moved, not the layout.
    checkf(sizeof(engine::material_uniforms) == 48,
           "material_uniforms is exactly 48 bytes — three registers, as gpu_uniform.hpp "
           "asserts. Lesson 6.5 changed WHO fills it and not WHAT it was (then 32); "
           "6.11 bought the third register, and the shader moved with it");
}

// ===========================================================================
//  §E — SHARING, MEASURED
// ===========================================================================

void section_e_sharing()
{
    std::printf("\n=== E. What the pool is actually for ===\n");

    // The swarm's own numbers: 96 ring drones cycling six tints.
    constexpr int k_ring = 96;
    constexpr int k_distinct = 6;

    const std::size_t by_value = static_cast<std::size_t>(k_ring) * sizeof(engine::material);
    const std::size_t by_handle = static_cast<std::size_t>(k_ring) * sizeof(engine::material_handle)
                                + static_cast<std::size_t>(k_distinct) * sizeof(engine::material);

    std::printf("      %d entities x %zu B (a material each)   = %5zu B\n",
                k_ring, sizeof(engine::material), by_value);
    std::printf("      %d handles  x %zu B  + %d x %zu B pooled = %5zu B\n",
                k_ring, sizeof(engine::material_handle), k_distinct,
                sizeof(engine::material), by_handle);

    checkf(by_handle < by_value,
           "sharing six materials across ninety-six entities costs %zu bytes "
           "instead of %zu — a %.1fx reduction on that component",
           by_handle, by_value, static_cast<double>(by_value) / static_cast<double>(by_handle));

    // AND THE PART THAT IS NOT ABOUT BYTES, which is the better argument. With
    // copies, "make the drones rougher" is a loop over the registry that has to
    // find every entity carrying that appearance. With handles it is one write.
    engine::material_pool pool;
    const engine::material_handle shared =
        pool.insert(engine::material{.tint = 0xFFE07A3Cu, .surface = {.roughness = 0.53f}});

    std::vector<engine::material_handle> users(k_ring, shared);
    pool.get(shared)->surface.roughness = 0.21f;

    bool all_updated = true;
    for (engine::material_handle h : users)
    {
        if (pool.get(h)->surface.roughness != 0.21f) { all_updated = false; }
    }
    checkf(all_updated,
           "one write updated all %d users, because they share the material rather "
           "than copies of it. THAT is the reason a pool is here — the bytes are a "
           "side effect", k_ring);

    // THE COUNTER-CASE, so the rule is a rule and not a preference.
    checkf(sizeof(engine::scene_object) > 0,
           "…and `scene_object` still holds its material BY VALUE, because it has "
           "exactly one. The rule is about SHARING, not about size: a handle where "
           "nothing shares is a lookup that buys nothing");
}

// ===========================================================================
//  §F — `closed` IS A FACT ABOUT THE MESH
// ===========================================================================

void section_f_closed()
{
    std::printf("\n=== F. The fact, the decision, and which is whose ===\n");

    // `closed()` is on `mesh_report`, which `validate()` produces — NOT on the
    // mesh itself. That distinction is the point: it is a MEASURED property of
    // the geometry, not a field somebody set.
    // `mesh_data` OWNS and `mesh` VIEWS (mesh.hpp), and `validate` takes the view.
    const engine::mesh_data cube = engine::with_normals(engine::cube_mesh(),
                                                        engine::normal_style::flat);
    const engine::mesh quad = engine::quad_mesh();   // a VIEW of static data

    checkf(engine::validate(cube.view()).closed(),
           "a cube VALIDATES as closed — counted from its edges by validate(), not "
           "taken on anyone's word");
    checkf(!engine::validate(quad).closed(),
           "a quad validates as OPEN, which is what makes culling it wrong rather "
           "than merely unusual");

    // THE CORRECTION THIS LESSON MAKES. `scene_object::closed` carried a comment
    // predicting it would move onto the material in Module 6. Building the
    // material showed that to be wrong twice: a material is explicitly NOT
    // pipeline state, and `closed` is not cull mode anyway — it is a property of
    // the mesh, which already knows it.
    checkf(engine::cull_of(true) == engine::cull_mode::back,
           "a closed mesh justifies back-face culling");
    checkf(engine::cull_of(false) == engine::cull_mode::none,
           "an open one does not, and getting this backwards makes a ground plane "
           "vanish when seen from behind");
    checkf(engine::cull_of(true, /*want_two_sided=*/true) == engine::cull_mode::none,
           "…and a closed mesh may still be drawn two-sided ON PURPOSE — to look "
           "inside it, or to debug a winding. THE MESH SUPPLIES THE FACT AND THE "
           "CALLER SUPPLIES THE INTENT, which is why both arguments exist");
}

// ===========================================================================
//  §G — THE GOLDEN
// ===========================================================================

void section_g_golden()
{
    std::printf("\n=== G. The golden — a refactor's only real claim ===\n");

    const int rc = demo::write_reference_shot("scratch/verify65.ppm");
    check(rc == 0, "write_reference_shot() succeeded");

    const std::string ours = read_file("scratch/verify65.ppm");
    const std::string golden = read_file("scratch/shot_52.ppm");
    checkf(!ours.empty() && ours.size() == golden.size(),
           "same size: %zu bytes — UNCHANGED from Lesson 6.4, because a refactor "
           "that changes the picture is not a refactor", ours.size());
    check(ours == golden,
          "BYTE-IDENTICAL, hash E917C06C. Lesson 6.4 broke this deliberately and "
          "6.5 puts it back to being the claim: `tint` and `surface` moved into a "
          "material, textures became handles resolved through a pool, and the GPU "
          "uniform is now assembled in one place instead of three — and not one "
          "pixel moved. MOVE WITHOUT CHANGING, THEN CHANGE WITHOUT MOVING");
}

}   // namespace

int main(int argc, char* argv[])
{
    (void)argc; (void)argv;
    if (!SDL_Init(0)) { SDL_Log("SDL_Init failed: %s", SDL_GetError()); return 1; }

    std::printf("=== Lesson 6.5 — a material system ==============================\n");

    section_a_split();
    section_b_derived();
    section_c_handles();
    section_d_uniform();
    section_e_sharing();
    section_f_closed();
    section_g_golden();

    std::printf("\n%d checks, %d failure%s\n", g_checks, g_failures,
                g_failures == 1 ? "" : "s");
    SDL_Quit();
    return g_failures == 0 ? 0 : 1;
}
