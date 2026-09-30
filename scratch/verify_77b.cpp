// scratch/verify_77b.cpp — every number Lesson 7.7b prints, measured rather than
// asserted.
//
// Build and run:  sh scratch/build_verify_77b.sh
//
// THE FIXTURES ARE FILES, AND TWO KINDS OF FILE ON PURPOSE.
//
//   * Blender's. assets/mannequin.glb and its two siblings in scratch/ were
//     written by Blender 5.1's glTF exporter (scratch/make_mannequin.py). Nothing
//     in this engine had a hand in their joint order, inverse binds, key times or
//     weights, which is the whole reason they are here: Module 8's rule that
//     hand-picked data agrees with the code by construction.
//   * Hand-built, in memory, by `mini_gltf` below: the SPEC's own skin example,
//     a chain listed child-first, a vertex with six influences, sparse keys at
//     known arcs. These exist because Blender's files are too well-behaved to
//     break anything — its joints are already parent-first, its rotation keys a
//     frame apart — and a check that only ever sees the easy case has not been
//     tested.
//
// Every section carries a control, and each control is written to answer 7.6's
// question: what would this print if the thing under test were completely
// broken?
//
// OUTPUT WIDTH: nothing below exceeds 66 characters (the lesson's <pre> fold).

#include <engine/anim/clip.hpp>
#include <engine/anim/import.hpp>
#include <engine/anim/skeleton.hpp>
#include <engine/anim/skin.hpp>
#include <engine/gfx/gltf.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/transform.hpp>
#include <engine/math/vec3.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <numbers>
#include <string>
#include <vector>

using engine::mat4;
using engine::quat;
using engine::transform;
using engine::vec3;
using engine::vec4;
using namespace engine::anim;

namespace {

constexpr float k_pi = std::numbers::pi_v<float>;

int checks_run = 0;
int checks_passed = 0;
double sink = 0.0;

float deg(float radians) { return radians * 180.0f / k_pi; }
float rad(float degrees) { return degrees * k_pi / 180.0f; }

void check(const char* name, bool ok)
{
    ++checks_run;
    if (ok) { ++checks_passed; }
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name);
}

void section(const char* title) { std::printf("\n== %s\n", title); }

// ---- Loading ----------------------------------------------------------------

struct loaded
{
    engine::gltf_scene_data data;
    engine::gltf_report report;
};

[[nodiscard]] loaded load(const char* path)
{
    loaded l;
    l.report = engine::load_gltf(path, l.data);
    return l;
}

// ---- mini_gltf: a glTF file written in memory ---------------------------------
//
// Just enough of the format to build the test files this harness needs, and no
// more: one buffer as a base64 data URI, float and unsigned-byte accessors, and
// the JSON assembled as text. It is a DIFFERENT implementation of glTF writing
// from Blender's and from 6.6's generator, which is the point of having it.

std::string base64(const std::vector<unsigned char>& in)
{
    static const char* k = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    std::size_t i = 0;
    for (; i + 2 < in.size(); i += 3)
    {
        const unsigned v = (in[i] << 16u) | (in[i + 1] << 8u) | in[i + 2];
        out += k[(v >> 18) & 63u]; out += k[(v >> 12) & 63u];
        out += k[(v >> 6) & 63u];  out += k[v & 63u];
    }
    if (i + 1 == in.size())
    {
        const unsigned v = in[i] << 16u;
        out += k[(v >> 18) & 63u]; out += k[(v >> 12) & 63u]; out += "==";
    }
    else if (i + 2 == in.size())
    {
        const unsigned v = (in[i] << 16u) | (in[i + 1] << 8u);
        out += k[(v >> 18) & 63u]; out += k[(v >> 12) & 63u];
        out += k[(v >> 6) & 63u];  out += '=';
    }
    return out;
}

struct mini_gltf
{
    std::vector<unsigned char> bin;
    std::string accessors;
    std::string views;
    int count = 0;

    // Append raw bytes, 4-aligned, as a buffer view + accessor; return its index.
    int add(const void* data, std::size_t bytes, int component, const char* type,
            std::size_t elements, const char* extra = "")
    {
        while (bin.size() % 4u != 0u) { bin.push_back(0); }
        const std::size_t offset = bin.size();
        const auto* p = static_cast<const unsigned char*>(data);
        bin.insert(bin.end(), p, p + bytes);
        char buf[512];
        std::snprintf(buf, sizeof buf, "%s{\"buffer\":0,\"byteOffset\":%zu,\"byteLength\":%zu}",
                      views.empty() ? "" : ",", offset, bytes);
        views += buf;
        std::snprintf(buf, sizeof buf,
                      "%s{\"bufferView\":%d,\"componentType\":%d,\"type\":\"%s\","
                      "\"count\":%zu%s}",
                      accessors.empty() ? "" : ",", count, component, type, elements, extra);
        accessors += buf;
        return count++;
    }
    int floats(const std::vector<float>& f, const char* type, std::size_t per, const char* extra = "")
    {
        return add(f.data(), f.size() * 4u, 5126, type, f.size() / per, extra);
    }
    int times(const std::vector<float>& t)
    {
        char mm[96];
        std::snprintf(mm, sizeof mm, ",\"min\":[%.9g],\"max\":[%.9g]", t.front(), t.back());
        return floats(t, "SCALAR", 1, mm);
    }
    int ubytes(const std::vector<unsigned char>& b, const char* type, std::size_t per)
    {
        return add(b.data(), b.size(), 5121, type, b.size() / per);
    }
    int ushorts(const std::vector<std::uint16_t>& s)
    {
        return add(s.data(), s.size() * 2u, 5123, "SCALAR", s.size());
    }

    // The whole file: `body` is the JSON between the asset and the buffers.
    [[nodiscard]] std::string json(const std::string& body) const
    {
        return "{\"asset\":{\"version\":\"2.0\",\"generator\":\"verify_77b mini_gltf\"},"
               + body + ",\"accessors\":[" + accessors + "],\"bufferViews\":[" + views
               + "],\"buffers\":[{\"byteLength\":" + std::to_string(bin.size())
               + ",\"uri\":\"data:application/octet-stream;base64," + base64(bin) + "\"}]}";
    }
};

[[nodiscard]] loaded parse_text(const std::string& text)
{
    loaded l;
    l.report = engine::parse_gltf(
        std::span<const std::byte>(reinterpret_cast<const std::byte*>(text.data()), text.size()),
        nullptr, l.data);
    return l;
}

// ---- Section A: the file as Blender wrote it ----------------------------------

void section_a()
{
    section("A  the file as Blender wrote it");
    const loaded f = load("assets/mannequin.glb");
    const engine::gltf_report& r = f.report;
    std::printf("  generator     %s\n", r.generator.c_str());
    std::printf("  status %s, nodes %zu, skins %d, primitives %d\n",
                engine::name_of(r.status), f.data.nodes.size(), r.skins, r.primitives);
    std::printf("  vertices %d (skinned prims %d, sets %d)\n", r.vertices,
                r.skinned_primitives, r.max_influence_sets);
    std::printf("  animations %d, channels %d, matrix nodes %d\n", r.animations,
                r.channels, r.matrix_nodes);
    check("A.1 Blender's file parses, with one skin and two prims",
          r.ok() && r.skins == 1 && r.skinned_primitives == 2);

    // Joint order: the skin's and the node array's.
    const engine::gltf_skin_desc& skin = f.data.skins[0];
    std::size_t node_order_breaks = 0;
    for (int n : skin.joints)
    {
        const int p = f.data.nodes[static_cast<std::size_t>(n)].parent;
        if (p >= 0 && p > n) { ++node_order_breaks; }
    }
    std::printf("  skin joints %zu; parents AFTER child in node order: %zu\n",
                skin.joints.size(), node_order_breaks);

    imported_rig rig;
    const rig_import_report ir = import_rig(f.data, 0, import_settings{}, rig);
    std::printf("  import %s: %zu joints = %zu skin + %zu ancestor\n", name_of(ir.status),
                ir.joints, ir.skin_joints, ir.ancestors);
    std::printf("  file-order breaks %zu, resorted %zu\n", ir.file_order_breaks, ir.resorted);
    std::printf("  root joint '%s', identity IBMs %s\n", rig.sk.joints[0].name.c_str(),
                ir.identity_inverse_binds ? "yes" : "no");
    std::printf("  bind vs rest %.3e, depth %zu, nonuniform binds %zu\n",
                static_cast<double>(ir.bind_vs_rest), ir.skeleton.depth,
                ir.skeleton.nonuniform_binds);
    std::printf("  meshes %zu, vertices %zu, influences 1..4: %zu %zu %zu %zu\n",
                ir.meshes, ir.vertices, ir.influences[1], ir.influences[2],
                ir.influences[3], ir.influences[4]);
    std::printf("  worst weight sum error %.3e, repaired %zu\n",
                static_cast<double>(ir.worst_weight_sum), ir.repaired);
    std::printf("  clips %zu, channels %zu (bound %zu, unbound %zu)\n", ir.clips,
                ir.channels, ir.bound, ir.unbound);
    std::printf("  by interpolation: linear %zu, step %zu, cubic %zu\n", ir.linear,
                ir.step, ir.cubic);
    std::printf("  keys in %zu -> out %zu (split %zu, step %zu)\n", ir.keys_in,
                ir.keys_out, ir.split_keys, ir.step_keys);
    std::printf("  worst fit: %.3e m, %.3e deg, %.3e scale\n",
                static_cast<double>(ir.worst_position),
                static_cast<double>(deg(ir.worst_rotation)),
                static_cast<double>(ir.worst_scale));
    std::printf("  sign flips %zu, frozen vertices %zu\n", ir.sign_flips, ir.frozen_vertices);
    for (const clip& c : rig.clips)
    {
        const clip_report cr = validate(c, rig.sk);
        std::printf("  clip %-5s %.3f s  keys %zu  seam %.2e deg\n", c.name.c_str(),
                    static_cast<double>(c.duration), cr.keys,
                    static_cast<double>(deg(cr.loop_gap_radians)));
    }
}


