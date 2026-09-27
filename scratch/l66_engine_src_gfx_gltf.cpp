// engine/src/gfx/gltf.cpp — the only translation unit that has ever seen cgltf.
//
// Lesson 6.6. Read `engine/include/engine/gfx/gltf.hpp` first; the design lives
// there. This file is the mechanics, and it has three jobs:
//
//   1. hand the bytes to cgltf and turn its failure enum into ours;
//   2. WALK THE NODE TREE, because a glTF file is a scene and OBJ was a shape;
//   3. copy accessor data into `mesh_data`, which is the one place a
//      combinatorial format meets a single concrete layout.
//
// Everything cgltf-shaped stops at this file's last line. That is enforced by
// engine/CMakeLists.txt (the include directory is PRIVATE), not by discipline.

// CGLTF_IMPLEMENTATION must be defined in EXACTLY ONE translation unit, exactly
// like stb_image in image.cpp. Defining it in a header would compile a second
// copy of the parser into every file that included it and then fail to link on
// the duplicate symbols — which is the good outcome; the bad one is a build
// system that quietly picks one.
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <engine/core/log.hpp>
#include <engine/gfx/gltf.hpp>

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace engine {
namespace {

// ---------------------------------------------------------------------------
//  cgltf's vocabulary, translated into ours
// ---------------------------------------------------------------------------

/// cgltf reports twelve failure kinds; we report nine, and the mapping is where
/// the difference is decided rather than in nine call sites.
///
/// Note what is deliberately COLLAPSED: `unknown_format`, `invalid_json` and
/// `invalid_gltf` all become `invalid_format`, because the caller's response to
/// all three is identical ("this is not a file I can read") and a distinction
/// nobody branches on is a distinction that costs vocabulary for nothing. What
/// is NOT collapsed is `missing_buffer` — a file whose JSON is perfect and whose
/// `.bin` is absent is a completely different bug report, usually a copy that
/// moved one file and not the other.
[[nodiscard]] gltf_status status_of(cgltf_result r)
{
    switch (r)
    {
    case cgltf_result_success:          return gltf_status::ok;
    case cgltf_result_file_not_found:   return gltf_status::cannot_open;
    case cgltf_result_io_error:         return gltf_status::cannot_open;
    case cgltf_result_unknown_format:   return gltf_status::invalid_format;
    case cgltf_result_invalid_json:     return gltf_status::invalid_format;
    case cgltf_result_invalid_gltf:     return gltf_status::invalid_content;
    case cgltf_result_data_too_short:   return gltf_status::missing_buffer;
    case cgltf_result_legacy_gltf:      return gltf_status::invalid_content;
    default:                            return gltf_status::invalid_format;
    }
}

/// glTF's filter enumerants are **OpenGL's GLenum values** — 9728 is
/// `GL_NEAREST` — which is a piece of the format's history showing through.
/// Ours are SDL_GPU's (3.9). Hence a switch and not a cast.
///
/// The mip variants all collapse onto their base filter, because this engine has
/// no mipmaps yet (3.9 §7 names that gap and Module 6 fills it). Collapsing is
/// the right answer rather than a rejection: a file asking for
/// `LINEAR_MIPMAP_LINEAR` is asking for linear magnification too, and honouring
/// the half we can implement is better than refusing the asset.
[[nodiscard]] filter filter_of(cgltf_filter_type f)
{
    switch (f)
    {
    case cgltf_filter_type_nearest:
    case cgltf_filter_type_nearest_mipmap_nearest:
    case cgltf_filter_type_nearest_mipmap_linear:
        return filter::nearest;
    default:
        // Includes `undefined`, which the spec leaves to the client. Linear is
        // the engine's default (3.9) and the one an artist expects.
        return filter::linear;
    }
}

/// The same for wrapping. All three glTF modes have an exact counterpart,
/// because both enums are ultimately describing the same three answers to "what
/// is outside [0, 1]?" — which is 3.9's point that these are three right answers
/// rather than a default plus two curiosities.
[[nodiscard]] address_mode address_of(cgltf_wrap_mode w)
{
    switch (w)
    {
    case cgltf_wrap_mode_clamp_to_edge:    return address_mode::clamp_to_edge;
    case cgltf_wrap_mode_mirrored_repeat:  return address_mode::mirrored_repeat;
    default:                               return address_mode::repeat;
    }
}

// ---------------------------------------------------------------------------
//  Reading an accessor
// ---------------------------------------------------------------------------

/// Read `count` elements of `components` floats each out of an accessor.
///
/// **This one function is why we did not write the parser.** The accessor it
/// reads may be `float`, `uint8`, `uint16`, `int8` or `int16`; normalized or
/// not; tightly packed or interleaved with three other attributes at some
/// stride; and sparse, meaning a base array plus a patch list applied on top.
/// `cgltf_accessor_read_float` collapses every one of those cases to "give me
/// element i as floats", and writing it ourselves would have taught byte
/// offsets rather than rendering.
///
/// Returns false if any element could not be read, which is cgltf's way of
/// saying the accessor points outside its buffer view.
[[nodiscard]] bool read_floats(const cgltf_accessor* accessor,
                               std::size_t components,
                               std::vector<float>& out)
{
    if (accessor == nullptr) { return false; }

    out.resize(accessor->count * components);
    for (std::size_t i = 0; i < accessor->count; ++i)
    {
        if (!cgltf_accessor_read_float(accessor, i, &out[i * components], components))
        {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
//  Materials
// ---------------------------------------------------------------------------

/// Turn one cgltf material into a description.
///
/// Short, and it is worth noticing how short: `metallicFactor` and
/// `roughnessFactor` go straight into `microsurface` with no conversion at all.
/// That is Lessons 6.3 and 6.4 having derived the parameters rather than picking
/// them — an engine using a hand-tuned "gloss" would need a fitted curve here,
/// and the fit would be a guess that made every imported asset subtly wrong.
[[nodiscard]] gltf_material_desc describe_material(const cgltf_material& m, std::size_t index)
{
    gltf_material_desc desc;

    desc.name = (m.name != nullptr && m.name[0] != '\0')
        ? m.name
        : ("material_" + std::to_string(index));

    desc.double_sided = m.double_sided != 0;
    desc.wants_normal_texture = m.normal_texture.texture != nullptr;

    // A material may legally have no `pbrMetallicRoughness` block — it might be
    // using an extension we do not implement. Its defaults then apply, which is
    // what `desc` already holds.
    if (m.has_pbr_metallic_roughness)
    {
        const cgltf_pbr_metallic_roughness& pbr = m.pbr_metallic_roughness;

        // LINEAR ALREADY. See gltf.hpp: the factor is linear, the texture is
        // sRGB-encoded, and calling to_linear here is the classic importer bug.
        desc.base_colour = {pbr.base_color_factor[0],
                            pbr.base_color_factor[1],
                            pbr.base_color_factor[2]};
        desc.alpha = pbr.base_color_factor[3];

        desc.surface.metallic  = pbr.metallic_factor;
        desc.surface.roughness = pbr.roughness_factor;
        // f0 stays at k_dielectric_f0 = 0.04, which is exactly what glTF fixes
        // it to: the spec sets the dielectric IOR at 1.5, and
        // ((1-1.5)/(1+1.5))^2 = 0.04. Two independent derivations agreeing is
        // the reason no conversion is needed here either.

        desc.wants_metallic_roughness_texture =
            pbr.metallic_roughness_texture.texture != nullptr;

        const cgltf_texture* tex = pbr.base_color_texture.texture;
        if (tex != nullptr)
        {
            if (tex->image != nullptr && tex->image->uri != nullptr)
            {
                desc.base_colour_uri = tex->image->uri;
            }
            // An image with a `buffer_view` instead of a `uri` is embedded in
            // the .glb's binary chunk. We leave the URI empty rather than
            // inventing a name, and the counter in the report says the material
            // wanted a texture — the alternative is a material that silently
            // looks untextured with nothing anywhere saying why.

            if (tex->sampler != nullptr)
            {
                desc.samp.texel_filter = filter_of(tex->sampler->mag_filter);
                desc.samp.address_u    = address_of(tex->sampler->wrap_s);
                desc.samp.address_v    = address_of(tex->sampler->wrap_t);
            }
        }
    }

    return desc;
}

// ---------------------------------------------------------------------------
//  Node walking
// ---------------------------------------------------------------------------

/// cgltf writes a node's world matrix as sixteen floats, **column-major**, which
/// is exactly `mat4`'s own storage (2.5) — so this is a copy and not a
/// transpose.
///
/// It is worth pausing on how much of a coincidence that is not. glTF stores
/// matrices column-major because that is what its `matrix` property specifies;
/// `mat4` is column-major because its columns are where the basis vectors land;
/// and HLSL constant buffers want the same thing, which is why Module 4 uploads
/// a `mat4` with a plain memcpy. Three independent decisions, one layout, and
/// the transpose that catches out every second importer never arises.
[[nodiscard]] mat4 to_mat4_column_major(const float m[16])
{
    return mat4{{m[0],  m[1],  m[2],  m[3]},
                {m[4],  m[5],  m[6],  m[7]},
                {m[8],  m[9],  m[10], m[11]},
                {m[12], m[13], m[14], m[15]}};
}

/// Build one primitive's geometry from its accessors.
///
/// Returns false — having recorded WHY in the report — for the three cases a
/// primitive can be skipped: it is not made of triangles, it has no positions,
/// or it has more vertices than a `uint16` index can name.
[[nodiscard]] bool build_geometry(const cgltf_primitive& prim,
                                  mesh_data& out,
                                  gltf_report& report)
{
    // TRIANGLES ONLY, and the skip is counted rather than silent. Strips and
    // fans are legal glTF and a renderer that draws them is a renderer with a
    // second index-buffer topology to configure everywhere; points and lines
    // are a different pipeline entirely (4.8). Converting a strip to a list is
    // ten lines and is Exercise 4.
    if (prim.type != cgltf_primitive_type_triangles)
    {
        ++report.skipped_non_triangles;
        return false;
    }

    // ---- Find the attributes we understand --------------------------------
    //
    // Iterated by NAME rather than fetched by index, because a glTF primitive's
    // attributes are a dictionary and their order carries no meaning. A file may
    // carry TANGENT, COLOR_0, JOINTS_0 and four texture coordinate sets; we take
    // three and ignore the rest without complaint, which is the correct
    // behaviour for a format designed to be extended.
    const cgltf_accessor* positions = nullptr;
    const cgltf_accessor* normals = nullptr;
    const cgltf_accessor* uvs = nullptr;

    for (std::size_t a = 0; a < prim.attributes_count; ++a)
    {
        const cgltf_attribute& attr = prim.attributes[a];
        if (attr.type == cgltf_attribute_type_position && positions == nullptr)
        {
            positions = attr.data;
        }
        else if (attr.type == cgltf_attribute_type_normal && normals == nullptr)
        {
            normals = attr.data;
        }
        else if (attr.type == cgltf_attribute_type_texcoord && attr.index == 0)
        {
            // TEXCOORD_0 specifically. A material names which set it wants and
            // ours only ever wants the first, so a second set is data we can
            // read and have nothing to do with.
            uvs = attr.data;
        }
    }

    if (positions == nullptr || positions->count == 0)
    {
        ++report.skipped_no_positions;
        return false;
    }

    const int vertex_count = static_cast<int>(positions->count);
    report.max_primitive_vertices = std::max(report.max_primitive_vertices, vertex_count);

    // ---- The ceiling -------------------------------------------------------
    //
    // REPORTED, NEVER TRUNCATED. See k_max_primitive_vertices in the header for
    // why this is the one limit worth a named constant and a counter: wrapping
    // the index around produces geometry that draws, as a spray of triangles
    // between the wrong corners, with nothing anywhere saying so.
    if (vertex_count > k_max_primitive_vertices)
    {
        ++report.skipped_too_large;
        return false;
    }

    // ---- Positions ---------------------------------------------------------
    //
    // NO AXIS CONVERSION. glTF is right-handed Y-up and so are we (see the
    // header). The three floats are copied into a vec3 in the order they were
    // written, and verify_66 §B asserts that a hand-computed position in the
    // file arrives at the same numbers in memory.
    std::vector<float> scratch;
    if (!read_floats(positions, 3, scratch)) { return false; }

    out.vertices.resize(static_cast<std::size_t>(vertex_count));
    for (int i = 0; i < vertex_count; ++i)
    {
        out.vertices[static_cast<std::size_t>(i)] =
            vec3{scratch[static_cast<std::size_t>(i) * 3 + 0],
                 scratch[static_cast<std::size_t>(i) * 3 + 1],
                 scratch[static_cast<std::size_t>(i) * 3 + 2]};
    }

    // ---- Texture coordinates ----------------------------------------------
    //
    // AND NO V FLIP EITHER, which is the surprise if you arrive here from the
    // OBJ loader. glTF §3.9.3: "The origin of the texture coordinates (0, 0)
    // corresponds to the upper left corner of a texture image." That is
    // `texture`'s convention exactly (3.9), and OBJ's is the opposite — so
    // `mesh_import::flip_uv_v`, which is correct for every OBJ this engine has
    // ever loaded, would turn every glTF asset upside down. The flip belongs to
    // the FORMAT, not to the importer's taste, which is why it is applied in
    // load_obj's caller and not here.
    if (uvs != nullptr && uvs->count == positions->count && read_floats(uvs, 2, scratch))
    {
        out.uvs.resize(static_cast<std::size_t>(vertex_count));
        for (int i = 0; i < vertex_count; ++i)
        {
            out.uvs[static_cast<std::size_t>(i)] =
                vec2{scratch[static_cast<std::size_t>(i) * 2 + 0],
                     scratch[static_cast<std::size_t>(i) * 2 + 1]};
        }
        ++report.with_uvs;
    }

    // ---- Normals -----------------------------------------------------------
    if (normals != nullptr && normals->count == positions->count
        && read_floats(normals, 3, scratch))
    {
        out.normals.resize(static_cast<std::size_t>(vertex_count));
        for (int i = 0; i < vertex_count; ++i)
        {
            out.normals[static_cast<std::size_t>(i)] =
                vec3{scratch[static_cast<std::size_t>(i) * 3 + 0],
                     scratch[static_cast<std::size_t>(i) * 3 + 1],
                     scratch[static_cast<std::size_t>(i) * 3 + 2]};
        }
        ++report.with_normals;
    }

    // ---- Indices -----------------------------------------------------------
    //
    // A primitive may have none, in which case the vertices are consumed in
    // order — which is `mesh`'s "expanded" form (4.5) and is easiest to produce
    // by writing the identity index buffer rather than by giving the rest of the
    // engine a second case to handle.
    if (prim.indices != nullptr)
    {
        out.indices.resize(prim.indices->count);
        for (std::size_t i = 0; i < prim.indices->count; ++i)
        {
            // `cgltf_accessor_read_index` returns a cgltf_size whatever the
            // file's component type was — uint8, uint16 or uint32 — so the
            // narrowing happens HERE, once, where the ceiling above has already
            // guaranteed it is lossless.
            out.indices[i] =
                static_cast<std::uint16_t>(cgltf_accessor_read_index(prim.indices, i));
        }
    }
    else
    {
        out.indices.resize(static_cast<std::size_t>(vertex_count));
        for (int i = 0; i < vertex_count; ++i)
        {
            out.indices[static_cast<std::size_t>(i)] = static_cast<std::uint16_t>(i);
        }
    }

    // ---- Normals we had to invent -----------------------------------------
    //
    // The spec says a client SHOULD compute flat normals when a primitive has
    // none, and `with_normals` (3.6) already does exactly that — including
    // splitting the vertices, which is why the result is assigned back wholesale
    // rather than having its normals array filled in place.
    if (out.normals.empty())
    {
        out = with_normals(out.view(), normal_style::flat);
        ++report.generated_normals;
    }

    return true;
}

/// Walk one node and its children, emitting a primitive per mesh primitive.
///
/// Recursive, and the recursion is the shape of the data rather than a
/// preference: a glTF scene is a forest of nodes, each with a local transform,
/// and a node's world transform is the product down from the root. cgltf already
/// offers `cgltf_node_transform_world`, which walks up the parent chain — we use
/// it rather than accumulating on the way down, because it is the library's own
/// answer and it handles the `matrix` versus `TRS` distinction (a node may
/// specify either, never both) that we would otherwise have to re-implement.
void walk_node(const cgltf_node* node,
               const cgltf_data* data,
               gltf_scene_data& out,
               gltf_report& report)
{
    if (node == nullptr) { return; }
    ++report.nodes;

    if (node->mesh != nullptr)
    {
        float world[16];
        cgltf_node_transform_world(node, world);

        for (std::size_t p = 0; p < node->mesh->primitives_count; ++p)
        {
            const cgltf_primitive& prim = node->mesh->primitives[p];

            gltf_primitive built;
            if (!build_geometry(prim, built.geometry, report)) { continue; }

            built.world_from_local = to_mat4_column_major(world);
            built.material = (prim.material != nullptr)
                ? static_cast<int>(cgltf_material_index(data, prim.material))
                : -1;
            built.node_name = (node->name != nullptr && node->name[0] != '\0')
                ? node->name
                : ("node_" + std::to_string(cgltf_node_index(data, node)));

            report.vertices  += static_cast<int>(built.geometry.vertices.size());
            report.triangles += static_cast<int>(built.geometry.triangle_count());
            ++report.primitives;

            out.primitives.push_back(std::move(built));
        }
    }

    for (std::size_t c = 0; c < node->children_count; ++c)
    {
        walk_node(node->children[c], data, out, report);
    }
}

}   // namespace

// ---------------------------------------------------------------------------
//  The public surface
// ---------------------------------------------------------------------------

const char* name_of(gltf_status s)
{
    switch (s)
    {
    case gltf_status::ok:                    return "ok";
    case gltf_status::cannot_open:           return "cannot open";
    case gltf_status::invalid_format:        return "not glTF, or unreadable JSON";
    case gltf_status::invalid_content:       return "violates the glTF specification";
    case gltf_status::missing_buffer:        return "a buffer could not be resolved";
    case gltf_status::no_primitives:         return "nothing to draw";
    case gltf_status::unsupported_primitive: return "unsupported primitive mode";
    case gltf_status::missing_positions:     return "a primitive has no POSITION";
    case gltf_status::too_many_vertices:     return "over the uint16 index ceiling";
    }
    return "?";
}

gltf_material_desc gltf_default_material()
{
    // glTF §5.19: a primitive with no material uses base colour (1,1,1,1),
    // metallic 1.0, roughness 1.0. Spelled as the spec's numbers rather than as
    // `microsurface{}`'s defaults, which are the ENGINE's opinion (0.5 / 0.0)
    // and a different thing entirely.
    gltf_material_desc desc;
    desc.name = "glTF default";
    desc.base_colour = {1.0f, 1.0f, 1.0f};
    desc.alpha = 1.0f;
    desc.surface.metallic = 1.0f;
    desc.surface.roughness = 1.0f;
    return desc;
}

gltf_report parse_gltf(std::span<const std::byte> bytes,
                       const char* base_dir,
                       gltf_scene_data& out)
{
    gltf_report report;
    out.clear();

    if (bytes.empty())
    {
        report.status = gltf_status::cannot_open;
        ENGINE_LOG_ERROR(log_asset, "glTF: empty input");
        return report;
    }

    // Zero-initialised options means "auto-detect the container and use the
    // default allocator and file reader". The auto-detect reads the first four
    // bytes: "glTF" is the binary container, anything else is parsed as JSON.
    cgltf_options options{};
    cgltf_data* data = nullptr;

    cgltf_result result = cgltf_parse(&options, bytes.data(), bytes.size(), &data);
    if (result != cgltf_result_success)
    {
        report.status = status_of(result);
        ENGINE_LOG_ERROR(log_asset, "glTF: parse failed (%s)", name_of(report.status));
        return report;
    }

    // From here every exit must free `data`. This is C, so there is no
    // destructor to lean on — which is itself the argument for keeping cgltf
    // inside one file with one exit path per failure, rather than letting a
    // `cgltf_data*` travel.
    report.binary = (data->file_type == cgltf_file_type_glb);
    report.meshes = static_cast<int>(data->meshes_count);
    report.file_materials = static_cast<int>(data->materials_count);
    report.file_images = static_cast<int>(data->images_count);
    if (data->asset.generator != nullptr) { report.generator = data->asset.generator; }

    // RESOLVE THE BUFFERS, which is the step that turns a description into
    // bytes. Three sources, all handled here: an external `.bin` beside the
    // file, a base64 `data:` URI inline in the JSON, and the binary chunk of a
    // `.glb`. `base_dir` is what makes the first one possible, and its absence
    // is exactly why `parse_gltf` takes it.
    const std::string dir = (base_dir != nullptr) ? std::string(base_dir) : std::string();
    result = cgltf_load_buffers(&options, data, dir.empty() ? nullptr : dir.c_str());
    if (result != cgltf_result_success)
    {
        report.status = gltf_status::missing_buffer;
        ENGINE_LOG_ERROR(log_asset,
                         "glTF: buffers unresolved (base dir '%s'). An external .bin "
                         "must sit beside the .gltf that names it.",
                         dir.c_str());
        cgltf_free(data);
        return report;
    }

    // cgltf_validate checks the invariants the SPEC states and the parser does
    // not: that accessors fit inside their buffer views, that an index accessor
    // has an integer component type, that a mesh's attributes agree on a count.
    // Running it is the difference between rejecting a bad file and reading past
    // the end of a buffer — and it is one line.
    result = cgltf_validate(data);
    if (result != cgltf_result_success)
    {
        report.status = gltf_status::invalid_content;
        ENGINE_LOG_ERROR(log_asset, "glTF: failed validation");
        cgltf_free(data);
        return report;
    }

    // ---- Materials first, so a primitive's index means something -----------
    out.materials.reserve(data->materials_count);
    for (std::size_t i = 0; i < data->materials_count; ++i)
    {
        out.materials.push_back(describe_material(data->materials[i], i));
    }

    // ---- Then the scene ----------------------------------------------------
    //
    // `data->scene` is the file's DEFAULT scene, which may be absent — a glTF
    // file is allowed to be a library of nodes with no scene selected, and a
    // viewer is then free to pick. We fall back to the first declared scene, and
    // failing that to every root node in the file, so that a hand-written test
    // file without a `scene` property still loads.
    const cgltf_scene* scene = data->scene;
    if (scene == nullptr && data->scenes_count > 0) { scene = &data->scenes[0]; }

    if (scene != nullptr)
    {
        for (std::size_t n = 0; n < scene->nodes_count; ++n)
        {
            walk_node(scene->nodes[n], data, out, report);
        }
    }
    else
    {
        for (std::size_t n = 0; n < data->nodes_count; ++n)
        {
            if (data->nodes[n].parent == nullptr)
            {
                walk_node(&data->nodes[n], data, out, report);
            }
        }
    }

    cgltf_free(data);

    // ---- Was any of that worth having? ------------------------------------
    if (out.primitives.empty())
    {
        // The three skip counters are what makes this diagnosable. "Nothing to
        // draw" plus "skipped_too_large = 1, max 197,346" is a bug report; the
        // status alone is a shrug.
        report.status = report.skipped_too_large > 0    ? gltf_status::too_many_vertices
                      : report.skipped_no_positions > 0 ? gltf_status::missing_positions
                      : report.skipped_non_triangles > 0
                          ? gltf_status::unsupported_primitive
                          : gltf_status::no_primitives;
        ENGINE_LOG_ERROR(log_asset, "glTF: %s (nodes %d, meshes %d)",
                         name_of(report.status), report.nodes, report.meshes);
        return report;
    }

    report.status = gltf_status::ok;
    return report;
}

gltf_report load_gltf(const char* path, gltf_scene_data& out)
{
    gltf_report report;
    out.clear();

    if (path == nullptr)
    {
        report.status = gltf_status::cannot_open;
        return report;
    }

    std::size_t size = 0;
    void* data = SDL_LoadFile(path, &size);
    if (data == nullptr)
    {
        report.status = gltf_status::cannot_open;
        ENGINE_LOG_ERROR(log_asset, "glTF: cannot open '%s': %s", path, SDL_GetError());
        return report;
    }

    // The directory the file came from, WITH its trailing separator, because
    // that is what cgltf concatenates a relative buffer URI onto. A file in the
    // working directory has no separator at all and gets an empty base dir,
    // which is the correct answer rather than a case to special-case.
    std::string base_dir(path);
    const std::size_t cut = base_dir.find_last_of("/\\");
    base_dir = (cut == std::string::npos) ? std::string() : base_dir.substr(0, cut + 1);

    report = parse_gltf(std::span<const std::byte>(static_cast<const std::byte*>(data), size),
                        base_dir.empty() ? nullptr : base_dir.c_str(), out);

    // SDL_free, not free: memory allocated inside SDL is returned to SDL's
    // allocator, for the cross-runtime reason load_obj states at length (3.5).
    SDL_free(data);
    return report;
}

}   // namespace engine
