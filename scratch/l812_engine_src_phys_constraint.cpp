// engine/src/phys/constraint.cpp — rows, blocks, and the three joints built from
// them.
//
// Lesson 8.11. Read `constraint.hpp` first; it carries the argument. The file
// has three layers and each is short once the one below it exists:
//
//   * ROWS. `point_row`, `angular_row`, `prepare_row`, `solve_row` — eight lines
//     of arithmetic that are 8.9's normal solve with the contact taken out of
//     it. Everything else in the file is a way of choosing rows.
//   * BLOCKS. A ball-socket's three point rows and a hinge's two alignment rows,
//     solved together with a 3x3 and a 2x2 effective mass. The only place in
//     the file where the effective mass is a matrix rather than a number, and
//     the only place where the answer to a joint's rows is exact in one visit.
//   * JOINTS. `prepare_joint` turns an authored joint into rows and blocks, and
//     is a switch on three kinds that mostly differs in which rows it emits.
//
// Two things are easy to get subtly wrong and both are commented where they
// happen: the SIGN of each row (every row here is written so that `J·V` is the
// rate of change of a `C` whose satisfied side is `>= 0`), and which frame a
// cached impulse lives in.
//
// Lesson 8.12 added a third, and it is the subtlest: the AXIS of a row is the
// derivative of its angle, and for a twist measured after a swing that is not
// the axis the twist is about. `prepare_joint`'s ball-socket case says why.

#include <engine/phys/constraint.hpp>

#include <engine/phys/manifold.hpp>   // pair_key

#include <algorithm>   // std::clamp, std::max, std::min, std::sort, std::unique
#include <cmath>       // std::sqrt, std::atan2, std::abs

