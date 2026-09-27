// engine/src/phys/ragdoll.cpp — a skeleton with mass, and the seam between a clip
// and a solver.
//
// Lesson 8.12. Read `ragdoll.hpp` first; it carries the argument. Three layers:
//
//   * BUILDING. `build_ragdoll` turns model-space capsules into bodies bolted to
//     bones, finds each part's parent part, authors 8.11's joints at the bind
//     pose, and audits every pair of capsules for overlap at rest.
//   * STEERING. `part_targets` and `steer` keep kinematic bodies on an animated
//     pose by VELOCITY — the only way to move a body whose velocity has to mean
//     something to the contacts it makes.
//   * READING BACK. `read_pose` and `realign_model` turn bodies into a pose the
//     animation system can blend from.
//
// Every transform here is RIGID — a position and a unit quaternion — because a
// body is. A skeleton's `transform` also has a scale, and a ragdoll requires it
// to be one; the scales of passengers are copied through untouched.

#include <engine/phys/ragdoll.hpp>

#include <engine/phys/convex.hpp>
#include <engine/phys/gjk.hpp>
#include <engine/phys/solver.hpp>

#include <algorithm>   // std::max, std::sort
#include <cmath>       // std::sqrt, std::atan2

namespace engine::phys
{
namespace
{

/// A rigid placement: where, and which way. `transform` without the scale.
struct rigid
{
    vec3 p{};
    quat q{};
};

[[nodiscard]] rigid compose(const rigid& parent, const rigid& child)
{
    return {parent.p + rotate(parent.q, child.p), parent.q * child.q};
}

/// `inverse(parent) · child` — the child as its parent sees it.
[[nodiscard]] rigid relative(const rigid& parent, const rigid& child)
{
    const quat inv = conjugate(parent.q);
    return {rotate(inv, child.p - parent.p), inv * child.q};
}

/// The rigid part of an affine matrix whose linear part is a rotation. Cheaper
/// than `transform_from_affine`, which also recovers a scale and a shear this
/// file has no use for.
[[nodiscard]] rigid rigid_of(const mat4& m)
{
    return {translation_of(m), normalised(quat_from_rotation(linear_of(m)))};
}

/// The smallest rotation carrying unit `from` onto unit `to` — two mirrors, as
/// in `constraint.cpp`. Used once per part at build time, to stand a capsule
/// (whose axis is body +y) along its bone.
[[nodiscard]] quat align(vec3 from, vec3 to)
{
    const vec3 sum = from + to;
    const float len2 = length_squared(sum);
    if (len2 < 1e-12f)
    {
        // Opposite: a half-turn about anything perpendicular. `from` here is
        // always +y, so +x will do.
        return quat{0.0f, vec3{1.0f, 0.0f, 0.0f}};
    }
    const quat r = rotor_from_mirrors(from, sum / std::sqrt(len2));
    return r.w < 0.0f ? -r : r;
}

[[nodiscard]] std::uint32_t pack(std::uint32_t i, std::uint32_t j)
{
    return i < j ? (i << 16) | j : (j << 16) | i;
}

/// Links between two parts in the tree: up from each to their deepest common
/// ancestor. Small trees, so the walk is the whole algorithm.
[[nodiscard]] int links_between(const std::vector<ragdoll_part>& parts, int i, int j)
{
    int depth_i = 0;
    for (int a = i; a >= 0; a = parts[static_cast<std::size_t>(a)].parent)
    {
        int depth_j = 0;
        for (int b = j; b >= 0; b = parts[static_cast<std::size_t>(b)].parent)
        {
            if (a == b) { return depth_i + depth_j; }
            ++depth_j;
        }
        ++depth_i;
    }
    return 1 << 20;   // different trees: as far apart as it gets
}

} // namespace

const char* name_of(ragdoll_link link)
{
    switch (link)
    {
    case ragdoll_link::root:       return "root";
    case ragdoll_link::hinge:      return "hinge";
    case ragdoll_link::cone_twist: return "cone-twist";
    }
    return "?";
}

// ---------------------------------------------------------------------------
// Building
// ---------------------------------------------------------------------------

ragdoll_report build_ragdoll(const anim::skeleton& sk, const ragdoll_desc& desc, ragdoll& out)
{
    ragdoll_report report;
    out = ragdoll{};

    const std::size_t joints = sk.joints.size();
    const std::size_t count = desc.parts.size();
    report.parts = count;
    out.part_of_joint.assign(joints, -1);

    // The bind pose, composed once, as rigid placements in model space.
    anim::rest_pose(sk, out.frozen_local);
    std::vector<mat4> bind;
    anim::compose_pose(sk, out.frozen_local, bind);

    // ---- pass 1: which skeleton joint each part follows --------------------
    for (std::size_t i = 0; i < count; ++i)
    {
        const anim::joint_index jt = desc.parts[i].bone;
        if (jt >= joints) { ++report.bad_joint; continue; }
        if (out.part_of_joint[jt] >= 0) { ++report.duplicate_joint; continue; }
        out.part_of_joint[jt] = static_cast<int>(i);
    }

    // ---- pass 2: bodies, and each part's parent part -----------------------
    //
    // The bodies are placed at the bind pose in MODEL space, only so that 8.11's
    // `make_*` functions can author each joint in the two bodies' own frames.
    // Those frames are all a joint keeps, so the placement is thrown away and
    // the joints are valid at every pose.
    std::vector<rigid_body> at_bind(count);
    out.parts.resize(count);

    for (std::size_t i = 0; i < count; ++i)
    {
        const ragdoll_part_desc& d = desc.parts[i];
        ragdoll_part& p = out.parts[i];
        p.name = d.name;
        p.bone = d.bone;
        p.kind = d.link;

        // The capsule: its segment is the authored one, its axis body +y.
        const vec3 segment = d.to - d.from;
        const float len = length(segment);
        const vec3 dir = len > 1e-6f ? segment / len : vec3{0.0f, 1.0f, 0.0f};
        const vec3 centre = (d.from + d.to) * 0.5f;
        const quat facing = align(vec3{0.0f, 1.0f, 0.0f}, dir);
        p.collider = capsule_shape(d.radius, 0.5f * len);
        p.mass = d.mass_fraction * desc.mass;
        p.inertia = inertia_of(p.collider, p.mass);
        report.mass_fraction_sum += d.mass_fraction;

        // joint_from_body: the body's placement in its bone's frame. A bone
        // with no valid joint is bolted to the model origin, and counted.
        const rigid bone = d.bone < joints ? rigid_of(bind[d.bone]) : rigid{};
        const rigid body = relative(bone, rigid{centre, facing});
        p.offset = body.p;
        p.rotation = body.q;

        rigid_body& b = at_bind[i];
        b = make_dynamic(centre, p.mass > 0.0f ? p.mass : 1.0f);
        b.orientation = facing;

        // The parent part: the nearest ancestor joint that is a part.
        if (d.bone < joints)
        {
            for (anim::joint_index up = sk.joints[d.bone].parent; up != anim::k_no_parent
                                                                    && up < joints;
                 up = sk.joints[up].parent)
            {
                if (out.part_of_joint[up] >= 0)
                {
                    p.parent = out.part_of_joint[up];
                    break;
                }
            }
        }
        if (p.parent >= static_cast<int>(i)) { ++report.out_of_order; }
    }

    // ---- pass 3: the joints --------------------------------------------------
    for (std::size_t i = 0; i < count; ++i)
    {
        const ragdoll_part_desc& d = desc.parts[i];
        ragdoll_part& p = out.parts[i];

        // A part with no parent part, or with a parent that comes after it,
        // cannot be joined; it is a free body, and counted as a root.
        if (p.parent < 0 || p.parent >= static_cast<int>(i) || d.link == ragdoll_link::root)
        {
            p.kind = ragdoll_link::root;
            ++report.roots;
            continue;
        }

        const rigid_body& a = at_bind[static_cast<std::size_t>(p.parent)];
        const rigid_body& b = at_bind[i];
        const vec3 anchor = translation_of(bind[d.bone]);

        if (d.link == ragdoll_link::hinge)
        {
            p.link = make_hinge(a, b, anchor, d.axis);
            ++report.hinges;
        }
        else
        {
            const vec3 bone = rotate(b.orientation, vec3{0.0f, 1.0f, 0.0f});
            const vec3 twist = length_squared(d.axis) > 0.0f ? d.axis : bone;
            p.link = make_cone_twist(a, b, anchor, twist, d.cone_axis);
            p.link.cone.enabled = d.swing > 0.0f;
            p.link.cone.swing = d.swing;
            ++report.cone_twists;
        }
        p.link.limit.enabled = d.upper > d.lower;
        p.link.limit.lower = d.lower;
        p.link.limit.upper = d.upper;
    }

    // ---- pass 4: which pairs may collide -------------------------------------
    //
    // Every pair is asked two questions — are they jointed, and do their
    // capsules overlap at rest — and the rule decides what to do with the
    // answers. GJK answers the second, which is 8.5's question asked for the
    // reason 8.5 gave: two capsules have no closed-form distance worth writing
    // when a support function already exists.
    std::vector<capsule> caps(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        caps[i] = world_capsule(out.parts[i].collider, at_bind[i].state.position,
                                at_bind[i].orientation);
    }

    for (std::size_t i = 0; i < count; ++i)
    {
        for (std::size_t j = i + 1; j < count; ++j)
        {
            const ragdoll_part& pi = out.parts[i];
            const ragdoll_part& pj = out.parts[j];
            const bool jointed = (pj.parent == static_cast<int>(i) && pj.kind != ragdoll_link::root)
                                 || (pi.parent == static_cast<int>(j) && pi.kind != ragdoll_link::root);

            const gjk_result g = gjk_distance(as_convex(caps[i]), as_convex(caps[j]));
            const bool overlap = g.status != gjk_status::separated || g.distance < desc.overlap_margin;
            const bool near = links_between(out.parts, static_cast<int>(i), static_cast<int>(j)) <= 2;

            bool exclude = jointed;
            if (jointed) { ++report.excluded_jointed; }
            else if (desc.exclusion == ragdoll_exclusion::overlapping && overlap)
            {
                exclude = true;
                ++report.excluded_overlapping;
            }
            else if (desc.exclusion == ragdoll_exclusion::two_links && near)
            {
                exclude = true;
                ++report.excluded_two_links;
            }

            if (exclude)
            {
                out.excluded.push_back(pack(static_cast<std::uint32_t>(i), static_cast<std::uint32_t>(j)));
            }
            else if (overlap)
            {
                ++report.overlapping_colliding;
            }
        }
    }
    std::sort(out.excluded.begin(), out.excluded.end());
    return report;
}

// ---------------------------------------------------------------------------
// Steering
// ---------------------------------------------------------------------------

void part_targets(const ragdoll& rd, std::span<const mat4> model_from_joint,
                  const mat4& world_from_model, std::vector<transform>& out)
{
    out.resize(rd.parts.size());
    const rigid model = rigid_of(world_from_model);
    for (std::size_t i = 0; i < rd.parts.size(); ++i)
    {
        const ragdoll_part& p = rd.parts[i];
        const rigid bone = p.bone < model_from_joint.size() ? rigid_of(model_from_joint[p.bone])
                                                              : rigid{};
        const rigid world = compose(compose(model, bone), rigid{p.offset, p.rotation});
        out[i].position = world.p;
        out[i].rotation = normalised(world.q);
        out[i].scale = vec3{1.0f, 1.0f, 1.0f};
    }
}

std::uint32_t spawn(ragdoll& rd, body_world& world, std::span<const transform> targets)
{
    rd.first_body = static_cast<std::uint32_t>(world.size());
    rd.mode = ragdoll_mode::animated;
    for (std::size_t i = 0; i < rd.parts.size(); ++i)
    {
        const transform t = i < targets.size() ? targets[i] : transform{};
        rigid_body b = make_kinematic(t.position, vec3{});
        b.orientation = t.rotation;
        world.add(b);
    }
    return rd.first_body;
}

vec3 steering_angular_velocity(quat from, quat to, float h, spin_rule rule)
{
    if (!(h > 0.0f)) { return vec3{}; }

    // The rotation still to do, in WORLD terms: `to = delta · from`, because the
    // integrator applies an angular velocity on the left (`ω q`, 8.3).
    quat delta = to * conjugate(from);
    if (delta.w < 0.0f) { delta = -delta; }   // the short way round (7.4's double cover)

    if (rule == spin_rule::exponential)
    {
        // The logarithm: an axis, and the whole angle, per step.
        const float s = length(delta.v);
        if (s < 1e-9f) { return delta.v * (2.0f / h); }
        return delta.v * (2.0f * std::atan2(s, delta.w) / (s * h));
    }

    // The inverse of `normalise(q + ½hωq)`: that step's delta is proportional
    // to `(1, ½hω)`, so `½hω = delta.v / delta.w`. A half-turn in one step has
    // `delta.w = 0` and no finite answer; it is clamped to a very fast one,
    // which a ragdoll whose animation turns a bone by 180 degrees in a frame
    // has earned.
    const float w = std::max(delta.w, 1e-3f);
    return delta.v * (2.0f / (h * w));
}

void steer(const ragdoll& rd, std::span<rigid_body> bodies, std::span<const transform> targets,
           float h, spin_rule rule)
{
    if (!(h > 0.0f)) { return; }
    const float inv_h = 1.0f / h;
    for (std::size_t i = 0; i < rd.parts.size() && i < targets.size(); ++i)
    {
        rigid_body& b = bodies[rd.first_body + i];
        if (b.kind != body_kind::kinematic) { continue; }

        // The chord. For the position half of every integrator in this engine
        // this is exact: the step adds `v·h` and nothing else.
        b.state.velocity = (targets[i].position - b.state.position) * inv_h;
        b.angular_velocity = steering_angular_velocity(b.orientation, targets[i].rotation, h, rule);
    }
}

void simulate(ragdoll& rd, std::span<rigid_body> bodies, std::span<const transform> pose)
{
    rd.mode = ragdoll_mode::simulated;

    // The passengers' locals, as the animation last had them. A pose shorter
    // than the skeleton keeps what was there for the missing joints.
    for (std::size_t j = 0; j < rd.frozen_local.size() && j < pose.size(); ++j)
    {
        rd.frozen_local[j] = pose[j];
    }

    for (std::size_t i = 0; i < rd.parts.size(); ++i)
    {
        rigid_body& b = bodies[rd.first_body + i];
        const ragdoll_part& p = rd.parts[i];

        // THE HANDOFF IS THESE LINES AND NOTHING ELSE. The velocities are left
        // exactly as `steer` set them — the chord that carried this body onto
        // this pose — and for semi-implicit Euler that chord IS the velocity.
        b.kind = body_kind::dynamic;
        (void)set_mass(b, p.mass);
        (void)set_inertia(b, p.inertia);
        b.force = vec3{};
        b.torque = vec3{};
        b.sleeping = false;
        b.sleep_timer = 0.0f;
    }
}

void animate(ragdoll& rd, std::span<rigid_body> bodies)
{
    rd.mode = ragdoll_mode::animated;
    for (std::size_t i = 0; i < rd.parts.size(); ++i)
    {
        rigid_body& b = bodies[rd.first_body + i];

        // `make_kinematic`'s zeros: no force and no torque can move it, and no
        // impulse from a contact or a joint can change its velocity.
        b.kind = body_kind::kinematic;
        b.inv_mass = 0.0f;
        b.inv_inertia_local = mat3{vec3{}, vec3{}, vec3{}};
        b.inertia_local = mat3{vec3{}, vec3{}, vec3{}};
        b.sleeping = false;
        b.sleep_timer = 0.0f;

        // A joint's cached impulses describe what it cost to hold THIS pose.
        // The next `simulate` starts from a different one.
        joint& j = rd.parts[i].link;
        j.point_impulse = vec3{};
        j.angular_impulse = vec3{};
        j.limit_impulse[0] = 0.0f;
        j.limit_impulse[1] = 0.0f;
        j.motor_impulse = 0.0f;
        j.swing_impulse = 0.0f;
    }
}

void add_joints(ragdoll& rd, constraint_solver& solver)
{
    if (rd.mode != ragdoll_mode::simulated) { return; }
    for (std::size_t i = 0; i < rd.parts.size(); ++i)
    {
        ragdoll_part& p = rd.parts[i];
        if (p.kind == ragdoll_link::root || p.parent < 0) { continue; }
        solver.add(rd.first_body + static_cast<std::uint32_t>(p.parent),
                   rd.first_body + static_cast<std::uint32_t>(i), p.link);
    }
}

void exclude_pairs(const ragdoll& rd, collision_filter& filter)
{
    for (const std::uint32_t key : rd.excluded)
    {
        filter.exclude(rd.first_body + (key >> 16), rd.first_body + (key & 0xFFFFu));
    }
}

// ---------------------------------------------------------------------------
// Reading back
// ---------------------------------------------------------------------------

void read_pose(const ragdoll& rd, const anim::skeleton& sk, std::span<const rigid_body> bodies,
               const mat4& world_from_model, std::vector<transform>& local_out)
{
    const std::size_t n = sk.joints.size();
    local_out.resize(n);

    const rigid model = rigid_of(world_from_model);
    const quat model_inv = conjugate(model.q);

    std::vector<rigid> posed(n);
    for (std::size_t j = 0; j < n; ++j)
    {
        const anim::joint_index parent = sk.joints[j].parent;
        const bool has_parent = parent != anim::k_no_parent && parent < j;
        const transform frozen = j < rd.frozen_local.size() ? rd.frozen_local[j]
                                                            : sk.joints[j].local_bind;
        const int part = j < rd.part_of_joint.size() ? rd.part_of_joint[j] : -1;

        if (part >= 0)
        {
            // A PART: the body says where its bone is. Into model space, then
            // back through joint_from_body.
            const ragdoll_part& p = rd.parts[static_cast<std::size_t>(part)];
            const rigid_body& b = bodies[rd.first_body + static_cast<std::size_t>(part)];
            const rigid body{rotate(model_inv, b.state.position - model.p), model_inv * b.orientation};
            const quat bone_q = body.q * conjugate(p.rotation);
            posed[j] = rigid{body.p - rotate(bone_q, p.offset), bone_q};

            const rigid local = has_parent ? relative(posed[parent], posed[j]) : posed[j];
            local_out[j].position = local.p;
            local_out[j].rotation = normalised(local.q);
            local_out[j].scale = frozen.scale;
        }
        else
        {
            // A PASSENGER: it rides on its parent at the animation's last word.
            const rigid local{frozen.position, frozen.rotation};
            posed[j] = has_parent ? compose(posed[parent], local) : local;
            local_out[j] = frozen;
        }
    }
}

mat4 realign_model(const ragdoll& rd, std::span<const rigid_body> bodies, const mat4& world_from_model,
                   vec3 anim_root_part_model)
{
    // The root part: the first part with no joint to a parent.
    std::size_t root = rd.parts.size();
    for (std::size_t i = 0; i < rd.parts.size(); ++i)
    {
        if (rd.parts[i].kind == ragdoll_link::root) { root = i; break; }
    }
    if (root == rd.parts.size()) { return world_from_model; }

    // Where the root part's BONE is now, in the world: its body, through the
    // inverse of joint_from_body — the same step `read_pose` takes.
    const ragdoll_part& p = rd.parts[root];
    const rigid_body& b = bodies[rd.first_body + root];
    const quat bone_q = b.orientation * conjugate(p.rotation);
    const vec3 bone_world = b.state.position - rotate(bone_q, p.offset);

    // Where the animation would put it with the current placement, and the
    // ground-plane difference. Only x and z: the height of a standing pelvis is
    // the clip's, and a character realigned vertically onto a lying one would
    // be put through the floor.
    const vec4 posed = world_from_model * vec4{anim_root_part_model.x, anim_root_part_model.y,
                                               anim_root_part_model.z, 1.0f};
    mat4 out = world_from_model;
    out.c3.x += bone_world.x - posed.x;
    out.c3.z += bone_world.z - posed.z;
    return out;
}

joint_error worst_joint_error(const ragdoll& rd, std::span<const rigid_body> bodies)
{
    joint_error worst;
    for (std::size_t i = 0; i < rd.parts.size(); ++i)
    {
        const ragdoll_part& p = rd.parts[i];
        if (p.kind == ragdoll_link::root || p.parent < 0) { continue; }
        const joint_error e = measure_joint(p.link, bodies[rd.first_body + static_cast<std::size_t>(p.parent)],
                                            bodies[rd.first_body + i]);
        worst.linear = std::max(worst.linear, e.linear);
        worst.angular = std::max(worst.angular, e.angular);
    }
    return worst;
}

} // namespace engine::phys
