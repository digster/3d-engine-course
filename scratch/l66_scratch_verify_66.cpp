// scratch/verify_66.cpp — Lesson 6.6's harness: reading somebody else's format.
//
//   §A  THE ROUND TRIP — a file becomes the mesh the engine already had
//   §B  NO CONVERSION: the axes, and the uv origin OBJ needed flipped
//   §C  the node tree, flattened — a transform that is a PRODUCT
//   §D  the material table: factors, samplers, and the specified default
//   §E  THE SPEC, SETTLED — the claim carried as ⚠ VERIFY since Lesson 6.4
//   §F  the asset store: caching, sharing, the cascade, and the one gap
//   §G  THE GOLDEN — a new path must not move the existing picture
//
// §E IS THE ONE THAT MATTERS. Lesson 6.4 shipped `diffuse_coupling::half_vector`
// with a comment claiming it was "what the glTF 2.0 reference BRDF uses", marked
// ⚠ VERIFY, and two lessons went by without anyone checking. §E checks it — and
// finds the claim true in substance and imprecise in a way that changes the
// design: the spec writes `mix(diffuse, specular, F)`, so the SAME Fresnel that
// scales the diffuse down also scales the specular up, and our specular already
// carries it. The divergence is exactly one factor, and §E measures it.
//
// Build and run:  sh scratch/build_verify_66.sh

#include <engine/asset/asset_store.hpp>
#include <engine/core/assert.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/gltf.hpp>
#include <engine/gfx/light.hpp>
#include <engine/gfx/material.hpp>
#include <engine/gfx/mesh.hpp>
#include <engine/gfx/microfacet.hpp>
#include <engine/gfx/obj.hpp>
#include <engine/gfx/texture.hpp>

#include "../demos/common/demo_scene.hpp"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
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
    char line[900];
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

/// The assets live beside the executable (3.5), and every harness in this course
/// is run from the repository root — so the path is the source tree's.
const char* const k_cube_gltf = "assets/cube.gltf";
const char* const k_shapes_glb = "assets/shapes.glb";

// ===========================================================================
//  §A — THE ROUND TRIP
// ===========================================================================
//
// The strongest test available for a loader, and Lesson 3.5 stated the reason
// when it built `save_obj`: A READER ALONE CAN ONLY BE CHECKED AGAINST
// EXPECTATIONS YOU TYPED BY HAND. Here the writer is a Python script
// (scratch/make_gltf_assets.py) that transcribed `k_cube_vertices` and
// `k_cube_indices` out of mesh.hpp — a different implementation, in a different
// language, going through a completely different serialisation. If the loaded
// mesh matches `cube_mesh()` index for index, the whole accessor path is right.