namespace engine::phys
{
namespace
{

constexpr float k_pi = 3.14159265358979323846f;

/// Below this, a distance joint's two anchors are treated as coincident and
/// the line between them has no direction.
constexpr float k_min_length = 1e-6f;

/// Below this `sin(swing)` — 0.0057 degrees — a ball-socket's swing has no
/// axis to be measured about. Lesson 8.12.
constexpr float k_min_swing_sin = 1e-4f;

/// Below this `1 + cos(swing)` — a swing within 2.6 degrees of 180 — the twist
/// row's axis `(a1 + b1)/(1 + a1·b1)` is dividing by nearly nothing. Lesson 8.12.
constexpr float k_min_twist_cos = 1e-3f;

/// A body-local point from a world point, and a body-local direction from a
/// world direction. The inverse of `world_point_of` and of `rotate`.
[[nodiscard]] vec3 local_point_of(const rigid_body& b, vec3 world_point)
{
    return rotate(conjugate(b.orientation), world_point - b.state.position);
}

[[nodiscard]] vec3 local_direction_of(const rigid_body& b, vec3 world_direction)
{
    return rotate(conjugate(b.orientation), world_direction);
}

/// **Two unit vectors perpendicular to a unit `n`, chosen in the frame where
/// `n` does not move.**
///
/// The same smallest-component rule as 8.9's `tangent_basis`, and applied to a
/// completely different kind of vector. `tangent_basis` is handed a WORLD
/// normal that wobbles from frame to frame, and 8.10 §4 found it flipping the
/// basis by ninety degrees on 0.96% of manifold-frames because two of that
/// normal's components were both near zero and crossed. This function is handed
/// the hinge axis in BODY `a`'s own frame, which is a constant — so its answer
/// is a constant, and the world basis is that constant rotated by `a`, which
/// moves exactly as smoothly as the body does. A frame chosen where nothing
/// moves cannot flip.
void perpendicular_pair(vec3 n, vec3& p, vec3& q)
{
    const vec3 seed = (std::abs(n.x) <= std::abs(n.y) && std::abs(n.x) <= std::abs(n.z))
                          ? vec3{1.0f, 0.0f, 0.0f}
                          : (std::abs(n.y) <= std::abs(n.z) ? vec3{0.0f, 1.0f, 0.0f}
                                                            : vec3{0.0f, 0.0f, 1.0f});
    p = normalised(cross(n, seed));
    q = cross(n, p);
}

/// **The smallest rotation carrying unit `from` onto unit `to`**, with `w >= 0`.
///
/// Two mirrors, 7.4's construction: reflect in the plane perpendicular to
/// `from`, then in the plane perpendicular to the HALF-WAY vector
/// `h = normalised(from + to)`. Two reflections make a rotation by twice the
/// angle between their normals — twice `angle(from, h)`, which is
/// `angle(from, to)` — about the line where the planes meet, which is
/// perpendicular to both. No trigonometry, and no division but the one in the
/// normalisation. 8.12 §5 differentiates exactly this form.
///
/// Opposite vectors have no half-way vector and every perpendicular axis is
/// equally minimal; one is chosen by `perpendicular_pair`'s rule.
[[nodiscard]] quat minimal_rotation(vec3 from, vec3 to)
{
    const vec3 sum = from + to;
    const float len2 = length_squared(sum);
    if (len2 < 1e-12f)
    {
        vec3 p;
        vec3 q;
        perpendicular_pair(from, p, q);
        return quat{0.0f, p};   // a half-turn about a perpendicular
    }
    const quat r = rotor_from_mirrors(from, sum / std::sqrt(len2));
    return r.w < 0.0f ? -r : r;
}

/// The unit axes a ball-socket's limits are measured with, as the bodies stand
/// now: the cone's axis as `a` carries it, and the twist axis as `b` does.
void cone_axes(const joint& j, const rigid_body& a, const rigid_body& b, vec3& a1, vec3& b1)
{
    a1 = normalised(rotate(a.orientation, j.axis_a));
    b1 = normalised(rotate(b.orientation, j.axis_b));
}

/// Scale `v` down to at most `max_length` long, keeping its direction.
[[nodiscard]] vec3 clamp_length(vec3 v, float max_length)
{
    const float len2 = length_squared(v);
    if (len2 <= max_length * max_length) { return v; }
    return v * (max_length / std::sqrt(len2));
}

/// The position-correction velocity for an EQUALITY error `C`: `−β·C/h`,
/// clamped to the ceiling. Signed, because a pin can be off either way.
[[nodiscard]] float equality_bias(float error, float inv_h, const joint_config& cfg)
{
    return std::clamp(-cfg.baumgarte * error * inv_h, -cfg.max_correction_speed,
                      cfg.max_correction_speed);
}

/// Apply an impulse `P` at the two anchors: `−P` to `a`, `+P` to `b`. The
/// point block's application, and the same two lines as 8.9's
/// `apply_impulse_pair` with the batch's own tensors.
void apply_point_impulse(const joint_batch& batch, vec3& va, vec3& wa, vec3& vb, vec3& wb, vec3 p)
{
    va = va - p * batch.inv_mass_a;
    wa = wa - batch.inv_inertia_a * cross(batch.r_a, p);
    vb = vb + p * batch.inv_mass_b;
    wb = wb + batch.inv_inertia_b * cross(batch.r_b, p);
}

/// Apply the perpendicular angular impulse `(λ0, λ1)`: rows `[0, −perp, 0, perp]`.
void apply_angular_impulse(const joint_batch& batch, vec3& wa, vec3& wb, float l0, float l1)
{
    wa = wa - batch.inv_inertia_perp_a[0] * l0 - batch.inv_inertia_perp_a[1] * l1;
    wb = wb + batch.inv_inertia_perp_b[0] * l0 + batch.inv_inertia_perp_b[1] * l1;
}

/// The point constraint's `dC/dt`: the velocity of `b`'s anchor relative to
/// `a`'s. The same quantity as 8.9's `relative_at`, from the batch's lever arms.
[[nodiscard]] vec3 anchor_velocity(const joint_batch& batch, vec3 va, vec3 wa, vec3 vb, vec3 wb)
{
    return (vb + cross(wb, batch.r_b)) - (va + cross(wa, batch.r_a));
}

/// Is this row an equality, or an inequality currently strictly inside its
/// bounds? Those are the rows whose residual means something: a row pinned at
/// a bound is not being violated when its velocity disagrees with its target,
/// it is giving way — a limit letting go, a motor out of torque.
[[nodiscard]] bool row_is_active(const jacobian_row& row)
{
    return row.impulse > row.lower && row.impulse < row.upper;
}

} // namespace

// ---------------------------------------------------------------------------
// Rows
// ---------------------------------------------------------------------------

jacobian_row point_row(vec3 direction, vec3 r_a, vec3 r_b)
{
    // `J·V` is to be `direction · (velocity of b's point − velocity of a's)`.
    // Expanding the angular term `direction · (w × r)` by the scalar triple
    // product gives `(r × direction) · w`, which is why the angular halves are
    // `r × d` rather than `d × r` — and why `a`'s halves carry the minus sign:
    // `a`'s point velocity is SUBTRACTED.
    jacobian_row row;
    row.linear_a = -direction;
    row.angular_a = -cross(r_a, direction);
    row.linear_b = direction;
    row.angular_b = cross(r_b, direction);
    return row;
}

jacobian_row angular_row(vec3 axis)
{
    jacobian_row row;
    row.angular_a = -axis;
    row.angular_b = axis;
    return row;
}

void prepare_row(jacobian_row& row, float inv_mass_a, const mat3& inv_inertia_a,
                 float inv_mass_b, const mat3& inv_inertia_b)
{
    row.inv_inertia_angular_a = inv_inertia_a * row.angular_a;
    row.inv_inertia_angular_b = inv_inertia_b * row.angular_b;

    // J M⁻¹ Jᵀ, four non-negative terms. For a point row with a unit direction
    // the two linear terms are the inverse masses and the two angular ones are
    // exactly 8.9's `dot(w, inv_I * w)` with `w = r × d` — the same number, by
    // construction, which §3 checks on 200 real contacts and finds equal to
    // 1.9e-07, i.e. to the rounding of two different orders of operations.
    const float k = inv_mass_a * dot(row.linear_a, row.linear_a)
                    + dot(row.angular_a, row.inv_inertia_angular_a)
                    + inv_mass_b * dot(row.linear_b, row.linear_b)
                    + dot(row.angular_b, row.inv_inertia_angular_b);

    row.mass = k > 0.0f ? 1.0f / k : 0.0f;
}

float row_velocity(const jacobian_row& row, vec3 v_a, vec3 w_a, vec3 v_b, vec3 w_b)
{
    return dot(row.linear_a, v_a) + dot(row.angular_a, w_a) + dot(row.linear_b, v_b)
           + dot(row.angular_b, w_b);
}

float solve_row(jacobian_row& row, float inv_mass_a, float inv_mass_b, vec3& v_a, vec3& w_a,
                vec3& v_b, vec3& w_b, float extra)
{
    const float jv = row_velocity(row, v_a, w_a, v_b, w_b);

    // 8.9's three lines, with the contact taken out. `mass` is `1/(J M⁻¹ Jᵀ)`,
    // so this is the impulse that moves `J·V` onto its target in one go.
    float delta = row.mass * (row.target + extra - jv);

    // Clamp the ACCUMULATED impulse into the row's bounds, never the increment.
    // With `[0, ∞)` this is 8.9's contact clamp; with `(−∞, ∞)` it does nothing;
    // with `[−τh, τh]` it is a motor running out of torque.
    const float total = std::clamp(row.impulse + delta, row.lower, row.upper);
    delta = total - row.impulse;
    row.impulse = total;

    // Apply `M⁻¹Jᵀ·delta`. Four scaled adds and no matrix: the angular halves
    // were multiplied through by the inverse tensors in `prepare_row`.
    v_a = v_a + row.linear_a * (inv_mass_a * delta);
    w_a = w_a + row.inv_inertia_angular_a * delta;
    v_b = v_b + row.linear_b * (inv_mass_b * delta);
    w_b = w_b + row.inv_inertia_angular_b * delta;
    return delta;
}

float solve_row_position(jacobian_row& row, float inv_mass_a, float inv_mass_b, pseudo_velocity& a,
                         pseudo_velocity& b)
{
    return solve_row_position_to(row, row.bias, inv_mass_a, inv_mass_b, a, b);
}

float solve_row_position_to(jacobian_row& row, float goal, float inv_mass_a, float inv_mass_b,
                            pseudo_velocity& a, pseudo_velocity& b)
{
    const float jv = row_velocity(row, a.linear, a.angular, b.linear, b.angular);
    float delta = row.mass * (goal - jv);
    const float total = std::clamp(row.pseudo_impulse + delta, row.lower, row.upper);
    delta = total - row.pseudo_impulse;
    row.pseudo_impulse = total;

    a.linear = a.linear + row.linear_a * (inv_mass_a * delta);
    a.angular = a.angular + row.inv_inertia_angular_a * delta;
    b.linear = b.linear + row.linear_b * (inv_mass_b * delta);
    b.angular = b.angular + row.inv_inertia_angular_b * delta;
    return delta;
}

// ---------------------------------------------------------------------------
// Joints: authoring
// ---------------------------------------------------------------------------

const char* name_of(joint_kind kind)
{
    switch (kind)
    {
    case joint_kind::distance:    return "distance";
    case joint_kind::ball_socket: return "ball-socket";
    case joint_kind::hinge:       return "hinge";
    }
    return "?";
}

joint make_ball_socket(const rigid_body& a, const rigid_body& b, vec3 world_anchor)
{
    joint j;
    j.kind = joint_kind::ball_socket;
    j.anchor_a = local_point_of(a, world_anchor);
    j.anchor_b = local_point_of(b, world_anchor);
    j.rest = conjugate(a.orientation) * b.orientation;
    return j;
}

joint make_hinge(const rigid_body& a, const rigid_body& b, vec3 world_anchor, vec3 world_axis)
{
    joint j = make_ball_socket(a, b, world_anchor);
    j.kind = joint_kind::hinge;

    // The axis is stored TWICE, once in each body's frame, because the whole
    // job of the two angular rows is to notice when those two stop agreeing.
    const vec3 axis = normalised(world_axis);
    j.axis_a = local_direction_of(a, axis);
    j.axis_b = local_direction_of(b, axis);
    return j;
}

joint make_cone_twist(const rigid_body& a, const rigid_body& b, vec3 world_anchor,
                      vec3 world_twist_axis, vec3 world_cone_axis)
{
    joint j = make_ball_socket(a, b, world_anchor);

    const vec3 twist = normalised(world_twist_axis);
    const vec3 cone = length_squared(world_cone_axis) > 0.0f ? normalised(world_cone_axis) : twist;
    j.axis_a = local_direction_of(a, cone);
    j.axis_b = local_direction_of(b, twist);

    // `rest` must make `r = conj(q_a) q_b conj(rest)` carry `axis_a` onto the
    // bone in `a`'s frame at every pose, so that r's swing–twist split IS the
    // joint's swing and twist. `make_ball_socket` set `rest = r0 =
    // conj(q_a) q_b`, which carries the bone onto itself; pre-multiplying by
    // the conjugate of `offset` — the swing from the cone's axis to the bone —
    // makes r equal `offset` now, which is a pure swing, so the twist reads
    // zero at the authored pose and the swing reads the tilt. With no tilt
    // `offset` is the identity and `rest` is the hinge's.
    const vec3 bone_in_a = local_direction_of(a, twist);
    const quat offset = minimal_rotation(j.axis_a, bone_in_a);
    j.rest = conjugate(offset) * j.rest;
    return j;
}

joint make_rod(const rigid_body& a, const rigid_body& b, vec3 anchor_a, vec3 anchor_b)
{
    joint j;
    j.kind = joint_kind::distance;
    j.anchor_a = local_point_of(a, anchor_a);
    j.anchor_b = local_point_of(b, anchor_b);
    j.rest = conjugate(a.orientation) * b.orientation;
    j.min_length = length(anchor_b - anchor_a);
    j.max_length = j.min_length;
    return j;
}

joint make_rope(const rigid_body& a, const rigid_body& b, vec3 anchor_a, vec3 anchor_b,
                float max_length)
{
    joint j = make_rod(a, b, anchor_a, anchor_b);
    j.min_length = 0.0f;
    j.max_length = max_length;
    return j;
}

float hinge_angle(const joint& j, const rigid_body& a, const rigid_body& b)
{
    // The deviation from the authored relative pose, expressed in `a`'s frame:
    // `q_b = q_a · q · rest`, solved for `q`. A pure turn of `b` about the hinge
    // axis by θ gives exactly `q = (cos θ/2, sin θ/2 · axis_a)`.
    quat q = conjugate(a.orientation) * b.orientation * conjugate(j.rest);

    // `q` and `−q` are one rotation. Only the `w >= 0` representative gives an
    // angle in (−π, π]; the other gives the same angle plus a full turn.
    if (q.w < 0.0f) { q = -q; }

    // The twist about the axis: the component of the rotation vector along it.
    // Any swing — the axes drifting apart, which the perpendicular rows exist
    // to prevent — is in the part of `q.v` perpendicular to the axis and is
    // ignored here, which is what makes this the angle a LIMIT should use.
    return 2.0f * std::atan2(dot(q.v, j.axis_a), q.w);
}

swing_twist split_swing_twist(quat q, vec3 unit_axis)
{
    swing_twist out;

    // Keep the component of q.v along the axis and delete the rest: what is
    // left is a rotation about the axis, and it is the twist because the part
    // deleted — q.v's perpendicular component — is exactly what the swing is
    // made of. The swing is then whatever turns the twist into q.
    const float along = dot(q.v, unit_axis);
    const float len2 = q.w * q.w + along * along;

    // The one singularity: a swing of 180 degrees leaves nothing to project.
    // Every twist is equally right there; the identity is the one that does
    // not invent a turn nobody asked for.
    if (len2 < 1e-12f)
    {
        out.swing = q.w < 0.0f ? -q : q;
        return out;
    }

    const float inv = 1.0f / std::sqrt(len2);
    out.twist = quat{q.w * inv, unit_axis * (along * inv)};
    if (out.twist.w < 0.0f) { out.twist = -out.twist; }
    out.swing = q * conjugate(out.twist);
    if (out.swing.w < 0.0f) { out.swing = -out.swing; }
    return out;
}

float swing_angle(const joint& j, const rigid_body& a, const rigid_body& b)
{
    vec3 a1;
    vec3 b1;
    cone_axes(j, a, b, a1, b1);
    return std::atan2(length(cross(a1, b1)), dot(a1, b1));
}

joint_error measure_joint(const joint& j, const rigid_body& a, const rigid_body& b)
{
    joint_error e;
    const vec3 pa = world_point_of(a, j.anchor_a);
    const vec3 pb = world_point_of(b, j.anchor_b);

    switch (j.kind)
    {
    case joint_kind::distance:
    {
        const float d = length(pb - pa);
        e.linear = std::max(0.0f, d - j.max_length) + std::max(0.0f, j.min_length - d);
        break;
    }
    case joint_kind::ball_socket:
        e.linear = length(pb - pa);
        // Lesson 8.12: the two limits, if any. Measured with the same two
        // functions the rows are built from, so an instrument and a row can
        // never disagree about what "outside the cone" means.
        if (j.cone.enabled)
        {
            e.angular += std::max(0.0f, swing_angle(j, a, b) - j.cone.swing);
        }
        if (j.limit.enabled)
        {
            const float theta = hinge_angle(j, a, b);
            e.angular += std::max(0.0f, j.limit.lower - theta) + std::max(0.0f, theta - j.limit.upper);
        }
        break;
    case joint_kind::hinge:
    {
        e.linear = length(pb - pa);
        const vec3 a1 = rotate(a.orientation, j.axis_a);
        const vec3 b1 = rotate(b.orientation, j.axis_b);

        // atan2 of the sine and cosine rather than acos of the cosine: 8.7
        // found `acos` has a 0.036-degree resolution floor on float unit
        // vectors, which is larger than the misalignment a working hinge has.
        e.angular = std::atan2(length(cross(a1, b1)), dot(a1, b1));
        if (j.limit.enabled)
        {
            const float theta = hinge_angle(j, a, b);
            e.angular += std::max(0.0f, j.limit.lower - theta) + std::max(0.0f, theta - j.limit.upper);
        }
        break;
    }
    }
    return e;
}

// ---------------------------------------------------------------------------
// Joints: prepare
// ---------------------------------------------------------------------------

joint_batch prepare_joint(const rigid_body& a, const rigid_body& b, const joint& j, float h,
                          const joint_config& cfg)
{
    joint_batch batch;
    batch.kind = j.kind;

    // The hoist: two basis changes per joint, not one per row.
    batch.inv_mass_a = a.inv_mass;
    batch.inv_mass_b = b.inv_mass;
    batch.inv_inertia_a = world_inv_inertia(a);
    batch.inv_inertia_b = world_inv_inertia(b);

    const vec3 pa = world_point_of(a, j.anchor_a);
    const vec3 pb = world_point_of(b, j.anchor_b);
    batch.r_a = pa - a.state.position;
    batch.r_b = pb - b.state.position;

    // `h <= 0` is a caller bug; a zero here makes every bias and every
    // speculative target zero, which is a joint that holds velocity and
    // corrects nothing, rather than an infinity in a velocity.
    const float inv_h = h > 0.0f ? 1.0f / h : 0.0f;

    const auto finish = [&](jacobian_row row, row_role role, int component, float warm) {
        prepare_row(row, batch.inv_mass_a, batch.inv_inertia_a, batch.inv_mass_b,
                    batch.inv_inertia_b);
        row.role = static_cast<std::uint8_t>(role);
        row.component = static_cast<std::uint8_t>(component);
        row.impulse = cfg.warm_start ? std::clamp(warm, row.lower, row.upper) : 0.0f;
        batch.rows[batch.row_count++] = row;
    };

    // *** A ONE-SIDED ROW: `C >= 0` IS SATISFIED, AND `C` IS THE GAP LEFT. ***
    //
    // A contact, a rope's long end, a hinge's stop — all the same shape. The
    // impulse may only push (`λ >= 0`), and two cases decide the target:
    //
    //   C > 0: not reached yet. With speculation the row exists anyway, with a
    //          target of `−C/h` — "close the gap if you must, but no faster
    //          than one step" — and the clamp makes it do nothing at all
    //          unless the approach is fast enough to overshoot this step.
    //          Without speculation the row does not exist, and §10 measures
    //          what that costs.
    //   C <= 0: reached or passed. Target zero (stop approaching), plus a
    //          bias to push back out that only Baumgarte adds to the velocity
    //          pass and only split impulse uses in the position pass.
    const auto one_sided = [&](jacobian_row row, float c, row_role role, float warm) {
        if (c > 0.0f && !cfg.speculative_limits) { return; }
        row.error = c;
        row.lower = 0.0f;
        row.upper = k_unbounded;
        row.target = c > 0.0f ? -c * inv_h : 0.0f;
        row.bias = c < 0.0f ? std::min(-cfg.baumgarte * c * inv_h, cfg.max_correction_speed) : 0.0f;
        finish(row, role, 0, warm);
    };

    switch (j.kind)
    {
    // ---- the distance joint: one row along the line --------------------------
    case joint_kind::distance:
    {
        const vec3 d = pb - pa;
        const float len = length(d);

        // Coincident anchors have no line between them, and any direction is
        // as good as any other for a rod of length zero. `y` rather than a
        // stale direction from last frame, because a joint is not supposed to
        // remember anything but its impulses.
        const vec3 u = len > k_min_length ? d / len : vec3{0.0f, 1.0f, 0.0f};

        if (j.max_length - j.min_length <= k_min_length)
        {
            // A ROD. One bilateral row, the same `point_row` a contact's
            // normal is, with the clamp taken off.
            jacobian_row row = point_row(u, batch.r_a, batch.r_b);
            row.error = len - j.max_length;
            row.bias = equality_bias(row.error, inv_h, cfg);
            finish(row, row_role::limit_lower, 0, j.limit_impulse[0]);
        }
        else
        {
            // A ROPE (or a rod with play). Two one-sided rows. The long end's
            // direction is `−u`, so that its `J·V` is the rate at which the
            // GAP to the maximum closes — a contact's normal, turned to point
            // inward. A rope is a contact turned inside out, and this minus
            // sign is the whole of the turn.
            if (j.min_length > 0.0f)
            {
                one_sided(point_row(u, batch.r_a, batch.r_b), len - j.min_length,
                          row_role::limit_lower, j.limit_impulse[0]);
            }
            one_sided(point_row(-u, batch.r_a, batch.r_b), j.max_length - len,
                      row_role::limit_upper, j.limit_impulse[1]);
        }
        break;
    }

    // ---- the hinge's own rows, then its point, which it shares ---------------
    case joint_kind::hinge:
    {
        const vec3 axis_local = normalised(j.axis_a);
        const vec3 a1 = rotate(a.orientation, axis_local);
        const vec3 b1 = normalised(rotate(b.orientation, j.axis_b));
        batch.axis = a1;
        batch.angle = hinge_angle(j, a, b);

        // MOTOR FIRST, THEN LIMITS: the rows that may give way are solved
        // before the ones that may not, so that the last thing each sweep
        // satisfies is the pin. A motor that runs out of torque against a
        // limit then loses to the limit, which is the answer you want.
        if (j.motor.enabled)
        {
            jacobian_row row = angular_row(a1);
            row.target = j.motor.speed;
            const float bound = std::max(0.0f, j.motor.max_torque) * h;
            row.lower = -bound;
            row.upper = bound;
            finish(row, row_role::motor, 0, j.motor_impulse);
        }

        if (j.limit.enabled)
        {
            // The lower stop: `θ − lower >= 0`, pushed along `+axis`. The
            // upper: `upper − θ >= 0`, pushed along `−axis`. A locked hinge is
            // `lower == upper` and both rows together are bilateral.
            one_sided(angular_row(a1), batch.angle - j.limit.lower, row_role::limit_lower,
                      j.limit_impulse[0]);
            one_sided(angular_row(-a1), j.limit.upper - batch.angle, row_role::limit_upper,
                      j.limit_impulse[1]);
        }

        // The two alignment rows. `C_i = (a1 × b1) · perp_i`: the cross product
        // of the two axes is, for a small misalignment, the rotation vector
        // that carries `a1` onto `b1`, and its components along the two
        // perpendiculars are the two angles the hinge must not allow.
        vec3 p0;
        vec3 p1;
        perpendicular_pair(axis_local, p0, p1);
        batch.perp[0] = rotate(a.orientation, p0);
        batch.perp[1] = rotate(a.orientation, p1);

        const vec3 misalignment = cross(a1, b1);
        for (int i = 0; i < 2; ++i)
        {
            batch.angular_error[i] = dot(misalignment, batch.perp[i]);
            batch.angular_bias[i] = equality_bias(batch.angular_error[i], inv_h, cfg);
        }

        // The cached impulse is a WORLD vector and is re-measured in this
        // step's basis — 8.10 §4's fix, in its other form. Storing the two
        // coordinates would have needed the basis stored beside them.
        const float warm0 = cfg.warm_start ? dot(j.angular_impulse, batch.perp[0]) : 0.0f;
        const float warm1 = cfg.warm_start ? dot(j.angular_impulse, batch.perp[1]) : 0.0f;

        if (cfg.block_solve)
        {
            batch.angular_block = true;
            for (int i = 0; i < 2; ++i)
            {
                batch.inv_inertia_perp_a[i] = batch.inv_inertia_a * batch.perp[i];
                batch.inv_inertia_perp_b[i] = batch.inv_inertia_b * batch.perp[i];
            }

            // K_ij = perp_i · (I_a⁻¹ + I_b⁻¹) perp_j — the 2x2 J M⁻¹ Jᵀ. There
            // are no linear terms: turning a body about its centre moves the
            // centre nowhere, and these rows care only about turning.
            const float k00 = dot(batch.perp[0], batch.inv_inertia_perp_a[0] + batch.inv_inertia_perp_b[0]);
            const float k01 = dot(batch.perp[0], batch.inv_inertia_perp_a[1] + batch.inv_inertia_perp_b[1]);
            const float k11 = dot(batch.perp[1], batch.inv_inertia_perp_a[1] + batch.inv_inertia_perp_b[1]);
            const float det = k00 * k11 - k01 * k01;
            if (det > 0.0f)
            {
                const float inv = 1.0f / det;
                batch.angular_mass[0] = k11 * inv;
                batch.angular_mass[1] = -k01 * inv;
                batch.angular_mass[2] = k00 * inv;
            }
            batch.angular_impulse[0] = warm0;
            batch.angular_impulse[1] = warm1;
        }
        else
        {
            for (int i = 0; i < 2; ++i)
            {
                jacobian_row row = angular_row(batch.perp[i]);
                row.error = batch.angular_error[i];
                row.bias = batch.angular_bias[i];
                finish(row, row_role::perpendicular, i, i == 0 ? warm0 : warm1);
            }
        }
        [[fallthrough]];
    }

    // ---- the point constraint: a ball-socket, and the hinge's pin ---------------
    case joint_kind::ball_socket:
    {
        // ---- Lesson 8.12: a swing cone and a twist range, before the pin -----
        //
        // Guarded on the kind because the hinge FALLS THROUGH into this case
        // for its pin, and a hinge's `limit` is its own angle, emitted above.
        if (j.kind == joint_kind::ball_socket && (j.cone.enabled || j.limit.enabled))
        {
            vec3 a1;
            vec3 b1;
            cone_axes(j, a, b, a1, b1);
            const vec3 n = cross(a1, b1);
            const float sin_swing = length(n);
            const float cos_swing = dot(a1, b1);
            batch.axis = a1;
            batch.swing = std::atan2(sin_swing, cos_swing);

            // THE SWING ROW. `C = cone − φ >= 0`, and φ is the angle between two
            // unit vectors, whose rate is exactly `(w_b − w_a) · n̂` with n̂ along
            // `a1 × b1` (8.12 §4: differentiate `cos φ = a1 · b1` and divide by
            // `|a1 × b1| = sin φ`). So `dC/dt = −(w_b − w_a) · n̂`, which is
            // `angular_row(−n̂)` — the hinge's upper stop with n̂ for the axis.
            //
            // On the cone's axis n̂ does not exist, and there the limb is as far
            // from the cone as it can be, so the row is left out: speculation
            // only misses a cone the limb could cross from dead centre in ONE
            // step, which at 60 Hz is a spin faster than `swing / h` — 31 rad/s
            // for a 30-degree cone. The other degenerate case, a limb folded
            // straight back through 180 degrees, is fully violated and still
            // needs pushing, so it gets a perpendicular.
            if (j.cone.enabled)
            {
                vec3 axis{};
                if (sin_swing > k_min_swing_sin) { axis = n / sin_swing; }
                else if (cos_swing < 0.0f)
                {
                    vec3 q;
                    perpendicular_pair(a1, axis, q);
                }
                if (length_squared(axis) > 0.0f)
                {
                    one_sided(angular_row(-axis), j.cone.swing - batch.swing, row_role::swing,
                              j.swing_impulse);
                }
            }

            // THE TWIST ROWS, and the one place in this file where the obvious
            // axis is wrong. The twist θ of `r = swing * twist` changes at
            //
            //     dθ/dt = (w_b − w_a) · (a1 + b1) / (1 + a1·b1)
            //
            // — the HALF-WAY axis over `cos(φ/2)`, not the bone. The part of
            // the relative spin along the bone that is not twist is the swing
            // plane turning, and a row about the bone would count it as twist
            // (8.12 §5 derives this from 7.4's mirrors, and measures what the
            // bone axis gets wrong: Codman's paradox). At φ = 0 it IS the bone,
            // and the hinge's limit rows are this with the swing held at zero.
            //
            // Undefined at φ = 180 degrees, where the twist itself is; skipped
            // within a whisker of it.
            if (j.limit.enabled && 1.0f + cos_swing > k_min_twist_cos)
            {
                const vec3 axis = (a1 + b1) / (1.0f + cos_swing);
                batch.angle = hinge_angle(j, a, b);
                one_sided(angular_row(axis), batch.angle - j.limit.lower, row_role::limit_lower,
                          j.limit_impulse[0]);
                one_sided(angular_row(-axis), j.limit.upper - batch.angle, row_role::limit_upper,
                          j.limit_impulse[1]);
            }
        }

        batch.point_error = pb - pa;
        const vec3 bias = clamp_length(batch.point_error * (-cfg.baumgarte * inv_h),
                                       cfg.max_correction_speed);
        const vec3 warm = cfg.warm_start ? j.point_impulse : vec3{};

        if (cfg.block_solve)
        {
            batch.point_block = true;

            // K = J M⁻¹ Jᵀ for the three rows at once:
            //
            //   K = (m_a⁻¹ + m_b⁻¹)·1 − [r_a]ₓ I_a⁻¹ [r_a]ₓ − [r_b]ₓ I_b⁻¹ [r_b]ₓ
            //
            // The first term is diagonal: a push along x through the centre of
            // mass moves the anchor along x and nowhere else. The other two are
            // not, and their off-diagonal entries are the COUPLING — an impulse
            // along x at an anchor off the centre spins the body, and the spin
            // moves the anchor in y and z. That is what a row-by-row solve has
            // to iterate away, and what the inverse below removes in one go.
            const mat3 sa = skew(batch.r_a);
            const mat3 sb = skew(batch.r_b);
            const float im = batch.inv_mass_a + batch.inv_mass_b;
            const mat3 k = diagonal(vec3{im, im, im}) - sa * batch.inv_inertia_a * sa
                           - sb * batch.inv_inertia_b * sb;

            // `inverse` returns zeros for a singular matrix (2.6's contract),
            // which is two immovable bodies — and zero is the right effective
            // mass for those, exactly as 8.9's scalar version returns zero.
            batch.point_mass = inverse(k);
            batch.point_bias = bias;
            batch.point_impulse = warm;
        }
        else
        {
            const vec3 axes[3] = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};
            const float errors[3] = {batch.point_error.x, batch.point_error.y, batch.point_error.z};
            const float biases[3] = {bias.x, bias.y, bias.z};
            const float warms[3] = {warm.x, warm.y, warm.z};
            for (int k = 0; k < 3; ++k)
            {
                jacobian_row row = point_row(axes[k], batch.r_a, batch.r_b);
                row.error = errors[k];
                row.bias = biases[k];
                finish(row, row_role::point, k, warms[k]);
            }
        }
        break;
    }
    }

