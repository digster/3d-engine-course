// scratch/verify_812.cpp — every number Lesson 8.12 prints, measured rather
// than asserted.
//
// Build and run:  sh scratch/build_verify_812.sh
//
// Eleven sections, in the lesson's order:
//
//   A  swing and twist
//   B  the swing cone is a limit like any other
//   C  the twist row, and Codman's paradox
//   D  a skeleton with mass
//   E  the first fall, and a bug 8.11 shipped
//   F  what eight sweeps do to a ragdoll
//   G  handing a character to the solver
//   H  when the animation disagrees with the joints
//   I  handing it back
//   J  a return the physics does not fight
//   K  settling, sleeping, and the bill
//
// Sections A to K are the lesson's §3 to §13; the text a section PRINTS uses
// the lesson's numbers, and the comments use the letters.
//
// TWO OF THEM FOUND ENGINE BUGS, and both are fixed in the engine this harness
// links. §E keeps a private copy of 8.11's joint position pass, so the bug it
// found — a violated limit never repaired under split impulse — can still be
// shown against the fix. §J's crate is the other: a moving KINEMATIC body never
// woke a sleeping one (8.10's islands), and the steered character ran straight
// through a crate that had fallen asleep before it arrived.
//
// EVERY SECTION CARRIES A CONTROL, in the two halves 8.10 settled: what would
// the control say if the thing were COMPLETELY BROKEN, and what would it say if
// it were completely FINE. Twelve times in Module 8 a section written to confirm
// a claim has refused it instead, so each one here is written to be allowed to.
//
// THE FIXTURE IS 7.7's HUMANOID, AND 7.7's WALK IS KEPT VERBATIM. The walk was
// built to measure clip storage, and it bends the knees FORWARD and twists the
// T-posed arms about their own axes — neither of which mattered to a sampler.
// A ragdoll's joints refuse both, which makes the walk section G's fixture. The
// motion every other section hands over is a JOG written for this lesson: arms
// down, knees bending the way knees bend, and every animated joint a part, so
// that the only disagreement between the clip and the joints is the one a
// section puts there on purpose.
//
// TIMINGS ARE MINIMA over many repetitions, as in 8.8 through 8.11.

#include <engine/anim/clip.hpp>
#include <engine/anim/skeleton.hpp>
#include <engine/math/euler.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/transform.hpp>
#include <engine/phys/broadphase.hpp>
#include <engine/phys/constraint.hpp>
#include <engine/phys/convex.hpp>
#include <engine/phys/gjk.hpp>
#include <engine/phys/integrate.hpp>
#include <engine/phys/manifold.hpp>
#include <engine/phys/ragdoll.hpp>
#include <engine/phys/rigid_body.hpp>
#include <engine/phys/shape.hpp>
#include <engine/phys/solver.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using engine::cross;
using engine::dot;
using engine::length;
using engine::length_squared;
using engine::mat3;
using engine::mat4;
using engine::normalised;
using engine::quat;
using engine::quat_from_axis_angle;
using engine::rotate;
using engine::transform;
using engine::vec3;
using engine::vec4;

using engine::anim::joint_index;
using engine::anim::k_no_parent;
using engine::anim::skeleton;

using namespace engine::phys;

namespace
{

int g_checks = 0;
int g_failures = 0;

void check(bool ok, const char* what)
{
    ++g_checks;
    if (!ok)
    {
        ++g_failures;
        std::printf("  FAIL  %s\n", what);
    }
}

void rule(const char* title)
{
    std::printf("\n%s\n", title);
    for (int i = 0; i < 70; ++i) { std::putchar('-'); }
    std::putchar('\n');
}

/// The same deterministic generator 8.1–8.11 used, for the same reason.
class rng
{
public:
    explicit rng(std::uint32_t seed) : state_(seed | 1u) {}

    std::uint32_t next()
    {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return state_;
    }

    float unit() { return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f); }
    float signed_unit() { return unit() * 2.0f - 1.0f; }
    float range(float lo, float hi) { return lo + (hi - lo) * unit(); }

    vec3 direction()
    {
        for (;;)
        {
            const vec3 v{signed_unit(), signed_unit(), signed_unit()};
            const float l2 = length_squared(v);
            if (l2 > 1e-4f && l2 <= 1.0f) { return v / std::sqrt(l2); }
        }
    }

    quat rotation() { return quat_from_axis_angle(direction(), range(0.0f, 3.14159f)); }

private:
    std::uint32_t state_;
};

constexpr float k_h = 1.0f / 60.0f;
constexpr double k_pi = 3.14159265358979323846;
constexpr float k_pif = 3.14159265358979323846f;

[[nodiscard]] float rad(float degrees) { return degrees * (k_pif / 180.0f); }
[[nodiscard]] double deg(double radians) { return radians * (180.0 / k_pi); }

using clock_type = std::chrono::steady_clock;

[[nodiscard]] double seconds_since(clock_type::time_point t0)
{
    return std::chrono::duration<double>(clock_type::now() - t0).count();
}

/// A body turned by angular velocity `w` for time `dt`, EXACTLY: the exponential.
[[nodiscard]] quat turned(quat q, vec3 w, float dt)
{
    const float s = length(w);
    if (s < 1e-12f) { return q; }
    return normalised(quat_from_axis_angle(w / s, s * dt) * q);
}

// ---------------------------------------------------------------------------
// The humanoid: 7.7's 23 joints, unchanged
// ---------------------------------------------------------------------------

struct joint_spec
{
    const char* name;
    int parent;
    vec3 offset;
};

const joint_spec k_rig[] = {
    {"root",        -1, {0.00f, 0.00f, 0.00f}},
    {"hips",         0, {0.00f, 0.95f, 0.00f}},
    {"spine1",       1, {0.00f, 0.12f, 0.00f}},
    {"spine2",       2, {0.00f, 0.14f, 0.00f}},
    {"chest",        3, {0.00f, 0.16f, 0.00f}},
    {"neck",         4, {0.00f, 0.18f, 0.00f}},
    {"head",         5, {0.00f, 0.10f, 0.00f}},
    {"clavicle.L",   4, {0.05f, 0.14f, 0.00f}},
    {"upperarm.L",   7, {0.13f, 0.00f, 0.00f}},
    {"forearm.L",    8, {0.28f, 0.00f, 0.00f}},
    {"hand.L",       9, {0.25f, 0.00f, 0.00f}},
    {"clavicle.R",   4, {-0.05f, 0.14f, 0.00f}},
    {"upperarm.R",  11, {-0.13f, 0.00f, 0.00f}},
    {"forearm.R",   12, {-0.28f, 0.00f, 0.00f}},
    {"hand.R",      13, {-0.25f, 0.00f, 0.00f}},
    {"thigh.L",      1, {0.09f, -0.05f, 0.00f}},
    {"shin.L",      15, {0.00f, -0.42f, 0.00f}},
    {"foot.L",      16, {0.00f, -0.41f, 0.00f}},
    {"toe.L",       17, {0.00f, -0.06f, 0.12f}},
    {"thigh.R",      1, {-0.09f, -0.05f, 0.00f}},
    {"shin.R",      19, {0.00f, -0.42f, 0.00f}},
    {"foot.R",      20, {0.00f, -0.41f, 0.00f}},
    {"toe.R",       21, {0.00f, -0.06f, 0.12f}},
};

constexpr std::size_t k_joints = sizeof(k_rig) / sizeof(k_rig[0]);

enum rig_joint : joint_index
{
    j_root = 0, j_hips = 1, j_spine1 = 2, j_spine2 = 3, j_chest = 4, j_neck = 5, j_head = 6,
    j_clav_l = 7, j_upper_l = 8, j_fore_l = 9, j_hand_l = 10,
    j_clav_r = 11, j_upper_r = 12, j_fore_r = 13, j_hand_r = 14,
    j_thigh_l = 15, j_shin_l = 16, j_foot_l = 17, j_toe_l = 18,
    j_thigh_r = 19, j_shin_r = 20, j_foot_r = 21, j_toe_r = 22,
};

[[nodiscard]] skeleton build_rig()
{
    skeleton sk;
    sk.joints.resize(k_joints);
    for (std::size_t j = 0; j < k_joints; ++j)
    {
        engine::anim::joint& jt = sk.joints[j];
        jt.name = k_rig[j].name;
        jt.parent = (k_rig[j].parent < 0) ? k_no_parent : static_cast<joint_index>(k_rig[j].parent);
        jt.local_bind.position = k_rig[j].offset;
    }
    engine::anim::bake_inverse_binds(sk);
    return sk;
}

// ---------------------------------------------------------------------------
// The ragdoll: eleven parts, Dempster's mass fractions (Winter, Table 4.1)
// ---------------------------------------------------------------------------

/// Parts in parent-before-child order. Indices used by name below.
enum rig_part : int
{
    p_pelvis = 0, p_torso, p_head, p_upper_l, p_fore_l, p_upper_r, p_fore_r,
    p_thigh_l, p_shin_l, p_thigh_r, p_shin_r, k_parts
};

constexpr float k_mass = 80.0f;

[[nodiscard]] ragdoll_desc humanoid_desc(ragdoll_exclusion exclusion = ragdoll_exclusion::overlapping)
{
    ragdoll_desc d;
    d.mass = k_mass;
    d.exclusion = exclusion;

    auto part = [&](const char* name, joint_index bone, vec3 from, vec3 to, float r, float fraction) {
        ragdoll_part_desc p;
        p.name = name;
        p.bone = bone;
        p.from = from;
        p.to = to;
        p.radius = r;
        p.mass_fraction = fraction;
        d.parts.push_back(p);
        return &d.parts.back();
    };
    auto cone = [](ragdoll_part_desc* p, vec3 twist, vec3 cone_axis, float swing_deg, float twist_deg) {
        p->link = ragdoll_link::cone_twist;
        p->axis = twist;
        p->cone_axis = cone_axis;
        p->swing = rad(swing_deg);
        p->lower = -rad(twist_deg);
        p->upper = rad(twist_deg);
    };
    auto hinge = [](ragdoll_part_desc* p, vec3 axis, float lo_deg, float hi_deg) {
        p->link = ragdoll_link::hinge;
        p->axis = axis;
        p->lower = rad(lo_deg);
        p->upper = rad(hi_deg);
    };

    // Dempster via Winter, Table 4.1: pelvis 0.142, thorax + abdomen 0.355,
    // head and neck 0.081, upper arm 0.028, forearm and hand 0.022, thigh
    // 0.100, foot and leg 0.061. They sum to exactly one.
    part("pelvis", j_hips, {-0.07f, 0.95f, 0.0f}, {0.07f, 0.95f, 0.0f}, 0.11f, 0.142f);
    cone(part("torso", j_spine1, {0.0f, 1.20f, 0.0f}, {0.0f, 1.40f, 0.0f}, 0.15f, 0.355f),
         {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.2f}, 25.0f, 30.0f);
    cone(part("head", j_neck, {0.0f, 1.64f, 0.0f}, {0.0f, 1.70f, 0.0f}, 0.10f, 0.081f),
         {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.3f}, 35.0f, 60.0f);
    cone(part("upperarm.L", j_upper_l, {0.22f, 1.51f, 0.0f}, {0.42f, 1.51f, 0.0f}, 0.045f, 0.028f),
         {1.0f, 0.0f, 0.0f}, {0.7f, -0.7f, 0.35f}, 85.0f, 60.0f);
    hinge(part("forearm.L", j_fore_l, {0.505f, 1.51f, 0.0f}, {0.74f, 1.51f, 0.0f}, 0.04f, 0.022f),
          {0.0f, -1.0f, 0.0f}, 0.0f, 145.0f);
    cone(part("upperarm.R", j_upper_r, {-0.22f, 1.51f, 0.0f}, {-0.42f, 1.51f, 0.0f}, 0.045f, 0.028f),
         {-1.0f, 0.0f, 0.0f}, {-0.7f, -0.7f, 0.35f}, 85.0f, 60.0f);
    hinge(part("forearm.R", j_fore_r, {-0.505f, 1.51f, 0.0f}, {-0.74f, 1.51f, 0.0f}, 0.04f, 0.022f),
          {0.0f, 1.0f, 0.0f}, 0.0f, 145.0f);
    cone(part("thigh.L", j_thigh_l, {0.09f, 0.83f, 0.0f}, {0.09f, 0.55f, 0.0f}, 0.07f, 0.100f),
         {0.0f, -1.0f, 0.0f}, {0.15f, -1.0f, 0.9f}, 80.0f, 35.0f);
    hinge(part("shin.L", j_shin_l, {0.09f, 0.43f, 0.0f}, {0.09f, 0.12f, 0.0f}, 0.05f, 0.061f),
          {1.0f, 0.0f, 0.0f}, 0.0f, 150.0f);
    cone(part("thigh.R", j_thigh_r, {-0.09f, 0.83f, 0.0f}, {-0.09f, 0.55f, 0.0f}, 0.07f, 0.100f),
         {0.0f, -1.0f, 0.0f}, {-0.15f, -1.0f, 0.9f}, 80.0f, 35.0f);
    hinge(part("shin.R", j_shin_r, {-0.09f, 0.43f, 0.0f}, {-0.09f, 0.12f, 0.0f}, 0.05f, 0.061f),
          {1.0f, 0.0f, 0.0f}, 0.0f, 150.0f);
    return d;
}

// ---------------------------------------------------------------------------
// The motions: a jog written for this lesson, and 7.7's walk, verbatim
// ---------------------------------------------------------------------------

constexpr float k_jog_cycle = 0.7f;    // seconds per stride pair
constexpr float k_jog_speed = 2.5f;    // m/s, along world +z

/// **A jog-shaped function of time**: arms hang and swing, elbows bend the way
/// elbows bend, knees bend backward, and every joint it rotates is a PART —
/// the passengers stay at their bind locals, so the ragdoll's joints (authored
/// at bind) and the clip agree exactly wherever the clip has not been told to
/// disagree. The root does not travel; `jog_model` carries the character
/// forward instead, as a character controller would.
[[nodiscard]] transform jog_pose(std::size_t j, float t)
{
    const float w = 2.0f * k_pif * t / k_jog_cycle;
    transform x;
    x.position = k_rig[j].offset;
    const float s = std::sin(w);

    switch (j)
    {
    case j_hips:
        x.position = x.position + vec3{0.0f, 0.03f * std::sin(2.0f * w), 0.0f};
        x.rotation = engine::quat_y(rad(5.0f) * s);
        break;
    case j_spine1:   // lean into the run, counter-rotate the shoulders
        x.rotation = engine::quat_y(-rad(7.0f) * s) * engine::quat_x(rad(8.0f));
        break;
    case j_neck: x.rotation = engine::quat_x(-rad(6.0f)); break;
    case j_upper_l:  // down 75 degrees, then swing forward/back
        x.rotation = engine::quat_x(rad(30.0f) * s) * engine::quat_z(-rad(75.0f));
        break;
    case j_upper_r:
        x.rotation = engine::quat_x(-rad(30.0f) * s) * engine::quat_z(rad(75.0f));
        break;
    case j_fore_l:
        x.rotation = quat_from_axis_angle({0.0f, -1.0f, 0.0f}, rad(70.0f + 20.0f * std::sin(w + 0.5f)));
        break;
    case j_fore_r:
        x.rotation = quat_from_axis_angle({0.0f, 1.0f, 0.0f}, rad(70.0f - 20.0f * std::sin(w + 0.5f)));
        break;
    case j_thigh_l: x.rotation = engine::quat_x(-rad(20.0f + 30.0f * s)); break;
    case j_thigh_r: x.rotation = engine::quat_x(-rad(20.0f - 30.0f * s)); break;
    case j_shin_l: x.rotation = engine::quat_x(rad(45.0f + 40.0f * std::sin(w - 1.2f))); break;
    case j_shin_r: x.rotation = engine::quat_x(rad(45.0f - 40.0f * std::sin(w - 1.2f))); break;
    default: break;
    }
    return x;
}

/// Where the jogging character stands: forward along +z at `k_jog_speed`.
[[nodiscard]] mat4 jog_model(float t) { return engine::translation(vec3{0.0f, 0.0f, k_jog_speed * t}); }

