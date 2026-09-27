// engine/src/anim/skeleton.cpp — composing a pose, and undoing a bind.
//
// Lesson 7.6. A translation unit rather than a header for the reason
// `src/gfx/draw_order.cpp` gives: every function here is a real loop over an
// array, and inlining a loop over a hundred joints into every file that mentions
// a skeleton buys nothing and costs a rebuild of all of them whenever the
// composition rule changes.

#include <engine/anim/skeleton.hpp>

#include <cmath>
#include <cstddef>

namespace engine::anim {
namespace {

/// The largest absolute difference between two matrices, entry by entry.
///
/// Used on `palette[j] - I`, so it answers "how far from the identity is this"
/// in the units the matrix is written in — which for a skinning matrix is a
/// mixture of a unitless basis and a length, and that is fine here because the
/// identity's entries are all 0 or 1 and the residual is being compared against
/// zero rather than against a scale.
float max_entry_diff(const mat4& a, const mat4& b)
{
    const vec4 cols_a[4]{a.c0, a.c1, a.c2, a.c3};
    const vec4 cols_b[4]{b.c0, b.c1, b.c2, b.c3};

    float worst = 0.0f;
    for (int c = 0; c < 4; ++c)
    {
        worst = std::fmax(worst, std::fabs(cols_a[c].x - cols_b[c].x));
        worst = std::fmax(worst, std::fabs(cols_a[c].y - cols_b[c].y));
        worst = std::fmax(worst, std::fabs(cols_a[c].z - cols_b[c].z));
        worst = std::fmax(worst, std::fabs(cols_a[c].w - cols_b[c].w));
    }
    return worst;
}

}   // namespace

void bake_inverse_binds(skeleton& sk)
{
    sk.joint_from_model.assign(sk.joints.size(), mat4::identity());

    // ONE FORWARD PASS, AND NOT ONE INVERSION. The forward chain composes on the
    // LEFT — a child's matrix is its parent's times its own — and inverting a
    // product reverses the order, so the inverse chain composes on the RIGHT:
    //
    //     (P * L)^-1  =  L^-1 * P^-1
    //
    // Both are valid here only because a parent's entry is already finished when
    // its child is read, which is the ordering precondition and the only thing
    // this loop assumes.
    for (std::size_t j = 0; j < sk.joints.size(); ++j)
    {
        const mat4 local = local_from_parent(sk.joints[j].local_bind);
        const joint_index p = sk.joints[j].parent;

        sk.joint_from_model[j] = (p == k_no_parent || static_cast<std::size_t>(p) >= j)
                                     ? local
                                     : local * sk.joint_from_model[p];
    }
}

void rest_pose(const skeleton& sk, std::vector<transform>& out)
{
    out.resize(sk.joints.size());
    for (std::size_t j = 0; j < sk.joints.size(); ++j) { out[j] = sk.joints[j].local_bind; }
}

void compose_pose(const skeleton& sk, std::span<const transform> local_pose,
                  std::vector<mat4>& model_from_joint)
{
    model_from_joint.resize(sk.joints.size());

    for (std::size_t j = 0; j < sk.joints.size(); ++j)
    {
        // A pose shorter than the skeleton falls back to the bind transform. The
        // alternative — reading past the end, or refusing to compose — makes a
        // half-authored rig undebuggable, and this way the un-posed joints simply
        // stand at rest, which is exactly what they look like.
        const transform& local = (j < local_pose.size()) ? local_pose[j]
                                                         : sk.joints[j].local_bind;
        const mat4 m = parent_from_local(local);
        const joint_index p = sk.joints[j].parent;

        // Lesson 5.9's composition rule, with the level-order machinery deleted:
        // the array IS the order.
        model_from_joint[j] = (p == k_no_parent || static_cast<std::size_t>(p) >= j)
                                  ? m
                                  : model_from_joint[p] * m;
    }
}

void build_palette(const skeleton& sk, std::span<const mat4> model_from_joint,
                   std::vector<mat4>& palette)
{
    const std::size_t n = sk.joints.size();
    palette.resize(n);

    // An unbaked skeleton would otherwise index an empty array. Composing against
    // the identity means the character renders as if the mesh were already in
    // joint space — badly wrong and visibly so, which beats a crash and beats a
    // silently correct-looking bind pose.
    const bool baked = sk.joint_from_model.size() == n;

    for (std::size_t j = 0; j < n; ++j)
    {
        const mat4 posed = (j < model_from_joint.size()) ? model_from_joint[j]
                                                         : mat4::identity();
        palette[j] = baked ? posed * sk.joint_from_model[j] : posed;
    }
}

void skinning_palette(const skeleton& sk, std::span<const transform> local_pose,
                      std::vector<mat4>& model_from_joint, std::vector<mat4>& palette)
{
    compose_pose(sk, local_pose, model_from_joint);
    build_palette(sk, model_from_joint, palette);
}

skeleton_report validate(const skeleton& sk)
{
    skeleton_report r;
    r.joints = sk.joints.size();
    r.unbaked = sk.joint_from_model.size() != sk.joints.size();

    // Depth is computed in the same pass, and it is O(n) rather than O(n * depth)
    // for free: the ordering precondition means a parent's depth is ALREADY known
    // when its child is reached, so there is nothing to memoise and no chase up
    // the ancestors. Lesson 5.9 needed both because its order was arbitrary.
    std::vector<std::size_t> depth(sk.joints.size(), 1u);

    for (std::size_t j = 0; j < sk.joints.size(); ++j)
    {
        const joint& jt = sk.joints[j];

        if (jt.parent == k_no_parent) { ++r.roots; }
        else if (static_cast<std::size_t>(jt.parent) >= sk.joints.size()) { ++r.bad_parent; }
        else if (static_cast<std::size_t>(jt.parent) >= j)
        {
            // Names a real joint, in the wrong order. Counted separately from
            // `bad_parent` because the picture it produces is different: an
            // out-of-range parent makes the joint a root, and an out-of-ORDER one
            // composes against a stale matrix, which is far harder to see.
            ++r.out_of_order;
        }
        else
        {
            depth[j] = depth[jt.parent] + 1u;
        }

        const vec3 s = jt.local_bind.scale;
        if (s.x == 0.0f || s.y == 0.0f || s.z == 0.0f) { ++r.singular_binds; }
        else if (s.x != s.y || s.y != s.z) { ++r.nonuniform_binds; }

        if (depth[j] > r.depth) { r.depth = depth[j]; }
    }

    // ---- The one number that tests a whole rig -----------------------------
    //
    // Pose the skeleton at its own bind pose. Every skinning matrix must then be
    // the identity, because `model_from_joint[j]` and `joint_from_model[j]` are
    // the same chain composed in opposite orders. Nothing about the mesh, the
    // weights or the renderer enters into it — which is why this test can be run
    // on a rig before any geometry exists.
    if (!r.unbaked && !sk.joints.empty())
    {
        std::vector<transform> rest;
        std::vector<mat4> posed;
        std::vector<mat4> palette;

        rest_pose(sk, rest);
        skinning_palette(sk, rest, posed, palette);

        for (const mat4& m : palette)
        {
            r.worst_bind_residual =
                std::fmax(r.worst_bind_residual, max_entry_diff(m, mat4::identity()));
        }
    }

    return r;
}

}   // namespace engine::anim
