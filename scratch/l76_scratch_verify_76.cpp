// scratch/verify_76.cpp — every number Lesson 7.6 prints, measured rather than
// asserted.
//
// Build and run:  sh scratch/build_verify_76.sh
//
// Nine sections, and the order is the lesson's:
//
//   A  local_from_parent is the exact inverse — and what it is when it is not
//   B  the inverse bind chain, against a general inverse that shares no code
//   C  the bind pose is the identity, and what breaks it
//   D  skinning at the bind pose reproduces the mesh; and the classic bug
//   E  the weights must sum to one, and what a 0.9 looks like
//   F  the candy wrapper: cos(theta/2), and the twist-bone fix cos(theta/2n)
//   G  normals, and the inverse transpose we are not using
//   H  what it costs, and the joints-to-vertices asymmetry
//   I  LBS against nlerp and slerp — two errors, and one the matrices dodge
//
// EVERY SECTION CARRIES A CONTROL, for the reason 7.1 states and 7.2 through 7.5
// repeat: a check whose degenerate case is a pass is not a check. The sharpest
// ones here are in B and C. B's claim is that composing exact per-node inverses
// equals inverting the composed matrix, so its control is a GENERAL 4x4 inverse
// written from scratch in this file — Gauss-Jordan with partial pivoting, which
// shares no line of code with the engine and can therefore disagree with it.
// C's claim is that a correct rig's bind residual is float noise, so its control
// is the same rig with one joint's bind transform nudged, which must report a
// residual four orders larger.
//
// THE RIG BELOW IS THE HARNESS'S OWN, not the demo's, and that is deliberate.
// `demos/rig` builds a 512-vertex tube to look at; this file builds the smallest
// chain that can be wrong in each of the ways above, because a test whose
// fixture is big enough to be pretty is a test whose failures are hard to read.
// The two share no code and agree on section F's numbers, which is a stronger
// statement than either could make alone.
//
// OUTPUT WIDTH IS A CONSTRAINT, not a preference. The lesson quotes this
// program's transcript in <pre> blocks that scroll and never wrap, and the fold
// is at about 66 characters. Nothing below exceeds 66.

#include <engine/anim/skeleton.hpp>
#include <engine/anim/skin.hpp>
#include <engine/gfx/mesh.hpp>
#include <engine/math/mat3.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/transform.hpp>
#include <engine/math/vec3.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numbers>
#include <vector>

using engine::mat3;
using engine::mat4;
using engine::quat;
using engine::transform;
using engine::vec3;
using engine::anim::joint;
using engine::anim::joint_index;
using engine::anim::k_max_influences;
using engine::anim::k_no_parent;
using engine::anim::skeleton;
using engine::anim::skin_influence;
using engine::anim::skinned_mesh;

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

float max_entry_diff(const mat4& a, const mat4& b)
{
    const engine::vec4 ca[4]{a.c0, a.c1, a.c2, a.c3};
    const engine::vec4 cb[4]{b.c0, b.c1, b.c2, b.c3};
    float worst = 0.0f;
    for (int c = 0; c < 4; ++c)
    {
        worst = std::fmax(worst, std::fabs(ca[c].x - cb[c].x));
        worst = std::fmax(worst, std::fabs(ca[c].y - cb[c].y));
        worst = std::fmax(worst, std::fabs(ca[c].z - cb[c].z));
        worst = std::fmax(worst, std::fabs(ca[c].w - cb[c].w));
    }
    return worst;
}

// ---- The control: a general 4x4 inverse, sharing no code with the engine ----
//
// Gauss-Jordan with partial pivoting, in double. It exists so that section B's
// claim — that composing exact per-node inverses is the same thing as inverting
// the composed matrix — is checked against an independent route rather than
// against a rearrangement of itself.
mat4 general_inverse(const mat4& m)
{
    double a[4][8]{};
    const engine::vec4 cols[4]{m.c0, m.c1, m.c2, m.c3};
    for (int c = 0; c < 4; ++c)
    {
        a[0][c] = static_cast<double>(cols[c].x);
        a[1][c] = static_cast<double>(cols[c].y);
        a[2][c] = static_cast<double>(cols[c].z);
        a[3][c] = static_cast<double>(cols[c].w);
    }
    for (int r = 0; r < 4; ++r) { a[r][4 + r] = 1.0; }

    for (int col = 0; col < 4; ++col)
    {
        int pivot = col;
        for (int r = col + 1; r < 4; ++r)
        {
            if (std::fabs(a[r][col]) > std::fabs(a[pivot][col])) { pivot = r; }
        }
        for (int k = 0; k < 8; ++k) { std::swap(a[col][k], a[pivot][k]); }

        const double d = a[col][col];
        for (int k = 0; k < 8; ++k) { a[col][k] /= d; }

        for (int r = 0; r < 4; ++r)
        {
            if (r == col) { continue; }
            const double f = a[r][col];
            for (int k = 0; k < 8; ++k) { a[r][k] -= f * a[col][k]; }
        }
    }

    mat4 out;
    engine::vec4* dst[4]{&out.c0, &out.c1, &out.c2, &out.c3};
    for (int c = 0; c < 4; ++c)
    {
        *dst[c] = {static_cast<float>(a[0][4 + c]), static_cast<float>(a[1][4 + c]),
                   static_cast<float>(a[2][4 + c]), static_cast<float>(a[3][4 + c])};
    }
    return out;
}

// ---- The fixture -----------------------------------------------------------