// ---- A reference that shares nothing with the engine ---------------------------
//
// Section G checks the whole pipeline against the spec's formula, and a check is
// only as independent as its reference. So the reference below is written from
// the SPEC's text, in DOUBLE precision, with its own matrix type, its own sampler
// (the spec's slerp formula, not `quat_slerp`) and its own tree walk (recursion
// by parent pointer, not a parent-first array) — and it reads the file's
// descriptions directly, all influence sets, weights unnormalised.

struct dmat
{
    double m[16];   // column-major, like the file
};

dmat dmul(const dmat& a, const dmat& b)
{
    dmat r{};
    for (int c = 0; c < 4; ++c)
    {
        for (int row = 0; row < 4; ++row)
        {
            double s = 0.0;
            for (int k = 0; k < 4; ++k) { s += a.m[k * 4 + row] * b.m[c * 4 + k]; }
            r.m[c * 4 + row] = s;
        }
    }
    return r;
}

struct dtrs
{
    double t[3]{0, 0, 0};
    double q[4]{0, 0, 0, 1};   // x, y, z, w — the file's order, kept
    double s[3]{1, 1, 1};
};

dmat dcompose(const dtrs& n)
{
    const double x = n.q[0], y = n.q[1], z = n.q[2], w = n.q[3];
    // T * R * S, spec §3.5.3.
    const double r[9] = {1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w),
                         2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w),
                         2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y)};
    dmat m{};
    for (int c = 0; c < 3; ++c)
    {
        for (int row = 0; row < 3; ++row) { m.m[c * 4 + row] = r[c * 3 + row] * n.s[c]; }
    }
    m.m[12] = n.t[0]; m.m[13] = n.t[1]; m.m[14] = n.t[2]; m.m[15] = 1.0;
    return m;
}

dmat dfrom(const mat4& a)
{
    dmat r{};
    const vec4 c[4] = {a.c0, a.c1, a.c2, a.c3};
    for (int i = 0; i < 4; ++i)
    {
        r.m[i * 4 + 0] = c[i].x; r.m[i * 4 + 1] = c[i].y;
        r.m[i * 4 + 2] = c[i].z; r.m[i * 4 + 3] = c[i].w;
    }
    return r;
}

/// The spec's Appendix C, evaluated at `t` for `width` components. For LINEAR
/// rotations, §C.4's formula exactly: a = acos(|dot|), s = sign(dot).
void spec_sample(const engine::gltf_sampler_desc& s, bool rotation, double t, double* out)
{
    const int w = s.components;
    const std::size_t n = s.times.size();
    const bool cubic = s.interpolation == engine::gltf_interpolation::cubic_spline;
    auto val = [&](std::size_t k, int c) {
        return static_cast<double>(s.values[(cubic ? 3 * k + 1 : k) * static_cast<std::size_t>(w) + static_cast<std::size_t>(c)]);
    };
    if (t <= s.times[0] || n == 1)
    {
        for (int c = 0; c < w; ++c) { out[c] = val(0, c); }
        return;
    }
    if (t >= s.times[n - 1])
    {
        for (int c = 0; c < w; ++c) { out[c] = val(n - 1, c); }
        return;
    }
    std::size_t k = 0;
    while (!(t >= s.times[k] && t < s.times[k + 1])) { ++k; }
    const double td = static_cast<double>(s.times[k + 1]) - s.times[k];
    const double u = (t - s.times[k]) / td;
    if (s.interpolation == engine::gltf_interpolation::step)
    {
        for (int c = 0; c < w; ++c) { out[c] = val(k, c); }
        return;
    }
    if (cubic)
    {
        const double u2 = u * u, u3 = u2 * u;
        for (int c = 0; c < w; ++c)
        {
            const double a1 = s.values[(3 * (k + 1)) * static_cast<std::size_t>(w) + static_cast<std::size_t>(c)];
            const double b0 = s.values[(3 * k + 2) * static_cast<std::size_t>(w) + static_cast<std::size_t>(c)];
            out[c] = (2 * u3 - 3 * u2 + 1) * val(k, c) + td * (u3 - 2 * u2 + u) * b0
                   + (-2 * u3 + 3 * u2) * val(k + 1, c) + td * (u3 - u2) * a1;
        }
        if (rotation)
        {
            const double l = std::sqrt(out[0] * out[0] + out[1] * out[1] + out[2] * out[2] + out[3] * out[3]);
            for (int c = 0; c < 4; ++c) { out[c] /= l; }
        }
        return;
    }
    if (!rotation)
    {
        for (int c = 0; c < w; ++c) { out[c] = (1 - u) * val(k, c) + u * val(k + 1, c); }
        return;
    }
    double d = 0;
    for (int c = 0; c < 4; ++c) { d += val(k, c) * val(k + 1, c); }
    const double sgn = d < 0 ? -1.0 : 1.0;
    const double a = std::acos(std::min(1.0, std::fabs(d)));
    if (a < 1e-9)
    {
        for (int c = 0; c < 4; ++c) { out[c] = val(k, c); }
        return;
    }
    for (int c = 0; c < 4; ++c)
    {
        out[c] = std::sin(a * (1 - u)) / std::sin(a) * val(k, c)
               + sgn * std::sin(a * u) / std::sin(a) * val(k + 1, c);
    }
}

/// Every node's local TRS at time `t` of animation `anim` (-1: the rest pose).
std::vector<dtrs> spec_pose(const engine::gltf_scene_data& d, int anim, double t)
{
    std::vector<dtrs> pose(d.nodes.size());
    for (std::size_t i = 0; i < d.nodes.size(); ++i)
    {
        const transform& l = d.nodes[i].local;
        pose[i].t[0] = l.position.x; pose[i].t[1] = l.position.y; pose[i].t[2] = l.position.z;
        pose[i].q[0] = l.rotation.v.x; pose[i].q[1] = l.rotation.v.y;
        pose[i].q[2] = l.rotation.v.z; pose[i].q[3] = l.rotation.w;
        pose[i].s[0] = l.scale.x; pose[i].s[1] = l.scale.y; pose[i].s[2] = l.scale.z;
    }
    if (anim < 0) { return pose; }
    const engine::gltf_animation_desc& a = d.animations[static_cast<std::size_t>(anim)];
    for (const engine::gltf_channel_desc& ch : a.channels)
    {
        if (ch.node < 0 || ch.path == engine::gltf_path::weights) { continue; }
        const engine::gltf_sampler_desc& s = a.samplers[static_cast<std::size_t>(ch.sampler)];
        dtrs& n = pose[static_cast<std::size_t>(ch.node)];
        switch (ch.path)
        {
        case engine::gltf_path::translation: spec_sample(s, false, t, n.t); break;
        case engine::gltf_path::rotation:    spec_sample(s, true, t, n.q);  break;
        case engine::gltf_path::scale:       spec_sample(s, false, t, n.s); break;
        default: break;
        }
    }
    return pose;
}

/// Global matrix of `node`, by recursion up the parent pointers.
dmat spec_global(const engine::gltf_scene_data& d, const std::vector<dtrs>& pose, int node)
{
    const dmat local = dcompose(pose[static_cast<std::size_t>(node)]);
    const int p = d.nodes[static_cast<std::size_t>(node)].parent;
    return (p < 0) ? local : dmul(spec_global(d, pose, p), local);
}

/// The spec's skinning of primitive `prim` at the pose: every influence set, the
/// file's weights as stored, jointMatrix = global(joint) * inverseBind.
std::vector<vec3> spec_skin(const engine::gltf_scene_data& d, int prim, int anim, double t,
                            bool apply_inverse_binds = true)
{
    const engine::gltf_primitive& p = d.primitives[static_cast<std::size_t>(prim)];
    const engine::gltf_skin_desc& skin = d.skins[static_cast<std::size_t>(p.skin)];
    const std::vector<dtrs> pose = spec_pose(d, anim, t);
    std::vector<dmat> jm(skin.joints.size());
    for (std::size_t j = 0; j < skin.joints.size(); ++j)
    {
        const dmat g = spec_global(d, pose, skin.joints[j]);
        jm[j] = (apply_inverse_binds && !skin.inverse_bind.empty())
                    ? dmul(g, dfrom(skin.inverse_bind[j])) : g;
    }
    const std::size_t per = static_cast<std::size_t>(p.influence_sets) * 4u;
    std::vector<vec3> out(p.geometry.vertices.size());
    for (std::size_t v = 0; v < out.size(); ++v)
    {
        const vec3 b = p.geometry.vertices[v];
        double acc[3] = {0, 0, 0};
        for (std::size_t k = 0; k < per; ++k)
        {
            const double w = p.weights[v * per + k];
            if (w == 0.0) { continue; }
            const dmat& m = jm[p.joints[v * per + k]];
            for (int r = 0; r < 3; ++r)
            {
                acc[r] += w * (m.m[0 * 4 + r] * b.x + m.m[1 * 4 + r] * b.y
                               + m.m[2 * 4 + r] * b.z + m.m[3 * 4 + r]);
            }
        }
        out[v] = {static_cast<float>(acc[0]), static_cast<float>(acc[1]),
                  static_cast<float>(acc[2])};
    }
    return out;
}

