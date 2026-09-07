// engine/include/engine/gfx/gltf.hpp — reading the format the industry actually ships.
//
// Lesson 6.6. Lesson 3.5 wrote an OBJ loader by hand and it was the right call:
// OBJ is six keywords of line-oriented text, and writing it taught the index
// problem, fan triangulation and vertex unification — every one of which is an
// engine subject. This file is the other half of that argument, because glTF is
// not a harder OBJ. It is a different KIND of thing, and three of the
// differences are load-bearing:
//
//   IT DESCRIBES A SCENE, NOT A SHAPE. OBJ is a bag of triangles. A glTF file is
//                 a node tree with transforms, meshes split into primitives, and
//                 a material table those primitives point into. Loading one
//                 produces several objects, not one mesh.
//   IT CARRIES MATERIALS THAT MEAN SOMETHING. `usemtl` names a string OBJ cannot
//                 define; glTF defines base colour, metallic and roughness in
//                 the file, with PHYSICAL units — the exact four numbers Lesson
//                 6.4's `microsurface` is made of. This is the first asset in
//                 this course that can tell the renderer what it is.
//   IT IS BINARY UNDERNEATH. Vertex data lives in typed, strided accessors over
//                 buffer views over buffers, which may be a separate file, a
//                 base64 data URI, or a chunk glued onto the JSON in a `.glb`.
//                 That combinatorial matrix is why we do not hand-roll the
//                 parser (see CMakeLists.txt for the full argument).
//
// ---------------------------------------------------------------------------
// WHAT THIS FILE IS NOT
// ---------------------------------------------------------------------------
//
// It is not the asset system's door. `asset_store::load_model` is (5.5's
// machinery, extended this lesson), and the split is the same one `parse_obj`
// and `load_obj` made in 3.5, for a reason worth restating: THIS LAYER TURNS
// BYTES INTO DESCRIPTIONS, AND KNOWS NOTHING ABOUT POOLS, HANDLES, CACHES OR
// SEARCH PATHS. It reports that a material wants an image called
// `"uv_grid.png"`; it does not go and get it. That keeps this file testable
// from a string literal with no filesystem, and it keeps the policy (where do
// images come from? are two references to one file one texture?) in the one
// place that already owns that policy.
//
// ---------------------------------------------------------------------------
// AND THE CONVENTION NEWS, WHICH IS THE PAYOFF FOR MODULE 2'S DISCIPLINE
// ---------------------------------------------------------------------------
//
// The glTF specification, §3.5 "Coordinate System and Units":
//
//   > glTF uses a right-handed coordinate system.
//   > glTF defines +Y as up; the front side of a glTF asset faces +Z [...]
//   > The units for all linear distances are meters.
//
// Our world space (course conventions §2) is right-handed, Y-up, −Z forward.
// **The axes are identical. There is no conversion in this file, at all** — no
// negated z, no swapped y and z, no basis change matrix, and `verify_66` §B
// asserts that a position in the file arrives at the same numbers in memory.
//
// That is not luck, it is Lesson 2.6 having been argued rather than guessed.
// The one thing that DOES differ is not a coordinate convention: glTF says an
// asset's front faces +Z, and our camera looks down −Z, so a model dropped into
// the world unrotated presents its BACK. That is a fact about how content is
// authored, not about how numbers are stored, and the fix is a yaw on the
// object rather than a change to the loader.

#pragma once