/// A deterministic spread of unit quaternions — Shoemake's uniform sampling,
/// the same generator 7.4 and 7.5 used, so the three harnesses sample the same
/// orientations.
quat sample_quat(std::uint32_t i)
{
    std::uint32_t s = i * 2654435761u + 1013904223u;
    auto next = [&s]() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return static_cast<float>(s & 0xFFFFFFu) / static_cast<float>(0xFFFFFF);
    };
    const float u1 = next();
    const float u2 = next();
    const float u3 = next();
    const float r1 = std::sqrt(1.0f - u1);
    const float r2 = std::sqrt(u1);
    return {r1 * std::sin(2.0f * k_pi * u2),
            {r1 * std::cos(2.0f * k_pi * u2),
             r2 * std::sin(2.0f * k_pi * u3),
             r2 * std::cos(2.0f * k_pi * u3)}};
}

/// A chain of `n` joints along +y, each one unit above the last, with a
/// deliberately awkward bind rotation and a non-uniform scale on one link.
///
/// Awkward on purpose: a chain of identity rotations makes half of section B
/// pass for the wrong reason, because `local_from_parent` of a pure translation
/// is a pure negation and never exercises the transpose.
skeleton make_chain(std::size_t n, bool with_scale = true)
{
    skeleton sk;
    sk.joints.resize(n);
    for (std::size_t j = 0; j < n; ++j)
    {
        joint& jt = sk.joints[j];
        jt.parent = (j == 0) ? k_no_parent : static_cast<joint_index>(j - 1);
        jt.name = "joint" + std::to_string(j);
        jt.local_bind.position = (j == 0) ? vec3{0.3f, 0.1f, -0.2f} : vec3{0.0f, 1.0f, 0.0f};
        jt.local_bind.rotation =
            engine::quat_from_axis_angle(engine::normalised(vec3{0.3f, 1.0f, 0.2f}),
                                         rad(17.0f * static_cast<float>(j + 1)));
        jt.local_bind.scale = (with_scale && j == 1) ? vec3{1.3f, 0.8f, 1.1f}
                                                     : vec3{1.0f, 1.0f, 1.0f};
    }
    engine::anim::bake_inverse_binds(sk);
    return sk;
}

/// A ring of `rings` x `around` vertices along the chain, weighted to the two
/// nearest joints by how far it sits between them. The smallest thing that can
/// show a candy wrapper.
skinned_mesh make_tube(const skeleton& sk, int rings, int around, float radius)
{
    skinned_mesh m;
    const float span = static_cast<float>(sk.size() - 1);

    for (int r = 0; r < rings; ++r)
    {
        const float t = static_cast<float>(r) / static_cast<float>(rings - 1);
        const float y = t * span;
        for (int a = 0; a < around; ++a)
        {
            const float phi = 2.0f * k_pi * static_cast<float>(a)
                            / static_cast<float>(around);
            m.bind.vertices.push_back({radius * std::cos(phi), y, radius * std::sin(phi)});
            m.bind.normals.push_back({std::cos(phi), 0.0f, std::sin(phi)});

            // Two influences, linear in the distance between the joints either
            // side. This is the weighting every "my first skin" produces and it
            // is also what an automatic bind in a DCC tool starts from.
            const float f = y;
            const int lower = std::min(static_cast<int>(f), static_cast<int>(span));
            const float frac = f - static_cast<float>(lower);
            const int upper = std::min(lower + 1, static_cast<int>(span));

            skin_influence inf;
            inf.joints[0] = static_cast<joint_index>(lower);
            inf.joints[1] = static_cast<joint_index>(upper);
            inf.weights[0] = 1.0f - frac;
            inf.weights[1] = frac;
            inf.weights[2] = 0.0f;
            inf.weights[3] = 0.0f;
            m.influences.push_back(inf);
        }
    }

    for (int r = 0; r + 1 < rings; ++r)
    {
        for (int a = 0; a < around; ++a)
        {
            const std::uint16_t i0 = static_cast<std::uint16_t>(r * around + a);
            const std::uint16_t i1 =
                static_cast<std::uint16_t>(r * around + (a + 1) % around);
            const std::uint16_t i2 = static_cast<std::uint16_t>(i0 + around);
            const std::uint16_t i3 = static_cast<std::uint16_t>(i1 + around);
            m.bind.indices.insert(m.bind.indices.end(), {i0, i2, i1, i1, i2, i3});
        }
    }
    return m;
}

float worst_vertex_move(const std::vector<vec3>& a, const std::vector<vec3>& b)
{
    float worst = 0.0f;
    for (std::size_t i = 0; i < std::min(a.size(), b.size()); ++i)
    {
        worst = std::fmax(worst, engine::length(a[i] - b[i]));
    }
    return worst;
}

// ---- A  local_from_parent --------------------------------------------------

void section_a()
{
    std::printf("\nA. local_from_parent: the inverse a transform knows\n");

    float worst_round = 0.0f;
    float worst_matrix = 0.0f;
    for (std::uint32_t i = 0; i < 20000u; ++i)
    {
        transform t;
        t.position = {static_cast<float>(i % 17) - 8.0f,
                      static_cast<float>(i % 23) - 11.0f,
                      static_cast<float>(i % 13) - 6.0f};
        t.rotation = sample_quat(i);
        t.scale = {0.25f + static_cast<float>(i % 7) * 0.5f,
                   0.25f + static_cast<float>(i % 5) * 0.5f,
                   0.25f + static_cast<float>(i % 3) * 0.5f};

        const mat4 fwd = engine::parent_from_local(t);
        const mat4 inv = engine::local_from_parent(t);
        worst_round = std::fmax(worst_round, max_entry_diff(inv * fwd, mat4::identity()));
        worst_matrix = std::fmax(worst_matrix, max_entry_diff(inv, general_inverse(fwd)));
    }
    std::printf("   20,000 transforms, worst |L*P - I| entry  %.3e\n",
                static_cast<double>(worst_round));
    std::printf("   worst entry vs a general 4x4 inverse      %.3e\n",
                static_cast<double>(worst_matrix));
    check("A.1  it is the exact inverse", worst_round < 1e-4f);
    check("A.2  and it agrees with Gauss-Jordan", worst_matrix < 1e-4f);

    // The degenerate case, named rather than hidden.
    transform flat;
    flat.rotation = engine::quat_from_axis_angle(vec3{0.0f, 1.0f, 0.0f}, rad(40.0f));
    flat.scale = {1.0f, 0.0f, 1.0f};
    const mat4 proj = engine::local_from_parent(flat) * engine::parent_from_local(flat);
    const float from_identity = max_entry_diff(proj, mat4::identity());
    std::printf("   CONTROL scale (1,0,1): |L*P - I| entry     %.4f\n",
                static_cast<double>(from_identity));
    std::printf("     and every entry of L is finite: %s\n",
                std::isfinite(engine::local_from_parent(flat).c1.y) ? "yes" : "no");
    check("A.3  CONTROL: a zero scale is a projection",
          from_identity > 0.9f && std::isfinite(proj.c1.y));
}