    return batch;
}

// ---------------------------------------------------------------------------
// Joints: solve
// ---------------------------------------------------------------------------

void warm_start_joint(rigid_body& a, rigid_body& b, const joint_batch& batch)
{
    vec3& va = a.state.velocity;
    vec3& wa = a.angular_velocity;
    vec3& vb = b.state.velocity;
    vec3& wb = b.angular_velocity;

    for (int i = 0; i < batch.row_count; ++i)
    {
        const jacobian_row& row = batch.rows[i];
        va = va + row.linear_a * (batch.inv_mass_a * row.impulse);
        wa = wa + row.inv_inertia_angular_a * row.impulse;
        vb = vb + row.linear_b * (batch.inv_mass_b * row.impulse);
        wb = wb + row.inv_inertia_angular_b * row.impulse;
    }
    if (batch.angular_block)
    {
        apply_angular_impulse(batch, wa, wb, batch.angular_impulse[0], batch.angular_impulse[1]);
    }
    if (batch.point_block)
    {
        apply_point_impulse(batch, va, wa, vb, wb, batch.point_impulse);
    }
}

joint_report solve_joint(rigid_body& a, rigid_body& b, joint_batch& batch, bool use_bias)
{
    joint_report report;

    vec3& va = a.state.velocity;
    vec3& wa = a.angular_velocity;
    vec3& vb = b.state.velocity;
    vec3& wb = b.angular_velocity;

    // ---- the scalar rows: motor, limits, a distance joint's row(s) ---------
    for (int i = 0; i < batch.row_count; ++i)
    {
        jacobian_row& row = batch.rows[i];
        report.impulse += std::abs(solve_row(row, batch.inv_mass_a, batch.inv_mass_b, va, wa, vb,
                                             wb, use_bias ? row.bias : 0.0f));
    }

    // ---- the hinge's alignment, as a 2x2 block -------------------------------
    if (batch.angular_block)
    {
        const vec3 w_rel = wb - wa;
        const float r0 = (use_bias ? batch.angular_bias[0] : 0.0f) - dot(batch.perp[0], w_rel);
        const float r1 = (use_bias ? batch.angular_bias[1] : 0.0f) - dot(batch.perp[1], w_rel);

        // The 2x2 inverse applied to the right-hand side: both rows solved at
        // once, so neither undoes the other. No clamp, because a hinge's axes
        // may be pushed back into line in either direction.
        const float l0 = batch.angular_mass[0] * r0 + batch.angular_mass[1] * r1;
        const float l1 = batch.angular_mass[1] * r0 + batch.angular_mass[2] * r1;
        batch.angular_impulse[0] += l0;
        batch.angular_impulse[1] += l1;
        apply_angular_impulse(batch, wa, wb, l0, l1);
        report.impulse += std::abs(l0) + std::abs(l1);
    }

    // ---- the pin, as a 3x3 block, LAST -----------------------------------------
    if (batch.point_block)
    {
        // `λ = K⁻¹ (target − dC/dt)`: the vector impulse that makes the two
        // anchors move together, exactly, in one visit — for this joint alone.
        // The next joint's visit disturbs it again, which is what the sweeps
        // are for.
        const vec3 cdot = anchor_velocity(batch, va, wa, vb, wb);
        const vec3 lambda = batch.point_mass * ((use_bias ? batch.point_bias : vec3{}) - cdot);
        batch.point_impulse = batch.point_impulse + lambda;
        apply_point_impulse(batch, va, wa, vb, wb, lambda);
        report.impulse += length(lambda);
    }

    // ---- the residual, after everything has moved ------------------------------
    //
    // Measured after the whole visit, which is the only measurement that means
    // anything: each row leaves its own `J·V` exactly on target, and the next
    // row moves it again. For a block the residual is zero to rounding on its
    // own rows and is disturbed only by the rows solved after it — which is why
    // the pin is solved last.
    for (int i = 0; i < batch.row_count; ++i)
    {
        const jacobian_row& row = batch.rows[i];
        if (!row_is_active(row)) { continue; }
        const float want = row.target + (use_bias ? row.bias : 0.0f);
        report.max_residual = std::max(report.max_residual,
                                       std::abs(row_velocity(row, va, wa, vb, wb) - want));
    }
    if (batch.angular_block)
    {
        const vec3 w_rel = wb - wa;
        for (int i = 0; i < 2; ++i)
        {
            const float want = use_bias ? batch.angular_bias[i] : 0.0f;
            report.max_residual =
                std::max(report.max_residual, std::abs(dot(batch.perp[i], w_rel) - want));
        }
    }
    if (batch.point_block)
    {
        const vec3 miss = anchor_velocity(batch, va, wa, vb, wb)
                          - (use_bias ? batch.point_bias : vec3{});
        report.max_residual = std::max({report.max_residual, std::abs(miss.x), std::abs(miss.y),
                                        std::abs(miss.z)});
    }
    return report;
}

