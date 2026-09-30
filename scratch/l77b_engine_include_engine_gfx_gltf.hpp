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
//
// *** CORRECTED IN LESSON 7.7b, the first lesson with an asset that has a
// face. *** The sentence above has the direction backwards. A camera that looks
// down −Z sees the surfaces whose normals point to +Z — toward it — and that is
// exactly where glTF puts an asset's front, so a character dropped in unrotated
// in front of the default camera LOOKS AT YOU: `mannequin --shot` shows its face
// plate. What the two conventions really disagree about is FORWARD. glTF's is
// +Z ("+Y as up, +Z as forward, and -X as right", spec §3.4) and this engine's
// is −Z, so gameplay code that moves a character along the engine's forward
// walks it backwards. The fix is still a yaw on the object and still outside
// the loader — for that reason, not the one the sentence gave.
//
// ---------------------------------------------------------------------------
// LESSON 7.7b: THE TREE COMES BACK
// ---------------------------------------------------------------------------
//
// 6.6 flattened the node tree into one matrix per primitive and threw the tree
// away, and said so: "right for a static prop and wrong for anything with a
// skeleton". A skeleton IS that tree. So the parser now also returns the tree
// itself (`gltf_node_desc`), the skins that name joints in it, the per-vertex
// JOINTS_n / WEIGHTS_n arrays and the animations — every one of them as a
// DESCRIPTION, in the file's own indices and order, exactly as 6.6 returned
// materials. Turning descriptions into a skeleton and clips is policy, and it
// lives in `anim/import.hpp`, the only file that knows what a joint is. This
// file still knows nothing about `anim`, and nothing that loaded a static model
// before this lesson loads it any differently.

#pragma once

#include <engine/gfx/blend.hpp>    // 6.11: alphaMode, at last
#include <engine/gfx/colour.hpp>
#include <engine/gfx/mesh.hpp>
#include <engine/gfx/microfacet.hpp>
#include <engine/gfx/texture.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/quat.hpp>        // 7.7b: node rotations
#include <engine/math/transform.hpp>   // 7.7b: node transforms

#include <cstddef>
#include <cstdint>
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
/// with reality: the importer runs once, offline in Module 9, and its output has
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

    /// `baseColorFactor[3]`, and **read by something at last** — Lesson 6.11
    /// gave the engine `material::alpha`, which is where this now lands.
    ///
    /// Between 6.6 and 6.11 this field was carried and consumed by nothing,
    /// which 6.6 §10 named as this importer's one SILENT gap: `alphaMode` and
    /// `alphaCutoff` were not read at all, so a material an artist marked
    /// `MASK` or `BLEND` imported as fully opaque with no status, no warning and
    /// no count. Every other limit in this file reports.
    ///
    /// **The reason was structural rather than sloppy, and the fix is the same
    /// shape as the reason.** A status code says *"this file wants something the
    /// engine cannot do"*, and the engine had no blend state to conflict with —
    /// `pipeline_desc` had said "no blending" since Lesson 4.4. So the gap could
    /// not be reported, only removed; 6.11 removed it. What remains is
    /// `gltf_report::masked_materials` and `blended_materials`, which are counts
    /// of what the file ASKED FOR rather than complaints about it.
    float alpha = 1.0f;

    /// `alphaMode` — Lesson 6.11.
    ///
    /// **Defaults to `opaque`, which is also what the spec defaults it to**, so
    /// a file that says nothing and a file that says `"OPAQUE"` import
    /// identically, and both import exactly as they did before this lesson.
    alpha_mode mode = alpha_mode::opaque;

    /// `alphaCutoff` — Lesson 6.11. glTF §5.19.2 defaults it to 0.5 and ignores
    /// it unless `alphaMode` is `MASK`; cgltf applies that default for us, so
    /// this field carries 0.5 even for a `BLEND` material and the renderer is
    /// what declines to read it.
    float alpha_cutoff = k_default_alpha_cutoff;

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
    /// **Lesson 6.7 turned this one into a real load.** It was counted and not
    /// read for exactly one reason — a normal map is linear data and `texture`
    /// had no way to say so — and `texel_space` is what removed the obstacle.
    /// The URI is below; this flag stays because it is still the honest answer
    /// to "did the file ASK for one", which is a different question from
    /// "did we get one" (that is `model_load::normal_maps_loaded`).
    bool wants_normal_texture = false;

    /// The URI of `normalTexture`'s image, or empty. Lesson 6.7.
    ///
    /// Resolved by the asset store, like `base_colour_uri` — and **with
    /// `texel_space::linear`**, which is the whole reason this could not be
    /// loaded a lesson ago.
    std::string normal_uri;

    /// `normalTexture.scale`. glTF §5.28: the sampled X and Y are multiplied by
    /// it before the normal is normalised, so `2.0` doubles the apparent
    /// steepness and `0` flattens the surface entirely.
    ///
    /// Carried because the file said it; **applied** in the fragment would mean
    /// a per-material float in the uniform block, and `material_uniforms` has
    /// exactly one spare slot which this lesson spends on the normal-mapped
    /// flag. Named as a gap rather than silently dropped — see Exercise 3.
    float normal_scale = 1.0f;

    /// Still counted and not read: a metallic-roughness texture is linear data
    /// like a normal map, so `texel_space` unblocked it too — but reading it
    /// means per-texel roughness and metallic, which is a change to the shading
    /// equation's inputs rather than to its normal. The factors import
    /// completely; only the per-texel variation is missing.
    bool wants_metallic_roughness_texture = false;

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

    // ---- Lesson 7.7b: what makes a primitive skinnable ---------------------

    /// The node this primitive hangs from — an index into
    /// `gltf_scene_data::nodes`. Lets a caller get from a drawable back to the
    /// tree, which 6.6's flattening had made impossible.
    int node = -1;

    /// The node's `skin`, or -1. **When it is not -1, `world_from_local` above
    /// is NOT how this primitive is placed.** The spec (§3.7.3.2): "Only the
    /// joint transforms are applied to the skinned mesh; the transform of the
    /// skinned mesh node MUST be ignored." The field is still filled in, because
    /// a static viewer that knows nothing about skins still has to put the mesh
    /// somewhere, and the bind pose at the node is the least surprising place.
    int skin = -1;

    /// How many JOINTS_n / WEIGHTS_n sets were read: four influences per set.
    /// Zero for a primitive that is not skinned, or whose sets did not pair up
    /// (`gltf_report::skipped_bad_influences` says which).
    int influence_sets = 0;

    /// `4 * influence_sets` entries per vertex, PARALLEL TO `geometry.vertices`
    /// — including after `with_normals` has split vertices, which is the one
    /// transformation in this file that renumbers them (see `build_geometry`).
    ///
    /// **These are indices into the SKIN'S `joints` array, not node indices and
    /// not joint indices of any skeleton this engine builds** — spec §3.7.3.3:
    /// "The JOINTS_n attribute data contains the indices of the joints from the
    /// corresponding skin.joints array". Three index spaces meet in a skinned
    /// primitive, and confusing any two of them is a character whose limbs are
    /// driven by their neighbours'.
    std::vector<std::uint16_t> joints;

    /// The weights, decoded to floats (a normalised byte or short becomes
    /// `c / 255` or `c / 65535`, the spec's rule), same layout as `joints`.
    std::vector<float> weights;
};