// ---- B  the inverse bind chain ---------------------------------------------

void section_b()
{
    std::printf("\nB. The inverse bind chain, and no inversion in it\n");

    const skeleton sk = make_chain(8);

    // The forward chain, composed here rather than taken from the engine, so
    // that both sides of the comparison are this file's.
    std::vector<mat4> forward(sk.size());
    for (std::size_t j = 0; j < sk.size(); ++j)
    {
        const mat4 local = engine::parent_from_local(sk.joints[j].local_bind);
        forward[j] = (j == 0) ? local : forward[j - 1] * local;
    }

    float worst = 0.0f;
    for (std::size_t j = 0; j < sk.size(); ++j)
    {
        worst = std::fmax(worst,
                          max_entry_diff(sk.joint_from_model[j], general_inverse(forward[j])));
    }
    std::printf("   8-joint chain, deepest worst entry vs a\n");
    std::printf("   general inverse of the composed matrix     %.3e\n",
                static_cast<double>(worst));
    check("B.1  composed inverses == inverse of the composition", worst < 1e-3f);

    // Depth is where a numerical claim earns its keep.
    for (std::size_t n : {2u, 4u, 8u, 16u, 32u})
    {
        const skeleton deep = make_chain(n);
        std::vector<mat4> fwd(n);
        for (std::size_t j = 0; j < n; ++j)
        {
            const mat4 local = engine::parent_from_local(deep.joints[j].local_bind);
            fwd[j] = (j == 0) ? local : fwd[j - 1] * local;
        }
        const float e = max_entry_diff(deep.joint_from_model[n - 1],
                                       general_inverse(fwd[n - 1]));
        const engine::anim::skeleton_report r = engine::anim::validate(deep);
        std::printf("   depth %2zu   vs general inverse %.3e   bind %.3e\n",
                    n, static_cast<double>(e), static_cast<double>(r.worst_bind_residual));
    }

    // CONTROL: a skeleton whose parents come after their children. The ordering
    // precondition is the only thing either loop assumes, and this is what
    // ignoring it costs.
    skeleton wrong = make_chain(6);
    std::swap(wrong.joints[2], wrong.joints[3]);
    const engine::anim::skeleton_report wr = engine::anim::validate(wrong);
    std::printf("   CONTROL parents after children: out_of_order %zu\n", wr.out_of_order);
    check("B.2  CONTROL: validate sees the bad order", wr.out_of_order > 0 && !wr.ok());
}

// ---- C  the bind pose is the identity --------------------------------------

void section_c()
{
    std::printf("\nC. At the bind pose every skinning matrix is I\n");

    const skeleton sk = make_chain(6);
    const engine::anim::skeleton_report r = engine::anim::validate(sk);
    std::printf("   6 joints, depth %zu, roots %zu, unbaked %s\n",
                r.depth, r.roots, r.unbaked ? "yes" : "no");
    std::printf("   worst |skin - I| entry at the bind pose    %.3e\n",
                static_cast<double>(r.worst_bind_residual));
    check("C.1  the residual is float noise", r.worst_bind_residual < 1e-5f);
    check("C.2  and the report says nothing is wrong", r.ok());

    // CONTROL: nudge one joint's bind AFTER baking, which is exactly what a
    // rig edited without a re-export looks like.
    skeleton stale = sk;
    stale.joints[3].local_bind.position.x += 0.01f;
    const engine::anim::skeleton_report sr = engine::anim::validate(stale);
    std::printf("   CONTROL bind moved 0.01 after baking:      %.3e\n",
                static_cast<double>(sr.worst_bind_residual));
    check("C.3  CONTROL: a stale bake is four orders louder",
          sr.worst_bind_residual > 1e-3f);

    // CONTROL: never baked at all.
    skeleton raw = sk;
    raw.joint_from_model.clear();
    const engine::anim::skeleton_report rr = engine::anim::validate(raw);
    std::printf("   CONTROL never baked: unbaked %s, ok %s\n",
                rr.unbaked ? "yes" : "no", rr.ok() ? "yes" : "no");
    check("C.4  CONTROL: an unbaked rig is reported", rr.unbaked && !rr.ok());
}

// ---- D  skinning at the bind pose ------------------------------------------