/// **7.7's walk, verbatim** — `scratch/verify_77.cpp`'s `walk_pose`, including
/// the root's travel. Section G's fixture.
[[nodiscard]] transform walk77_pose(std::size_t j, float t)
{
    const float phase = t;   // 7.7's clip was one second long
    const float w = 2.0f * k_pif * phase;
    transform x;
    x.position = k_rig[j].offset;

    auto swing = [&](float amp_deg, float bias_deg, float offset, vec3 axis) {
        return quat_from_axis_angle(normalised(axis), rad(bias_deg + amp_deg * std::sin(w + offset)));
    };

    switch (j)
    {
    case 0:
        x.position = x.position + vec3{0.0f, 0.035f * std::sin(2.0f * w), 1.2f * phase};
        break;
    case 1: x.rotation = swing(3.0f, 0.0f, 0.0f, {0, 1, 0}); break;
    case 2: x.rotation = swing(2.0f, 1.0f, k_pif, {1, 0, 0}); break;
    case 3: x.rotation = swing(1.5f, 1.0f, k_pif, {1, 0, 0}); break;
    case 4: x.rotation = swing(2.5f, 0.0f, 0.5f, {0, 1, 0}); break;
    case 5: x.rotation = swing(1.0f, 0.0f, 0.0f, {0, 1, 0}); break;
    case 8:  x.rotation = swing(22.0f, 0.0f, k_pif, {1, 0, 0}); break;
    case 9:  x.rotation = swing(14.0f, -18.0f, k_pif * 0.5f, {1, 0, 0}); break;
    case 12: x.rotation = swing(22.0f, 0.0f, 0.0f, {1, 0, 0}); break;
    case 13: x.rotation = swing(14.0f, -18.0f, -k_pif * 0.5f, {1, 0, 0}); break;
    case 15: x.rotation = swing(28.0f, 0.0f, 0.0f, {1, 0, 0}); break;
    case 16: x.rotation = swing(24.0f, -24.0f, -1.1f, {1, 0, 0}); break;
    case 17: x.rotation = swing(16.0f, 0.0f, 2.0f, {1, 0, 0}); break;
    case 19: x.rotation = swing(28.0f, 0.0f, k_pif, {1, 0, 0}); break;
    case 20: x.rotation = swing(24.0f, -24.0f, k_pif - 1.1f, {1, 0, 0}); break;
    case 21: x.rotation = swing(16.0f, 0.0f, k_pif + 2.0f, {1, 0, 0}); break;
    default: break;
    }
    return x;
}

using pose_fn = transform (*)(std::size_t, float);

void pose_at(pose_fn f, float t, std::vector<transform>& out)
{
    out.resize(k_joints);
    for (std::size_t j = 0; j < k_joints; ++j) { out[j] = f(j, t); }
}

/// The bind pose as a motion: everything at rest. For drops.
[[nodiscard]] transform bind_pose(std::size_t j, float) { transform x; x.position = k_rig[j].offset; return x; }

// ---------------------------------------------------------------------------
// The scene: 8.11's, with capsules in the narrow phase and ragdolls in the solve
// ---------------------------------------------------------------------------

/// One placed primitive, stored by value so `as_convex` has an lvalue to view.
/// 8.10's and 8.11's scenes dispatched on spheres and boxes; a ragdoll is made
/// of capsules, and `collide_manifold` takes any convex pair, so this is
/// dispatch rather than geometry — the handover said as much.
struct placed
{
    shape_kind kind = shape_kind::sphere;
    engine::sphere s{};
    obb b{};
    capsule c{};

    [[nodiscard]] convex view() const
    {
        switch (kind)
        {
        case shape_kind::sphere:  return as_convex(s);
        case shape_kind::box:     return as_convex(b);
        case shape_kind::capsule: return as_convex(c);
        }
        return as_convex(s);
    }
};

[[nodiscard]] placed place(const shape& sh, const rigid_body& body)
{
    placed p;
    p.kind = sh.kind;
    switch (sh.kind)
    {
    case shape_kind::sphere:  p.s = world_sphere(sh, body.state.position); break;
    case shape_kind::box:     p.b = world_obb(sh, body.state.position, body.orientation); break;
    case shape_kind::capsule: p.c = world_capsule(sh, body.state.position, body.orientation); break;
    }
    return p;
}

struct scene
{
    struct link
    {
        std::uint32_t a = 0;
        std::uint32_t b = 0;
        joint j{};
    };

    body_world world;
    std::vector<shape> shapes;
    uniform_grid grid;
    manifold_cache cache;
    constraint_solver solver;
    collision_filter filter;

    solver_config cfg{};
    sleep_config sleep{};
    broadphase_config bp{};
    manifold_config mf{};
    contact_material material{0.0f, 0.6f};
    float h = k_h;
    int substeps = 1;

    std::vector<link> links;
    std::vector<ragdoll*> ragdolls;
    std::vector<proxy> proxies;
    std::vector<contact_manifold> manifolds;
    std::vector<std::uint64_t> keys;
    std::vector<std::uint32_t> pair_a;
    std::vector<std::uint32_t> pair_b;

    solver_stats stats{};
    double t_broad = 0.0;
    double t_narrow = 0.0;
    double t_solve = 0.0;

    scene() { bp.cell_size = 0.5f; }

    std::uint32_t add(const rigid_body& b, const shape& s)
    {
        const auto index = static_cast<std::uint32_t>(world.size());
        world.add(b);
        shapes.push_back(s);
        return index;
    }

    [[nodiscard]] rigid_body& body(std::uint32_t i) { return world.bodies()[i]; }
    [[nodiscard]] const rigid_body& body(std::uint32_t i) const { return world.bodies()[i]; }

    std::size_t connect(std::uint32_t a, std::uint32_t b, const joint& j)
    {
        links.push_back(link{a, b, j});
        if (!j.collide_connected) { filter.exclude(a, b); }
        filter.finalize();
        return links.size() - 1;
    }

    void collide_all()
    {
        auto bodies = world.bodies();
        clock_type::time_point t0 = clock_type::now();
        proxies.clear();
        for (std::size_t i = 0; i < bodies.size(); ++i)
        {
            proxies.push_back(proxy{bounds_of(shapes[i], bodies[i].state.position, bodies[i].orientation),
                                    static_cast<std::uint32_t>(i)});
        }
        grid.build(proxies, bp);
        t_broad = seconds_since(t0);

        t0 = clock_type::now();
        cache.begin_frame();
        manifolds.clear();
        keys.clear();
        pair_a.clear();
        pair_b.clear();
        manifolds.reserve(grid.pairs().size());
        for (const broadphase_pair& p : grid.pairs())
        {
            const rigid_body& a = bodies[p.a];
            const rigid_body& b = bodies[p.b];
            if (a.kind != body_kind::dynamic && b.kind != body_kind::dynamic) { continue; }
            if (filter.excluded(p.a, p.b)) { continue; }

            const placed x = place(shapes[p.a], a);
            const placed y = place(shapes[p.b], b);
            contact_manifold m = collide_manifold(x.view(), y.view(), mf);
            if (m.count == 0) { continue; }
            const std::uint64_t key = pair_key(p.a, p.b);
            if (const contact_manifold* previous = cache.find(key)) { carry_impulses(m, *previous); }
            manifolds.push_back(m);
            keys.push_back(key);
            pair_a.push_back(p.a);
            pair_b.push_back(p.b);
        }
        t_narrow = seconds_since(t0);
    }

    void solve(float dt)
    {
        const clock_type::time_point t0 = clock_type::now();
        solver.begin(world.bodies());
        for (std::size_t i = 0; i < manifolds.size(); ++i)
        {
            solver.add(pair_a[i], pair_b[i], manifolds[i], material);
        }
        for (link& l : links) { solver.add(l.a, l.b, l.j); }
        for (ragdoll* r : ragdolls) { add_joints(*r, solver); }
        stats = solver.solve(dt, cfg, sleep);
        t_solve = seconds_since(t0);
        for (std::size_t k = 0; k < manifolds.size(); ++k) { cache.store(keys[k], manifolds[k]); }
        cache.end_frame();
    }

    /// One frame. With `substeps > 1` the whole pipeline runs that many times
    /// at `h / substeps` — 8.11 §14's answer, collision included.
    void step()
    {
        const float dt = h / static_cast<float>(substeps);
        for (int s = 0; s < substeps; ++s)
        {
            cfg.restitution_bias = world.gravity() * dt;
            world.integrate_velocities(dt);
            collide_all();
            solve(dt);
            world.integrate_positions(dt);
        }
    }

    void run(float seconds)
    {
        const int steps = static_cast<int>(seconds / h + 0.5f);
        for (int i = 0; i < steps; ++i) { step(); }
    }
};

std::uint32_t add_floor(scene& s, float half_extent = 40.0f)
{
    return s.add(make_fixed(vec3{0.0f, -0.5f, 0.0f}), box_shape(vec3{half_extent, 0.5f, half_extent}));
}

// ---------------------------------------------------------------------------
// A character: a skeleton, a ragdoll in a scene, and the glue between them
// ---------------------------------------------------------------------------

struct character
{
    skeleton sk = build_rig();
    ragdoll rd{};
    ragdoll_report report{};
    scene* sc = nullptr;
    pose_fn motion = jog_pose;
    bool travels = true;          // does world_from_model move with the jog?
    mat4 base = mat4::identity(); // extra placement, applied before the travel

    std::vector<transform> local;
    std::vector<mat4> posed;
    std::vector<transform> targets;

    [[nodiscard]] mat4 model_at(float t) const { return travels ? base * jog_model(t) : base; }

    [[nodiscard]] std::span<rigid_body> bodies() { return sc->world.bodies(); }
    [[nodiscard]] rigid_body& part(int i) { return sc->body(rd.first_body + static_cast<std::uint32_t>(i)); }

    /// Build the ragdoll and put it in the scene, kinematic, at the pose for `t`.
    /// `filter = false` excludes NOTHING — not even jointed pairs — for §D's
    /// control.
    void spawn_at(scene& s, float t, const ragdoll_desc& desc, bool filter = true)
    {
        sc = &s;
        report = build_ragdoll(sk, desc, rd);
        targets_at(t);
        (void)spawn(rd, s.world, targets);
        for (const ragdoll_part& p : rd.parts) { s.shapes.push_back(p.collider); }
        if (filter) { exclude_pairs(rd, s.filter); }
        s.filter.finalize();
        s.ragdolls.push_back(&rd);
    }

    /// Jog until `t_trip`, then hand over. The whole of a trip.
    void jog_and_trip(float t_trip)
    {
        const int steps = static_cast<int>(t_trip / sc->h + 0.5f);
        for (int i = 0; i < steps; ++i) { animate_to(static_cast<float>(i + 1) * sc->h); }
        trip(static_cast<float>(steps) * sc->h);
    }

    void targets_at(float t)
    {
        pose_at(motion, t, local);
        engine::anim::compose_pose(sk, local, posed);
        part_targets(rd, posed, model_at(t), targets);
    }

    /// One animated frame ending at `t_next`: aim, then step.
    void animate_to(float t_next)
    {
        targets_at(t_next);
        steer(rd, bodies(), targets, sc->h);
        sc->step();
    }

    /// The instant of handoff: the clip's pose at `t` goes to the passengers.
    void trip(float t)
    {
        pose_at(motion, t, local);
        simulate(rd, bodies(), local);
    }

    /// Kinetic energy of the ragdoll's parts, joules.
    [[nodiscard]] double energy()
    {
        double e = 0.0;
        for (int i = 0; i < k_parts; ++i) { e += static_cast<double>(kinetic_energy(part(i))); }
        return e;
    }

    /// Linear momentum of the ragdoll's parts, using their PART masses (valid in
    /// both modes; kinematic bodies carry no mass of their own).
    [[nodiscard]] vec3 momentum()
    {
        vec3 p{};
        for (int i = 0; i < k_parts; ++i) { p = p + part(i).state.velocity * rd.parts[static_cast<std::size_t>(i)].mass; }
        return p;
    }

    [[nodiscard]] vec3 centre_of_mass()
    {
        vec3 c{};
        for (int i = 0; i < k_parts; ++i) { c = c + part(i).state.position * rd.parts[static_cast<std::size_t>(i)].mass; }
        return c / k_mass;
    }
};

} // namespace