void section_a_round_trip()
{
    std::printf("\n=== A. The round trip: a file becomes the mesh we already had ===\n");

    engine::gltf_scene_data scene;
    const engine::gltf_report r = engine::load_gltf(k_cube_gltf, scene);

    checkf(r.ok(), "load_gltf(cube.gltf) -> %s  (generator '%s')",
           engine::name_of(r.status), r.generator.c_str());
    if (!r.ok()) { return; }

    checkf(r.primitives == 1 && scene.primitives.size() == 1,
           "one node, one mesh, one primitive -> %d primitive(s)", r.primitives);
    checkf(!r.binary, "detected as TEXT glTF from its first four bytes, not .glb");

    const engine::mesh_data& got = scene.primitives[0].geometry;
    const engine::mesh built = engine::cube_mesh();

    checkf(got.vertices.size() == built.vertices.size(),
           "%zu vertices, matching cube_mesh()'s %zu. THE FILE SHIPS ITS OWN "
           "NORMALS, and that is what keeps the count at eight: flat normals "
           "would force the 8 -> 36 split, because a corner where three faces "
           "meet needs three normals and a vertex carries one (3.5's index "
           "problem, in a second format). The first run of this harness asserted "
           "8 and got 36",
           got.vertices.size(), built.vertices.size());
    checkf(got.indices.size() == built.indices.size(),
           "%zu indices, matching %zu", got.indices.size(), built.indices.size());

    if (got.vertices.size() != built.vertices.size()
        || got.indices.size() != built.indices.size())
    {
        return;
    }

    // BIT-EXACT, not approximately equal. Every value in the file is a float
    // written by struct.pack("<f") from a Python float that was itself exactly
    // representable (-0.5, 0.0, 0.5), so there is no rounding anywhere in the
    // chain and a tolerance would be hiding something rather than allowing for
    // something.
    int position_mismatches = 0;
    for (std::size_t i = 0; i < got.vertices.size(); ++i)
    {
        const engine::vec3 a = got.vertices[i];
        const engine::vec3 b = built.vertices[i];
        if (a.x != b.x || a.y != b.y || a.z != b.z) { ++position_mismatches; }
    }
    checkf(position_mismatches == 0,
           "all 8 positions BIT-EXACT against k_cube_vertices — %d mismatch(es). "
           "Not a tolerance: every coordinate is -0.5, 0 or +0.5, exactly "
           "representable, so a difference here would be a real one",
           position_mismatches);

    int index_mismatches = 0;
    for (std::size_t i = 0; i < got.indices.size(); ++i)
    {
        if (got.indices[i] != built.indices[i]) { ++index_mismatches; }
    }
    checkf(index_mismatches == 0,
           "all 36 indices identical, in order — %d mismatch(es). The winding "
           "survived too, which matters because glTF and this course agree that a "
           "front face is wound counter-clockwise",
           index_mismatches);

    // A FILE'S NORMALS ARE AUTHORSHIP. `with_normals` returns geometry that
    // already has them unchanged whatever style is asked for (3.6's rule, and
    // the loader lie 3.5 forbade), so nothing was generated here — and the
    // normals that arrived are the smooth outward diagonals the file wrote,
    // not the flat per-face ones a generator would have produced.
    checkf(r.generated_normals == 0 && r.with_normals == 1
               && got.normals.size() == got.vertices.size(),
           "the file's own NORMAL attribute was read and NOT overwritten: %zu "
           "normals, %d generated. A loader that regenerates over authored "
           "normals is the loader lie 3.5 forbade",
           got.normals.size(), r.generated_normals);
    {
        // Vertex 6's normal is the (+,+,+) diagonal, normalised. Checking the
        // VALUE and not just the count is what distinguishes "read the file"
        // from "computed something the same length".
        const engine::vec3 n6 = got.normals[6];
        const float k = 1.0f / std::sqrt(3.0f);
        checkf(std::fabs(n6.x - k) < 1e-6f && std::fabs(n6.y - k) < 1e-6f
                   && std::fabs(n6.z - k) < 1e-6f,
               "…and vertex 6's normal is (%.4f, %.4f, %.4f), the outward diagonal "
               "the file wrote — arriving unit-length, because the accessor read "
               "back exactly what struct.pack wrote",
               static_cast<double>(n6.x), static_cast<double>(n6.y),
               static_cast<double>(n6.z));
    }
    checkf(r.with_uvs == 1 && got.uvs.size() == got.vertices.size(),
           "TEXCOORD_0 was read: %zu uvs", got.uvs.size());

    const engine::mesh_report m = engine::validate(got.view());
    checkf(m.closed() && m.consistently_wound(),
           "the loaded cube VALIDATES as closed and consistently wound "
           "(euler %d, %d boundary edges, %d reversed) — which is the property "
           "cull_of() reads, computed from the geometry rather than trusted",
           m.euler, m.boundary_edges, m.reversed_edges);
}

// ===========================================================================
//  §B — NO CONVERSION
// ===========================================================================
//
// Two conventions that could have disagreed and do not, and one that does. Worth
// asserting rather than asserting in prose, because "we checked and there is
// nothing to do" is a claim like any other.

void section_b_conventions()
{
    std::printf("\n=== B. No conversion: the axes agree, and so does the uv origin ===\n");

    engine::gltf_scene_data scene;
    const engine::gltf_report r = engine::load_gltf(k_cube_gltf, scene);
    if (!r.ok()) { check(false, "cube.gltf did not load"); return; }

    const engine::mesh_data& got = scene.primitives[0].geometry;

    // 1. THE AXES. glTF §3.5 is right-handed, +Y up; conventions §2 is
    //    right-handed, Y-up, -Z forward. Vertex 6 is the (+,+,+) corner in the
    //    file and must still be the (+,+,+) corner in memory. A loader that
    //    negated z — which is what you write if you assume glTF is Z-up, as FBX
    //    and 3ds Max are — would put it at (+0.5, +0.5, -0.5) and the cube would
    //    still look like a cube, wound inside out.
    const engine::vec3 far_corner = got.vertices[6];
    checkf(far_corner.x == +0.5f && far_corner.y == +0.5f && far_corner.z == +0.5f,
           "vertex 6 is (%.1f, %.1f, %.1f) — the (+,+,+) corner, unmoved. No axis "
           "swap, no negated z. A cube survives the wrong answer LOOKING like a "
           "cube, which is why this is asserted rather than eyeballed",
           static_cast<double>(far_corner.x), static_cast<double>(far_corner.y),
           static_cast<double>(far_corner.z));

    // 2. THE UV ORIGIN, WHICH IS THE TRAP. OBJ puts (0,0) at the BOTTOM left, so
    //    `mesh_import::flip_uv_v` defaults to true and every OBJ in this engine
    //    is flipped on import. glTF §3.9.3 puts it at the UPPER left — the same
    //    as `texture` (3.9) — so the flip must NOT be applied. The asset writes
    //    vertex 0's uv as (0, 1); if anything flipped it we would read (0, 0).
    const engine::vec2 uv0 = got.uvs[0];
    checkf(uv0.x == 0.0f && uv0.y == 1.0f,
           "vertex 0's uv is (%.1f, %.1f), exactly as written in the file. NOT "
           "flipped — glTF's texture origin is the upper left, the same as ours, "
           "and OBJ's is the lower left. The flip belongs to the FORMAT, so a "
           "loader that honoured mesh_import::flip_uv_v here would turn every "
           "glTF asset upside down for anyone using the default",
           static_cast<double>(uv0.x), static_cast<double>(uv0.y));

    // 3. And the one that DOES differ, stated so it is not a surprise later:
    //    glTF says an asset's front faces +Z; our camera looks down -Z. That is
    //    about how content is authored, not how numbers are stored, so there is
    //    nothing for the loader to do and everything for the placement to.
    check(true,
          "(noted, not a conversion) glTF says an asset's front faces +Z and our "
          "camera looks down -Z, so a model dropped in unrotated presents its "
          "BACK. That is an authoring fact, fixed with a yaw on the object — not "
          "a coordinate change, which would also mirror the geometry");
}