float solve_joint_positions(joint_batch& batch, pseudo_velocity& pa, pseudo_velocity& pb)
{
    float worst = 0.0f;

    for (int i = 0; i < batch.row_count; ++i)
    {
        jacobian_row& row = batch.rows[i];

        // A motor has no position error — it is a velocity target and nothing
        // else — so it has nothing to say to a pass whose only job is
        // repairing error that already exists.
        if (row.role == static_cast<std::uint8_t>(row_role::motor)) { continue; }

        const bool one_sided = row.lower == 0.0f;
        worst = std::max(worst, one_sided ? std::max(0.0f, -row.error) : std::abs(row.error));

        // *** LESSON 8.12 FIXED THIS, AND IT HAD BEEN WRONG SINCE 8.11. ***
        //
        // A one-sided row that is NOT violated — a limit the joint has not
        // reached — used to be solved here against `row.bias`, which is zero for
        // it, and that made it a hard "no pseudo-velocity toward me" row. A hinge
        // with both stops enabled always has both rows, so the far stop, a
        // hundred and fifty degrees away, cancelled every correction of the
        // near one exactly: a violated limit was NEVER repaired under split
        // impulse. 8.11 measured limits at the velocity level only — bounces,
        // overshoots, stalls — and never met it. 8.12 §7 found it holding a
        // ragdoll's knee 9 degrees hyperextended, and an isolated shin on its
        // stop at a violation frozen, to four decimals, for ten seconds.
        //
        // The row's own speculative target is the right answer here too: the
        // correction may carry the joint up to the far stop and no further,
        // which is what `target = −C/h` already says. A violated row keeps its
        // bias; a rod's bilateral row has no speculative side and keeps its bias.
        const float goal = (one_sided && row.error > 0.0f) ? row.target : row.bias;
        (void)solve_row_position_to(row, goal, batch.inv_mass_a, batch.inv_mass_b, pa, pb);
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
        apply_angular_impulse(batch, pa.angular, pb.angular, l0, l1);
    }

    if (batch.point_block)
    {
        worst = std::max(worst, length(batch.point_error));
        const vec3 cdot = anchor_velocity(batch, pa.linear, pa.angular, pb.linear, pb.angular);
        const vec3 lambda = batch.point_mass * (batch.point_bias - cdot);
        batch.point_pseudo_impulse = batch.point_pseudo_impulse + lambda;
        apply_point_impulse(batch, pa.linear, pa.angular, pb.linear, pb.angular, lambda);
    }

    return worst;
}