/// OUR pipeline for mesh `m` of an imported rig: sample, compose, palette, skin.
std::vector<vec3> ours_skin(const imported_rig& rig, std::size_t m, int clip_index, float t,
                            bool inverse_binds = true)
{
    std::vector<transform> pose;
    if (clip_index < 0) { rest_pose(rig.sk, pose); }
    else { sample_at(rig.clips[static_cast<std::size_t>(clip_index)], rig.sk, t, pose); }
    std::vector<mat4> posed;
    std::vector<mat4> palette;
    skinning_palette(rig.sk, pose, posed, palette);
    const skinned_mesh& sm = rig.meshes[m];
    std::vector<vec3> out(sm.vertex_count());
    skin_positions(sm.bind.vertices, sm.influences, inverse_binds ? palette : posed, out);
    return out;
}

float worst_gap(const std::vector<vec3>& a, const std::vector<vec3>& b)
{
    float worst = 0.0f;
    for (std::size_t i = 0; i < a.size() && i < b.size(); ++i)
    {
        worst = std::max(worst, engine::length(a[i] - b[i]));
    }
    return worst;
}

// ---- Section B: the spec's own example -----------------------------------------
//
// §3.7.3.2's example, completed into a file: node_0 translates (0,1,0) and is
// NOT a joint; node_1 scales 0.5 and is; node_2 is a joint one unit along node_1's
// x; node_3/node_4 carry the skinned mesh and a translation and a rotation the
// spec says MUST be ignored. Every number in the lesson's worked example is here.

std::string spec_example(bool with_animation)
{
    mini_gltf g;
    // A triangle in model space: v0 on joint A (node_1), v1 on joint B (node_2),
    // v2 half and half.
    const std::vector<float> pos = {0.0f, 1.0f, 0.0f, 0.5f, 1.0f, 0.0f, 0.25f, 1.5f, 0.0f};
    const std::vector<unsigned char> joints = {0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0};
    const std::vector<float> weights = {1, 0, 0, 0, 1, 0, 0, 0, 0.5f, 0.5f, 0, 0};
    const std::vector<std::uint16_t> idx = {0, 1, 2};
    // Inverse binds: inverse of each joint's GLOBAL rest matrix.
    //   global(A) = T(0,1,0) S(.5)            -> inverse S(2) T(0,-1,0)
    //   global(B) = T(0,1,0) S(.5) T(1,0,0)   -> inverse S(2) T(-.5,-1,0)
    const std::vector<float> ibm = {2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2, 0, 0, -2, 0, 1,
                                    2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2, 0, -1, -2, 0, 1};
    const int a_pos = g.floats(pos, "VEC3", 3,
                               ",\"min\":[0,1,0],\"max\":[0.5,1.5,0]");
    const int a_j = g.ubytes(joints, "VEC4", 4);
    const int a_w = g.floats(weights, "VEC4", 4);
    const int a_i = g.ushorts(idx);
    const int a_ibm = g.floats(ibm, "MAT4", 16);
    std::string anim;
    if (with_animation)
    {
        // node_1 turns 90 degrees about z over one second: two LINEAR keys.
        const float h = std::sqrt(0.5f);
        const int a_t = g.times({0.0f, 1.0f});
        const int a_q = g.floats({0, 0, 0, 1, 0, 0, h, h}, "VEC4", 4);
        char buf[256];
        std::snprintf(buf, sizeof buf,
                      ",\"animations\":[{\"name\":\"turn\",\"samplers\":[{\"input\":%d,"
                      "\"output\":%d}],\"channels\":[{\"sampler\":0,\"target\":"
                      "{\"node\":1,\"path\":\"rotation\"}}]}]", a_t, a_q);
        anim = buf;
    }
    char body[1024];
    std::snprintf(body, sizeof body,
        "\"scene\":0,\"scenes\":[{\"nodes\":[0,3]}],\"nodes\":["
        "{\"name\":\"node_0\",\"children\":[1],\"translation\":[0,1,0]},"
        "{\"name\":\"node_1\",\"children\":[2],\"scale\":[0.5,0.5,0.5]},"
        "{\"name\":\"node_2\",\"translation\":[1,0,0]},"
        "{\"name\":\"node_3\",\"children\":[4],\"translation\":[1,0,0]},"
        "{\"name\":\"node_4\",\"mesh\":0,\"skin\":0,\"rotation\":[0,1,0,0]}],"
        "\"skins\":[{\"inverseBindMatrices\":%d,\"joints\":[1,2],\"skeleton\":1}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":%d,"
        "\"JOINTS_0\":%d,\"WEIGHTS_0\":%d},\"indices\":%d}]}]%s",
        a_ibm, a_pos, a_j, a_w, a_i, anim.c_str());
    return g.json(body);
}

void section_b()
{
    section("B  the spec's own example (§3.7.3.2)");
    const loaded f = parse_text(spec_example(true));
    check("B.1 the hand-written file parses", f.report.ok() && f.data.skins.size() == 1);

    // NOT A LOOP. glTF has no loop flag, so the importer has to be told: a turn
    // that ends somewhere else is not a cycle, and with the default (`loops`
    // true) t = 1 wraps to t = 0 and the joint snaps back to rest.
    import_settings once;
    once.loops = false;
    imported_rig rig;
    const rig_import_report r = import_rig(f.data, 0, once, rig);
    std::printf("  joints %zu (skin %zu + ancestors %zu): ", r.joints, r.skin_joints,
                r.ancestors);
    for (const joint& j : rig.sk.joints) { std::printf("%s ", j.name.c_str()); }
    std::printf("\n  bind vs rest %.2e\n", static_cast<double>(r.bind_vs_rest));
    check("B.2 node_0 is carried as a joint above node_1",
          r.ancestors == 1 && rig.sk.joints[0].name == "node_0");

    // At rest every palette entry is the identity, so the triangle is where the
    // file put it — and NOT where node_3/node_4 would move it.
    const std::vector<vec3> rest = ours_skin(rig, 0, -1, 0.0f);
    std::printf("  rest: v0 (%.3f, %.3f, %.3f)  v1 (%.3f, %.3f, %.3f)\n",
                static_cast<double>(rest[0].x), static_cast<double>(rest[0].y),
                static_cast<double>(rest[0].z), static_cast<double>(rest[1].x),
                static_cast<double>(rest[1].y), static_cast<double>(rest[1].z));
    check("B.3 at rest the mesh node's transform is ignored",
          engine::length(rest[0] - vec3{0.0f, 1.0f, 0.0f}) < 1e-6f);

    // CONTROL 1: an importer that applied the skinned mesh node's world matrix.
    const mat4 mesh_world = f.data.primitives[0].world_from_local;
    const vec3 wrong = engine::xyz(mesh_world * engine::point(rest[0]));
    std::printf("  control, mesh node applied: v0 -> (%.3f, %.3f, %.3f)\n",
                static_cast<double>(wrong.x), static_cast<double>(wrong.y),
                static_cast<double>(wrong.z));
    check("B.4 control: applying it moves v0 by a metre",
          engine::length(wrong - rest[0]) > 0.9f);

    // CONTROL 2: an importer that dropped the non-joint ancestor. Rebuild the
    // skeleton without node_0: node_1 becomes a root with its own transform only.
    imported_rig dropped = rig;
    dropped.sk.joints.erase(dropped.sk.joints.begin());
    for (joint& j : dropped.sk.joints)
    {
        j.parent = (j.parent == 0 || j.parent == k_no_parent) ? k_no_parent
                                                              : static_cast<joint_index>(j.parent - 1);
    }
    dropped.sk.joint_from_model.erase(dropped.sk.joint_from_model.begin());
    for (skin_influence& inf : dropped.meshes[0].influences)
    {
        for (joint_index& j : inf.joints) { j = (j == 0) ? 0 : static_cast<joint_index>(j - 1); }
    }
    const std::vector<vec3> lost = ours_skin(dropped, 0, -1, 0.0f);
    std::printf("  control, node_0 dropped: v0 -> (%.3f, %.3f, %.3f)\n",
                static_cast<double>(lost[0].x), static_cast<double>(lost[0].y),
                static_cast<double>(lost[0].z));
    check("B.5 control: dropping node_0 sinks the mesh a metre",
          std::fabs(lost[0].y - 0.0f) < 1e-6f);

    // Animated: node_1 turns 90 degrees about z. v1 rides joint B, which sits
    // one unit along node_1's x axis — half a metre after the scale.
    const std::vector<vec3> t1 = ours_skin(rig, 0, 0, 1.0f);
    const std::vector<vec3> t05 = ours_skin(rig, 0, 0, 0.5f);
    const std::vector<vec3> t025 = ours_skin(rig, 0, 0, 0.25f);
    const std::vector<vec3> ref025 = spec_skin(f.data, 0, 0, 0.25);
    std::printf("  t=1    v1 (%.5f, %.5f)   predicted (0, 1.5)\n",
                static_cast<double>(t1[1].x), static_cast<double>(t1[1].y));
    std::printf("  t=0.5  v1 (%.5f, %.5f)   predicted (0.35355, 1.35355)\n",
                static_cast<double>(t05[1].x), static_cast<double>(t05[1].y));
    std::printf("  t=0.25 v1 (%.5f, %.5f)   spec (%.5f, %.5f)\n",
                static_cast<double>(t025[1].x), static_cast<double>(t025[1].y),
                static_cast<double>(ref025[1].x), static_cast<double>(ref025[1].y));
    check("B.6 v1 lands at (0, 1.5) at t = 1",
          engine::length(t1[1] - vec3{0.0f, 1.5f, 0.0f}) < 1e-5f);
    check("B.7 and at 45 degrees at t = 0.5",
          engine::length(t05[1] - vec3{0.35355339f, 1.35355339f, 0.0f}) < 1e-5f);
    std::printf("  keys: rotation track of node_1 has %zu (file 2)\n",
                rig.clips[0].tracks[1].rotation.size());
    check("B.8 at t = 0.25 we match the spec's slerp to 0.05 deg",
          engine::length(t025[1] - ref025[1]) < 0.5f * rad(0.05f));
}