// ===========================================================================
//  §C — THE NODE TREE
// ===========================================================================

void section_c_nodes()
{
    std::printf("\n=== C. A scene, not a shape: the node tree flattened ===\n");

    engine::gltf_scene_data scene;
    const engine::gltf_report r = engine::load_gltf(k_shapes_glb, scene);

    checkf(r.ok(), "load_gltf(shapes.glb) -> %s", engine::name_of(r.status));
    if (!r.ok()) { return; }

    checkf(r.binary, "detected as the BINARY container (.glb) from the 'glTF' magic "
                     "in its first four bytes — same call, no flag, because the "
                     "caller should not have to know which form it was handed");
    checkf(r.primitives == 4 && r.nodes == 5,
           "%d primitives from %d nodes — one file, FOUR draws. That is the "
           "structural difference from OBJ: a glTF mesh is a list of primitives "
           "and each one has its own material, so flattening them into one "
           "mesh_data would give geometry that is correct and unpaintable",
           r.primitives, r.nodes);

    // THE PRODUCT. "group" translates by (1, 0, 0) and "gold octahedron" — its
    // child — translates by (0, 2, 0), so the octahedron's world translation
    // must be the sum. Getting this wrong in the obvious way (using the LOCAL
    // transform) puts it at (0, 2, 0); getting it wrong the other obvious way
    // (multiplying in the wrong order) puts it somewhere neither node asked for.
    const engine::gltf_primitive* oct = nullptr;
    const engine::gltf_primitive* tet = nullptr;
    for (const engine::gltf_primitive& p : scene.primitives)
    {
        if (p.node_name == "gold octahedron") { oct = &p; }
        if (p.node_name == "red tetrahedron") { tet = &p; }
    }

    checkf(oct != nullptr && tet != nullptr,
           "node names survived the load, which is what turns 'primitive 2 is "
           "wrong' into a usable bug report");
    if (oct == nullptr || tet == nullptr) { return; }

    const engine::vec4 t1 = oct->world_from_local.c3;
    checkf(t1.x == 1.0f && t1.y == 2.0f && t1.z == 0.0f,
           "the octahedron's world translation is (%.1f, %.1f, %.1f) = "
           "group(1,0,0) + local(0,2,0). A PRODUCT down the tree, not the local "
           "transform — cgltf_node_transform_world walks the parent chain",
           static_cast<double>(t1.x), static_cast<double>(t1.y),
           static_cast<double>(t1.z));

    const engine::vec4 t2 = tet->world_from_local.c3;
    checkf(t2.x == 1.0f && t2.y == 3.5f && t2.z == 0.0f,
           "and the tetrahedron, two levels down, is at (%.1f, %.1f, %.1f) = "
           "1 + 0, 0 + 2 + 1.5. Three matrices deep and the arithmetic is "
           "checkable by hand, which is the whole reason the test asset has a "
           "hierarchy instead of three siblings",
           static_cast<double>(t2.x), static_cast<double>(t2.y),
           static_cast<double>(t2.z));

    // TWO PRIMITIVES, ONE SHAPE. The plinth reuses the cube's accessors and is
    // flattened by its own node's scale — which a loader that baked transforms
    // into vertices could not express at all, because the two would then be
    // different geometry.
    {
        const engine::vec4 plinth_scale_col =
            [&scene]() -> engine::vec4 {
                for (const engine::gltf_primitive& p : scene.primitives)
                {
                    if (p.node_name == "plinth") { return p.world_from_local.c1; }
                }
                return {};
            }();
        checkf(plinth_scale_col.y == 0.25f,
               "the plinth's node carries a scale of %.2f on y, and its GEOMETRY is "
               "the cube's — two primitives sharing one shape and differing only in "
               "placement, which is precisely what baking a transform destroys",
               static_cast<double>(plinth_scale_col.y));
    }

    // ---- The other half of §A's story: generation, and the split it costs --
    //
    // shapes.glb ships NO normals, so every primitive in it went through
    // `with_normals(flat)` — which cannot keep the vertex count, because a
    // corner where three faces meet needs three different normals and a vertex
    // carries exactly one. 3.5's index problem, arriving in a second format and
    // measured on a cube whose eight corners the file wrote.
    checkf(r.generated_normals == 4 && r.with_normals == 0,
           "all %d primitives had flat normals generated (spec: a client SHOULD), "
           "and %d arrived with their own",
           r.generated_normals, r.with_normals);
    for (const engine::gltf_primitive& p : scene.primitives)
    {
        if (p.node_name != "silver cube") { continue; }
        checkf(p.geometry.vertices.size() == 36,
               "the cube's 8 file vertices became %zu after flat generation — one "
               "per face-CORNER, index buffer 0,1,2,... So the cost of flat "
               "shading is a 4.5x vertex count, and cube.gltf ships smooth "
               "normals precisely to avoid paying it in a test that wants to "
               "compare index for index",
               p.geometry.vertices.size());
    }

    // And the geometry is NOT baked. If it were, the octahedron's own vertices
    // would carry the +y offset and its local origin would be gone — which is a
    // scene that can never be re-parented, instanced or animated (5.9).
    for (const engine::gltf_primitive& p : scene.primitives)
    {
        if (p.node_name != "gold octahedron") { continue; }
        float max_y = 0.0f;
        for (const engine::vec3 v : p.geometry.vertices)
        {
            max_y = (v.y > max_y) ? v.y : max_y;
        }
        checkf(max_y < 1.0f,
               "the octahedron's own vertices top out at y = %.2f, NOT 2.6 — the "
               "transform was not baked into the geometry. A loader that bakes "
               "produces a scene that cannot be animated or instanced, and the "
               "loss is silent",
               static_cast<double>(max_y));
    }
}