void write_back(const joint_batch& batch, joint& j)
{
    // Everything is overwritten, including the fields this step had no row
    // for: a limit that was not reached this step (speculation off) must not
    // hand last step's impulse to the next step that reaches it.
    j.point_impulse = batch.point_block ? batch.point_impulse : vec3{};
    j.angular_impulse = batch.angular_block
                            ? batch.perp[0] * batch.angular_impulse[0]
                                  + batch.perp[1] * batch.angular_impulse[1]
                            : vec3{};
    j.limit_impulse[0] = 0.0f;
    j.limit_impulse[1] = 0.0f;
    j.motor_impulse = 0.0f;
    j.swing_impulse = 0.0f;

    for (int i = 0; i < batch.row_count; ++i)
    {
        const jacobian_row& row = batch.rows[i];
        switch (static_cast<row_role>(row.role))
        {
        case row_role::point:
        {
            // The three world axes, so the three row impulses ARE the world
            // vector's components.
            float* c = row.component == 0 ? &j.point_impulse.x
                       : row.component == 1 ? &j.point_impulse.y
                                            : &j.point_impulse.z;
            *c = row.impulse;
            break;
        }
        case row_role::perpendicular:
            j.angular_impulse = j.angular_impulse + batch.perp[row.component] * row.impulse;
            break;
        case row_role::limit_lower: j.limit_impulse[0] = row.impulse; break;
        case row_role::limit_upper: j.limit_impulse[1] = row.impulse; break;
        case row_role::motor:       j.motor_impulse = row.impulse; break;
        case row_role::swing:       j.swing_impulse = row.impulse; break;
        }
    }
}