namespace
{

// ===========================================================================
// §A  swing and twist
// ===========================================================================
//
// Three claims. (1) `split_swing_twist` is exact: swing * twist rebuilds q, the
// swing's axis is perpendicular to the twist axis, and the swing carries the
// axis where q does. (2) The swing IS two mirrors — the rotor built from the
// axis and the half-way vector — which is the form §C differentiates. (3) Euler
// angles break where arms go and swing–twist does not: perturb a rotation by a
// hair and see how far each split's angles jump. The CONTROL is swing–twist's
// own singularity, a swing of 180 degrees, where it must blow up the same way.

/// The largest change in a split's angles, per radian of perturbation, over
/// small random perturbations of `q`. The split's CONDITION NUMBER, measured.
template <typename Split>
double sensitivity(quat q, Split split, rng& g, int trials = 400)
{
    const float eps = 1e-3f;   // radians
    double worst = 0.0;
    float a0[3];
    split(q, a0);
    for (int t = 0; t < trials; ++t)
    {
        const quat p = normalised(quat_from_axis_angle(g.direction(), eps) * q);
        float a1[3];
        split(p, a1);
        double change = 0.0;
        for (int k = 0; k < 3; ++k)
        {
            double d = std::fabs(static_cast<double>(a1[k] - a0[k]));
            if (d > k_pi) { d = 2.0 * k_pi - d; }
            change = std::max(change, d);
        }
        worst = std::max(worst, change / eps);
    }
    return worst;
}

void section_a()
{
    rule("8.12 section 3 (A)  swing and twist");
    rng g(812001);

    // ---- A.1 the split is exact, and the swing is two mirrors -----------------
    double worst_rebuild = 0.0;
    double worst_perp = 0.0;
    double worst_axis = 0.0;
    double worst_mirror = 0.0;
    for (int n = 0; n < 20000; ++n)
    {
        const quat q = g.rotation();
        const vec3 t = g.direction();
        const swing_twist st = split_swing_twist(q, t);
        const quat back = engine::nearest(q, st.swing * st.twist);
        worst_rebuild = std::max(worst_rebuild,
                                 static_cast<double>(std::fabs(back.w - q.w) + length(back.v - q.v)));
        worst_perp = std::max(worst_perp, static_cast<double>(std::fabs(dot(st.swing.v, t))));
        worst_axis = std::max(worst_axis, static_cast<double>(length(rotate(st.swing, t) - rotate(q, t))));

        // The swing as two mirrors: first the plane perpendicular to t, then the
        // plane perpendicular to the half-way vector between t and where q sends it.
        // Within 1 degree of a 180-degree swing the half-way vector is a
        // difference of nearly opposite vectors and means nothing; that is
        // A.3's control, not this check's business.
        const vec3 c = rotate(q, t);
        if (dot(t, c) < std::cos(rad(179.0f))) { continue; }
        quat m = engine::rotor_from_mirrors(t, normalised(t + c));
        if (m.w < 0.0f) { m = -m; }
        worst_mirror = std::max(worst_mirror,
                                static_cast<double>(std::fabs(m.w - st.swing.w) + length(m.v - st.swing.v)));
    }
    std::printf("  20,000 random rotations and axes, q = swing * twist:\n");
    std::printf("     |swing * twist - q|                 %.3e\n", worst_rebuild);
    std::printf("     |swing.v . axis|  (perpendicular)   %.3e\n", worst_perp);
    std::printf("     |swing(axis) - q(axis)|             %.3e\n", worst_axis);
    std::printf("     |two mirrors - swing|  (swing < 179) %.3e\n", worst_mirror);
    check(worst_rebuild < 1e-5, "A.1 swing * twist rebuilds q");
    check(worst_perp < 1e-5, "A.1 the swing turns about an axis perpendicular to the twist axis");
    check(worst_axis < 1e-5, "A.1 the swing carries the axis where q does");
    // The mirror form divides by |t + c| = 2 cos(phi/2), so float error grows
    // toward 180 degrees — 1.3e-05 at a 179-degree swing is that, not a fault.
    check(worst_mirror < 5e-5, "A.1 the swing is the rotor of two mirrors: t, then the half-way vector");

    // ---- A.2 the worked example -------------------------------------------------
    {
        // An upper arm, bone along +x: twisted 30 degrees about itself, THEN
        // raised 60 degrees toward +y (about +z). q = swing * twist.
        const quat q = engine::quat_z(rad(60.0f)) * engine::quat_x(rad(30.0f));
        const swing_twist st = split_swing_twist(q, {1.0f, 0.0f, 0.0f});
        const double twist = deg(2.0 * std::atan2(st.twist.v.x, st.twist.w));
        const double swing = deg(2.0 * std::atan2(st.swing.v.z, st.swing.w));
        std::printf("\n  Worked: q = rot_z(60) * rot_x(30) = (%.4f, %.4f, %.4f, %.4f)\n", q.w, q.v.x, q.v.y, q.v.z);
        std::printf("     twist = (%.4f, %.4f, %.4f, %.4f)  = %.3f deg about x\n", st.twist.w, st.twist.v.x,
                    st.twist.v.y, st.twist.v.z, twist);
        std::printf("     swing = (%.4f, %.4f, %.4f, %.4f)  = %.3f deg about z\n", st.swing.w, st.swing.v.x,
                    st.swing.v.y, st.swing.v.z, swing);
        check(std::fabs(twist - 30.0) < 1e-3, "A.2 the worked twist is 30 degrees");
        check(std::fabs(swing - 60.0) < 1e-3, "A.2 the worked swing is 60 degrees");
    }

    // ---- A.3 conditioning: Euler against swing–twist, on a raised arm -----------
    //
    // A hanging arm (bone along -y), turned 10 degrees about the vertical and
    // then raised FORWARD by `elev` degrees. In 7.2's YXZ convention a forward
    // raise is PITCH, the middle angle, which locks at 90.
    auto euler_split = [](quat q, float out[3]) {
        const engine::euler_extraction e = engine::euler_from_rotation(engine::mat3_from_quat(q));
        out[0] = e.angles.yaw;
        out[1] = e.angles.pitch;
        out[2] = e.angles.roll;
    };
    auto st_split = [](quat q, float out[3]) {
        // The swing as its rotation vector's two components (a tilt and an
        // azimuth would be singular at zero swing for no reason but the
        // parameterisation), and the twist angle.
        const vec3 bone{0.0f, -1.0f, 0.0f};
        const swing_twist st = split_swing_twist(q, bone);
        const float half = std::atan2(length(st.swing.v), st.swing.w);
        const float s = length(st.swing.v);
        const vec3 rv = s > 1e-12f ? st.swing.v * (2.0f * half / s) : vec3{};
        out[0] = rv.x;
        out[1] = rv.z;
        out[2] = 2.0f * std::atan2(dot(st.twist.v, bone), st.twist.w);
    };
    std::printf("\n  The largest change in any extracted angle, per radian of random\n"
                "  perturbation, for a hanging arm raised forward:\n");
    std::printf("     raised    Euler (yaw/pitch/roll)   swing-twist\n");
    const float elevations[] = {0.0f, 45.0f, 80.0f, 89.0f, 89.9f, 135.0f, 170.0f, 179.0f};
    double euler_at_89_9 = 0.0;
    double st_at_89_9 = 0.0;
    double st_at_179 = 0.0;
    for (const float elev : elevations)
    {
        // Raise first, then turn 10 degrees about the vertical: YXZ's pitch is
        // then exactly -elev and its yaw 10 — a clean three-angle pose.
        const quat q = engine::quat_y(rad(10.0f)) * engine::quat_x(-rad(elev));
        const double se = sensitivity(q, euler_split, g);
        const double ss = sensitivity(q, st_split, g);
        // Past 90 degrees YXZ's pitch has wrapped back (a 135-degree forward
        // raise extracts as pitch 45 with yaw and roll 180), so 1/cos(elev)
        // is not its conditioning there and is not printed.
        char euler_pred[16] = "       -";
        if (elev < 90.0f)
        {
            std::snprintf(euler_pred, sizeof euler_pred, "%8.1f", 1.0 / std::cos(static_cast<double>(rad(elev))));
        }
        std::printf("     %6.1f      %12.1f          %8.2f      1/cos(pitch) %s   1/cos(swing/2) %7.2f\n",
                    static_cast<double>(elev), se, ss, euler_pred,
                    1.0 / std::cos(0.5 * static_cast<double>(rad(elev))));
        if (elev == 89.9f) { euler_at_89_9 = se; st_at_89_9 = ss; }
        if (elev == 179.0f) { st_at_179 = ss; }
    }
    check(euler_at_89_9 > 300.0, "A.3 Euler angles blow up at an arm raised forward (1/cos(pitch))");
    check(st_at_89_9 < 2.0, "A.3 swing-twist does not");
    check(st_at_179 > 100.0, "A.3 CONTROL: swing-twist has its own singularity, at a swing of 180");
}

// ===========================================================================
// §B  the swing cone is a limit like any other
// ===========================================================================
//
// (1) The swing row's J·V is exactly −dφ/dt, against a finite difference over
// random joints. (2) Speculation: a limb swung into its cone overshoots by
// zero, and without speculation by a slice of ω·h uniform on [0,1] — 8.11
// §10's turnstile, on a cone. (3) The rebound off the cone at eight sweeps is
// ρ-driven like a hinge stop's. CONTROL for (1): the same finite difference
// against the row with its sign flipped.

/// The rotation that stands a capsule's body +y along unit `bone`: two
/// mirrors, or a half-turn about x when `bone` is straight down.
[[nodiscard]] quat stand_along(vec3 bone)
{
    const vec3 y{0.0f, 1.0f, 0.0f};
    const vec3 sum = y + bone;
    if (length_squared(sum) < 1e-10f) { return quat{0.0f, vec3{1.0f, 0.0f, 0.0f}}; }
    const quat q = engine::rotor_from_mirrors(y, normalised(sum));
    return q.w < 0.0f ? -q : q;
}

/// A rod — a thin capsule — on a fixed socket at its end, pointing along
/// `bone`, with a cone about `cone_axis`. Returns the scene's link index.
struct cone_rig
{
    scene s;
    std::uint32_t anchor = 0;
    std::uint32_t arm = 0;
    std::size_t link = 0;
};

void build_cone_rig(cone_rig& r, vec3 bone, vec3 cone_axis, float cone_deg, bool speculative,
                    bool centred = false)
{
    r.s.world.set_gravity(vec3{});
    r.s.sleep.enabled = false;
    r.s.cfg.joints.speculative_limits = speculative;
    r.anchor = r.s.add(make_fixed(vec3{}), sphere_shape(0.01f));
    const float half = 0.25f;
    const shape cap = capsule_shape(0.03f, half);
    // Socket at the END (a limb, 8.11's door) or through the CENTRE OF MASS
    // (8.11's turnstile, where an angular row has no pin to converge against).
    rigid_body arm = make_dynamic(centred ? vec3{} : bone * (half + 0.03f), 2.0f);
    arm.orientation = stand_along(bone);
    (void)set_inertia(arm, inertia_of(cap, 2.0f));
    r.arm = r.s.add(arm, cap);
    joint j = make_cone_twist(r.s.body(r.anchor), r.s.body(r.arm), vec3{}, bone, cone_axis);
    j.cone.enabled = true;
    j.cone.swing = rad(cone_deg);
    r.link = r.s.connect(r.anchor, r.arm, j);
}

void section_b()
{
    rule("8.12 section 4 (B)  the swing cone is a limit like any other");
    rng g(812002);

    // ---- B.1 the row is the derivative -----------------------------------------
    double worst_rel = 0.0;
    double worst_flipped = 0.0;
    for (int n = 0; n < 2000; ++n)
    {
        rigid_body a = make_box(g.direction(), 3.0f, {0.2f, 0.3f, 0.1f});
        rigid_body b = make_box(g.direction(), 2.0f, {0.1f, 0.4f, 0.1f});
        a.orientation = g.rotation();
        b.orientation = g.rotation();
        joint j = make_cone_twist(a, b, (a.state.position + b.state.position) * 0.5f, g.direction(), g.direction());
        j.cone.enabled = true;
        j.cone.swing = rad(170.0f);   // never violated, so the row always exists
        a.angular_velocity = g.direction() * g.range(0.5f, 4.0f);
        b.angular_velocity = g.direction() * g.range(0.5f, 4.0f);

        const joint_batch jb = prepare_joint(a, b, j, k_h);
        const jacobian_row* row = nullptr;
        for (int k = 0; k < jb.row_count; ++k)
        {
            if (jb.rows[k].role == static_cast<std::uint8_t>(row_role::swing)) { row = &jb.rows[k]; }
        }
        if (row == nullptr || jb.swing < 0.05f || jb.swing > 3.0f) { continue; }

        const float dt = 1e-3f;
        rigid_body a1 = a;
        rigid_body b1 = b;
        a1.orientation = turned(a.orientation, a.angular_velocity, dt);
        b1.orientation = turned(b.orientation, b.angular_velocity, dt);
        rigid_body a0 = a;
        rigid_body b0 = b;
        a0.orientation = turned(a.orientation, a.angular_velocity, -dt);
        b0.orientation = turned(b.orientation, b.angular_velocity, -dt);
        const double rate = (static_cast<double>(swing_angle(j, a1, b1)) - swing_angle(j, a0, b0)) / (2.0 * dt);
        const double jv = row_velocity(*row, a.state.velocity, a.angular_velocity, b.state.velocity,
                                       b.angular_velocity);
        const double scale = std::max(1.0, std::fabs(rate));
        worst_rel = std::max(worst_rel, std::fabs(jv + rate) / scale);          // J·V = −dφ/dt
        worst_flipped = std::max(worst_flipped, std::fabs(-jv + rate) / scale);
    }
    std::printf("  2,000 random joints, central difference of the swing angle:\n");
    std::printf("     |J.V + dphi/dt| / max(1, |dphi/dt|)    %.3e\n", worst_rel);
    std::printf("     CONTROL, sign flipped                 %.3e\n", worst_flipped);
    check(worst_rel < 2e-3, "B.1 the swing row's J.V is -dphi/dt");
    check(worst_flipped > 0.5, "B.1 CONTROL: the flipped row is not");

    // ---- B.2 speculation: overshoot past a cone ----------------------------------
    //
    // A rod pointing straight down on a 40-degree cone about -y, socketed at its
    // CENTRE OF MASS so the cone row has no pin to fight (that is B.3's), spun
    // about +z toward the edge. Arrival PHASE sampled uniformly (8.11's lesson:
    // sample the phase, not the parameter) by starting the rod at 200 swing
    // angles spread over one step's travel.
    const float cone_deg = 40.0f;
    const float omega = 3.0f;
    double spec_worst = 0.0;
    double reactive_mean = 0.0;
    double reactive_worst = 0.0;
    const int phases = 200;
    for (int k = 0; k < phases; ++k)
    {
        for (int mode = 0; mode < 2; ++mode)
        {
            cone_rig r;
            build_cone_rig(r, {0.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, cone_deg, mode == 0, true);
            // start a little short of the cone, offset by a fraction of one step
            const float start = rad(cone_deg) - 0.5f - omega * k_h * (static_cast<float>(k) + 0.5f) / phases;
            rigid_body& arm = r.s.body(r.arm);
            const quat q = engine::quat_z(start);
            arm.state.position = rotate(q, arm.state.position);
            arm.orientation = q * arm.orientation;
            arm.angular_velocity = vec3{0.0f, 0.0f, omega};
            arm.state.velocity = cross(arm.angular_velocity, arm.state.position);
            double peak = 0.0;
            for (int i = 0; i < 60; ++i)
            {
                r.s.step();
                const float phi = swing_angle(r.s.links[r.link].j, r.s.body(r.anchor), r.s.body(r.arm));
                peak = std::max(peak, static_cast<double>(phi) - rad(cone_deg));
            }
            const double frac = peak / (omega * k_h);
            if (mode == 0) { spec_worst = std::max(spec_worst, peak); }
            else
            {
                reactive_mean += frac / phases;
                reactive_worst = std::max(reactive_worst, frac);
            }
        }
    }
    std::printf("\n  A rod spun at %.0f rad/s into a %.0f-degree cone, 200 arrival phases:\n", omega, cone_deg);
    std::printf("     speculative   worst overshoot    %.2e rad\n", spec_worst);
    std::printf("     reactive      mean overshoot     %.4f of w.h   (uniform on [0,1] has mean 0.5)\n",
                reactive_mean);
    std::printf("     reactive      worst overshoot    %.4f of w.h\n", reactive_worst);
    check(spec_worst < 1e-4, "B.2 a speculative cone overshoots by nothing");
    check(std::fabs(reactive_mean - 0.5) < 0.05, "B.2 a reactive cone overshoots by a uniform slice of a step");

    // ---- B.3 the rebound off a cone, and rho ---------------------------------------
    //
    // The rod is a body on a pin at its end, so the cone row — an ANGULAR row,
    // which sees only I_cm — converges against the pin at rho = m d^2 / I_pivot
    // per sweep, exactly 8.11 §10's door. Measured: the speed the rod bounces
    // back off the cone at, over the speed it arrived at, restitution zero.
    const shape cap = capsule_shape(0.03f, 0.25f);
    const mat3 icm = inertia_of(cap, 2.0f);
    const float d = 0.28f;
    const float rho = lever_ratio(icm.c0.x, 2.0f, d);
    std::printf("\n  Rebound off the cone, restitution zero; rho = %.4f for this rod:\n", rho);
    std::printf("     sweeps   rho^n     rebound\n");
    double rebound8 = 0.0;
    for (const int sweeps : {1, 2, 4, 8, 16, 32})
    {
        cone_rig r;
        build_cone_rig(r, {0.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, cone_deg, true);
        r.s.cfg.velocity_iterations = sweeps;
        rigid_body& arm = r.s.body(r.arm);
        const quat q = engine::quat_z(rad(cone_deg) - 0.3f);
        arm.state.position = rotate(q, arm.state.position);
        arm.orientation = q * arm.orientation;
        arm.angular_velocity = vec3{0.0f, 0.0f, omega};
        arm.state.velocity = cross(arm.angular_velocity, arm.state.position);
        double arrive = 0.0;
        double leave = 0.0;
        for (int i = 0; i < 90; ++i)
        {
            const double before = r.s.body(r.arm).angular_velocity.z;
            r.s.step();
            const double after = r.s.body(r.arm).angular_velocity.z;
            if (arrive == 0.0 && after < 0.5 * before) { arrive = before; }
            leave = std::min(leave, after);
        }
        const double rebound = arrive > 0.0 ? -leave / arrive : 0.0;
        std::printf("     %4d    %.4f    %.4f\n", sweeps, std::pow(static_cast<double>(rho), sweeps), rebound);
        if (sweeps == 8) { rebound8 = rebound; }
    }
    check(rebound8 > 0.3 * std::pow(static_cast<double>(rho), 8) && rebound8 < 3.0 * std::pow(static_cast<double>(rho), 8),
          "B.3 the cone's rebound at eight sweeps is of order rho^8, like a hinge stop's");
}

} // namespace
namespace
{

// ===========================================================================
// §C  the twist row, and Codman's paradox
// ===========================================================================
//
// (1) The twist's rate is (w_b − w_a)·(a1 + b1)/(1 + a1·b1), exactly, at every
// swing — and the two obvious rows, about the bone and about the cone's axis,
// are wrong by an amount that grows with the swing. (2) The error has a name:
// carry a limb round a loop with no spin about itself and its twist changes by
// the solid angle the loop encloses — 2π(1 − cos φ) for a circle at swing φ,
// and a quarter-turn for Codman's octant. (3) What the naive row does in the
// solver: a rod circling the rim of its cone walks straight through its twist
// stop, because the row it is checked with cannot see the twist changing.
// CONTROL for (1): at zero swing all three rows agree, because they are the
// same row.

/// Twist of `q` about unit `axis`, radians, from the swing–twist split.
[[nodiscard]] double twist_of(quat q, vec3 axis)
{
    const swing_twist st = split_swing_twist(q, axis);
    return 2.0 * std::atan2(static_cast<double>(dot(st.twist.v, axis)), static_cast<double>(st.twist.w));
}

/// Carry a body whose bone is `t` at rest round a path of bone directions
/// with NO spin about the bone: each step turns the body by the minimal
/// rotation from the old direction to the new, whose axis is perpendicular to
/// the bone. Returns the accumulated (unwrapped) swing–twist twist.
template <typename Path>
double carry(vec3 t, Path path, int steps)
{
    quat q{};
    vec3 c = t;
    double twist = 0.0;
    double last = 0.0;
    for (int k = 1; k <= steps; ++k)
    {
        const vec3 next = path(static_cast<double>(k) / steps);
        const quat turn = engine::rotor_from_mirrors(c, normalised(c + next));
        q = normalised((turn.w < 0.0f ? -turn : turn) * q);
        c = next;
        const double now = twist_of(q, t);
        double d = now - last;
        if (d > k_pi) { d -= 2.0 * k_pi; }
        if (d < -k_pi) { d += 2.0 * k_pi; }
        twist += d;
        last = now;
    }
    return twist;
}

/// One joint, stepped by a harness-local copy of the solver's joint loop, so
/// that the twist rows can be swapped for the naive ones before solving. The
/// loop is `constraint_solver::solve`'s joint path exactly: prepare, warm
/// start, eight sweeps, three position iterations, write back.
struct solo
{
    rigid_body a{};
    rigid_body b{};
    joint j{};
    joint_config cfg{};
    bool naive = false;
    bool correct = true;   // run the split-impulse position pass
    float h = k_h;

    void step()
    {
        // No gravity and a fixed `a`: the velocity half has nothing to do.
        joint_batch batch = prepare_joint(a, b, j, h, cfg);
        if (naive)
        {
            // The obvious twist rows: about the BONE, as `b` carries it.
            const vec3 b1 = normalised(rotate(b.orientation, j.axis_b));
            for (int k = 0; k < batch.row_count; ++k)
            {
                jacobian_row& row = batch.rows[k];
                const auto role = static_cast<row_role>(row.role);
                if (role != row_role::limit_lower && role != row_role::limit_upper) { continue; }
                const vec3 axis = role == row_role::limit_lower ? b1 : -b1;
                row.angular_a = -axis;
                row.angular_b = axis;
                prepare_row(row, batch.inv_mass_a, batch.inv_inertia_a, batch.inv_mass_b, batch.inv_inertia_b);
                row.impulse = std::clamp(row.impulse, row.lower, row.upper);
            }
        }
        warm_start_joint(a, b, batch);
        for (int s = 0; s < 8; ++s) { (void)solve_joint(a, b, batch, false); }
        pseudo_velocity pa{};
        pseudo_velocity pb{};
        for (int s = 0; correct && s < 3; ++s) { (void)solve_joint_positions(batch, pa, pb); }
        apply_pseudo_velocity(b, pb, h, spin_rule::linearised);
        write_back(batch, j);
        b.state.position = b.state.position + b.state.velocity * h;
        b.orientation = advance_orientation(b.orientation, b.angular_velocity, h, spin_rule::linearised);
    }
};

void section_c()
{
    rule("8.12 section 5 (C)  the twist row, and Codman's paradox");
    rng g(812003);

    // ---- C.1 three candidate rows against the twist's real rate -----------------
    const float edges[] = {0.0f, 1.0f, 30.0f, 60.0f, 90.0f, 120.0f, 150.0f, 175.0f};
    constexpr int bins = 7;
    double err_half[bins] = {};
    double err_bone[bins] = {};
    double err_cone[bins] = {};
    int count[bins] = {};
    double bound_ratio = 0.0;   // |bone row - rate| / (|w_perp| tan(phi/2)), worst
    for (int n = 0; n < 40000; ++n)
    {
        rigid_body a = make_box(g.direction(), 3.0f, {0.2f, 0.3f, 0.1f});
        rigid_body b = make_box(g.direction(), 2.0f, {0.1f, 0.4f, 0.1f});
        a.orientation = g.rotation();
        b.orientation = g.rotation();
        joint j = make_cone_twist(a, b, a.state.position, g.direction(), g.direction());
        // Pull the pose toward small swings half the time so every bin fills.
        if (n % 2 == 0)
        {
            const vec3 a1 = rotate(a.orientation, j.axis_a);
            const vec3 b1 = rotate(b.orientation, j.axis_b);
            const float want = rad(g.range(0.0f, 40.0f));
            const quat pull = engine::rotor_from_mirrors(b1, normalised(b1 + a1));
            const float have = std::atan2(length(cross(a1, b1)), dot(a1, b1));
            if (have > 1e-3f)
            {
                const vec3 axis = normalised(cross(b1, a1));
                b.orientation = normalised(quat_from_axis_angle(axis, have - want) * b.orientation);
            }
            (void)pull;
        }
        j.limit.enabled = true;
        j.limit.lower = -rad(179.0f);
        j.limit.upper = rad(179.0f);
        a.angular_velocity = g.direction() * g.range(0.5f, 4.0f);
        b.angular_velocity = g.direction() * g.range(0.5f, 4.0f);

        const joint_batch jb = prepare_joint(a, b, j, k_h);
        const jacobian_row* lower = nullptr;
        for (int k = 0; k < jb.row_count; ++k)
        {
            if (jb.rows[k].role == static_cast<std::uint8_t>(row_role::limit_lower)) { lower = &jb.rows[k]; }
        }
        if (lower == nullptr) { continue; }
        const double swing = deg(jb.swing);
        int bin = -1;
        for (int k = 0; k < bins; ++k)
        {
            if (swing >= edges[k] && swing < edges[k + 1]) { bin = k; }
        }
        if (bin < 0) { continue; }

        const float dt = 2e-4f;
        rigid_body a1 = a;
        rigid_body b1 = b;
        rigid_body a0 = a;
        rigid_body b0 = b;
        a1.orientation = turned(a.orientation, a.angular_velocity, dt);
        b1.orientation = turned(b.orientation, b.angular_velocity, dt);
        a0.orientation = turned(a.orientation, a.angular_velocity, -dt);
        b0.orientation = turned(b.orientation, b.angular_velocity, -dt);
        double d = static_cast<double>(hinge_angle(j, a1, b1)) - hinge_angle(j, a0, b0);
        if (d > k_pi) { d -= 2.0 * k_pi; }
        if (d < -k_pi) { d += 2.0 * k_pi; }
        const double rate = d / (2.0 * dt);
        if (std::fabs(rate) > 30.0) { continue; }   // too near 180 to difference

        const vec3 w = b.angular_velocity - a.angular_velocity;
        const double half = row_velocity(*lower, a.state.velocity, a.angular_velocity, b.state.velocity,
                                         b.angular_velocity);
        const double bone = dot(w, normalised(rotate(b.orientation, j.axis_b)));
        const double cone = dot(w, normalised(rotate(a.orientation, j.axis_a)));
        const double scale = std::max(1.0, std::fabs(rate));
        {
            // The bone row's error is the swing plane turning: at most
            // |w_perp| tan(phi/2), with w_perp the relative spin across the bone.
            const vec3 b1n = normalised(rotate(b.orientation, j.axis_b));
            const double perp = static_cast<double>(length(w - b1n * dot(w, b1n)));
            const double bound = perp * std::tan(0.5 * static_cast<double>(jb.swing));
            // Only where the bound is large against the difference's own
            // resolution (~1e-3 of the rate): the error is (w_perp . a1) /
            // (1 + cos phi) exactly, and the bound is its maximum over the
            // direction of w_perp, so a noisy rate can nudge it over by noise.
            if (bound > 0.1) { bound_ratio = std::max(bound_ratio, std::fabs(bone - rate) / bound); }
        }
        err_half[bin] = std::max(err_half[bin], std::fabs(half - rate) / scale);
        err_bone[bin] = std::max(err_bone[bin], std::fabs(bone - rate) / scale);
        err_cone[bin] = std::max(err_cone[bin], std::fabs(cone - rate) / scale);
        ++count[bin];
    }
    std::printf("  40,000 random joints: worst |row J.V - dtheta/dt| / max(1, |dtheta/dt|),\n"
                "  the twist's rate by central difference, binned by swing:\n");
    std::printf("     swing (deg)    joints   half-way/cos   the bone    the cone axis\n");
    for (int k = 0; k < bins; ++k)
    {
        std::printf("     %5.0f-%-5.0f   %6d     %.2e     %.2e    %.2e\n", static_cast<double>(edges[k]),
                    static_cast<double>(edges[k + 1]), count[k], err_half[k], err_bone[k], err_cone[k]);
    }
    double half_worst = 0.0;
    for (int k = 0; k < bins; ++k) { half_worst = std::max(half_worst, err_half[k]); }
    std::printf("     the bone row's error over |w_perp| tan(swing/2), worst: %.4f\n", bound_ratio);

    // The control: EXACTLY zero swing, where the three candidate axes coincide.
    double zero_worst = 0.0;
    for (int n = 0; n < 500; ++n)
    {
        rigid_body a = make_box(g.direction(), 3.0f, {0.2f, 0.3f, 0.1f});
        rigid_body b = make_box(g.direction(), 2.0f, {0.1f, 0.4f, 0.1f});
        a.orientation = g.rotation();
        b.orientation = g.rotation();
        const vec3 axis = g.direction();
        joint j = make_cone_twist(a, b, a.state.position, axis);   // cone axis = twist axis
        j.limit.enabled = true;
        j.limit.lower = -rad(179.0f);
        j.limit.upper = rad(179.0f);
        a.angular_velocity = g.direction() * 3.0f;
        b.angular_velocity = g.direction() * 3.0f;
        const joint_batch jb = prepare_joint(a, b, j, k_h);
        for (int k = 0; k < jb.row_count; ++k)
        {
            if (jb.rows[k].role != static_cast<std::uint8_t>(row_role::limit_lower)) { continue; }
            const vec3 w = b.angular_velocity - a.angular_velocity;
            const double half = row_velocity(jb.rows[k], a.state.velocity, a.angular_velocity, b.state.velocity,
                                             b.angular_velocity);
            zero_worst = std::max(zero_worst, std::fabs(half - dot(w, axis)));
        }
    }
    std::printf("     CONTROL, exactly zero swing: |half-way row - bone row|  %.3e\n", zero_worst);
    // Float central difference of a float angle: resolution ~ eps/dt, amplified
    // by the row's own 1/cos(phi/2) toward 180 degrees.
    check(half_worst < 1e-2, "C.1 the half-way row is the twist's rate at every swing, to difference resolution");
    check(err_bone[bins - 2] > 0.2 && err_cone[bins - 2] > 0.2, "C.1 the bone and cone-axis rows are not");
    check(bound_ratio <= 1.01, "C.1 the bone row's error is bounded by |w_perp| tan(swing/2)");
    check(zero_worst < 1e-5, "C.1 CONTROL: at exactly zero swing the rows coincide");

    // ---- C.2 Codman's paradox ---------------------------------------------------------
    const vec3 down{0.0f, -1.0f, 0.0f};
    std::printf("\n  A limb carried round a loop with NO spin about itself:\n");
    std::printf("     loop                              twist after     solid angle\n");
    double codman = 0.0;
    {
        // Codman's octant: down -> forward -> out to the side -> down again,
        // three quarter-great-circles.
        auto octant = [&](double s) {
            const double u = s * 3.0;
            const int leg = std::min(2, static_cast<int>(u));
            const double f = (u - leg) * 0.5 * k_pi;
            vec3 p;
            if (leg == 0) { p = vec3{0.0f, static_cast<float>(-std::cos(f)), static_cast<float>(std::sin(f))}; }
            else if (leg == 1) { p = vec3{static_cast<float>(std::sin(f)), 0.0f, static_cast<float>(std::cos(f))}; }
            else { p = vec3{static_cast<float>(std::cos(f)), static_cast<float>(-std::sin(f)), 0.0f}; }
            return p;
        };
        codman = carry(down, octant, 3000);
        std::printf("     Codman: forward, sideways, down    %8.3f deg     %8.3f deg\n", deg(codman), 90.0);
    }
    double worst_circle = 0.0;
    for (const float phi_deg : {30.0f, 60.0f, 90.0f, 120.0f})
    {
        const double phi = rad(phi_deg);
        auto circle = [&](double s) {
            const double a = 2.0 * k_pi * s;
            // a cone of half-angle phi about `down`
            return normalised(vec3{static_cast<float>(std::sin(phi) * std::cos(a)), static_cast<float>(-std::cos(phi)),
                                   static_cast<float>(std::sin(phi) * std::sin(a))});
        };
        // start ON the circle: carry from `down` out to the circle first
        auto path = [&](double s) {
            if (s < 0.25)
            {
                const double f = phi * s / 0.25;
                return normalised(vec3{static_cast<float>(std::sin(f)), static_cast<float>(-std::cos(f)), 0.0f});
            }
            if (s < 0.75) { return circle((s - 0.25) / 0.5); }
            const double f = phi * (1.0 - (s - 0.75) / 0.25);
            return normalised(vec3{static_cast<float>(std::sin(f)), static_cast<float>(-std::cos(f)), 0.0f});
        };
        const double twist = carry(down, path, 8000);
        const double solid = 2.0 * k_pi * (1.0 - std::cos(phi));
        std::printf("     circle at swing %5.1f              %8.3f deg     %8.3f deg\n", static_cast<double>(phi_deg),
                    deg(twist), deg(solid));
        worst_circle = std::max(worst_circle, std::fabs(std::fabs(twist) - solid));
    }
    check(std::fabs(std::fabs(codman) - 0.5 * k_pi) < rad(0.05f), "C.2 Codman's octant leaves a quarter-turn of twist");
    check(worst_circle < rad(0.1f), "C.2 a circle at swing phi leaves 2 pi (1 - cos phi) of twist");

    // ---- C.3 the naive row in the solver ------------------------------------------
    //
    // A rod on a fixed socket, cone 60 degrees, twist range +/-20 degrees, no
    // gravity. Started ON the rim of its cone with no spin about itself and a
    // velocity along the rim: a great circle tangent to a small one leaves it
    // outward, so the cone holds the rod on its rim and it circles. By C.2 its
    // twist must then change by 180 degrees per loop — and the twist stop is at
    // 20.
    std::printf("\n  A rod circling the rim of a 60-degree cone, twist range +/-20 degrees:\n");
    std::printf("     twist rows             position pass   loops   worst |twist|   final swing\n");
    double worst_right = 0.0;
    double worst_naive = 0.0;
    double worst_none = 0.0;
    for (int variant = 0; variant < 3; ++variant)
    {
        solo r;
        r.naive = variant >= 1;
        r.correct = variant != 2;
        r.a = make_fixed(vec3{});
        const shape cap = capsule_shape(0.03f, 0.25f);
        const float cone = rad(60.0f);
        const vec3 bone = normalised(vec3{std::sin(cone), -std::cos(cone), 0.0f});
        r.b = make_dynamic(bone * 0.28f, 2.0f);
        r.b.orientation = stand_along(bone);
        (void)set_inertia(r.b, inertia_of(cap, 2.0f));
        // Author the joint with the rod HANGING (so rest = hanging), then move
        // it to the rim: the cone and twist are measured from the hanging pose.
        rigid_body hanging = r.b;
        hanging.state.position = vec3{0.0f, -0.28f, 0.0f};
        hanging.orientation = stand_along({0.0f, -1.0f, 0.0f});
        r.j = make_cone_twist(r.a, hanging, vec3{}, {0.0f, -1.0f, 0.0f});
        r.j.cone.enabled = true;
        r.j.cone.swing = cone;
        r.j.limit.enabled = true;
        r.j.limit.lower = -rad(20.0f);
        r.j.limit.upper = rad(20.0f);
        // Place the rod on the rim by the minimal rotation from hanging (zero
        // twist), moving along the rim at 4 rad/s about the vertical.
        r.b.orientation = normalised(engine::rotor_from_mirrors({0.0f, -1.0f, 0.0f}, normalised(vec3{0.0f, -1.0f, 0.0f} + bone)) * hanging.orientation);
        if (r.b.orientation.w < 0.0f) { r.b.orientation = -r.b.orientation; }
        const vec3 spin{0.0f, 4.0f, 0.0f};
        // angular velocity about the vertical, minus its component along the
        // bone: no spin about itself
        r.b.angular_velocity = spin - bone * dot(spin, bone);
        r.b.state.velocity = cross(r.b.angular_velocity, r.b.state.position);
        double worst = 0.0;
        double loops = 0.0;
        double last_az = std::atan2(r.b.state.position.z, r.b.state.position.x);
        for (int i = 0; i < 600; ++i)
        {
            r.step();
            worst = std::max(worst, std::fabs(static_cast<double>(hinge_angle(r.j, r.a, r.b))));
            const double az = std::atan2(r.b.state.position.z, r.b.state.position.x);
            double d = az - last_az;
            if (d > k_pi) { d -= 2.0 * k_pi; }
            if (d < -k_pi) { d += 2.0 * k_pi; }
            loops += d / (2.0 * k_pi);
            last_az = az;
        }
        std::printf("     %-18s     %-13s  %5.2f   %8.2f deg     %6.2f deg\n",
                    r.naive ? "about the bone" : "half-way / cos", r.correct ? "split impulse" : "none",
                    std::fabs(loops), deg(worst), deg(swing_angle(r.j, r.a, r.b)));
        (variant == 0 ? worst_right : (variant == 1 ? worst_naive : worst_none)) = worst;
    }
    // The naive row never sees the twist move — the rod has no spin about
    // itself — so only the position pass, which measures the TRUE twist, pushes
    // back, and it removes beta of the violation per step while the circling adds
    // alpha'(1 - cos phi) h. It settles where the two are equal.
    const double predicted_lag = 4.0 * (1.0 - std::cos(k_pi / 3.0)) * static_cast<double>(k_h) / 0.2;
    std::printf("     predicted lag past the stop, naive rows: alpha'(1 - cos phi) h / beta = %.2f deg\n",
                deg(predicted_lag));
    check(worst_right < rad(20.05f), "C.3 the half-way rows hold the twist at its stop");
    check(std::fabs((worst_naive - rad(20.0f)) / predicted_lag - 1.0) < 0.1,
          "C.3 the bone rows lag past the stop by the drift over beta");
    check(worst_none > rad(170.0f), "C.3 and with no position pass they walk straight through");
}

} // namespace
namespace
{

// ===========================================================================
// §D  a skeleton with mass
// ===========================================================================
//
// (1) What `build_ragdoll` makes of the humanoid: eleven parts, the mass table,
// and the ratios the solver will have to live with. (2) How good a capsule is
// as a model of a limb, against Dempster's measured centres of mass and radii
// of gyration — priced in rho, because rho is what the solver feels. (3) The
// self-collision audit: which pairs overlap at rest, in the bind pose AND over
// the two motions this lesson hands over. (4) Which pairs actually TOUCH when a
// ragdoll falls, over twelve trips — and what the "two links" rule does to
// them. CONTROL: exclude nothing, and the jointed pairs fight.

/// Dempster via Winter, Table 4.1: centre of mass from the PROXIMAL joint and
/// radius of gyration about the centre of mass, both as fractions of the
/// segment's joint-to-joint length.
struct dempster
{
    const char* name;
    int part;
    joint_index proximal;
    joint_index distal;
    double com;
    double gyration;
};

const dempster k_dempster[] = {
    {"upper arm",       p_upper_l, j_upper_l, j_fore_l, 0.436, 0.322},
    {"forearm + hand",  p_fore_l,  j_fore_l,  j_hand_l, 0.682, 0.468},
    {"thigh",           p_thigh_l, j_thigh_l, j_shin_l, 0.433, 0.323},
    {"shin + foot",     p_shin_l,  j_shin_l,  j_foot_l, 0.606, 0.416},
};

/// Every contact the narrow phase reports between two parts of the ragdoll,
/// over a run: which pairs touched, how often, how deep.
struct self_contacts
{
    int frames[k_parts][k_parts] = {};
    double deepest[k_parts][k_parts] = {};
};

void record_self_contacts(const scene& s, const character& c, self_contacts& out)
{
    const std::uint32_t first = c.rd.first_body;
    for (std::size_t m = 0; m < s.manifolds.size(); ++m)
    {
        const std::uint32_t a = s.pair_a[m];
        const std::uint32_t b = s.pair_b[m];
        if (a < first || b < first || a >= first + k_parts || b >= first + k_parts) { continue; }
        int i = static_cast<int>(a - first);
        int j = static_cast<int>(b - first);
        if (i > j) { std::swap(i, j); }
        ++out.frames[i][j];
        for (int k = 0; k < s.manifolds[m].count; ++k)
        {
            out.deepest[i][j] = std::max(out.deepest[i][j], static_cast<double>(s.manifolds[m].points[k].depth));
        }
    }
}

/// The deepest overlap of two parts' capsules right now, metres; zero if apart.
[[nodiscard]] double overlap_of(character& c, int i, int j)
{
    const placed x = place(c.rd.parts[static_cast<std::size_t>(i)].collider, c.part(i));
    const placed y = place(c.rd.parts[static_cast<std::size_t>(j)].collider, c.part(j));
    const contact_manifold m = collide_manifold(x.view(), y.view());
    double d = 0.0;
    for (int k = 0; k < m.count; ++k) { d = std::max(d, static_cast<double>(m.points[k].depth)); }
    return d;
}

[[nodiscard]] bool jointed(const ragdoll& rd, int i, int j)
{
    const ragdoll_part& a = rd.parts[static_cast<std::size_t>(i)];
    const ragdoll_part& b = rd.parts[static_cast<std::size_t>(j)];
    return (b.parent == i && b.kind != ragdoll_link::root) || (a.parent == j && a.kind != ragdoll_link::root);
}

void section_d()
{
    rule("8.12 section 6 (D)  a skeleton with mass");

    // ---- D.1 the build -----------------------------------------------------------
    character c;
    scene s;
    add_floor(s);
    c.spawn_at(s, 0.0f, humanoid_desc());
    const ragdoll_report& r = c.report;
    std::printf("  build_ragdoll on 7.7's 23-joint humanoid, %.0f kg:\n", static_cast<double>(k_mass));
    std::printf("     parts %zu  (hinges %zu, cone-twists %zu, roots %zu)   mass fractions sum to %.4f\n",
                r.parts, r.hinges, r.cone_twists, r.roots, static_cast<double>(r.mass_fraction_sum));
    std::printf("     part          follows       mass kg   link         parent\n");
    for (const ragdoll_part& p : c.rd.parts)
    {
        std::printf("     %-12s  %-12s  %6.2f    %-10s   %s\n", p.name.c_str(), k_rig[p.bone].name,
                    static_cast<double>(p.mass), name_of(p.kind),
                    p.parent >= 0 ? c.rd.parts[static_cast<std::size_t>(p.parent)].name.c_str() : "-");
    }
    const double ratio = c.rd.parts[p_torso].mass / c.rd.parts[p_fore_l].mass;
    std::printf("     heaviest / lightest: torso / forearm = %.1f : 1\n", ratio);
    std::size_t passengers = 0;
    for (const int k : c.rd.part_of_joint) { passengers += k < 0 ? 1u : 0u; }
    std::printf("     passengers (joints that ride on a part): %zu of %zu\n", passengers, k_joints);
    check(r.ok() && r.parts == 11 && r.hinges == 4 && r.cone_twists == 6 && r.roots == 1, "D.1 the report is clean");
    check(std::fabs(r.mass_fraction_sum - 1.0f) < 1e-6f, "D.1 Dempster's fractions sum to one");

    // ---- D.2 a capsule against Dempster ---------------------------------------------
    std::printf("\n  A capsule as a limb, against Dempster (Winter Table 4.1), all as fractions\n"
                "  of the joint-to-joint length L; rho is about the proximal joint:\n");
    std::printf("     segment          L m    CoM: capsule  Dempster   k: capsule  Dempster   rho: capsule  Dempster\n");
    double worst_rho_gap = 0.0;
    for (const dempster& d : k_dempster)
    {
        const ragdoll_part& p = c.rd.parts[static_cast<std::size_t>(d.part)];
        std::vector<transform> rest;
        engine::anim::rest_pose(c.sk, rest);
        std::vector<mat4> bind;
        engine::anim::compose_pose(c.sk, rest, bind);
        const vec3 prox = engine::translation_of(bind[d.proximal]);
        const vec3 dist = engine::translation_of(bind[d.distal]);
        const double L = length(dist - prox);
        const vec3 centre = prox + rotate(quat{}, p.offset);   // bind joint frames are unrotated
        const double com = length(centre - prox) / L;
        const double k_cap = std::sqrt(static_cast<double>(p.inertia.c0.x) / p.mass) / L;   // transverse
        const double rho_cap = com * com / (k_cap * k_cap + com * com);
        const double rho_dem = d.com * d.com / (d.gyration * d.gyration + d.com * d.com);
        std::printf("     %-15s  %.3f   %.3f        %.3f      %.3f      %.3f      %.3f         %.3f\n", d.name, L,
                    com, d.com, k_cap, d.gyration, rho_cap, rho_dem);
        worst_rho_gap = std::max(worst_rho_gap, std::fabs(rho_cap - rho_dem));
    }
    check(worst_rho_gap < 0.15, "D.2 a capsule's rho is within 0.15 of Dempster's on every limb");

    // ---- D.3 the audit at rest ---------------------------------------------------------
    std::printf("\n  Pairs excluded by each rule (55 pairs in all):\n");
    for (const ragdoll_exclusion rule_kind :
         {ragdoll_exclusion::jointed, ragdoll_exclusion::overlapping, ragdoll_exclusion::two_links})
    {
        ragdoll rd;
        const ragdoll_report rr = build_ragdoll(c.sk, humanoid_desc(rule_kind), rd);
        const char* name = rule_kind == ragdoll_exclusion::jointed       ? "jointed"
                           : rule_kind == ragdoll_exclusion::overlapping ? "overlapping (default)"
                                                                         : "two links";
        std::printf("     %-22s  %2zu excluded   (%zu jointed + %zu overlapping + %zu two-links);  "
                    "%zu overlapping pairs left colliding\n",
                    name, rd.excluded.size(), rr.excluded_jointed, rr.excluded_overlapping, rr.excluded_two_links,
                    rr.overlapping_colliding);
    }

    // The closest any NON-jointed pair comes over the bind pose and over a
    // whole cycle of each motion this lesson hands over.
    std::printf("\n  The closest any two unjointed parts come, at rest in each motion:\n");
    double closest_any = 1e9;
    for (int m = 0; m < 3; ++m)
    {
        character d;
        d.motion = m == 0 ? bind_pose : (m == 1 ? jog_pose : walk77_pose);
        d.travels = false;
        scene s2;
        d.spawn_at(s2, 0.0f, humanoid_desc());
        double closest = 1e9;
        int ci = 0;
        int cj = 0;
        for (int step = 0; step < 100; ++step)
        {
            d.targets_at(static_cast<float>(step) * 0.01f);
            for (int i = 0; i < k_parts; ++i)
            {
                for (int j = i + 1; j < k_parts; ++j)
                {
                    if (jointed(d.rd, i, j)) { continue; }
                    const capsule a = world_capsule(d.rd.parts[static_cast<std::size_t>(i)].collider,
                                                    d.targets[static_cast<std::size_t>(i)].position,
                                                    d.targets[static_cast<std::size_t>(i)].rotation);
                    const capsule b = world_capsule(d.rd.parts[static_cast<std::size_t>(j)].collider,
                                                    d.targets[static_cast<std::size_t>(j)].position,
                                                    d.targets[static_cast<std::size_t>(j)].rotation);
                    const gjk_result gr = gjk_distance(as_convex(a), as_convex(b));
                    const double dist = gr.status == gjk_status::separated ? gr.distance : -1.0;
                    if (dist < closest) { closest = dist; ci = i; cj = j; }
                }
            }
        }
        std::printf("     %-18s  %.4f m   (%s and %s)\n", m == 0 ? "bind (T) pose" : (m == 1 ? "the jog" : "7.7's walk"),
                    closest, d.rd.parts[static_cast<std::size_t>(ci)].name.c_str(),
                    d.rd.parts[static_cast<std::size_t>(cj)].name.c_str());
        closest_any = std::min(closest_any, closest);
    }
    check(closest_any > 0.03, "D.3 no unjointed pair comes within 3 cm at rest, in any motion");

    // ---- D.4 which pairs touch in a fall --------------------------------------------------
    //
    // Twelve trips spread evenly over one stride, each followed for three seconds.
    // The narrow phase's own manifolds are the instrument.
    self_contacts touched;
    int falls_touching[k_parts][k_parts] = {};
    for (int k = 0; k < 12; ++k)
    {
        character d;
        scene s2;
        add_floor(s2);
        d.spawn_at(s2, 0.0f, humanoid_desc());
        d.jog_and_trip(1.0f + k_jog_cycle * static_cast<float>(k) / 12.0f);
        self_contacts once;
        for (int i = 0; i < 180; ++i)
        {
            s2.step();
            record_self_contacts(s2, d, once);
        }
        for (int i = 0; i < k_parts; ++i)
        {
            for (int j = i + 1; j < k_parts; ++j)
            {
                touched.frames[i][j] += once.frames[i][j];
                touched.deepest[i][j] = std::max(touched.deepest[i][j], once.deepest[i][j]);
                falls_touching[i][j] += once.frames[i][j] > 0 ? 1 : 0;
            }
        }
    }
    std::printf("\n  Twelve trips from the jog, three seconds each — every pair of parts the\n"
                "  narrow phase found touching:\n");
    std::printf("     pair                        links   falls   frames   deepest mm   two-links rule\n");
    int two_link_pairs_that_touch = 0;
    for (int i = 0; i < k_parts; ++i)
    {
        for (int j = i + 1; j < k_parts; ++j)
        {
            if (falls_touching[i][j] == 0) { continue; }
            int links = 0;
            {
                // links between i and j in the part tree
                std::vector<int> up_i;
                for (int a = i; a >= 0; a = c.rd.parts[static_cast<std::size_t>(a)].parent) { up_i.push_back(a); }
                int depth_j = 0;
                for (int b = j; b >= 0; b = c.rd.parts[static_cast<std::size_t>(b)].parent, ++depth_j)
                {
                    const auto it = std::find(up_i.begin(), up_i.end(), b);
                    if (it != up_i.end())
                    {
                        links = static_cast<int>(it - up_i.begin()) + depth_j;
                        break;
                    }
                }
            }
            char label[64];
            std::snprintf(label, sizeof label, "%s / %s", c.rd.parts[static_cast<std::size_t>(i)].name.c_str(),
                          c.rd.parts[static_cast<std::size_t>(j)].name.c_str());
            std::printf("     %-28s  %d      %2d     %5d     %6.1f      %s\n", label, links, falls_touching[i][j],
                        touched.frames[i][j], touched.deepest[i][j] * 1000.0, links <= 2 ? "EXCLUDED" : "collides");
            two_link_pairs_that_touch += links <= 2 ? 1 : 0;
        }
    }
    check(two_link_pairs_that_touch > 0, "D.4 pairs the two-links rule would exclude really do touch in falls");

    // The same twelve trips under each rule. The deepest instant is NOT the
    // instrument: a forearm swung at several metres a second arrives up to v.h
    // deep in one step whatever the rule, which is 8.10 §6's arrival depth and
    // this engine's missing speculative contacts. What separates the rules is
    // whether the overlap PERSISTS — frames spent more than 2 cm deep, and how
    // deep the pair lies once the ragdoll has come to rest.
    std::printf("\n  Those same pairs, over the same twelve falls, under each rule:\n");
    std::printf("     rule                     deepest instant   frames > 20 mm   deepest at rest\n");
    double rest_two = 0.0;
    double rest_default = 0.0;
    int deep_two = 0;
    int deep_default = 0;
    for (int rule_pass = 0; rule_pass < 2; ++rule_pass)
    {
        double deepest = 0.0;
        double at_rest = 0.0;
        int deep_frames = 0;
        for (int k = 0; k < 12; ++k)
        {
            character d;
            scene s2;
            add_floor(s2);
            d.spawn_at(s2, 0.0f, humanoid_desc(rule_pass == 0 ? ragdoll_exclusion::two_links
                                                               : ragdoll_exclusion::overlapping));
            d.jog_and_trip(1.0f + k_jog_cycle * static_cast<float>(k) / 12.0f);
            for (int i = 0; i < 180; ++i)
            {
                s2.step();
                double worst_now = 0.0;
                for (int a = 0; a < k_parts; ++a)
                {
                    for (int b = a + 1; b < k_parts; ++b)
                    {
                        if (jointed(d.rd, a, b) || falls_touching[a][b] == 0) { continue; }
                        worst_now = std::max(worst_now, overlap_of(d, a, b));
                    }
                }
                deepest = std::max(deepest, worst_now);
                deep_frames += worst_now > 0.02 ? 1 : 0;
                if (i == 179) { at_rest = std::max(at_rest, worst_now); }
            }
        }
        std::printf("     %-22s   %8.1f mm        %6d          %8.1f mm\n",
                    rule_pass == 0 ? "two links" : "overlapping (default)", deepest * 1000.0, deep_frames,
                    at_rest * 1000.0);
        (rule_pass == 0 ? rest_two : rest_default) = at_rest;
        (rule_pass == 0 ? deep_two : deep_default) = deep_frames;
    }
    // The default's residue is not the filter's: every pair left overlapping at
    // rest under it is a forearm UNDER the chest, the normal pointing straight
    // down — a 28 kg torso lying on a 1.76 kg arm on the floor, a 16:1 stack
    // that eight sweeps cannot hold. §F measures it on its own.
    check(deep_two > 2 * deep_default, "D.4 under the two-links rule limbs stay inside the body three times as long");
    check(rest_two > 2.5 * rest_default, "D.4 and come to rest deep inside it");

    // ---- D.5 the control: exclude nothing ---------------------------------------------
    std::printf("\n  After a trip and four seconds, by what the filter excludes:\n");
    std::printf("     filter                  KE J       worst joint gap mm   asleep\n");
    double ke_none = 0.0;
    double ke_default = 0.0;
    for (int variant = 0; variant < 2; ++variant)
    {
        character d;
        scene s2;
        add_floor(s2);
        d.spawn_at(s2, 0.0f, humanoid_desc(), variant == 1);
        d.jog_and_trip(1.0f);
        for (int i = 0; i < 240; ++i) { s2.step(); }
        const joint_error e = worst_joint_error(d.rd, d.bodies());
        const bool asleep = d.part(p_pelvis).sleeping;
        std::printf("     %-22s  %8.4f     %8.2f            %s\n", variant == 0 ? "nothing (control)" : "overlapping (default)",
                    d.energy(), static_cast<double>(e.linear) * 1000.0, asleep ? "yes" : "no");
        (variant == 0 ? ke_none : ke_default) = d.energy();
    }
    check(ke_none > 10.0 * std::max(ke_default, 1e-6), "D.5 CONTROL: with jointed pairs colliding it never settles");
}

// ===========================================================================
// §E  the first fall, and a bug 8.11 shipped
// ===========================================================================
//
// The first ragdoll this lesson dropped settled with a knee hyperextended by
// nine degrees, and a limit held under load turned out never to be corrected
// under split impulse at all: the far stop's row cancelled the near stop's
// correction in the position pass. The engine is fixed; this section keeps a
// PRIVATE COPY of 8.11's position pass (8.10's device for its step split) and
// runs both against one isolated knee. CONTROL: Baumgarte, whose correction is
// in the velocity pass and never had the bug.

/// 8.11's `solve_joint_positions`, verbatim in behaviour: every non-motor row
/// solved against `row.bias`, then the angular block, then the point block.
float solve_joint_positions_811(joint_batch& batch, pseudo_velocity& pa, pseudo_velocity& pb)
{
    float worst = 0.0f;
    for (int i = 0; i < batch.row_count; ++i)
    {
        jacobian_row& row = batch.rows[i];
        if (row.role == static_cast<std::uint8_t>(row_role::motor)) { continue; }
        const bool one_sided = row.lower == 0.0f;
        worst = std::max(worst, one_sided ? std::max(0.0f, -row.error) : std::abs(row.error));
        (void)solve_row_position(row, batch.inv_mass_a, batch.inv_mass_b, pa, pb);   // row.bias: the bug
    }
    if (batch.angular_block)
    {
        const vec3 w_rel = pb.angular - pa.angular;
        const float r0 = batch.angular_bias[0] - dot(batch.perp[0], w_rel);
        const float r1 = batch.angular_bias[1] - dot(batch.perp[1], w_rel);
        const float l0 = batch.angular_mass[0] * r0 + batch.angular_mass[1] * r1;
        const float l1 = batch.angular_mass[1] * r0 + batch.angular_mass[2] * r1;
        batch.angular_pseudo_impulse[0] += l0;
        batch.angular_pseudo_impulse[1] += l1;
        pa.angular = pa.angular - batch.inv_inertia_perp_a[0] * l0 - batch.inv_inertia_perp_a[1] * l1;
        pb.angular = pb.angular + batch.inv_inertia_perp_b[0] * l0 + batch.inv_inertia_perp_b[1] * l1;
    }
    if (batch.point_block)
    {
        worst = std::max(worst, length(batch.point_error));
        const vec3 cdot = (pb.linear + cross(pb.angular, batch.r_b)) - (pa.linear + cross(pa.angular, batch.r_a));
        const vec3 lambda = batch.point_mass * (batch.point_bias - cdot);
        batch.point_pseudo_impulse = batch.point_pseudo_impulse + lambda;
        pa.linear = pa.linear - lambda * batch.inv_mass_a;
        pa.angular = pa.angular - batch.inv_inertia_a * cross(batch.r_a, lambda);
        pb.linear = pb.linear + lambda * batch.inv_mass_b;
        pb.angular = pb.angular + batch.inv_inertia_b * cross(batch.r_b, lambda);
    }
    return worst;
}

/// A shin on a FIXED thigh, lying straight out behind the knee, so gravity
/// drives it into its hyperextension stop and the stop must hold a steady
/// load. A harness-local copy of the solver's joint path, like §C's `solo`,
/// with gravity and a choice of position pass.
struct knee_rig
{
    rigid_body thigh = make_fixed({0.0f, 1.0f, 0.0f});
    rigid_body shin{};
    joint j{};
    joint_config cfg{};
    bool old_pass = false;
    bool split = true;
    float h = k_h;

    knee_rig()
    {
        const shape cap = capsule_shape(0.05f, 0.155f);
        shin = make_dynamic({0.0f, 1.0f, -0.24f}, 4.88f);
        shin.orientation = engine::quat_x(-rad(90.0f));   // body +y -> world -z: lying backward
        (void)set_inertia(shin, inertia_of(cap, 4.88f));
        j = make_hinge(thigh, shin, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f});
        j.limit.enabled = true;
        j.limit.lower = 0.0f;
        j.limit.upper = rad(150.0f);
    }

    void step()
    {
        shin.state.velocity = shin.state.velocity + vec3{0.0f, -k_gravity, 0.0f} * h;
        joint_batch batch = prepare_joint(thigh, shin, j, h, cfg);
        warm_start_joint(thigh, shin, batch);
        for (int s = 0; s < 8; ++s) { (void)solve_joint(thigh, shin, batch, !split); }
        if (split)
        {
            pseudo_velocity pa{};
            pseudo_velocity pb{};
            for (int s = 0; s < 3; ++s)
            {
                (void)(old_pass ? solve_joint_positions_811(batch, pa, pb) : solve_joint_positions(batch, pa, pb));
            }
            apply_pseudo_velocity(shin, pb, h, spin_rule::linearised);
        }
        write_back(batch, j);
        shin.state.position = shin.state.position + shin.state.velocity * h;
        shin.orientation = advance_orientation(shin.orientation, shin.angular_velocity, h, spin_rule::linearised);
    }
};

void section_e()
{
    rule("8.12 section 7 (E)  the first fall, and a bug 8.11 shipped");

    // ---- E.1 the isolated knee, both position passes ---------------------------------
    std::printf("  A shin on a fixed thigh, loaded into its hyperextension stop by gravity.\n"
                "  Knee angle (degrees; the stop is at 0) at 2, 4, 6, 8 and 10 seconds:\n");
    std::printf("     position pass       warm start   2 s        4 s        6 s        8 s        10 s\n");
    double old_warm[5] = {};
    double new_warm[5] = {};
    double old_cold[5] = {};
    double new_cold[5] = {};
    double baum[5] = {};
    for (int variant = 0; variant < 5; ++variant)
    {
        knee_rig k;
        k.old_pass = variant == 0 || variant == 2;
        k.cfg.warm_start = variant < 2 || variant == 4;
        k.split = variant != 4;
        double* out = variant == 0 ? old_warm : variant == 1 ? new_warm : variant == 2 ? old_cold
                    : variant == 3 ? new_cold : baum;
        for (int i = 0; i < 600; ++i)
        {
            k.step();
            if (i % 120 == 119) { out[i / 120] = deg(hinge_angle(k.j, k.thigh, k.shin)); }
        }
        const char* name = variant == 4 ? "none (Baumgarte)" : (k.old_pass ? "8.11's" : "8.12's");
        std::printf("     %-18s  %-10s", name, k.cfg.warm_start ? "on" : "off");
        for (int t = 0; t < 5; ++t) { std::printf("  %9.4f", out[t]); }
        std::printf("\n");
    }
    check(std::fabs(old_warm[0] - old_warm[4]) < 1e-4 && old_warm[4] < -0.05,
          "E.1 under 8.11's pass a loaded stop's violation is FROZEN, not corrected");
    check(std::fabs(new_warm[4]) < 1e-3, "E.1 under 8.12's it is corrected to nothing");
    check(old_cold[4] < -30.0, "E.1 cold, 8.11's pass lets the stop give way without bound");
    check(new_cold[4] > -3.0 && std::fabs(new_cold[4] - new_cold[3]) < 1e-3,
          "E.1 cold, 8.12's settles at a steady lag");
    check(std::fabs(baum[4]) < 1e-3, "E.1 CONTROL: Baumgarte never had the bug");

    // ---- E.2 what the far stop was doing ---------------------------------------------
    {
        knee_rig k;
        k.old_pass = true;
        for (int i = 0; i < 300; ++i) { k.step(); }
        // one more prepare and position pass, instrumented
        joint_batch batch = prepare_joint(k.thigh, k.shin, k.j, k.h, k.cfg);
        pseudo_velocity pa{};
        pseudo_velocity pb{};
        for (int s = 0; s < 3; ++s) { (void)solve_joint_positions_811(batch, pa, pb); }
        std::printf("\n  Inside 8.11's position pass, five seconds in:\n");
        for (int i = 0; i < batch.row_count; ++i)
        {
            const jacobian_row& row = batch.rows[i];
            std::printf("     %-12s  C = %+.6f rad   bias %.5f   pseudo impulse %+.6f\n",
                        row.role == static_cast<std::uint8_t>(row_role::limit_lower) ? "lower stop" : "upper stop",
                        static_cast<double>(row.error), static_cast<double>(row.bias),
                        static_cast<double>(row.pseudo_impulse));
        }
        std::printf("     the knee's pseudo spin about the hinge after the pass: %+.3e rad/s\n",
                    static_cast<double>(dot(pb.angular, vec3{1.0f, 0.0f, 0.0f})));
        const double lo = batch.rows[0].pseudo_impulse;
        const double hi = batch.rows[1].pseudo_impulse;
        check(lo > 0.0 && std::fabs(lo - hi) < 1e-3 * lo, "E.2 the far stop's pseudo impulse cancels the near stop's");
    }
}

} // namespace
namespace
{

// ===========================================================================
// §F  what eight sweeps do to a ragdoll
// ===========================================================================
//
// 8.11 §14 measured chains: stretch grows with the mass ratio, halves with each
// doubling of sweeps, and eight sub-steps of one sweep beat one step of eight
// by 19x. A ragdoll is a tree of chains with a 16:1 ratio in it. Four
// scenes: (1) the peak joint gap during a fall, (2) the gap at rest after it,
// (3) the whole character HANGING FROM ONE HAND — 80 kg through a 1.76 kg
// forearm, a 45:1 load — and (4) the stack nobody built: the chest lying on
// the forearm on the floor. Each at 8, 16 and 32 sweeps, and at eight
// sub-steps of one sweep (the same joint visits as one step of eight).
// CONTROL: 32 sub-steps of 4 sweeps, far more work than anything shipped.

struct budget
{
    const char* name;
    int sweeps;
    int substeps;
};

const budget k_budgets[] = {
    {"8 sweeps (shipped)", 8, 1},
    {"16 sweeps", 16, 1},
    {"32 sweeps", 32, 1},
    {"8 sub-steps x 1", 1, 8},
    {"CONTROL 32 x 4", 4, 32},
};

void apply(scene& s, const budget& b)
{
    s.cfg.velocity_iterations = b.sweeps;
    s.substeps = b.substeps;
}

void section_f()
{
    rule("8.12 section 8 (F)  what eight sweeps do to a ragdoll");

    std::printf("  Worst joint gap (mm) and the chest-on-forearm overlap (mm), by budget:\n");
    std::printf("     budget               fall: peak   rest     hanging by a hand   chest on arm\n");
    double peak8 = 0.0;
    double peak_sub = 0.0;
    double hang8 = 0.0;
    double hang_sub = 0.0;
    double hang_ctl = 0.0;
    double crush8 = 0.0;
    double crush_sub = 0.0;
    double crush_ctl = 0.0;
    for (const budget& b : k_budgets)
    {
        // ---- (1) and (2): a trip, three seconds, the peak and the final gap ------
        double peak = 0.0;
        double rest = 0.0;
        for (int k = 0; k < 6; ++k)
        {
            character d;
            scene s;
            add_floor(s);
            d.spawn_at(s, 0.0f, humanoid_desc());
            d.jog_and_trip(1.0f + k_jog_cycle * static_cast<float>(k) / 6.0f);
            apply(s, b);
            for (int i = 0; i < 180; ++i)
            {
                s.step();
                peak = std::max(peak, static_cast<double>(worst_joint_error(d.rd, d.bodies()).linear));
            }
            rest = std::max(rest, static_cast<double>(worst_joint_error(d.rd, d.bodies()).linear));
        }

        // ---- (3): hanging from the left hand, AT REST -----------------------------
        //
        // Released from the T-pose with one hand pinned, the body swings like a
        // pendulum for a long time, and a gap sampled mid-swing measures the
        // swing's phase rather than the solver. So every part is damped
        // (8.1's exact exp(-k h), k = 3/s) and the gap is read after eight
        // seconds, with the character hanging still.
        double hang = 0.0;
        double hang_ke = 0.0;
        {
            character d;
            scene s;
            d.motion = bind_pose;
            d.travels = false;
            d.base = engine::translation(vec3{0.0f, 1.0f, 0.0f});
            d.spawn_at(s, 0.0f, humanoid_desc());
            d.trip(0.0f);
            // the hand: the far end of the forearm's segment
            const rigid_body& fore = d.part(p_fore_l);
            const vec3 hand = fore.state.position + rotate(fore.orientation, vec3{0.0f, 0.1175f, 0.0f});
            const std::uint32_t bar = s.add(make_fixed(hand), sphere_shape(0.01f));
            s.connect(bar, d.rd.first_body + p_fore_l, make_ball_socket(s.body(bar), d.part(p_fore_l), hand));
            for (int i = 0; i < k_parts; ++i)
            {
                d.part(i).damping = 3.0f;
                d.part(i).angular_damping = 3.0f;
            }
            s.sleep.enabled = false;
            apply(s, b);
            for (int i = 0; i < 480; ++i) { s.step(); }
            hang = std::max(static_cast<double>(worst_joint_error(d.rd, d.bodies()).linear),
                            static_cast<double>(measure_joint(s.links[0].j, s.body(bar), d.part(p_fore_l)).linear));
            hang_ke = d.energy();
        }

        // ---- (4): the chest on the arm — fall 5 of §D, which ends face down on it --
        double crush = 0.0;
        {
            character d;
            scene s;
            add_floor(s);
            d.spawn_at(s, 0.0f, humanoid_desc());
            d.jog_and_trip(1.0f + k_jog_cycle * 5.0f / 12.0f);
            apply(s, b);
            for (int i = 0; i < 180; ++i) { s.step(); }
            crush = std::max(overlap_of(d, p_torso, p_fore_l), overlap_of(d, p_torso, p_fore_r));
        }

        std::printf("     %-20s %8.2f   %7.3f      %8.3f  (KE %.1e)   %6.1f\n", b.name, peak * 1000.0,
                    rest * 1000.0, hang * 1000.0, hang_ke, crush * 1000.0);
        if (b.sweeps == 8 && b.substeps == 1) { peak8 = peak; hang8 = hang; crush8 = crush; }
        if (b.sweeps == 1 && b.substeps == 8) { peak_sub = peak; hang_sub = hang; crush_sub = crush; }
        if (b.substeps == 32) { hang_ctl = hang; crush_ctl = crush; }
    }
    check(peak_sub < peak8, "F.1 sub-steps beat sweeps on the peak gap of a fall, for the same visits");
    check(hang_sub < 0.25 * hang8, "F.3 and by a large factor on the character hung from one hand");
    check(hang_ctl <= hang_sub + 1e-5, "F.3 CONTROL: four hundred times the work does no better than sub-steps");
    check(crush_sub < crush8 && crush_ctl < 0.25 * crush8, "F.4 the chest-on-arm stack is an iteration problem");
}

} // namespace
namespace
{

// ===========================================================================
// §G  handing a character to the solver
// ===========================================================================
//
// (1) Steering lands a kinematic body EXACTLY on its target, because the
// velocity it is given is the inverse of the step that will move it: the chord
// for position, the Rodrigues vector for the linearised spin. CONTROL: the
// calculus answer, the logarithm, which misses by θ³/12. (2) The chord is the
// velocity at the step's MIDPOINT, second order — which is what semi-implicit
// Euler's v_n is. (3) The handoff keeps the character's momentum; handing over
// at rest does not, and the character stops in mid-stride. (4) The handed-over
// velocities violate each joint by exactly the centripetal term
// ½h·ω×(ω×r) — the chord of a rotation is not its tangent — predicted and
// measured.

/// The angle between two unit quaternions, radians — by atan2 of the
/// difference rotation, NOT acos of the dot product: acos of a float near 1
/// bottoms out near 7e-4 rad (8.7's floor), which is larger than what G.1 is
/// trying to see. The first draft of this section measured that floor twice
/// and called it a result.
[[nodiscard]] double between(quat a, quat b)
{
    const quat d = a * conjugate(b);
    return 2.0 * std::atan2(static_cast<double>(length(d.v)), std::fabs(static_cast<double>(d.w)));
}

void section_g()
{
    rule("8.12 section 9 (G)  handing a character to the solver");

    // ---- G.1 steering lands exactly --------------------------------------------------
    std::printf("  Two seconds of the jog, steered: how far each body lands from its target:\n");
    std::printf("     angular velocity from        worst position     worst orientation   per-step turn\n");
    double pos_rod = 0.0;
    double ang_rod = 0.0;
    double ang_log = 0.0;
    double turn_max = 0.0;
    for (int variant = 0; variant < 2; ++variant)
    {
        character d;
        scene s;
        d.spawn_at(s, 0.0f, humanoid_desc());
        const spin_rule rule_used = variant == 0 ? spin_rule::linearised : spin_rule::exponential;
        double worst_p = 0.0;
        double worst_q = 0.0;
        for (int i = 0; i < 120; ++i)
        {
            const float t = static_cast<float>(i + 1) * k_h;
            d.targets_at(t);
            for (int k = 0; k < k_parts; ++k)
            {
                turn_max = std::max(turn_max, between(d.part(k).orientation, d.targets[static_cast<std::size_t>(k)].rotation));
            }
            steer(d.rd, d.bodies(), d.targets, s.h, rule_used);   // the world integrates LINEARISED
            s.step();
            for (int k = 0; k < k_parts; ++k)
            {
                worst_p = std::max(worst_p, static_cast<double>(length(d.part(k).state.position
                                                                       - d.targets[static_cast<std::size_t>(k)].position)));
                worst_q = std::max(worst_q, between(d.part(k).orientation, d.targets[static_cast<std::size_t>(k)].rotation));
            }
        }
        std::printf("     %-28s %.2e m        %.2e rad\n",
                    variant == 0 ? "Rodrigues (2/h) v/w" : "CONTROL: logarithm, angle/h", worst_p, worst_q);
        if (variant == 0) { pos_rod = worst_p; ang_rod = worst_q; }
        else { ang_log = worst_q; }
    }
    std::printf("     largest turn in one step: %.4f rad;  theta^3/12 = %.2e rad\n", turn_max,
                turn_max * turn_max * turn_max / 12.0);
    check(pos_rod < 1e-5 && ang_rod < 1e-5, "G.1 the Rodrigues vector and the chord land every body exactly");
    check(ang_log > 10.0 * ang_rod && std::fabs(ang_log / (turn_max * turn_max * turn_max / 12.0) - 1.0) < 0.1,
          "G.1 CONTROL: the logarithm misses by theta^3/12");

    // ---- G.2 the chord is the midpoint velocity ---------------------------------------
    {
        character d;
        scene s;
        d.spawn_at(s, 0.0f, humanoid_desc());
        double to_end = 0.0;
        double to_mid = 0.0;
        for (int i = 0; i < 60; ++i)
        {
            const float t = static_cast<float>(i + 1) * k_h;
            d.animate_to(t);
            // the animation's own velocity at the end and the middle of the step,
            // by a small symmetric difference of the targets
            auto velocity_at = [&](float at) {
                std::vector<transform> lo;
                std::vector<transform> hi;
                const float e = 1e-3f;
                d.targets_at(at - e);
                lo = d.targets;
                d.targets_at(at + e);
                hi = d.targets;
                std::vector<vec3> v(k_parts);
                for (int k = 0; k < k_parts; ++k) { v[static_cast<std::size_t>(k)] = (hi[static_cast<std::size_t>(k)].position - lo[static_cast<std::size_t>(k)].position) / (2.0f * e); }
                return v;
            };
            const std::vector<vec3> v_end = velocity_at(t);
            const std::vector<vec3> v_mid = velocity_at(t - 0.5f * k_h);
            for (int k = 0; k < k_parts; ++k)
            {
                const vec3 chord = d.part(k).state.velocity;
                to_end = std::max(to_end, static_cast<double>(length(chord - v_end[static_cast<std::size_t>(k)])));
                to_mid = std::max(to_mid, static_cast<double>(length(chord - v_mid[static_cast<std::size_t>(k)])));
            }
        }
        std::printf("\n  The chord's velocity against the animation's own, over one second of the jog:\n");
        std::printf("     worst |chord - v(end of step)|      %.4f m/s   (first order: a.h/2)\n", to_end);
        std::printf("     worst |chord - v(middle of step)|   %.4f m/s   (second order)\n", to_mid);
        check(to_mid < 0.1 * to_end, "G.2 the chord is the midpoint velocity, to second order");
    }

    // ---- G.3 the handoff keeps the momentum -------------------------------------------
    std::printf("\n  Tripped mid-stride at 2.5 m/s, handed over two ways:\n");
    std::printf("     velocities           momentum kg m/s   KE J      centre of mass forward in 0.5 s   in 1.5 s\n");
    double p_anim = 0.0;
    double p_chord = 0.0;
    double fwd_chord = 0.0;
    double fwd_zero = 0.0;
    for (int variant = 0; variant < 2; ++variant)
    {
        character d;
        scene s;
        add_floor(s);
        d.spawn_at(s, 0.0f, humanoid_desc());
        d.jog_and_trip(1.0f);
        const vec3 p_before = d.momentum();
        if (variant == 1)
        {
            for (int k = 0; k < k_parts; ++k)
            {
                d.part(k).state.velocity = vec3{};
                d.part(k).angular_velocity = vec3{};
            }
        }
        const vec3 p_after = d.momentum();
        const double ke = d.energy();
        const vec3 c0 = d.centre_of_mass();
        s.run(0.5f);
        const double f05 = d.centre_of_mass().z - c0.z;
        s.run(1.0f);
        const double f15 = d.centre_of_mass().z - c0.z;
        std::printf("     %-20s %7.2f            %7.2f   %8.3f m                      %7.3f m\n",
                    variant == 0 ? "the chord (steer)" : "at rest (control)", p_after.z, ke, f05, f15);
        if (variant == 0) { p_anim = p_before.z; p_chord = p_after.z; fwd_chord = f05; }
        else { fwd_zero = f05; }
    }
    std::printf("     the animation's own momentum at the instant of the trip: %.2f kg m/s = %.1f kg x %.3f m/s\n",
                p_anim, static_cast<double>(k_mass), p_anim / k_mass);
    check(std::fabs(p_chord - p_anim) < 1e-4, "G.3 the handoff keeps the animation's momentum exactly");
    check(fwd_chord > 1.0 && std::fabs(fwd_zero) < 0.05,
          "G.3 CONTROL: handed over at rest, the character stops dead (then topples)");

    // ---- G.4 the joints at the instant of handoff -----------------------------------------
    {
        character d;
        scene s;
        add_floor(s);
        d.spawn_at(s, 0.0f, humanoid_desc());
        d.jog_and_trip(1.0f);
        double worst_measured = 0.0;
        double worst_ratio_err = 0.0;
        double worst_predicted = 0.0;
        std::printf("\n  Each joint's anchor velocity mismatch at the instant of handoff:\n");
        std::printf("     joint         measured m/s   predicted 1/2 h |wb x (wb x rb) - wa x (wa x ra)|\n");
        for (int i = 1; i < k_parts; ++i)
        {
            const ragdoll_part& p = d.rd.parts[static_cast<std::size_t>(i)];
            const rigid_body& a = d.part(p.parent);
            const rigid_body& b = d.part(i);
            const vec3 ra = world_point_of(a, p.link.anchor_a) - a.state.position;
            const vec3 rb = world_point_of(b, p.link.anchor_b) - b.state.position;
            const vec3 va = a.state.velocity + cross(a.angular_velocity, ra);
            const vec3 vb = b.state.velocity + cross(b.angular_velocity, rb);
            const double measured = length(vb - va);
            const vec3 ca = cross(a.angular_velocity, cross(a.angular_velocity, ra));
            const vec3 cb = cross(b.angular_velocity, cross(b.angular_velocity, rb));
            const double predicted = 0.5 * static_cast<double>(k_h) * static_cast<double>(length(cb - ca));
            std::printf("     %-12s  %.5f        %.5f\n", p.name.c_str(), measured, predicted);
            worst_measured = std::max(worst_measured, measured);
            worst_predicted = std::max(worst_predicted, predicted);
            if (predicted > 1e-3) { worst_ratio_err = std::max(worst_ratio_err, std::fabs(measured / predicted - 1.0)); }
        }
        std::printf("     worst |measured / predicted - 1| where predicted > 1 mm/s: %.4f\n", worst_ratio_err);
        check(worst_ratio_err < 0.05, "G.4 the handoff's joint residual is the centripetal term, to 5%");
    }
}

// ===========================================================================
// §H  when the animation disagrees with the joints
// ===========================================================================
//
// 7.7's walk, handed over as it stands. Its knees bend FORWARD and its forearms
// twist about their own axes, so the ragdoll starts violated. (1) By how much,
// joint by joint, over a cycle. (2) What the first second does with it, with
// no gravity and no floor so that nothing else adds or removes energy: split
// impulse repairs the pose and adds nothing; Baumgarte repairs it with a real
// velocity. CONTROL: the jog, which disagrees with nothing.

/// Kinetic energy relative to the centre of mass: the motion of the limbs
/// against one another, which is all a joint can add to or take away.
[[nodiscard]] double internal_energy(character& c)
{
    const vec3 v = c.momentum() / k_mass;
    double e = 0.0;
    for (int i = 0; i < k_parts; ++i)
    {
        const rigid_body& b = c.part(i);
        const double m = c.rd.parts[static_cast<std::size_t>(i)].mass;
        const vec3 rel = b.state.velocity - v;
        e += 0.5 * m * static_cast<double>(dot(rel, rel));
        e += 0.5 * static_cast<double>(dot(b.angular_velocity, world_inertia(b.orientation, c.rd.parts[static_cast<std::size_t>(i)].inertia) * b.angular_velocity));
    }
    return e;
}

void section_h()
{
    rule("8.12 section 10 (H)  when the animation disagrees with the joints");

    // ---- H.1 how far 7.7's walk is outside the ragdoll's joints ------------------------
    std::printf("  7.7's walk against the ragdoll's joints, worst over one cycle:\n");
    std::printf("     joint         worst angular violation   at t     worst anchor gap mm\n");
    double worst_knee = 0.0;
    double worst_elbow = 0.0;
    double worst_jog = 0.0;
    float worst_t = 0.0f;
    double worst_total = 0.0;
    for (int m = 0; m < 2; ++m)
    {
        character d;
        scene s;
        d.motion = m == 0 ? walk77_pose : jog_pose;
        d.travels = m == 1;
        d.spawn_at(s, 0.0f, humanoid_desc());
        std::vector<double> worst(k_parts, 0.0);
        std::vector<float> at(k_parts, 0.0f);
        std::vector<double> gap(k_parts, 0.0);
        for (int step = 0; step < 100; ++step)
        {
            const float t = static_cast<float>(step) * (m == 0 ? 0.01f : k_jog_cycle / 100.0f);
            d.targets_at(t);
            for (int k = 0; k < k_parts; ++k)
            {
                d.part(k).state.position = d.targets[static_cast<std::size_t>(k)].position;
                d.part(k).orientation = d.targets[static_cast<std::size_t>(k)].rotation;
            }
            double total = 0.0;
            for (int i = 1; i < k_parts; ++i)
            {
                const ragdoll_part& p = d.rd.parts[static_cast<std::size_t>(i)];
                const joint_error e = measure_joint(p.link, d.part(p.parent), d.part(i));
                total += e.angular;
                if (e.angular > worst[static_cast<std::size_t>(i)]) { worst[static_cast<std::size_t>(i)] = e.angular; at[static_cast<std::size_t>(i)] = t; }
                gap[static_cast<std::size_t>(i)] = std::max(gap[static_cast<std::size_t>(i)], static_cast<double>(e.linear));
            }
            if (m == 0 && total > worst_total) { worst_total = total; worst_t = t; }
        }
        if (m == 0)
        {
            for (int i = 1; i < k_parts; ++i)
            {
                std::printf("     %-12s  %8.2f deg               %.2f     %6.2f\n", d.rd.parts[static_cast<std::size_t>(i)].name.c_str(),
                            deg(worst[static_cast<std::size_t>(i)]), static_cast<double>(at[static_cast<std::size_t>(i)]),
                            gap[static_cast<std::size_t>(i)] * 1000.0);
            }
            worst_knee = std::max(worst[p_shin_l], worst[p_shin_r]);
            worst_elbow = std::max(worst[p_fore_l], worst[p_fore_r]);
        }
        else
        {
            for (int i = 1; i < k_parts; ++i) { worst_jog = std::max(worst_jog, worst[static_cast<std::size_t>(i)]); }
        }
    }
    std::printf("     CONTROL, the jog: worst angular violation over a cycle %.2e deg\n", deg(worst_jog));
    check(worst_knee > rad(40.0f), "H.1 7.7's walk bends the knees forward, far past the stop");
    check(worst_elbow > rad(20.0f), "H.1 and twists the forearms off the elbow hinge");
    check(worst_jog < rad(0.01f), "H.1 CONTROL: the jog is inside every joint");

    // ---- H.2 what the first second does with it ------------------------------------------
    std::printf("\n  Handed over at t = %.2f s (the worst total), no gravity, no floor:\n", static_cast<double>(worst_t));
    std::printf("     KE is INTERNAL — relative to the centre of mass, whose 1.2 m/s of travel\n"
                "     would otherwise swamp everything the joints do:\n");
    std::printf("     correction        KE at handoff   KE after 1 s   violation after 0.25 s   after 1 s\n");
    double ke_split = 0.0;
    double ke_baum = 0.0;
    double ke_none = 0.0;
    double ke0 = 0.0;
    double v_split = 0.0;
    for (int variant = 0; variant < 3; ++variant)
    {
        character d;
        scene s;
        s.world.set_gravity(vec3{});
        s.sleep.enabled = false;
        d.motion = walk77_pose;
        d.travels = false;
        d.spawn_at(s, 0.0f, humanoid_desc());
        const int steps = static_cast<int>(worst_t / k_h + 0.5f);
        for (int i = 0; i < steps; ++i) { d.animate_to(static_cast<float>(i + 1) * k_h); }
        d.trip(static_cast<float>(steps) * k_h);
        s.cfg.correction = variant == 0 ? position_correction::split_impulse
                         : variant == 1 ? position_correction::baumgarte
                                        : position_correction::none;
        s.cfg.joints.baumgarte = 0.2f;
        const double e0 = internal_energy(d);
        double viol_q = 0.0;
        for (int i = 0; i < 60; ++i)
        {
            s.step();
            if (i == 14)
            {
                for (int k = 1; k < k_parts; ++k)
                {
                    const ragdoll_part& p = d.rd.parts[static_cast<std::size_t>(k)];
                    viol_q = std::max(viol_q, static_cast<double>(measure_joint(p.link, d.part(p.parent), d.part(k)).angular));
                }
            }
        }
        double viol = 0.0;
        for (int k = 1; k < k_parts; ++k)
        {
            const ragdoll_part& p = d.rd.parts[static_cast<std::size_t>(k)];
            viol = std::max(viol, static_cast<double>(measure_joint(p.link, d.part(p.parent), d.part(k)).angular));
        }
        const double e1 = internal_energy(d);
        std::printf("     %-16s  %8.3f J       %8.3f J       %8.2f deg             %6.2f deg\n",
                    variant == 0 ? "split impulse" : variant == 1 ? "Baumgarte" : "none", e0, e1, deg(viol_q), deg(viol));
        if (variant == 0) { ke_split = e1; ke0 = e0; v_split = viol; }
        if (variant == 1) { ke_baum = e1; }
        if (variant == 2) { ke_none = e1; }
    }
    std::printf("     Baumgarte leaves %.3f J more internal energy than split impulse; split impulse\n"
                "     leaves %.4f J more than no correction at all, having repaired %.1f degrees more.\n",
                ke_baum - ke_split, ke_split - ke_none, 24.12 - deg(v_split));
    check(std::fabs(ke_split - ke_none) < 0.01, "H.2 split impulse repairs the pose and adds nothing: it matches NO correction");
    check(ke_baum > ke0 && ke_baum - ke_split > 0.2, "H.2 Baumgarte repairs it with a real velocity, and keeps it");
    check(v_split < rad(1.0f), "H.2 split impulse puts the limbs back inside their joints within a second");
}

} // namespace
namespace
{

// ===========================================================================
// §I  handing it back
// ===========================================================================
//
// (1) `read_pose` inverts `part_targets` exactly: place the bodies from a pose
// and read the same pose back. (2) The return, after a real fall: the blend
// starts FROM the read-back pose, so the first frame does not jump — and
// snapping to the clip does. (3) Without `realign_model` the whole character
// slides back to where it tripped over the blend; with it, it stands up where
// it fell. (4) A local-space blend keeps every bone its length; blending the
// model-space joint positions shortens them — 7.7 §7, one level up. (5) The
// arcs this return blends across, and what nlerp would do to their timing.

/// Model-space joint positions for a local pose.
void joint_positions(const skeleton& sk, std::span<const transform> local, std::vector<vec3>& out)
{
    std::vector<mat4> posed;
    engine::anim::compose_pose(sk, local, posed);
    out.resize(posed.size());
    for (std::size_t j = 0; j < posed.size(); ++j) { out[j] = engine::translation_of(posed[j]); }
}

/// The worst change in any bone's length (joint to parent), metres, against
/// the bind pose's.
[[nodiscard]] double worst_bone_change(const skeleton& sk, const std::vector<vec3>& p)
{
    // From joint 2: joint 1 is the hips, whose "bone" to the root is where the
    // character's pelvis is relative to its own model origin — a placement,
    // which a return from lying down changes by a metre, not a bone.
    double worst = 0.0;
    for (std::size_t j = 2; j < sk.joints.size(); ++j)
    {
        const std::size_t parent = sk.joints[j].parent;
        const double bind = length(sk.joints[j].local_bind.position);
        worst = std::max(worst, std::fabs(static_cast<double>(length(p[j] - p[parent])) - bind));
    }
    return worst;
}

void section_i()
{
    rule("8.12 section 11 (I)  handing it back");

    // ---- I.1 the round trip ----------------------------------------------------------
    {
        character d;
        scene s;
        d.spawn_at(s, 0.0f, humanoid_desc());
        double worst_p = 0.0;
        double worst_q = 0.0;
        for (int i = 0; i < 70; ++i)
        {
            const float t = static_cast<float>(i) * 0.01f;
            d.targets_at(t);
            for (int k = 0; k < k_parts; ++k)
            {
                d.part(k).state.position = d.targets[static_cast<std::size_t>(k)].position;
                d.part(k).orientation = d.targets[static_cast<std::size_t>(k)].rotation;
            }
            d.rd.frozen_local = d.local;   // what `simulate` would store
            std::vector<transform> back;
            read_pose(d.rd, d.sk, d.bodies(), d.model_at(t), back);
            for (std::size_t j = 0; j < k_joints; ++j)
            {
                worst_p = std::max(worst_p, static_cast<double>(length(back[j].position - d.local[j].position)));
                worst_q = std::max(worst_q, between(back[j].rotation, d.local[j].rotation));
            }
        }
        std::printf("  read_pose(part_targets(pose)) against the pose, 70 instants of the jog:\n");
        std::printf("     worst position %.2e m,  worst rotation %.2e rad\n", worst_p, worst_q);
        check(worst_p < 1e-5 && worst_q < 1e-5, "I.1 read_pose inverts part_targets");
    }

    // ---- I.2–I.5 the return, after a real fall ------------------------------------------
    //
    // Trip at 1 s, lie for 3 s, then return to the jog pose — in place, since a
    // character getting up does not keep running — over 0.5 s.
    const float t_trip = 1.0f;
    const float blend = 0.5f;
    std::printf("\n  After a trip and three seconds on the floor, back to the clip over %.1f s:\n",
                static_cast<double>(blend));
    std::printf("     return                         first-frame jump   pelvis slide   worst bone change\n");
    double jump_blend = 0.0;
    double jump_snap = 0.0;
    double slide_raw = 0.0;
    double slide_realigned = 0.0;
    double bone_local = 0.0;
    double bone_model = 0.0;
    double worst_arc = 0.0;
    double worst_nlerp = 0.0;
    for (int variant = 0; variant < 4; ++variant)
    {
        // 0: blend + realign   1: blend, no realign   2: snap + realign   3: model-space blend + realign
        character d;
        scene s;
        add_floor(s);
        d.spawn_at(s, 0.0f, humanoid_desc());
        d.jog_and_trip(t_trip);
        s.run(3.0f);

        const mat4 tripped_at = d.model_at(t_trip);
        std::vector<transform> target;
        pose_at(jog_pose, t_trip, target);
        std::vector<vec3> target_joints;
        joint_positions(d.sk, target, target_joints);
        const mat4 place = variant == 1 ? tripped_at
                                        : realign_model(d.rd, d.bodies(), tripped_at, target_joints[j_hips]);
        std::vector<transform> start;
        read_pose(d.rd, d.sk, d.bodies(), place, start);
        std::vector<vec3> start_joints;
        joint_positions(d.sk, start, start_joints);

        animate(d.rd, d.bodies());
        d.motion = jog_pose;
        d.travels = false;
        d.base = place;

        const vec3 pelvis0 = d.part(p_pelvis).state.position;
        std::vector<vec3> before(k_parts);
        double first_jump = 0.0;
        double bone = 0.0;
        const int steps = static_cast<int>(blend / k_h + 0.5f);
        for (int i = 0; i < steps; ++i)
        {
            const float w = variant == 2 ? 1.0f : static_cast<float>(i + 1) / static_cast<float>(steps);
            std::vector<transform> pose(k_joints);
            std::vector<vec3> joints_now(k_joints);
            if (variant == 3)
            {
                // The wrong blend: interpolate the MODEL-SPACE joint positions
                // (and nothing else can be drawn from them), then measure bones.
                for (std::size_t j = 0; j < k_joints; ++j)
                {
                    joints_now[j] = engine::lerp(start_joints[j], target_joints[j], w);
                }
                pose = start;   // bodies follow the local blend; only the bones are measured here
                for (std::size_t j = 0; j < k_joints; ++j) { pose[j] = engine::transform_blend_slerp(start[j], target[j], w); }
            }
            else
            {
                for (std::size_t j = 0; j < k_joints; ++j) { pose[j] = engine::transform_blend_slerp(start[j], target[j], w); }
                joint_positions(d.sk, pose, joints_now);
            }
            bone = std::max(bone, worst_bone_change(d.sk, joints_now));

            std::vector<mat4> posed;
            engine::anim::compose_pose(d.sk, pose, posed);
            part_targets(d.rd, posed, place, d.targets);
            for (int k = 0; k < k_parts; ++k) { before[static_cast<std::size_t>(k)] = d.part(k).state.position; }
            steer(d.rd, d.bodies(), d.targets, s.h);
            s.step();
            if (i == 0)
            {
                for (int k = 0; k < k_parts; ++k)
                {
                    first_jump = std::max(first_jump, static_cast<double>(length(d.part(k).state.position
                                                                                 - before[static_cast<std::size_t>(k)])));
                }
            }
        }
        const vec3 moved = d.part(p_pelvis).state.position - pelvis0;
        const double slide = std::sqrt(static_cast<double>(moved.x * moved.x + moved.z * moved.z));
        const char* name = variant == 0 ? "slerp blend, realigned"
                         : variant == 1 ? "slerp blend, NOT realigned"
                         : variant == 2 ? "snap to the clip (control)"
                                        : "model-space position blend";
        std::printf("     %-30s  %8.4f m        %6.3f m       %7.2f mm\n", name, first_jump, slide, bone * 1000.0);
        if (variant == 0)
        {
            jump_blend = first_jump;
            slide_realigned = slide;
            bone_local = bone;
            for (std::size_t j = 0; j < k_joints; ++j)
            {
                const double arc = between(start[j].rotation, target[j].rotation);
                worst_arc = std::max(worst_arc, arc);
                // nlerp and slerp AGREE at the midpoint, by symmetry — the first
                // draft measured there and reported 0.00 degrees. The timing
                // error peaks near the quarter-points, so scan the whole blend.
                for (int q = 1; q < 40; ++q)
                {
                    const float u = static_cast<float>(q) / 40.0f;
                    const quat n = engine::quat_nlerp(start[j].rotation, target[j].rotation, u);
                    const quat sl = engine::quat_slerp(start[j].rotation, target[j].rotation, u);
                    worst_nlerp = std::max(worst_nlerp, between(n, sl));
                }
            }
        }
        if (variant == 1) { slide_raw = slide; }
        if (variant == 2) { jump_snap = first_jump; }
        if (variant == 3) { bone_model = bone; }
    }
    std::printf("     worst arc any joint blends across: %.1f deg;  worst nlerp-against-slerp lag over the blend: %.2f deg\n",
                deg(worst_arc), deg(worst_nlerp));
    check(jump_snap > 20.0 * jump_blend, "I.2 blending from the read-back pose does not jump; snapping does");
    check(slide_raw > 0.5 && slide_realigned < 0.25 * slide_raw, "I.3 realign_model removes the slide back to the trip");
    check(bone_local < 0.01, "I.4 a local-space blend keeps every bone its length, to the joints' own gap");
    check(bone_model > 100.0 * bone_local, "I.4 a model-space blend shortens them");
    // 7.7 measured two adjacent keys of a 30 Hz clip 0.016921 degrees apart
    // under nlerp and slerp. A return blends across whole poses.
    check(worst_arc > rad(90.0f) && worst_nlerp > 50.0 * rad(0.016921f),
          "I.5 the return blends across arcs where nlerp lags slerp by a hundred times a clip's");
}

// ===========================================================================
// §J  a return the physics does not fight
// ===========================================================================
//
// An animated character jogs into a 5 kg crate. Three ways to make its bodies
// follow the clip: (1) KINEMATIC and steered — this lesson's design; (2)
// DYNAMIC and teleported onto the pose every frame, the velocities left to the
// solver — the tempting version; (3) dynamic, teleported, AND given the chord as
// its velocity every frame. Measured: whether a body's velocity describes its
// motion (the "lie"), what the crate does, and what the bodies carry when they
// are finally let go.

void section_j()
{
    rule("8.12 section 12 (J)  a return the physics does not fight");
    std::printf("  Jogging at 2.5 m/s into a 5 kg crate for two seconds, then let go:\n");
    std::printf("     bodies follow the clip by        velocity lie   deepest limb in crate   crate top speed   bodies' speed when let go\n");
    double lie[3] = {};
    double crate_speed[3] = {};
    double release[3] = {};
    double inside[3] = {};
    for (int variant = 0; variant < 3; ++variant)
    {
        character d;
        scene s;
        add_floor(s);
        d.spawn_at(s, 0.0f, humanoid_desc());
        const std::uint32_t crate = s.add(make_box({0.0f, 0.25f, 2.2f}, 5.0f, {0.25f, 0.25f, 0.25f}),
                                          box_shape({0.25f, 0.25f, 0.25f}));
        if (variant > 0) { simulate(d.rd, d.bodies(), d.local); }   // dynamic from the start
        double worst_lie = 0.0;
        double top = 0.0;
        double deepest = 0.0;
        std::vector<vec3> before(k_parts);
        for (int i = 0; i < 120; ++i)
        {
            const float t = static_cast<float>(i + 1) * k_h;
            d.targets_at(t);
            for (int k = 0; k < k_parts; ++k) { before[static_cast<std::size_t>(k)] = d.part(k).state.position; }
            if (variant == 0) { steer(d.rd, d.bodies(), d.targets, s.h); }
            s.step();
            if (variant > 0)
            {
                // the teleport: after the physics has had its say
                for (int k = 0; k < k_parts; ++k)
                {
                    rigid_body& b = d.part(k);
                    if (variant == 2)
                    {
                        b.state.velocity = (d.targets[static_cast<std::size_t>(k)].position - before[static_cast<std::size_t>(k)]) / s.h;
                    }
                    b.state.position = d.targets[static_cast<std::size_t>(k)].position;
                    b.orientation = d.targets[static_cast<std::size_t>(k)].rotation;
                }
            }
            for (int k = 0; k < k_parts; ++k)
            {
                const vec3 moved = (d.part(k).state.position - before[static_cast<std::size_t>(k)]) / s.h;
                worst_lie = std::max(worst_lie, static_cast<double>(length(d.part(k).state.velocity - moved)));
            }
            top = std::max(top, static_cast<double>(length(s.body(crate).state.velocity)));
            for (int k = 0; k < k_parts; ++k)
            {
                const placed x = place(d.rd.parts[static_cast<std::size_t>(k)].collider, d.part(k));
                const placed y = place(s.shapes[crate], s.body(crate));
                const contact_manifold m = collide_manifold(x.view(), y.view());
                for (int q = 0; q < m.count; ++q) { deepest = std::max(deepest, static_cast<double>(m.points[q].depth)); }
            }
        }
        double speed = 0.0;
        for (int k = 0; k < k_parts; ++k) { speed = std::max(speed, static_cast<double>(length(d.part(k).state.velocity))); }
        const char* name = variant == 0 ? "kinematic, steered" : variant == 1 ? "dynamic, teleported" : "dynamic, teleported + chord";
        std::printf("     %-32s  %8.3f m/s     %8.1f mm            %6.2f m/s         %6.2f m/s\n", name, worst_lie,
                    deepest * 1000.0, top, speed);
        inside[variant] = deepest;
        lie[variant] = worst_lie;
        crate_speed[variant] = top;
        release[variant] = speed;
    }
    check(lie[0] < 1e-4, "J.1 a steered body's velocity is exactly its motion");
    check(lie[1] > 1.0, "J.2 a teleported body's velocity is a lie");
    check(release[1] > 3.0 * release[0], "J.2 and it hands the lie to whatever lets it go");
    check(crate_speed[0] > 2.0, "J.3 the steered character pushes the crate (8.12's wake fix: it was asleep)");
    check(inside[1] > 3.0 * inside[0], "J.3 a teleported limb is driven into what the solver just pushed it out of");
    check(inside[2] > 2.0 * inside[0], "J.3 even with the right velocity, a teleport still overrules the solve");
}

// ===========================================================================
// §K  settling, sleeping, and the bill
// ===========================================================================
//
// (1) How long a tripped ragdoll takes to fall asleep — as ONE island, 8.10's
// rule — under each correction. (2) What a ragdoll costs per step, falling and
// lying, and how many fit in a millisecond. Timings are minima of repeated
// runs, as since 8.8.

void section_k()
{
    rule("8.12 section 13 (K)  settling, sleeping, and the bill");

    // ---- K.1 settling, and sleeping ------------------------------------------------------
    //
    // Every ragdoll settles to millijoules within seconds. SLEEPING is a
    // different claim, and it refused: 8.10's thresholds are 0.05 m/s and
    // 0.10 rad/s on EVERY body of an island, tuned on crates, and a ragdoll has
    // eleven bodies, some of them thin. Each ragdoll still awake at eight
    // seconds is classified: CRUSHED if the chest lies more than 5 mm into an
    // arm (§8's stack, which fights for ever), otherwise TURNING.
    std::printf("  Twelve trips, eight seconds each:\n");
    std::printf("     settings                         asleep   median s   KE at 8 s (mean)   awake: crushed / turning\n");
    int asleep_default = 0;
    double ke_default = 0.0;
    for (int variant = 0; variant < 4; ++variant)
    {
        std::vector<double> times;
        int crushed = 0;
        int turning = 0;
        double ke = 0.0;
        for (int k = 0; k < 12; ++k)
        {
            character d;
            scene s;
            add_floor(s);
            if (variant == 1) { s.cfg.correction = position_correction::baumgarte; }
            d.spawn_at(s, 0.0f, humanoid_desc());
            d.jog_and_trip(1.0f + k_jog_cycle * static_cast<float>(k) / 12.0f);
            if (variant >= 2)
            {
                for (int i = 0; i < k_parts; ++i) { d.part(i).angular_damping = 1.0f; }
            }
            if (variant == 3) { s.cfg.velocity_iterations = 1; s.substeps = 8; }
            double when = -1.0;
            for (int i = 0; i < 480; ++i)
            {
                s.step();
                if (when < 0.0 && d.part(p_pelvis).sleeping) { when = static_cast<double>(i + 1) * k_h; }
            }
            ke += d.energy() / 12.0;
            if (when >= 0.0) { times.push_back(when); continue; }
            const double crush = std::max(overlap_of(d, p_torso, p_fore_l), overlap_of(d, p_torso, p_fore_r));
            (crush > 0.005 ? crushed : turning) += 1;
        }
        std::sort(times.begin(), times.end());
        const double median = times.empty() ? 0.0 : times[times.size() / 2];
        const char* name = variant == 0 ? "split impulse (shipped)"
                         : variant == 1 ? "Baumgarte"
                         : variant == 2 ? "+ angular damping 1/s"
                                        : "+ damping, 8 sub-steps x 1";
        std::printf("     %-32s  %2zu/12     %6.2f       %9.5f J           %d / %d\n", name, times.size(), median, ke,
                    crushed, turning);
        if (variant == 0) { asleep_default = static_cast<int>(times.size()); ke_default = ke; }
    }
    check(ke_default < 0.05, "K.1 every trip settles to millijoules");
    check(asleep_default < 12, "K.1 REFUSED: 8.10's thresholds do not put every ragdoll to sleep");

    // ---- K.2 the bill -------------------------------------------------------------------
    const int crowd = 16;
    double falling_best = 1e9;
    double lying_best = 1e9;
    double narrow_falling = 1e9;
    double solve_falling = 1e9;
    for (int rep = 0; rep < 3; ++rep)
    {
        scene s;
        add_floor(s);
        s.sleep.enabled = false;   // price the work, not the saving
        std::vector<character> people(crowd);
        for (int n = 0; n < crowd; ++n)
        {
            people[static_cast<std::size_t>(n)].base =
                engine::translation(vec3{static_cast<float>(n % 4) * 3.0f - 4.5f, 0.0f, static_cast<float>(n / 4) * 3.0f - 12.0f});
            people[static_cast<std::size_t>(n)].spawn_at(s, 0.0f, humanoid_desc());
        }
        for (int i = 0; i < 60; ++i)
        {
            for (character& p : people) { p.targets_at(static_cast<float>(i + 1) * k_h); steer(p.rd, p.bodies(), p.targets, s.h); }
            s.step();
        }
        for (character& p : people) { p.trip(1.0f); }
        // MEANS over a window, minimised over repetitions: the work changes
        // from step to step as contacts come and go, so the minimum single step
        // would price the step with the fewest contacts in it.
        double fall = 0.0;
        double narrow = 0.0;
        double solve = 0.0;
        for (int i = 0; i < 60; ++i)
        {
            const clock_type::time_point t0 = clock_type::now();
            s.step();
            fall += seconds_since(t0) / 60.0;
            narrow += s.t_narrow / 60.0;
            solve += s.t_solve / 60.0;
        }
        falling_best = std::min(falling_best, fall);
        narrow_falling = std::min(narrow_falling, narrow);
        solve_falling = std::min(solve_falling, solve);
        s.run(3.0f);
        double lie_time = 0.0;
        for (int i = 0; i < 60; ++i)
        {
            const clock_type::time_point t0 = clock_type::now();
            s.step();
            lie_time += seconds_since(t0) / 60.0;
        }
        lying_best = std::min(lying_best, lie_time);
    }
    std::printf("\n  %d ragdolls, one step, sleeping off (mean of 60 steps, best of 3 runs):\n", crowd);
    std::printf("     falling:  %.1f us a step  (%.1f us a ragdoll: narrow phase %.1f, solve %.1f)\n",
                falling_best * 1e6, falling_best * 1e6 / crowd, narrow_falling * 1e6 / crowd,
                solve_falling * 1e6 / crowd);
    std::printf("     lying:    %.1f us a step  (%.1f us a ragdoll)\n", lying_best * 1e6, lying_best * 1e6 / crowd);
    std::printf("     ragdolls per millisecond, falling: %.0f\n", 1e-3 / (falling_best / crowd));
    check(falling_best > 0.0, "K.2 measured");
}

} // namespace

int main(int argc, char** argv)
{
    const char* only = nullptr;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strncmp(argv[i], "--only=", 7) == 0) { only = argv[i] + 7; }
    }
    auto want = [&](char c) { return only == nullptr || std::strchr(only, c) != nullptr; };

    const clock_type::time_point t0 = clock_type::now();
    if (want('a')) { section_a(); }
    if (want('b')) { section_b(); }
    if (want('c')) { section_c(); }
    if (want('d')) { section_d(); }
    if (want('e')) { section_e(); }
    if (want('f')) { section_f(); }
    if (want('g')) { section_g(); }
    if (want('h')) { section_h(); }
    if (want('i')) { section_i(); }
    if (want('j')) { section_j(); }
    if (want('k')) { section_k(); }

    std::printf("\n%s: %d checks, %d failures  (%.2f s)\n", g_failures == 0 ? "PASS" : "FAIL", g_checks,
                g_failures, seconds_since(t0));
    return g_failures == 0 ? 0 : 1;
}