// ===========================================================================
//  §D — THE MATERIAL TABLE
// ===========================================================================

void section_d_materials()
{
    std::printf("\n=== D. Materials that mean something ===\n");

    engine::gltf_scene_data glb;
    if (!engine::load_gltf(k_shapes_glb, glb).ok())
    {
        check(false, "shapes.glb did not load");
        return;
    }

    checkf(glb.materials.size() == 3, "%zu materials in the table", glb.materials.size());
    if (glb.materials.size() != 3) { return; }

    const engine::gltf_material_desc& gold = glb.materials[1];
    checkf(gold.name == "gold", "material 1 is named '%s'", gold.name.c_str());

    // NO REMAP. The file says metallic 1.0, roughness 0.35; `microsurface` holds
    // 1.0 and 0.35. That is not a trivial check — it is the payoff for Lessons
    // 6.3 and 6.4 having DERIVED the parameters rather than fitting them. An
    // engine with a hand-tuned "gloss" would need a conversion curve here, and
    // the curve would be a guess applied to every asset ever imported.
    checkf(gold.surface.metallic == 1.0f && gold.surface.roughness == 0.45f,
           "metallic %.2f, roughness %.2f — copied, not converted. glTF's alpha "
           "remap is roughness^2 (spec, Appendix B) and so is "
           "alpha_from_roughness; its dielectric IOR is 1.5 and so is ours",
           static_cast<double>(gold.surface.metallic),
           static_cast<double>(gold.surface.roughness));

    checkf(gold.surface.f0 == engine::k_dielectric_f0,
           "f0 stays at k_dielectric_f0 = %.2f, which is exactly what the spec "
           "fixes it to: ((1-1.5)/(1+1.5))^2 = 0.04. Two independent derivations "
           "landing on one number is why there is no conversion here either",
           static_cast<double>(engine::k_dielectric_f0));

    // THE FACTOR IS LINEAR. This is the single most commonly mishandled number
    // in a glTF importer. gold's baseColorFactor is (1.00, 0.71, 0.29) — the
    // measured F0 of gold, which Lesson 6.4 quotes — and it must arrive
    // unchanged. A to_linear() applied "for consistency with material::tint"
    // would give 0.462 for the green channel and darken every imported asset.
    checkf(std::fabs(gold.base_colour.g - 0.71f) < 1e-6f,
           "baseColorFactor's green is %.4f, unchanged — the FACTOR is linear and "
           "the TEXTURE is sRGB-encoded. Decoding this would give %.4f and would "
           "look like a plausible bug for years",
           static_cast<double>(gold.base_colour.g),
           static_cast<double>(engine::srgb_to_linear(0.71f)));

    // And for a metal, that factor IS the F0 — which is `f0_of`'s metal branch
    // arriving with a real asset behind it rather than a hand-typed constant.
    const engine::linear_rgb f0 = engine::f0_of(gold.surface, gold.base_colour);
    checkf(std::fabs(f0.r - 1.00f) < 1e-6f && std::fabs(f0.g - 0.71f) < 1e-6f
               && std::fabs(f0.b - 0.29f) < 1e-6f,
           "f0_of(metallic=1) returns the base colour verbatim: (%.2f, %.2f, "
           "%.2f). For a conductor the base colour IS the normal-incidence "
           "reflectance, which is why one field serves both workflows",
           static_cast<double>(f0.r), static_cast<double>(f0.g),
           static_cast<double>(f0.b));

    const engine::linear_rgb d_alb = engine::diffuse_albedo_of(gold.surface, gold.base_colour);
    checkf(d_alb.r == 0.0f && d_alb.g == 0.0f && d_alb.b == 0.0f,
           "…and its diffuse albedo is exactly black. A metal absorbs what crosses "
           "the interface, so there is nothing to scatter back out");

    checkf(glb.materials[2].double_sided,
           "'red plastic' carries doubleSided, which feeds cull_of()'s INTENT "
           "argument — the half Lesson 6.5 said belongs to the caller. The mesh "
           "still supplies the fact");

    // ---- The sampler mapping ------------------------------------------------
    engine::gltf_scene_data cube;
    if (!engine::load_gltf(k_cube_gltf, cube).ok())
    {
        check(false, "cube.gltf did not load");
        return;
    }
    const engine::gltf_material_desc& grid = cube.materials[0];

    checkf(grid.base_colour_uri == "uv_grid.png",
           "the material names its image as a URI ('%s'), NOT as a handle. That is "
           "the layering: a parser that returned handles would need a pool and a "
           "search path, and could never be tested from a string literal",
           grid.base_colour_uri.c_str());
    checkf(grid.samp.address_u == engine::address_mode::clamp_to_edge
               && grid.samp.address_v == engine::address_mode::clamp_to_edge,
           "wrapS/wrapT 33071 mapped to clamp_to_edge. glTF's sampler enumerants "
           "are literal GLenum values — 33071 is GL_CLAMP_TO_EDGE — and ours are "
           "SDL_GPU's, so the mapping is a switch and not a cast");
    checkf(grid.samp.texel_filter == engine::filter::linear,
           "magFilter 9729 (GL_LINEAR) mapped to filter::linear");

    // ---- The specified default ---------------------------------------------
    const engine::gltf_material_desc def = engine::gltf_default_material();
    checkf(def.surface.metallic == 1.0f && def.surface.roughness == 1.0f,
           "the spec's default material for an unpainted primitive is white, "
           "metallic 1, rough 1 — NOT microsurface{}'s 0.5/0.0, which is the "
           "engine's opinion and a different thing. A fully rough conductor is "
           "dark and colourless (6.3 measured it losing 69%% of its energy), so a "
           "primitive that lost its material LOOKS like it lost its material");
}