#include <engine/gfx/colour.hpp>
#include <engine/gfx/mesh.hpp>
#include <engine/gfx/microfacet.hpp>
#include <engine/gfx/texture.hpp>
#include <engine/math/mat4.hpp>

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace engine {

// ---- How a load ended -------------------------------------------------------

/// The outcome of a glTF load. `ok` is the only success.
///
/// The same shape as `obj_status` (3.5) and `image_status` (5.3), and by now the
/// repetition is the point: an enum plus a report is this engine's answer to
/// "how did that go", and a fourth loader inventing a fourth answer would be the
/// mistake. The engine core does not throw (CLAUDE.md §4), so the caller writes
/// the branch.
enum class gltf_status
{
    ok,                     ///< parsed, and produced at least one drawable primitive
    cannot_open,            ///< missing, unreadable, or empty
    invalid_format,         ///< not glTF, or JSON we cannot parse
    invalid_content,        ///< parses, but violates the spec (cgltf_validate says so)
    missing_buffer,         ///< a buffer's URI could not be resolved or read
    no_primitives,          ///< valid, but contains nothing to draw
    unsupported_primitive,  ///< a mode we do not draw (points, lines, strips, fans)
    missing_positions,      ///< a primitive with no POSITION attribute
    too_many_vertices       ///< more vertices in one primitive than a uint16 can name
};

[[nodiscard]] const char* name_of(gltf_status s);

// ---- What a material said about itself --------------------------------------

/// A material **as the file described it** — names for images, not handles.
///
/// This is the type that keeps the layering honest, and it is worth being
/// precise about why it is not just `engine::material`. A `material` holds a
/// `texture_handle`, which is meaningless without the pool that issued it; a
/// file holds a URI, which is meaningless without a search path. Neither of
/// those belongs to a parser. So the parser produces THIS — a description, with
/// every reference still a string — and `asset_store::load_model` is what turns
/// descriptions into materials.
///
/// The same split every real pipeline makes, and the reason it survives contact
/// with reality: the importer runs once, offline in Module 8, and its output has
/// to be serializable. A handle is not.
struct gltf_material_desc
{
    /// The material's name in the file, or a generated `"material_3"` when it
    /// had none. **Never empty**, because this string is the cache key
    /// `asset_store` stores it under and an unnamed material still needs to be
    /// findable — and still needs two references to it to resolve to one
    /// material rather than two.
    std::string name;

    /// `pbrMetallicRoughness.baseColorFactor`, **already linear**.
    ///
    /// This is the single most commonly mishandled number in a glTF importer, so
    /// it is worth stating flatly: **the FACTOR is linear and the TEXTURE is
    /// sRGB-encoded.** The spec is explicit that base colour factors are linear
    /// values, while the base colour texture is stored with the sRGB transfer
    /// function. They are multiplied together in linear space after the texture
    /// is decoded. Lesson 6.1 built the whole vocabulary for this sentence; here
    /// is where it earns its keep.
    ///
    /// So there is no `to_linear` call anywhere near this field, and adding one
    /// "for consistency with `material::tint`" would darken every imported
    /// asset by the gamma curve — the classic importer bug, and one that looks
    /// plausible enough to survive review.
    linear_rgb base_colour{1.0f, 1.0f, 1.0f};

    /// `baseColorFactor[3]`. Carried because the file said it; unused until the
    /// engine has alpha blending, which is not this lesson.
    float alpha = 1.0f;

    /// `metallicFactor` and `roughnessFactor`, straight into Lesson 6.4's type.
    ///
    /// **No remapping, no rescaling, no tweaking**, and that is the reward for
    /// 6.3 and 6.4 having derived rather than tuned. glTF's roughness is
    /// perceptual roughness with `alpha = roughness^2` (spec, Appendix B), which
    /// is exactly `alpha_from_roughness`; its metallic is the same switch
    /// `microsurface::metallic` is; and its dielectric F0 is fixed at 0.04 from
    /// an IOR of 1.5, which is exactly `k_dielectric_f0`. An engine that had
    /// invented its own gloss parameter would need a conversion here, and the
    /// conversion would be a guess.
    microsurface surface{};

    /// The URI of `baseColorTexture`'s image, or empty for none.
    ///
    /// A **relative path as written in the file**, percent-decoded. Resolving it
    /// is the asset store's job, against its own search path — which is the
    /// difference between a loader and an asset system, and the reason a `.gltf`
    /// referencing `"../shared/wood.png"` cannot reach outside the search root.
    std::string base_colour_uri;

    /// How that image should be read: glTF sampler `magFilter` and `wrapS`/`wrapT`
    /// mapped onto Lesson 3.9's types.
    ///
    /// glTF's filter and wrap enumerants are literally OpenGL's GLenum values
    /// (9728 = `GL_NEAREST`, 10497 = `GL_REPEAT`), which is a piece of history
    /// showing through the format. Ours are SDL_GPU's, so the mapping is a small
    /// switch and `verify_66` §D drives every arm of it.
    sampler samp{};

    /// **Textures this material asked for that we do not yet read**, counted
    /// rather than ignored.
    ///
    /// A loader that silently drops what it does not understand is a loader that
    /// makes an asset look wrong with no way to find out why. These two flags
    /// are the whole difference between "the model is too shiny" and "the model
    /// declares a metallicRoughness texture and we are using the factors".
    ///
    /// Both are deferred for one concrete reason: they are **linear data in an
    /// image**, and `texture` (3.9) stores sRGB-encoded texels because `sample`
    /// decodes on read. A roughness map read through an sRGB decode is wrong by
    /// the gamma curve everywhere except 0 and 1. Fixing that means giving a
    /// texture a colour space, which is Lesson 6.7's problem because normal maps
    /// need exactly the same thing.
    bool wants_metallic_roughness_texture = false;
    bool wants_normal_texture = false;

    /// `doubleSided`. Feeds `cull_of`'s second argument — the INTENT half of
    /// Lesson 6.5's pair, which is exactly what this flag is: the mesh still
    /// supplies the fact.
    bool double_sided = false;
};

// ---- What a primitive is ----------------------------------------------------

/// One drawable piece: geometry, where it sits, and which material it wants.
///
/// **A glTF mesh is a LIST of primitives, and each one has its own material.**
/// That is the structural difference from OBJ that costs the most to ignore: a
/// car body and its windscreen are one "mesh" in the file and must be two draws,
/// because they cannot share a material. Flattening them into one `mesh_data`
/// would produce geometry that is correct and unpaintable.
struct gltf_primitive
{
    /// The geometry, in the node's LOCAL space — **not** baked into world space.
    ///
    /// A loader that bakes transforms produces a scene that can never be
    /// animated, re-parented or instanced, and Lesson 5.9 built a transform
    /// hierarchy precisely so that placement stays data. So the vertices are
    /// what the file said and `world_from_local` is what the node tree said,
    /// kept apart.
    mesh_data geometry;

    /// The accumulated transform from this primitive's node to the scene root.
    ///
    /// Flattened, which IS a simplification and is named as one: the node tree's
    /// shape is lost and only the product survives. That is right for a static
    /// prop and wrong for anything with a skeleton, and Module 7's skinning is
    /// where the tree has to be kept. The 90% picture; the missing 10% is joints.
    mat4 world_from_local{};

    /// Index into `gltf_scene_data::materials`, or **-1** for "the file gave this
    /// primitive no material".
    ///
    /// -1 is not an error and must not be turned into one. The spec says a
    /// primitive without a material uses the default material — base colour
    /// white, metallic 1, roughness 1 — and that default is a real, specified
    /// appearance rather than a missing value. `asset_store` supplies it.
    int material = -1;

    /// The node's name in the file, or `"node_7"`. Diagnostic, and worth having:
    /// "primitive 4 is wrong" is a much worse bug report than `"windscreen"`.
    std::string node_name;
};

// ---- Everything a file contained --------------------------------------------

/// The parsed contents of one glTF file: primitives, and the materials they
/// point at.
///
/// Owning, movable, and cheap to move — four vectors, so the compiler's special
/// members are all correct and RAII does the rest (Lesson 0.6). Note it is
/// returned through an out-parameter like `mesh_data`, for the same reason: the
/// caller keeps its capacity across loads.
struct gltf_scene_data
{
    std::vector<gltf_primitive> primitives;
    std::vector<gltf_material_desc> materials;

    void clear()
    {
        primitives.clear();
        materials.clear();
    }
};

// ---- What the loader learned ------------------------------------------------

/// Facts about the load, whether or not it succeeded.
///
/// Same argument as `obj_report`: a bool answers "did it work", and these answer
/// the question you ask next. The three `skipped_*` counters are the important
/// ones — they are how a file that loads and looks wrong tells you why.
struct gltf_report
{
    gltf_status status = gltf_status::cannot_open;

    // ---- What the file contained ------------------------------------------
    int meshes = 0;             ///< `meshes` entries in the JSON
    int nodes = 0;              ///< nodes visited while flattening the scene
    int file_materials = 0;     ///< `materials` entries
    int file_images = 0;        ///< `images` entries
    bool binary = false;        ///< true for `.glb`, false for `.gltf`
    std::string generator;      ///< `asset.generator`, for when an exporter is the suspect

    // ---- What we built -----------------------------------------------------
    int primitives = 0;         ///< drawable primitives produced
    int vertices = 0;           ///< summed over those primitives
    int triangles = 0;
    int with_uvs = 0;           ///< primitives that carried TEXCOORD_0
    int with_normals = 0;       ///< …that carried NORMAL
    int generated_normals = 0;  ///< …that did not, and had them computed

    // ---- What we did not build ---------------------------------------------
    int skipped_non_triangles = 0;  ///< point/line/strip/fan primitives dropped
    int skipped_no_positions = 0;   ///< primitives with no POSITION
    int skipped_too_large = 0;      ///< primitives over the uint16 index ceiling

    /// The largest vertex count seen in one primitive, whether or not it fit.
    /// The number to quote when `too_many_vertices` fires, because it says how
    /// far over the ceiling the asset actually is.
    int max_primitive_vertices = 0;

    [[nodiscard]] bool ok() const { return status == gltf_status::ok; }
};

// ---- The two entry points ----------------------------------------------------

/// The most vertices one primitive may have, and **why the ceiling exists**.
///
/// `mesh_data::indices` is `std::uint16_t` (Lesson 2.12), so an index can name
/// 65,536 distinct vertices and no more. OBJ never made this fire because the
/// files this course ships are small; glTF will, because real exported assets
/// routinely exceed it and a `.glb` from Blender may hold a single 200k-vertex
/// primitive.
///
/// **It is reported, never silently truncated**, and that distinction is the
/// whole reason the constant is public. A loader that wraps the index around
/// produces geometry that renders — as a spray of triangles connecting the wrong
/// corners of the model — and there is no message anywhere saying what happened.
/// Widening the index type is a real answer and it costs real bytes; splitting
/// the primitive is the other real answer and it costs real code. Both are
/// exercises, and neither is a thing to guess at inside a loader.
inline constexpr int k_max_primitive_vertices = 65536;

/// Parse glTF bytes into descriptions.
///
/// Accepts both forms — JSON text (`.gltf`) and the binary container (`.glb`) —
/// because the container is detected from the first four bytes ("glTF") and the
/// caller should not have to care which it was handed.
///
/// @param bytes    the whole file. For a `.gltf` with external buffers this must
///                 be paired with a usable `base_dir`, or the load fails with
///                 `missing_buffer` — which is the honest outcome, since half a
///                 mesh is worse than none.
/// @param base_dir the directory the file came from, used to resolve relative
///                 buffer URIs. May be null or empty for self-contained files
///                 (`.glb`, or a `.gltf` whose buffers are `data:` URIs).
/// @param out      cleared first; may hold partial data on failure.
///
/// **Separate from `load_gltf` for the same reason `parse_obj` was**: bytes can
/// come from a string literal in a test, or from a pack file in Module 8. A
/// parser that can only be handed a path is a parser that can only be tested
/// with a filesystem.
[[nodiscard]] gltf_report parse_gltf(std::span<const std::byte> bytes,
                                     const char* base_dir,
                                     gltf_scene_data& out);

/// Read a `.gltf` or `.glb` from disk and parse it.
///
/// `base_dir` is derived from `path`, so a file's sibling `.bin` and its textures
/// resolve the way an exporter meant them to.
[[nodiscard]] gltf_report load_gltf(const char* path, gltf_scene_data& out);

// ---- The default material ----------------------------------------------------

/// The material the spec prescribes for a primitive that names none.
///
/// White, fully metallic, fully rough. It is worth knowing that this is a
/// deliberately conspicuous choice on the specification's part rather than a
/// neutral one: a fully-rough metal is dark and colourless under a single light,
/// so a primitive that lost its material LOOKS like it lost its material.
/// Lesson 6.3 measured why it is dark — a rough conductor loses 69% of its
/// energy to single-scattering masking — which makes this the one place in the
/// course where that loss is the desired behaviour.
[[nodiscard]] gltf_material_desc gltf_default_material();

}   // namespace engine