// ---- Section C: joints listed child-first ----------------------------------------

std::string chain_file(bool with_ibm)
{
    // Four joints one unit apart up +y, written TIP FIRST: node 0 is the tip,
    // node 3 the root, and skins.joints lists them in node order.
    mini_gltf g;
    const std::vector<float> pos = {0.0f, 0.0f, 0.0f, 0.1f, 0.0f, 0.0f, 0.0f, 0.1f, 0.0f};
    const std::vector<unsigned char> joints = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    const std::vector<float> weights = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
    const int a_pos = g.floats(pos, "VEC3", 3, ",\"min\":[0,0,0],\"max\":[0.1,0.1,0]");
    const int a_j = g.ubytes(joints, "VEC4", 4);
    const int a_w = g.floats(weights, "VEC4", 4);
    std::string ibm;
    if (with_ibm)
    {
        std::vector<float> m;
        for (int j = 0; j < 4; ++j)
        {
            const float y = -static_cast<float>(3 - j);   // node j sits at height 3 - j
            const float one[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, y, 0, 1};
            m.insert(m.end(), one, one + 16);
        }
        ibm = ",\"inverseBindMatrices\":" + std::to_string(g.floats(m, "MAT4", 16));
    }
    const float h = std::sqrt(0.5f);
    const int a_t = g.times({0.0f, 1.0f});
    const int a_q = g.floats({0, 0, 0, 1, 0, 0, h, h}, "VEC4", 4);
    char body[1400];
    std::snprintf(body, sizeof body,
        "\"scene\":0,\"scenes\":[{\"nodes\":[3,4]}],\"nodes\":["
        "{\"name\":\"tip\",\"translation\":[0,1,0]},"
        "{\"name\":\"upper\",\"children\":[0],\"translation\":[0,1,0]},"
        "{\"name\":\"lower\",\"children\":[1],\"translation\":[0,1,0]},"
        "{\"name\":\"root\",\"children\":[2]},"
        "{\"name\":\"mesh\",\"mesh\":0,\"skin\":0}],"
        "\"skins\":[{\"joints\":[0,1,2,3]%s}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":%d,"
        "\"JOINTS_0\":%d,\"WEIGHTS_0\":%d}}]}],"
        "\"animations\":[{\"samplers\":[{\"input\":%d,\"output\":%d}],"
        "\"channels\":[{\"sampler\":0,\"target\":{\"node\":2,\"path\":\"rotation\"}}]}]",
        ibm.c_str(), a_pos, a_j, a_w, a_t, a_q);
    return g.json(body);
}

void section_c()
{
    section("C  joints listed child-first, and no inverse binds");
    const loaded f = parse_text(chain_file(false));
    imported_rig rig;
    const rig_import_report r = import_rig(f.data, 0, import_settings{}, rig);
    std::printf("  file-order breaks %zu, resorted %zu, identity IBMs %s\n",
                r.file_order_breaks, r.resorted, r.identity_inverse_binds ? "yes" : "no");
    std::printf("  our order: ");
    for (const joint& j : rig.sk.joints) { std::printf("%s ", j.name.c_str()); }
    std::printf("\n");
    check("C.1 three joints come before their parents in the file",
          r.file_order_breaks == 3 && r.resorted == 4);
    check("C.2 the sort puts root, lower, upper, tip", r.skeleton.out_of_order == 0
          && rig.sk.joints[0].name == "root" && rig.sk.joints[3].name == "tip");

    // No inverse binds: the spec's identity. A vertex at the origin of the tip's
    // space is then at the tip, three units up — NOT at the origin, which is where
    // baking inverse binds from the rest pose would have put it.
    const std::vector<vec3> rest = ours_skin(rig, 0, -1, 0.0f);
    std::printf("  identity IBMs: v0 at y = %.3f (the tip is at 3)\n",
                static_cast<double>(rest[0].y));
    check("C.3 absent IBMs are the identity, not baked", std::fabs(rest[0].y - 3.0f) < 1e-6f);
    imported_rig baked = rig;
    bake_inverse_binds(baked.sk);
    const std::vector<vec3> b = ours_skin(baked, 0, -1, 0.0f);
    std::printf("  control, IBMs baked from rest: v0 at y = %.3f\n",
                static_cast<double>(b[0].y));
    check("C.4 control: baking moves it to the origin", std::fabs(b[0].y) < 1e-6f);

    // WHAT THE SORT PREVENTS. Build the skeleton in the FILE's order, parents
    // pointing forward, and compose it — twice, so that a "last frame's value"
    // would have something to be.
    skeleton file_order;
    file_order.joints.resize(4);
    for (int j = 0; j < 4; ++j)
    {
        const engine::gltf_node_desc& nd = f.data.nodes[static_cast<std::size_t>(j)];
        file_order.joints[static_cast<std::size_t>(j)].name = nd.name;
        file_order.joints[static_cast<std::size_t>(j)].local_bind = nd.local;
        file_order.joints[static_cast<std::size_t>(j)].parent =
            nd.parent < 0 ? k_no_parent : static_cast<joint_index>(nd.parent);
    }
    const skeleton_report fr = validate([&] { skeleton s = file_order; bake_inverse_binds(s); return s; }());
    std::vector<transform> pose;
    rest_pose(file_order, pose);
    std::vector<mat4> posed;
    compose_pose(file_order, pose, posed);
    const float first = engine::translation_of(posed[0]).y;
    pose[2].rotation = engine::quat_z(rad(90.0f));   // 'lower' turns
    compose_pose(file_order, pose, posed);
    const float second = engine::translation_of(posed[0]).y;
    std::printf("  control, file order: out_of_order %zu; tip y %.3f,"
                " then %.3f\n", fr.out_of_order, static_cast<double>(first),
                static_cast<double>(second));
    check("C.5 unsorted, the tip is a root at y = 1 on BOTH frames",
          fr.out_of_order == 3 && std::fabs(first - 1.0f) < 1e-6f
          && std::fabs(second - 1.0f) < 1e-6f);
}

// ---- Section D: influences ----------------------------------------------------------

void section_d()
{
    section("D  influences: counts, sums, and a vertex with six");
    const loaded f = load("assets/mannequin.glb");
    imported_rig rig;
    const rig_import_report r = import_rig(f.data, 0, import_settings{}, rig);
    const std::size_t n = r.vertices;
    std::printf("  %zu vertices kept 1/2/3/4 influences: %zu %zu %zu %zu\n", n,
                r.influences[1], r.influences[2], r.influences[3], r.influences[4]);
    std::printf("  as percentages: %.1f %.1f %.1f %.1f\n",
                100.0 * static_cast<double>(r.influences[1]) / static_cast<double>(n),
                100.0 * static_cast<double>(r.influences[2]) / static_cast<double>(n),
                100.0 * static_cast<double>(r.influences[3]) / static_cast<double>(n),
                100.0 * static_cast<double>(r.influences[4]) / static_cast<double>(n));
    std::printf("  worst weight sum error in the file %.2e\n",
                static_cast<double>(r.worst_weight_sum));
    check("D.1 Blender's weights sum to 1 within 2e-7 x 4",
          r.worst_weight_sum < 8e-7f && r.truncated == 0);

    // Six influences in two sets: .3 .2 .15 .1 | .15 .1
    mini_gltf g;
    // One triangle, every corner with the same six influences.
    const int a_pos = g.floats({0, 0, 0, 1, 0, 0, 0, 1, 0}, "VEC3", 3,
                               ",\"min\":[0,0,0],\"max\":[1,1,0]");
    const int a_j0 = g.ubytes({0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3}, "VEC4", 4);
    const int a_w0 = g.floats({0.3f, 0.2f, 0.15f, 0.1f, 0.3f, 0.2f, 0.15f, 0.1f,
                               0.3f, 0.2f, 0.15f, 0.1f}, "VEC4", 4);
    const int a_j1 = g.ubytes({4, 5, 0, 0, 4, 5, 0, 0, 4, 5, 0, 0}, "VEC4", 4);
    const int a_w1 = g.floats({0.15f, 0.1f, 0.0f, 0.0f, 0.15f, 0.1f, 0.0f, 0.0f,
                               0.15f, 0.1f, 0.0f, 0.0f}, "VEC4", 4);
    char body[900];
    std::snprintf(body, sizeof body,
        "\"scenes\":[{\"nodes\":[0,1,2,3,4,5,6]}],\"nodes\":["
        "{\"name\":\"j0\"},{\"name\":\"j1\"},{\"name\":\"j2\"},{\"name\":\"j3\"},"
        "{\"name\":\"j4\"},{\"name\":\"j5\"},{\"mesh\":0,\"skin\":0}],"
        "\"skins\":[{\"joints\":[0,1,2,3,4,5]}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":%d,"
        "\"JOINTS_0\":%d,\"WEIGHTS_0\":%d,\"JOINTS_1\":%d,\"WEIGHTS_1\":%d}}]}]",
        a_pos, a_j0, a_w0, a_j1, a_w1);
    const loaded six = parse_text(g.json(body));
    imported_rig r6;
    const rig_import_report i6 = import_rig(six.data, 0, import_settings{}, r6);
    const skin_influence& inf = r6.meshes[0].influences[0];
    std::printf("  six influences: sets %d, truncated %zu, worst dropped %.2f\n",
                six.report.max_influence_sets, i6.truncated,
                static_cast<double>(i6.worst_dropped));
    std::printf("  kept joints %s %s %s %s, weights %.4f %.4f %.4f %.4f\n",
                r6.sk.joints[inf.joints[0]].name.c_str(), r6.sk.joints[inf.joints[1]].name.c_str(),
                r6.sk.joints[inf.joints[2]].name.c_str(), r6.sk.joints[inf.joints[3]].name.c_str(),
                static_cast<double>(inf.weights[0]), static_cast<double>(inf.weights[1]),
                static_cast<double>(inf.weights[2]), static_cast<double>(inf.weights[3]));
    check("D.2 two sets read; the four largest kept, j2 before j4",
          six.report.max_influence_sets == 2 && i6.truncated == 3
          && r6.sk.joints[inf.joints[2]].name == "j2" && r6.sk.joints[inf.joints[3]].name == "j4");
    check("D.3 what was dropped is reported, and the rest sum to 1",
          std::fabs(i6.worst_dropped - 0.1f) < 1e-6f
          && std::fabs(inf.weights[0] + inf.weights[1] + inf.weights[2] + inf.weights[3] - 1.0f) < 1e-6f);
}

