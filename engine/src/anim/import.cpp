// engine/src/anim/import.cpp — glTF descriptions into a skeleton, skins and clips.
//
// Lesson 7.7b. Read `engine/include/engine/anim/import.hpp` first: the four
// decisions this file implements are argued there. This file is the mechanics,
// in the order `import_rig` runs them — skeleton, then meshes, then clips —
// because each needs the one before: the meshes need the joint permutation, and
// the clips need both the permutation and the rest pose that fills an
// unanimated channel.
//
// Nothing here has seen cgltf. Everything arrives through `gfx/gltf.hpp`'s
// descriptions, so every function below can be driven from a hand-built
// `gltf_scene_data` in a test with no file at all.

#include <engine/anim/import.hpp>

#include <engine/core/log.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>

namespace engine::anim {
namespace {

constexpr int k_none = -1;

// ---------------------------------------------------------------------------
//  The skeleton
// ---------------------------------------------------------------------------

/// Build the skeleton: the skin's joints and every node above them, parent first.
///
/// Fills `out.sk.joints`, `out.joint_node` and `joint_of_node` (node -> joint, or
/// -1), and returns false with `report.status` set if the skin is unusable.
[[nodiscard]] bool build_skeleton(const gltf_scene_data& file, const gltf_skin_desc& skin,
                                  imported_rig& out, std::vector<int>& joint_of_node,
                                  rig_import_report& report)
{
    const std::size_t n_nodes = file.nodes.size();
    report.skin_joints = skin.joints.size();
    if (skin.joints.empty())
    {
        report.status = rig_status::empty_skin;
        return false;
    }
    for (int node : skin.joints)
    {
        if (node < 0 || static_cast<std::size_t>(node) >= n_nodes)
        {
            report.status = rig_status::bad_joint;
            return false;
        }
    }

    // ---- Which nodes belong: the joints, and everything above them --------
    //
    // `in_skin` marks the skin's own joints; `in_rig` adds every ancestor. The
    // walk up is bounded by the node count, so a cycle (which cgltf_validate
    // already refuses) could not hang the loader even if one slipped through.
    std::vector<char> in_skin(n_nodes, 0);
    std::vector<char> in_rig(n_nodes, 0);
    for (int node : skin.joints)
    {
        in_skin[static_cast<std::size_t>(node)] = 1;
        int at = node;
        for (std::size_t guard = 0; at >= 0 && guard < n_nodes; ++guard)
        {
            in_rig[static_cast<std::size_t>(at)] = 1;
            at = file.nodes[static_cast<std::size_t>(at)].parent;
        }
    }

    // ---- How the FILE's order would have gone -----------------------------
    //
    // Measured before sorting, because it is the answer to "would a skeleton
    // built in skin order have worked?" — which for Blender is yes, and for the
    // next exporter may not be.
    std::vector<int> rank(n_nodes, k_none);
    for (std::size_t r = 0; r < skin.joints.size(); ++r)
    {
        rank[static_cast<std::size_t>(skin.joints[r])] = static_cast<int>(r);
    }
    for (std::size_t r = 0; r < skin.joints.size(); ++r)
    {
        const int parent = file.nodes[static_cast<std::size_t>(skin.joints[r])].parent;
        if (parent >= 0 && rank[static_cast<std::size_t>(parent)] > static_cast<int>(r))
        {
            ++report.file_order_breaks;
        }
    }

    // ---- The stable sort --------------------------------------------------
    //
    // Take the skin's joints in the skin's order, and before emitting each one,
    // emit its not-yet-emitted ancestors from the top down. Every node is then
    // emitted after its parent, ancestors land immediately before their first
    // descendant, and a skin that was already parent-first comes out in exactly
    // its own order — which is what makes `resorted` mean something. Iterative
    // rather than recursive: the chain is at most the file's depth, but a
    // loader's stack depth should not be the file's choice.
    joint_of_node.assign(n_nodes, k_none);
    std::vector<int> order;
    std::vector<int> chain;
    order.reserve(n_nodes);
    for (int node : skin.joints)
    {
        chain.clear();
        for (int at = node; at >= 0 && joint_of_node[static_cast<std::size_t>(at)] == k_none
                            && chain.size() < n_nodes;
             at = file.nodes[static_cast<std::size_t>(at)].parent)
        {
            chain.push_back(at);
        }
        for (auto it = chain.rbegin(); it != chain.rend(); ++it)
        {
            joint_of_node[static_cast<std::size_t>(*it)] = static_cast<int>(order.size());
            order.push_back(*it);
        }
    }

    if (order.size() > k_max_joints)
    {
        report.status = rig_status::too_many_joints;
        return false;
    }

    report.joints = order.size();
    report.ancestors = order.size() - skin.joints.size();

    // `resorted`: walk the new order, keep only skin joints, and count those
    // whose place among them is not their place in the file.
    int seen = 0;
    for (int node : order)
    {
        if (!in_skin[static_cast<std::size_t>(node)]) { continue; }
        if (rank[static_cast<std::size_t>(node)] != seen) { ++report.resorted; }
        ++seen;
    }

    // ---- The joints themselves --------------------------------------------
    out.sk.joints.resize(order.size());
    out.joint_node = order;
    for (std::size_t j = 0; j < order.size(); ++j)
    {
        const gltf_node_desc& nd = file.nodes[static_cast<std::size_t>(order[j])];
        joint& jt = out.sk.joints[j];
        jt.name = nd.name;

        // The rest value of every channel a clip leaves alone. glTF: "Non-animated
        // properties MUST keep their values during animation" (§3.11), and the
        // value a node keeps is this one — which is why the node's transform, and
        // not the bind pose the inverse binds encode, is what `local_bind` holds.
        jt.local_bind = nd.local;

        // A root of the skeleton is a node with no parent in the rig — a scene
        // root, since every ancestor was carried in.
        jt.parent = (nd.parent >= 0 && joint_of_node[static_cast<std::size_t>(nd.parent)] != k_none)
            ? static_cast<joint_index>(joint_of_node[static_cast<std::size_t>(nd.parent)])
            : k_no_parent;

        if (nd.from_matrix)
        {
            report.worst_out_of_square = std::max(report.worst_out_of_square, nd.out_of_square);
        }
    }

    // ---- The inverse binds -------------------------------------------------
    //
    // Baked first, for EVERY joint, and then overwritten for the skin's joints
    // with what the file says. The ancestors keep their baked values: no vertex
    // is weighted to them, and baking makes their skinning matrix the identity
    // at rest, which keeps `validate`'s residual about the file rather than
    // about joints the file never bound.
    bake_inverse_binds(out.sk);
    report.identity_inverse_binds = skin.inverse_bind.empty();
    for (std::size_t r = 0; r < skin.joints.size(); ++r)
    {
        const int node = skin.joints[r];
        const std::size_t j = static_cast<std::size_t>(joint_of_node[static_cast<std::size_t>(node)]);
        out.sk.joint_from_model[j] = skin.inverse_bind.empty() ? mat4::identity()
                                                               : skin.inverse_bind[r];
    }
    return true;
}

// ---------------------------------------------------------------------------
//  The meshes
// ---------------------------------------------------------------------------

/// One (joint, weight) pair on its way from the file to a `skin_influence`.
struct pair_jw
{
    int joint;
    float weight;
};

/// Turn every skinned primitive of this skin into a `skinned_mesh`.
void build_meshes(const gltf_scene_data& file, int skin_index, const gltf_skin_desc& skin,
                  const std::vector<int>& joint_of_node, imported_rig& out,
                  rig_import_report& report)
{
    // The skin's own index space (what JOINTS_n holds) -> the skeleton's.
    std::vector<int> ours(skin.joints.size());
    for (std::size_t r = 0; r < skin.joints.size(); ++r)
    {
        ours[r] = joint_of_node[static_cast<std::size_t>(skin.joints[r])];
    }

    std::vector<pair_jw> pairs;
    for (const gltf_primitive& p : file.primitives)
    {
        if (p.skin != skin_index || p.influence_sets == 0) { continue; }

        skinned_mesh m;
        m.bind = p.geometry;
        const std::size_t n = m.bind.vertices.size();
        const std::size_t per = static_cast<std::size_t>(p.influence_sets) * 4u;
        m.influences.resize(n);

        for (std::size_t v = 0; v < n; ++v)
        {
            pairs.clear();
            double sum = 0.0;
            for (std::size_t k = 0; k < per; ++k)
            {
                const float w = p.weights[v * per + k];
                const std::size_t idx = p.joints[v * per + k];
                sum += static_cast<double>(w);
                if (w == 0.0f) { continue; }
                if (w < 0.0f || idx >= ours.size())
                {
                    ++report.bad_influences;
                    continue;
                }
                pairs.push_back({ours[idx], w});
            }
            report.worst_weight_sum = std::max(report.worst_weight_sum,
                                               static_cast<float>(std::fabs(sum - 1.0)));

            // The four largest, and — when there were more — what the rest cost.
            // stable_sort, so equal weights keep the file's order and the result
            // does not depend on the standard library's sort.
            std::stable_sort(pairs.begin(), pairs.end(),
                             [](const pair_jw& a, const pair_jw& b) { return a.weight > b.weight; });
            if (pairs.size() > static_cast<std::size_t>(k_max_influences))
            {
                ++report.truncated;
                report.worst_dropped = std::max(report.worst_dropped,
                                                pairs[static_cast<std::size_t>(k_max_influences)].weight);
                pairs.resize(static_cast<std::size_t>(k_max_influences));
            }

            // Unused slots: joint 0, weight 0 — `skin_influence`'s own rule, that
            // a zero weight must still carry a valid index.
            skin_influence& inf = m.influences[v];
            for (int k = 0; k < k_max_influences; ++k)
            {
                const bool used = static_cast<std::size_t>(k) < pairs.size();
                inf.joints[k] = used ? static_cast<joint_index>(pairs[static_cast<std::size_t>(k)].joint) : 0;
                inf.weights[k] = used ? pairs[static_cast<std::size_t>(k)].weight : 0.0f;
            }
            ++report.influences[pairs.size()];
        }

        report.repaired += normalise_weights(m, out.sk.size());
        report.vertices += n;
        ++report.meshes;
        out.meshes.push_back(std::move(m));
        out.mesh_material.push_back(p.material);
    }
}

// ---------------------------------------------------------------------------
//  The curves: the file's, evaluated exactly
// ---------------------------------------------------------------------------

/// Where value `k` of a sampler lives in its flat array: every key holds one
/// value, except CUBICSPLINE's, which hold in-tangent, value, out-tangent.
[[nodiscard]] std::size_t value_slot(const gltf_sampler_desc& s, std::size_t k)
{
    return (s.interpolation == gltf_interpolation::cubic_spline) ? 3u * k + 1u : k;
}

[[nodiscard]] vec3 read_vec3(const gltf_sampler_desc& s, std::size_t slot)
{
    const float* f = &s.values[slot * 3u];
    return {f[0], f[1], f[2]};
}

/// A rotation value or tangent, through the ONE swizzle (`quat_from_xyzw`).
/// Tangents are not unit and are not normalised here.
[[nodiscard]] quat read_quat(const gltf_sampler_desc& s, std::size_t slot)
{
    const float* f = &s.values[slot * 4u];
    return quat_from_xyzw(f[0], f[1], f[2], f[3]);
}

/// The spec's cubic Hermite basis at `u`, with the tangents scaled by the
/// interval's length (§C.5: `t_d (t³ − 2t² + t) b_k` and `t_d (t³ − t²) a_{k+1}`).
///
/// **The `t_d` is the whole trap.** File tangents are per SECOND; the basis
/// wants them per unit of `u`, which runs 0 to 1 over the interval. Leave it out
/// and every tangent is off by a factor of `1 / t_d` — too STEEP on any interval
/// shorter than a second, which is every interval an animator keys: thirty times
/// on a 30 Hz curve, 7.5 times on Blender's four-frame keys, and the curve
/// overshoots between every pair of keys by tens of degrees (verify_77b §E: 40.25
/// on the walk). Only an interval longer than a second comes out flatter. (The
/// first draft of this comment had the direction backwards; the plot of the
/// mistake is what corrected it.)
struct hermite
{
    float h00, h10, h01, h11;
};

[[nodiscard]] hermite hermite_at(float u, float td)
{
    const float u2 = u * u;
    const float u3 = u2 * u;
    return {2.0f * u3 - 3.0f * u2 + 1.0f,
            td * (u3 - 2.0f * u2 + u),
            -2.0f * u3 + 3.0f * u2,
            td * (u3 - u2)};
}

/// The file's translation or scale curve on interval `k` at `u` in [0, 1].
[[nodiscard]] vec3 file_vec3(const gltf_sampler_desc& s, std::size_t k, float u)
{
    if (s.interpolation == gltf_interpolation::cubic_spline)
    {
        const hermite h = hermite_at(u, s.times[k + 1] - s.times[k]);
        const vec3 v0 = read_vec3(s, 3u * k + 1u);
        const vec3 b0 = read_vec3(s, 3u * k + 2u);   // out-tangent of k
        const vec3 v1 = read_vec3(s, 3u * (k + 1u) + 1u);
        const vec3 a1 = read_vec3(s, 3u * (k + 1u));  // in-tangent of k+1
        return v0 * h.h00 + b0 * h.h10 + v1 * h.h01 + a1 * h.h11;
    }
    const vec3 a = read_vec3(s, k);
    const vec3 b = read_vec3(s, k + 1u);
    return a + (b - a) * u;
}

/// The file's rotation curve on interval `k` at `u`: SLERP for LINEAR (§C.4),
/// Hermite on the four raw components and then normalised for CUBICSPLINE (§C.5:
/// "the interpolated quaternion MUST be normalized").
///
/// The cubic case does NOT pick the nearer sign first, and deliberately: the
/// spec's formula has no `nearest` in it, and an exporter is told instead to
/// avoid writing `v_k = -v_{k+1}`. Following the spec exactly is the point of a
/// reference curve.
[[nodiscard]] quat file_quat(const gltf_sampler_desc& s, std::size_t k, float u)
{
    if (s.interpolation == gltf_interpolation::cubic_spline)
    {
        const hermite h = hermite_at(u, s.times[k + 1] - s.times[k]);
        const quat v0 = read_quat(s, 3u * k + 1u);
        const quat b0 = read_quat(s, 3u * k + 2u);
        const quat v1 = read_quat(s, 3u * (k + 1u) + 1u);
        const quat a1 = read_quat(s, 3u * (k + 1u));
        const quat raw{v0.w * h.h00 + b0.w * h.h10 + v1.w * h.h01 + a1.w * h.h11,
                       v0.v * h.h00 + b0.v * h.h10 + v1.v * h.h01 + a1.v * h.h11};
        return normalised_or(raw, normalised(v0));
    }
    return quat_slerp(normalised(read_quat(s, k)), normalised(read_quat(s, k + 1u)), u);
}

// ---------------------------------------------------------------------------
//  The curves: ours, fitted to the file's
// ---------------------------------------------------------------------------

/// How far apart two values of a channel are, in that channel's tolerance units.
[[nodiscard]] float distance_of(vec3 a, vec3 b, bool is_scale)
{
    if (is_scale)
    {
        return std::max({std::fabs(a.x - b.x), std::fabs(a.y - b.y), std::fabs(a.z - b.z)});
    }
    return length(a - b);
}

/// The seven probes inside every piece. The midpoint is among them and is not
/// enough on its own — see the header's note on nlerp and slerp.
constexpr int k_probes = 7;

/// Split interval `k` of `s` into the fewest equal pieces for which our
/// interpolation follows the file's curve within `limit`, and append the keys
/// strictly inside the interval to `keys`. Returns the residual at the probes.
///
/// A template over the two value types a channel can hold, with the three things
/// that differ between them — how the file evaluates, how we interpolate, how
/// far apart two values are — passed in. The search itself is written once.
template <typename T, typename FileFn, typename OurFn, typename DistFn>
float fit_interval(const gltf_sampler_desc& s, std::size_t k, float limit, int max_split,
                   FileFn file_at, OurFn ours_at, DistFn dist,
                   std::vector<keyframe<T>>& keys, rig_import_report& report)
{
    const float t0 = s.times[k];
    const float t1 = s.times[k + 1];

    std::vector<T> nodes;
    float err = 0.0f;
    int m = 1;
    for (; m <= max_split; ++m)
    {
        nodes.resize(static_cast<std::size_t>(m) + 1u);
        for (int i = 0; i <= m; ++i)
        {
            nodes[static_cast<std::size_t>(i)] =
                file_at(k, static_cast<float>(i) / static_cast<float>(m));
        }
        err = 0.0f;
        for (int i = 0; i < m; ++i)
        {
            for (int p = 1; p <= k_probes; ++p)
            {
                const float f = static_cast<float>(p) / static_cast<float>(k_probes + 1);
                const float u = (static_cast<float>(i) + f) / static_cast<float>(m);
                err = std::max(err, dist(ours_at(nodes[static_cast<std::size_t>(i)],
                                                 nodes[static_cast<std::size_t>(i) + 1u], f),
                                         file_at(k, u)));
            }
        }
        if (err <= limit) { break; }
    }
    if (m > max_split)
    {
        m = max_split;
        ++report.capped;
    }

    // The interior keys, at their exact file values. A key whose time rounds
    // onto a neighbour's is skipped rather than written: the sampler's
    // precondition is STRICTLY increasing time, and an interval too short to
    // hold it is too short to need it.
    for (int i = 1; i < m; ++i)
    {
        const float t = t0 + (t1 - t0) * (static_cast<float>(i) / static_cast<float>(m));
        if (t <= keys.back().time || t >= t1) { continue; }
        keys.push_back({t, nodes[static_cast<std::size_t>(i)]});
        ++report.split_keys;
    }
    return err;
}

/// Convert one sampler into a channel of vec3 keys (translation or scale).
void convert_vec3(const gltf_sampler_desc& s, bool is_scale, const import_settings& settings,
                  std::vector<vec3_key>& keys, rig_import_report& report)
{
    const std::size_t n = s.times.size();
    keys.clear();
    for (std::size_t k = 0; k < n; ++k)
    {
        const vec3 v = read_vec3(s, value_slot(s, k));
        keys.push_back({s.times[k], v});
        if (k + 1u == n) { break; }

        if (s.interpolation == gltf_interpolation::step)
        {
            // HOLD UNTIL ONE FLOAT STEP BEFORE THE NEXT KEY. Our sampler has no
            // step mode, and it does not need one: two keys with the same value
            // a single representable time apart ARE a step, to within the one
            // float between them. Only written when the value actually changes —
            // Blender writes every constant channel as a two-key STEP, and those
            // need nothing.
            const vec3 next = read_vec3(s, value_slot(s, k + 1u));
            const float hold = std::nextafter(s.times[k + 1], -std::numeric_limits<float>::infinity());
            if ((next.x != v.x || next.y != v.y || next.z != v.z) && hold > s.times[k])
            {
                keys.push_back({hold, v});
                ++report.step_keys;
            }
            continue;
        }
        if (s.interpolation == gltf_interpolation::linear) { continue; }   // ours exactly

        const float limit = is_scale ? settings.tolerance.scale : settings.tolerance.position;
        const float err = fit_interval<vec3>(
            s, k, limit, settings.max_split,
            [&](std::size_t kk, float u) { return file_vec3(s, kk, u); },
            [](vec3 a, vec3 b, float u) { return lerp(a, b, u); },
            [&](vec3 a, vec3 b) { return distance_of(a, b, is_scale); },
            keys, report);
        float& worst = is_scale ? report.worst_scale : report.worst_position;
        worst = std::max(worst, err);
    }
}

/// Convert one sampler into a channel of rotation keys.
void convert_quat(const gltf_sampler_desc& s, const import_settings& settings,
                  std::vector<quat_key>& keys, rig_import_report& report)
{
    const std::size_t n = s.times.size();
    keys.clear();

    // Every key value once, normalised, through the one swizzle. A file rotation
    // "MUST be unit" (§3.5.3); one that is not by more than float noise — a
    // quantised export, usually — is counted, and renormalised either way.
    std::vector<quat> value(n);
    for (std::size_t k = 0; k < n; ++k)
    {
        const quat raw = read_quat(s, value_slot(s, k));
        if (std::fabs(length(raw) - 1.0f) > 1e-3f) { ++report.unnormalised_keys; }
        value[k] = normalised_or(raw, quat::identity());
    }

    for (std::size_t k = 0; k < n; ++k)
    {
        const quat v = value[k];
        keys.push_back({s.times[k], v});
        if (k + 1u == n) { break; }

        if (s.interpolation == gltf_interpolation::step)
        {
            const quat next = value[k + 1u];
            const float hold = std::nextafter(s.times[k + 1], -std::numeric_limits<float>::infinity());
            const bool changes = next.w != v.w || next.v.x != v.v.x || next.v.y != v.v.y
                              || next.v.z != v.v.z;
            if (changes && hold > s.times[k])
            {
                keys.push_back({hold, v});
                ++report.step_keys;
            }
            continue;
        }

        // LINEAR rotations are SLERP in the file and nlerp here, so unlike
        // positions they are fitted too — on sampled content, almost always with
        // no split at all.
        const float err = fit_interval<quat>(
            s, k, settings.tolerance.rotation, settings.max_split,
            [&](std::size_t kk, float u) { return file_quat(s, kk, u); },
            [](quat a, quat b, float u) { return quat_nlerp(a, b, u); },
            [](quat a, quat b) { return angle_between(a, b); },
            keys, report);
        report.worst_rotation = std::max(report.worst_rotation, err);
    }
}

/// Is a sampler's value the same at every key? (Blender's constant channels.)
[[nodiscard]] bool is_constant(const gltf_sampler_desc& s)
{
    const std::size_t width = static_cast<std::size_t>(s.components);
    const std::size_t n = s.times.size();
    for (std::size_t k = 1; k < n; ++k)
    {
        for (std::size_t c = 0; c < width; ++c)
        {
            if (s.values[value_slot(s, k) * width + c] != s.values[value_slot(s, 0) * width + c])
            {
                return false;
            }
        }
    }
    return true;
}

/// One glTF animation -> one clip, if it moves this skeleton. `moves` collects,
/// per joint, whether any channel changes it over time.
[[nodiscard]] bool build_clip(const gltf_animation_desc& a, const std::vector<int>& joint_of_node,
                              const import_settings& settings, clip& c, std::vector<char>& moves,
                              rig_import_report& report)
{
    c.name = a.name;
    c.loops = settings.loops;
    c.tracks.assign(moves.size(), joint_track{});

    // THE DURATION IS NOT IN THE FILE. The spec's time runs from 0 to the last
    // input of any sampler in the animation, so that is what a clip's length has
    // to be — and it is the right answer exactly when the exporter wrote a closing
    // key equal to the opening one, which is Lesson 7.7's `duration` argument
    // arriving from the other side. `validate`'s loop gap measures whether it did.
    c.duration = 0.0f;
    for (const gltf_sampler_desc& s : a.samplers)
    {
        if (!s.times.empty()) { c.duration = std::max(c.duration, s.times.back()); }
    }

    bool any = false;
    for (const gltf_channel_desc& ch : a.channels)
    {
        ++report.channels;
        if (ch.path == gltf_path::weights)
        {
            ++report.morph;
            continue;
        }
        const bool valid_sampler = ch.sampler >= 0
                                   && static_cast<std::size_t>(ch.sampler) < a.samplers.size();
        const int j = (ch.node >= 0 && static_cast<std::size_t>(ch.node) < joint_of_node.size())
            ? joint_of_node[static_cast<std::size_t>(ch.node)]
            : k_none;
        if (j == k_none || !valid_sampler)
        {
            ++report.unbound;
            continue;
        }

        const gltf_sampler_desc& s = a.samplers[static_cast<std::size_t>(ch.sampler)];
        const int want = (ch.path == gltf_path::rotation) ? 4 : 3;
        const std::size_t per_key = (s.interpolation == gltf_interpolation::cubic_spline) ? 3u : 1u;
        if (s.times.empty() || s.components != want
            || s.values.size() != s.times.size() * per_key * static_cast<std::size_t>(want))
        {
            ++report.unbound;
            continue;
        }

        ++report.bound;
        report.keys_in += s.times.size();
        switch (s.interpolation)
        {
        case gltf_interpolation::linear:       ++report.linear; break;
        case gltf_interpolation::step:         ++report.step;   break;
        case gltf_interpolation::cubic_spline: ++report.cubic;  break;
        }

        joint_track& track = c.tracks[static_cast<std::size_t>(j)];
        switch (ch.path)
        {
        case gltf_path::translation: convert_vec3(s, false, settings, track.position, report); break;
        case gltf_path::scale:       convert_vec3(s, true, settings, track.scale, report);     break;
        case gltf_path::rotation:    convert_quat(s, settings, track.rotation, report);        break;
        case gltf_path::weights:     break;
        }
        if (!is_constant(s)) { moves[static_cast<std::size_t>(j)] = 1; }
        any = true;
    }
    return any;
}

}   // namespace

const char* name_of(rig_status s)
{
    switch (s)
    {
    case rig_status::ok:              return "ok";
    case rig_status::no_such_skin:    return "no such skin";
    case rig_status::empty_skin:      return "the skin names no joints";
    case rig_status::bad_joint:       return "a joint names a node that does not exist";
    case rig_status::too_many_joints: return "more joints than a joint_index can name";
    case rig_status::no_meshes:       return "nothing is skinned by this skin";
    }
    return "?";
}

rig_import_report import_rig(const gltf_scene_data& file, int skin,
                             const import_settings& settings, imported_rig& out)
{
    rig_import_report report;
    out.clear();

    if (skin < 0 || static_cast<std::size_t>(skin) >= file.skins.size())
    {
        report.status = rig_status::no_such_skin;
        ENGINE_LOG_ERROR(log_asset, "import_rig: no skin %d (the file has %zu)", skin,
                         file.skins.size());
        return report;
    }
    const gltf_skin_desc& sd = file.skins[static_cast<std::size_t>(skin)];

    std::vector<int> joint_of_node;
    if (!build_skeleton(file, sd, out, joint_of_node, report))
    {
        ENGINE_LOG_ERROR(log_asset, "import_rig: skin '%s': %s", sd.name.c_str(),
                         name_of(report.status));
        return report;
    }

    build_meshes(file, skin, sd, joint_of_node, out, report);
    if (out.meshes.empty())
    {
        report.status = rig_status::no_meshes;
        ENGINE_LOG_ERROR(log_asset, "import_rig: skin '%s' skins no primitive", sd.name.c_str());
        return report;
    }

    // ---- Clips -------------------------------------------------------------
    std::vector<char> moves(out.sk.size(), 0);
    for (const gltf_animation_desc& a : file.animations)
    {
        clip c;
        if (!build_clip(a, joint_of_node, settings, c, moves, report)) { continue; }
        report.sign_flips += canonicalise_rotations(c);
        report.keys_out += validate(c, out.sk).keys;
        out.clips.push_back(std::move(c));
    }
    report.clips = out.clips.size();

    // ---- What no clip can move ----------------------------------------------
    //
    // A joint moves if a channel changes it or its parent moves; parents come
    // first, so one pass settles it.
    for (std::size_t j = 0; j < out.sk.size(); ++j)
    {
        const joint_index p = out.sk.joints[j].parent;
        if (p != k_no_parent && moves[p]) { moves[j] = 1; }
    }
    for (const skinned_mesh& m : out.meshes)
    {
        for (const skin_influence& inf : m.influences)
        {
            bool frozen = true;
            for (int k = 0; k < k_max_influences; ++k)
            {
                if (inf.weights[k] != 0.0f && moves[inf.joints[k]]) { frozen = false; }
            }
            if (frozen) { ++report.frozen_vertices; }
        }
    }

    report.skeleton = validate(out.sk);
    report.bind_vs_rest = report.skeleton.worst_bind_residual;
    report.status = rig_status::ok;
    return report;
}

}   // namespace engine::anim