// ---------------------------------------------------------------------------
// The collision filter
// ---------------------------------------------------------------------------

void collision_filter::clear() { keys_.clear(); }

void collision_filter::exclude(std::uint32_t a, std::uint32_t b) { keys_.push_back(pair_key(a, b)); }

void collision_filter::finalize()
{
    std::sort(keys_.begin(), keys_.end());
    keys_.erase(std::unique(keys_.begin(), keys_.end()), keys_.end());
}

bool collision_filter::excluded(std::uint32_t a, std::uint32_t b) const
{
    return std::binary_search(keys_.begin(), keys_.end(), pair_key(a, b));
}

std::size_t collision_filter::size() const { return keys_.size(); }

// ---------------------------------------------------------------------------
// Closed forms
// ---------------------------------------------------------------------------

float pendulum_period(float inertia_about_pivot, float mass, float gravity, float pivot_to_centre)
{
    const float denominator = mass * gravity * pivot_to_centre;
    if (!(denominator > 0.0f)) { return std::numeric_limits<float>::infinity(); }
    return 2.0f * k_pi * std::sqrt(inertia_about_pivot / denominator);
}

float pendulum_period_at(float small_amplitude_period, float amplitude_radians)
{
    // T = T₀·(2/π)·K(k), k = sin(θ₀/2), and K(k) = π / (2·AGM(1, k')) with
    // k' = sqrt(1 − k²) = cos(θ₀/2). The π's cancel and what is left is the
    // loveliest formula in the file: the period at amplitude θ₀ is the small-
    // swing period divided by the arithmetic–geometric mean of 1 and cos(θ₀/2).
    //
    // Computed in double, because the AGM converges quadratically to a number
    // whose last float bit is the thing §8 compares against.
    double a = 1.0;
    double g = std::cos(0.5 * static_cast<double>(amplitude_radians));
    if (!(g > 0.0)) { return std::numeric_limits<float>::infinity(); }   // θ₀ >= π: never returns
    for (int i = 0; i < 32 && std::abs(a - g) > 1e-15 * a; ++i)
    {
        const double next_a = 0.5 * (a + g);
        g = std::sqrt(a * g);
        a = next_a;
    }
    return static_cast<float>(static_cast<double>(small_amplitude_period) / a);
}