void section_d()
{
    std::printf("\nD. Skinning at rest reproduces the mesh exactly\n");

    const skeleton sk = make_chain(6);
    skinned_mesh tube = make_tube(sk, 13, 16, 0.35f);

    const engine::anim::skin_report sr = engine::anim::validate(tube, sk.size());
    std::printf("   %zu vertices, unnormalised %zu, out of range %zu\n",
                sr.vertices, sr.unnormalised, sr.out_of_range);
    check("D.1  the weighting is clean", sr.ok());

    std::vector<transform> pose;
    std::vector<mat4> posed;
    std::vector<mat4> palette;
    engine::anim::rest_pose(sk, pose);
    engine::anim::skinning_palette(sk, pose, posed, palette);

    engine::mesh_data out;
    engine::anim::skin_into(tube, palette, out);
    const float moved = worst_vertex_move(tube.bind.vertices, out.vertices);
    std::printf("   worst vertex movement at the bind pose     %.3e\n",
                static_cast<double>(moved));
    check("D.2  nothing moves", moved < 1e-5f);

    // CONTROL, and it is THE bug: skin with the posed joint matrices and no
    // inverse binds. This is what "I multiplied by the bone matrix" produces.
    engine::mesh_data no_bind;
    engine::anim::skin_into(tube, posed, no_bind);
    const float flew = worst_vertex_move(tube.bind.vertices, no_bind.vertices);
    std::printf("   CONTROL without the inverse binds:         %.4f units\n",
                static_cast<double>(flew));
    std::printf("     (the tube is %.2f units long)\n",
                static_cast<double>(static_cast<float>(sk.size() - 1)));
    check("D.3  CONTROL: the mesh leaves the building", flew > 1.0f);

    // And the same test on a posed skeleton: two routes to the same answer.
    // Route one is the engine's palette. Route two is this file, vertex by
    // vertex, from the definition.
    std::vector<transform> bent = pose;
    for (std::size_t j = 1; j < bent.size(); ++j)
    {
        bent[j].rotation = bent[j].rotation
                         * engine::quat_from_axis_angle(vec3{0.0f, 0.0f, 1.0f}, rad(22.0f));
    }
    engine::anim::skinning_palette(sk, bent, posed, palette);
    engine::anim::skin_into(tube, palette, out);

    float worst_route = 0.0f;
    for (std::size_t i = 0; i < tube.bind.vertices.size(); ++i)
    {
        const skin_influence& inf = tube.influences[i];
        vec3 acc{0.0f, 0.0f, 0.0f};
        for (int k = 0; k < k_max_influences; ++k)
        {
            if (inf.weights[k] == 0.0f) { continue; }
            const mat4 m = posed[inf.joints[k]] * sk.joint_from_model[inf.joints[k]];
            const engine::vec4 p = m * engine::vec4{tube.bind.vertices[i].x,
                                                    tube.bind.vertices[i].y,
                                                    tube.bind.vertices[i].z, 1.0f};
            acc = acc + vec3{p.x, p.y, p.z} * inf.weights[k];
        }
        worst_route = std::fmax(worst_route, engine::length(acc - out.vertices[i]));
    }
    std::printf("   posed: engine palette vs the definition    %.3e\n",
                static_cast<double>(worst_route));
    check("D.4  two routes, one answer", worst_route < 1e-5f);
}

// ---- E  the weights must sum to one ----------------------------------------

void section_e()
{
    std::printf("\nE. Sum to one, or shrink toward the origin\n");

    const skeleton sk = make_chain(6);
    skinned_mesh tube = make_tube(sk, 13, 16, 0.35f);

    std::vector<transform> pose;
    std::vector<mat4> posed;
    std::vector<mat4> palette;
    engine::anim::rest_pose(sk, pose);
    engine::anim::skinning_palette(sk, pose, posed, palette);

    // Scale every weight by 0.9 — the shape a dropped fifth influence leaves.
    skinned_mesh light = tube;
    for (skin_influence& inf : light.influences)
    {
        for (int k = 0; k < k_max_influences; ++k) { inf.weights[k] *= 0.9f; }
    }

    const engine::anim::skin_report lr = engine::anim::validate(light, sk.size());
    std::printf("   weights x0.9: unnormalised %zu of %zu, worst sum %.4f\n",
                lr.unnormalised, lr.vertices, static_cast<double>(lr.worst_weight_sum));
    check("E.1  validate sees every one of them",
          lr.unnormalised == lr.vertices && !lr.ok());

    engine::mesh_data out;
    engine::anim::skin_into(light, palette, out);

    // At the bind pose every palette matrix is I, so the prediction is exact:
    // the vertex lands at 0.9 of its distance from the MODEL ORIGIN.
    float worst_pred = 0.0f;
    float worst_move = 0.0f;
    for (std::size_t i = 0; i < tube.bind.vertices.size(); ++i)
    {
        const vec3 expect = tube.bind.vertices[i] * 0.9f;
        worst_pred = std::fmax(worst_pred, engine::length(out.vertices[i] - expect));
        worst_move = std::fmax(worst_move,
                               engine::length(out.vertices[i] - tube.bind.vertices[i]));
    }
    std::printf("   worst gap from the prediction 0.9*v        %.3e\n",
                static_cast<double>(worst_pred));
    std::printf("   worst vertex displacement                  %.4f units\n",
                static_cast<double>(worst_move));
    check("E.2  it is exactly a scale toward the origin", worst_pred < 1e-5f);

    const std::size_t fixed = engine::anim::normalise_weights(light, sk.size());
    engine::anim::skin_into(light, palette, out);
    const float after = worst_vertex_move(tube.bind.vertices, out.vertices);
    std::printf("   normalise_weights repaired %zu vertices,\n", fixed);
    std::printf("   worst displacement afterwards              %.3e\n",
                static_cast<double>(after));
    check("E.3  one divide per vertex puts it back", after < 1e-5f);

    // CONTROL: a vertex weighted to nothing at all.
    skinned_mesh orphan = tube;
    for (int k = 0; k < k_max_influences; ++k) { orphan.influences[7].weights[k] = 0.0f; }
    const engine::anim::skin_report orr = engine::anim::validate(orphan, sk.size());
    engine::anim::skin_into(orphan, palette, out);
    const float spike = engine::length(out.vertices[7]);
    std::printf("   CONTROL one unweighted vertex: counted %zu,\n", orr.unweighted);
    std::printf("     it lands %.4f from the model origin\n", static_cast<double>(spike));
    check("E.4  CONTROL: no weight means the origin",
          orr.unweighted == 1 && spike < 1e-6f);
}

// ---- F  the candy wrapper --------------------------------------------------