/// glTF writes a rotation x, y, z, w; this engine stores w first.
///
/// The spec (§3.5.3): "rotation is a unit quaternion value, XYZW, in the local
/// coordinate system, where W is the scalar." Our `quat` is `{w, v}` (conventions:
/// quat). **One function, used by every reader of a file rotation** — node
/// rotations here and animation keys in `anim/import.cpp` — so the swizzle exists
/// in exactly one place.
///
/// What a blind copy costs is measured in Lesson 7.7b, and it is not the tidy
/// "180° about a diagonal axis" Lesson 7.7's exercise predicted: shifting four
/// components one place is an odd permutation, so it is not one fixed rotation
/// applied to every key, and the error depends on the key. At the identity it is
/// 180° about z; at `(½, ½, ½, ½)` it is zero, because that quaternion is its own
/// shift.
[[nodiscard]] inline quat quat_from_xyzw(float x, float y, float z, float w)
{
    return quat{w, vec3{x, y, z}};
}

// ---- Lesson 7.7b: the node tree, skins and animations ------------------------

/// One node of the file's tree, **as the file wrote it**.
///
/// Every node in the file, not only the ones a scene reaches, in the file's own
/// order — which the importer must not assume is parent-before-child. Blender
/// writes children first.
struct gltf_node_desc
{
    std::string name;          ///< as written, or `"node_7"`
    int parent = -1;           ///< index into `nodes`, or -1 for a root

    /// The node's local transform. Read from `translation` / `rotation` / `scale`
    /// (absent ones take the spec's defaults, which cgltf fills in), or — for a
    /// node that wrote a `matrix` instead — decomposed by `transform_from_affine`.
    ///
    /// The spec says a matrix "MUST be decomposable to TRS properties", so a
    /// conforming file never loses anything here. `out_of_square` below is the
    /// check that it was conforming.
    transform local{};
    bool from_matrix = false;
    float out_of_square = 0.0f;   ///< `transform_extraction::out_of_square`; 0 for TRS

    int mesh = -1;             ///< `meshes` index, or -1
    int skin = -1;             ///< `skins` index, or -1
};

/// One skin: which nodes are its joints, and the matrices that bind a mesh to them.
struct gltf_skin_desc
{
    std::string name;          ///< as written, or `"skin_0"`

    /// The joints, as NODE indices, **in the skin's own order** — the order
    /// `JOINTS_n` indexes and `inverse_bind` is parallel to. The spec imposes no
    /// order at all, and Blender happens to write one that is parent-first.
    std::vector<int> joints;

    /// One matrix per joint, parallel to `joints`, column-major as written.
    ///
    /// **EMPTY WHEN THE FILE GAVE NONE, and empty does not mean "compute them".**
    /// The spec (§5.28.1): "When undefined, each matrix is a 4x4 identity
    /// matrix." An importer that bakes inverse binds from the node pose when the
    /// accessor is absent is inventing data the file explicitly declined to give.
    std::vector<mat4> inverse_bind;