// ---- Section E: the file's curves, and ours ----------------------------------------

/// A file of three free joints, each with ONE rotation channel of two keys an
/// `arc` apart — about three different axes, so no special case can hide.
std::string arcs_file(const float* arcs_deg, int n)
{
    mini_gltf g;
    const int a_pos = g.floats({0.0f, 0.0f, 0.0f}, "VEC3", 3, ",\"min\":[0,0,0],\"max\":[0,0,0]");
    const int a_j = g.ubytes({0, 0, 0, 0}, "VEC4", 4);
    const int a_w = g.floats({1, 0, 0, 0}, "VEC4", 4);
    const int a_t = g.times({0.0f, 1.0f});
    std::string nodes, joints, samplers, channels;
    const vec3 axes[3] = {engine::normalised(vec3{1, 0, 0}), engine::normalised(vec3{0, 1, 1}),
                          engine::normalised(vec3{1, 2, 3})};
    for (int i = 0; i < n; ++i)
    {
        const quat q = engine::quat_from_axis_angle(axes[i % 3], rad(arcs_deg[i]));
        const int a_q = g.floats({0, 0, 0, 1, q.v.x, q.v.y, q.v.z, q.w}, "VEC4", 4);
        nodes += "{\"name\":\"arc" + std::to_string(i) + "\"},";
        joints += (i ? "," : "") + std::to_string(i);
        samplers += std::string(i ? "," : "") + "{\"input\":" + std::to_string(a_t)
                  + ",\"output\":" + std::to_string(a_q) + "}";
        channels += std::string(i ? "," : "") + "{\"sampler\":" + std::to_string(i)
                  + ",\"target\":{\"node\":" + std::to_string(i) + ",\"path\":\"rotation\"}}";
    }
    std::string scene_nodes;
    for (int i = 0; i <= n; ++i) { scene_nodes += (i ? "," : "") + std::to_string(i); }
    const std::string body =
        "\"scenes\":[{\"nodes\":[" + scene_nodes + "]}],\"nodes\":[" + nodes
        + "{\"mesh\":0,\"skin\":0}],\"skins\":[{\"joints\":[" + joints + "]}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":" + std::to_string(a_pos)
        + ",\"JOINTS_0\":" + std::to_string(a_j) + ",\"WEIGHTS_0\":" + std::to_string(a_w)
        + "}}]}],\"animations\":[{\"samplers\":[" + samplers + "],\"channels\":["
        + channels + "]}]";
    return g.json(body);
}

/// The largest angle between nlerp and slerp over one arc, and where it is.
void nlerp_lag(float arc, float& worst, float& where, float& at_mid)
{
    const quat a = quat::identity();
    const quat b = engine::quat_from_axis_angle({0, 0, 1}, arc);
    worst = 0.0f; where = 0.0f;
    for (int i = 0; i <= 1000; ++i)
    {
        const float u = static_cast<float>(i) / 1000.0f;
        const float e = engine::angle_between(engine::quat_nlerp(a, b, u), engine::quat_slerp(a, b, u));
        if (e > worst) { worst = e; where = u; }
    }
    at_mid = engine::angle_between(engine::quat_nlerp(a, b, 0.5f), engine::quat_slerp(a, b, 0.5f));
}