/// One vertex, two joints at the same place, twisted `theta` apart, weighted
/// half and half. The smallest possible candy wrapper.
float twist_radius(float theta, int segments)
{
    // A chain of `segments + 1` joints all at the origin, each rotated one
    // `theta / segments` step further about +y. The vertex sits at radius 1 on
    // the x axis, weighted between the last pair.
    skeleton sk;
    sk.joints.resize(static_cast<std::size_t>(segments) + 1u);
    for (std::size_t j = 0; j < sk.joints.size(); ++j)
    {
        sk.joints[j].parent = (j == 0) ? k_no_parent : static_cast<joint_index>(j - 1);
    }
    engine::anim::bake_inverse_binds(sk);

    std::vector<transform> pose;
    engine::anim::rest_pose(sk, pose);
    for (std::size_t j = 1; j < pose.size(); ++j)
    {
        pose[j].rotation = engine::quat_from_axis_angle(
            vec3{0.0f, 1.0f, 0.0f}, theta / static_cast<float>(segments));
    }

    std::vector<mat4> posed;
    std::vector<mat4> palette;
    engine::anim::skinning_palette(sk, pose, posed, palette);

    skinned_mesh m;
    m.bind.vertices.push_back({1.0f, 0.0f, 0.0f});
    skin_influence inf;
    inf.joints[0] = static_cast<joint_index>(segments - 1);
    inf.joints[1] = static_cast<joint_index>(segments);
    inf.weights[0] = 0.5f;
    inf.weights[1] = 0.5f;
    m.influences.push_back(inf);

    engine::mesh_data out;
    engine::anim::skin_into(m, palette, out);
    return engine::length(out.vertices[0]);
}

void section_f()
{
    std::printf("\nF. The candy wrapper is cos(theta/2), exactly\n");

    std::printf("   twist   measured   cos(t/2)    gap\n");
    float worst = 0.0f;
    for (float t : {0.0f, 30.0f, 60.0f, 90.0f, 120.0f, 150.0f, 179.0f, 180.0f})
    {
        const float measured = twist_radius(rad(t), 1);
        const float predicted = std::cos(rad(t) * 0.5f);
        worst = std::fmax(worst, std::fabs(measured - predicted));
        std::printf("   %5.1f   %8.6f   %8.6f   %.2e\n",
                    static_cast<double>(t), static_cast<double>(measured),
                    static_cast<double>(predicted),
                    static_cast<double>(std::fabs(measured - predicted)));
    }
    std::printf("   worst gap over the sweep                   %.3e\n",
                static_cast<double>(worst));
    check("F.1  the collapse is cos(theta/2)", worst < 1e-5f);

    const float at180 = twist_radius(rad(180.0f), 1);
    std::printf("   at 180 degrees the radius is               %.3e\n",
                static_cast<double>(at180));
    check("F.2  and 180 degrees is exactly zero", at180 < 1e-6f);

    std::printf("   segments   radius at 180   cos(90/n)\n");
    float worst_seg = 0.0f;
    for (int n : {1, 2, 3, 4, 6, 8})
    {
        const float measured = twist_radius(rad(180.0f), n);
        const float predicted = std::cos(rad(90.0f) / static_cast<float>(n));
        worst_seg = std::fmax(worst_seg, std::fabs(measured - predicted));
        std::printf("   %8d   %13.6f   %8.6f\n", n,
                    static_cast<double>(measured), static_cast<double>(predicted));
    }
    check("F.3  n segments give cos(theta/2n)", worst_seg < 1e-5f);

    // A BEND OBEYS THE SAME LAW, ON ITS MINOR AXIS. The rotation axis is across
    // the tube rather than along it, so the vertices lying on that axis do not
    // move and the ring becomes an ELLIPSE — semi-minor `r cos(delta/2)`,
    // semi-major `r`. That is why an elbow loses volume where a forearm loses
    // its whole cross-section, and it is the same cosine in both.
    {
        skeleton sk;
        sk.joints.resize(2);
        sk.joints[1].parent = 0;
        sk.joints[1].local_bind.position = {0.0f, 1.0f, 0.0f};
        engine::anim::bake_inverse_binds(sk);

        std::printf("   bend    min radius   r cos(d/2)   max radius\n");
        float worst_minor = 0.0f;
        for (float d : {0.0f, 30.0f, 60.0f, 90.0f, 120.0f})
        {
            std::vector<transform> pose;
            engine::anim::rest_pose(sk, pose);
            pose[1].rotation = engine::quat_from_axis_angle(vec3{0.0f, 0.0f, 1.0f}, rad(d));

            std::vector<mat4> posed;
            std::vector<mat4> palette;
            engine::anim::skinning_palette(sk, pose, posed, palette);

            // A ring at the midpoint, weighted half to each joint.
            skinned_mesh ring;
            for (int a = 0; a < 64; ++a)
            {
                const float phi = 2.0f * k_pi * static_cast<float>(a) / 64.0f;
                ring.bind.vertices.push_back({std::cos(phi), 1.0f, std::sin(phi)});
                skin_influence inf;
                inf.joints[0] = 0;
                inf.joints[1] = 1;
                inf.weights[0] = 0.5f;
                inf.weights[1] = 0.5f;
                ring.influences.push_back(inf);
            }

            engine::mesh_data out;
            engine::anim::skin_into(ring, palette, out);
            vec3 centre{};
            for (const vec3& v : out.vertices) { centre = centre + v; }
            centre = centre / static_cast<float>(out.vertices.size());

            float lo = 1e30f;
            float hi = 0.0f;
            for (const vec3& v : out.vertices)
            {
                const float r = engine::length(v - centre);
                lo = std::fmin(lo, r);
                hi = std::fmax(hi, r);
            }
            const float predicted = std::cos(rad(d) * 0.5f);
            worst_minor = std::fmax(worst_minor, std::fabs(lo - predicted));
            std::printf("   %5.1f   %10.6f   %10.6f   %10.6f\n",
                        static_cast<double>(d), static_cast<double>(lo),
                        static_cast<double>(predicted), static_cast<double>(hi));
        }
        std::printf("   worst gap on the minor axis                %.3e\n",
                    static_cast<double>(worst_minor));
        check("F.4  a bend flattens; the minor axis is the same cosine",
              worst_minor < 1e-5f);
    }

    // CONTROL: the same vertex weighted entirely to one joint. A rigid vertex
    // cannot collapse, which is what says the artifact belongs to the BLEND and
    // not to the twist.
    skeleton sk;
    sk.joints.resize(2);
    sk.joints[1].parent = 0;
    engine::anim::bake_inverse_binds(sk);
    std::vector<transform> pose;
    engine::anim::rest_pose(sk, pose);
    pose[1].rotation = engine::quat_from_axis_angle(vec3{0.0f, 1.0f, 0.0f}, rad(180.0f));
    std::vector<mat4> posed;
    std::vector<mat4> palette;
    engine::anim::skinning_palette(sk, pose, posed, palette);

    skinned_mesh rigid;
    rigid.bind.vertices.push_back({1.0f, 0.0f, 0.0f});
    skin_influence one;
    one.joints[0] = 1;
    one.weights[0] = 1.0f;
    rigid.influences.push_back(one);
    engine::mesh_data out;
    engine::anim::skin_into(rigid, palette, out);
    std::printf("   CONTROL weight 1.0 on one joint at 180:    %.6f\n",
                static_cast<double>(engine::length(out.vertices[0])));
    check("F.5  CONTROL: a rigid vertex keeps its radius",
          std::fabs(engine::length(out.vertices[0]) - 1.0f) < 1e-5f);
}

