// engine/src/anim/skin.cpp — the four multiply-adds a character costs.
//
// Lesson 7.6. Everything in `anim/skeleton.cpp` runs once per JOINT and
// everything here runs once per VERTEX, and on real content those differ by two
// to three orders of magnitude. That asymmetry is the whole reason the palette
// exists: it is a place to hoist work to where it is nearly free.

#include <engine/anim/skin.hpp>

#include <cmath>
#include <cstddef>

namespace engine::anim {
namespace {

/// Apply the affine matrix `m` to a POINT, and accumulate `w` of the answer.
///
/// Written out rather than going through `mat4 * vec4`, and for once the reason
/// is readability rather than speed: at the call site below it is important that
/// a position picks up `m.c3` and a direction does not, and a `vec4` with a 1 or
/// a 0 in it hides that difference inside a constructor.
vec3 blend_point(const mat4& m, vec3 v, float w)
{
    const vec3 r{m.c0.x * v.x + m.c1.x * v.y + m.c2.x * v.z + m.c3.x,
                 m.c0.y * v.x + m.c1.y * v.y + m.c2.y * v.z + m.c3.y,
                 m.c0.z * v.x + m.c1.z * v.y + m.c2.z * v.z + m.c3.z};
    return r * w;
}

/// The same, for a DIRECTION: the linear part only, no `c3`.
vec3 blend_direction(const mat4& m, vec3 v, float w)
{
    const vec3 r{m.c0.x * v.x + m.c1.x * v.y + m.c2.x * v.z,
                 m.c0.y * v.x + m.c1.y * v.y + m.c2.y * v.z,
                 m.c0.z * v.x + m.c1.z * v.y + m.c2.z * v.z};
    return r * w;
}

}   // namespace

skin_report validate(const skinned_mesh& m, std::size_t joint_count)
{
    skin_report r;
    r.vertices = m.bind.vertices.size();
    r.size_mismatch = m.influences.size() != m.bind.vertices.size();

    float worst_gap = 0.0f;

    for (const skin_influence& inf : m.influences)
    {
        float sum = 0.0f;
        bool bad_index = false;
        bool any_negative = false;

        for (int k = 0; k < k_max_influences; ++k)
        {
            const float w = inf.weights[k];
            const bool live = (w != 0.0f);
            const bool in_range = static_cast<std::size_t>(inf.joints[k]) < joint_count;

            sum += w;
            if (w < 0.0f) { any_negative = true; }

            // Two different problems wearing one symptom. A live slot naming a
            // missing joint changes the picture; a dead one is a latent
            // out-of-bounds read that today's zero weight happens to hide.
            if (!in_range && live) { bad_index = true; }
            if (!in_range && !live) { ++r.padded_indices; }
        }

        if (bad_index) { ++r.out_of_range; }
        if (any_negative) { ++r.negative; }
        if (sum == 0.0f) { ++r.unweighted; }

        const float gap = std::fabs(sum - 1.0f);
        if (gap > k_weight_tolerance) { ++r.unnormalised; }
        if (gap > worst_gap) { worst_gap = gap; r.worst_weight_sum = sum; }
    }

    return r;
}

std::size_t normalise_weights(skinned_mesh& m, std::size_t joint_count)
{
    std::size_t changed = 0;

    for (skin_influence& inf : m.influences)
    {
        bool touched = false;
        float sum = 0.0f;

        for (int k = 0; k < k_max_influences; ++k)
        {
            // An index the palette does not contain cannot contribute, and
            // zeroing the weight is the only repair that does not invent a joint.
            // The index itself is clamped to 0 as well, so the slot is safe to
            // read unconditionally in the hot loop below.
            if (static_cast<std::size_t>(inf.joints[k]) >= joint_count)
            {
                if (inf.weights[k] != 0.0f || inf.joints[k] != 0) { touched = true; }
                inf.joints[k] = 0;
                inf.weights[k] = 0.0f;
            }
            if (inf.weights[k] < 0.0f) { inf.weights[k] = 0.0f; touched = true; }
            sum += inf.weights[k];
        }

        if (sum == 0.0f)
        {
            // Attached to nothing. Give it to the first slot rather than leaving
            // it at the origin: a vertex frozen at the model's origin draws a
            // spike halfway across the character, and `validate` has already
            // counted this case so the repair is not a secret.
            inf.joints[0] = 0;
            inf.weights[0] = 1.0f;
            ++changed;
            continue;
        }

        if (std::fabs(sum - 1.0f) > 1e-6f)
        {
            const float inv = 1.0f / sum;
            for (int k = 0; k < k_max_influences; ++k) { inf.weights[k] *= inv; }
            touched = true;
        }

        if (touched) { ++changed; }
    }

    return changed;
}

void skin_positions(std::span<const vec3> bind,
                    std::span<const skin_influence> influences,
                    std::span<const mat4> palette,
                    std::span<vec3> out)
{
    const std::size_t n = std::min({bind.size(), influences.size(), out.size()});
    if (palette.empty()) { return; }

    for (std::size_t i = 0; i < n; ++i)
    {
        const skin_influence& inf = influences[i];
        const vec3 v = bind[i];

        // THE FOUR MULTIPLY-ADDS. No branch on the weight being zero, and that is
        // deliberate: a zero weight costs one multiply and a predictable branch
        // costs about the same, while an unpredictable one costs far more. The
        // index is always valid (see `skin_influence`), so there is nothing to
        // guard and the loop body is straight-line code a compiler can vectorise.
        vec3 sum{0.0f, 0.0f, 0.0f};
        for (int k = 0; k < k_max_influences; ++k)
        {
            sum = sum + blend_point(palette[inf.joints[k]], v, inf.weights[k]);
        }
        out[i] = sum;
    }
}

void skin_directions(std::span<const vec3> bind,
                     std::span<const skin_influence> influences,
                     std::span<const mat4> palette,
                     std::span<vec3> out)
{
    const std::size_t n = std::min({bind.size(), influences.size(), out.size()});
    if (palette.empty()) { return; }

    for (std::size_t i = 0; i < n; ++i)
    {
        const skin_influence& inf = influences[i];
        const vec3 v = bind[i];

        vec3 sum{0.0f, 0.0f, 0.0f};
        for (int k = 0; k < k_max_influences; ++k)
        {
            sum = sum + blend_direction(palette[inf.joints[k]], v, inf.weights[k]);
        }

        // A weighted sum of unit vectors is shorter than one — by the same
        // `cos(theta/2)` that collapses the positions, which is worth noticing:
        // the artifact this file's header describes is visible in the SHADING
        // before it is visible in the silhouette, as a band of wrongly-lit
        // pixels around a twisted joint. `normalised_or` keeps a direction that
        // cancelled exactly from becoming a NaN.
        out[i] = normalised_or(sum, v);
    }
}

void skin_into(const skinned_mesh& src, std::span<const mat4> palette, mesh_data& out)
{
    const std::size_t n = src.bind.vertices.size();

    // ---- The static half, copied once ---------------------------------------
    //
    // Uvs, tangents and indices do not depend on the pose, so they are copied
    // only when `out` is not already the right shape — which is once, on the
    // first frame, and again only if the source mesh is replaced. Everything
    // below this block is per-frame work.
    if (out.vertices.size() != n || out.indices.size() != src.bind.indices.size())
    {
        out.vertices.assign(n, vec3{});
        out.normals.assign(src.bind.normals.size(), vec3{});
        out.uvs = src.bind.uvs;
        out.tangents = src.bind.tangents;
        out.indices = src.bind.indices;
    }

    skin_positions(src.bind.vertices, src.influences, palette, out.vertices);

    if (!src.bind.normals.empty())
    {
        skin_directions(src.bind.normals, src.influences, palette, out.normals);
    }
}

}   // namespace engine::anim