void section_e()
{
    section("E  the file's curves become ours, to a tolerance");

    // E.1 LINEAR rotations: the file says slerp, we play nlerp.
    std::printf("  arc   nlerp lag (at u)    at u=.5   pieces  residual\n");
    const float arcs[] = {15.5f, 30.0f, 60.0f, 90.0f, 120.0f, 150.0f, 179.0f};
    const int n_arcs = static_cast<int>(sizeof(arcs) / sizeof(arcs[0]));
    const loaded f = parse_text(arcs_file(arcs, n_arcs));
    imported_rig rig;
    const rig_import_report r = import_rig(f.data, 0, import_settings{}, rig);
    bool mid_blind = true;
    bool fitted = true;
    for (int i = 0; i < n_arcs; ++i)
    {
        float worst = 0, where = 0, mid = 0;
        nlerp_lag(rad(arcs[i]), worst, where, mid);
        const std::size_t keys = rig.clips[0].tracks[static_cast<std::size_t>(i)].rotation.size();
        std::printf("  %5.1f %8.4f deg (%.3f)  %.1e  %4zu\n", static_cast<double>(arcs[i]),
                    static_cast<double>(deg(worst)), static_cast<double>(where),
                    static_cast<double>(deg(mid)), keys - 1);
        mid_blind = mid_blind && deg(mid) < 1e-3f;
        fitted = fitted && (keys >= 2);
    }
    std::printf("  split keys %zu, worst residual %.4f deg (limit 0.05)\n", r.split_keys,
                static_cast<double>(deg(r.worst_rotation)));
    check("E.1 every arc is fitted within 0.05 degrees",
          fitted && deg(r.worst_rotation) <= 0.05f && r.capped == 0);
    check("E.2 control: at the midpoint nlerp and slerp agree", mid_blind);

    // E.2 STEP.
    {
        mini_gltf g;
        const int a_pos = g.floats({0.0f, 0.0f, 0.0f}, "VEC3", 3, ",\"min\":[0,0,0],\"max\":[0,0,0]");
        const int a_j = g.ubytes({0, 0, 0, 0}, "VEC4", 4);
        const int a_w = g.floats({1, 0, 0, 0}, "VEC4", 4);
        const int a_t = g.times({0.0f, 0.5f, 1.0f});
        const int a_v = g.floats({0, 0, 0, 1, 0, 0, 2, 0, 0}, "VEC3", 3);
        char body[700];
        std::snprintf(body, sizeof body,
            "\"scenes\":[{\"nodes\":[0,1]}],\"nodes\":[{\"name\":\"j\"},{\"mesh\":0,\"skin\":0}],"
            "\"skins\":[{\"joints\":[0]}],\"meshes\":[{\"primitives\":[{\"attributes\":"
            "{\"POSITION\":%d,\"JOINTS_0\":%d,\"WEIGHTS_0\":%d}}]}],\"animations\":[{"
            "\"samplers\":[{\"input\":%d,\"output\":%d,\"interpolation\":\"STEP\"}],"
            "\"channels\":[{\"sampler\":0,\"target\":{\"node\":0,\"path\":\"translation\"}}]}]",
            a_pos, a_j, a_w, a_t, a_v);
        const loaded s = parse_text(g.json(body));
        imported_rig sr;
        const rig_import_report si = import_rig(s.data, 0, import_settings{}, sr);
        std::vector<transform> pose;
        const float just_before = std::nextafter(0.5f, 0.0f);
        sample_at(sr.clips[0], sr.sk, 0.25f, pose);
        const float at_quarter = pose[0].position.x;
        sample_at(sr.clips[0], sr.sk, just_before, pose);
        const float before = pose[0].position.x;
        sample_at(sr.clips[0], sr.sk, 0.5f, pose);
        const float at = pose[0].position.x;
        std::printf("  STEP 0,1,2 at 0, .5, 1 s: keys %zu (%zu of them held)\n",
                    sr.clips[0].tracks[0].position.size(), si.step_keys);
        std::printf("  x(.25) %.3f   x(.5 - 1 ulp) %.3f   x(.5) %.3f\n",
                    static_cast<double>(at_quarter), static_cast<double>(before),
                    static_cast<double>(at));
        check("E.3 a STEP holds to the float before its next key",
              at_quarter == 0.0f && before == 0.0f && at == 1.0f && si.step_keys == 2);
        // CONTROL: the same keys as LINEAR, which is what copying them does.
        clip plain = sr.clips[0];
        plain.tracks[0].position = {{0.0f, {0, 0, 0}}, {0.5f, {1, 0, 0}}, {1.0f, {2, 0, 0}}};
        sample_at(plain, sr.sk, 0.25f, pose);
        std::printf("  control, keys copied as-is: x(.25) %.3f\n",
                    static_cast<double>(pose[0].position.x));
        check("E.4 control: copied, a STEP becomes a ramp", pose[0].position.x == 0.5f);
    }

    // E.3 CUBICSPLINE, from Blender.
    {
        const loaded c = load("scratch/mannequin_curves.glb");
        imported_rig cr;
        const rig_import_report ci = import_rig(c.data, 0, import_settings{}, cr);
        std::printf("  Blender, sampling off: %zu cubic channels, %zu keys in\n", ci.cubic,
                    ci.keys_in);
        std::printf("  -> %zu keys out (%zu split), worst %.4f deg / %.2e m\n", ci.keys_out,
                    ci.split_keys, static_cast<double>(deg(ci.worst_rotation)),
                    static_cast<double>(ci.worst_position));
        check("E.5 Blender's CUBICSPLINE fitted within tolerance",
              ci.cubic > 0 && ci.capped == 0 && deg(ci.worst_rotation) <= 0.05f
              && ci.worst_position <= 1e-4f);

        // CONTROL: the tangents without t_d, which is the classic bug. How far
        // does the curve move?
        float worst_td = 0.0f;
        for (const engine::gltf_animation_desc& a : c.data.animations)
        {
            for (const engine::gltf_channel_desc& ch : a.channels)
            {
                if (ch.path != engine::gltf_path::rotation) { continue; }
                engine::gltf_sampler_desc s = a.samplers[static_cast<std::size_t>(ch.sampler)];
                engine::gltf_sampler_desc bad = s;
                // Scaling every tangent by 1/t_d undoes the spec's t_d.
                for (std::size_t k = 0; k + 1 < s.times.size(); ++k)
                {
                    const float td = s.times[k + 1] - s.times[k];
                    for (int comp = 0; comp < 4; ++comp)
                    {
                        bad.values[(3 * k + 2) * 4 + static_cast<std::size_t>(comp)] /= td;
                        bad.values[(3 * (k + 1)) * 4 + static_cast<std::size_t>(comp)] /= td;
                    }
                }
                for (int i = 0; i <= 200; ++i)
                {
                    const double t = s.times.back() * i / 200.0;
                    double q1[4], q2[4];
                    spec_sample(s, true, t, q1);
                    spec_sample(bad, true, t, q2);
                    const quat a1 = engine::quat_from_xyzw(static_cast<float>(q1[0]), static_cast<float>(q1[1]),
                                                           static_cast<float>(q1[2]), static_cast<float>(q1[3]));
                    const quat a2 = engine::quat_from_xyzw(static_cast<float>(q2[0]), static_cast<float>(q2[1]),
                                                           static_cast<float>(q2[2]), static_cast<float>(q2[3]));
                    worst_td = std::max(worst_td, engine::angle_between(a1, a2));
                }
            }
        }
        std::printf("  control, tangents without t_d: curve off by %.2f deg\n",
                    static_cast<double>(deg(worst_td)));
        check("E.6 control: forgetting t_d moves the curve by degrees", deg(worst_td) > 1.0f);

        // For figure: one channel, the walk's left thigh — the file's curve, the
        // same curve with t_d forgotten, and what the imported clip plays — as the
        // angle from the channel's first key.
        {
            const engine::gltf_animation_desc& a = c.data.animations[0];
            int node = -1;
            for (std::size_t i = 0; i < c.data.nodes.size(); ++i)
            {
                if (c.data.nodes[i].name == "thigh.L") { node = static_cast<int>(i); }
            }
            for (const engine::gltf_channel_desc& ch : a.channels)
            {
                if (ch.node != node || ch.path != engine::gltf_path::rotation) { continue; }
                const engine::gltf_sampler_desc& sd = a.samplers[static_cast<std::size_t>(ch.sampler)];
                engine::gltf_sampler_desc bad = sd;
                for (std::size_t k = 0; k + 1 < sd.times.size(); ++k)
                {
                    const float td = sd.times[k + 1] - sd.times[k];
                    for (int comp = 0; comp < 4; ++comp)
                    {
                        bad.values[(3 * k + 2) * 4 + static_cast<std::size_t>(comp)] /= td;
                        bad.values[(3 * (k + 1)) * 4 + static_cast<std::size_t>(comp)] /= td;
                    }
                }
                std::size_t j = 0;
                for (std::size_t q = 0; q < cr.joint_node.size(); ++q) { if (cr.joint_node[q] == node) { j = q; } }
                double q0[4];
                spec_sample(sd, true, 0.0, q0);
                const quat first = engine::quat_from_xyzw(static_cast<float>(q0[0]), static_cast<float>(q0[1]),
                                                          static_cast<float>(q0[2]), static_cast<float>(q0[3]));
                FILE* out = std::fopen("scratch/l77b_e_cubic.csv", "w");
                std::fprintf(out, "t,spec,no_td,ours\n");
                std::vector<transform> pose;
                for (int i = 0; i <= 300; ++i)
                {
                    const double t = static_cast<double>(i) / 300.0;
                    double g[4], b[4];
                    spec_sample(sd, true, t, g);
                    spec_sample(bad, true, t, b);
                    const quat qg = engine::quat_from_xyzw(static_cast<float>(g[0]), static_cast<float>(g[1]),
                                                           static_cast<float>(g[2]), static_cast<float>(g[3]));
                    const quat qb = engine::quat_from_xyzw(static_cast<float>(b[0]), static_cast<float>(b[1]),
                                                           static_cast<float>(b[2]), static_cast<float>(b[3]));
                    sample_at(cr.clips[0], cr.sk, static_cast<float>(t), pose);
                    std::fprintf(out, "%.5f,%.5f,%.5f,%.5f\n", t,
                                 static_cast<double>(deg(engine::angle_between(first, qg))),
                                 static_cast<double>(deg(engine::angle_between(first, qb))),
                                 static_cast<double>(deg(engine::angle_between(first, pose[j].rotation))));
                }
                std::fclose(out);
                FILE* keys = std::fopen("scratch/l77b_e_cubic_keys.csv", "w");
                std::fprintf(keys, "kind,t\n");
                for (float t : sd.times) { std::fprintf(keys, "file,%.6f\n", static_cast<double>(t)); }
                for (const quat_key& k : cr.clips[0].tracks[j].rotation)
                {
                    std::fprintf(keys, "ours,%.6f\n", static_cast<double>(k.time));
                }
                std::fclose(keys);
                std::printf("  thigh.L: %zu file keys -> %zu imported (figure data)\n",
                            sd.times.size(), cr.clips[0].tracks[j].rotation.size());
            }
        }

        // E.4: two exports of one motion.
        const loaded s = load("assets/mannequin.glb");
        imported_rig sr;
        (void)import_rig(s.data, 0, import_settings{}, sr);
        float worst_angle = 0.0f;
        float worst_frame = 0.0f;
        std::vector<transform> pa, pb;
        FILE* two = std::fopen("scratch/l77b_e_two.csv", "w");
        std::fprintf(two, "t,worst_deg\n");
        for (int i = 0; i <= 240; ++i)
        {
            const float t = static_cast<float>(i) / 240.0f;
            sample_at(sr.clips[0], sr.sk, t, pa);
            sample_at(cr.clips[0], cr.sk, t, pb);
            float here = 0.0f;
            for (std::size_t j = 0; j < pa.size(); ++j)
            {
                const float e = engine::angle_between(pa[j].rotation, pb[j].rotation);
                here = std::max(here, e);
                worst_angle = std::max(worst_angle, e);
                if (i % 8 == 0) { worst_frame = std::max(worst_frame, e); }
            }
            std::fprintf(two, "%.5f,%.5f\n", static_cast<double>(t), static_cast<double>(deg(here)));
        }
        std::fclose(two);
        std::printf("  walk, sampled vs curves: worst joint %.3f deg"
                    " (on frames %.3f)\n", static_cast<double>(deg(worst_angle)),
                    static_cast<double>(deg(worst_frame)));

        // Whose disagreement is that? The same comparison with NO importer: the
        // curves file evaluated by the spec's formula at every frame, against the
        // sampled file's key at that frame — which is Blender's own evaluation
        // of its own curve.
        float exporter = 0.0f;
        const engine::gltf_animation_desc& sa = s.data.animations[0];
        const engine::gltf_animation_desc& ca = c.data.animations[0];
        for (const engine::gltf_channel_desc& sch : sa.channels)
        {
            if (sch.path != engine::gltf_path::rotation) { continue; }
            for (const engine::gltf_channel_desc& cch : ca.channels)
            {
                if (cch.node != sch.node || cch.path != sch.path) { continue; }
                const engine::gltf_sampler_desc& ss = sa.samplers[static_cast<std::size_t>(sch.sampler)];
                const engine::gltf_sampler_desc& cs = ca.samplers[static_cast<std::size_t>(cch.sampler)];
                for (std::size_t k = 0; k < ss.times.size(); ++k)
                {
                    double q[4];
                    spec_sample(cs, true, ss.times[k], q);
                    const float* v = &ss.values[k * 4];
                    const quat a1 = engine::quat_from_xyzw(v[0], v[1], v[2], v[3]);
                    const quat a2 = engine::quat_from_xyzw(static_cast<float>(q[0]), static_cast<float>(q[1]),
                                                           static_cast<float>(q[2]), static_cast<float>(q[3]));
                    exporter = std::max(exporter, engine::angle_between(engine::normalised(a1), a2));
                }
            }
        }
        std::printf("  the same, from the files alone (no importer): %.3f deg\n",
                    static_cast<double>(deg(exporter)));
        check("E.7 the two exports differ, and by the exporter's amount",
              deg(worst_frame) > 0.05f && std::fabs(deg(worst_frame) - deg(exporter)) < 0.05f);
    }
}