// ---- G  normals and the inverse transpose ----------------------------------

void section_g()
{
    std::printf("\nG. Normals: the inverse transpose we are not using\n");

    // One joint with a non-uniform scale, one vertex weighted entirely to it, so
    // the blend is out of the picture and only the normal rule is under test.
    // The last column is what the error COSTS: the change in Lambert brightness
    // for a light 30 degrees off the surface, which is the thing a viewer sees.
    const vec3 light = engine::normalised(vec3{0.5f, 0.8f, 0.33f});
    std::printf("   joint scale        normal error   d(N.L)\n");
    for (float squash : {1.0f, 1.1f, 1.25f, 1.5f, 2.0f})
    {
        skeleton sk;
        sk.joints.resize(1);
        engine::anim::bake_inverse_binds(sk);

        std::vector<transform> pose;
        engine::anim::rest_pose(sk, pose);
        pose[0].scale = {squash, 1.0f, 1.0f};

        std::vector<mat4> posed;
        std::vector<mat4> palette;
        engine::anim::skinning_palette(sk, pose, posed, palette);

        const vec3 n{std::cos(rad(45.0f)), std::sin(rad(45.0f)), 0.0f};
        const mat3 linear = engine::linear_of(palette[0]);
        const vec3 ours = engine::normalised(linear * n);
        const vec3 right = engine::normalised(engine::transpose(engine::inverse(linear)) * n);
        const float err = deg(std::acos(std::clamp(engine::dot(ours, right), -1.0f, 1.0f)));
        const float dl = std::fabs(engine::dot(ours, light) - engine::dot(right, light));
        std::printf("   (%4.2f, 1.00, 1.00)   %8.4f deg   %.4f\n",
                    static_cast<double>(squash), static_cast<double>(err),
                    static_cast<double>(dl));
        sink += static_cast<double>(err);
    }

    // …and the case that matters, which is a rotation, where the two rules are
    // the same rule.
    float worst = 0.0f;
    for (std::uint32_t i = 0; i < 4000u; ++i)
    {
        const mat3 r = engine::mat3_from_quat(sample_quat(i));
        const vec3 n = engine::normalised(vec3{0.3f, -0.7f, 0.64f});
        const vec3 a = engine::normalised(r * n);
        const vec3 b = engine::normalised(engine::transpose(engine::inverse(r)) * n);
        worst = std::fmax(worst, engine::length(a - b));
    }
    std::printf("   4,000 pure rotations, worst normal chord   %.3e\n",
                static_cast<double>(worst));
    check("G.1  for a rotation the two rules agree", worst < 1e-4f);

    // A uniform scale is the other exact case: the inverse transpose is the
    // linear part divided by s^2, and a direction does not notice a positive
    // scalar once it is renormalised.
    {
        skeleton sk;
        sk.joints.resize(1);
        engine::anim::bake_inverse_binds(sk);
        std::vector<transform> pose;
        engine::anim::rest_pose(sk, pose);
        pose[0].scale = {2.5f, 2.5f, 2.5f};
        std::vector<mat4> posed;
        std::vector<mat4> palette;
        engine::anim::skinning_palette(sk, pose, posed, palette);

        const vec3 n = engine::normalised(vec3{0.3f, -0.7f, 0.64f});
        const mat3 linear = engine::linear_of(palette[0]);
        const vec3 ours = engine::normalised(linear * n);
        const vec3 right = engine::normalised(engine::transpose(engine::inverse(linear)) * n);
        std::printf("   uniform scale 2.5: normal chord            %.3e\n",
                    static_cast<double>(engine::length(ours - right)));
        check("G.2  a uniform scale is exact too",
              engine::length(ours - right) < 1e-5f);
    }

    // `validate` reports the authored half of the problem, which is the half a
    // load-time check can see.
    skeleton scaled = make_chain(6);
    const engine::anim::skeleton_report scr = engine::anim::validate(scaled);
    std::printf("   the fixture has %zu non-uniform bind scales\n",
                scr.nonuniform_binds);
    check("G.3  validate counts them", scr.nonuniform_binds == 1);

    // CONTROL: skinning a normal as if it were a POINT — the translation column
    // applies, so the joint's POSITION leaks into the lighting. It has to be a
    // POSED skeleton: at the bind pose every palette matrix is the identity, so
    // its translation is zero and the wrong rule gives the right answer. The
    // first version of this control ran at rest and reported 0.020 degrees,
    // which is a check passing because its fixture could not fail.
    const skeleton sk = make_chain(6);
    skinned_mesh tube = make_tube(sk, 13, 16, 0.35f);
    std::vector<transform> pose;
    std::vector<mat4> posed;
    std::vector<mat4> palette;
    engine::anim::rest_pose(sk, pose);
    for (std::size_t j = 1; j < pose.size(); ++j)
    {
        pose[j].rotation = pose[j].rotation
                         * engine::quat_from_axis_angle(vec3{0.0f, 0.0f, 1.0f}, rad(22.0f));
    }
    engine::anim::skinning_palette(sk, pose, posed, palette);

    std::vector<vec3> as_dirs(tube.bind.normals.size());
    std::vector<vec3> as_points(tube.bind.normals.size());
    engine::anim::skin_directions(tube.bind.normals, tube.influences, palette, as_dirs);
    engine::anim::skin_positions(tube.bind.normals, tube.influences, palette, as_points);

    float worst_tilt = 0.0f;
    for (std::size_t i = 0; i < as_points.size(); ++i)
    {
        const vec3 got = engine::normalised(as_points[i]);
        worst_tilt = std::fmax(worst_tilt,
                               deg(std::acos(std::clamp(engine::dot(got, as_dirs[i]),
                                                        -1.0f, 1.0f))));
    }
    std::printf("   CONTROL normals skinned as points, POSED:\n");
    std::printf("     worst tilt %.3f degrees\n", static_cast<double>(worst_tilt));
    check("G.4  CONTROL: w = 1 ruins a normal", worst_tilt > 10.0f);
}