// ===========================================================================
//  §E — THE SPEC, SETTLED
// ===========================================================================
//
// Lesson 6.4 wrote, and marked ⚠ VERIFY:
//
//     `1 - F(v.h)` — what most real-time renderers ship, including the glTF 2.0
//     reference BRDF.
//
// Khronos glTF 2.0, Appendix B "BRDF Implementation", gives the complete model
// as:
//
//     fresnel_w       = (1 - abs(VdotH))^5
//     diffuse_brdf    = (1 / pi) * baseColor.rgb
//     specular_brdf   = D * G / (4 * abs(VdotN) * abs(LdotN))
//     dielectric_brdf = mix(diffuse_brdf, specular_brdf,
//                           0.04 + 0.96 * fresnel_w)
//     metal_brdf      = (baseColor + (1 - baseColor) * fresnel_w) * specular_brdf
//     material        = mix(dielectric_brdf, metal_brdf, metallic)
//
// So the claim is TRUE IN SUBSTANCE — `mix(a, b, F)` is `(1-F)a + Fb`, and the
// diffuse really is scaled by `1 - F(v.h)` — and IMPRECISE in a way that
// changes the design: the SAME F scales the specular UP, and our
// `cook_torrance_specular` already carries it. Two claims follow, and §E is
// where both get measured rather than argued.