// ---- Section F: the blind copy ------------------------------------------------------

void section_f()
{
    section("F  xyzw read as wxyz: what a blind copy costs");
    const loaded f = load("assets/mannequin.glb");
    std::vector<float> errors;
    for (const engine::gltf_animation_desc& a : f.data.animations)
    {
        for (const engine::gltf_channel_desc& ch : a.channels)
        {
            if (ch.path != engine::gltf_path::rotation) { continue; }
            const engine::gltf_sampler_desc& s = a.samplers[static_cast<std::size_t>(ch.sampler)];
            for (std::size_t k = 0; k < s.times.size(); ++k)
            {
                const float* v = &s.values[k * 4];
                const quat right = engine::quat_from_xyzw(v[0], v[1], v[2], v[3]);
                const quat blind{v[0], vec3{v[1], v[2], v[3]}};   // memcpy into {w, x, y, z}
                errors.push_back(engine::angle_between(right, engine::normalised(blind)));
            }
        }
    }
    std::sort(errors.begin(), errors.end());
    FILE* fx = std::fopen("scratch/l77b_f_xyzw.csv", "w");
    std::fprintf(fx, "deg\n");
    for (float e : errors) { std::fprintf(fx, "%.4f\n", static_cast<double>(deg(e))); }
    std::fclose(fx);
    std::printf("  %zu rotation keys: error min %.1f, median %.1f, max %.1f deg\n",
                errors.size(), static_cast<double>(deg(errors.front())),
                static_cast<double>(deg(errors[errors.size() / 2])),
                static_cast<double>(deg(errors.back())));
    const quat id_blind{0.0f, vec3{0.0f, 0.0f, 1.0f}};
    const engine::axis_angle_extraction e = engine::axis_angle_from_quat(id_blind);
    std::printf("  identity -> %.1f deg about (%.0f, %.0f, %.0f)\n",
                static_cast<double>(deg(e.value.angle)), static_cast<double>(e.value.axis.x),
                static_cast<double>(e.value.axis.y), static_cast<double>(e.value.axis.z));
    const quat half{0.5f, vec3{0.5f, 0.5f, 0.5f}};
    const quat half_blind{half.v.x, vec3{half.v.y, half.v.z, half.w}};
    std::printf("  (1/2,1/2,1/2,1/2) -> error %.1f deg\n",
                static_cast<double>(deg(engine::angle_between(half, half_blind))));
    check("F.1 never small, and not one fixed error either",
          deg(errors.front()) > 90.0f && deg(errors.back()) - deg(errors.front()) > 30.0f);
    check("F.2 the identity misreads as 180 about z, not a diagonal",
          std::fabs(deg(e.value.angle) - 180.0f) < 1e-3f && std::fabs(e.value.axis.z - 1.0f) < 1e-6f);
}

// ---- Section G: end to end, against the spec ----------------------------------------

void section_g()
{
    section("G  Blender's character, end to end, against the spec");
    const loaded f = load("assets/mannequin.glb");
    imported_rig rig;
    (void)import_rig(f.data, 0, import_settings{}, rig);
    float worst = 0.0f;
    float worst_no_ibm = 0.0f;
    for (int a = 0; a < 2; ++a)
    {
        const float dur = rig.clips[static_cast<std::size_t>(a)].duration;
        // INSIDE the clip. Past the last key the spec CLAMPS and a looping clip
        // WRAPS: the first draft of this section sampled at t = 1.013 s and
        // reported an 11.7 mm "error" that was the walk's motion over 13 ms.
        for (int i = 0; i < 12; ++i)
        {
            const float t = dur * static_cast<float>(i) / 12.0f + 0.013f;   // off the keys
            for (std::size_t m = 0; m < rig.meshes.size(); ++m)
            {
                const std::vector<vec3> ours = ours_skin(rig, m, a, t);
                const std::vector<vec3> spec = spec_skin(f.data, static_cast<int>(m), a, t);
                worst = std::max(worst, worst_gap(ours, spec));
                if (i == 3)
                {
                    worst_no_ibm = std::max(worst_no_ibm, worst_gap(ours_skin(rig, m, a, t, false), spec));
                }
            }
        }
    }
    std::printf("  24 poses x 2 meshes: worst vertex %.2f um from the spec\n",
                static_cast<double>(worst * 1e6f));
    std::printf("  control, inverse binds skipped: worst %.3f m\n",
                static_cast<double>(worst_no_ibm));
    check("G.1 every vertex within 0.1 mm of the spec's formula", worst < 1e-4f);
    check("G.2 control: without inverse binds, metres off", worst_no_ibm > 0.5f);
}

// ---- Section H: the joint Blender invented ------------------------------------------

void section_h()
{
    section("H  the first export: vertices on a joint nothing moves");
    const loaded u = load("scratch/mannequin_unfixed.glb");
    imported_rig ur;
    const rig_import_report ui = import_rig(u.data, 0, import_settings{}, ur);
    const std::string last = ur.sk.joints.back().name;
    std::size_t on_last = 0;
    for (const skin_influence& inf : ur.meshes[1].influences)
    {
        if (inf.weights[0] > 0.0f && ur.sk.joints[inf.joints[0]].name == "neutral_bone") { ++on_last; }
    }
    for (const skin_influence& inf : ur.meshes[0].influences)
    {
        if (inf.weights[0] > 0.0f && ur.sk.joints[inf.joints[0]].name == "neutral_bone") { ++on_last; }
    }
    std::printf("  unfixed: %zu skin joints, last '%s'; frozen vertices %zu\n",
                ui.skin_joints, last.c_str(), ui.frozen_vertices);
    std::printf("  vertices fully on neutral_bone: %zu\n", on_last);
    const loaded fx = load("assets/mannequin.glb");
    imported_rig fr;
    const rig_import_report fi = import_rig(fx.data, 0, import_settings{}, fr);
    std::printf("  fixed:   %zu skin joints; frozen vertices %zu\n", fi.skin_joints,
                fi.frozen_vertices);
    check("H.1 the unfixed file freezes what bone heat missed",
          ui.skin_joints == 20 && last == "neutral_bone" && ui.frozen_vertices == on_last
          && on_last > 0);
    check("H.2 the fixed file freezes none", fi.frozen_vertices == 0 && fi.skin_joints == 19);

    // How far the nose tip is left behind when the head turns in the wave.
    float worst = 0.0f;
    for (int i = 0; i <= 60; ++i)
    {
        const float t = static_cast<float>(i) / 30.0f;
        const std::vector<vec3> a = ours_skin(ur, 1, 1, t);
        const std::vector<vec3> b = ours_skin(fr, 1, 1, t);
        worst = std::max(worst, worst_gap(a, b));
    }
    std::printf("  wave: the unfixed face plate lags the fixed by up to %.1f mm\n",
                static_cast<double>(worst * 1000.0f));

    // Figure data: at the worst instant, every head vertex seen from above, in
    // both files, with the frozen ones marked.
    float worst_t = 0.0f;
    float best = 0.0f;
    for (int i = 0; i <= 60; ++i)
    {
        const float t = static_cast<float>(i) / 30.0f;
        const float g = worst_gap(ours_skin(ur, 1, 1, t), ours_skin(fr, 1, 1, t));
        if (g > best) { best = g; worst_t = t; }
    }
    FILE* nose = std::fopen("scratch/l77b_h_nose.csv", "w");
    std::fprintf(nose, "mesh,x_fixed,z_fixed,x_unfixed,z_unfixed,y,frozen\n");
    for (std::size_t m = 0; m < 2; ++m)
    {
        const std::vector<vec3> a = ours_skin(fr, m, 1, worst_t);
        const std::vector<vec3> b = ours_skin(ur, m, 1, worst_t);
        for (std::size_t v = 0; v < a.size(); ++v)
        {
            if (ur.meshes[m].bind.vertices[v].y < 1.55f) { continue; }
            const skin_influence& inf = ur.meshes[m].influences[v];
            const bool frozen = ur.sk.joints[inf.joints[0]].name == "neutral_bone";
            std::fprintf(nose, "%zu,%.5f,%.5f,%.5f,%.5f,%.5f,%d\n", m,
                         static_cast<double>(a[v].x), static_cast<double>(a[v].z),
                         static_cast<double>(b[v].x), static_cast<double>(b[v].z),
                         static_cast<double>(a[v].y), frozen ? 1 : 0);
        }
    }
    std::fclose(nose);
    std::printf("  worst at t = %.3f s (figure data)\n", static_cast<double>(worst_t));
}

// ---- Section I: two checks that fired on a healthy rig ----------------------------