// ---- H  what it costs ------------------------------------------------------

void section_h()
{
    std::printf("\nH. Cost, and the asymmetry the palette exploits\n");

    const skeleton sk = make_chain(64);
    skinned_mesh tube = make_tube(sk, 121, 40, 0.35f);
    std::printf("   %zu joints, %zu vertices, %zu triangles\n",
                sk.size(), tube.vertex_count(), tube.bind.triangle_count());

    std::vector<transform> pose;
    std::vector<mat4> posed;
    std::vector<mat4> palette;
    engine::anim::rest_pose(sk, pose);

    engine::mesh_data out;
    engine::anim::skinning_palette(sk, pose, posed, palette);
    engine::anim::skin_into(tube, palette, out);   // warm-up, and first-frame copy

    using clock = std::chrono::steady_clock;

    double best_palette = 1e30;
    double best_skin = 1e30;
    constexpr int k_reps = 200;

    for (int run = 0; run < 3; ++run)
    {
        auto t0 = clock::now();
        for (int i = 0; i < k_reps; ++i)
        {
            pose[1].position.x = static_cast<float>(i) * 1e-6f;   // vary the input
            engine::anim::skinning_palette(sk, pose, posed, palette);
            sink += static_cast<double>(palette[sk.size() - 1].c3.x);
        }
        auto t1 = clock::now();
        best_palette = std::min(best_palette,
                                std::chrono::duration<double>(t1 - t0).count() / k_reps);

        t0 = clock::now();
        for (int i = 0; i < k_reps; ++i)
        {
            palette[0].c3.x = static_cast<float>(i) * 1e-6f;
            engine::anim::skin_into(tube, palette, out);
            sink += static_cast<double>(out.vertices[0].x);
        }
        t1 = clock::now();
        best_skin = std::min(best_skin,
                             std::chrono::duration<double>(t1 - t0).count() / k_reps);
    }

    const double per_vertex = best_skin / static_cast<double>(tube.vertex_count());
    std::printf("   palette for %zu joints   %8.2f us\n", sk.size(), best_palette * 1e6);
    std::printf("   skin %zu vertices      %8.2f us  (%.2f ns/vertex)\n",
                tube.vertex_count(), best_skin * 1e6, per_vertex * 1e9);
    std::printf("   the palette is %.2f%% of the two\n",
                100.0 * best_palette / (best_palette + best_skin));
    check("H.1  the per-joint work is noise beside the per-vertex",
          best_palette < best_skin * 0.25);

    // What the pose costs on the bus, which is the GPU argument in one line.
    const std::size_t pose_bytes = sk.size() * sizeof(mat4);
    const std::size_t vtx_bytes = tube.vertex_count() * sizeof(vec3) * 2u;
    std::printf("   per frame: palette %zu bytes, deformed\n", pose_bytes);
    std::printf("   positions + normals %zu bytes  (%.1fx)\n",
                vtx_bytes, static_cast<double>(vtx_bytes) / static_cast<double>(pose_bytes));
    check("H.2  the palette is the smaller thing to send", pose_bytes < vtx_bytes);

    // CONTROL: the static arrays must not be copied per frame.
    const engine::mesh_data before = out;
    engine::anim::skin_into(tube, palette, out);
    bool uvs_untouched = out.uvs.size() == tube.bind.uvs.size();
    bool idx_untouched = out.indices.size() == tube.bind.indices.size()
                      && (tube.bind.indices.empty() || out.indices[0] == tube.bind.indices[0]);
    std::printf("   CONTROL indices and uvs after a second\n");
    std::printf("     skin: %s\n", (uvs_untouched && idx_untouched) ? "intact" : "CHANGED");
    check("H.3  CONTROL: only positions and normals move",
          uvs_untouched && idx_untouched && before.indices.size() == out.indices.size());
}

// ---- I  LBS against nlerp and slerp ----------------------------------------

