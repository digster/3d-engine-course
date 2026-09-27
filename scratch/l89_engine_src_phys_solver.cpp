// engine/src/phys/solver.cpp — one contact, resolved.
//
// Lesson 8.9. Read `solver.hpp` first; it carries the argument. This file is
// mostly one loop over one array, and the interesting part is how few lines the
// physics turns out to be once the effective mass is a scalar: the normal solve
// is three lines and a `max`, and friction is the same three lines twice with a
// clip on the end.
//
// Two things in here are easy to get subtly wrong and both are commented at the
// point they happen: the impulse is applied with OPPOSITE signs to the two
// bodies (Newton's third law, and getting it wrong makes contacts suck), and the
// accumulated impulse is what gets clamped, not the increment.

#include <engine/phys/solver.hpp>

#include <algorithm>   // std::clamp, std::max, std::min
#include <cmath>       // std::sqrt, std::atan, std::abs, std::pow
#include <limits>      // std::numeric_limits

namespace engine::phys
{
namespace
{

/// Radians to degrees. Local rather than shared: `math/rotation.hpp` has the
/// pair, and pulling a rotation header into the solver for one constant would
/// be a dependency for a multiply.
constexpr float k_rad_to_deg = 57.295779513082320876f;

/// The velocity of `b`'s material point relative to `a`'s, given lever arms that
/// have already been computed.
///
/// The hot inner quantity: `solve_contacts` evaluates it three times per point
/// per iteration, and 8.10 will evaluate it a great many more times than that.
/// It takes the lever arms rather than a world point because the constraint
/// already holds them and `p - position` would be two subtractions to recover
/// what is sitting in the struct.
[[nodiscard]] vec3 relative_at(const rigid_body& a, const rigid_body& b, vec3 r_a, vec3 r_b)
{
    return (b.state.velocity + cross(b.angular_velocity, r_b))
           - (a.state.velocity + cross(a.angular_velocity, r_a));
}

/// Apply `+impulse` to `b` and `-impulse` to `a`, at the contact point.
///
/// **THE OPPOSITE SIGNS ARE NEWTON'S THIRD LAW** and they are the reason a
/// contact conserves momentum exactly, for free, with no correction step: the
/// two linear changes are `+J*inv_m_b` and `-J*inv_m_a`, so the total change in
/// `m*v` summed over the pair is `J - J = 0` in exact arithmetic and to the last
/// bit in floating point, because the same `J` is used twice. §5 measures it.
///
/// An immovable body has `inv_mass == 0` and an all-zero inverse tensor, so both
/// of its lines are multiplications by zero. No branch, and 8.2 §3 chose the
/// reciprocal storage precisely so that this would be true.
void apply_impulse_pair(rigid_body& a, rigid_body& b, const contact_batch& batch,
                        const contact_constraint& c, vec3 impulse)
{
    a.state.velocity   = a.state.velocity - impulse * batch.inv_mass_a;
    a.angular_velocity = a.angular_velocity - batch.inv_inertia_a * cross(c.r_a, impulse);

    b.state.velocity   = b.state.velocity + impulse * batch.inv_mass_b;
    b.angular_velocity = b.angular_velocity + batch.inv_inertia_b * cross(c.r_b, impulse);
}

/// The velocity that this step's external acceleration added to one body.
///
/// Zero for anything that is not dynamic, and scaled by the body's own
/// `gravity_scale`, because `body_world::step` scales gravity that way and a
/// correction that did not would be correcting for a force that was never
/// applied.
[[nodiscard]] vec3 step_bias_of(const rigid_body& body, vec3 bias)
{
    return body.kind == body_kind::dynamic ? bias * body.gravity_scale : vec3{};
}

} // namespace

// ---------------------------------------------------------------------------
// Materials
// ---------------------------------------------------------------------------

const char* name_of(combine_rule rule)
{
    switch (rule)
    {
    case combine_rule::geometric_mean: return "geometric mean";
    case combine_rule::minimum:        return "minimum";
    case combine_rule::maximum:        return "maximum";
    case combine_rule::average:        return "average";
    }
    return "?";
}

float combine(float a, float b, combine_rule rule)
{
    switch (rule)
    {
    // `std::max(0.0f, ...)` guards a negative coefficient handed in by a
    // designer's slider: `sqrt` of a negative is a NaN, and a NaN in a friction
    // coefficient propagates into a velocity and then into a position, where it
    // presents as a body that has vanished rather than as a bad material.
    case combine_rule::geometric_mean: return std::sqrt(std::max(0.0f, a) * std::max(0.0f, b));
    case combine_rule::minimum:        return std::min(a, b);
    case combine_rule::maximum:        return std::max(a, b);
    case combine_rule::average:        return 0.5f * (a + b);
    }
    return 0.0f;
}

contact_material combine_material(contact_material a, contact_material b,
                                  combine_rule restitution_rule, combine_rule friction_rule)
{
    return {combine(a.restitution, b.restitution, restitution_rule),
            combine(a.friction, b.friction, friction_rule)};
}

const char* name_of(friction_model model)
{
    switch (model)
    {
    case friction_model::none: return "none";
    case friction_model::box:  return "box";
    case friction_model::cone: return "cone";
    }
    return "?";
}

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

void tangent_basis(vec3 n, vec3& t1, vec3& t2)
{
    // Seed with whichever axis `n` is LEAST aligned with. The cross product of
    // two nearly parallel vectors is nearly zero and normalising it amplifies
    // whatever rounding error it contains, so the whole trick is never to take
    // one. Picking the smallest component of `n` guarantees the seed axis is at
    // least 54.7 degrees away — the worst case being a normal pointing along a
    // cube's diagonal, where all three components are equal — so the cross
    // product never has length below sin(54.7 deg) = 0.816.
    const vec3 seed = (std::abs(n.x) <= std::abs(n.y) && std::abs(n.x) <= std::abs(n.z))
                          ? vec3{1.0f, 0.0f, 0.0f}
                          : (std::abs(n.y) <= std::abs(n.z) ? vec3{0.0f, 1.0f, 0.0f}
                                                            : vec3{0.0f, 0.0f, 1.0f});

    t1 = normalised(cross(n, seed));
    // No normalisation needed on the second: `n` is unit, `t1` is unit, and they
    // are perpendicular, so their cross product is unit already. Normalising it
    // anyway would cost a square root to fix an error that is not there — and
    // §7 measures the orthonormality that results over a million normals.
    t2 = cross(n, t1);
}

vec3 contact_velocity(const rigid_body& a, const rigid_body& b, vec3 world_point)
{
    return point_velocity(b, world_point) - point_velocity(a, world_point);
}

float effective_mass(float inv_mass_a, const mat3& inv_inertia_a, vec3 r_a,
                     float inv_mass_b, const mat3& inv_inertia_b, vec3 r_b,
                     vec3 direction)
{
    // The two angular terms, in the rearranged form the header derives:
    // `w . (inv_I * w)` with `w = r x dir`. Manifestly non-negative for a
    // positive semi-definite tensor, which is what makes the division below
    // safe to do without a guard on the sign.
    const vec3 wa = cross(r_a, direction);
    const vec3 wb = cross(r_b, direction);

    const float k = inv_mass_a + inv_mass_b
                    + dot(wa, inv_inertia_a * wa)
                    + dot(wb, inv_inertia_b * wb);

    // `k` is zero exactly when both bodies are immovable, because every term is
    // non-negative and the two inverse masses are the only ones that can be
    // positive on their own. Returning zero rather than an infinity means the
    // impulse computed from it is zero, which is the right answer: no impulse
    // can change the relative velocity of two static bodies.
    return k > 0.0f ? 1.0f / k : 0.0f;
}

float effective_mass(const rigid_body& a, const rigid_body& b, vec3 r_a, vec3 r_b,
                     vec3 direction)
{
    return effective_mass(a.inv_mass, world_inv_inertia(a), r_a,
                          b.inv_mass, world_inv_inertia(b), r_b, direction);
}

// ---------------------------------------------------------------------------
// Prepare
// ---------------------------------------------------------------------------

contact_batch prepare_contacts(const rigid_body& a, const rigid_body& b,
                               const contact_manifold& m, const contact_material& material,
                               const solver_config& cfg)
{
    contact_batch batch;
    batch.normal = m.normal;
    batch.material = material;
    batch.count = std::min(m.count, k_max_clip_points);

    // The hoist. Two basis changes for the whole manifold instead of two per
    // effective-mass call — see `contact_batch`, and §13 for what it is worth.
    batch.inv_mass_a = a.inv_mass;
    batch.inv_mass_b = b.inv_mass;
    batch.inv_inertia_a = world_inv_inertia(a);
    batch.inv_inertia_b = world_inv_inertia(b);

    tangent_basis(batch.normal, batch.tangent[0], batch.tangent[1]);

    // ---- restitution, decided once for the manifold -----------------------
    //
    // A manifold has ONE restitution, not one per point, and it is decided by
    // the deepest-approaching point rather than by each point's own approach
    // speed. Per-point restitution sounds more careful and is worse: the four
    // corners of a crate landing flat approach at slightly different speeds, so
    // a per-point threshold makes two corners bouncy and two dead on the frame
    // where the speed straddles it, and the crate takes off sideways.
    float worst_approach = 0.0f;
    for (int i = 0; i < batch.count; ++i)
    {
        const vec3 p = m.points[i].position;
        const vec3 r_a = p - a.state.position;
        const vec3 r_b = p - b.state.position;
        const float vn = dot(relative_at(a, b, r_a, r_b), batch.normal);
        worst_approach = std::min(worst_approach, vn);
    }

    // The step's own contribution to the RELATIVE normal velocity. Note the
    // subtraction: if both bodies are dynamic and unscaled, gravity gave each of
    // them the same velocity and the relative velocity it produced is exactly
    // zero — so two crates colliding in mid-air need no correction at all, and
    // this expression says so without a special case. §6's control is precisely
    // that pair.
    const float bias_n = dot(step_bias_of(b, cfg.restitution_bias)
                                 - step_bias_of(a, cfg.restitution_bias),
                             batch.normal);

    // The speed the contact really ARRIVED with, as opposed to the speed it has
    // by the time the solver is looking at it.
    const float arrival = worst_approach - bias_n;

    batch.restitution_used = (arrival < -cfg.restitution_threshold) ? material.restitution : 0.0f;

    // ---- per point --------------------------------------------------------
    for (int i = 0; i < batch.count; ++i)
    {
        const contact_point& cp = m.points[i];
        contact_constraint& c = batch.points[i];

        c.point_index = i;
        c.r_a = cp.position - a.state.position;
        c.r_b = cp.position - b.state.position;

        c.normal_mass = effective_mass(batch.inv_mass_a, batch.inv_inertia_a, c.r_a,
                                       batch.inv_mass_b, batch.inv_inertia_b, c.r_b,
                                       batch.normal);
        for (int t = 0; t < 2; ++t)
        {
            c.tangent_mass[t] = effective_mass(batch.inv_mass_a, batch.inv_inertia_a, c.r_a,
                                               batch.inv_mass_b, batch.inv_inertia_b, c.r_b,
                                               batch.tangent[t]);
        }

        c.initial_normal_velocity = dot(relative_at(a, b, c.r_a, c.r_b), batch.normal);

        // The target. Zero is "stop approaching"; a negative multiple of the
        // arrival speed is "come back off at `e` times the speed you hit with".
        // Restitution is applied to the CORRECTED arrival speed for the reason
        // `solver_config::restitution_bias` gives, and `min(0, ...)` keeps a
        // point that was already separating from being told to approach.
        const float point_arrival = c.initial_normal_velocity - bias_n;
        c.target_normal_velocity =
            batch.restitution_used > 0.0f ? -batch.restitution_used * std::min(0.0f, point_arrival)
                                          : 0.0f;

        if (cfg.warm_start)
        {
            c.normal_impulse = cp.normal_impulse;
            c.tangent_impulse[0] = cp.tangent_impulse[0];
            c.tangent_impulse[1] = cp.tangent_impulse[1];
        }
    }

    return batch;
}

// ---------------------------------------------------------------------------
// Solve
// ---------------------------------------------------------------------------

namespace
{

/// One point's normal solve. Returns the impulse magnitude actually applied,
/// which may be negative when an accumulated impulse is being unwound.
float solve_one_normal(rigid_body& a, rigid_body& b, contact_batch& batch, contact_constraint& c)
{
    const float vn = dot(relative_at(a, b, c.r_a, c.r_b), batch.normal);

    // The whole of the normal solve. `normal_mass` is `1/k` and `k` is the
    // change in `vn` per unit impulse, so this is a division dressed as a
    // multiply: the impulse that moves `vn` from where it is to where it should
    // be.
    float delta = (c.target_normal_velocity - vn) * c.normal_mass;

    // *** CLAMP THE ACCUMULATED IMPULSE, NOT THE INCREMENT. ***
    //
    // A contact may push and may not pull, so the TOTAL must be non-negative —
    // but an individual increment is allowed to be negative, because a later
    // point may have overshot and this point is entitled to take some of its own
    // impulse back. Clamping `delta` instead of the total is the classic bug: it
    // makes every increment a push, the accumulator ratchets upward, and a stack
    // of boxes slowly launches itself. On the single pass this lesson makes, the
    // two are identical whenever the accumulator starts at zero; with warm
    // starting or with 8.10's iterations they are not, and this file is written
    // for the version that is coming.
    const float total = std::max(0.0f, c.normal_impulse + delta);
    delta = total - c.normal_impulse;
    c.normal_impulse = total;

    apply_impulse_pair(a, b, batch, c, batch.normal * delta);
    return delta;
}

/// One point's friction solve. Returns the magnitude of the tangential impulse
/// actually applied.
float solve_one_friction(rigid_body& a, rigid_body& b, contact_batch& batch,
                         contact_constraint& c, friction_model model)
{
    if (model == friction_model::none || batch.material.friction <= 0.0f)
    {
        return 0.0f;
    }

    // Coulomb's radius is `mu` times THIS point's accumulated normal impulse.
    // Not the manifold's total, and not last frame's: a corner that is barely
    // touching gets barely any friction, which is what stops a tilting crate
    // from gripping on the corner that has just left the floor.
    const float limit = batch.material.friction * c.normal_impulse;

    const vec3 u = relative_at(a, b, c.r_a, c.r_b);

    // Both tangents are solved from the SAME relative velocity, sampled once,
    // rather than one after the other. Solving them sequentially would make the
    // pair of impulses depend on the order of the two axes — and the axes came
    // out of `tangent_basis`, which chose them from the normal's smallest
    // component, so the physics would depend on which way the floor faces.
    float total[2];
    float delta[2];
    for (int t = 0; t < 2; ++t)
    {
        delta[t] = -dot(u, batch.tangent[t]) * c.tangent_mass[t];
        total[t] = c.tangent_impulse[t] + delta[t];
    }

    c.sliding = false;
    if (model == friction_model::cone)
    {
        const float magnitude = std::sqrt(total[0] * total[0] + total[1] * total[1]);
        if (magnitude > limit)
        {
            // Scale both components by one factor, which is the projection onto
            // the disc. Note that this is the projection in the IMPULSE metric
            // and not in the velocity one — the two tangent effective masses
            // differ, so the mathematically exact projection would be onto an
            // ellipse. Every engine does it this way; the error only appears on
            // a contact whose lever arm is strongly anisotropic, and it makes
            // the friction slightly wrong in direction rather than wrong in
            // magnitude. §7 measures it.
            const float scale = magnitude > 0.0f ? limit / magnitude : 0.0f;
            total[0] *= scale;
            total[1] *= scale;
            c.sliding = true;
        }
    }
    else   // friction_model::box
    {
        for (int t = 0; t < 2; ++t)
        {
            const float clipped = std::clamp(total[t], -limit, limit);
            if (clipped != total[t]) { c.sliding = true; }
            total[t] = clipped;
        }
    }

    vec3 impulse{};
    for (int t = 0; t < 2; ++t)
    {
        impulse = impulse + batch.tangent[t] * (total[t] - c.tangent_impulse[t]);
        c.tangent_impulse[t] = total[t];
    }

    apply_impulse_pair(a, b, batch, c, impulse);
    return length(impulse);
}

} // namespace

void warm_start_contacts(rigid_body& a, rigid_body& b, contact_batch& batch)
{
    for (int i = 0; i < batch.count; ++i)
    {
        contact_constraint& c = batch.points[i];
        const vec3 impulse = batch.normal * c.normal_impulse
                             + batch.tangent[0] * c.tangent_impulse[0]
                             + batch.tangent[1] * c.tangent_impulse[1];
        apply_impulse_pair(a, b, batch, c, impulse);
    }
}

solve_report solve_contacts(rigid_body& a, rigid_body& b, contact_batch& batch,
                            const solver_config& cfg)
{
    solve_report report;

    // Two separate loops, normals first. Friction's clip needs a normal impulse
    // to clip against, and interleaving them per point would give point 0's
    // friction a radius while point 3's normal had not been solved yet — which
    // is not wrong so much as arbitrary, and arbitrary is what makes a crate
    // land differently depending on which corner the clipper emitted first.
    if (cfg.normal_before_friction)
    {
        for (int i = 0; i < batch.count; ++i)
        {
            report.normal_impulse += solve_one_normal(a, b, batch, batch.points[i]);
        }
        for (int i = 0; i < batch.count; ++i)
        {
            report.friction_impulse += solve_one_friction(a, b, batch, batch.points[i], cfg.friction);
        }
    }
    else
    {
        // The wrong order, kept because §10 measures it. With no warm start the
        // cone radius is zero here and friction does nothing at all on the
        // frame of first contact.
        for (int i = 0; i < batch.count; ++i)
        {
            report.friction_impulse += solve_one_friction(a, b, batch, batch.points[i], cfg.friction);
        }
        for (int i = 0; i < batch.count; ++i)
        {
            report.normal_impulse += solve_one_normal(a, b, batch, batch.points[i]);
        }
    }

    for (int i = 0; i < batch.count; ++i)
    {
        const contact_constraint& c = batch.points[i];
        if (c.normal_impulse > 0.0f) { ++report.pushing_points; }
        if (c.sliding)               { ++report.sliding_points; }

        // The residual, measured AFTER every point has been solved, which is the
        // only measurement that means anything: a point's own solve leaves its
        // velocity exactly on target, and then the next point's impulse moves it
        // again. On one point this is zero to float precision; on four it is
        // not, and the size of it is 8.10's reason for existing.
        const float vn = dot(relative_at(a, b, c.r_a, c.r_b), batch.normal);
        const float residual = std::abs(vn - c.target_normal_velocity);

        // A point whose accumulated impulse is zero is not being violated when
        // its velocity is above target — it is simply separating, which is
        // allowed. Counting that as a residual would report a large error on
        // every contact that is coming apart.
        if (c.normal_impulse > 0.0f || vn < c.target_normal_velocity)
        {
            report.max_residual = std::max(report.max_residual, residual);
        }
    }

    return report;
}

void write_back(const contact_batch& batch, contact_manifold& m)
{
    for (int i = 0; i < batch.count; ++i)
    {
        const contact_constraint& c = batch.points[i];
        if (c.point_index < 0 || c.point_index >= m.count) { continue; }

        contact_point& cp = m.points[c.point_index];
        cp.normal_impulse = c.normal_impulse;
        cp.tangent_impulse[0] = c.tangent_impulse[0];
        cp.tangent_impulse[1] = c.tangent_impulse[1];
    }
}

solve_report resolve_contact(rigid_body& a, rigid_body& b, contact_manifold& m,
                             const contact_material& material, const solver_config& cfg)
{
    contact_batch batch = prepare_contacts(a, b, m, material, cfg);
    if (cfg.warm_start) { warm_start_contacts(a, b, batch); }
    const solve_report report = solve_contacts(a, b, batch, cfg);
    write_back(batch, m);
    return report;
}

// ---------------------------------------------------------------------------
// Closed forms
// ---------------------------------------------------------------------------

float bounce_height(float drop_height, float restitution, int bounces)
{
    return drop_height * std::pow(restitution, 2.0f * static_cast<float>(bounces));
}

float terminal_bounce_speed(float restitution, float gravity_step)
{
    if (restitution >= 1.0f) { return std::numeric_limits<float>::infinity(); }
    return restitution * gravity_step / (1.0f - restitution);
}

float critical_slope_degrees(float friction)
{
    return std::atan(friction) * k_rad_to_deg;
}

float rolling_speed_fraction(float inertia_coefficient)
{
    return 1.0f / (1.0f + inertia_coefficient);
}

float rolling_time(float initial_speed, float friction, float gravity, float inertia_coefficient)
{
    if (friction <= 0.0f || inertia_coefficient <= 0.0f)
    {
        return std::numeric_limits<float>::infinity();
    }
    // Sliding decelerates the centre at `mu*g` and spins the body up at
    // `mu*g/(c*r)` in contact-point terms; the gap `v - omega*r` therefore closes
    // at `mu*g*(1 + 1/c)`, and the time is the initial gap over that rate. The
    // radius cancels, which is why it is not a parameter.
    return initial_speed / (friction * gravity * (1.0f + 1.0f / inertia_coefficient));
}

float penalty_stiffness(float mass, float gravity, float penetration)
{
    return penetration > 0.0f ? mass * gravity / penetration
                              : std::numeric_limits<float>::infinity();
}

float penalty_omega(float gravity, float penetration)
{
    return penetration > 0.0f ? std::sqrt(gravity / penetration)
                              : std::numeric_limits<float>::infinity();
}

float penalty_impact_stiffness(float mass, float speed, float penetration)
{
    // Energy in, energy stored: (1/2)m*v^2 = (1/2)k*x^2. The mass does NOT
    // cancel here, which is the difference between this and the static formula
    // — and it is why a penalty floor that holds a pebble beautifully swallows
    // a crate.
    return penetration > 0.0f ? mass * speed * speed / (penetration * penetration)
                              : std::numeric_limits<float>::infinity();
}

} // namespace engine::phys