void section_i()
{
    section("I  two tolerances, against a real exporter and the spec");
    const loaded f = load("assets/mannequin.glb");
    imported_rig rig;
    const rig_import_report r = import_rig(f.data, 0, import_settings{}, rig);
    float spread = 0.0f;
    for (const joint& j : rig.sk.joints)
    {
        const vec3 s = j.local_bind.scale;
        const float hi = std::max({std::fabs(s.x), std::fabs(s.y), std::fabs(s.z)});
        const float lo = std::min({std::fabs(s.x), std::fabs(s.y), std::fabs(s.z)});
        spread = std::max(spread, hi / lo - 1.0f);
    }
    std::printf("  Blender's rest scales: worst relative spread %.2e\n",
                static_cast<double>(spread));
    std::printf("  nonuniform_binds reports %zu of %zu joints\n", r.skeleton.nonuniform_binds,
                r.joints);
    check("I.1 float noise in a rest scale is not a non-uniform bind",
          r.skeleton.nonuniform_binds == 0);
    skeleton stretched = rig.sk;
    stretched.joints[5].local_bind.scale = {1.5f, 1.0f, 1.0f};
    bake_inverse_binds(stretched);
    check("I.2 control: a 1.5:1 stretch is still counted",
          validate(stretched).nonuniform_binds == 1);

    // What a spread of that size does to a normal, measured: the angle between
    // the linear part and the inverse transpose applied to a 45-degree normal.
    auto normal_error = [](float eps) {
        const vec3 n = engine::normalised(vec3{1.0f, 1.0f, 0.0f});
        const vec3 lin = engine::normalised(vec3{n.x * (1.0f + eps), n.y, 0.0f});
        const vec3 inv = engine::normalised(vec3{n.x / (1.0f + eps), n.y, 0.0f});
        return std::atan2(engine::length(engine::cross(lin, inv)), engine::dot(lin, inv));
    };
    std::printf("  normal error: %.2e deg at Blender's spread, %.2e at 1e-4\n",
                static_cast<double>(deg(normal_error(spread))),
                static_cast<double>(deg(normal_error(1e-4f))));

    // Weights. The spec: normalised unsigned bytes "MUST be 255" before
    // normalisation. A byte-weighted vertex one step short is off by 1/255.
    skinned_mesh m;
    m.bind.vertices = {vec3{0, 0, 0}, vec3{1, 0, 0}};
    m.influences.resize(2);
    m.influences[0].weights[0] = 255.0f / 255.0f;
    m.influences[1].joints[1] = 1;
    m.influences[1].weights[0] = 200.0f / 255.0f;
    m.influences[1].weights[1] = 54.0f / 255.0f;   // 254 of 255
    const skin_report sr = validate(m, 2);
    std::printf("  byte weights 255/255 and 254/255: unnormalised %zu\n", sr.unnormalised);
    check("I.3 a quantised sum one step short is reported", sr.unnormalised == 1);
}

// ---- Section J: which way the character faces -------------------------------------

void section_j()
{
    section("J  which way a glTF character faces");
    const loaded f = load("assets/mannequin.glb");
    imported_rig rig;
    (void)import_rig(f.data, 0, import_settings{}, rig);
    // The face plate is mesh 1; the head is the joint named "head".
    const skinned_mesh& plate = rig.meshes[1];
    vec3 tip = plate.bind.vertices[0];
    for (const vec3& v : plate.bind.vertices) { if (v.z > tip.z) { tip = v; } }
    std::vector<mat4> posed;
    std::vector<transform> pose;
    rest_pose(rig.sk, pose);
    compose_pose(rig.sk, pose, posed);
    std::size_t head = 0;
    for (std::size_t j = 0; j < rig.sk.size(); ++j) { if (rig.sk.joints[j].name == "head") { head = j; } }
    const vec3 h = engine::translation_of(posed[head]);
    std::printf("  head joint (%.3f, %.3f, %.3f), nose tip z = %.3f\n",
                static_cast<double>(h.x), static_cast<double>(h.y), static_cast<double>(h.z),
                static_cast<double>(tip.z));
    // The default camera: at +z, looking down -z (course conventions).
    const vec3 eye{0.0f, 1.2f, 3.0f};
    const float nose_depth = eye.z - tip.z;
    const float head_depth = eye.z - h.z;
    std::printf("  camera at z = 3 looking down -z: nose %.3f m away, head %.3f\n",
                static_cast<double>(nose_depth), static_cast<double>(head_depth));
    check("J.1 the nose points +z, toward a camera looking down -z",
          tip.z > h.z + 0.08f && nose_depth < head_depth);
}

// ---- Section K: 7.7's reducer on a real clip ------------------------------------

void section_k()
{
    section("K  7.7's reduce on Blender's sampled clips");
    const loaded f = load("assets/mannequin.glb");
    imported_rig rig;
    (void)import_rig(f.data, 0, import_settings{}, rig);
    for (clip& c : rig.clips)
    {
        const clip_report before = validate(c, rig.sk);
        clip reduced = c;
        const std::size_t removed = reduce(reduced, rig.sk, reduction_limits{});
        const clip_report after = validate(reduced, rig.sk);
        float worst = 0.0f;
        std::vector<transform> pa, pb;
        for (int i = 0; i <= 240; ++i)
        {
            const float t = c.duration * static_cast<float>(i) / 240.0f;
            sample_at(c, rig.sk, t, pa);
            sample_at(reduced, rig.sk, t, pb);
            for (std::size_t j = 0; j < pa.size(); ++j)
            {
                worst = std::max(worst, engine::angle_between(pa[j].rotation, pb[j].rotation));
            }
        }
        std::printf("  %-5s keys %4zu -> %3zu (-%zu), channels %2zu -> %2zu\n",
                    c.name.c_str(), before.keys, after.keys, removed, before.channels,
                    after.channels);
        std::printf("        %6zu B -> %5zu B, worst playback %.3f deg\n",
                    before.key_bytes, after.key_bytes, static_cast<double>(deg(worst)));
        check(c.name == "walk" ? "K.1 walk reduced within half a degree"
                               : "K.2 wave reduced within half a degree",
              deg(worst) <= 0.5f + 1e-3f && after.keys < before.keys);
    }
}

// ---- Section L: what it costs -----------------------------------------------------

void section_l()
{
    section("L  what importing and playing cost");
#ifdef ENGINE_LIB_BUILD_TYPE
    std::printf("  libengine.a built as %s\n", ENGINE_LIB_BUILD_TYPE);
#endif
    using clock = std::chrono::steady_clock;
    double best_load = 1e9, best_import = 1e9;
    for (int rep = 0; rep < 15; ++rep)
    {
        const auto t0 = clock::now();
        loaded f = load("assets/mannequin.glb");
        const auto t1 = clock::now();
        imported_rig rig;
        const rig_import_report r = import_rig(f.data, 0, import_settings{}, rig);
        const auto t2 = clock::now();
        sink += static_cast<double>(r.keys_out);
        best_load = std::min(best_load, std::chrono::duration<double, std::milli>(t1 - t0).count());
        best_import = std::min(best_import, std::chrono::duration<double, std::milli>(t2 - t1).count());
    }
    std::printf("  load_gltf %.3f ms, import_rig %.3f ms (min of 15)\n", best_load, best_import);

    const loaded f = load("assets/mannequin.glb");
    imported_rig rig;
    (void)import_rig(f.data, 0, import_settings{}, rig);
    std::vector<track_cursor> cursors;
    std::vector<transform> pose;
    std::vector<mat4> posed, palette;
    std::vector<engine::mesh_data> out(rig.meshes.size());
    double best_sample = 1e9, best_palette = 1e9, best_skin = 1e9;
    for (int rep = 0; rep < 15; ++rep)
    {
        constexpr int frames = 200;
        double ts = 0, tp = 0, tk = 0;
        for (int i = 0; i < frames; ++i)
        {
            const float t = static_cast<float>(i) / 60.0f + static_cast<float>(rep) * 0.001f;
            const auto a = clock::now();
            sample(rig.clips[0], rig.sk, t, cursors, pose);
            const auto b = clock::now();
            skinning_palette(rig.sk, pose, posed, palette);
            const auto c = clock::now();
            for (std::size_t m = 0; m < rig.meshes.size(); ++m) { skin_into(rig.meshes[m], palette, out[m]); }
            const auto d = clock::now();
            sink += static_cast<double>(out[0].vertices[static_cast<std::size_t>(i) % out[0].vertices.size()].y);
            ts += std::chrono::duration<double, std::micro>(b - a).count();
            tp += std::chrono::duration<double, std::micro>(c - b).count();
            tk += std::chrono::duration<double, std::micro>(d - c).count();
        }
        best_sample = std::min(best_sample, ts / frames);
        best_palette = std::min(best_palette, tp / frames);
        best_skin = std::min(best_skin, tk / frames);
    }
    const double verts = static_cast<double>(rig.meshes[0].vertex_count() + rig.meshes[1].vertex_count());
    std::printf("  per frame: sample %.2f us, palette %.2f us, skin %.1f us\n", best_sample,
                best_palette, best_skin);
    std::printf("  skin_into %.2f ns a vertex over %.0f vertices\n", best_skin * 1000.0 / verts, verts);
    std::printf("  (sink %.1f)\n", sink);
}
}   // namespace

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);   // a crash must not eat the transcript
    std::printf("verify_77b — Lesson 7.7b, animated characters from glTF\n");
    section_a();
    section_b();
    section_c();
    section_d();
    section_e();
    section_f();
    section_g();
    section_h();
    section_i();
    section_j();
    section_k();
    section_l();
    std::printf("\n%d of %d checks passed\n", checks_passed, checks_run);
    return checks_passed == checks_run ? 0 : 1;
}