float constraint_drift_per_step(float speed, float radius, float h)
{
    // `sqrt(r² + s²) − r`, written as `s²/(sqrt(r² + s²) + r)` so that it does
    // not cancel catastrophically when the step is small — which is always:
    // at 60 Hz the drift is a few millionths of the radius, and the naive form
    // loses every significant figure of it.
    const float s = speed * h;
    return s * s / (std::sqrt(radius * radius + s * s) + radius);
}

float lever_ratio(float inertia_about_centre, float mass, float pivot_to_centre)
{
    // `m·d²` is the parallel-axis term: the inertia the body has about the pin
    // BECAUSE its centre is not on it. Divided by the whole inertia about the
    // pin, it is the share of any rotation's kinetic energy that is the centre
    // orbiting — the only share a point constraint's projection can touch.
    const float orbit = mass * pivot_to_centre * pivot_to_centre;
    const float total = inertia_about_centre + orbit;
    return total > 0.0f ? orbit / total : 0.0f;
}

float projection_loss_per_step(float lever, float angular_speed, float h)
{
    const float turn = angular_speed * h;
    return lever * turn * turn;
}

float motor_spin_up_time(float inertia, float speed, float max_torque)
{
    if (!(max_torque > 0.0f)) { return std::numeric_limits<float>::infinity(); }
    return inertia * std::abs(speed) / max_torque;
}

} // namespace engine::phys