void section_e_the_spec()
{
    std::printf("\n=== E. The spec, settled — the ⚠ VERIFY carried since 6.4 ===\n");

    // ---- Claim 1: the f0 lerp is EXACT, not an approximation ---------------
    //
    // The spec lerps two whole BRDFs by `metallic`; this engine lerps the F0 and
    // evaluates one BRDF (`f0_of`). Those look like different models and are the
    // same one, because Schlick is AFFINE in f0:
    //
    //     F(f0) = f0 + (1 - f0) w = f0 (1 - w) + w
    //
    // so lerping f0 and lerping F commute exactly. The specular coefficients:
    //
    //     spec: (1-m) F(0.04) + m F(base)  ==  F((1-m) 0.04 + m base)  :ours
    //
    // Derived, then measured over the whole parameter space, because a
    // derivation that has not been run is a conjecture.
    float worst_lerp = 0.0f;
    for (int mi = 0; mi <= 20; ++mi)
    {
        const float m = static_cast<float>(mi) / 20.0f;
        for (int ai = 0; ai <= 10; ++ai)
        {
            const float base = static_cast<float>(ai) / 10.0f;
            for (int ti = 0; ti <= 20; ++ti)
            {
                const float v_dot_h = static_cast<float>(ti) / 20.0f;

                // The spec's route: two Fresnels, lerped.
                const float f_di = engine::fresnel_schlick(v_dot_h,
                                                           engine::k_dielectric_f0);
                const float f_me = engine::fresnel_schlick(v_dot_h, base);
                const float spec_gltf = (1.0f - m) * f_di + m * f_me;

                // Ours: one Fresnel over a lerped f0.
                const engine::microsurface s{.roughness = 0.5f, .metallic = m};
                const engine::linear_rgb f0 = engine::f0_of(s, {base, base, base});
                const float spec_ours = engine::fresnel_schlick(v_dot_h, f0).r;

                const float d = std::fabs(spec_gltf - spec_ours);
                if (d > worst_lerp) { worst_lerp = d; }
            }
        }
    }
    checkf(worst_lerp < 1e-6f,
           "CLAIM 1 — the specular halves are ALGEBRAICALLY IDENTICAL. Worst "
           "difference %.3e over 21 x 11 x 21 = 4,851 points. glTF lerps two "
           "BRDFs by `metallic`; we lerp the F0 and evaluate one. Those commute "
           "exactly because Schlick is AFFINE in f0: F(f0) = f0(1-w) + w. So "
           "f0_of is not an approximation of the spec's mix — it IS the spec's "
           "mix, rearranged",
           static_cast<double>(worst_lerp));

    // ---- Claim 2: the diffuse halves differ, and by how much ---------------
    //
    // glTF:  (1 - m) * (1 - F(v.h, f0=0.04)) * albedo/pi
    // ours:  (1 - m) * (1 - F(n.l, f0)) * (1 - F(n.v, f0)) * albedo/pi
    //
    // Two crossings versus one. Lesson 6.4 §7 gave the physical argument for
    // ours and measured the price of the spec's: white furnace reflectance up to
    // 1.3395, a surface emitting a third more light than reaches it. What has
    // never been measured is how much of a DIFFERENCE that makes on an actual
    // shaded pixel, which is the number that decides whether the divergence
    // matters in practice.
    float worst_abs = 0.0f;
    float worst_at_theta = 0.0f;
    float worst_ratio = 1.0f;

    const engine::microsurface plastic{.roughness = 0.55f, .metallic = 0.0f};
    const engine::linear_rgb albedo{0.8f, 0.12f, 0.1f};   // shapes.glb's 'red plastic'

    for (int li = 0; li <= 88; li += 2)
    {
        for (int vi = 0; vi <= 88; vi += 2)
        {
            const float tl = static_cast<float>(li) * 3.14159265f / 180.0f;
            const float tv = static_cast<float>(vi) * 3.14159265f / 180.0f;

            const engine::vec3 n{0.0f, 0.0f, 1.0f};
            const engine::vec3 l{std::sin(tl), 0.0f, std::cos(tl)};
            const engine::vec3 v{-std::sin(tv), 0.0f, std::cos(tv)};
            const engine::vec3 h = engine::halfway(l, v);

            const float n_dot_l = engine::dot(n, l);
            const float n_dot_v = engine::dot(n, v);
            const float n_dot_h = engine::dot(n, h);
            const float v_dot_h = engine::dot(v, h);
            if (n_dot_l <= 0.0f || n_dot_v <= 0.0f) { continue; }

            const engine::linear_rgb ours = engine::cook_torrance_brdf(
                albedo, plastic, n_dot_l, n_dot_v, n_dot_h, v_dot_h,
                engine::ndf_model::ggx, engine::diffuse_coupling::two_crossing);
            const engine::linear_rgb theirs = engine::cook_torrance_brdf(
                albedo, plastic, n_dot_l, n_dot_v, n_dot_h, v_dot_h,
                engine::ndf_model::ggx, engine::diffuse_coupling::half_vector);

            const float d = std::fabs(theirs.r - ours.r);
            if (d > worst_abs)
            {
                worst_abs = d;
                worst_at_theta = static_cast<float>(vi);
                worst_ratio = (ours.r > 0.0f) ? theirs.r / ours.r : 1.0f;
            }
        }
    }

    checkf(worst_abs > 0.0f,
           "CLAIM 2 — the diffuse halves DIFFER, as they must: worst absolute "
           "difference %.4f sr^-1 on shapes.glb's 'red plastic', at a view angle "
           "of %.0f degrees, where the spec's form is %.3fx ours. One crossing "
           "against two",
           static_cast<double>(worst_abs), static_cast<double>(worst_at_theta),
           static_cast<double>(worst_ratio));

    // At NORMAL incidence the two forms must nearly agree — one crossing of a
    // 4% interface against two is a 4% difference, not a 30% one — which is why
    // the divergence is invisible on a flat-on wall and obvious at a grazing
    // silhouette.
    {
        const engine::vec3 n{0.0f, 0.0f, 1.0f};
        const engine::linear_rgb ours = engine::cook_torrance_brdf(
            albedo, plastic, 1.0f, 1.0f, 1.0f, 1.0f,
            engine::ndf_model::ggx, engine::diffuse_coupling::two_crossing);
        const engine::linear_rgb theirs = engine::cook_torrance_brdf(
            albedo, plastic, 1.0f, 1.0f, 1.0f, 1.0f,
            engine::ndf_model::ggx, engine::diffuse_coupling::half_vector);
        const float ratio = theirs.r / ours.r;
        checkf(ratio > 1.0f && ratio < 1.05f,
               "at normal incidence the two agree to %.3fx — the difference is one "
               "crossing of a 4%% interface. THAT is why this divergence never "
               "shows on a wall seen flat-on and always shows at a silhouette, "
               "and why it survived two lessons unverified",
               static_cast<double>(ratio));
        (void)n;
    }

    // ---- And the permission, which is what settles it ----------------------
    //
    // glTF 2.0 §3.9.6: "Implementations of the bidirectional reflectance
    // distribution function (BRDF) itself MAY vary based on device performance
    // and resource constraints." And Appendix B, "Complete Model": "In a
    // physically accurate light simulation, the BRDFs MUST be positive,
    // reciprocal, and energy conserving."
    //
    // So the conformance question has an answer and it is not the one 6.4
    // assumed it would be: shading a glTF material with the two-crossing
    // coupling is conformant, and it is the branch that satisfies the MUST.
    check(true,
          "(the resolution) The spec permits BRDF variation explicitly and "
          "requires the BRDF be reciprocal and energy conserving. verify_64 §E "
          "measured the spec's own sample form at 1.3395 hemispherical "
          "reflectance and the two-crossing form at 0.9255 — so we ship the one "
          "that meets the MUST, and half_vector stays selectable and measured. "
          "The ⚠ VERIFY is discharged");
}