    /// The optional `skeleton` hint, or -1. The spec calls it a "pivot point" and
    /// says it "is not needed for computing skinning transforms"; it is carried
    /// for diagnostics only.
    int skeleton = -1;
};

/// Which property a channel animates.
enum class gltf_path
{
    translation,
    rotation,
    scale,
    weights      ///< morph-target weights: read, counted, and not played (no morphs yet)
};

/// How a sampler reconstructs values between its keys (spec Appendix C).
enum class gltf_interpolation
{
    linear,        ///< lerp — and SLERP for a rotation (§C.4)
    step,          ///< hold each key until the next
    cubic_spline   ///< Hermite, with an in-tangent, a value and an out-tangent per key
};

/// Keys: times, and the values at them, exactly as the accessors held them.
struct gltf_sampler_desc
{
    gltf_interpolation interpolation = gltf_interpolation::linear;

    /// Seconds from the start of the animation. The spec requires them strictly
    /// increasing; nothing here checks that — `anim::validate(clip)` counts it.
    std::vector<float> times;

    /// `components` floats per value, `times.size()` values — or THREE values per
    /// key for `cubic_spline`, in the order in-tangent, value, out-tangent.
    /// Rotations are x, y, z, w as written; `quat_from_xyzw` is how to read one.
    std::vector<float> values;

    /// 3 for a translation or scale, 4 for a rotation, 1 per morph target.
    int components = 0;
};

/// A channel: one property of one node, driven by one sampler.
struct gltf_channel_desc
{
    int node = -1;             ///< the target node; -1 = none ("SHOULD be ignored", §3.11)
    gltf_path path = gltf_path::translation;
    int sampler = -1;          ///< index into the animation's `samplers`
};

/// One animation: a set of channels, and the samplers they read.
///
/// Note what is NOT here: a duration, a loop flag or a frame rate. The spec
/// (§3.11) "doesn't define any runtime behavior, such as: order of playing,
/// auto-start, loops, mapping of timelines". An importer that wants a clip has
/// to decide those — `anim/import.hpp` says how it does.
struct gltf_animation_desc
{
    std::string name;          ///< as written, or `"animation_0"`
    std::vector<gltf_sampler_desc> samplers;
    std::vector<gltf_channel_desc> channels;
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

    // Lesson 7.7b. Filled for every file; empty vectors cost nothing, which is
    // why a static model's load did not have to change to make room for them.
    std::vector<gltf_node_desc> nodes;
    std::vector<gltf_skin_desc> skins;
    std::vector<gltf_animation_desc> animations;

    void clear()
    {
        primitives.clear();
        materials.clear();
        nodes.clear();
        skins.clear();
        animations.clear();
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
    int with_tangents = 0;      ///< …that carried TANGENT (6.7)
    int generated_tangents = 0; ///< …that did not, and had a frame derived (6.7)

    /// Materials the file marked `MASK` and `BLEND` — Lesson 6.11.
    ///
    /// **Counts, not complaints.** Both are now fully supported, so neither is a
    /// status; they are here because a scene that renders as expected except for
    /// one pane of glass is a scene where the first useful question is "did the
    /// file even say it was glass?", and a number answers that faster than a
    /// texture viewer does. They are also the caller's cue that this model needs
    /// `order_draws` — a scene with `blended_materials == 0` does not.
    int masked_materials = 0;
    int blended_materials = 0;

    // ---- What we did not build ---------------------------------------------
    int skipped_non_triangles = 0;  ///< point/line/strip/fan primitives dropped
    int skipped_no_positions = 0;   ///< primitives with no POSITION
    int skipped_too_large = 0;      ///< primitives over the uint16 index ceiling

    /// The largest vertex count seen in one primitive, whether or not it fit.
    /// The number to quote when `too_many_vertices` fires, because it says how
    /// far over the ceiling the asset actually is.
    int max_primitive_vertices = 0;

    // ---- Skins and animation (Lesson 7.7b) ---------------------------------
    int skins = 0;                  ///< `skins` entries
    int skinned_primitives = 0;     ///< primitives that came with influences
    int max_influence_sets = 0;     ///< the most JOINTS_n/WEIGHTS_n pairs on one primitive
    int animations = 0;             ///< `animations` entries
    int channels = 0;               ///< summed over them
    int matrix_nodes = 0;           ///< nodes that wrote `matrix` rather than TRS

    /// Skinned primitives whose influence data could not be used — a JOINTS_n
    /// with no matching WEIGHTS_n, or a count that disagrees with POSITION. The
    /// geometry is kept and drawn unskinned; the counter is how a character that
    /// stands in its bind pose while its clips play tells you why.
    int skipped_bad_influences = 0;

    /// Skins whose `inverseBindMatrices` accessor holds FEWER matrices than the
    /// skin has joints (the spec says "greater than or equal"). The skin is kept
    /// with no inverse binds — which is the identity, by the spec's own default —
    /// and this says the default was not what the file meant.
    int short_inverse_binds = 0;

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
/// come from a string literal in a test, or from a pack file in Module 9. A
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