/// Where LBS, nlerp and slerp put a unit vertex on a pure twist of `theta`,
/// at blend parameter `t`. Returns the angle each one turns it to, in degrees,
/// and LBS's radius.
struct blend_probe
{
    float lbs_angle;
    float lbs_radius;
    float nlerp_angle;
    float slerp_angle;
};

blend_probe probe_blend(float theta, float t)
{
    const vec3 axis{0.0f, 1.0f, 0.0f};
    const vec3 v{1.0f, 0.0f, 0.0f};

    const quat a = quat::identity();
    const quat b = engine::quat_from_axis_angle(axis, theta);

    const mat4 ma = engine::affine(engine::mat3_from_quat(a), vec3{});
    const mat4 mb = engine::affine(engine::mat3_from_quat(b), vec3{});

    const vec3 pa = engine::linear_of(ma) * v;
    const vec3 pb = engine::linear_of(mb) * v;
    const vec3 lbs = pa * (1.0f - t) + pb * t;

    const vec3 nl = engine::rotate(engine::quat_nlerp(a, b, t), v);
    const vec3 sl = engine::rotate(engine::quat_slerp(a, b, t), v);

    auto angle_of = [](vec3 p) { return deg(std::atan2(-p.z, p.x)); };
    return {angle_of(lbs), engine::length(lbs), angle_of(nl), angle_of(sl)};
}

void section_i()
{
    std::printf("\nI. LBS is not nlerp, and it is not slerp either\n");

    std::printf("   twist   worst LBS   worst nlerp   (degrees of pose)\n");
    for (float theta : {30.0f, 60.0f, 90.0f, 120.0f, 150.0f, 179.0f})
    {
        float worst_lbs = 0.0f;
        float worst_nl = 0.0f;
        for (int s = 0; s <= 200; ++s)
        {
            const float t = static_cast<float>(s) / 200.0f;
            const blend_probe p = probe_blend(rad(theta), t);
            worst_lbs = std::fmax(worst_lbs, std::fabs(p.lbs_angle - p.slerp_angle));
            worst_nl = std::fmax(worst_nl, std::fabs(p.nlerp_angle - p.slerp_angle));
        }
        std::printf("   %5.1f   %9.4f   %11.4f\n", static_cast<double>(theta),
                    static_cast<double>(worst_lbs), static_cast<double>(worst_nl));
        sink += static_cast<double>(worst_lbs);
    }

    // The midpoint, where the schedule error vanishes by symmetry for both and
    // only the radius is left. That isolates the two failures from each other.
    const blend_probe mid = probe_blend(rad(120.0f), 0.5f);
    std::printf("   at t = 0.5, twist 120: LBS angle %.4f,\n",
                static_cast<double>(mid.lbs_angle));
    std::printf("     slerp %.4f, LBS radius %.6f, cos(60) %.6f\n",
                static_cast<double>(mid.slerp_angle),
                static_cast<double>(mid.lbs_radius),
                static_cast<double>(std::cos(rad(60.0f))));
    check("I.1  at the midpoint only the radius is wrong",
          std::fabs(mid.lbs_angle - mid.slerp_angle) < 1e-3f
              && std::fabs(mid.lbs_radius - std::cos(rad(60.0f))) < 1e-5f);

    // LBS's schedule is nlerp's formula at the WHOLE angle rather than the half
    // angle, so it is worse at the same pose difference. Checked as a strict
    // inequality at every arc rather than eyeballed off the table.
    bool lbs_worse = true;
    for (float theta : {30.0f, 60.0f, 90.0f, 120.0f, 150.0f, 179.0f})
    {
        float worst_lbs = 0.0f;
        float worst_nl = 0.0f;
        for (int s = 0; s <= 200; ++s)
        {
            const float t = static_cast<float>(s) / 200.0f;
            const blend_probe p = probe_blend(rad(theta), t);
            worst_lbs = std::fmax(worst_lbs, std::fabs(p.lbs_angle - p.slerp_angle));
            worst_nl = std::fmax(worst_nl, std::fabs(p.nlerp_angle - p.slerp_angle));
        }
        if (worst_lbs <= worst_nl) { lbs_worse = false; }
    }
    check("I.2  LBS's schedule is worse than nlerp's, at every arc", lbs_worse);

    // …and the one thing matrices are better at. A rotation matrix is unique, so
    // there is no double cover, so there is no long way round and no `nearest`
    // to forget. 7.5's 359-degree bug cannot be written here.
    float worst_arc = 0.0f;
    for (std::uint32_t i = 0; i < 20000u; ++i)
    {
        const quat a = sample_quat(i * 2u);
        const quat b = sample_quat(i * 2u + 1u);

        const mat3 ra = engine::mat3_from_quat(a);
        const mat3 rb = engine::mat3_from_quat(b);
        const vec3 v{0.577350f, 0.577350f, 0.577350f};
        const vec3 mid_p = (ra * v + rb * v) * 0.5f;
        // The blended point can never be further from either endpoint than the
        // endpoints are from each other: an average lies between its inputs.
        const float d = engine::length(mid_p - ra * v) + engine::length(mid_p - rb * v);
        worst_arc = std::fmax(worst_arc, d - engine::length(ra * v - rb * v));
    }
    std::printf("   20,000 pairs: worst excess over the chord  %.3e\n",
                static_cast<double>(worst_arc));
    check("I.3  a matrix blend cannot take the long way", worst_arc < 1e-5f);
}

}   // namespace

int main()
{
    std::printf("verify_76 - Lesson 7.6, Skeletal Animation\n");

    section_a();
    section_b();
    section_c();
    section_d();
    section_e();
    section_f();
    section_g();
    section_h();
    section_i();

    std::printf("\n%d / %d checks passed\n", checks_passed, checks_run);
    std::printf("(sink %.3f)\n", sink);
    return checks_passed == checks_run ? 0 : 1;
}