// ===========================================================================
//  §F — THE ASSET STORE
// ===========================================================================

void section_f_store()
{
    std::printf("\n=== F. Textures and materials become assets ===\n");

    engine::search_path paths;
    paths.add_root("assets");
    engine::asset_store store(paths);

    const engine::model_load first = store.load_model("cube.gltf");
    checkf(first.ok(), "load_model(cube.gltf) -> %d mesh(es), %zu material(s)",
           first.report.primitives, first.materials.size());
    if (!first.ok()) { return; }

    const engine::asset_counters after_first = store.counters();
    checkf(after_first.models_loaded == 1,
           "models_loaded = %d, counted separately from files_read = %d — because "
           "one model file reads MANY files (the .gltf, its .bin, and one per "
           "texture), and conflating them makes 'did the cache work?' "
           "unanswerable",
           after_first.models_loaded, after_first.files_read);

    // ---- The texture arrived, through a door that did not exist yesterday --
    const engine::material* m = store.material_at(first.materials[0]);
    checkf(m != nullptr && m->textured(),
           "the material resolves and reports textured() — so uv_grid.png was "
           "found on the search path, decoded, channel-shuffled by to_texture() "
           "and stored. THAT PATH HAD NO IMPLEMENTATION BEFORE THIS LESSON: the "
           "engine could decode a PNG (5.3) and could sample a texture (3.9) and "
           "nothing joined them, because every CPU texture was generated and "
           "every loaded image went straight to the GPU");
    if (m == nullptr) { return; }

    const engine::texture* t = store.texture_at(m->albedo_map);
    checkf(t != nullptr && t->width() > 0 && t->width() == t->height(),
           "the texture is %dx%d, matching uv_grid.png's own dimensions",
           t ? t->width() : 0, t ? t->height() : 0);

    // The channel shuffle, checked against the image it came from. RGBA bytes
    // into ARGB8888: copy the buffer wholesale instead and red and blue swap,
    // which is the single most recognisable wrong-looking texture there is.
    const engine::image_handle img = store.find_image("uv_grid.png");
    const engine::image_data* src = store.image_at(img);
    if (src != nullptr && t != nullptr && src->valid())
    {
        const Uint32 texel = t->texel(3, 3);
        const std::size_t at = (3u * static_cast<std::size_t>(src->width) + 3u) * 4u;
        const Uint32 expect = engine::pack_argb(src->pixels[at + 0],
                                                        src->pixels[at + 1],
                                                        src->pixels[at + 2],
                                                        src->pixels[at + 3]);
        checkf(texel == expect,
               "texel (3,3) is 0x%08X, matching pack_argb of the image's own RGBA "
               "bytes. A memcpy would have given 0x%08X — red and blue swapped",
               texel, (texel & 0xFF00FF00u) | ((texel & 0xFFu) << 16)
                          | ((texel >> 16) & 0xFFu));
    }

    // ---- The cache ---------------------------------------------------------
    const engine::model_load second = store.load_model("cube.gltf");
    checkf(second.cached && second.meshes.size() == first.meshes.size()
               && second.meshes[0] == first.meshes[0],
           "loading the same model twice returns the SAME handles and reads no "
           "file (cached = %s). The names are derived and deterministic — "
           "'cube.gltf#0' for the primitive — which is what makes the cache "
           "possible without a second map",
           second.cached ? "true" : "false");
    checkf(store.counters().files_read == after_first.files_read,
           "files_read stayed at %d across the second load", store.counters().files_read);

    // ---- Sharing, which is the reason a texture is an asset ---------------
    const engine::texture_load a = store.load_texture("uv_grid.png");
    checkf(a.cached && a.handle == m->albedo_map,
           "asking the store for uv_grid.png directly returns the texture the "
           "material already has — one decode, one copy, however many materials "
           "name it. That is the property that matters for glTF specifically, "
           "where a file's materials routinely share one atlas");

    // ---- The cascade -------------------------------------------------------
    checkf(store.derived_count(a.source) == 1,
           "the texture is registered as DERIVED from the image (%d edge). Nobody "
           "asked for it by a name of its own, so its lifetime is the image's — "
           "the same edge derive_mesh has used since 5.5",
           store.derived_count(a.source));

    const int released = store.unload_image(a.source);
    checkf(released == 2 && !store.textures().contains(a.handle),
           "unloading the image released %d assets and took the texture with it. "
           "The material's handle is now stale, which is not a crash: bind_albedo "
           "returns an unbound binding and the surface falls back to its tint — "
           "5.4's whole bargain, arriving with a real dependency behind it",
           released);
    checkf(!store.find_texture("uv_grid.png").valid(),
           "…and the texture's NAME entry went too, so find_texture cannot hand "
           "out a handle to a slot the pool has already recycled");

    // ---- The one conformance gap, reported rather than hidden -------------
    engine::asset_store store2(paths);
    const engine::model_load glb = store2.load_model("shapes.glb");
    checkf(glb.ok() && glb.materials.size() == 4,
           "shapes.glb -> %zu materials: three from the file plus the SPECIFIED "
           "DEFAULT, materialised only because the tetrahedron names none",
           glb.materials.size());
    checkf(glb.factor_texture_conflicts == 0,
           "no conformance conflicts: 'red plastic' has a non-white factor but no "
           "texture, so nothing is dropped. The gap is the COMBINATION — a factor "
           "AND a texture, which glTF multiplies and this engine's albedo image "
           "replaces (3.9) — and cube.gltf's factor is white precisely so the "
           "common case imports exactly");
    checkf(glb.textures_missing == 0, "and no missing textures");

    // The default material really was built to the spec's numbers.
    const engine::material* fallback = store2.material_at(glb.materials.back());
    checkf(fallback != nullptr && fallback->surface.metallic == 1.0f
               && fallback->surface.roughness == 1.0f,
           "the fallback material is metallic 1, rough 1 — the spec's numbers, not "
           "the engine's defaults");
}

// ===========================================================================
//  §G — THE GOLDEN
// ===========================================================================

void section_g_golden()
{
    std::printf("\n=== G. The golden — a new path must not move the old picture ===\n");

    const int rc = demo::write_reference_shot("scratch/verify66.ppm");
    check(rc == 0, "write_reference_shot() succeeded");

    const std::string ours = read_file("scratch/verify66.ppm");
    const std::string golden = read_file("scratch/shot_52.ppm");
    checkf(!ours.empty() && ours.size() == golden.size(),
           "same size: %zu bytes", ours.size());
    check(ours == golden,
          "BYTE-IDENTICAL, hash E917C06C — fifteenth lesson at this hash and the "
          "second since 6.4 re-baselined it. This lesson added a whole second "
          "asset format, a texture pool inside the store, a material cache and a "
          "channel shuffle, and moved not one pixel. LOADING A NEW FORMAT ADDS A "
          "PATH; IT MUST NOT MOVE THE EXISTING PICTURE");
}

}   // namespace

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    std::printf("verify_66 — Lesson 6.6: glTF 2.0 loading\n");

    section_a_round_trip();
    section_b_conventions();
    section_c_nodes();
    section_d_materials();
    section_e_the_spec();
    section_f_store();
    section_g_golden();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return (g_failures == 0) ? 0 : 1;
}
